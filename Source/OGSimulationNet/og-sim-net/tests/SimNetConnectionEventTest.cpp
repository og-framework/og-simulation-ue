// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Handshake/Handshake.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simNetConnectionEventTest
{
struct ChurnEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { laneHandlers[static_cast<size_t>(lane)] = handler; }
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId id) { disconnects.push_back(id); }

	void deliver(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		const ognet::LaneHandler& handler = laneHandlers[static_cast<size_t>(lane)];
		handler.fn(handler.ctx, id, lane, bytes, recvSeconds);
	}

	std::array<ognet::LaneHandler, ognet::kLaneCount> laneHandlers{};
	std::vector<ognet::ConnectionId> disconnects;
};

using Transport = ogsimnet::SimNetTransport<ChurnEndpoint, 2048>;

constexpr size_t kSlots = 16u;
constexpr size_t kStayers = 8u;
constexpr std::uint32_t kQuota = 124u;
constexpr std::uint32_t kTopUp = kQuota + 6u;
constexpr std::uint64_t kHellosPerSecond = static_cast<std::uint64_t>(ognet::handshake::kCookieHellosPerSecond);

static_assert(kHellosPerSecond == 200u);

std::array<std::byte, 8> stamped(SimConnectionId conn, SimLane lane, std::uint32_t sequence)
{
	std::array<std::byte, 8> out{};
	std::memcpy(out.data(), &conn, sizeof(conn));
	std::memcpy(out.data() + 4, &sequence, sizeof(sequence));
	static_cast<void>(lane);
	return out;
}

std::uint32_t sequenceOf(const InboundSimRecord& record)
{
	std::uint32_t sequence = 0u;
	std::memcpy(&sequence, record.bytes.data() + 4, sizeof(sequence));
	return sequence;
}

enum class Seen : std::uint8_t { Nothing, Up, Down };

struct ChurnRun
{
	bool fullBucket = false;
	int gapMs = 100;
	int gaps = 20;
	int longGapIndex = -1;
	int longGapMs = 0;
};

struct ChurnResult
{
	size_t churners = 0u;
	size_t announced = 0u;
	size_t unannounced = 0u;
	size_t maxPairsInOneGap = 0u;
	size_t maxLive = 0u;
	size_t endpointRefusedConnects = 0u;
	std::uint64_t badConnected = 0u;
	std::uint64_t messagesOutsideUp = 0u;
	std::uint64_t badDisconnected = 0u;
	std::uint64_t announcedWithoutOneDisconnected = 0u;
	std::uint64_t unannouncedWithRecords = 0u;
	std::uint64_t unannouncedDisconnectRequestsWrong = 0u;
	std::uint64_t unannouncedMessagesDelivered = 0u;
	std::uint64_t stayerControlOffered = 0u;
	std::uint64_t stayerControlArrived = 0u;
	std::uint64_t stayerOutOfOrder = 0u;
	std::uint64_t stayersWithoutConnectedFirst = 0u;
	std::uint64_t stayerDisconnectedAtEnd = 0u;
	std::uint64_t announcedAfterTheLongGap = 0u;
	size_t disconnectRequests = 0u;
	SimInboxStats stats;
	ogsimnet::SimNetTransportStats transportStats;
	size_t eventCapacity = 0u;
};

class ChurnRig
{
public:
	explicit ChurnRig(ogsimnet::SimNetConfig config = {}) : transport(std::make_unique<Transport>(config))
	{
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
	}

