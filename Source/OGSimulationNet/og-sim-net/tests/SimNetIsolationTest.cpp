// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/Lanes/LaneScheduler.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simNetIsolationTest
{
struct IsoPosition
{
	std::int32_t x = 0;
};

struct IsoInput
{
	std::int32_t move = 0;
};
} // namespace simNetIsolationTest

template <>
struct SerializableFields<simNetIsolationTest::IsoPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetIsolationTest::IsoPosition, x)); }
};

template <>
struct SerializableFields<simNetIsolationTest::IsoInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetIsolationTest::IsoInput, move)); }
};

namespace simNetIsolationTest
{
using IsoState = SimulationComposite<IsoPosition>;

struct IsoSim
{
	using StateType = IsoState;
	using InputType = IsoInput;
};

struct IsoGame
{
	using Simulatables = SimulatableList<IsoSim>;
};

struct RecordingEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) { ++sends; }
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) { ++sends; }
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { laneHandlers[static_cast<size_t>(lane)] = handler; }
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId id) { disconnects.push_back(id); }

	void deliver(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		const ognet::LaneHandler& handler = laneHandlers[static_cast<size_t>(lane)];
		handler.fn(handler.ctx, id, lane, bytes, recvSeconds);
	}

	std::array<ognet::LaneHandler, ognet::kLaneCount> laneHandlers{};
	int sends = 0;
	std::vector<ognet::ConnectionId> disconnects;
};

struct ServerHost
{
	double now = 0.0;
	std::vector<std::string> lines;

	double nowSeconds() { return now; }
	double rttMs(SimConnectionId) { return 20.0; }
	std::int32_t receptionTick() { return 0; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
};

struct ClientHost
{
	double now = 0.0;
	std::vector<std::string> lines;

	double nowSeconds() { return now; }
	double serverRttSeconds() { return 0.05; }
	void onTimingInfo(std::uint32_t, double) {}
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char* line) { lines.emplace_back(line); }
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

static_assert(ogsimnet::SimNetServerHost<ServerHost>);
static_assert(ogsimnet::SimNetClientHost<ClientHost>);

using Transport = ogsimnet::SimNetTransport<RecordingEndpoint, 2048>;
using SmallTransport = ogsimnet::SimNetTransport<RecordingEndpoint, 128>;

constexpr size_t kConnections = 16u;
constexpr SimConnectionId kFirstConn = 100u;
constexpr std::array<ognet::Lane, 3> kUpLanes{ognet::Lane::Control, ognet::Lane::SimClock, ognet::Lane::SimInput};

std::array<std::byte, 8> stamped(SimConnectionId conn, SimLane lane, std::uint32_t sequence)
{
	std::array<std::byte, 8> out{};
	out[0] = static_cast<std::byte>(conn & 0xFFu);
	out[1] = static_cast<std::byte>(lane);
	std::memcpy(out.data() + 4, &sequence, sizeof(sequence));
	return out;
}

std::uint32_t sequenceOf(const InboundSimRecord& record)
{
	std::uint32_t sequence = 0u;
	std::memcpy(&sequence, record.bytes.data() + 4, sizeof(sequence));
	return sequence;
}

std::uint64_t sumOf(const std::array<std::uint64_t, kSimLaneCount>& values)
{
	std::uint64_t total = 0u;
	for (const std::uint64_t value : values)
		total += value;
	return total;
}

template <class TransportT>
void start(TransportT& transport, RecordingEndpoint& endpoint)
{
	transport.installHandlers(endpoint);
	transport.markEndpointStarted();
}

struct FloodRun
{
	size_t flooders = 0u;
	int drainEveryFrames = 1;
	int seconds = 20;
};

struct FloodResult
{
	bool correctOverOgNetCap = false;
	std::array<std::uint64_t, kSimLaneCount> correctOffered{};
	std::array<std::uint64_t, kSimLaneCount> correctArrived{};
	std::uint64_t correctOutOfSequence = 0u;
	std::uint64_t flooderControlOffered = 0u;
	std::uint64_t flooderControlArrived = 0u;
	std::uint64_t flooderControlOutOfSequence = 0u;
	std::uint64_t flooderOffered = 0u;
	std::uint64_t correctQuotaRefused = 0u;
	std::uint64_t flooderQuotaRefused = 0u;
	SimInboxStats stats;
	ogsimnet::SimNetTransportStats transportStats;
	size_t disconnectsRequested = 0u;
	size_t controlCapacity = 0u;
};

FloodResult runFlood(const FloodRun& run)
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<Transport>();
	start(*transport, endpoint);

