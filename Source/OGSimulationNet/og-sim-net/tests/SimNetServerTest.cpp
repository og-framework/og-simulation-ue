// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetConnections.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/RelayRingStager.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationQueues.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace ogsimnet;

namespace simNetServerTest
{
struct SrvPosition
{
	std::int32_t x = 0;
};

struct SrvInput
{
	std::int32_t move = 0;
	std::uint8_t flags = 0u;
};
} // namespace simNetServerTest

template <>
struct SerializableFields<simNetServerTest::SrvPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetServerTest::SrvPosition, x)); }
};

template <>
struct SerializableFields<simNetServerTest::SrvInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(simNetServerTest::SrvInput, move), SIM_MEMBER(simNetServerTest::SrvInput, flags));
	}
};

namespace simNetServerTest
{
using SrvState = SimulationComposite<SrvPosition>;

template <int Tag>
struct SrvSim
{
	using StateType = SrvState;
	using InputType = SrvInput;
};

using SimA = SrvSim<0>;
using SimB = SrvSim<1>;

struct MockGame
{
	using Simulatables = SimulatableList<SimA, SimB>;
};

std::vector<std::uint8_t> toU8(std::span<const std::byte> bytes)
{
	std::vector<std::uint8_t> out;
	for (const std::byte b : bytes)
		out.push_back(std::to_integer<std::uint8_t>(b));
	return out;
}

struct FakeHost
{
	double now = 0.0;
	double defaultRttMs = 20.0;
	std::int32_t tick = 0;
	std::unordered_map<SimConnectionId, double> rttByConn;
	std::vector<std::string> lines;
	std::vector<std::tuple<std::uint8_t, std::uint32_t, double>> serverReceived;
	std::vector<std::pair<std::uint8_t, std::uint32_t>> handed;

	double nowSeconds() { return now; }
	double rttMs(SimConnectionId conn)
	{
		const auto found = rttByConn.find(conn);
		return found == rttByConn.end() ? defaultRttMs : found->second;
	}
	std::int32_t receptionTick() { return tick; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t simId, std::uint32_t captureTick, double recvSeconds)
	{
		serverReceived.emplace_back(simId, captureTick, recvSeconds);
	}
	void stampHanded(std::uint8_t simId, std::uint32_t captureTick) { handed.emplace_back(simId, captureTick); }
};

static_assert(ogsimnet::SimNetServerHost<FakeHost>);
static_assert(!ogsimnet::SimNetDeliveryHook<FakeHost>);

struct DeliveryHost : FakeHost
{
	std::vector<std::pair<SimLane, double>> deliveries;

	void noteDelivered(SimLane lane, double recvSeconds) { deliveries.emplace_back(lane, recvSeconds); }
};

static_assert(ogsimnet::SimNetServerHost<DeliveryHost> && ogsimnet::SimNetDeliveryHook<DeliveryHost>);

struct RecordingEndpoint
{
	struct Call
	{
		bool broadcast = false;
		ognet::ConnectionId id = ognet::kInvalidConnection;
		ognet::Lane lane = ognet::Lane::Control;
		std::vector<std::uint8_t> bytes;

		friend bool operator==(const Call&, const Call&) = default;
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

using FakeTransport = ogsimnet::SimNetTransport<RecordingEndpoint, 256>;
using FakeServer = ogsimnet::SimNetServer<MockGame, FakeHost, FakeTransport>;
using Shape = FakeServer::Shape;

constexpr SimConnectionId kConnA = 5u;
constexpr SimConnectionId kConnB = 6u;
constexpr std::uint8_t kSimA0 = 3u;
constexpr std::uint8_t kSimB1 = 4u;
constexpr std::uint8_t kSimOther = 7u;
constexpr std::uint8_t kSimUnknown = 9u;

TimeConfig testTimeConfig()
{
	TimeConfig config;
	config.tierMinDwellTicks = 4;
	return config;
}

struct Slot
{
	std::uint32_t captureTick = 0u;
	std::int32_t move = 0;
};

InputBundleBuffer bundleOf(const std::vector<Slot>& slots)
{
	InputBundleBuffer bundle;
	for (const Slot& slot : slots)
		bundle.appendSlot<SrvInput>(slot.captureTick, SrvInput{slot.move, static_cast<std::uint8_t>(slot.move & 0x7F)});
	return bundle;
}

std::vector<std::byte> inputSet(const std::vector<std::pair<std::uint8_t, std::vector<Slot>>>& entries)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	size_t used = simEnvelope::writeInputBundleSetHeader(out);
	for (const auto& [simId, slots] : entries)
	{
		const InputBundleBuffer bundle = bundleOf(slots);
		used = simEnvelope::appendInputBundle(out, used, simId, bundle.payload());
		REQUIRE(used != 0u);
	}
	return std::vector<std::byte>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(used));
}

struct Delivered
{
	std::uint32_t simId = 0u;
	std::uint32_t captureTick = 0u;
	std::int32_t move = 0;

	friend bool operator==(const Delivered&, const Delivered&) = default;
};

struct Fixture
{
	explicit Fixture(TimeConfig configIn = testTimeConfig(), ogsimnet::SimNetConfig transportConfig = {})
		: config(configIn)
		, transport(std::make_unique<FakeTransport>(transportConfig))
	{
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		server = std::make_unique<FakeServer>(host, *transport, config, size_t{4096});
	}

	void connect(SimConnectionId conn) { transport->inbox().pushEvent(InboundKind::Connected, conn, ogsimnet::kNoReceiveSeconds); }
	void disconnect(SimConnectionId conn) { transport->inbox().pushEvent(InboundKind::Disconnected, conn, ogsimnet::kNoReceiveSeconds); }
	void push(SimConnectionId conn, const std::vector<std::byte>& bytes, double recvSeconds = 1.5)
	{
		REQUIRE(transport->inbox().pushMessage(conn, SimLane::SimInput, recvSeconds, bytes));
	}

	template <class SimT>
	void addRecorded(std::uint8_t simId, std::uint8_t slot, SimConnectionId owner, std::uint8_t localIndex)
	{
		auto* const channel = server->addCharacter<SimT>(simId, slot, owner, localIndex, "char" + std::to_string(simId));
		REQUIRE(channel != nullptr);
		channel->setOnRemoteMoveReceivedCallback([this, simId](std::uint32_t captureTick, const SrvInput& input) {
			delivered.push_back(Delivered{simId, captureTick, input.move});
		});
	}

	std::vector<std::string> linesWith(const char* tag) const
	{
		std::vector<std::string> out;
		for (const std::string& line : host.lines)
		{
			if (line.find(tag) != std::string::npos)
				out.push_back(line);
		}
		return out;
	}

	TimeConfig config;
	FakeHost host;
	RecordingEndpoint endpoint;
	std::unique_ptr<FakeTransport> transport;
	std::unique_ptr<FakeServer> server;
	std::vector<Delivered> delivered;
};

struct ReferenceSinks
{
	std::vector<Delivered>* delivered = nullptr;
	std::map<std::uint8_t, std::unique_ptr<RelayRingStager>> stagers;
	std::vector<std::pair<unsigned int, std::uint8_t>> tiers;

	void sendConnectionTierToOwningClient(unsigned int id, std::uint8_t tier) { tiers.emplace_back(id, tier); }