	bool connect(SimConnectionId id)
	{
		if (live.size() >= transport->inbox().tagCount())
		{
			++endpointRefusedConnects;
			return false;
		}
		live.insert(id);
		maxLive = std::max(maxLive, live.size());
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, id});
		return true;
	}

	void close(SimConnectionId id)
	{
		if (live.erase(id) == 0u)
			return;
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, id});
	}

	void applyQueuedDisconnects()
	{
		for (; applied < endpoint.disconnects.size(); ++applied)
			close(endpoint.disconnects[applied]);
	}

	void deliver(SimConnectionId id, ognet::Lane lane, double now)
	{
		const SimLane simLane = static_cast<SimLane>(lane);
		const std::array<std::byte, 8> bytes = stamped(id, simLane, sent[id][static_cast<size_t>(simLane)]++);
		++delivered[id];
		endpoint.deliver(id, lane, bytes, now);
	}

	void drain()
	{
		transport->inbox().drain([&](const InboundSimRecord& record) {
			++records[record.conn];
			Seen& seen = state[record.conn];
			switch (record.kind)
			{
			case InboundKind::Connected:
				badConnected += seen == Seen::Nothing ? 0u : 1u;
				seen = Seen::Up;
				return;
			case InboundKind::Disconnected:
				badDisconnected += seen == Seen::Up ? 0u : 1u;
				++disconnectedDrained[record.conn];
				seen = Seen::Down;
				return;
			case InboundKind::Message:
				break;
			}
			if (seen != Seen::Up)
			{
				++messagesOutsideUp;
				return;
			}
			const size_t lane = static_cast<size_t>(record.lane);
			const std::uint32_t sequence = sequenceOf(record);
			std::array<std::uint32_t, kSimLaneCount>& next = expected[record.conn];
			outOfOrder[record.conn] += sequence < next[lane] ? 1u : 0u;
			next[lane] = sequence + 1u;
			if (record.lane == SimLane::Control)
				++controlArrived[record.conn];
		});
	}

	ChurnEndpoint endpoint;
	std::unique_ptr<Transport> transport;
	std::unordered_set<SimConnectionId> live;
	size_t maxLive = 0u;
	size_t endpointRefusedConnects = 0u;
	size_t applied = 0u;
	std::unordered_map<SimConnectionId, std::array<std::uint32_t, kSimLaneCount>> sent;
	std::unordered_map<SimConnectionId, std::uint64_t> delivered;
	std::unordered_map<SimConnectionId, std::uint64_t> records;
	std::unordered_map<SimConnectionId, Seen> state;
	std::unordered_map<SimConnectionId, std::uint64_t> disconnectedDrained;
	std::unordered_map<SimConnectionId, std::array<std::uint32_t, kSimLaneCount>> expected;
	std::unordered_map<SimConnectionId, std::uint64_t> outOfOrder;
	std::unordered_map<SimConnectionId, std::uint64_t> controlArrived;
	std::uint64_t badConnected = 0u;
	std::uint64_t badDisconnected = 0u;
	std::uint64_t messagesOutsideUp = 0u;
};