	using ognet::lanes::LaneScheduler;
	std::vector<std::unique_ptr<LaneScheduler>> admitted;
	std::vector<std::unique_ptr<LaneScheduler>> mirror;
	for (size_t c = 0u; c < kConnections; ++c)
	{
		admitted.push_back(std::make_unique<LaneScheduler>(ognet::Role::Server, ognet::lanes::kDefaultLaneTable, ognet::lanes::kDefaultBudget, 0.0));
		mirror.push_back(std::make_unique<LaneScheduler>(ognet::Role::Server, ognet::lanes::kDefaultLaneTable, ognet::lanes::kDefaultBudget, 0.0));
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, kFirstConn + static_cast<SimConnectionId>(c)});
	}

	FloodResult result;
	std::vector<std::array<std::uint32_t, kSimLaneCount>> sent(kConnections);
	std::vector<std::array<std::uint32_t, kSimLaneCount>> expected(kConnections);
	auto isFlooder = [&](size_t c) { return c < run.flooders; };
	auto drainAll = [&] {
		transport->inbox().drain([&](const InboundSimRecord& record) {
			if (record.kind != InboundKind::Message)
				return;
			const size_t c = record.conn - kFirstConn;
			const size_t lane = static_cast<size_t>(record.lane);
			const std::uint32_t sequence = sequenceOf(record);
			if (!isFlooder(c))
			{
				++result.correctArrived[lane];
				result.correctOutOfSequence += sequence == expected[c][lane] ? 0u : 1u;
				expected[c][lane] = sequence + 1u;
			}
			else if (record.lane == SimLane::Control)
			{
				++result.flooderControlArrived;
				result.flooderControlOutOfSequence += sequence == expected[c][lane] ? 0u : 1u;
				expected[c][lane] = sequence + 1u;
			}
		});
	};
	drainAll();

	auto deliver = [&](size_t c, ognet::Lane lane, double now) {
		const SimConnectionId conn = kFirstConn + static_cast<SimConnectionId>(c);
		const SimLane simLane = static_cast<SimLane>(lane);
		const std::array<std::byte, 8> bytes = stamped(conn, simLane, sent[c][static_cast<size_t>(simLane)]++);
		endpoint.deliver(conn, lane, bytes, now);
	};

	const int frames = run.seconds * 60;
	for (int frame = 1; frame <= frames; ++frame)
	{
		const double now = frame / 60.0;
		if (frame % 60 == 0)
		{
			for (size_t c = 0u; c < run.flooders; ++c)
			{
				for (const ognet::Lane lane : kUpLanes)
				{
					for (int k = 0; k < 5000; ++k)
					{
						if (mirror[c]->admitReceived(lane, now) != ognet::lanes::ReceiveVerdict::Accept)
							break;
						admitted[c]->admitReceived(lane, now);
						deliver(c, lane, now);
						++result.flooderOffered;
						if (lane == ognet::Lane::Control)
							++result.flooderControlOffered;
					}
				}
			}
		}
		for (size_t c = run.flooders; c < kConnections; ++c)
		{
			auto offer = [&](ognet::Lane lane) {
				mirror[c]->admitReceived(lane, now);
				if (admitted[c]->admitReceived(lane, now) != ognet::lanes::ReceiveVerdict::Accept)
				{
					result.correctOverOgNetCap = true;
					return;
				}
				++result.correctOffered[static_cast<size_t>(lane)];
				deliver(c, lane, now);
			};
			offer(ognet::Lane::SimInput);
			offer(ognet::Lane::SimInput);
			offer(ognet::Lane::SimClock);
			if ((static_cast<size_t>(frame) + c) % 7u == 0u)
				offer(ognet::Lane::Control);
		}
		if (frame % run.drainEveryFrames == 0)
			drainAll();
	}
	drainAll();

	for (size_t c = 0u; c < kConnections; ++c)
	{
		const std::uint64_t refused = sumOf(transport->quotaRefused(kFirstConn + static_cast<SimConnectionId>(c)));
		(isFlooder(c) ? result.flooderQuotaRefused : result.correctQuotaRefused) += refused;
	}
	result.stats = transport->inbox().stats();
	result.transportStats = transport->stats();
	result.disconnectsRequested = endpoint.disconnects.size();
	result.controlCapacity = transport->inbox().controlCapacity();
	return result;
}

void checkIsolated(const FloodResult& result)
{
	CHECK_FALSE(result.correctOverOgNetCap);
	for (size_t lane = 0u; lane < kSimLaneCount; ++lane)
	{
		INFO("lane " << lane);
		CHECK(result.correctArrived[lane] == result.correctOffered[lane]);
	}
	CHECK(result.correctOffered[static_cast<size_t>(SimLane::Control)] > 0u);
	CHECK(result.correctOffered[static_cast<size_t>(SimLane::SimInput)] > 0u);
	CHECK(result.correctOutOfSequence == 0u);
	CHECK(result.correctQuotaRefused == 0u);
	CHECK(result.flooderControlArrived == result.flooderControlOffered);
	CHECK(result.flooderControlOutOfSequence == 0u);
	CHECK(sumOf(result.stats.dropped) == 0u);
	CHECK(result.stats.tooLarge == 0u);
	CHECK(result.stats.controlHighWater <= result.controlCapacity);
	CHECK(result.transportStats.controlQuotaDisconnects == 0u);
	CHECK(result.disconnectsRequested == 0u);
	CHECK(result.transportStats.unslottedRefused == 0u);
}

TEST_CASE("SimInbox.WithAControlRingTheDrainKeepsThePushOrderAcrossBothRings", "[NetTransport][SimNetIsolation]")
{
	SimInbox<128> inbox(SimInboxConfig{8u, 2u, 4u, 4u});
	const std::array<std::byte, 3> bytes{std::byte{1}, std::byte{2}, std::byte{3}};
	inbox.pushEvent(InboundKind::Connected, 7u, -1.0);
	REQUIRE(inbox.pushMessage(7u, SimLane::Control, 1.0, bytes, 0u));
	REQUIRE(inbox.pushMessage(7u, SimLane::SimInput, 2.0, bytes, 0u));
	REQUIRE(inbox.pushMessage(7u, SimLane::Control, 3.0, bytes, 0u));
	REQUIRE(inbox.pushMessage(8u, SimLane::SimClock, 4.0, bytes, 1u));
	REQUIRE(inbox.pushMessage(8u, SimLane::Control, 5.0, bytes));
	inbox.pushEvent(InboundKind::Disconnected, 7u, -1.0);
	CHECK(inbox.undrained(0u) == 1u);
	CHECK(inbox.undrained(1u) == 1u);
	CHECK(inbox.controlUndrained(0u) == 2u);
	CHECK(inbox.controlUndrained(1u) == 0u);

	std::vector<double> order;
	std::vector<std::uint16_t> tags;
	const size_t handed = inbox.drain([&](const InboundSimRecord& record) {
		order.push_back(record.recvSeconds);
		tags.push_back(record.tag);
		if (record.kind == InboundKind::Message)
			CHECK(record.payload().size() == bytes.size());
	});
	CHECK(handed == 7u);
	CHECK(order == std::vector<double>{-1.0, 1.0, 2.0, 3.0, 4.0, 5.0, -1.0});
	CHECK(tags == std::vector<std::uint16_t>{kNoInboxTag, 0u, 0u, 0u, 1u, kNoInboxTag, kNoInboxTag});
	CHECK(inbox.undrained(0u) == 0u);
	CHECK(inbox.undrained(1u) == 0u);
	CHECK(inbox.controlUndrained(0u) == 0u);

	const SimInboxStats stats = inbox.stats();
	CHECK(stats.pushed[static_cast<size_t>(SimLane::Control)] == 3u);
	CHECK(stats.controlHighWater == 3u);
	CHECK(stats.maxOccupancy == 4u);
	CHECK(stats.events == 2u);
	CHECK(inbox.drain([](const InboundSimRecord&) {}) == 0u);
}

