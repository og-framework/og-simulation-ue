// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetChannel.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationQueues.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimulation/SimulationNetSync.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace simNetChannelTest
{
struct ChanPosition
{
	std::int32_t x = 0;
};

struct ChanInput
{
	std::int32_t move  = 0;
	std::uint8_t flags = 0u;
};
} // namespace simNetChannelTest

template <>
struct SerializableFields<simNetChannelTest::ChanPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetChannelTest::ChanPosition, x)); }
};

template <>
struct SerializableFields<simNetChannelTest::ChanInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(simNetChannelTest::ChanInput, move), SIM_MEMBER(simNetChannelTest::ChanInput, flags));
	}
};

namespace simNetChannelTest
{
using ChanState = SimulationComposite<ChanPosition>;

class ChanAllState
{
public:
	const ChanState& getState() const { return m_state; }
	ChanState&       editState() { return m_state; }

private:
	ChanState m_state;
};

template <int Tag>
struct ChanSim
{
	using StateType = ChanState;
	using InputType = ChanInput;

	ChanAllState m_allState;
	ChanAllState m_vizState;

	const ChanAllState& getAllState() const { return m_allState; }
	ChanAllState&       editAllState() { return m_allState; }
	void                updateVizState() { m_vizState = m_allState; }
	const ChanAllState& getVizState() const { return m_vizState; }
};

using MockOwnedSim    = ChanSim<0>;
using ChannelOwnedSim = ChanSim<1>;

static_assert(SimulatableState<MockOwnedSim> && SimulatableState<ChannelOwnedSim>);

struct AppendedBundle
{
	std::uint8_t              simId = 0u;
	std::vector<std::uint8_t> bytes;
};

struct BundleRecorder
{
	std::vector<AppendedBundle> appended;
	int                         endInputSendsCount = 0;
};

std::vector<std::uint8_t> toU8(std::span<const std::byte> bytes)
{
	std::vector<std::uint8_t> out;
	for (const std::byte b : bytes)
		out.push_back(std::to_integer<std::uint8_t>(b));
	return out;
}

std::vector<std::byte> toBytes(const std::vector<std::uint8_t>& bytes)
{
	std::vector<std::byte> out;
	for (const std::uint8_t b : bytes)
		out.push_back(std::byte{b});
	return out;
}

class FakeChannelTransport
{
public:
	FakeChannelTransport(BundleRecorder* recorder, std::uint8_t simId) : m_recorder(recorder), m_simId(simId) {}

	void sendInputBundle(std::span<const std::byte> bundle) { m_recorder->appended.push_back(AppendedBundle{m_simId, toU8(bundle)}); }
	void endInputSends() { ++m_recorder->endInputSendsCount; }
	std::uint8_t simId() const { return m_simId; }

private:
	BundleRecorder* m_recorder;
	std::uint8_t    m_simId;
};

static_assert(ClientSimTransport<FakeChannelTransport>);
static_assert(std::is_copy_constructible_v<FakeChannelTransport>);

template <class SimT>
using Channel = ogsimnet::SimNetChannel<SimT, FakeChannelTransport>;

static_assert(PredictionSyncedBufferOwnerConcept<Channel<ChannelOwnedSim>, ChanState, ChanInput>,
	"SimNetChannel must satisfy NetSync's prediction owner concept, as UESimulatableOwnerBinding does");
static_assert(AuthoritySyncedBufferOwnerConcept<Channel<ChannelOwnedSim>, ChanState, ChanInput>,
	"SimNetChannel must satisfy NetSync's authority owner concept, as UESimulatableOwnerBinding does");
static_assert(std::is_same_v<Channel<ChannelOwnedSim>::SyncedCorrectionBufferType, CorrectionStateBuffer>);
static_assert(std::is_same_v<Channel<ChannelOwnedSim>::RelayedInputRingType, RelayRingBuffer>);
static_assert(!std::is_copy_constructible_v<Channel<ChannelOwnedSim>> && !std::is_move_constructible_v<Channel<ChannelOwnedSim>>
		&& !std::is_copy_assignable_v<Channel<ChannelOwnedSim>> && !std::is_move_assignable_v<Channel<ChannelOwnedSim>>,
	"SimNetChannel is pinned in place: NetSync keeps a raw pointer to a registered owner");

struct MockCorrectionBuffer
{
	std::uint32_t lastTick               = 0u;
	std::uint32_t lastAppliedCaptureTick = kNoInputCaptureTick;
	ChanState     lastState;
	int           writeCount = 0;