ChurnResult runChurn(const ChurnRun& run)
{
	ChurnRig rig;
	ChurnResult result;
	result.eventCapacity = rig.transport->inbox().eventCapacity();

	std::vector<SimConnectionId> stayers;
	auto topUp = [&](SimConnectionId id, double now) {
		for (std::uint32_t i = 0u; i < kTopUp; ++i)
			rig.deliver(id, ognet::Lane::SimInput, now);
		rig.deliver(id, ognet::Lane::SimClock, now);
	};
	auto stayerFrame = [&](double now) {
		for (const SimConnectionId id : stayers)
		{
			topUp(id, now);
			rig.deliver(id, ognet::Lane::Control, now);
			++result.stayerControlOffered;
		}
	};

	for (SimConnectionId id = 1u; id <= kSlots; ++id)
	{
		REQUIRE(rig.connect(id));
		topUp(id, 0.0);
		if (id <= kStayers)
			stayers.push_back(id);
	}
	for (SimConnectionId id = kStayers + 1u; id <= kSlots; ++id)
		rig.close(id);
	for (std::uint16_t slot = 0u; slot < kSlots; ++slot)
		CHECK(rig.transport->inbox().undrained(slot) == kQuota);

	std::unordered_set<SimConnectionId> churners;
	SimConnectionId nextChurner = 1000u;
	SimConnectionId firstAfterLongGap = 0xFFFFFFFFu;
	double now = 0.0;
	for (int gap = 0; gap < run.gaps; ++gap)
	{
		const int gapMs = gap == run.longGapIndex ? run.longGapMs : run.gapMs;
		const std::uint64_t pairs =
			(gap == 0 && run.fullBucket ? kHellosPerSecond : 0u) + kHellosPerSecond * static_cast<std::uint64_t>(gapMs) / 1000u;
		result.maxPairsInOneGap = std::max<size_t>(result.maxPairsInOneGap, static_cast<size_t>(pairs));
		now += gapMs / 1000.0;
		stayerFrame(now);
		for (std::uint64_t pair = 0u; pair < pairs; ++pair)
		{
			const SimConnectionId id = nextChurner++;
			if (!rig.connect(id))
				continue;
			churners.insert(id);
			topUp(id, now);
			rig.deliver(id, ognet::Lane::Control, now);
			rig.applyQueuedDisconnects();
			rig.close(id);
			if (pair % 16u == 0u)
				stayerFrame(now);
		}
		rig.drain();
		if (gap == run.longGapIndex)
			firstAfterLongGap = nextChurner;
	}
	result.stats = rig.transport->inbox().stats();
	result.transportStats = rig.transport->stats();

	std::unordered_map<SimConnectionId, int> requests;
	for (const ognet::ConnectionId id : rig.endpoint.disconnects)
		++requests[id];
	result.churners = churners.size();
	result.disconnectRequests = rig.endpoint.disconnects.size();
	for (const SimConnectionId id : churners)
	{
		result.announcedAfterTheLongGap += id >= firstAfterLongGap && rig.state[id] != Seen::Nothing ? 1u : 0u;
		if (rig.state[id] == Seen::Nothing)
		{
			++result.unannounced;
			result.unannouncedWithRecords += rig.records[id] == 0u ? 0u : 1u;
			result.unannouncedDisconnectRequestsWrong += requests[id] == 1 ? 0u : 1u;
			result.unannouncedMessagesDelivered += rig.delivered[id];
		}
		else
		{
			++result.announced;
			result.announcedWithoutOneDisconnected += rig.disconnectedDrained[id] == 1u ? 0u : 1u;
		}
	}
	for (const SimConnectionId id : stayers)
	{
		result.stayerControlArrived += rig.controlArrived[id];
		result.stayerOutOfOrder += rig.outOfOrder[id];
		result.stayersWithoutConnectedFirst += rig.state[id] == Seen::Up ? 0u : 1u;
	}
	for (const SimConnectionId id : stayers)
		rig.close(id);
	rig.drain();
	for (const SimConnectionId id : stayers)
		result.stayerDisconnectedAtEnd += rig.disconnectedDrained[id];
	result.maxLive = rig.maxLive;
	result.endpointRefusedConnects = rig.endpointRefusedConnects;
	result.badConnected = rig.badConnected;
	result.badDisconnected = rig.badDisconnected;
	result.messagesOutsideUp = rig.messagesOutsideUp;
	return result;
}

void checkConsistent(const ChurnResult& result)
{
	CHECK(result.maxLive <= kSlots);
	CHECK(result.endpointRefusedConnects == 0u);
	CHECK(result.badConnected == 0u);
	CHECK(result.badDisconnected == 0u);
	CHECK(result.messagesOutsideUp == 0u);
	CHECK(result.announcedWithoutOneDisconnected == 0u);
	CHECK(result.unannouncedWithRecords == 0u);
	CHECK(result.unannouncedDisconnectRequestsWrong == 0u);
	CHECK(result.transportStats.unannouncedConnects == result.unannounced);
	CHECK(result.transportStats.unannouncedRefused == result.unannouncedMessagesDelivered);
	CHECK(result.stats.eventsRefused == result.unannounced);
	CHECK(result.stats.eventHighWater <= result.eventCapacity);
	CHECK(result.stats.dropped[static_cast<size_t>(SimLane::Control)] == 0u);
	CHECK(result.stayersWithoutConnectedFirst == 0u);
	CHECK(result.stayerControlArrived == result.stayerControlOffered);
	CHECK(result.stayerOutOfOrder == 0u);
	CHECK(result.stayerDisconnectedAtEnd == kStayers);
	CHECK(result.transportStats.unslottedRefused == 0u);
	CHECK(result.transportStats.noSlotConnects == 0u);
	CHECK(result.transportStats.controlQuotaDisconnects == 0u);
	CHECK(result.disconnectRequests == result.unannounced);
}