	template <class InputT>
	void deliverRemoteInput(unsigned int id, std::uint32_t captureTick, const InputT& input)
	{
		delivered->push_back(Delivered{id, captureTick, input.move});
	}

	template <class InputT>
	void relayRemoteInput(unsigned int id, std::uint32_t captureTick, std::uint8_t dA, const InputT& input)
	{
		std::unique_ptr<RelayRingStager>& stager = stagers[static_cast<std::uint8_t>(id)];
		if (!stager)
			stager = std::make_unique<RelayRingStager>("ref" + std::to_string(id));
		stager->stageArrival<InputT>(captureTick, dA, input);
	}
};

struct Reference
{
	using Coordinator = ServerReceptionCoordinator<ogsimnet::SimNetConnectionHandle, SimA, SimB>;

	explicit Reference(const TimeConfig& config)
		: coordinator(config)
	{
		sinks.delivered = &delivered;
		coordinator.setLogger([this](const char* line) { lines.emplace_back(line); });
	}

	void event(InboundKind kind, SimConnectionId conn)
	{
		InboundSimRecord record;
		record.kind = kind;
		record.conn = conn;
		liveness.apply(record);
	}

	ogsimnet::SimNetConnectionHandle handle(SimConnectionId conn) const { return liveness.handleFor(conn); }

	template <class SimT>
	void bundle(SimConnectionId conn, std::uint8_t simId, std::uint8_t localIndex, const std::vector<Slot>& slots, std::int32_t tick, double rttMs)
	{
		const InputBundleBuffer wire = bundleOf(slots);
		coordinator.noteRttSample(handle(conn), simId, tick, rttMs, sinks);
		coordinator.receiveInputBundle<SimT>(simId, handle(conn), localIndex, wire, sinks, sinks);
	}

	void release(std::int32_t firstTick, std::int32_t numSteps)
	{
		auto deliver = [this](unsigned int id, std::uint32_t captureTick, const SrvInput& input) -> bool {
			delivered.push_back(Delivered{id, captureTick, input.move});
			return true;
		};
		coordinator.releaseDelayedInputs<SimA>(firstTick, numSteps, deliver);
		coordinator.releaseDelayedInputs<SimB>(firstTick, numSteps, deliver);
	}

	std::vector<RecordingEndpoint::Call> flushRelays(std::uint8_t resendEntries, const std::map<std::uint8_t, SimConnectionId>& owners)
	{
		std::vector<RecordingEndpoint::Call> out;
		for (const auto& [simId, owner] : owners)
		{
			std::unique_ptr<RelayRingStager>& stager = sinks.stagers[simId];
			if (!stager)
				stager = std::make_unique<RelayRingStager>("ref" + std::to_string(simId));
			if (stager->flush(resendEntries) == 0u)
				continue;
			std::array<std::byte, kMaxSimMessageBytes> bytes{};
			const size_t size = simEnvelope::writeRelayRing(bytes, simId, stager->published().payload());
			out.push_back(RecordingEndpoint::Call{true, owner, ognet::Lane::SimRelay, toU8(std::span<const std::byte>(bytes.data(), size))});
		}
		return out;
	}

	ogsimnet::LivenessTable liveness;
	Coordinator coordinator;
	ReferenceSinks sinks;
	std::vector<std::string> lines;
	std::vector<Delivered> delivered;
};

std::vector<std::string> coordinatorLines(const std::vector<std::string>& lines)
{
	std::vector<std::string> out;
	for (const std::string& line : lines)
	{
		if (line.find("[Roster.") != std::string::npos || line.find("[RelayFlush]") != std::string::npos
			|| line.find("[SimNet.Window]") != std::string::npos)
			continue;
		out.push_back(line);
	}
	return out;
}

size_t countWith(const std::vector<std::string>& lines, const char* tag)
{
	size_t count = 0u;
	for (const std::string& line : lines)
		count += line.find(tag) != std::string::npos ? 1u : 0u;
	return count;
}

struct Decoded
{
	SimMessageKind kind = SimMessageKind{};
	DecodedSimMessage message;
	std::vector<std::uint8_t> bytes;
};

DecodedSimMessage decodeDown(SimLane lane, const std::vector<std::uint8_t>& bytes, std::vector<std::byte>& storage)
{
	storage.clear();
	for (const std::uint8_t b : bytes)
		storage.push_back(std::byte{b});
	return simEnvelope::decode<Shape>(lane, SimDirection::ServerToClient, storage);
}

std::vector<std::uint8_t> correctionEnvelope(std::uint8_t simId, const CorrectionStateBuffer& buffer)
{
	std::array<std::byte, kMaxSimMessageBytes> bytes{};
	const size_t size = simEnvelope::writeCorrectionState(bytes, simId, correctionStateBuffer::kWireFormatVersion, buffer.payload());
	return toU8(std::span<const std::byte>(bytes.data(), size));
}

SrvState stateWithX(std::int32_t x)
{
	SrvState state;
	state.edit<SrvPosition>().x = x;
	return state;
}
} // namespace simNetServerTest

using namespace simNetServerTest;

TEST_CASE("SimNetServer.TheSessionSatisfiesTheCoordinatorsSinksAndTheSeams", "[NetTransport][SimNetServer]")
{
	STATIC_CHECK(ConnectionTierSink<FakeServer>);
	STATIC_CHECK(RemoteInputDeliverySink<FakeServer, SrvInput>);
	STATIC_CHECK(RemoteInputRelaySink<FakeServer, SrvInput>);
	STATIC_CHECK(ogsimnet::CorrectionPolicy<ogsimnet::SendAllDirty>);
	STATIC_CHECK(ogsimnet::SimNetServerTransport<FakeTransport>);
	STATIC_CHECK(ogsimnet::SimNetGame<MockGame>);
	STATIC_CHECK(!ogsimnet::SimNetGame<SimA>);
	STATIC_CHECK(ogsimnet::correctionAllowanceBytes(ognet::lanes::kDefaultLaneTable, ognet::lanes::kDefaultBudget, 60.0) == 1250u);
}

