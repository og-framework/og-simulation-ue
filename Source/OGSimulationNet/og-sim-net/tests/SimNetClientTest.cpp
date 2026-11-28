// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/CorrectionStaleFilter.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationQueues.h"
#include "OGSimulation/SimulationReconciliation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ogsimnet;

namespace simNetClientTest
{
struct CliPosition
{
	std::int32_t x = 0;
};

struct CliInput
{
	std::int32_t move = 0;
	std::uint8_t flags = 0u;
};

struct RateInput
{
	std::int32_t a0 = 0;
	std::int32_t a1 = 0;
	std::int32_t a2 = 0;
	std::int32_t a3 = 0;
	std::int32_t a4 = 0;
	std::int32_t a5 = 0;
	std::int32_t a6 = 0;
	std::int32_t a7 = 0;
	std::int32_t a8 = 0;
	std::uint8_t b0 = 0u;
	std::uint8_t b1 = 0u;
	std::uint8_t b2 = 0u;
};
} // namespace simNetClientTest

template <>
struct SerializableFields<simNetClientTest::CliPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetClientTest::CliPosition, x)); }
};

template <>
struct SerializableFields<simNetClientTest::CliInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(simNetClientTest::CliInput, move), SIM_MEMBER(simNetClientTest::CliInput, flags));
	}
};

template <>
struct SerializableFields<simNetClientTest::RateInput>
{
	static constexpr auto get()
	{
		using simNetClientTest::RateInput;
		return std::make_tuple(SIM_MEMBER(RateInput, a0), SIM_MEMBER(RateInput, a1), SIM_MEMBER(RateInput, a2), SIM_MEMBER(RateInput, a3),
			SIM_MEMBER(RateInput, a4), SIM_MEMBER(RateInput, a5), SIM_MEMBER(RateInput, a6), SIM_MEMBER(RateInput, a7),
			SIM_MEMBER(RateInput, a8), SIM_MEMBER(RateInput, b0), SIM_MEMBER(RateInput, b1), SIM_MEMBER(RateInput, b2));
	}
};

namespace simNetClientTest
{
using CliState = SimulationComposite<CliPosition>;

class CliAllState
{
public:
	const CliState& getState() const { return m_state; }
	CliState& editState() { return m_state; }

private:
	CliState m_state;
};

struct CliSim
{
	using StateType = CliState;
	using InputType = CliInput;

	CliAllState m_allState;
	CliAllState m_vizState;

	const CliAllState& getAllState() const { return m_allState; }
	CliAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const CliAllState& getVizState() const { return m_vizState; }
};

struct RateSim
{
	using StateType = CliState;
	using InputType = RateInput;
};

struct CliGame
{
	using Simulatables = SimulatableList<CliSim>;
};

struct RateGame
{
	using Simulatables = SimulatableList<RateSim>;
};

std::vector<std::uint8_t> toU8(std::span<const std::byte> bytes)
{
	std::vector<std::uint8_t> out;
	for (const std::byte b : bytes)
		out.push_back(std::to_integer<std::uint8_t>(b));
	return out;
}

std::vector<std::byte> toBytes(std::span<const std::byte> bytes) { return std::vector<std::byte>(bytes.begin(), bytes.end()); }

struct Stamp
{
	SimNetStream stream = SimNetStream::Input;
	std::uint8_t simId = 0u;
	std::uint32_t tick = 0u;

	friend bool operator==(const Stamp&, const Stamp&) = default;
};

struct ClientHost
{
	double now = 0.0;
	double rttSeconds = 0.05;
	std::vector<std::string> events;
	std::vector<std::string> lines;
	std::vector<Stamp> stamps;
	std::vector<double> stampSeconds;
	std::vector<std::pair<std::uint32_t, double>> timing;
	std::function<void(std::uint8_t)> added;
	std::function<void(std::uint8_t)> removed;

	double nowSeconds() { return now; }
	double serverRttSeconds() { return rttSeconds; }
	void onTimingInfo(std::uint32_t tick, double rtt)
	{
		timing.emplace_back(tick, rtt);
		events.push_back("clock " + std::to_string(tick));
	}
	void onRelayFloor(std::uint8_t floorTicks, bool replayed)
	{
		events.push_back("floor " + std::to_string(floorTicks) + (replayed ? " replayed" : " live"));
	}
	void onConnectionTier(std::uint8_t oldTier, std::uint8_t newTier, bool replayed)
	{
		events.push_back("tier " + std::to_string(oldTier) + "->" + std::to_string(newTier) + (replayed ? " replayed" : " live"));
	}
	void onRosterAdded(std::uint8_t simId)
	{
		events.push_back("added " + std::to_string(simId));
		if (added)
			added(simId);
	}
	void onRosterRemoved(std::uint8_t simId)
	{
		events.push_back("removed " + std::to_string(simId));
		if (removed)
			removed(simId);
	}
	void onWireMismatch(std::uint8_t received, std::uint8_t expected)
	{
		events.push_back("mismatch " + std::to_string(received) + "/" + std::to_string(expected));
	}
	void log(const char* line) { lines.emplace_back(line); }
	void stampClientReceived(SimNetStream stream, std::uint8_t simId, std::uint32_t tick, double recvSeconds)
	{
		stamps.push_back(Stamp{stream, simId, tick});
		stampSeconds.push_back(recvSeconds);
	}

	std::vector<std::string> linesWith(const char* tag) const
	{
		std::vector<std::string> out;
		for (const std::string& line : lines)
		{
			if (line.find(tag) != std::string::npos)
				out.push_back(line);
		}
		return out;
	}
};

static_assert(SimNetClientHost<ClientHost>);
static_assert(!SimNetDeliveryHook<ClientHost>);

struct DeliveryClientHost : ClientHost
{
	std::vector<std::pair<SimLane, double>> deliveries;

	void noteDelivered(SimLane lane, double recvSeconds) { deliveries.emplace_back(lane, recvSeconds); }
};

static_assert(SimNetClientHost<DeliveryClientHost> && SimNetDeliveryHook<DeliveryClientHost>);

struct ServerHost
{
	double now = 0.0;
	std::int32_t tick = 0;
	std::vector<std::string> lines;

	double nowSeconds() { return now; }
	double rttMs(SimConnectionId) { return 20.0; }
	std::int32_t receptionTick() { return tick; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
};

static_assert(SimNetServerHost<ServerHost>);

struct RecordingEndpoint
{
	struct Call
	{
		bool broadcast = false;
		ognet::ConnectionId id = ognet::kInvalidConnection;
		ognet::Lane lane = ognet::Lane::Control;
		std::vector<std::uint8_t> bytes;
	};

	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { calls.push_back(Call{false, id, lane, toU8(bytes)}); }
	void broadcastExcept(ognet::ConnectionId skip, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		calls.push_back(Call{true, skip, lane, toU8(bytes)});
	}
	void setLaneHandler(ognet::Lane, ognet::LaneHandler) {}
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId) {}

	std::vector<Call> on(ognet::Lane lane) const
	{
		std::vector<Call> out;
		for (const Call& call : calls)
		{
			if (call.lane == lane)
				out.push_back(call);
		}
		return out;
	}

	std::vector<Call> calls;
};

using ScriptTransport = SimNetTransport<RecordingEndpoint, 256>;
using ScriptClient = SimNetClient<CliGame, ClientHost, ScriptTransport>;
using CliShape = ScriptClient::Shape;
using ScriptChannel = ScriptClient::Channel;

static_assert(std::is_same_v<ScriptClient::SimT, CliSim>);
static_assert(ClientSimTransport<ScriptClient::ChannelTransport>);
static_assert(std::is_same_v<ScriptClient::ChannelTransport, ScriptTransport::ChannelTransport>);
static_assert(std::is_same_v<ScriptChannel, SessionChannel<CliSim, ScriptTransport::ChannelTransport>>);
static_assert(PredictionSyncedBufferOwnerConcept<ScriptChannel, CliState, CliInput>,
	"the client session's channel is NetSync's prediction owner of the game's simulatable");
static_assert(simEnvelope::ShapeFor<RateInput, CliState>::kInputBytes == 39u, "RateInput has the brawler input's 39 B");
} // namespace simNetClientTest

template <>
struct SimulatableOwnerTraits<simNetClientTest::CliSim>
{
	using PredictionOwnerType = simNetClientTest::ScriptChannel;
	using AuthorityOwnerType = simNetClientTest::ScriptChannel;
};