TEST_CASE("SimNetTransport.TwoHundredConnectAndDisconnectPairsASecondWithEverySlotAtItsQuotaLoseNoConnectionEvent",
	"[NetTransport][SimNetIsolation]")
{
	const ChurnResult result = runChurn(ChurnRun{});
	CHECK(result.eventCapacity == 816u);
	CHECK(result.churners == 400u);
	CHECK(result.maxPairsInOneGap == 20u);
	CHECK(result.announced == 400u);
	CHECK(result.unannounced == 0u);
	CHECK(result.stats.eventHighWater == 64u);
	checkConsistent(result);
}

TEST_CASE("SimNetTransport.OgNetsWholeCookieBucketInOneDrainGapThenTwoHundredASecondLoseNoConnectionEvent",
	"[NetTransport][SimNetIsolation]")
{
	ChurnRun run;
	run.fullBucket = true;
	const ChurnResult result = runChurn(run);
	CHECK(result.maxPairsInOneGap == 220u);
	CHECK(result.churners == 600u);
	CHECK(result.announced == 600u);
	CHECK(result.unannounced == 0u);
	CHECK(result.stats.eventHighWater == 464u);
	checkConsistent(result);
}

TEST_CASE("SimNetTransport.ADrainGapLongerThanTheEventRingIsSizedForLeavesTheExtraConnectionsUnannouncedAndLosesNoEvent",
	"[NetTransport][SimNetIsolation]")
{
	ChurnRun run;
	run.fullBucket = true;
	run.gaps = 3;
	run.longGapIndex = 0;
	run.longGapMs = 3000;
	const ChurnResult result = runChurn(run);
	CHECK(result.maxPairsInOneGap == 800u);
	CHECK(result.churners == 840u);
	CHECK(result.announced == 392u + 40u);
	CHECK(result.unannounced == 408u);
	CHECK(result.announcedAfterTheLongGap == 40u);
	CHECK(result.stats.eventHighWater == 816u - kStayers);
	checkConsistent(result);
}

TEST_CASE("SimNetTransport.ThousandsOfSequentialConnectionsKeepTheEventRingsRoom", "[NetTransport][SimNetIsolation]")
{
	ChurnRig rig;
	SimConnectionId id = 1u;
	for (int round = 0; round < 500; ++round)
	{
		for (int pair = 0; pair < 10; ++pair, ++id)
		{
			REQUIRE(rig.connect(id));
			rig.deliver(id, ognet::Lane::SimInput, 0.0);
			rig.close(id);
		}
		rig.drain();
	}
	CHECK(rig.transport->inbox().stats().eventsRefused == 0u);
	CHECK(rig.transport->inbox().stats().events == 10000u);
	CHECK(rig.transport->inbox().stats().eventHighWater == 20u);
	CHECK(rig.transport->stats().unannouncedConnects == 0u);
	CHECK(rig.badConnected == 0u);
	CHECK(rig.badDisconnected == 0u);
	CHECK(rig.messagesOutsideUp == 0u);
}

TEST_CASE("SimNetTransport.AClientsEventRingHoldsFourReconnectsInOneDrainGap", "[NetTransport][SimNetIsolation]")
{
	ognet::EndpointConfig clientEndpoint;
	clientEndpoint.role = ognet::Role::Client;
	ChurnRig rig(ogsimnet::simNetConfigFor(clientEndpoint));
	CHECK(rig.transport->inbox().eventCapacity() == 9u);
	REQUIRE(rig.connect(1u));
	rig.drain();
	for (SimConnectionId id = 1u; id <= 5u; ++id)
	{
		rig.close(id);
		REQUIRE(rig.connect(id + 1u));
	}
	rig.drain();
	CHECK(rig.transport->stats().unannouncedConnects == 1u);
	CHECK(rig.transport->inbox().stats().eventsRefused == 1u);
	CHECK(rig.endpoint.disconnects == std::vector<ognet::ConnectionId>{6u});
	for (SimConnectionId id = 1u; id <= 5u; ++id)
	{
		INFO("connection " << id);
		CHECK(rig.disconnectedDrained[id] == 1u);
	}
	CHECK(rig.state[6u] == Seen::Nothing);
	rig.deliver(6u, ognet::Lane::SimRelay, 1.0);
	rig.close(6u);
	REQUIRE(rig.connect(7u));
	rig.drain();
	CHECK(rig.records[6u] == 0u);
	CHECK(rig.transport->stats().unannouncedRefused == 1u);
	CHECK(rig.state[7u] == Seen::Up);
	CHECK(rig.badConnected == 0u);
	CHECK(rig.badDisconnected == 0u);
}