TEST_CASE("SimNetServer.DecisionsEqualTheCoordinatorFedDirectlyWithTheSameArguments", "[NetTransport][SimNetServer]")
{
	Fixture f;
	Reference ref(f.config);
	f.host.rttByConn[kConnA] = 100.0;
	f.host.rttByConn[kConnB] = 20.0;

	f.connect(kConnA);
	f.connect(kConnB);
	ref.event(InboundKind::Connected, kConnA);
	ref.event(InboundKind::Connected, kConnB);
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.addRecorded<SimB>(kSimB1, 1u, kConnA, 1u);
	f.addRecorded<SimA>(kSimOther, 2u, kConnB, 0u);
	const std::map<std::uint8_t, SimConnectionId> owners{{kSimA0, kConnA}, {kSimB1, kConnA}, {kSimOther, kConnB}};

	const std::int32_t firstTick = 100;
	size_t parkedTierTwoFrames = 0u;
	for (std::int32_t frame = 0; frame < 12; ++frame)
	{
		const std::int32_t t = firstTick + frame;
		const std::uint32_t u = static_cast<std::uint32_t>(t);
		f.host.tick = t;
		f.config.relayResendEntries = frame >= 6 ? 2 : 0;

		const std::vector<Slot> slotsA0{{u, frame}, {u - 1u, frame + 100}, {u - 2u, frame + 200}};
		const std::vector<Slot> slotsB1{{u, -frame}, {u - 1u, -frame - 100}};
		f.push(kConnA, inputSet({{kSimA0, slotsA0}, {kSimB1, slotsB1}}));
		ref.bundle<SimA>(kConnA, kSimA0, 0u, slotsA0, t, 100.0);
		ref.bundle<SimB>(kConnA, kSimB1, 1u, slotsB1, t, 100.0);

		std::vector<Slot> slotsOther{{u, 1000 + frame}};
		if (frame == 3)
			slotsOther.push_back(Slot{u + 50u, 5000});
		if (frame == 5)
			slotsOther.push_back(Slot{u - 6u, 6000});
		f.push(kConnB, inputSet({{kSimOther, slotsOther}}));
		ref.bundle<SimA>(kConnB, kSimOther, 0u, slotsOther, t, 20.0);

		if (frame == 7)
		{
			std::vector<std::byte> truncated = inputSet({{kSimA0, {{u, 9999}}}});
			truncated.pop_back();
			f.push(kConnA, truncated);
		}

		f.server->dispatchFrame();
		f.server->releaseForStep(t, 1);
		ref.release(t, 1);

		f.endpoint.calls.clear();
		f.server->flushFrame();
		const std::uint8_t resend = static_cast<std::uint8_t>(latencyKnobs::clampRelayResendEntries(f.config.relayResendEntries));
		CHECK(f.endpoint.on(ognet::Lane::SimRelay) == ref.flushRelays(resend, owners));

		f.server->afterBatch(t + 1);
		ref.coordinator.reapConnections(t + 1);

		const std::int32_t tierA = f.server->coordinator().lookupTierIndex(f.server->handleFor(kConnA));
		CHECK(tierA == ref.coordinator.lookupTierIndex(ref.handle(kConnA)));
		CHECK(f.server->coordinator().lookupTierIndex(f.server->handleFor(kConnB)) == ref.coordinator.lookupTierIndex(ref.handle(kConnB)));
		parkedTierTwoFrames += tierA == 2 ? 1u : 0u;
	}

	{
		const std::uint32_t u = static_cast<std::uint32_t>(firstTick + 12);
		const InputBundleBuffer wire = bundleOf({{u, 7777}});
		f.server->coordinator().receiveInputBundle<SimA>(kSimA0, f.server->handleFor(kConnA), 8u, wire, *f.server, *f.server);
		ref.coordinator.receiveInputBundle<SimA>(kSimA0, ref.handle(kConnA), 8u, wire, ref.sinks, ref.sinks);
	}

	const std::vector<std::string> sessionLines = coordinatorLines(f.host.lines);
	CHECK(sessionLines == ref.lines);
	CHECK(f.delivered == ref.delivered);
	CHECK(f.server->coordinator().outOfDomainRejectCount() == ref.coordinator.outOfDomainRejectCount());
	CHECK(f.server->coordinator().lateDroppedCount() == ref.coordinator.lateDroppedCount());
	CHECK(f.server->coordinator().relayOooSkipCount() == ref.coordinator.relayOooSkipCount());
	CHECK(f.server->stats().badDecode == 1u);

	CHECK(countWith(sessionLines, "[Park]") > 20u);
	CHECK(countWith(sessionLines, "[Release]") > 20u);
	CHECK(ref.coordinator.outOfDomainRejectCount() >= 1u);
	CHECK(ref.coordinator.lateDroppedCount() >= 1u);
	CHECK(parkedTierTwoFrames >= 4u);
	CHECK(parkedTierTwoFrames < 12u);
	CHECK(std::find_if(f.delivered.begin(), f.delivered.end(), [](const Delivered& d) { return d.move == 7777; }) != f.delivered.end());
	CHECK(f.host.serverReceived.size() == 12u * 6u + 2u);
}

TEST_CASE("SimNetServer.ABundleForACharacterOfAnotherConnectionIsRefusedAndCounted", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.host.tick = 100;

	f.push(kConnB, inputSet({{kSimA0, {{100u, 1}, {99u, 2}}}, {kSimUnknown, {{100u, 3}}}}));
	f.server->dispatchFrame();
	f.server->releaseForStep(100, 8);

	CHECK(f.server->stats().refusedNotOwner == 1u);
	CHECK(f.server->stats().unknownSim == 1u);
	CHECK(f.server->coordinator().claimCount() == 0u);
	CHECK(f.linesWith("[ServerReceive]").empty());
	CHECK(f.linesWith("[Park]").empty());
	CHECK(f.host.serverReceived.empty());
	CHECK(f.delivered.empty());
	CHECK(f.linesWith("[Roster.InputRefused]") == std::vector<std::string>{"[Roster.InputRefused] conn=6 simId=3 reason=NotOwner"});

	f.push(kConnA, inputSet({{kSimA0, {{110u, 4}}}}));
	f.server->dispatchFrame();
	CHECK(f.server->coordinator().claimCount() == 1u);
	CHECK(f.linesWith("[Park]").size() == 1u);
}

TEST_CASE("SimNetServer.TheTierGoesOncePerConnectionChangeAndTierZeroIsNeverAFirstValue", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.host.rttByConn[kConnA] = 100.0;
	f.host.rttByConn[kConnB] = 10.0;
	f.connect(kConnA);
	f.connect(kConnB);
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.addRecorded<SimB>(kSimB1, 1u, kConnA, 1u);
	f.addRecorded<SimA>(kSimOther, 2u, kConnB, 0u);

	for (std::int32_t frame = 0; frame < 8; ++frame)
	{
		const std::uint32_t u = static_cast<std::uint32_t>(100 + frame);
		f.host.tick = 100 + frame;
		f.push(kConnA, inputSet({{kSimA0, {{u, 1}}}, {kSimB1, {{u, 2}}}}));
		f.push(kConnB, inputSet({{kSimOther, {{u, 3}}}}));
		f.server->dispatchFrame();
		f.server->flushFrame();
	}

	std::vector<RecordingEndpoint::Call> tiers;
	std::vector<std::byte> storage;
	for (const RecordingEndpoint::Call& call : f.endpoint.on(ognet::Lane::Control))
	{
		if (decodeDown(SimLane::Control, call.bytes, storage).kind == SimMessageKind::ConnectionTier)
			tiers.push_back(call);
	}
	REQUIRE(tiers.size() == 2u);
	CHECK(tiers[0].id == kConnA);
	CHECK(tiers[1].id == kConnA);
	CHECK(!tiers[0].broadcast);
	CHECK(decodeDown(SimLane::Control, tiers[0].bytes, storage).value == 1u);
	CHECK(decodeDown(SimLane::Control, tiers[1].bytes, storage).value == 2u);
	CHECK(f.server->coordinator().lookupTierIndex(f.server->handleFor(kConnA)) == 2);
	CHECK(f.server->stats().tiersSent == 2u);

	f.server->publishTier(kConnB, 0u);
	f.server->publishTier(kConnA, 2u);
	f.server->flushFrame();
	CHECK(f.server->stats().tiersSent == 2u);
	f.server->publishTier(kConnA, 1u);
	f.server->publishTier(kConnA, 0u);
	CHECK(f.server->stats().tiersSent == 4u);

	f.server->publishTier(kConnA, 1u);
	CHECK(f.server->stats().tiersSent == 5u);
	f.disconnect(kConnA);
	f.connect(kConnA);
	f.server->dispatchFrame();
	f.server->publishTier(kConnA, 0u);
	CHECK(f.server->stats().tiersSent == 5u);
	f.server->publishTier(kConnA, 1u);
	CHECK(f.server->stats().tiersSent == 6u);
}