namespace simNetClientTest
{
constexpr SimConnectionId kServerConn = 11u;
constexpr std::uint8_t kSimA = 3u;
constexpr std::uint8_t kSimB = 4u;
constexpr std::uint8_t kSimC = 5u;
constexpr double kRecvSeconds = 2.5;
constexpr char kTodaysReceiveCorrectionStateFormat[] = "[ReceiveCorrectionState] id=%u tick=%u";

using MessageBytes = std::vector<std::byte>;

template <class WriteFn>
MessageBytes envelope(WriteFn&& write)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const size_t size = write(std::span<std::byte>(out));
	REQUIRE(size != 0u);
	return MessageBytes(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(size));
}

RosterEntry remoteEntry(std::uint8_t simId, std::uint8_t slot) { return RosterEntry{simId, slot, kRosterNotLocal}; }

MessageBytes snapshotMsg(std::uint16_t revision, const std::vector<RosterEntry>& entries)
{
	const RosterMessage message = rosterCodec::encodeSnapshot(revision, std::span<const RosterEntry>(entries));
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

MessageBytes addMsg(std::uint16_t revision, const RosterEntry& entry)
{
	const RosterMessage message = rosterCodec::encodeAdd(revision, entry);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

MessageBytes removeMsg(std::uint16_t revision, std::uint8_t simId)
{
	const RosterMessage message = rosterCodec::encodeRemove(revision, simId, RosterRemoveReason::Left);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

CliInput inputFor(std::uint32_t tick) { return CliInput{static_cast<std::int32_t>(tick * 3u), static_cast<std::uint8_t>(tick & 0x3Fu)}; }

RelayRingBuffer ringOf(std::uint32_t firstTick, std::uint32_t lastTick)
{
	RelayRingBuffer ring;
	for (std::uint32_t tick = firstTick; tick <= lastTick; ++tick)
		ring.writeLatest<CliInput>(tick, 2u, inputFor(tick), 8);
	return ring;
}

MessageBytes ringMsg(std::uint8_t simId, std::uint32_t firstTick, std::uint32_t lastTick)
{
	const RelayRingBuffer ring = ringOf(firstTick, lastTick);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRelayRing(out, simId, ring.payload()); });
}

CliState stateWithX(std::int32_t x)
{
	CliState state;
	state.edit<CliPosition>().x = x;
	return state;
}

CorrectionStateBuffer correctionOf(std::uint32_t tick)
{
	CorrectionStateBuffer buffer;
	buffer.write(stateWithX(static_cast<std::int32_t>(tick) * 10), tick, tick - 1u);
	return buffer;
}

MessageBytes correctionMsg(std::uint8_t simId, std::uint32_t tick, std::uint8_t version = correctionStateBuffer::kWireFormatVersion)
{
	const CorrectionStateBuffer buffer = correctionOf(tick);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeCorrectionState(out, simId, version, buffer.payload()); });
}

MessageBytes clockMsg(std::uint32_t tick)
{
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeClockPublish(out, tick); });
}

MessageBytes floorMsg(std::uint8_t value)
{
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRelayFloor(out, value); });
}

MessageBytes tierMsg(std::uint8_t value)
{
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeConnectionTier(out, value); });
}

std::string receiveLine(std::uint8_t simId, std::uint32_t tick)
{
	std::array<char, 96> line{};
	std::snprintf(line.data(), line.size(), kTodaysReceiveCorrectionStateFormat, static_cast<unsigned>(simId), static_cast<unsigned>(tick));
	return line.data();
}

struct Delivery
{
	char kind = '?';
	std::uint8_t simId = 0u;
	std::uint32_t tick = 0u;

	friend bool operator==(const Delivery&, const Delivery&) = default;
};

} // namespace simNetClientTest

template <>
struct Catch::StringMaker<simNetClientTest::Delivery>
{
	static std::string convert(const simNetClientTest::Delivery& d)
	{
		return std::string(1, d.kind) + std::to_string(d.simId) + "@" + std::to_string(d.tick);
	}
};

namespace simNetClientTest
{
std::string describe(const std::vector<Delivery>& deliveries)
{
	std::string text;
	for (const Delivery& d : deliveries)
		text += std::string(" ") + d.kind + std::to_string(d.simId) + "@" + std::to_string(d.tick);
	return text;
}

template <class ChannelT>
void recordDeliveries(ChannelT& channel, std::vector<Delivery>& out)
{
	const std::uint8_t simId = channel.simId();
	channel.setOnCorrectionStateReceivedCallback([&out, simId](const CorrectionStateBuffer& buffer) {
		out.push_back(Delivery{'c', simId, buffer.template readFromBuffer<std::uint32_t>(correctionStateBuffer::kTickOffset)});
	});
	channel.setOnRelayedInputReceivedCallback([&out, simId](const RelayRingBuffer& ring) {
		std::uint32_t newest = 0u;
		ring.template forEachEntry<CliInput>([&newest](std::uint32_t captureTick, std::uint8_t, const CliInput&) {
			if (captureTick != kNoInputCaptureTick && captureTick > newest)
				newest = captureTick;
		});
		out.push_back(Delivery{'r', simId, newest});
	});
}

struct Script
{
	explicit Script(SimNetConfig config = SimNetConfig{}) : transport(std::make_unique<ScriptTransport>(config))
	{
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		client = std::make_unique<ScriptClient>(host, *transport);
	}

	void connect(SimConnectionId conn = kServerConn) { transport->inbox().pushEvent(InboundKind::Connected, conn, kNoReceiveSeconds); }
	void disconnect(SimConnectionId conn = kServerConn) { transport->inbox().pushEvent(InboundKind::Disconnected, conn, kNoReceiveSeconds); }

	void push(SimLane lane, const MessageBytes& bytes, double recvSeconds = kRecvSeconds)
	{
		REQUIRE(transport->inbox().pushMessage(kServerConn, lane, recvSeconds, bytes));
	}

	void roster(const MessageBytes& bytes) { push(SimLane::Control, bytes); }
	void ring(std::uint8_t simId, std::uint32_t first, std::uint32_t last) { push(SimLane::SimRelay, ringMsg(simId, first, last)); }
	void correction(std::uint8_t simId, std::uint32_t tick) { push(SimLane::SimState, correctionMsg(simId, tick)); }
	void clock(std::uint32_t tick) { push(SimLane::SimClock, clockMsg(tick)); }

	void drain() { client->drain(); }

	void recordAll(std::vector<Delivery>& out)
	{
		host.added = [this, &out](std::uint8_t simId) { recordDeliveries(*client->channel(simId), out); };
	}

	ClientHost host;
	RecordingEndpoint endpoint;
	std::unique_ptr<ScriptTransport> transport;
	std::unique_ptr<ScriptClient> client;
};

void fillQueue(PendingInputQueue<CliInput>& queue, std::uint32_t first, std::uint32_t last)
{
	for (std::uint32_t tick = first; tick <= last; ++tick)
		queue.enqueue(tick, inputFor(tick));
}

template <class SimT>
struct NetSyncPeer
{
	SimulationObjectStorage<SimT> storage;
	SimulationReconciliation<SimT> reconciliation{storage};
	SimulationInputResolution<SimT> resolution{storage, reconciliation};
	SimulationNetSync<SimT> netSync{storage, reconciliation, resolution};
	std::vector<std::string> trace;

	NetSyncPeer()
	{
		auto logger = [this](const char* line) { trace.emplace_back(line); };
		netSync.setLogger(logger);
		reconciliation.setLogger(logger);
		resolution.setLogger(logger);
		resolution.template setNeutralInput<SimT>(typename SimT::InputType{});
	}
};
} // namespace simNetClientTest

using namespace simNetClientTest;

TEST_CASE("SimNetClient.TheSessionSatisfiesTheSeamsAndKeepsTodaysCorrectionLine", "[NetTransport][SimNetClient]")
{
	STATIC_CHECK(SimNetClientTransport<ScriptTransport>);
	STATIC_CHECK(PredictionSyncedBufferOwnerConcept<ScriptChannel, CliState, CliInput>);
	CHECK(std::string(kReceiveCorrectionStateFormat) == std::string(kTodaysReceiveCorrectionStateFormat));

	Script s;
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
	s.correction(kSimA, 41u);
	s.drain();
	CHECK(s.host.linesWith("[ReceiveCorrectionState]") == std::vector<std::string>{receiveLine(kSimA, 41u)});
	CHECK(s.host.linesWith("[ReceiveCorrectionState]")[0] == "[ReceiveCorrectionState] id=3 tick=41");
	CHECK(s.host.stamps == std::vector<Stamp>{Stamp{SimNetStream::State, kSimA, 41u}});
	CHECK(s.host.stampSeconds == std::vector<double>{kRecvSeconds});
	CHECK(s.transport->serverConnection() == kServerConn);
	CHECK(s.client->serverConnection() == kServerConn);
}