TEST_CASE("SimNetTransport.ARepeatedConnectedIsPushedUnpairedSoItKeepsNoSecondSlot", "[NetTransport][SimNetIsolation]")
{
	ognet::EndpointConfig clientEndpoint;
	clientEndpoint.role = ognet::Role::Client;
	ChurnRig rig(ogsimnet::simNetConfigFor(clientEndpoint));
	for (int round = 0; round < 20; ++round)
	{
		rig.transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 5u});
		rig.transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 5u});
		rig.transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 5u});
		rig.state[5u] = Seen::Nothing;
		rig.drain();
	}
	CHECK(rig.records[5u] == 60u);
	CHECK(rig.transport->inbox().stats().eventsRefused == 0u);
	CHECK(rig.transport->stats().unannouncedConnects == 0u);
	CHECK(rig.disconnectedDrained[5u] == 20u);
	CHECK(rig.badConnected == 20u);
}

TEST_CASE("SimInbox.APairedConnectedKeepsASlotForItsDisconnectedAndAFullEventRingRefusesOnlyNewConnections", "[NetTransport][SimNetIsolation]")
{
	SimInboxConfig config;
	config.eventSlots = 5u;
	SimInbox<128> inbox(config);
	CHECK(inbox.pushPairedConnected(1u, 0.0) == SimInboxPush::Pushed);
	CHECK(inbox.pushPairedConnected(2u, 0.0) == SimInboxPush::Pushed);
	CHECK(inbox.pushPairedConnected(3u, 0.0) == SimInboxPush::EventRefused);
	CHECK(inbox.pushEvent(InboundKind::Disconnected, 0u, 0.0) == SimInboxPush::Pushed);
	CHECK(inbox.pushEvent(InboundKind::Disconnected, 0u, 0.0) == SimInboxPush::EventRefused);
	CHECK(inbox.pushPairedDisconnected(1u, 0.0) == SimInboxPush::Pushed);
	CHECK(inbox.pushPairedDisconnected(2u, 0.0, InboundCause::ControlQuota) == SimInboxPush::Pushed);
	CHECK(inbox.stats().eventHighWater == 5u);
	CHECK(inbox.stats().eventsRefused == 2u);
	CHECK(inbox.stats().events == 5u);

	const std::array<std::byte, 2> bytes{};
	std::vector<std::uint32_t> order;
	std::vector<InboundCause> causes;
	inbox.pushMessage(9u, SimLane::SimInput, 1.0, bytes);
	CHECK(inbox.drain([&](const InboundSimRecord& record) {
		order.push_back(record.conn);
		causes.push_back(record.cause);
	}) == 6u);
	CHECK(order == std::vector<std::uint32_t>{1u, 2u, 0u, 1u, 2u, 9u});
	CHECK(causes.back() == InboundCause::Unstated);
	CHECK(causes[4] == InboundCause::ControlQuota);
	CHECK(inbox.pushPairedConnected(3u, 0.0) == SimInboxPush::Pushed);
	CHECK(inbox.pushPairedConnected(4u, 0.0) == SimInboxPush::Pushed);
}