TEST_CASE("SimInbox.ControlTakesItsOwnRingWhenTheSharedRingHasOnlyItsReserveLeft", "[NetTransport][SimNetIsolation]")
{
	SimInbox<128> inbox(SimInboxConfig{4u, 0u, 0u});
	const std::array<std::byte, 2> bytes{};
	for (size_t i = 0u; i < 64u; ++i)
		REQUIRE(inbox.pushMessage(1u, SimLane::SimInput, 0.0, bytes));
	CHECK_FALSE(inbox.pushMessage(1u, SimLane::SimInput, 0.0, bytes));
	for (size_t i = 0u; i < 4u; ++i)
		REQUIRE(inbox.pushMessage(1u, SimLane::Control, 0.0, bytes));
	inbox.pushEvent(InboundKind::Connected, 2u, 0.0);

	const SimInboxStats stats = inbox.stats();
	CHECK(stats.dropped[static_cast<size_t>(SimLane::SimInput)] == 1u);
	CHECK(stats.dropped[static_cast<size_t>(SimLane::Control)] == 0u);
	CHECK(stats.controlHighWater == 4u);
	CHECK(inbox.drain([](const InboundSimRecord&) {}) == 69u);

	const std::array<std::byte, kMaxSimMessageBytes + 1u> over{};
	CHECK_FALSE(inbox.pushMessage(1u, SimLane::Control, 0.0, over));
	CHECK(inbox.stats().tooLarge == 1u);
}

TEST_CASE("SimInbox.ATaggedMessageOverItsQuotaIsRefusedAndCountedPerTagAndLaneButControlNeverIs", "[NetTransport][SimNetIsolation]")
{
	SimInbox<128> inbox(SimInboxConfig{0u, 2u, 3u});
	const std::array<std::byte, 2> bytes{};
	for (int i = 0; i < 3; ++i)
		REQUIRE(inbox.pushMessage(1u, SimLane::SimInput, 0.0, bytes, 0u));
	CHECK_FALSE(inbox.pushMessage(1u, SimLane::SimInput, 0.0, bytes, 0u));
	CHECK_FALSE(inbox.pushMessage(1u, SimLane::SimClock, 0.0, bytes, 0u));
	CHECK(inbox.pushMessage(1u, SimLane::Control, 0.0, bytes, 0u));
	CHECK(inbox.undrained(0u) == 4u);
	CHECK(inbox.pushMessage(2u, SimLane::SimInput, 0.0, bytes, 1u));
	CHECK(inbox.pushMessage(3u, SimLane::SimInput, 0.0, bytes));
	CHECK(inbox.pushMessage(3u, SimLane::SimInput, 0.0, bytes, 9u));
	CHECK(inbox.undrained(9u) == 0u);

	SimInboxStats stats = inbox.stats();
	CHECK(stats.quotaRefused[static_cast<size_t>(SimLane::SimInput)] == 1u);
	CHECK(stats.quotaRefused[static_cast<size_t>(SimLane::SimClock)] == 1u);
	CHECK(sumOf(stats.dropped) == 0u);
	const std::array<std::uint64_t, kSimLaneCount> perTag = inbox.quotaRefusedFor(0u);
	CHECK(perTag[static_cast<size_t>(SimLane::SimInput)] == 1u);
	CHECK(perTag[static_cast<size_t>(SimLane::SimClock)] == 1u);
	CHECK(sumOf(inbox.quotaRefusedFor(1u)) == 0u);

	CHECK(inbox.drain([](const InboundSimRecord&) {}) == 7u);
	CHECK(inbox.undrained(0u) == 0u);
	CHECK(inbox.pushMessage(1u, SimLane::SimInput, 0.0, bytes, 0u));
	inbox.resetTagCounters(0u);
	CHECK(sumOf(inbox.quotaRefusedFor(0u)) == 0u);
	stats = inbox.stats();
	CHECK(sumOf(stats.quotaRefused) == 2u);
}

TEST_CASE("SimInbox.OneProducerAndOneConsumerOverBothRingsKeepThePushOrderAndTheTagCounts", "[NetTransport][SimNetIsolation]")
{
	constexpr std::uint32_t kRecords = 400000u;
	constexpr std::uint32_t kControlQuota = 10u;
	auto inbox = std::make_unique<SimInbox<256>>(SimInboxConfig{4u * kControlQuota, 4u, 40u, kControlQuota});
	auto laneOf = [](std::uint32_t sequence) { return sequence % 50u == 0u ? SimLane::Control : static_cast<SimLane>(1u + sequence % 4u); };
	std::atomic<bool> producerDone{false};
	std::uint64_t refused = 0u;
	std::thread producer([&] {
		for (std::uint32_t sequence = 0u; sequence < kRecords; ++sequence)
		{
			const SimLane lane = laneOf(sequence);
			const std::uint16_t tag = static_cast<std::uint16_t>(sequence % 4u);
			std::array<std::byte, 8> bytes{};
			std::memcpy(bytes.data() + 4, &sequence, sizeof(sequence));
			while (!inbox->pushMessage(1u + tag, lane, static_cast<double>(sequence), bytes, tag))
				++refused;
		}
		producerDone.store(true, std::memory_order_release);
	});

	std::uint32_t next = 0u;
	std::uint64_t outOfOrder = 0u;
	std::uint64_t badRecords = 0u;
	for (;;)
	{
		const bool doneBefore = producerDone.load(std::memory_order_acquire);
		const size_t got = inbox->drain([&](const InboundSimRecord& record) {
			const std::uint32_t sequence = sequenceOf(record);
			outOfOrder += sequence == next ? 0u : 1u;
			badRecords += record.lane == laneOf(sequence) && record.conn == 1u + sequence % 4u
					&& record.recvSeconds == static_cast<double>(sequence)
				? 0u
				: 1u;
			next = sequence + 1u;
		});
		if (doneBefore && got == 0u)
			break;
	}
	producer.join();

	CHECK(next == kRecords);
	CHECK(outOfOrder == 0u);
	CHECK(badRecords == 0u);
	for (std::uint16_t tag = 0u; tag < 4u; ++tag)
	{
		CHECK(inbox->undrained(tag) == 0u);
		CHECK(inbox->controlUndrained(tag) == 0u);
	}
	const SimInboxStats stats = inbox->stats();
	CHECK(sumOf(stats.pushed) == kRecords);
	CHECK(sumOf(stats.dropped) + sumOf(stats.quotaRefused) == refused);
	CHECK(stats.dropped[static_cast<size_t>(SimLane::Control)] == 0u);
	CHECK(stats.pushed[static_cast<size_t>(SimLane::Control)] == kRecords / 50u);
	CHECK(stats.controlHighWater <= 4u * kControlQuota);
}