	void write(const ChanState& state, std::uint32_t tick, std::uint32_t appliedCaptureTick)
	{
		lastState              = state;
		lastTick               = tick;
		lastAppliedCaptureTick = appliedCaptureTick;
		++writeCount;
	}

	void write(const ChanState& state, std::uint32_t tick) { write(state, tick, kNoInputCaptureTick); }

	std::uint32_t readInto(ChanState& out) const
	{
		out = lastState;
		return lastTick;
	}

	std::uint32_t getAppliedCaptureTick() const { return lastAppliedCaptureTick; }
};

struct MockRelayedInputRing
{
	std::vector<std::uint8_t> bytes;

	std::int32_t bundleByteNum() const { return static_cast<std::int32_t>(bytes.size()); }
	void bundleAddZeroedBytes(std::int32_t count) { bytes.resize(bytes.size() + static_cast<std::size_t>(count), 0u); }

	template <typename T>
	void writeToBuffer(std::uint32_t off, const T& value)
	{
		std::memcpy(bytes.data() + off, &value, sizeof(T));
	}

	template <typename T>
	T readFromBuffer(std::uint32_t off) const
	{
		T value;
		std::memcpy(&value, bytes.data() + off, sizeof(T));
		return value;
	}
};

struct SentSlot
{
	std::uint32_t tick = 0u;
	ChanInput     input;
};

struct MockPredictionOwner
{
	using SyncedCorrectionBufferType = MockCorrectionBuffer;
	using RelayedInputRingType       = MockRelayedInputRing;

	std::function<void(const MockCorrectionBuffer&)> onCorrectionStateReceived;
	std::function<void(const MockRelayedInputRing&)> onRelayedInputReceived;
	MockCorrectionBuffer                             receivedState;
	MockRelayedInputRing                             relayedInputRing;
	std::vector<std::vector<SentSlot>>               sends;

	void setOnCorrectionStateReceivedCallback(std::function<void(const MockCorrectionBuffer&)> fn) { onCorrectionStateReceived = std::move(fn); }
	void clearOnCorrectionStateReceivedCallback() { onCorrectionStateReceived = nullptr; }
	void setOnRelayedInputReceivedCallback(std::function<void(const MockRelayedInputRing&)> fn) { onRelayedInputReceived = std::move(fn); }
	void clearOnRelayedInputReceivedCallback() { onRelayedInputReceived = nullptr; }
	const MockRelayedInputRing& getRelayedInputRing() const { return relayedInputRing; }

	void sendLocalInputToAuthority(const PendingInputQueue<ChanInput>& queue, std::uint32_t currentTick, std::uint32_t redundancyDepth)
	{
		std::vector<SentSlot> slots;
		queue.forEachRecent(currentTick, static_cast<std::size_t>(redundancyDepth),
			[&slots](std::uint32_t tick, const ChanInput& input) { slots.push_back(SentSlot{tick, input}); });
		sends.push_back(std::move(slots));
	}
};

struct MockAuthorityOwner
{
	MockCorrectionBuffer                                 stateBuf;
	std::function<void(std::uint32_t, const ChanInput&)> onRemoteMoveReceived;

	MockCorrectionBuffer& getSyncedCorrectionStateBuffer() { return stateBuf; }
	void setOnRemoteMoveReceivedCallback(std::function<void(std::uint32_t, const ChanInput&)> fn) { onRemoteMoveReceived = std::move(fn); }
	void clearOnRemoteMoveReceivedCallback() { onRemoteMoveReceived = nullptr; }
};

static_assert(PredictionSyncedBufferOwnerConcept<MockPredictionOwner, ChanState, ChanInput>);
static_assert(AuthoritySyncedBufferOwnerConcept<MockAuthorityOwner, ChanState, ChanInput>);
} // namespace simNetChannelTest

template <>
struct SimulatableOwnerTraits<simNetChannelTest::MockOwnedSim>
{
	using PredictionOwnerType = simNetChannelTest::MockPredictionOwner;
	using AuthorityOwnerType  = simNetChannelTest::MockAuthorityOwner;
};