TEST_CASE("SimNetClient.TheRosterCreatesChannelsBeforeTheHostEventAndDestroysThemAfterIt", "[NetTransport][SimNetClient]")
{
	Script s;
	std::vector<std::string> seen;
	s.host.added = [&](std::uint8_t simId) { seen.push_back("added " + std::to_string(simId) + (s.client->channel(simId) ? " with" : " without")); };
	s.host.removed = [&](std::uint8_t simId) { seen.push_back("removed " + std::to_string(simId) + (s.client->channel(simId) ? " with" : " without")); };
	s.connect();
	s.roster(snapshotMsg(4u, {remoteEntry(kSimA, 0u), remoteEntry(kSimB, 1u)}));
	s.correction(kSimA, 10u);
	s.drain();
	CHECK(seen == std::vector<std::string>{"added 3 with", "added 4 with"});
	REQUIRE(s.client->channel(kSimA) != nullptr);
	CHECK(s.client->channel(kSimA)->simId() == kSimA);
	CHECK(s.client->staleFilter().newest(kSimA) == std::optional<std::uint32_t>(10u));

	s.roster(removeMsg(5u, kSimA));
	s.drain();
	CHECK(seen == std::vector<std::string>{"added 3 with", "added 4 with", "removed 3 with"});
	CHECK(s.client->channel(kSimA) == nullptr);
	CHECK(s.client->channel(kSimB) != nullptr);
	CHECK(!s.client->staleFilter().newest(kSimA).has_value());

	s.correction(kSimA, 11u);
	s.ring(kSimA, 9u, 11u);
	s.drain();
	CHECK(s.client->stats().unknownSim == 2u);
	CHECK(s.client->heldRingCount() == 0u);
	CHECK(s.client->stats().rosterApplied == 2u);
	CHECK(s.client->roster().entries().size() == 1u);
}

TEST_CASE("SimNetClient.ASecondSnapshotReplacesTheChannelsByTheirDifference", "[NetTransport][SimNetClient]")
{
	Script s;
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u), remoteEntry(kSimB, 1u)}));
	s.ring(kSimA, 10u, 12u);
	s.drain();
	const ScriptChannel* const kept = s.client->channel(kSimB);
	REQUIRE(kept != nullptr);
	s.roster(snapshotMsg(9u, {remoteEntry(kSimB, 1u)}));
	s.drain();
	CHECK(s.client->channel(kSimA) == nullptr);
	CHECK(s.client->channel(kSimB) == kept);
	s.roster(snapshotMsg(12u, {remoteEntry(kSimA, 0u), remoteEntry(kSimB, 1u)}));
	s.drain();
	REQUIRE(s.client->channel(kSimA) != nullptr);
	CHECK(s.client->channel(kSimA)->getRelayedInputRing().payload().empty());
	CHECK(s.client->channel(kSimB) == kept);
	CHECK(s.host.events == std::vector<std::string>{"added 3", "added 4", "removed 3", "added 3"});
	CHECK(s.client->roster().revision() == 12u);
}

TEST_CASE("SimNetClient.CorrectionsRingsAndClockTicksReachTheBoundCallbacksInArrivalOrderScripted", "[NetTransport][SimNetClient]")
{
	Script s;
	std::vector<Delivery> deliveries;
	s.recordAll(deliveries);
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u), remoteEntry(kSimB, 1u)}));
	s.ring(kSimA, 98u, 100u);
	s.correction(kSimB, 50u);
	s.clock(60u);
	s.correction(kSimA, 51u);
	s.ring(kSimB, 99u, 101u);
	s.clock(61u);
	s.drain();
	for (std::uint32_t frame = 0u; frame < 3u; ++frame)
	{
		s.correction(kSimA, 52u + frame);
		s.ring(kSimA, 99u + frame, 101u + frame);
		s.clock(62u + frame);
		s.drain();
	}

	std::vector<Delivery> merged = deliveries;
	std::vector<Delivery> expected{{'r', kSimA, 100u}, {'c', kSimB, 50u}, {'c', kSimA, 51u}, {'r', kSimB, 101u}, {'c', kSimA, 52u},
		{'r', kSimA, 101u}, {'c', kSimA, 53u}, {'r', kSimA, 102u}, {'c', kSimA, 54u}, {'r', kSimA, 103u}};
	INFO("delivered:" << describe(merged));
	CHECK(merged == expected);
	std::vector<std::uint32_t> clocks;
	for (const auto& [tick, rtt] : s.host.timing)
	{
		clocks.push_back(tick);
		CHECK(rtt == s.host.rttSeconds);
	}
	CHECK(clocks == std::vector<std::uint32_t>{61u, 62u, 63u, 64u});
	CHECK(s.client->stats().clocksDelivered == 4u);
	CHECK(s.client->stats().clocksCoalesced == 1u);
	CHECK(s.host.linesWith("[ReceiveCorrectionState]")
		== std::vector<std::string>{receiveLine(kSimB, 50u), receiveLine(kSimA, 51u), receiveLine(kSimA, 52u), receiveLine(kSimA, 53u),
			receiveLine(kSimA, 54u)});
	CHECK(s.host.stamps.size() == 3u + 3u + 5u + 3u * 3u);
	CHECK(s.host.stamps[0] == Stamp{SimNetStream::Input, kSimA, 98u});
	CHECK(s.host.stamps[3] == Stamp{SimNetStream::State, kSimB, 50u});
	CHECK(s.client->stats().ringsStored == 5u);
	CHECK(s.client->stats().correctionsStored == 5u);
}

TEST_CASE("SimNetClient.NoCorrectionOlderThanOneAlreadyInjectedIsDeliveredUnderAScriptedReorder", "[NetTransport][SimNetClient]")
{
	Script s;
	std::vector<Delivery> deliveries;
	s.recordAll(deliveries);
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u), remoteEntry(kSimB, 1u)}));
	for (const std::uint32_t tick : {10u, 12u, 11u, 12u, 13u, 9u, 14u})
		s.correction(kSimA, tick);
	s.correction(kSimB, 11u);
	for (const std::uint32_t tick : {5u, 7u, 6u, 7u, 8u})
		s.clock(tick);
	s.drain();

	std::vector<std::uint32_t> injectedA;
	for (const Delivery& d : deliveries)
	{
		if (d.kind == 'c' && d.simId == kSimA)
			injectedA.push_back(d.tick);
	}
	CHECK(injectedA == std::vector<std::uint32_t>{10u, 12u, 13u, 14u});
	CHECK(deliveries.back() == Delivery{'c', kSimB, 11u});
	CHECK(s.client->staleFilter().staleDropped() == 2u);
	CHECK(s.client->staleFilter().duplicateDropped() == 1u);
	CHECK(s.host.linesWith("[ReceiveCorrectionState]").size() == 5u);
	std::vector<std::uint32_t> clocks;
	for (const auto& sample : s.host.timing)
		clocks.push_back(sample.first);
	CHECK(clocks == std::vector<std::uint32_t>{8u});
	CHECK(s.client->stats().clocksCoalesced == 2u);
	CHECK(s.client->clockFilter().staleDropped() == 1u);
	CHECK(s.client->clockFilter().duplicateDropped() == 1u);
}

TEST_CASE("SimNetClient.QueuedClockTicksReachTheHostAsTheNewestOnePerDrain", "[NetTransport][SimNetClient]")
{
	Script s;
	s.connect();
	for (std::uint32_t tick = 1487u; tick <= 1502u; ++tick)
		s.clock(tick);
	s.drain();

	REQUIRE(s.host.timing.size() == 1u);
	CHECK(s.host.timing[0].first == 1502u);
	CHECK(s.host.timing[0].second == s.host.rttSeconds);
	CHECK(s.client->stats().clocksDelivered == 1u);
	CHECK(s.client->stats().clocksCoalesced == 15u);

	TimeConfig config;
	NetworkTimeEstimator estimator(config, nullptr);
	const auto feed = [&](std::size_t from) {
		for (std::size_t index = from; index < s.host.timing.size(); ++index)
		{
			estimator.updateRTT(s.host.timing[index].second);
			estimator.recordAuthorityTick(s.host.timing[index].first);
		}
	};
	feed(0u);
	CHECK(estimator.getMeasuredRTTSampleCount() == 1u);
	CHECK(estimator.getLastAuthorityTick() == 1502u);

	s.drain();
	CHECK(s.host.timing.size() == 1u);

	s.clock(1503u);
	s.clock(1501u);
	s.drain();
	REQUIRE(s.host.timing.size() == 2u);
	CHECK(s.host.timing[1].first == 1503u);
	feed(1u);
	CHECK(estimator.getMeasuredRTTSampleCount() == 2u);
	CHECK(estimator.getLastAuthorityTick() == 1503u);
	CHECK(s.client->clockFilter().staleDropped() == 1u);
}