TEST_CASE("SimNetTransport.TheInboxIsSizedFromTheEndpointConfigForEachRole", "[NetTransport][SimNetIsolation]")
{
	auto server = std::make_unique<Transport>();
	CHECK(server->inbox().controlCapacity() == 7296u);
	CHECK(server->inbox().tagCount() == 16u);
	CHECK(server->inbox().tagQuota() == 124u);
	CHECK(server->inbox().tagControlQuota() == 456u);

	ognet::EndpointConfig clientEndpoint;
	clientEndpoint.role = ognet::Role::Client;
	auto client = std::make_unique<Transport>(ogsimnet::simNetConfigFor(clientEndpoint));
	CHECK(client->inbox().controlCapacity() == 456u);
	CHECK(client->inbox().tagCount() == 1u);
	CHECK(client->inbox().tagQuota() == 0u);
	CHECK(client->inbox().tagControlQuota() == 456u);
	RecordingEndpoint clientSide;
	start(*client, clientSide);
	client->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 51u});
	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 2000; ++i)
		clientSide.deliver(51u, ognet::Lane::SimRelay, bytes, 1.0);
	CHECK(client->inbox().stats().dropped[static_cast<size_t>(SimLane::SimRelay)] == 16u);
	CHECK(sumOf(client->inbox().stats().quotaRefused) == 0u);
	CHECK(client->stats().unslottedRefused == 0u);

	ognet::EndpointConfig serverEndpoint;
	serverEndpoint.role = ognet::Role::Server;
	serverEndpoint.maxConnections = 8u;
	const ogsimnet::SimNetConfig eight = ogsimnet::simNetConfigFor(serverEndpoint);
	auto small = std::make_unique<Transport>(eight);
	CHECK(small->inbox().controlCapacity() == 8u * 456u);
	CHECK(small->inbox().tagQuota() == 248u);
	CHECK(sizeof(InboundSimRecord) * (2048u + 7296u) == 5008384u);
}

TEST_CASE("SimNetTransport.EachConnectionHasItsOwnQuotaAndControlPastItsQuotaDisconnectsTheConnection", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	auto transport = std::make_unique<SmallTransport>(config);
	start(*transport, endpoint);
	REQUIRE(transport->inbox().tagQuota() == 32u);
	REQUIRE(transport->inbox().tagControlQuota() == 456u);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 11u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 12u});

	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 40; ++i)
		endpoint.deliver(11u, ognet::Lane::SimInput, bytes, 1.0);
	for (int i = 0; i < 600; ++i)
		endpoint.deliver(11u, ognet::Lane::Control, bytes, 1.0);
	for (int i = 0; i < 10; ++i)
		endpoint.deliver(12u, ognet::Lane::SimClock, bytes, 1.0);
	for (int i = 0; i < 5; ++i)
		endpoint.deliver(11u, ognet::Lane::SimInput, bytes, 1.0);

	CHECK(transport->quotaRefused(11u)[static_cast<size_t>(SimLane::SimInput)] == 8u);
	CHECK(transport->quotaRefused(11u)[static_cast<size_t>(SimLane::Control)] == 1u);
	CHECK(sumOf(transport->quotaRefused(11u)) == 9u);
	CHECK(sumOf(transport->quotaRefused(12u)) == 0u);
	CHECK(sumOf(transport->quotaRefused(99u)) == 0u);
	CHECK(endpoint.disconnects == std::vector<ognet::ConnectionId>{11u});
	CHECK(transport->stats().controlQuotaDisconnects == 1u);
	CHECK(transport->stats().closingRefused == 143u + 5u);
	CHECK(transport->stats().unslottedRefused == 0u);

	std::array<size_t, 2> messages{};
	size_t control = 0u;
	transport->inbox().drain([&](const InboundSimRecord& record) {
		if (record.kind != InboundKind::Message)
			return;
		++messages[record.conn - 11u];
		control += record.lane == SimLane::Control ? 1u : 0u;
	});
	CHECK(messages[0] == 32u + 456u);
	CHECK(messages[1] == 10u);
	CHECK(control == 456u);
	CHECK(transport->inbox().stats().controlHighWater == 456u);
	CHECK(sumOf(transport->inbox().stats().dropped) == 0u);

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 11u});
	std::vector<InboundCause> causes;
	transport->inbox().drain([&](const InboundSimRecord& record) {
		if (record.kind == InboundKind::Disconnected)
			causes.push_back(record.cause);
	});
	CHECK(causes == std::vector<InboundCause>{InboundCause::ControlQuota});

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 13u});
	CHECK(transport->slotOf(13u) == 0u);
	for (int i = 0; i < 33; ++i)
		endpoint.deliver(13u, ognet::Lane::SimInput, bytes, 2.0);
	CHECK(transport->quotaRefused(13u)[static_cast<size_t>(SimLane::SimInput)] == 1u);
	CHECK(sumOf(transport->quotaRefused(13u)) == 1u);
}

TEST_CASE("SimNetTransport.AClosedConnectionsUndrainedMessagesCountAgainstItsSlotWhichANewConnectionTakesAtOnce", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	auto transport = std::make_unique<SmallTransport>(config);
	start(*transport, endpoint);
	REQUIRE(transport->inbox().tagQuota() == 32u);
	const std::array<std::byte, 8> bytes{};

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 21u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 23u});
	for (int i = 0; i < 20; ++i)
		endpoint.deliver(21u, ognet::Lane::SimInput, bytes, 1.0);
	for (int i = 0; i < 5; ++i)
		endpoint.deliver(21u, ognet::Lane::Control, bytes, 1.0);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 21u});
	REQUIRE(transport->slotOf(21u) == kNoInboxTag);
	REQUIRE(transport->inbox().undrained(0u) == 20u);
	REQUIRE(transport->inbox().controlUndrained(0u) == 5u);

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 22u});
	CHECK(transport->slotOf(22u) == 0u);
	for (int i = 0; i < 20; ++i)
		endpoint.deliver(22u, ognet::Lane::SimInput, bytes, 1.0);
	for (int i = 0; i < 452; ++i)
		endpoint.deliver(22u, ognet::Lane::Control, bytes, 1.0);
	CHECK(transport->stats().unslottedRefused == 0u);
	CHECK(transport->quotaRefused(22u)[static_cast<size_t>(SimLane::SimInput)] == 8u);
	CHECK(transport->quotaRefused(22u)[static_cast<size_t>(SimLane::Control)] == 1u);
	CHECK(endpoint.disconnects == std::vector<ognet::ConnectionId>{22u});
	CHECK(transport->inbox().undrained(0u) == 32u);
	CHECK(transport->inbox().controlUndrained(0u) == 456u);
	CHECK(transport->inbox().undrained(1u) == 0u);

	std::array<size_t, 2> messages{};
	std::vector<InboundCause> causes;
	transport->inbox().drain([&](const InboundSimRecord& record) {
		if (record.kind == InboundKind::Disconnected)
			causes.push_back(record.cause);
		if (record.kind == InboundKind::Message)
			++messages[record.conn - 21u];
	});
	CHECK(messages[0] == 25u);
	CHECK(messages[1] == 12u + 451u);
	CHECK(causes == std::vector<InboundCause>{InboundCause::Unstated});
	CHECK(transport->inbox().undrained(0u) == 0u);
	CHECK(transport->inbox().controlUndrained(0u) == 0u);
	CHECK(sumOf(transport->quotaRefused(23u)) == 0u);
}