TEST_CASE("SimNetServer.TheFloorGoesToAttachedConnectionsOnEveryChangeIncludingAFirstZero", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.connect(kConnA);
	f.server->dispatchFrame();
	f.server->publishFloor(0u);
	f.server->publishFloor(0u);
	CHECK(f.server->stats().floorsSent == 1u);
	f.server->publishFloor(4u);
	CHECK(f.server->stats().floorsSent == 2u);
	f.connect(kConnB);
	f.server->dispatchFrame();
	CHECK(f.server->stats().floorsSent == 3u);
	f.server->flushFrame();

	std::vector<std::pair<SimConnectionId, std::uint8_t>> floors;
	std::vector<std::byte> storage;
	for (const RecordingEndpoint::Call& call : f.endpoint.on(ognet::Lane::Control))
	{
		const DecodedSimMessage message = decodeDown(SimLane::Control, call.bytes, storage);
		if (message.kind == SimMessageKind::RelayFloor)
			floors.emplace_back(call.id, message.value);
	}
	CHECK(floors == std::vector<std::pair<SimConnectionId, std::uint8_t>>{{kConnA, 0u}, {kConnA, 4u}, {kConnB, 4u}});
}

TEST_CASE("SimNetServer.TheReapDropsADisconnectedConnectionsClaimsAtTheNextDwellBoundary", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.addRecorded<SimA>(kSimOther, 1u, kConnB, 0u);
	f.host.tick = 100;
	f.push(kConnA, inputSet({{kSimA0, {{120u, 1}}}}));
	f.push(kConnB, inputSet({{kSimOther, {{120u, 2}}}}));
	f.server->dispatchFrame();
	f.server->afterBatch(100);
	REQUIRE(f.server->coordinator().claimCount() == 2u);

	f.disconnect(kConnB);
	f.server->dispatchFrame();
	CHECK(!f.server->handleFor(kConnB).isAlive());
	CHECK(f.server->attachedConnections().size() == 1u);

	f.server->afterBatch(101);
	f.server->afterBatch(102);
	f.server->afterBatch(103);
	CHECK(f.server->coordinator().claimCount() == 2u);
	f.server->afterBatch(104);
	CHECK(f.server->coordinator().claimCount() == 1u);
	CHECK(f.server->coordinator().hasClaim(ConnectionSlotKey<ogsimnet::SimNetConnectionHandle>(f.server->handleFor(kConnA), 0u)));
}

TEST_CASE("SimNetServer.RemovingACharacterReportsItsStagerSendsTheRemoveAndForgetsIt", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.server->dispatchFrame();
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.host.tick = 100;
	f.push(kConnA, inputSet({{kSimA0, {{110u, 1}}}}));
	f.server->dispatchFrame();
	REQUIRE(f.server->coordinator().claimCount() == 1u);

	f.server->removeCharacter(kSimA0, RosterRemoveReason::Left);
	CHECK(f.linesWith("[RelayFlush] teardown owner=char3").size() == 1u);
	CHECK(f.server->coordinator().claimCount() == 0u);
	CHECK(f.server->channel<SimA>(kSimA0) == nullptr);
	CHECK(f.server->stager(kSimA0) == nullptr);
	CHECK(f.server->roster().liveCount() == 0u);

	f.server->flushFrame();
	std::vector<std::byte> storage;
	std::vector<std::pair<SimConnectionId, RosterKind>> roster;
	for (const RecordingEndpoint::Call& call : f.endpoint.on(ognet::Lane::Control))
	{
		const DecodedSimMessage message = decodeDown(SimLane::Control, call.bytes, storage);
		REQUIRE(message.kind == SimMessageKind::Roster);
		roster.emplace_back(call.id, rosterCodec::decode(message.body).kind);
	}
	CHECK(roster == std::vector<std::pair<SimConnectionId, RosterKind>>{{kConnA, RosterKind::Snapshot}, {kConnB, RosterKind::Snapshot},
		{kConnA, RosterKind::Add}, {kConnB, RosterKind::Add}, {kConnA, RosterKind::Remove}, {kConnB, RosterKind::Remove}});

	f.server->addCharacter<SimB>(11u, 0u, kConnA, 0u, "reuse");
	CHECK(f.server->roster().liveCount() == 1u);
}

TEST_CASE("SimNetServer.AnAddWithALiveSimIdOrALiveSlotIsRefusedAndChangesNothing", "[NetTransport][SimNetServer]")
{
	constexpr SimConnectionId kConnC = 8u;
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.server->dispatchFrame();
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.server->flushFrame();
	FakeServer::Channel<SimA>* const first = f.server->channel<SimA>(kSimA0);
	REQUIRE(first != nullptr);
	const RelayRingStager* const firstStager = f.server->stager(kSimA0);
	const RosterMessage snapshotBefore = f.server->roster().snapshotFor(kConnC);
	const std::uint16_t revisionBefore = f.server->roster().revision();
	const std::size_t callsBefore = f.endpoint.calls.size();

	CHECK(f.server->addCharacter<SimA>(kSimA0, 5u, kConnB, 0u, "again") == nullptr);
	CHECK(f.server->addCharacter<SimB>(kSimA0, 6u, kConnB, 1u, "otherType") == nullptr);
	CHECK(f.server->addCharacter<SimB>(kSimB1, 0u, kConnB, 0u, "slot") == nullptr);
	CHECK(f.server->stats().addRefused == 3u);
	CHECK(f.linesWith("[Roster.Refused]")
		== std::vector<std::string>{"[Warning][Roster.Refused] role=server simId=3 slot=5 reason=SimIdLive",
			"[Warning][Roster.Refused] role=server simId=3 slot=6 reason=SimIdLive",
			"[Warning][Roster.Refused] role=server simId=4 slot=0 reason=SlotLive"});

	CHECK(f.server->channel<SimA>(kSimA0) == first);
	CHECK(first->simId() == kSimA0);
	CHECK(first->debugName() == "char3");
	CHECK(f.server->channel<SimB>(kSimA0) == nullptr);
	CHECK(f.server->channel<SimB>(kSimB1) == nullptr);
	CHECK(f.server->stager(kSimA0) == firstStager);
	CHECK(f.server->stager(kSimB1) == nullptr);
	CHECK(f.server->roster().revision() == revisionBefore);
	CHECK(f.server->roster().liveCount() == 1u);
	CHECK(f.server->roster().ownerOf(kSimA0) == std::optional<SimConnectionId>(kConnA));
	f.server->flushFrame();
	CHECK(f.endpoint.calls.size() == callsBefore);

	f.connect(kConnC);
	f.server->dispatchFrame();
	f.server->flushFrame();
	std::array<std::byte, kMaxSimMessageBytes> expected{};
	const size_t expectedSize = simEnvelope::writeRoster(expected, snapshotBefore.view());
	const std::vector<RecordingEndpoint::Call> controlCalls = f.endpoint.on(ognet::Lane::Control);
	REQUIRE(!controlCalls.empty());
	CHECK(controlCalls.back().id == kConnC);
	CHECK(controlCalls.back().bytes == toU8(std::span<const std::byte>(expected.data(), expectedSize)));

	f.host.tick = 100;
	f.push(kConnA, inputSet({{kSimA0, {{110u, 1}}}}));
	f.push(kConnB, inputSet({{kSimA0, {{111u, 2}}}}));
	f.server->dispatchFrame();
	CHECK(f.server->coordinator().claimCount() == 1u);
	CHECK(f.server->stats().refusedNotOwner == 1u);

	CHECK(f.server->addCharacter<SimA>(kSimOther, kRosterNoSlot, kInvalidSimConnection, kRosterNotLocal, "noSlot1") != nullptr);
	CHECK(f.server->addCharacter<SimA>(kSimUnknown, kRosterNoSlot, kInvalidSimConnection, kRosterNotLocal, "noSlot2") != nullptr);
	f.server->removeCharacter(kSimA0, RosterRemoveReason::Left);
	CHECK(f.server->addCharacter<SimB>(kSimB1, 0u, kConnB, 0u, "slotAgain") != nullptr);
	CHECK(f.server->stats().addRefused == 3u);
	CHECK(f.server->roster().liveCount() == 3u);
}