template <>
struct SimulatableOwnerTraits<simNetChannelTest::ChannelOwnedSim>
{
	using PredictionOwnerType = simNetChannelTest::Channel<simNetChannelTest::ChannelOwnedSim>;
	using AuthorityOwnerType  = simNetChannelTest::Channel<simNetChannelTest::ChannelOwnedSim>;
};

static_assert(sizeof(AuthorityWriter<simNetChannelTest::ChannelOwnedSim>) == sizeof(void*),
	"AuthorityWriter over a SimNetChannel owner must stay one pointer (SimulationNetSync.h's writer-struct rule)");
static_assert(sizeof(LocalInputSender<simNetChannelTest::ChannelOwnedSim>) == sizeof(void*),
	"LocalInputSender over a SimNetChannel owner must stay one pointer (SimulationNetSync.h's writer-struct rule)");

namespace simNetChannelTest
{
constexpr unsigned int  kLocalId           = 7u;
constexpr unsigned int  kRemoteId          = 9u;
constexpr std::uint32_t kFirstTick         = 100u;
constexpr std::uint32_t kTickCount         = 6u;
constexpr std::uint32_t kRedundancyDepth   = 3u;
constexpr std::int32_t  kRollbackWindow    = 30;
constexpr std::int32_t  kRotationK         = 1;
constexpr float         kDeltaSeconds      = 1.0f / 60.0f;
constexpr std::int32_t  kRelayDepth        = 4;

ChanInput inputFor(std::uint32_t tick, unsigned int id)
{
	return ChanInput{static_cast<std::int32_t>(tick * 3u + id), static_cast<std::uint8_t>(tick & 0x0Fu)};
}

std::vector<std::uint8_t> relayRingBytes(std::uint32_t firstTick, std::uint32_t lastTick)
{
	RelayRingBuffer ring;
	for (std::uint32_t tick = firstTick; tick <= lastTick; ++tick)
		ring.writeLatest<ChanInput>(tick, 2u, inputFor(tick, kRemoteId), kRelayDepth);
	return toU8(ring.payload());
}

std::string slotsText(const std::vector<SentSlot>& slots)
{
	std::string text;
	for (const SentSlot& slot : slots)
		text += " " + std::to_string(slot.tick) + ":" + std::to_string(slot.input.move) + "/" + std::to_string(slot.input.flags);
	return text;
}

std::vector<SentSlot> decodeBundle(const std::vector<std::uint8_t>& bytes)
{
	InputBundleBuffer bundle;
	const std::vector<std::byte> wire = toBytes(bytes);
	REQUIRE(bundle.assignFromWire(wire));
	std::vector<SentSlot> slots;
	bundle.forEachSlot<ChanInput>([&slots](std::uint32_t tick, const ChanInput& input) { slots.push_back(SentSlot{tick, input}); });
	return slots;
}

template <class SimT>
struct Peer
{
	SimulationObjectStorage<SimT>   storage;
	SimulationReconciliation<SimT>  reconciliation{storage};
	SimulationInputResolution<SimT> resolution{storage, reconciliation};
	SimulationNetSync<SimT>         netSync{storage, reconciliation, resolution};

	Peer(std::vector<std::string>& trace, const char* role)
	{
		const std::string prefix = std::string(role) + " ";
		auto              logger = [&trace, prefix](const char* line) { trace.push_back(prefix + line); };
		netSync.setLogger(logger);
		reconciliation.setLogger(logger);
		resolution.setLogger(logger);
		resolution.template setNeutralInput<SimT>(ChanInput{});
	}
};

struct MockOwners
{
	using Sim = MockOwnedSim;

	MockPredictionOwner serverPrediction[2];
	MockAuthorityOwner  serverAuthority[2];
	MockPredictionOwner clientOwner[2];
	int                 seenServerWrites[2] = {0, 0};

	explicit MockOwners(BundleRecorder&) {}

	static int slot(unsigned int id) { return id == kLocalId ? 0 : 1; }

	MockPredictionOwner& serverPredictionOwner(unsigned int id) { return serverPrediction[slot(id)]; }
	MockAuthorityOwner&  serverAuthorityOwner(unsigned int id) { return serverAuthority[slot(id)]; }
	MockPredictionOwner& clientPredictionOwner(unsigned int id) { return clientOwner[slot(id)]; }