TEST_CASE("SimNetTransport.ANewConnectionTakesTheFreeSlotWithTheLeastResidueAndOneWithNoSlotIsRefused", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 3u;
	auto transport = std::make_unique<SmallTransport>(config);
	start(*transport, endpoint);
	const std::array<std::byte, 8> bytes{};
	auto connect = [&](SimConnectionId id) { transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, id}); };
	auto close = [&](SimConnectionId id) { transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, id}); };

	connect(31u);
	connect(32u);
	connect(33u);
	REQUIRE(transport->slotOf(31u) == 0u);
	REQUIRE(transport->slotOf(32u) == 1u);
	REQUIRE(transport->slotOf(33u) == 2u);
	for (int i = 0; i < 3; ++i)
		endpoint.deliver(31u, ognet::Lane::SimInput, bytes, 1.0);
	endpoint.deliver(32u, ognet::Lane::Control, bytes, 1.0);
	close(31u);
	close(32u);
	connect(34u);
	CHECK(transport->slotOf(34u) == 1u);
	close(33u);
	connect(35u);
	CHECK(transport->slotOf(35u) == 2u);
	connect(36u);
	CHECK(transport->slotOf(36u) == 0u);

	endpoint.deliver(37u, ognet::Lane::Control, bytes, 1.0);
	endpoint.deliver(37u, ognet::Lane::SimInput, bytes, 1.0);
	CHECK(transport->slotOf(37u) == kNoInboxTag);
	CHECK(transport->stats().unslottedRefused == 2u);
	CHECK(transport->stats().noSlotConnects == 0u);
	size_t from37 = 0u;
	transport->inbox().drain([&](const InboundSimRecord& record) { from37 += record.conn == 37u ? 1u : 0u; });
	CHECK(from37 == 0u);
	CHECK(endpoint.disconnects.empty());
}

TEST_CASE("SimNetTransport.TwoFloodersEveryFrameDropNoCorrectMessage", "[NetTransport][SimNetIsolation]")
{
	const FloodResult result = runFlood(FloodRun{2u, 1});
	checkIsolated(result);
	CHECK(result.flooderQuotaRefused > 0u);
}

TEST_CASE("SimNetTransport.FourFloodersEveryFrameDropNoCorrectMessage", "[NetTransport][SimNetIsolation]")
{
	const FloodResult result = runFlood(FloodRun{4u, 1});
	checkIsolated(result);
	CHECK(result.flooderQuotaRefused > 0u);
}

TEST_CASE("SimNetTransport.FiveFloodersEveryFrameLoseNoControlMessage", "[NetTransport][SimNetIsolation]")
{
	const FloodResult result = runFlood(FloodRun{5u, 1});
	checkIsolated(result);
	CHECK(result.stats.controlHighWater > 1984u);
}

TEST_CASE("SimNetTransport.FifteenFloodersEveryFrameDropNoCorrectMessage", "[NetTransport][SimNetIsolation]")
{
	const FloodResult result = runFlood(FloodRun{15u, 1});
	checkIsolated(result);
	CHECK(result.flooderQuotaRefused > 0u);
	CHECK(result.stats.controlHighWater >= 15u * 456u);
}

TEST_CASE("SimNetTransport.OneFlooderAndAHundredMillisecondHitchDropNoCorrectMessage", "[NetTransport][SimNetIsolation]")
{
	const FloodResult result = runFlood(FloodRun{1u, 6});
	checkIsolated(result);
	CHECK(result.flooderQuotaRefused > 0u);
}

TEST_CASE("SimNetServer.TheWindowLineCountsQuotaRefusalsAndTheControlHighWaterMark", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(config);
	start(*transport, endpoint);
	ServerHost host;
	TimeConfig timeConfig;
	ogsimnet::SimNetServer<IsoGame, ServerHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> server(host, *transport, timeConfig,
		size_t{4096});

	host.now = 0.0;
	server.afterInlineBatch(0);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 31u});
	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 100; ++i)
		endpoint.deliver(31u, ognet::Lane::SimInput, bytes, 1.0);
	for (int i = 0; i < 3; ++i)
		endpoint.deliver(31u, ognet::Lane::Control, bytes, 1.0);
	server.drain();
	host.now = 10.0;
	server.afterInlineBatch(0);

	std::vector<std::string> windows;
	for (const std::string& line : host.lines)
	{
		if (line.find("[SimNet.Window]") != std::string::npos)
			windows.push_back(line);
	}
	REQUIRE(windows.size() == 1u);
	CHECK(windows[0]
		== "[Warning][SimNet.Window] role=server drains=1 msgs=99 maxDepth=100 drops=0 tooLarge=0 badDecode=99 refusedNotOwner=0 "
		   "unknownSim=0 statesSent=0 relaysSent=0 overBudget=0 quotaRefused=4 controlHighWater=3 quotaDisconnects=0 "
		   "eventsRefused=0 eventHighWater=1");
}