TEST_CASE("SimNetServer.AnAddTheRosterRefusesAsFullOrDuplicateOwnerSlotReturnsNullAndChangesNothing", "[NetTransport][SimNetServer]")
{
	constexpr SimConnectionId kConnC = 8u;
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.server->dispatchFrame();
	f.addRecorded<SimA>(1u, 0u, kConnA, 0u);
	f.server->flushFrame();
	const std::uint16_t revisionBefore = f.server->roster().revision();
	std::size_t callsBefore = f.endpoint.calls.size();

	CHECK(f.server->addCharacter<SimB>(2u, 1u, kConnA, 0u, "dup") == nullptr);
	CHECK(f.server->stats().addRefused == 1u);
	CHECK(f.server->channel<SimB>(2u) == nullptr);
	CHECK(f.server->stager(2u) == nullptr);
	CHECK(f.server->roster().revision() == revisionBefore);
	CHECK(f.server->roster().liveCount() == 1u);
	f.server->flushFrame();
	CHECK(f.endpoint.calls.size() == callsBefore);

	for (std::uint8_t simId = 10u; simId <= 16u; ++simId)
		f.addRecorded<SimA>(simId, static_cast<std::uint8_t>(simId - 9u), kInvalidSimConnection, kRosterNotLocal);
	REQUIRE(f.server->roster().liveCount() == 8u);
	f.server->flushFrame();
	const RosterMessage snapshotBefore = f.server->roster().snapshotFor(kConnC);
	const std::uint16_t revisionFull = f.server->roster().revision();
	callsBefore = f.endpoint.calls.size();

	CHECK(f.server->addCharacter<SimA>(20u, kRosterNoSlot, kConnB, 0u, "full") == nullptr);
	CHECK(f.server->stats().addRefused == 2u);
	CHECK(f.linesWith("[Roster.Refused]")
		== std::vector<std::string>{"[Warning][Roster.Refused] role=server simId=2 slot=1 reason=DuplicateOwnerSlot",
			"[Warning][Roster.Refused] role=server simId=20 slot=255 reason=Full"});
	CHECK(f.server->channel<SimA>(20u) == nullptr);
	CHECK(f.server->stager(20u) == nullptr);
	CHECK(f.server->roster().revision() == revisionFull);
	CHECK(f.server->roster().liveCount() == 8u);
	f.server->flushFrame();
	CHECK(f.endpoint.calls.size() == callsBefore);

	f.connect(kConnC);
	f.server->dispatchFrame();
	f.server->flushFrame();
	std::array<std::byte, kMaxSimMessageBytes> expected{};
	const size_t expectedSize = simEnvelope::writeRoster(expected, snapshotBefore.view());
	const std::vector<RecordingEndpoint::Call> controlCalls = f.endpoint.on(ognet::Lane::Control);
	REQUIRE(!controlCalls.empty());
	CHECK(controlCalls.back().id == kConnC);
	CHECK(controlCalls.back().bytes == toU8(std::span<const std::byte>(expected.data(), expectedSize)));

	f.server->removeCharacter(16u, RosterRemoveReason::Despawned);
	CHECK(f.server->addCharacter<SimA>(20u, kRosterNoSlot, kConnB, 0u, "notSpent") != nullptr);
	CHECK(f.server->addCharacter<SimB>(2u, 7u, kConnA, 1u, "fullAgain") == nullptr);
	CHECK(f.server->stats().addRefused == 3u);
	CHECK(f.linesWith("[Roster.Refused]").back() == "[Warning][Roster.Refused] role=server simId=2 slot=7 reason=Full");
	CHECK(f.server->roster().ownerOf(20u) == std::optional<SimConnectionId>(kConnB));
	CHECK(f.server->roster().revision() == static_cast<std::uint16_t>(revisionFull + 2u));
}

TEST_CASE("SimNetServer.ItsChannelIsTheSessionChannelTypeAndAnEmptyBundleIsCounted", "[NetTransport][SimNetServer]")
{
	STATIC_CHECK(std::is_same_v<FakeServer::ChannelTransport, FakeTransport::ChannelTransport>);
	STATIC_CHECK(std::is_same_v<FakeServer::Channel<SimA>, SessionChannel<SimA, FakeTransport::ChannelTransport>>);
	STATIC_CHECK(std::is_same_v<FakeServer::Channel<SimB>, SimNetChannel<SimB, SimNetBundleTransport<FakeTransport::ChannelTransport>>>);

	Fixture f;
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	PendingInputQueue<SrvInput> queue;
	f.server->channel<SimA>(kSimA0)->sendLocalInputToAuthority(queue, 10u, 3u);
	CHECK(f.server->stats().emptyBundles == 1u);
	queue.enqueue(10u, SrvInput{4, 4u});
	f.server->channel<SimA>(kSimA0)->sendLocalInputToAuthority(queue, 10u, 3u);
	CHECK(f.server->stats().emptyBundles == 1u);
}

TEST_CASE("SimNetServer.AHostWithNoteDeliveredSeesOneCallPerMessageRecordAndNoneForEvents", "[NetTransport][SimNetServer]")
{
	TimeConfig config = testTimeConfig();
	RecordingEndpoint endpoint;
	DeliveryHost host;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	transport->markEndpointStarted();
	SimNetServer<MockGame, DeliveryHost, FakeTransport> server(host, *transport, config, size_t{4096});
	transport->inbox().pushEvent(InboundKind::Connected, kConnA, ogsimnet::kNoReceiveSeconds);
	transport->inbox().pushEvent(InboundKind::Connected, kConnB, ogsimnet::kNoReceiveSeconds);
	server.dispatchFrame();
	CHECK(host.deliveries.empty());
	REQUIRE(server.addCharacter<SimA>(kSimA0, 0u, kConnA, 0u, "a") != nullptr);

	host.tick = 100;
	std::vector<std::byte> bad = inputSet({{kSimA0, {{101u, 1}}}});
	bad[0] = std::byte{0x7E};
	REQUIRE(transport->inbox().pushMessage(kConnA, SimLane::SimInput, 1.25, inputSet({{kSimA0, {{101u, 1}}}})));
	REQUIRE(transport->inbox().pushMessage(kConnB, SimLane::SimInput, 1.5, inputSet({{kSimA0, {{102u, 2}}}})));
	REQUIRE(transport->inbox().pushMessage(kConnA, SimLane::SimInput, 1.75, bad));
	REQUIRE(transport->inbox().pushMessage(kConnA, SimLane::Control, 2.0, inputSet({{kSimA0, {{103u, 3}}}})));
	transport->inbox().pushEvent(InboundKind::Disconnected, kConnB, ogsimnet::kNoReceiveSeconds);
	server.dispatchFrame();

	CHECK(host.deliveries
		== std::vector<std::pair<SimLane, double>>{{SimLane::SimInput, 1.25}, {SimLane::SimInput, 1.5}, {SimLane::SimInput, 1.75},
			{SimLane::Control, 2.0}});
	CHECK(server.stats().messages == 4u);
	CHECK(server.stats().badDecode == 2u);
	CHECK(server.stats().refusedNotOwner == 1u);
	CHECK(server.coordinator().claimCount() == 1u);
}