	void deliverRemoteMove(unsigned int id, std::uint32_t tick, const ChanInput& input)
	{
		MockAuthorityOwner& owner = serverAuthority[slot(id)];
		if (owner.onRemoteMoveReceived)
			owner.onRemoteMoveReceived(tick, input);
	}

	bool takeServerWrite(unsigned int id, std::uint32_t& tick, std::uint32_t& applied, std::int32_t& x)
	{
		const MockCorrectionBuffer& buffer = serverAuthority[slot(id)].stateBuf;
		if (buffer.writeCount == seenServerWrites[slot(id)])
			return false;
		seenServerWrites[slot(id)] = buffer.writeCount;
		tick                       = buffer.lastTick;
		applied                    = buffer.lastAppliedCaptureTick;
		x                          = buffer.lastState.get<ChanPosition>().x;
		return true;
	}

	void wireCorrection(unsigned int id)
	{
		MockPredictionOwner& owner = clientOwner[slot(id)];
		owner.receivedState        = serverAuthority[slot(id)].stateBuf;
		if (owner.onCorrectionStateReceived)
			owner.onCorrectionStateReceived(owner.receivedState);
	}

	void wireRing(unsigned int id, const std::vector<std::uint8_t>& bytes)
	{
		MockPredictionOwner& owner = clientOwner[slot(id)];
		owner.relayedInputRing.bytes = bytes;
		if (owner.onRelayedInputReceived)
			owner.onRelayedInputReceived(owner.relayedInputRing);
	}

	std::vector<std::vector<SentSlot>> takeClientSends()
	{
		std::vector<std::vector<SentSlot>> sends;
		sends.swap(clientOwner[slot(kLocalId)].sends);
		return sends;
	}
};

struct ChannelOwners
{
	using Sim = ChannelOwnedSim;

	BundleRecorder&                 recorder;
	BundleRecorder                  serverRecorder;
	std::unique_ptr<Channel<Sim>>   server[2];
	std::unique_ptr<Channel<Sim>>   client[2];
	std::size_t                     seenAppends = 0u;

	explicit ChannelOwners(BundleRecorder& clientRecorder) : recorder(clientRecorder)
	{
		for (unsigned int id : {kLocalId, kRemoteId})
		{
			const auto simId = static_cast<std::uint8_t>(id);
			server[slot(id)] = std::make_unique<Channel<Sim>>(FakeChannelTransport(&serverRecorder, simId), simId, "server-" + std::to_string(id));
			client[slot(id)] = std::make_unique<Channel<Sim>>(FakeChannelTransport(&recorder, simId), simId, "client-" + std::to_string(id));
		}
	}

	static int slot(unsigned int id) { return id == kLocalId ? 0 : 1; }

	Channel<Sim>& serverPredictionOwner(unsigned int id) { return *server[slot(id)]; }
	Channel<Sim>& serverAuthorityOwner(unsigned int id) { return *server[slot(id)]; }
	Channel<Sim>& clientPredictionOwner(unsigned int id) { return *client[slot(id)]; }

	void deliverRemoteMove(unsigned int id, std::uint32_t tick, const ChanInput& input)
	{
		server[slot(id)]->deliverDelayedRemoteInput(tick, input);
	}

	bool takeServerWrite(unsigned int id, std::uint32_t& tick, std::uint32_t& applied, std::int32_t& x)
	{
		Channel<Sim>& channel = *server[slot(id)];
		if (!channel.takeDirty())
			return false;
		ChanState state;
		tick    = std::as_const(channel).getSyncedCorrectionStateBuffer().readInto(state);
		applied = std::as_const(channel).getSyncedCorrectionStateBuffer().getAppliedCaptureTick();
		x       = state.get<ChanPosition>().x;
		return true;
	}

	void wireCorrection(unsigned int id)
	{
		REQUIRE(client[slot(id)]->storeCorrection(std::as_const(*server[slot(id)]).getSyncedCorrectionStateBuffer().payload()));
	}

	void wireRing(unsigned int id, const std::vector<std::uint8_t>& bytes)
	{
		const std::vector<std::byte> wire = toBytes(bytes);
		REQUIRE(client[slot(id)]->storeRing(wire));
	}