TEST_CASE("SimNetClient.TheWindowLineCountsQuotaRefusalsAndTheControlHighWaterMark", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(config);
	start(*transport, endpoint);
	ClientHost host;
	ogsimnet::SimNetClient<IsoGame, ClientHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> client(host, *transport);

	host.now = 0.0;
	client.flushFrame();
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 41u});
	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 100; ++i)
		endpoint.deliver(41u, ognet::Lane::SimRelay, bytes, 1.0);
	for (int i = 0; i < 5; ++i)
		endpoint.deliver(41u, ognet::Lane::Control, bytes, 1.0);
	client.drain();
	host.now = 10.0;
	client.flushFrame();

	std::vector<std::string> windows;
	for (const std::string& line : host.lines)
	{
		if (line.find("[SimNet.Window]") != std::string::npos)
			windows.push_back(line);
	}
	REQUIRE(windows.size() == 1u);
	CHECK(windows[0]
		== "[Warning][SimNet.Window] role=client drains=1 msgs=101 maxDepth=102 drops=0 tooLarge=0 badDecode=101 refusedNotOwner=0 "
		   "unknownSim=0 statesSent=0 relaysSent=0 overBudget=0 staleDropped=0 duplicateDropped=0 quotaRefused=4 controlHighWater=5 "
		   "quotaDisconnects=0 eventsRefused=0 eventHighWater=1");
}

std::vector<std::string> windowLines(const std::vector<std::string>& lines)
{
	std::vector<std::string> windows;
	for (const std::string& line : lines)
	{
		if (line.find("[SimNet.Window]") != std::string::npos)
			windows.push_back(line);
	}
	return windows;
}

TEST_CASE("SimNetServer.TheWindowLineCountsRefusedConnectionEventsAndTheEventHighWaterMark", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	config.connectsPerDrainGap = 1u;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(config);
	start(*transport, endpoint);
	REQUIRE(transport->inbox().eventCapacity() == 4u);
	ServerHost host;
	TimeConfig timeConfig;
	ogsimnet::SimNetServer<IsoGame, ServerHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> server(host, *transport, timeConfig,
		size_t{4096});

	host.now = 0.0;
	server.afterInlineBatch(0);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 31u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 32u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 31u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 33u});
	CHECK(endpoint.disconnects == std::vector<ognet::ConnectionId>{33u});
	CHECK(transport->inbox().stats().eventsRefused == 1u);
	CHECK(transport->inbox().stats().eventHighWater == 3u);
	server.drain();
	host.now = 10.0;
	server.afterInlineBatch(0);
	server.drain();
	host.now = 20.0;
	server.afterInlineBatch(0);

	const std::vector<std::string> windows = windowLines(host.lines);
	REQUIRE(windows.size() == 2u);
	CHECK(windows[0].ends_with(" quotaDisconnects=0 eventsRefused=1 eventHighWater=3"));
	CHECK(windows[1].ends_with(" quotaDisconnects=0 eventsRefused=0 eventHighWater=3"));
}

TEST_CASE("SimNetClient.TheWindowLineCountsRefusedConnectionEventsAndTheEventHighWaterMark", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ognet::EndpointConfig clientEndpoint;
	clientEndpoint.role = ognet::Role::Client;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(ogsimnet::simNetConfigFor(clientEndpoint));
	start(*transport, endpoint);
	REQUIRE(transport->inbox().eventCapacity() == 9u);
	ClientHost host;
	ogsimnet::SimNetClient<IsoGame, ClientHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> client(host, *transport);

	host.now = 0.0;
	client.flushFrame();
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 41u});
	client.drain();
	for (ognet::ConnectionId id = 41u; id <= 45u; ++id)
	{
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, id});
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, id + 1u});
	}
	CHECK(endpoint.disconnects == std::vector<ognet::ConnectionId>{46u});
	CHECK(transport->inbox().stats().eventsRefused == 1u);
	CHECK(transport->inbox().stats().eventHighWater == 9u);
	client.drain();
	host.now = 10.0;
	client.flushFrame();
	client.drain();
	host.now = 20.0;
	client.flushFrame();

	const std::vector<std::string> windows = windowLines(host.lines);
	REQUIRE(windows.size() == 2u);
	CHECK(windows[0].ends_with(" quotaDisconnects=0 eventsRefused=1 eventHighWater=9"));
	CHECK(windows[1].ends_with(" quotaDisconnects=0 eventsRefused=0 eventHighWater=9"));
}

constexpr std::uint32_t kChurnQuota = 456u;
constexpr int kMaxBurstPerLane = 5000;

struct ChurnResult
{
	std::array<std::uint64_t, kSimLaneCount> correctOffered{};
	std::array<std::uint64_t, kSimLaneCount> correctArrived{};
	std::uint64_t correctOutOfSequence = 0u;
	size_t flooders = 0u;
	size_t maxLive = 0u;
	size_t connectsRefusedByEndpoint = 0u;
	std::unordered_map<SimConnectionId, int> disconnectRequests;
	std::unordered_set<std::uint16_t> slotsClosedOverQuota;
	std::uint64_t correctDisconnectRequests = 0u;
	std::uint64_t quotaCauseDisconnects = 0u;
	std::uint64_t unstatedDisconnects = 0u;
	std::uint64_t deliveredWhileClosing = 0u;
	size_t offenders = 0u;
	size_t leftUnderQuota = 0u;
	std::uint64_t unexpectedResidue = 0u;
	std::uint64_t highWaterAfterAllSixteen = 0u;
	SimInboxStats stats;
	ogsimnet::SimNetTransportStats transportStats;
	size_t controlCapacity = 0u;
};