TEST_CASE("SimInbox.TheDrainKeepsThePushOrderAcrossTheSharedControlAndEventRings", "[NetTransport][SimNetIsolation]")
{
	SimInboxConfig config;
	config.controlSlots = 4u;
	config.eventSlots = 4u;
	SimInbox<128> inbox(config);
	const std::array<std::byte, 1> bytes{};
	inbox.pushPairedConnected(7u, 1.0);
	inbox.pushMessage(7u, SimLane::Control, 2.0, bytes);
	inbox.pushMessage(7u, SimLane::SimInput, 3.0, bytes);
	inbox.pushPairedConnected(8u, 4.0);
	inbox.pushMessage(8u, SimLane::SimInput, 5.0, bytes);
	inbox.pushMessage(7u, SimLane::Control, 6.0, bytes);
	inbox.pushPairedDisconnected(7u, 7.0);
	inbox.pushMessage(8u, SimLane::Control, 8.0, bytes);
	inbox.pushPairedDisconnected(8u, 9.0);
	CHECK(inbox.stats().eventsRefused == 0u);
	std::vector<std::uint32_t> conns;
	std::vector<InboundKind> kinds;
	inbox.drain([&](const InboundSimRecord& record) {
		conns.push_back(record.conn);
		kinds.push_back(record.kind);
	});
	CHECK(conns == std::vector<std::uint32_t>{7u, 7u, 7u, 8u, 8u, 7u, 7u, 8u, 8u});
	CHECK(kinds
		== std::vector<InboundKind>{InboundKind::Connected, InboundKind::Message, InboundKind::Message, InboundKind::Connected,
			InboundKind::Message, InboundKind::Message, InboundKind::Disconnected, InboundKind::Message, InboundKind::Disconnected});
	CHECK(inbox.stats().maxOccupancy == 2u);
}

TEST_CASE("SimInbox.Death.TheOldSixtyFourSlotEventReserveUnderTheCookieBucketChurnFiresTheEventCheck", "[.][OGSimDeath]")
{
	SimInboxConfig config = ogsimnet::simNetInboxConfig<2048>(ogsimnet::SimNetConfig{});
	config.eventSlots = 0u;
	auto inbox = std::make_unique<SimInbox<2048>>(config);
	const std::array<std::byte, 8> bytes{};
	for (std::uint16_t slot = 0u; slot < kSlots; ++slot)
	{
		REQUIRE(inbox->pushPairedConnected(1u + slot, 0.0) == SimInboxPush::Pushed);
		for (std::uint32_t i = 0u; i < kQuota; ++i)
			inbox->pushMessage(1u + slot, SimLane::SimInput, 0.0, bytes, slot);
	}
	for (SimConnectionId id = 1000u; id < 1220u; ++id)
	{
		inbox->pushPairedConnected(id, 0.0);
		inbox->pushPairedDisconnected(id, 0.0);
	}
	FAIL("220 connect and disconnect pairs in one drain gap, with every slot at its quota, did NOT fire the shared ring's event "
		 "OG_CHECK");
}

TEST_CASE("SimInbox.Death.APairedDisconnectedWithNoPairedConnectedFiresTheCheck", "[.][OGSimDeath]")
{
	SimInboxConfig config;
	config.eventSlots = 4u;
	SimInbox<128> inbox(config);
	inbox.pushPairedDisconnected(1u, 0.0);
	FAIL("a paired Disconnected with no paired Connected open did NOT fire SimInbox's OG_CHECK");
}

TEST_CASE("SimInbox.Death.AnEventRingOfOneSlotFiresTheCheck", "[.][OGSimDeath]")
{
	SimInboxConfig config;
	config.eventSlots = 1u;
	SimInbox<128> inbox(config);
	FAIL("an event ring of one slot did NOT fire SimInbox's OG_CHECK");
}

TEST_CASE("SimNetTransport.Death.NoConnectsPerDrainGapFiresTheCheck", "[.][OGSimDeath]")
{
	ogsimnet::SimNetConfig config;
	config.connectsPerDrainGap = 0u;
	auto transport = std::make_unique<Transport>(config);
	FAIL("a transport with connectsPerDrainGap 0 did NOT fire SimNetTransport's OG_CHECK");
}
} // namespace simNetConnectionEventTest