TEST_CASE("SimNetServer.TheWindowLineReportsEveryTenSecondsOfHostTime", "[NetTransport][SimNetServer]")
{
	Fixture f;
	f.connect(kConnA);
	f.connect(kConnB);
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.host.now = 50.0;
	f.server->dispatchFrame();
	f.server->flushFrame();

	f.push(kConnB, inputSet({{kSimA0, {{100u, 1}}}}));
	f.push(kConnB, inputSet({{kSimUnknown, {{100u, 1}}}}));
	std::vector<std::byte> bad = inputSet({{kSimA0, {{100u, 1}}}});
	bad[0] = std::byte{0x7E};
	f.push(kConnA, bad);
	f.server->dispatchFrame();
	f.host.now = 59.9;
	f.server->flushFrame();
	CHECK(f.linesWith("[SimNet.Window]").empty());

	f.host.now = 60.0;
	f.server->flushFrame();
	REQUIRE(f.linesWith("[SimNet.Window]").size() == 1u);
	CHECK(f.linesWith("[SimNet.Window]")[0]
		== "[Warning][SimNet.Window] role=server drains=2 msgs=3 maxDepth=3 drops=0 tooLarge=0 badDecode=1 refusedNotOwner=1 "
		   "unknownSim=1 statesSent=0 relaysSent=0 overBudget=0 quotaRefused=0 controlHighWater=0 quotaDisconnects=0 "
		   "eventsRefused=0 eventHighWater=2");

	f.host.now = 70.0;
	f.server->flushFrame();
	REQUIRE(f.linesWith("[SimNet.Window]").size() == 2u);
	CHECK(f.linesWith("[SimNet.Window]")[1]
		== "[Warning][SimNet.Window] role=server drains=0 msgs=0 maxDepth=0 drops=0 tooLarge=0 badDecode=0 refusedNotOwner=0 "
		   "unknownSim=0 statesSent=0 relaysSent=0 overBudget=0 quotaRefused=0 controlHighWater=0 quotaDisconnects=0 "
		   "eventsRefused=0 eventHighWater=2");
}

TEST_CASE("SimNetServer.AnOverBudgetFrameIsCountedAndStillSentInFull", "[NetTransport][SimNetServer]")
{
	TimeConfig config = testTimeConfig();
	RecordingEndpoint endpoint;
	FakeHost host;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	const size_t oneCorrection = simEnvelope::kHeaderBytes + 2u + Shape::kCorrectionPayloadBytes;
	FakeServer server(host, *transport, config, oneCorrection);
	transport->inbox().pushEvent(InboundKind::Connected, kConnA, ogsimnet::kNoReceiveSeconds);
	server.addCharacter<SimA>(kSimA0, 0u, kConnA, 0u, "a");
	server.addCharacter<SimB>(kSimB1, 1u, kConnA, 1u, "b");
	server.dispatchFrame();

	server.channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(1), 10u, 9u);
	server.flushFrame();
	CHECK(server.stats().statesSent == 1u);
	CHECK(server.stats().overBudget == 0u);

	server.channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(2), 11u, 10u);
	server.channel<SimB>(kSimB1)->getSyncedCorrectionStateBuffer().write(stateWithX(3), 11u, 10u);
	server.flushFrame();
	CHECK(server.stats().statesSent == 3u);
	CHECK(server.stats().overBudget == 1u);
}

namespace
{
struct RecordingPolicy
{
	std::vector<std::pair<SimConnectionId, std::vector<ogsimnet::CorrectionCandidate>>>* seen = nullptr;

	void select(SimConnectionId receiver, std::span<const ogsimnet::CorrectionCandidate> candidates, size_t, ogsimnet::CorrectionSelection& out)
	{
		seen->emplace_back(receiver, std::vector<ogsimnet::CorrectionCandidate>(candidates.begin(), candidates.end()));
		for (size_t i = 0u; i < candidates.size(); ++i)
			out.select(i);
	}
};
} // namespace

TEST_CASE("SimNetServer.CandidatesCarryEachReceiversAgeAndOnlyAttachedConnectionsAreAsked", "[NetTransport][SimNetServer]")
{
	TimeConfig config = testTimeConfig();
	RecordingEndpoint endpoint;
	FakeHost host;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	std::vector<std::pair<SimConnectionId, std::vector<CorrectionCandidate>>> seen;
	SimNetServer<MockGame, FakeHost, FakeTransport, RecordingPolicy> server(host, *transport, config, size_t{4096}, RecordingPolicy{&seen});
	transport->inbox().pushEvent(InboundKind::Connected, kConnA, kNoReceiveSeconds);
	server.dispatchFrame();
	server.addCharacter<SimA>(kSimA0, 0u, kConnA, 0u, "a");

	server.channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(1), 10u, 9u);
	server.flushFrame();
	transport->inbox().pushEvent(InboundKind::Connected, kConnB, kNoReceiveSeconds);
	server.dispatchFrame();
	server.channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(2), 14u, 13u);
	server.flushFrame();
	transport->inbox().pushEvent(InboundKind::Disconnected, kConnA, kNoReceiveSeconds);
	server.dispatchFrame();
	server.channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(3), 15u, 14u);
	server.flushFrame();

	REQUIRE(seen.size() == 4u);
	CHECK(seen[0].first == kConnA);
	CHECK(seen[1].first == kConnA);
	CHECK(seen[2].first == kConnB);
	CHECK(seen[3].first == kConnB);
	for (const auto& [receiver, candidates] : seen)
	{
		REQUIRE(candidates.size() == 1u);
		CHECK(candidates[0].simId == kSimA0);
		CHECK(candidates[0].bytes == simEnvelope::kHeaderBytes + 2u + Shape::kCorrectionPayloadBytes);
		CHECK(!candidates[0].substituted);
	}
	CHECK(seen[0].second[0].ageTicks == kNeverSentAgeTicks);
	CHECK(seen[1].second[0].ageTicks == 4u);
	CHECK(seen[2].second[0].ageTicks == kNeverSentAgeTicks);
	CHECK(seen[3].second[0].ageTicks == 1u);
	CHECK(server.stats().statesSent == 4u);
}