ChurnResult runChurn()
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<Transport>();
	start(*transport, endpoint);
	ChurnResult result;
	result.controlCapacity = transport->inbox().controlCapacity();

	std::vector<SimConnectionId> live;
	std::unordered_set<SimConnectionId> correct;
	std::unordered_map<SimConnectionId, std::array<std::uint32_t, kSimLaneCount>> sent;
	std::unordered_map<SimConnectionId, std::array<std::uint32_t, kSimLaneCount>> expected;
	size_t appliedDisconnects = 0u;

	auto connect = [&](SimConnectionId id) {
		if (live.size() >= kConnections)
		{
			++result.connectsRefusedByEndpoint;
			return false;
		}
		live.push_back(id);
		result.maxLive = std::max(result.maxLive, live.size());
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, id});
		return true;
	};
	auto close = [&](SimConnectionId id) {
		std::erase(live, id);
		transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, id});
	};
	auto applyQueuedDisconnects = [&] {
		for (; appliedDisconnects < endpoint.disconnects.size(); ++appliedDisconnects)
			close(endpoint.disconnects[appliedDisconnects]);
	};
	auto deliver = [&](SimConnectionId id, ognet::Lane lane, double now) {
		const SimLane simLane = static_cast<SimLane>(lane);
		const std::array<std::byte, 8> bytes = stamped(id, simLane, sent[id][static_cast<size_t>(simLane)]++);
		if (correct.contains(id))
			++result.correctOffered[static_cast<size_t>(simLane)];
		endpoint.deliver(id, lane, bytes, now);
	};
	auto correctPeers = [&] {
		std::vector<SimConnectionId> out;
		for (const SimConnectionId id : live)
		{
			if (correct.contains(id))
				out.push_back(id);
		}
		return out;
	};
	auto correctFrame = [&](double now) {
		for (const SimConnectionId id : correctPeers())
		{
			deliver(id, ognet::Lane::SimInput, now);
			deliver(id, ognet::Lane::SimInput, now);
			deliver(id, ognet::Lane::SimClock, now);
			deliver(id, ognet::Lane::Control, now);
		}
	};
	auto drainAll = [&] {
		transport->inbox().drain([&](const InboundSimRecord& record) {
			if (record.kind == InboundKind::Disconnected)
			{
				++(record.cause == InboundCause::ControlQuota ? result.quotaCauseDisconnects : result.unstatedDisconnects);
				return;
			}
			if (record.kind != InboundKind::Message || !correct.contains(record.conn))
				return;
			const size_t lane = static_cast<size_t>(record.lane);
			const std::uint32_t sequence = sequenceOf(record);
			++result.correctArrived[lane];
			result.correctOutOfSequence += sequence == expected[record.conn][lane] ? 0u : 1u;
			expected[record.conn][lane] = sequence + 1u;
		});
	};
	SimConnectionId nextFlooder = 1000u;
	auto floodGap = [&](size_t perGeneration, int generations, double now) {
		for (int generation = 0; generation < generations; ++generation)
		{
			std::vector<SimConnectionId> wave;
			for (size_t i = 0u; i < perGeneration; ++i)
			{
				if (connect(nextFlooder))
					wave.push_back(nextFlooder);
				++nextFlooder;
			}
			std::vector<SimConnectionId> underQuota;
			for (const SimConnectionId id : wave)
			{
				const std::uint16_t slot = transport->slotOf(id);
				const std::uint32_t residue = transport->inbox().controlUndrained(slot);
				result.unexpectedResidue += residue == 0u || residue == kChurnQuota ? 0u : 1u;
				ognet::lanes::LaneScheduler admitted(ognet::Role::Server, ognet::lanes::kDefaultLaneTable, ognet::lanes::kDefaultBudget, now);
				std::array<bool, kUpLanes.size()> capped{};
				const size_t requestsBefore = endpoint.disconnects.size();
				for (int k = 0; k < kMaxBurstPerLane; ++k)
				{
					for (size_t l = 0u; l < kUpLanes.size(); ++l)
					{
						if (capped[l] || admitted.admitReceived(kUpLanes[l], now) != ognet::lanes::ReceiveVerdict::Accept)
						{
							capped[l] = true;
							continue;
						}
						result.deliveredWhileClosing += endpoint.disconnects.size() > requestsBefore ? 1u : 0u;
						deliver(id, kUpLanes[l], now);
					}
				}
				if (endpoint.disconnects.size() > requestsBefore)
				{
					result.slotsClosedOverQuota.insert(slot);
					++result.offenders;
				}
				else
				{
					underQuota.push_back(id);
				}
				for (const SimConnectionId peer : correctPeers())
				{
					deliver(peer, ognet::Lane::Control, now);
					deliver(peer, ognet::Lane::SimInput, now);
				}
			}
			result.flooders += wave.size();
			result.leftUnderQuota += underQuota.size();
			applyQueuedDisconnects();
			for (const SimConnectionId id : underQuota)
				close(id);
		}
	};

	SimConnectionId nextCorrect = 1u;
	for (size_t i = 0u; i < 4u; ++i)
	{
		correct.insert(nextCorrect);
		connect(nextCorrect++);
	}
	for (int frame = 1; frame <= 20; ++frame)
	{
		const double now = frame / 60.0;
		if (frame == 5)
		{
			floodGap(kConnections - 4u, 3, now);
		}
		if (frame == 10)
		{
			for (const SimConnectionId id : correctPeers())
				close(id);
			floodGap(kConnections, 2, now);
			result.highWaterAfterAllSixteen = transport->inbox().stats().controlHighWater;
		}
		if (frame == 11)
		{
			for (size_t i = 0u; i < 4u; ++i)
			{
				correct.insert(nextCorrect);
				connect(nextCorrect++);
			}
		}
		correctFrame(now);
		drainAll();
	}

	for (const ognet::ConnectionId id : endpoint.disconnects)
	{
		++result.disconnectRequests[id];
		result.correctDisconnectRequests += correct.contains(id) ? 1u : 0u;
	}
	result.stats = transport->inbox().stats();
	result.transportStats = transport->stats();
	return result;
}

TEST_CASE("SimNetTransport.AllSixteenSlotsFloodingControlPastTheirQuotaWithChurnInOneDrainGapCannotOverflowTheControlRing",
	"[NetTransport][SimNetIsolation]")
{
	const ChurnResult result = runChurn();
	const size_t flooders = (kConnections - 4u) * 3u + kConnections * 2u;
	const size_t offenders = (kConnections - 4u) * 2u + kConnections;

	CHECK(result.maxLive == kConnections);
	CHECK(result.connectsRefusedByEndpoint == 0u);
	CHECK(result.flooders == flooders);
	CHECK(result.unexpectedResidue == 0u);
	CHECK(result.stats.controlHighWater <= result.controlCapacity);
	CHECK(result.highWaterAfterAllSixteen == 7296u);
	CHECK(result.stats.dropped[static_cast<size_t>(SimLane::Control)] == 0u);
	CHECK(result.slotsClosedOverQuota.size() == kConnections);

	CHECK(result.offenders == offenders);
	CHECK(result.leftUnderQuota == flooders - offenders);
	CHECK(result.disconnectRequests.size() == offenders);
	for (const auto& [id, requests] : result.disconnectRequests)
	{
		INFO("connection " << id);
		CHECK(requests == 1);
	}
	CHECK(result.correctDisconnectRequests == 0u);
	CHECK(result.transportStats.controlQuotaDisconnects == offenders);
	CHECK(result.stats.quotaRefused[static_cast<size_t>(SimLane::Control)] == offenders);
	CHECK(result.deliveredWhileClosing > 0u);
	CHECK(result.transportStats.closingRefused == result.deliveredWhileClosing);
	CHECK(result.transportStats.unslottedRefused == 0u);
	CHECK(result.transportStats.noSlotConnects == 0u);
	CHECK(result.quotaCauseDisconnects == offenders);
	CHECK(result.unstatedDisconnects == 4u + (flooders - offenders));

	for (size_t lane = 0u; lane < kSimLaneCount; ++lane)
	{
		INFO("lane " << lane);
		CHECK(result.correctArrived[lane] == result.correctOffered[lane]);
	}
	CHECK(result.correctOffered[static_cast<size_t>(SimLane::Control)] > 0u);
	CHECK(result.correctOutOfSequence == 0u);
}