TEST_CASE("SimNetClient.ARingStoredBeforeRegistrationIsReadAtTheBindTimeRead", "[NetTransport][SimNetClient]")
{
	SECTION("the ring arrives after the roster Add and before registration")
	{
		Script s;
		s.connect();
		s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
		s.ring(kSimA, 98u, 100u);
		s.drain();
		ScriptChannel* channel = s.client->channel(kSimA);
		REQUIRE(channel != nullptr);
		CHECK(toU8(channel->getRelayedInputRing().payload()) == toU8(ringOf(98u, 100u).payload()));

		NetSyncPeer<CliSim> peer;
		registerSimulatable<CliSim>(peer.storage, peer.reconciliation, peer.resolution, peer.netSync, kSimA, CliSim{}, *channel);
		const auto* cache = peer.resolution.template findRemoteInputCache<CliSim>(kSimA);
		REQUIRE(cache != nullptr);
		CHECK(cache->findLatest().valid);
		CHECK(cache->findLatest().captureTick == 100u);
		CHECK(s.client->stats().ringsStored == 1u);
		CHECK(s.client->stats().ringsHeld == 0u);
	}
	SECTION("the ring arrives before the Snapshot, is held, and is in the channel when the host registers")
	{
		Script s;
		NetSyncPeer<CliSim> peer;
		std::vector<std::uint8_t> ringAtBind;
		std::optional<std::uint32_t> latestAtBind;
		s.host.added = [&](std::uint8_t simId) {
			ScriptChannel& channel = *s.client->channel(simId);
			ringAtBind = toU8(channel.getRelayedInputRing().payload());
			registerSimulatable<CliSim>(peer.storage, peer.reconciliation, peer.resolution, peer.netSync, simId, CliSim{}, channel);
			const auto* cache = peer.resolution.template findRemoteInputCache<CliSim>(simId);
			if (cache != nullptr && cache->findLatest().valid)
				latestAtBind = cache->findLatest().captureTick;
		};
		s.connect();
		s.ring(kSimA, 95u, 97u);
		s.ring(kSimA, 98u, 100u);
		s.drain();
		CHECK(s.client->heldRingCount() == 1u);
		CHECK(s.host.stamps.empty());
		s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
		s.drain();
		CHECK(ringAtBind == toU8(ringOf(98u, 100u).payload()));
		CHECK(latestAtBind == std::optional<std::uint32_t>(100u));
		CHECK(s.client->heldRingCount() == 0u);
		CHECK(s.client->stats().ringsHeld == 2u);
		CHECK(s.client->stats().heldRingsApplied == 1u);
		CHECK(s.client->stats().unknownSim == 0u);
		CHECK(s.host.stamps.empty());
	}
}

TEST_CASE("SimNetClient.RelaysAndTheClockBeforeTheSnapshotFollowTheStatedRuleInBothOrders", "[NetTransport][SimNetClient]")
{
	SECTION("relay, clock and correction before the Snapshot")
	{
		Script s;
		std::vector<Delivery> deliveries;
		s.recordAll(deliveries);
		s.connect();
		s.ring(kSimB, 40u, 42u);
		s.clock(70u);
		s.correction(kSimB, 30u);
		s.drain();
		CHECK(s.host.events == std::vector<std::string>{"clock 70"});
		CHECK(s.client->heldRingCount() == 1u);
		CHECK(s.client->stats().unknownSim == 1u);
		s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
		s.roster(addMsg(2u, remoteEntry(kSimB, 1u)));
		s.drain();
		CHECK(s.host.events == std::vector<std::string>{"clock 70", "added 3", "added 4"});
		CHECK(deliveries.empty());
		REQUIRE(s.client->channel(kSimB) != nullptr);
		CHECK(toU8(s.client->channel(kSimB)->getRelayedInputRing().payload()) == toU8(ringOf(40u, 42u).payload()));
		CHECK(s.client->staleFilter().newest(kSimB) == std::nullopt);
	}
	SECTION("relay, clock and correction after the Snapshot")
	{
		Script s;
		std::vector<Delivery> deliveries;
		s.recordAll(deliveries);
		s.connect();
		s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
		s.roster(addMsg(2u, remoteEntry(kSimB, 1u)));
		s.ring(kSimB, 40u, 42u);
		s.clock(70u);
		s.correction(kSimB, 30u);
		s.drain();
		CHECK(s.host.events == std::vector<std::string>{"added 3", "added 4", "clock 70"});
		CHECK(deliveries == std::vector<Delivery>{{'r', kSimB, 42u}, {'c', kSimB, 30u}});
		CHECK(s.client->heldRingCount() == 0u);
		CHECK(s.client->stats().unknownSim == 0u);
	}
}

TEST_CASE("SimNetClient.AVersionMismatchFiresOneHostCallAndThenSilence", "[NetTransport][SimNetClient]")
{
	Script s;
	std::vector<Delivery> deliveries;
	s.recordAll(deliveries);
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
	s.correction(kSimA, 20u);
	s.push(SimLane::SimState, correctionMsg(kSimA, 21u, correctionStateBuffer::kWireFormatVersion - 1u));
	s.correction(kSimA, 22u);
	s.push(SimLane::SimState, correctionMsg(kSimA, 23u, correctionStateBuffer::kWireFormatVersion + 1u));
	s.correction(kSimA, 24u);
	s.drain();

	CHECK(s.host.events == std::vector<std::string>{"added 3", "mismatch 14/15"});
	CHECK(deliveries == std::vector<Delivery>{{'c', kSimA, 20u}});
	CHECK(s.host.linesWith("[ReceiveCorrectionState]") == std::vector<std::string>{receiveLine(kSimA, 20u)});
	CHECK(s.host.stamps.size() == 1u);
	CHECK(s.client->wireMismatchLatched());
	CHECK(s.client->stats().mismatchIgnored == 3u);
	CHECK(s.client->staleFilter().newest(kSimA) == std::optional<std::uint32_t>(20u));
}

TEST_CASE("SimNetClient.AFloorOrTierThatArrivesBeforeBindListenerIsReplayedOnce", "[NetTransport][SimNetClient]")
{
	Script s;
	s.connect();
	s.push(SimLane::Control, floorMsg(6u));
	s.push(SimLane::Control, tierMsg(2u));
	s.drain();
	CHECK(s.host.events.empty());
	CHECK(s.client->latchedFloor() == std::optional<std::uint8_t>(std::uint8_t{6}));

	s.client->bindListener();
	s.drain();
	CHECK(s.host.events == std::vector<std::string>{"floor 6 replayed", "tier 0->2 replayed"});
	s.client->bindListener();
	s.drain();
	CHECK(s.host.events.size() == 2u);

	s.push(SimLane::Control, floorMsg(7u));
	s.push(SimLane::Control, tierMsg(3u));
	s.drain();
	CHECK(s.host.events == std::vector<std::string>{"floor 6 replayed", "tier 0->2 replayed", "floor 7 live", "tier 2->3 live"});

	s.client->unbindListener();
	s.push(SimLane::Control, floorMsg(8u));
	s.drain();
	CHECK(s.host.events.size() == 4u);
	s.client->bindListener();
	s.drain();
	CHECK(s.host.events.back() == "tier 2->3 replayed");
	CHECK(s.host.events[4] == "floor 8 replayed");
	CHECK(s.client->stats().floorsReceived == 3u);
	CHECK(s.client->stats().tiersReceived == 2u);
}

TEST_CASE("SimNetClient.ARevisionGapDisconnectsWithAReasonAndTheSessionFailsClosed", "[NetTransport][SimNetClient]")
{
	Script s;
	std::vector<Delivery> deliveries;
	s.recordAll(deliveries);
	s.connect();
	s.roster(snapshotMsg(5u, {remoteEntry(kSimA, 0u)}));
	s.roster(addMsg(7u, remoteEntry(kSimB, 1u)));
	s.ring(kSimA, 10u, 12u);
	s.correction(kSimA, 12u);
	s.drain();

	CHECK(s.host.linesWith("[Roster.Refused]") == std::vector<std::string>{"[Roster.Refused] reason=RevisionGap"});
	const std::optional<SimNetDisconnectRequest> request = s.client->takeDisconnectRequest();
	REQUIRE(request.has_value());
	CHECK(request->conn == kServerConn);
	CHECK(request->reason == RosterRefusal::RevisionGap);
	CHECK(!s.client->takeDisconnectRequest().has_value());
	CHECK(s.client->refused());
	CHECK(s.transport->serverConnection() == kInvalidSimConnection);
	CHECK(deliveries.empty());
	CHECK(s.client->stats().ignoredWhileRefused == 2u);
	CHECK(s.client->channel(kSimB) == nullptr);

	PendingInputQueue<CliInput> queue;
	fillQueue(queue, 10u, 12u);
	s.transport->beginInputSends();
	s.client->channel(kSimA)->sendLocalInputToAuthority(queue, 12u, 3u);
	s.transport->endInputSends();
	s.client->flushFrame();
	CHECK(s.endpoint.on(ognet::Lane::SimInput).empty());
	CHECK(s.transport->stats().inputSetsWithoutServer == 1u);

	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
	s.correction(kSimA, 13u);
	s.drain();
	CHECK(!s.client->refused());
	CHECK(deliveries == std::vector<Delivery>{{'c', kSimA, 13u}});
	CHECK(s.transport->serverConnection() == kServerConn);
}