namespace
{
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using RealEndpoint = ognet::NetEndpoint<TestHost, VirtualBackend>;
using RealTransport = ogsimnet::SimNetTransport<RealEndpoint, 2048>;
using RealServer = ogsimnet::SimNetServer<MockGame, FakeHost, RealTransport>;

constexpr std::uint8_t kServerHostIndex = 1;
constexpr std::uint8_t kFirstClientHostIndex = 10;
constexpr std::uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr double kStepSeconds = 0.001;
constexpr int kMaxSteps = 5000;

struct Got
{
	InboundKind kind = InboundKind::Message;
	SimLane lane = SimLane::Control;
	std::vector<std::uint8_t> bytes;
};

struct Peer
{
	Peer(VirtualNetwork& network, VirtualBackend& backendIn, std::uint64_t seed)
		: transport(std::make_unique<RealTransport>()), host(network.clock(), seed), backend(backendIn), endpoint(host, backend)
	{
	}

	void start(ognet::Role role)
	{
		transport->installHandlers(endpoint);
		endpoint.setConnectionHandler(transport->connectionHandler());
		ognet::EndpointConfig config;
		config.role = role;
		if (role == ognet::Role::Server)
			config.bind.port = kServerPort;
		config.buildIdentity.fill(0x5A);
		REQUIRE(endpoint.start(config));
		transport->markEndpointStarted();
	}

	void collect()
	{
		transport->inbox().drain([&](const InboundSimRecord& record) {
			if (record.kind == InboundKind::Connected && serverConnection == kInvalidSimConnection)
			{
				serverConnection = record.conn;
				transport->setServerConnection(record.conn);
			}
			records.push_back(Got{record.kind, record.lane, toU8(record.payload())});
		});
	}

	std::vector<DecodedSimMessage> messagesOf(SimMessageKind kind, std::vector<std::vector<std::byte>>& storage) const
	{
		std::vector<DecodedSimMessage> out;
		for (const Got& got : records)
		{
			if (got.kind != InboundKind::Message)
				continue;
			storage.emplace_back();
			for (const std::uint8_t b : got.bytes)
				storage.back().push_back(std::byte{b});
			const DecodedSimMessage message = simEnvelope::decode<Shape>(got.lane, SimDirection::ServerToClient, storage.back());
			if (message.refusal == DecodeRefusal::None && message.kind == kind)
				out.push_back(message);
		}
		return out;
	}

	std::vector<SimMessageKind> controlKinds() const
	{
		std::vector<SimMessageKind> out;
		std::vector<std::byte> storage;
		for (const Got& got : records)
		{
			if (got.kind == InboundKind::Message && got.lane == SimLane::Control)
				out.push_back(decodeDown(SimLane::Control, got.bytes, storage).kind);
		}
		return out;
	}

	std::unique_ptr<RealTransport> transport;
	TestHost host;
	VirtualBackend& backend;
	RealEndpoint endpoint;
	std::vector<Got> records;
	SimConnectionId serverConnection = kInvalidSimConnection;
};

struct Network
{
	Network()
	{
		network.setNow(kStartSeconds);
		serverPeer = std::make_unique<Peer>(network, network.addHost(kServerHostIndex), 100);
		serverPeer->start(ognet::Role::Server);
		server = std::make_unique<RealServer>(host, *serverPeer->transport, config, size_t{4096});
	}

	void join()
	{
		const size_t index = clients.size();
		clients.push_back(std::make_unique<Peer>(network, network.addHost(static_cast<std::uint8_t>(kFirstClientHostIndex + index)), 200 + index));
		Peer& client = *clients.back();
		client.start(ognet::Role::Client);
		client.endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		REQUIRE(stepUntil([&] { return client.serverConnection != kInvalidSimConnection && server->attachedConnections().size() == index + 1u; }));
		serverIds.push_back(server->attachedConnections().back());
	}

	template <class DoneT>
	bool stepUntil(DoneT done)
	{
		for (int i = 0; i < kMaxSteps; ++i)
		{
			if (done())
				return true;
			pass();
		}
		return done();
	}

	void pass()
	{
		++step;
		network.setNow(kStartSeconds + static_cast<double>(step) * kStepSeconds);
		serverPeer->endpoint.serviceUntil(network.nowSeconds());
		server->dispatchFrame();
		server->flushFrame();
		for (auto& client : clients)
		{
			client->endpoint.serviceUntil(network.nowSeconds());
			client->transport->flushFrame();
			client->collect();
		}
	}

	void settle()
	{
		for (int i = 0; i < 50; ++i)
			pass();
	}

	void clearClients()
	{
		for (auto& client : clients)
			client->records.clear();
	}

	TimeConfig config = testTimeConfig();
	FakeHost host;
	VirtualNetwork network;
	std::unique_ptr<Peer> serverPeer;
	std::unique_ptr<RealServer> server;
	std::vector<std::unique_ptr<Peer>> clients;
	std::vector<SimConnectionId> serverIds;
	int step = 0;
};
} // namespace

TEST_CASE("SimNetServer.ASnapshotAndTheFloorReachANewConnectionBeforeAnyDelta", "[NetTransport][SimNetServer]")
{
	Network net;
	net.join();
	net.server->publishFloor(6u);
	net.server->addCharacter<SimA>(kSimA0, 0u, net.serverIds[0], 0u, "a");
	net.settle();

	net.join();
	net.server->addCharacter<SimB>(kSimB1, 1u, net.serverIds[1], 0u, "b");
	net.server->publishFloor(7u);
	net.settle();

	const Peer& late = *net.clients[1];
	CHECK(late.controlKinds() == std::vector<SimMessageKind>{SimMessageKind::Roster, SimMessageKind::RelayFloor, SimMessageKind::Roster,
		SimMessageKind::RelayFloor});
	std::vector<std::vector<std::byte>> storage;
	const std::vector<DecodedSimMessage> roster = late.messagesOf(SimMessageKind::Roster, storage);
	REQUIRE(roster.size() == 2u);
	const DecodedRoster snapshot = rosterCodec::decode(roster[0].body);
	CHECK(snapshot.kind == RosterKind::Snapshot);
	REQUIRE(snapshot.view.count == 1u);
	CHECK(snapshot.view.entries[0].simId == kSimA0);
	CHECK(!snapshot.view.entries[0].isLocal());
	const DecodedRoster add = rosterCodec::decode(roster[1].body);
	CHECK(add.kind == RosterKind::Add);
	CHECK(add.revision == snapshot.revision + 1u);
	REQUIRE(add.view.count == 1u);
	CHECK(add.view.entries[0].simId == kSimB1);
	CHECK(add.view.entries[0].localIndex == 0u);
	const std::vector<DecodedSimMessage> floors = late.messagesOf(SimMessageKind::RelayFloor, storage);
	REQUIRE(floors.size() == 2u);
	CHECK(floors[0].value == 6u);
	CHECK(floors[1].value == 7u);

	CHECK(net.clients[0]->controlKinds() == std::vector<SimMessageKind>{SimMessageKind::Roster, SimMessageKind::RelayFloor,
		SimMessageKind::Roster, SimMessageKind::Roster, SimMessageKind::RelayFloor});
}