	std::vector<std::vector<SentSlot>> takeClientSends()
	{
		std::vector<std::vector<SentSlot>> sends;
		for (; seenAppends < recorder.appended.size(); ++seenAppends)
		{
			const AppendedBundle& appended = recorder.appended[seenAppends];
			REQUIRE(appended.simId == static_cast<std::uint8_t>(kLocalId));
			sends.push_back(decodeBundle(appended.bytes));
		}
		return sends;
	}
};

SimulationTimeStep normalStep(std::uint32_t tick) { return SimulationTimeStep(tick, false, StepKind::Normal, kDeltaSeconds); }

template <class OwnersT>
std::vector<std::string> runNetSyncScript()
{
	using SimT = typename OwnersT::Sim;
	std::vector<std::string> trace;
	BundleRecorder           clientRecorder;
	OwnersT                  owners(clientRecorder);
	Peer<SimT>               server(trace, "server");
	Peer<SimT>               client(trace, "client");

	owners.wireRing(kRemoteId, relayRingBytes(kFirstTick - 3u, kFirstTick - 1u));

	for (unsigned int id : {kLocalId, kRemoteId})
		registerSimulatable<SimT>(server.storage, server.reconciliation, server.resolution, server.netSync, id, SimT{},
			owners.serverPredictionOwner(id), owners.serverAuthorityOwner(id));

	registerSimulatable<SimT>(client.storage, client.reconciliation, client.resolution, client.netSync, kLocalId, SimT{},
		owners.clientPredictionOwner(kLocalId),
		[](const SimulationTimeStep&, const LocalInputCache<ChanInput>&) { return ChanInput{}; });
	registerSimulatable<SimT>(client.storage, client.reconciliation, client.resolution, client.netSync, kRemoteId, SimT{},
		owners.clientPredictionOwner(kRemoteId));

	for (std::uint32_t tick = kFirstTick; tick < kFirstTick + kTickCount; ++tick)
	{
		PendingInputQueue<ChanInput>* queue = client.resolution.template findPendingInputQueue<SimT>(kLocalId);
		REQUIRE(queue != nullptr);
		queue->enqueue(tick, inputFor(tick, kLocalId));
		client.netSync.sendLocalInputToAuthorityAll(tick, kRedundancyDepth, 0);

		server.netSync.setAuthorityGuardContext(tick, kRollbackWindow);
		for (const std::vector<SentSlot>& send : owners.takeClientSends())
		{
			trace.push_back("wire send id=" + std::to_string(kLocalId) + slotsText(send));
			for (const SentSlot& slot : send)
				owners.deliverRemoteMove(kLocalId, slot.tick, slot.input);
		}

		for (unsigned int id : {kLocalId, kRemoteId})
			server.storage.template get<SimT>(id).editAllState().editState().template edit<ChanPosition>().x =
				static_cast<std::int32_t>(tick * 10u + id);
		server.netSync.sendCorrectionAll(normalStep(tick), kRotationK);

		for (unsigned int id : {kLocalId, kRemoteId})
		{
			std::uint32_t writtenTick = 0u;
			std::uint32_t applied     = 0u;
			std::int32_t  x           = 0;
			if (owners.takeServerWrite(id, writtenTick, applied, x))
			{
				trace.push_back("wire write id=" + std::to_string(id) + " tick=" + std::to_string(writtenTick)
					+ " applied=" + std::to_string(applied) + " x=" + std::to_string(x));
				owners.wireCorrection(id);
			}
		}

		owners.wireRing(kRemoteId, relayRingBytes(tick > kFirstTick + 1u ? tick - 2u : kFirstTick, tick));
	}

	unregisterSimulatable<SimT>(client.storage, client.reconciliation, client.resolution, client.netSync, kRemoteId,
		&owners.clientPredictionOwner(kRemoteId));
	owners.wireCorrection(kRemoteId);
	owners.wireRing(kRemoteId, relayRingBytes(kFirstTick, kFirstTick + 2u));

	unregisterSimulatable<SimT>(server.storage, server.reconciliation, server.resolution, server.netSync, kLocalId,
		&owners.serverPredictionOwner(kLocalId), &owners.serverAuthorityOwner(kLocalId));
	owners.deliverRemoteMove(kLocalId, kFirstTick + kTickCount, inputFor(kFirstTick + kTickCount, kLocalId));
	trace.push_back("end");
	return trace;
}

std::size_t countContaining(const std::vector<std::string>& trace, const std::string& needle)
{
	std::size_t count = 0u;
	for (const std::string& line : trace)
		if (line.find(needle) != std::string::npos)
			++count;
	return count;
}

using TestChannel = Channel<ChannelOwnedSim>;

void fillQueue(PendingInputQueue<ChanInput>& queue, std::uint32_t first, std::uint32_t last)
{
	for (std::uint32_t tick = first; tick <= last; ++tick)
		queue.enqueue(tick, inputFor(tick, kLocalId));
}
} // namespace simNetChannelTest