TEST_CASE("SimNetClient.AMalformedRosterIsRefusedLikeARefusedOne", "[NetTransport][SimNetClient]")
{
	Script s;
	s.connect();
	MessageBytes truncated = snapshotMsg(1u, {remoteEntry(kSimA, 0u)});
	truncated.pop_back();
	s.roster(truncated);
	s.drain();
	CHECK(s.host.linesWith("[Roster.Refused]") == std::vector<std::string>{"[Roster.Refused] reason=Truncated"});
	CHECK(s.client->refused());
	CHECK(s.client->stats().badDecode == 0u);

	Script t;
	t.connect();
	t.push(SimLane::SimState, snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
	t.drain();
	CHECK(!t.client->refused());
	CHECK(t.client->stats().badDecode == 1u);
}

TEST_CASE("SimNetClient.ASetDropsEmptyBundlesAndCarriesEachNonEmptyOne", "[NetTransport][SimNetClient]")
{
	Script s;
	s.connect();
	s.roster(snapshotMsg(1u, {RosterEntry{kSimA, 0u, 0u}, RosterEntry{kSimB, 1u, 1u}, RosterEntry{kSimC, 2u, 2u}}));
	s.drain();
	PendingInputQueue<CliInput> queue;
	fillQueue(queue, 20u, 22u);

	s.transport->beginInputSends();
	s.client->channel(kSimA)->sendLocalInputToAuthority(queue, 22u, 3u);
	s.client->channel(kSimB)->sendLocalInputToAuthority(queue, 22u, 0u);
	s.client->channel(kSimC)->sendLocalInputToAuthority(queue, 22u, 2u);
	s.transport->endInputSends();
	const std::vector<RecordingEndpoint::Call> sets = s.endpoint.on(ognet::Lane::SimInput);
	REQUIRE(sets.size() == 1u);
	CHECK(sets[0].id == kServerConn);
	CHECK(s.client->stats().emptyBundles == 1u);
	CHECK(s.transport->stats().inputBundlesRefused == 0u);
	std::vector<std::byte> wire;
	for (const std::uint8_t b : sets[0].bytes)
		wire.push_back(std::byte{b});
	const DecodedSimMessage set = simEnvelope::decode<CliShape>(SimLane::SimInput, SimDirection::ClientToServer, wire);
	REQUIRE(set.refusal == DecodeRefusal::None);
	CHECK(set.count == 2u);
	std::vector<std::uint8_t> ids;
	CHECK(simEnvelope::forEachInputBundle<CliShape>(set.body, [&](std::uint8_t simId, std::span<const std::byte>) { ids.push_back(simId); })
		== DecodeRefusal::None);
	CHECK(ids == std::vector<std::uint8_t>{kSimA, kSimC});
}

TEST_CASE("SimNetClient.AHostWithNoteDeliveredSeesOneCallPerMessageRecordAndNoneForEvents", "[NetTransport][SimNetClient]")
{
	RecordingEndpoint endpoint;
	DeliveryClientHost host;
	auto transport = std::make_unique<ScriptTransport>(SimNetConfig{});
	transport->installHandlers(endpoint);
	transport->markEndpointStarted();
	SimNetClient<CliGame, DeliveryClientHost, ScriptTransport> client(host, *transport);
	auto push = [&](SimLane lane, const MessageBytes& bytes, double recvSeconds) {
		REQUIRE(transport->inbox().pushMessage(kServerConn, lane, recvSeconds, bytes));
	};

	transport->inbox().pushEvent(InboundKind::Connected, kServerConn, kNoReceiveSeconds);
	client.drain();
	CHECK(host.deliveries.empty());

	push(SimLane::Control, snapshotMsg(1u, {remoteEntry(kSimA, 0u)}), 3.0);
	push(SimLane::SimRelay, ringMsg(kSimA, 10u, 12u), 3.25);
	push(SimLane::SimState, correctionMsg(kSimA, 12u), 3.5);
	push(SimLane::SimClock, clockMsg(40u), 3.75);
	push(SimLane::SimState, clockMsg(41u), 4.0);
	push(SimLane::Control, addMsg(3u, remoteEntry(kSimB, 1u)), 4.25);
	push(SimLane::SimState, correctionMsg(kSimA, 13u), 4.5);
	transport->inbox().pushEvent(InboundKind::Disconnected, kServerConn, kNoReceiveSeconds);
	client.drain();

	CHECK(host.deliveries
		== std::vector<std::pair<SimLane, double>>{{SimLane::Control, 3.0}, {SimLane::SimRelay, 3.25}, {SimLane::SimState, 3.5},
			{SimLane::SimClock, 3.75}, {SimLane::SimState, 4.0}, {SimLane::Control, 4.25}, {SimLane::SimState, 4.5}});
	CHECK(client.stats().messages == 7u);
	CHECK(client.stats().badDecode == 1u);
	CHECK(client.stats().correctionsStored == 1u);
	CHECK(client.refused());
	CHECK(client.stats().ignoredWhileRefused == 1u);
	CHECK(client.stats().connects == 1u);
	CHECK(client.stats().disconnects == 1u);
}

TEST_CASE("SimNetClient.TheWindowLineReportsEveryTenSecondsOfHostTime", "[NetTransport][SimNetClient]")
{
	Script s;
	s.client->flushFrame();
	s.connect();
	s.roster(snapshotMsg(1u, {remoteEntry(kSimA, 0u)}));
	s.correction(kSimA, 10u);
	s.correction(kSimA, 10u);
	s.correction(kSimA, 9u);
	s.correction(kSimC, 9u);
	s.clock(4u);
	s.clock(3u);
	s.push(SimLane::SimState, clockMsg(5u));
	s.drain();
	s.host.now = 9.9;
	s.client->flushFrame();
	CHECK(s.host.linesWith("[SimNet.Window]").empty());
	s.host.now = 10.0;
	s.client->flushFrame();
	const std::vector<std::string> windows = s.host.linesWith("[SimNet.Window]");
	REQUIRE(windows.size() == 1u);
	CHECK(windows[0]
		== "[Warning][SimNet.Window] role=client drains=1 msgs=8 maxDepth=9 drops=0 tooLarge=0 badDecode=1 refusedNotOwner=0 "
		   "unknownSim=1 statesSent=0 relaysSent=0 overBudget=0 staleDropped=2 duplicateDropped=1 quotaRefused=0 controlHighWater=1 quotaDisconnects=0 "
		   "eventsRefused=0 eventHighWater=1");
	s.host.now = 20.0;
	s.client->flushFrame();
	const std::vector<std::string> second = s.host.linesWith("[SimNet.Window]");
	REQUIRE(second.size() == 2u);
	CHECK(second[1]
		== "[Warning][SimNet.Window] role=client drains=0 msgs=0 maxDepth=0 drops=0 tooLarge=0 badDecode=0 refusedNotOwner=0 "
		   "unknownSim=0 statesSent=0 relaysSent=0 overBudget=0 staleDropped=0 duplicateDropped=0 quotaRefused=0 controlHighWater=1 quotaDisconnects=0 "
		   "eventsRefused=0 eventHighWater=1");
}

namespace
{
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using EmulatedVirtual = ognet::EmulatedBackend<VirtualBackend>;

constexpr std::uint8_t kServerHostIndex = 1;
constexpr std::uint8_t kFirstClientHostIndex = 10;
constexpr std::uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr double kStepSeconds = 0.001;
constexpr int kMaxSteps = 5000;

struct Arrival
{
	InboundKind kind = InboundKind::Message;
	SimMessageKind message = SimMessageKind{};
	std::uint8_t simId = 0u;
	std::uint32_t tick = 0u;
	std::uint32_t newest = 0u;
};

template <class InnerT>
class TeeTransport
{
public:
	using ChannelTransport = typename InnerT::ChannelTransport;

	class Inbox
	{
	public:
		Inbox(InnerT& inner, std::vector<Arrival>& arrivals) : m_inner(inner), m_arrivals(arrivals) {}

		template <class Fn>
		size_t drain(Fn&& fn)
		{
			return m_inner.inbox().drain([&](const InboundSimRecord& record) {
				m_arrivals.push_back(arrivalOf(record));
				fn(record);
			});
		}

		SimInboxStats stats() const { return m_inner.inbox().stats(); }

	private:
		InnerT& m_inner;
		std::vector<Arrival>& m_arrivals;
	};

	explicit TeeTransport(InnerT& inner) : m_inner(inner) {}

	Inbox inbox() { return Inbox(m_inner, arrivals); }
	void endInputSends() { m_inner.endInputSends(); }
	ChannelTransport channelTransport(std::uint8_t simId) { return m_inner.channelTransport(simId); }
	void setServerConnection(SimConnectionId id) { m_inner.setServerConnection(id); }
	SimConnectionId serverConnection() const { return m_inner.serverConnection(); }

	std::vector<Arrival> arrivals;

private:
	static Arrival arrivalOf(const InboundSimRecord& record)
	{
		Arrival arrival;
		arrival.kind = record.kind;
		if (record.kind != InboundKind::Message)
			return arrival;
		const DecodedSimMessage message = simEnvelope::decode<CliShape>(record.lane, SimDirection::ServerToClient, record.payload());
		arrival.message = message.kind;
		arrival.simId = message.simId;
		arrival.tick = message.tick;
		if (message.refusal == DecodeRefusal::None && message.kind == SimMessageKind::RelayRing)
		{
			RelayRingBuffer ring;
			if (ring.assignFromWire(message.body))
			{
				ring.forEachEntry<CliInput>([&](std::uint32_t captureTick, std::uint8_t, const CliInput&) {
					if (captureTick != kNoInputCaptureTick && captureTick > arrival.newest)
						arrival.newest = captureTick;
				});
			}
		}
		return arrival;
	}

	InnerT& m_inner;
};

template <class GameT, class BackendT>
struct Rig
{
	using Endpoint = ognet::NetEndpoint<TestHost, BackendT>;
	using Transport = SimNetTransport<Endpoint, 2048>;
	using Tee = TeeTransport<Transport>;
	using Client = SimNetClient<GameT, ClientHost, Tee>;
	using Server = SimNetServer<GameT, ServerHost, Transport>;

	struct Peer
	{
		Peer(VirtualNetwork& network, BackendT& backendIn, std::uint64_t seed, SimNetConfig config = {})
			: transport(std::make_unique<Transport>(config)), host(network.clock(), seed), backend(backendIn), endpoint(host, backend)
		{
		}

		void start(ognet::Role role)
		{
			transport->installHandlers(endpoint);
			endpoint.setConnectionHandler(transport->connectionHandler());
			ognet::EndpointConfig endpointConfig;
			endpointConfig.role = role;
			if (role == ognet::Role::Server)
				endpointConfig.bind.port = kServerPort;
			endpointConfig.buildIdentity.fill(0x5A);
			REQUIRE(endpoint.start(endpointConfig));
			transport->markEndpointStarted();
		}

		std::unique_ptr<Transport> transport;
		TestHost host;
		BackendT& backend;
		Endpoint endpoint;
	};

	struct ClientPeer
	{
		std::unique_ptr<EmulatedVirtual> emulator;
		std::unique_ptr<Peer> peer;
		std::unique_ptr<Tee> tee;
		ClientHost simHost;
		std::unique_ptr<Client> session;
	};

	Rig()
	{
		network.setNow(kStartSeconds);
		serverPeer = std::make_unique<Peer>(network, serverBackend(), 100);
		serverPeer->start(ognet::Role::Server);
		server = std::make_unique<Server>(serverHost, *serverPeer->transport, config, size_t{1u << 20});
	}

	BackendT& serverBackend()
	{
		if constexpr (std::is_same_v<BackendT, VirtualBackend>)
		{
			return network.addHost(kServerHostIndex);
		}
		else
		{
			emulators.push_back(std::make_unique<EmulatedVirtual>(network.addHost(kServerHostIndex), network.clock()));
			return *emulators.back();
		}
	}

	ClientPeer& join(const ognet::EmulatorConfig& emulation = {})
	{
		const size_t index = clients.size();
		auto client = std::make_unique<ClientPeer>();
		VirtualBackend& raw = network.addHost(static_cast<std::uint8_t>(kFirstClientHostIndex + index));
		if constexpr (std::is_same_v<BackendT, VirtualBackend>)
		{
			client->peer = std::make_unique<Peer>(network, raw, 200 + index, SimNetConfig{});
		}
		else
		{
			client->emulator = std::make_unique<EmulatedVirtual>(raw, network.clock());
			REQUIRE(client->emulator->configure(emulation));
			client->peer = std::make_unique<Peer>(network, *client->emulator, 200 + index, SimNetConfig{});
		}
		client->peer->start(ognet::Role::Client);
		client->tee = std::make_unique<Tee>(*client->peer->transport);
		client->session = std::make_unique<Client>(client->simHost, *client->tee);
		client->peer->endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		ClientPeer& joined = *client;
		clients.push_back(std::move(client));
		REQUIRE(stepUntil([&] {
			return joined.session->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == index + 1u
				&& joined.session->roster().attached();
		}));
		serverIds.push_back(server->attachedConnections().back());
		return joined;
	}

	template <class DoneT>
	bool stepUntil(DoneT done)
	{
		for (int i = 0; i < kMaxSteps; ++i)
		{
			if (done())
				return true;
			pass(kStepSeconds);
		}
		return done();
	}

	void pass(double seconds)
	{
		network.advance(seconds);
		serverHost.now = network.nowSeconds();
		serverPeer->endpoint.serviceUntil(network.nowSeconds());
		server->drain();
		if (beforeServerFlush)
			beforeServerFlush();
		server->afterInlineBatch(serverHost.tick);
		serverPeer->endpoint.serviceUntil(network.nowSeconds());
		for (auto& client : clients)
		{
			client->simHost.now = network.nowSeconds();
			client->peer->endpoint.serviceUntil(network.nowSeconds());
			client->session->drain();
			if (beforeClientFlush)
				beforeClientFlush(*client);
			client->session->flushFrame();
			client->peer->endpoint.serviceUntil(network.nowSeconds());
		}
	}

	void settle(int passes = 50)
	{
		for (int i = 0; i < passes; ++i)
			pass(kStepSeconds);
	}

	static TimeConfig makeConfig()
	{
		TimeConfig out;
		out.tierMinDwellTicks = 4;
		return out;
	}

	TimeConfig config = makeConfig();
	ServerHost serverHost;
	VirtualNetwork network;
	std::vector<std::unique_ptr<EmulatedVirtual>> emulators;
	std::unique_ptr<Peer> serverPeer;
	std::unique_ptr<Server> server;
	std::vector<std::unique_ptr<ClientPeer>> clients;
	std::vector<SimConnectionId> serverIds;
	std::function<void()> beforeServerFlush;
	std::function<void(ClientPeer&)> beforeClientFlush;
};

using PlainRig = Rig<CliGame, VirtualBackend>;

std::vector<Delivery> expectedFromArrivals(const std::vector<Arrival>& arrivals, bool newestWins)
{
	std::vector<Delivery> out;
	std::map<std::uint8_t, std::uint32_t> newest;
	for (const Arrival& arrival : arrivals)
	{
		if (arrival.kind != InboundKind::Message)
			continue;
		if (arrival.message == SimMessageKind::RelayRing)
			out.push_back(Delivery{'r', arrival.simId, arrival.newest});
		if (arrival.message == SimMessageKind::CorrectionState)
		{
			const auto found = newest.find(arrival.simId);
			if (newestWins && found != newest.end() && arrival.tick <= found->second)
				continue;
			newest[arrival.simId] = arrival.tick;
			out.push_back(Delivery{'c', arrival.simId, arrival.tick});
		}
	}
	return out;
}
} // namespace

TEST_CASE("SimNetClient.OverOgNetCorrectionsRingsAndClockTicksReachTheBoundCallbacksInArrivalOrder", "[NetTransport][SimNetClient]")
{
	PlainRig rig;
	auto& owner = rig.join();
	auto& watcher = rig.join();
	std::vector<Delivery> deliveries;
	watcher.simHost.added = [&](std::uint8_t simId) { recordDeliveries(*watcher.session->channel(simId), deliveries); };

	rig.server->addCharacter<CliSim>(kSimA, 0u, rig.serverIds[0], 0u, "a");
	rig.server->addCharacter<CliSim>(kSimB, 1u, kInvalidSimConnection, 0u, "local");
	rig.settle();
	REQUIRE(watcher.session->channel(kSimA) != nullptr);
	REQUIRE(owner.session->channel(kSimA) != nullptr);
	CHECK(owner.session->roster().entryFor(kSimA)->isLocal());
	CHECK(!watcher.session->roster().entryFor(kSimA)->isLocal());

	PendingInputQueue<CliInput> queue;
	std::uint32_t frame = 0u;
	rig.beforeServerFlush = [&] {
		rig.serverHost.tick = static_cast<std::int32_t>(100u + frame);
		rig.server->channel<CliSim>(kSimA)->getSyncedCorrectionStateBuffer().write(stateWithX(static_cast<std::int32_t>(frame)), 200u + frame, 99u + frame);
		rig.server->channel<CliSim>(kSimB)->getSyncedCorrectionStateBuffer().write(stateWithX(static_cast<std::int32_t>(frame)), 200u + frame, 0u);
		rig.server->publishClock(300u + frame);
	};
	rig.beforeClientFlush = [&](PlainRig::ClientPeer& client) {
		if (&client != &owner)
			return;
		queue.enqueue(101u + frame, inputFor(101u + frame));
		owner.peer->transport->beginInputSends();
		owner.session->channel(kSimA)->sendLocalInputToAuthority(queue, 101u + frame, 3u);
		owner.peer->transport->endInputSends();
		queue.releaseAllButRecent(3u);
	};
	watcher.tee->arrivals.clear();
	std::vector<std::size_t> drainEnds;
	for (frame = 0u; frame < 40u; ++frame)
	{
		rig.pass(1.0 / 60.0);
		drainEnds.push_back(watcher.tee->arrivals.size());
	}

	const std::vector<Delivery> expected = expectedFromArrivals(watcher.tee->arrivals, false);
	INFO("delivered:" << describe(deliveries));
	CHECK(deliveries == expected);
	size_t rings = 0u;
	size_t corrections = 0u;
	for (const Delivery& d : deliveries)
		(d.kind == 'r' ? rings : corrections) += 1u;
	CHECK(rings >= 30u);
	CHECK(corrections == 80u);

	std::vector<std::uint32_t> newestPerDrain;
	std::size_t arrivedClockCount = 0u;
	std::size_t drainBegin = 0u;
	for (const std::size_t drainEnd : drainEnds)
	{
		std::optional<std::uint32_t> newest;
		for (std::size_t index = drainBegin; index < drainEnd; ++index)
		{
			const Arrival& arrival = watcher.tee->arrivals[index];
			if (arrival.kind != InboundKind::Message || arrival.message != SimMessageKind::ClockPublish)
				continue;
			++arrivedClockCount;
			if (!newest || arrival.tick > *newest)
				newest = arrival.tick;
		}
		if (newest)
			newestPerDrain.push_back(*newest);
		drainBegin = drainEnd;
	}
	std::vector<std::uint32_t> deliveredClocks;
	for (const auto& sample : watcher.simHost.timing)
	{
		if (sample.first >= 300u)
			deliveredClocks.push_back(sample.first);
	}
	CHECK(deliveredClocks == newestPerDrain);
	CHECK(arrivedClockCount == 40u);
	CHECK(deliveredClocks.back() == 339u);

	std::vector<std::string> expectedLines;
	for (const Delivery& d : deliveries)
	{
		if (d.kind == 'c')
			expectedLines.push_back(receiveLine(d.simId, d.tick));
	}
	CHECK(watcher.simHost.linesWith("[ReceiveCorrectionState]") == expectedLines);
	CHECK(watcher.session->stats().unknownSim == 0u);
	CHECK(watcher.session->stats().badDecode == 0u);
	CHECK(owner.session->stats().ringsStored == 0u);
}

TEST_CASE("SimNetClient.UnderTheEmulatorsDuplicateAndReorderNoOlderCorrectionIsInjected", "[NetTransport][SimNetClient]")
{
	using EmuRig = Rig<CliGame, EmulatedVirtual>;
	EmuRig rig;
	rig.join();
	ognet::EmulatorConfig shaping;
	shaping.seed = 7u;
	shaping.inbound.duplicatePercent = 30.0;
	shaping.inbound.reorder = true;
	shaping.inbound.lagMinMs = 0.0;
	shaping.inbound.lagMaxMs = 40.0;
	auto& shaped = rig.join(shaping);
	std::vector<Delivery> deliveries;
	shaped.simHost.added = [&](std::uint8_t simId) { recordDeliveries(*shaped.session->channel(simId), deliveries); };
	rig.server->addCharacter<CliSim>(kSimA, 0u, rig.serverIds[0], 0u, "a");
	rig.settle(200);
	REQUIRE(shaped.session->channel(kSimA) != nullptr);

	std::uint32_t frame = 0u;
	rig.beforeServerFlush = [&] {
		rig.server->channel<CliSim>(kSimA)->getSyncedCorrectionStateBuffer().write(stateWithX(static_cast<std::int32_t>(frame)), 500u + frame, 0u);
		rig.server->publishClock(900u + frame);
	};
	shaped.tee->arrivals.clear();
	for (frame = 0u; frame < 300u; ++frame)
		rig.pass(1.0 / 120.0);
	rig.beforeServerFlush = nullptr;
	rig.settle(200);

	std::vector<std::uint32_t> injected;
	for (const Delivery& d : deliveries)
	{
		if (d.kind == 'c')
			injected.push_back(d.tick);
	}
	REQUIRE(injected.size() > 100u);
	bool increasing = true;
	for (size_t i = 1u; i < injected.size(); ++i)
		increasing = increasing && injected[i] > injected[i - 1u];
	CHECK(increasing);
	CHECK(deliveries == expectedFromArrivals(shaped.tee->arrivals, true));
	size_t arrivedCorrections = 0u;
	for (const Arrival& arrival : shaped.tee->arrivals)
		arrivedCorrections += arrival.kind == InboundKind::Message && arrival.message == SimMessageKind::CorrectionState ? 1u : 0u;
	const std::uint64_t stale = shaped.session->staleFilter().staleDropped();
	CHECK(stale > 0u);
	CHECK(arrivedCorrections == injected.size() + stale + shaped.session->staleFilter().duplicateDropped());
	CHECK(shaped.emulator->emulatorStats().inbound.duplicated > 0u);
	std::vector<std::uint32_t> clocks;
	for (const auto& sample : shaped.simHost.timing)
	{
		if (sample.first >= 900u)
			clocks.push_back(sample.first);
	}
	bool clocksIncreasing = true;
	for (size_t i = 1u; i < clocks.size(); ++i)
		clocksIncreasing = clocksIncreasing && clocks[i] > clocks[i - 1u];
	CHECK(clocksIncreasing);
	CHECK(arrivedCorrections == 300u);
}

TEST_CASE("SimNetClient.OverOgNetAFloorPublishedBeforeTheListenerBindsIsReplayedOnce", "[NetTransport][SimNetClient]")
{
	PlainRig rig;
	rig.server->publishFloor(6u);
	auto& client = rig.join();
	rig.settle();
	CHECK(client.simHost.events.empty());
	CHECK(client.session->latchedFloor() == std::optional<std::uint8_t>(std::uint8_t{6}));
	client.session->bindListener();
	client.session->drain();
	CHECK(client.simHost.events == std::vector<std::string>{"floor 6 replayed"});
	rig.server->publishFloor(7u);
	rig.settle();
	CHECK(client.simHost.events == std::vector<std::string>{"floor 6 replayed", "floor 7 live"});
}

TEST_CASE("SimNetClient.OverOgNetARevisionGapDisconnectsWithAReason", "[NetTransport][SimNetClient]")
{
	PlainRig rig;
	auto& client = rig.join();
	rig.server->addCharacter<CliSim>(kSimA, 0u, rig.serverIds[0], 0u, "a");
	rig.settle();
	REQUIRE(client.session->channel(kSimA) != nullptr);

	const std::uint16_t revision = client.session->roster().revision();
	const MessageBytes gap = addMsg(static_cast<std::uint16_t>(revision + 2u), remoteEntry(kSimB, 1u));
	REQUIRE(client.peer->transport->inbox().pushMessage(client.session->serverConnection(), SimLane::Control, 0.0, gap));
	client.session->drain();
	CHECK(client.simHost.linesWith("[Roster.Refused]") == std::vector<std::string>{"[Roster.Refused] reason=RevisionGap"});
	const std::optional<SimNetDisconnectRequest> request = client.session->takeDisconnectRequest();
	REQUIRE(request.has_value());
	CHECK(request->reason == RosterRefusal::RevisionGap);
	client.peer->endpoint.disconnect(request->conn);
	REQUIRE(rig.stepUntil([&] { return rig.server->attachedConnections().empty(); }));
	CHECK(rig.server->liveness().upCount() == 0u);
	CHECK(client.session->serverConnection() == kInvalidSimConnection);
}

namespace
{
using RateRig = Rig<RateGame, VirtualBackend>;

struct RateResult
{
	double setsPerSecond = 0.0;
	double receivedPerSecond = 0.0;
	double droppedByCapPerSecond = 0.0;
	double droppedAtSenderPerSecond = 0.0;
	double setsPerSend = 0.0;
};

enum class SendCadence : std::uint8_t { EveryFrame, EveryTick };

constexpr double kTickHz = 60.0;
constexpr std::uint8_t kLocalPlayers = 4u;

double simInputCap()
{
	return static_cast<double>(ognet::lanes::kDefaultLaneTable[ognet::lanes::laneIndex(ognet::Lane::SimInput)].recvMaxMessagesPerSecond);
}

RateResult measureInputRate(double framesPerSecond, std::uint32_t depth, SendCadence cadence)
{
	RateRig rig;
	auto& client = rig.join();
	for (std::uint8_t player = 0u; player < kLocalPlayers; ++player)
		rig.server->addCharacter<RateSim>(static_cast<std::uint8_t>(kSimA + player), player, rig.serverIds[0], player, "p" + std::to_string(player));
	rig.settle();
	for (std::uint8_t player = 0u; player < kLocalPlayers; ++player)
		REQUIRE(client.session->channel(static_cast<std::uint8_t>(kSimA + player)) != nullptr);

	std::array<PendingInputQueue<RateInput>, kLocalPlayers> queues;
	const double frameSeconds = 1.0 / framesPerSecond;
	const double startSeconds = rig.network.nowSeconds();
	std::uint32_t enqueuedThrough = 0u;
	std::uint64_t sends = 0u;
	rig.beforeClientFlush = [&](RateRig::ClientPeer& peer) {
		const auto tick = static_cast<std::uint32_t>(std::floor((rig.network.nowSeconds() - startSeconds) * kTickHz)) + 1u;
		const bool newTick = enqueuedThrough < tick;
		for (; enqueuedThrough < tick; ++enqueuedThrough)
		{
			for (std::uint8_t player = 0u; player < kLocalPlayers; ++player)
				queues[player].enqueue(enqueuedThrough + 1u, RateInput{static_cast<std::int32_t>(enqueuedThrough)});
		}
		if (cadence == SendCadence::EveryTick && !newTick)
			return;
		++sends;
		peer.peer->transport->beginInputSends();
		for (std::uint8_t player = 0u; player < kLocalPlayers; ++player)
		{
			peer.session->channel(static_cast<std::uint8_t>(kSimA + player))->sendLocalInputToAuthority(queues[player], tick, depth);
			queues[player].releaseAllButRecent(depth);
		}
		peer.peer->transport->endInputSends();
	};

	const int warmupFrames = static_cast<int>(framesPerSecond * 2.0);
	const int measuredFrames = static_cast<int>(framesPerSecond * 2.0);
	for (int i = 0; i < warmupFrames; ++i)
		rig.pass(frameSeconds);
	const SimConnectionId toServer = client.session->serverConnection();
	const std::uint64_t setsBefore = client.peer->transport->stats().inputSets;
	const std::uint64_t sendsBefore = sends;
	const ognet::ConnectionStats serverBefore = rig.serverPeer->endpoint.stats(rig.serverIds[0]);
	const ognet::ConnectionStats clientBefore = client.peer->endpoint.stats(toServer);
	for (int i = 0; i < measuredFrames; ++i)
		rig.pass(frameSeconds);
	const std::uint64_t setsAfter = client.peer->transport->stats().inputSets;
	const ognet::ConnectionStats serverAfter = rig.serverPeer->endpoint.stats(rig.serverIds[0]);
	const ognet::ConnectionStats clientAfter = client.peer->endpoint.stats(toServer);
	const size_t lane = static_cast<size_t>(ognet::Lane::SimInput);
	const double seconds = measuredFrames * frameSeconds;
	const auto senderDrops = [lane](const ognet::ConnectionStats& stats) {
		return stats.lanes[lane].queueDropsAge + stats.lanes[lane].queueDropsCap + stats.lanes[lane].sendsRefused + stats.lanes[lane].sendsTooLarge;
	};

	RateResult result;
	result.setsPerSecond = static_cast<double>(setsAfter - setsBefore) / seconds;
	result.receivedPerSecond = static_cast<double>(serverAfter.lanes[lane].messagesReceived - serverBefore.lanes[lane].messagesReceived) / seconds;
	result.droppedByCapPerSecond =
		static_cast<double>(serverAfter.lanes[lane].receiveDroppedRate - serverBefore.lanes[lane].receiveDroppedRate) / seconds;
	result.droppedAtSenderPerSecond = static_cast<double>(senderDrops(clientAfter) - senderDrops(clientBefore)) / seconds;
	result.setsPerSend = sends > sendsBefore ? static_cast<double>(setsAfter - setsBefore) / static_cast<double>(sends - sendsBefore) : 0.0;
	return result;
}
} // namespace

TEST_CASE("SimNetClient.TheClientAndServerChannelsAreOneTypeForOneTransport", "[NetTransport][SimNetClient]")
{
	STATIC_CHECK(std::is_same_v<PlainRig::Client::Channel, PlainRig::Server::Channel<CliSim>>);
	STATIC_CHECK(std::is_same_v<PlainRig::Client::Channel, SessionChannel<CliSim, PlainRig::Transport::ChannelTransport>>);
	STATIC_CHECK(std::is_same_v<RateRig::Client::Channel, RateRig::Server::Channel<RateSim>>);
	STATIC_CHECK(PredictionSyncedBufferOwnerConcept<PlainRig::Client::Channel, CliState, CliInput>);
	STATIC_CHECK(AuthoritySyncedBufferOwnerConcept<PlainRig::Server::Channel<CliSim>, CliState, CliInput>);
}

TEST_CASE("SimNetClient.FourLocalPlayersSendTwoOrFourSimInputMessagesPerSendAndTheCapBoundsWhatArrives", "[NetTransport][SimNetClient]")
{
	const double cap = simInputCap();
	const auto shippedDepth = static_cast<std::uint32_t>(TimeConfig{}.redundancyDepthTicks);

	const RateResult frameShipped = measureInputRate(144.0, shippedDepth, SendCadence::EveryFrame);
	CHECK(frameShipped.setsPerSend == 2.0);
	CHECK(frameShipped.setsPerSecond == Catch::Approx(288.0).margin(1.0));
	CHECK(frameShipped.receivedPerSecond == Catch::Approx(std::min(288.0, cap)).margin(3.0));
	CHECK(frameShipped.droppedByCapPerSecond == Catch::Approx(std::max(0.0, 288.0 - cap)).margin(3.0));
	CHECK(frameShipped.droppedAtSenderPerSecond == 0.0);

	const RateResult frameDeepest = measureInputRate(144.0, inputRedundancyBundle::kMaxSlots, SendCadence::EveryFrame);
	CHECK(frameDeepest.setsPerSend == 4.0);
	CHECK(frameDeepest.setsPerSecond == Catch::Approx(576.0).margin(1.0));
	CHECK(frameDeepest.receivedPerSecond == Catch::Approx(std::min(576.0, cap)).margin(3.0));
	CHECK(frameDeepest.droppedByCapPerSecond == Catch::Approx(std::max(0.0, 576.0 - cap)).margin(3.0));

	const RateResult tickDeepest = measureInputRate(144.0, inputRedundancyBundle::kMaxSlots, SendCadence::EveryTick);
	CHECK(tickDeepest.setsPerSend == 4.0);
	CHECK(tickDeepest.setsPerSecond == Catch::Approx(240.0).margin(1.0));
	CHECK(tickDeepest.receivedPerSecond == Catch::Approx(240.0).margin(3.0));
	CHECK(tickDeepest.droppedByCapPerSecond == 0.0);
}

TEST_CASE("SimNetClient.Rate.TheSimInputRateTable", "[.][SimNetRate]")
{
	std::printf("[SimNetRate] cap=%.0f/s (og-net's SimInput receive cap in this tree); tick=%.0f Hz; %u local players\n", simInputCap(), kTickHz,
		static_cast<unsigned>(kLocalPlayers));
	for (const SendCadence cadence : {SendCadence::EveryFrame, SendCadence::EveryTick})
	{
		for (const double fps : {60.0, 120.0, 144.0, 240.0})
		{
			for (const std::uint32_t depth : {3u, 8u})
			{
				const RateResult r = measureInputRate(fps, depth, cadence);
				std::printf("[SimNetRate] cadence=%s fps=%.0f depth=%u setsPerSend=%.2f sent/s=%.1f received/s=%.1f droppedByCap/s=%.1f "
							"droppedAtSender/s=%.1f headroomVs300=%.1f headroomVs1200=%.1f\n",
					cadence == SendCadence::EveryFrame ? "frame" : "tick", fps, depth, r.setsPerSend, r.setsPerSecond, r.receivedPerSecond,
					r.droppedByCapPerSecond, r.droppedAtSenderPerSecond, 300.0 - r.setsPerSecond, 1200.0 - r.setsPerSecond);
			}
		}
	}
	SUCCEED();
}