TEST_CASE("SimNetServer.AFramesRelayGoesToEveryConnectionButTheOwner", "[NetTransport][SimNetServer]")
{
	Network net;
	net.join();
	net.join();
	net.join();
	net.server->addCharacter<SimA>(kSimA0, 0u, net.serverIds[0], 0u, "a");
	net.settle();
	net.clearClients();

	net.host.tick = 100;
	const InputBundleBuffer bundle = bundleOf({{101u, 42}});
	auto adapter = net.clients[0]->transport->channelTransport(kSimA0);
	adapter.sendInputBundle(bundle.payload());
	adapter.endInputSends();
	REQUIRE(net.stepUntil([&] { return net.server->coordinator().claimCount() == 1u; }));
	net.settle();

	std::vector<std::vector<std::byte>> storage;
	CHECK(net.clients[0]->messagesOf(SimMessageKind::RelayRing, storage).empty());
	for (size_t i = 1u; i < 3u; ++i)
	{
		const std::vector<DecodedSimMessage> rings = net.clients[i]->messagesOf(SimMessageKind::RelayRing, storage);
		REQUIRE(rings.size() == 1u);
		CHECK(rings[0].simId == kSimA0);
		CHECK(toU8(rings[0].body) == toU8(net.server->stager(kSimA0)->published().payload()));
	}
	CHECK(net.server->stats().relaysSent == 1u);
	CHECK(net.host.handed == std::vector<std::pair<std::uint8_t, std::uint32_t>>{{kSimA0, 101u}});
	REQUIRE(net.host.serverReceived.size() == 1u);
	CHECK(std::get<0>(net.host.serverReceived[0]) == kSimA0);
	CHECK(std::get<1>(net.host.serverReceived[0]) == 101u);
	CHECK(std::get<2>(net.host.serverReceived[0]) > kStartSeconds);
}

TEST_CASE("SimNetServer.CorrectionsGoToEveryConnectionOncePerDirtyWrite", "[NetTransport][SimNetServer]")
{
	Network net;
	net.join();
	net.join();
	net.join();
	net.server->addCharacter<SimA>(kSimA0, 0u, net.serverIds[0], 0u, "a");
	net.server->addCharacter<SimB>(kSimB1, 1u, kInvalidSimConnection, 0u, "local");
	net.settle();
	net.clearClients();

	net.server->channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(11), 50u, 49u);
	net.settle();
	net.server->channel<SimB>(kSimB1)->getSyncedCorrectionStateBuffer().write(stateWithX(12), 51u, 50u);
	net.server->channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(13), 51u, 50u);
	const std::vector<std::uint8_t> lastA = correctionEnvelope(kSimA0, std::as_const(*net.server->channel<SimA>(kSimA0)).getSyncedCorrectionStateBuffer());
	net.settle();

	for (const auto& client : net.clients)
	{
		std::vector<std::vector<std::byte>> storage;
		const std::vector<DecodedSimMessage> states = client->messagesOf(SimMessageKind::CorrectionState, storage);
		REQUIRE(states.size() == 3u);
		CHECK(states[0].simId == kSimA0);
		CHECK(states[0].tick == 50u);
		CHECK(states[0].corrVersion == correctionStateBuffer::kWireFormatVersion);
		CHECK(states[1].simId == kSimA0);
		CHECK(states[1].tick == 51u);
		CHECK(states[2].simId == kSimB1);
		CHECK(states[2].tick == 51u);
		std::vector<std::uint8_t> raw;
		for (const Got& got : client->records)
		{
			if (got.kind == InboundKind::Message && got.lane == SimLane::SimState && got.bytes.size() > 2u && got.bytes[2] == kSimA0
				&& got.bytes[4] == 51u)
				raw = got.bytes;
		}
		CHECK(raw == lastA);
	}
	CHECK(net.server->stats().statesSent == 9u);
	CHECK(net.server->stats().overBudget == 0u);
}

TEST_CASE("SimNetServer.TheClockIsSentOncePerChangedTick", "[NetTransport][SimNetServer]")
{
	Network net;
	net.join();
	net.join();
	net.settle();
	net.clearClients();

	net.server->publishClock(10u);
	net.settle();
	net.server->publishClock(10u);
	net.settle();
	net.server->publishClock(11u);
	net.server->publishClock(12u);
	net.settle();

	for (const auto& client : net.clients)
	{
		std::vector<std::vector<std::byte>> storage;
		const std::vector<DecodedSimMessage> clocks = client->messagesOf(SimMessageKind::ClockPublish, storage);
		REQUIRE(clocks.size() == 2u);
		CHECK(clocks[0].tick == 10u);
		CHECK(clocks[1].tick == 12u);
	}
	CHECK(net.server->stats().clocksSent == 2u);
}

namespace
{
struct SelectsNineTimes
{
	void select(SimConnectionId, std::span<const ogsimnet::CorrectionCandidate>, size_t, ogsimnet::CorrectionSelection& out) const
	{
		for (size_t i = 0u; i < 9u; ++i)
			out.select(0u);
	}
};

struct SelectsAMissingCandidate
{
	void select(SimConnectionId, std::span<const ogsimnet::CorrectionCandidate> candidates, size_t, ogsimnet::CorrectionSelection& out) const
	{
		out.select(candidates.size());
	}
};

template <class PolicyT>
void flushOneCorrectionWith()
{
	TimeConfig config = testTimeConfig();
	RecordingEndpoint endpoint;
	FakeHost host;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	ogsimnet::SimNetServer<MockGame, FakeHost, FakeTransport, PolicyT> server(host, *transport, config, size_t{4096});
	transport->inbox().pushEvent(InboundKind::Connected, kConnA, ogsimnet::kNoReceiveSeconds);
	server.dispatchFrame();
	server.template addCharacter<SimA>(kSimA0, 0u, kConnA, 0u, "a");
	server.template channel<SimA>(kSimA0)->getSyncedCorrectionStateBuffer().write(stateWithX(1), 1u, 0u);
	server.flushFrame();
}
} // namespace

TEST_CASE("SimNetServer.Death.APolicySelectingMoreThanEveryCandidateFiresTheCheck", "[.][OGSimDeath]")
{
	flushOneCorrectionWith<SelectsNineTimes>();
	FAIL("CorrectionSelection::select past kMaxCorrectionCandidates did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetServer.Death.APolicySelectingAMissingCandidateFiresTheCheck", "[.][OGSimDeath]")
{
	flushOneCorrectionWith<SelectsAMissingCandidate>();
	FAIL("SimNetServer::flushCorrections with a selected index past the candidates did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetServer.Death.RemovingAnUnknownCharacterFiresTheCheck", "[.][OGSimDeath]")
{
	Fixture f;
	f.server->removeCharacter(kSimA0, RosterRemoveReason::Left);
	FAIL("SimNetServer::removeCharacter of a sim id with no character did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetServer.Death.ARemovedSimIdIsNotRefusedAsLiveAndReachesTheRostersNeverReusedCheck", "[.][OGSimDeath]")
{
	Fixture f;
	f.addRecorded<SimA>(kSimA0, 0u, kConnA, 0u);
	f.server->removeCharacter(kSimA0, RosterRemoveReason::Left);
	const auto* const again = f.server->addCharacter<SimA>(kSimA0, 1u, kConnA, 0u, "again");
	CHECK(f.server->stats().addRefused == 0u);
	FAIL((again == nullptr ? "SimNetServer::addCharacter refused a removed sim id as live"
						   : "RosterAuthority::add's never-reused OG_CHECK did NOT fire for a removed sim id"));
}