using namespace simNetChannelTest;

TEST_CASE("SimNetChannel.ANetSyncScriptGivesTheMockOwnersSequence", "[NetTransport][SimNetChannel]")
{
	const std::vector<std::string> mockTrace    = runNetSyncScript<MockOwners>();
	const std::vector<std::string> channelTrace = runNetSyncScript<ChannelOwners>();

	CHECK(countContaining(mockTrace, "wire send id=7") == kTickCount);
	CHECK(countContaining(mockTrace, "wire write id=") == kTickCount);
	CHECK(countContaining(mockTrace, "client [InjectCorrectionState]") == kTickCount);
	CHECK(countContaining(mockTrace, "server [ReceiveLocalInput] id=7") > 0u);
	CHECK(countContaining(mockTrace, "client [Verbose][RelayProbe.Ingest] id=9") > 0u);

	std::size_t firstDifference = 0u;
	while (firstDifference < mockTrace.size() && firstDifference < channelTrace.size()
		&& channelTrace[firstDifference] == mockTrace[firstDifference])
		++firstDifference;
	INFO("first difference at line " << firstDifference);
	CHECK((firstDifference < channelTrace.size() ? channelTrace[firstDifference] : std::string("<end>"))
		== (firstDifference < mockTrace.size() ? mockTrace[firstDifference] : std::string("<end>")));
	CHECK(channelTrace.size() == mockTrace.size());
	CHECK(channelTrace == mockTrace);
}

TEST_CASE("SimNetChannel.OnlyTheMutableCorrectionAccessorMarksDirty", "[NetTransport][SimNetChannel]")
{
	BundleRecorder recorder;
	TestChannel    channel(FakeChannelTransport(&recorder, 7u), 7u, "dirty");
	CHECK(channel.simId() == 7u);
	CHECK(channel.debugName() == "dirty");
	CHECK_FALSE(channel.isDirty());

	(void)std::as_const(channel).getSyncedCorrectionStateBuffer();
	CHECK_FALSE(channel.isDirty());

	ChanState state;
	state.edit<ChanPosition>().x = 41;
	channel.getSyncedCorrectionStateBuffer().write(state, 500u, 498u);
	CHECK(channel.isDirty());
	CHECK(channel.takeDirty());
	CHECK_FALSE(channel.isDirty());
	CHECK_FALSE(channel.takeDirty());

	TestChannel receiver(FakeChannelTransport(&recorder, 7u), 7u, "receiver");
	int         deliveries = 0;
	receiver.setOnCorrectionStateReceivedCallback([&deliveries](const CorrectionStateBuffer&) { ++deliveries; });
	REQUIRE(receiver.storeCorrection(std::as_const(channel).getSyncedCorrectionStateBuffer().payload()));
	CHECK(deliveries == 1);
	CHECK_FALSE(receiver.isDirty());
	ChanState decoded;
	CHECK(std::as_const(receiver).getSyncedCorrectionStateBuffer().readInto(decoded) == 500u);
	CHECK(std::as_const(receiver).getSyncedCorrectionStateBuffer().getAppliedCaptureTick() == 498u);
	CHECK(decoded.get<ChanPosition>().x == 41);
	CHECK(toU8(std::as_const(receiver).getSyncedCorrectionStateBuffer().payload())
		== toU8(std::as_const(channel).getSyncedCorrectionStateBuffer().payload()));
}