TEST_CASE("SimNetServer.AConnectionClosedOverItsControlQuotaIsLoggedWithItsCauseAndCountedOnTheWindowLine", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 2u;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(config);
	start(*transport, endpoint);
	ServerHost host;
	TimeConfig timeConfig;
	ogsimnet::SimNetServer<IsoGame, ServerHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> server(host, *transport, timeConfig,
		size_t{4096});

	host.now = 0.0;
	server.afterInlineBatch(0);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 31u});
	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 457; ++i)
		endpoint.deliver(31u, ognet::Lane::Control, bytes, 1.0);
	REQUIRE(endpoint.disconnects == std::vector<ognet::ConnectionId>{31u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 31u});
	server.drain();
	host.now = 10.0;
	server.afterInlineBatch(0);

	std::vector<std::string> disconnects;
	std::vector<std::string> windows;
	for (const std::string& line : host.lines)
	{
		if (line.find("[SimNet.Disconnect]") != std::string::npos)
			disconnects.push_back(line);
		if (line.find("[SimNet.Window]") != std::string::npos)
			windows.push_back(line);
	}
	CHECK(disconnects
		== std::vector<std::string>{
			"[Warning][SimNet.Disconnect] role=server conn=31 cause=controlQuota (more undrained Control messages than its slot's quota)"});
	REQUIRE(windows.size() == 1u);
	CHECK(windows[0].ends_with(" quotaRefused=1 controlHighWater=456 quotaDisconnects=1 eventsRefused=0 eventHighWater=2"));
}

TEST_CASE("SimNetClient.AServerConnectionClosedOverItsControlQuotaIsLoggedWithItsCauseAndCountedOnTheWindowLine", "[NetTransport][SimNetIsolation]")
{
	RecordingEndpoint endpoint;
	ognet::EndpointConfig clientEndpoint;
	clientEndpoint.role = ognet::Role::Client;
	auto transport = std::make_unique<ogsimnet::SimNetTransport<RecordingEndpoint, 256>>(ogsimnet::simNetConfigFor(clientEndpoint));
	start(*transport, endpoint);
	ClientHost host;
	ogsimnet::SimNetClient<IsoGame, ClientHost, ogsimnet::SimNetTransport<RecordingEndpoint, 256>> client(host, *transport);

	host.now = 0.0;
	client.flushFrame();
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 41u});
	const std::array<std::byte, 8> bytes{};
	for (int i = 0; i < 200; ++i)
		endpoint.deliver(41u, ognet::Lane::SimRelay, bytes, 1.0);
	for (int i = 0; i < 460; ++i)
		endpoint.deliver(41u, ognet::Lane::Control, bytes, 1.0);
	REQUIRE(endpoint.disconnects == std::vector<ognet::ConnectionId>{41u});
	CHECK(transport->stats().closingRefused == 3u);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 41u});
	client.drain();
	host.now = 10.0;
	client.flushFrame();

	std::vector<std::string> disconnects;
	std::vector<std::string> windows;
	for (const std::string& line : host.lines)
	{
		if (line.find("[SimNet.Disconnect]") != std::string::npos)
			disconnects.push_back(line);
		if (line.find("[SimNet.Window]") != std::string::npos)
			windows.push_back(line);
	}
	CHECK(disconnects
		== std::vector<std::string>{
			"[Warning][SimNet.Disconnect] role=client conn=41 cause=controlQuota (more undrained Control messages than its slot's quota)"});
	REQUIRE(windows.size() == 1u);
	CHECK(windows[0].find(" drops=8 ") != std::string::npos);
	CHECK(windows[0].ends_with(" quotaRefused=1 controlHighWater=456 quotaDisconnects=1 eventsRefused=0 eventHighWater=2"));
}

TEST_CASE("SimInbox.Death.AnUntaggedControlMessageIntoAFullControlRingFiresTheCheck", "[.][OGSimDeath]")
{
	SimInbox<128> inbox(SimInboxConfig{2u, 0u, 0u});
	const std::array<std::byte, 2> bytes{};
	REQUIRE(inbox.pushMessage(1u, SimLane::Control, 0.0, bytes));
	REQUIRE(inbox.pushMessage(1u, SimLane::Control, 0.0, bytes));
	inbox.pushMessage(1u, SimLane::Control, 0.0, bytes);
	FAIL("a Control message into a full Control ring did NOT fire SimInbox's OG_CHECK");
}

TEST_CASE("SimInbox.Death.ControlQuotasThatDoNotFitTheControlRingFireTheCheck", "[.][OGSimDeath]")
{
	SimInbox<128> inbox(SimInboxConfig{4u, 2u, 0u, 3u});
	FAIL("a tagged inbox whose Control quotas (2 x 3) exceed its Control ring (4) did NOT fire SimInbox's OG_CHECK");
}

TEST_CASE("SimNetTransport.Death.AConnectedWithEveryConnectionSlotTakenFiresTheCheck", "[.][OGSimDeath]")
{
	RecordingEndpoint endpoint;
	ogsimnet::SimNetConfig config;
	config.maxConnections = 1u;
	auto transport = std::make_unique<SmallTransport>(config);
	start(*transport, endpoint);
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 1u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 2u});
	FAIL("a Connected with every connection slot taken did NOT fire SimNetTransport's OG_CHECK");
}
} // namespace simNetIsolationTest