TEST_CASE("SimNetChannel.InboundDoorsAssignThenCallAndRefuseOversize", "[NetTransport][SimNetChannel]")
{
	BundleRecorder recorder;
	TestChannel    channel(FakeChannelTransport(&recorder, 9u), 9u, "inbound");

	const std::vector<std::uint8_t> ring = relayRingBytes(10u, 12u);
	const std::vector<std::byte>    wire = toBytes(ring);
	REQUIRE(channel.storeRing(wire));
	CHECK(toU8(channel.getRelayedInputRing().payload()) == ring);

	std::vector<std::vector<std::uint8_t>> rings;
	channel.setOnRelayedInputReceivedCallback([&rings](const RelayRingBuffer& r) { rings.push_back(toU8(r.payload())); });
	CHECK(rings.empty());
	REQUIRE(channel.storeRing(wire));
	REQUIRE(rings.size() == 1u);
	CHECK(rings.front() == ring);

	const std::vector<std::byte> oversizeRing(RelayRingBuffer::kMaxWireBytes + 1u, std::byte{0x5A});
	CHECK_FALSE(channel.storeRing(oversizeRing));
	CHECK(rings.size() == 1u);
	CHECK(toU8(channel.getRelayedInputRing().payload()) == ring);

	int corrections = 0;
	channel.setOnCorrectionStateReceivedCallback([&corrections](const CorrectionStateBuffer&) { ++corrections; });
	const std::vector<std::byte> oversizeCorrection(CorrectionStateBuffer::kBufferBytes + 1u, std::byte{0x5A});
	CHECK_FALSE(channel.storeCorrection(oversizeCorrection));
	CHECK(corrections == 0);
	CHECK_FALSE(channel.isDirty());

	channel.clearOnRelayedInputReceivedCallback();
	channel.clearOnCorrectionStateReceivedCallback();
	REQUIRE(channel.storeRing(wire));
	CHECK(rings.size() == 1u);

	std::vector<std::uint32_t> moves;
	channel.deliverDelayedRemoteInput(1u, ChanInput{});
	channel.setOnRemoteMoveReceivedCallback([&moves](std::uint32_t tick, const ChanInput& input)
		{ moves.push_back(tick * 1000u + static_cast<std::uint32_t>(input.move)); });
	channel.deliverDelayedRemoteInput(2u, ChanInput{5, 0u});
	channel.clearOnRemoteMoveReceivedCallback();
	channel.deliverDelayedRemoteInput(3u, ChanInput{6, 0u});
	CHECK(moves == std::vector<std::uint32_t>{2005u});
}

TEST_CASE("SimNetChannel.ASendHandsTheCodecBundleThenFiresHanded", "[NetTransport][SimNetChannel]")
{
	BundleRecorder recorder;
	TestChannel    channel(FakeChannelTransport(&recorder, 7u), 7u, "send");
	PendingInputQueue<ChanInput> queue;
	fillQueue(queue, 200u, 211u);

	std::vector<std::string> events;
	channel.setOnInputHandedCallback([&](const InputBundleBuffer& bundle)
		{
			events.push_back("handed after " + std::to_string(recorder.appended.size()) + " sends, "
				+ std::to_string(static_cast<unsigned int>(inputRedundancyBundle::slotCount(bundle))) + " slots");
		});

	channel.sendLocalInputToAuthority(queue, 211u, 3u);
	channel.sendLocalInputToAuthority(queue, 211u, 12u);
	channel.sendLocalInputToAuthority(queue, 205u, 3u);
	channel.clearOnInputHandedCallback();
	channel.sendLocalInputToAuthority(queue, 211u, 1u);

	REQUIRE(recorder.appended.size() == 4u);
	CHECK(recorder.endInputSendsCount == 0);
	CHECK(events == std::vector<std::string>{"handed after 1 sends, 3 slots", "handed after 2 sends, 8 slots", "handed after 3 sends, 0 slots"});

	for (const AppendedBundle& appended : recorder.appended)
		CHECK(appended.simId == 7u);

	std::vector<std::uint32_t> ticks;
	for (const SentSlot& slot : decodeBundle(recorder.appended[0].bytes))
		ticks.push_back(slot.tick);
	CHECK(ticks == std::vector<std::uint32_t>{209u, 210u, 211u});
	CHECK(decodeBundle(recorder.appended[1].bytes).size() == InputBundleBuffer::kMaxSlots);
	CHECK(decodeBundle(recorder.appended[2].bytes).empty());
	CHECK(decodeBundle(recorder.appended[3].bytes).size() == 1u);
	CHECK(!channel.isDirty());
}
