// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationTimeContext.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simNetLifecycleQueue
{
struct LqPosition
{
	std::int32_t x = 0;
};

struct LqInput
{
	std::int32_t move = 0;
};
} // namespace simNetLifecycleQueue

template <>
struct SerializableFields<simNetLifecycleQueue::LqPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetLifecycleQueue::LqPosition, x)); }
};

template <>
struct SerializableFields<simNetLifecycleQueue::LqInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetLifecycleQueue::LqInput, move)); }
};

namespace simNetLifecycleQueue
{
using LqState = SimulationComposite<LqPosition>;

class LqAllState
{
public:
	const LqState& getState() const { return m_state; }
	LqState& editState() { return m_state; }

private:
	LqState m_state;
};

struct Hooks
{
	std::function<void()> moved;
	std::function<void()> destroyed;
};

inline Hooks& hooks()
{
	static Hooks instance;
	return instance;
}

struct HooksReset
{
	~HooksReset()
	{
		hooks().moved = nullptr;
		hooks().destroyed = nullptr;
	}
};

struct LqSim
{
	using StateType = LqState;
	using InputType = LqInput;

	LqAllState m_allState;
	LqAllState m_vizState;

	LqSim() = default;
	LqSim(const LqSim&) = default;
	LqSim& operator=(const LqSim&) = default;
	LqSim& operator=(LqSim&&) = default;

	LqSim(LqSim&& other) noexcept
		: m_allState(other.m_allState)
		, m_vizState(other.m_vizState)
	{
		if (hooks().moved)
			hooks().moved();
	}

	~LqSim()
	{
		if (hooks().destroyed)
			hooks().destroyed();
	}

	const LqAllState& getAllState() const { return m_allState; }
	LqAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const LqAllState& getVizState() const { return m_vizState; }
};

struct LqGame
{
	using Simulatables = SimulatableList<LqSim>;
};

struct NullEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void setLaneHandler(ognet::Lane, ognet::LaneHandler) {}
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId) {}
};

using LqTransport = ogsimnet::SimNetTransport<NullEndpoint, 256>;

struct LqClientHost
{
	std::vector<std::string> lines;
	std::function<void(std::uint8_t)> removed;

	double nowSeconds() { return 0.0; }
	double serverRttSeconds() { return 0.05; }
	void onTimingInfo(std::uint32_t, double) {}
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t simId)
	{
		if (removed)
			removed(simId);
	}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char* line) { lines.emplace_back(line); }
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}

	std::size_t countWith(const char* tag) const
	{
		std::size_t count = 0u;
		for (const std::string& line : lines)
			count += line.find(tag) != std::string::npos ? 1u : 0u;
		return count;
	}
};

struct LqServerHost
{
	std::vector<std::string> lines;

	double nowSeconds() { return 0.0; }
	double rttMs(SimConnectionId) { return 20.0; }
	std::int32_t receptionTick() { return 100; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
};

static_assert(ogsimnet::SimNetClientHost<LqClientHost>);
static_assert(ogsimnet::SimNetServerHost<LqServerHost>);

using LqClient = ogsimnet::SimNetClient<LqGame, LqClientHost, LqTransport>;
using LqServer = ogsimnet::SimNetServer<LqGame, LqServerHost, LqTransport>;
using LqChannel = LqClient::Channel;

static_assert(std::is_same_v<LqChannel, LqServer::Channel<LqSim>>);
} // namespace simNetLifecycleQueue

template <>
struct SimulatableOwnerTraits<simNetLifecycleQueue::LqSim>
{
	using PredictionOwnerType = simNetLifecycleQueue::LqChannel;
	using AuthorityOwnerType = simNetLifecycleQueue::LqChannel;
};

namespace simNetLifecycleQueue
{
using ogsimnet::RosterEntry;

constexpr SimConnectionId kServerConn = 11u;
constexpr SimConnectionId kClientConn = 21u;
constexpr std::uint8_t kLocal = 3u;
constexpr std::uint8_t kRemote = 7u;
constexpr std::uint8_t kUnlisted = 9u;
constexpr float kDeltaSeconds = 1.0f / 60.0f;

using Bytes = std::vector<std::byte>;
using Provider = LqClient::InputProvider;

template <class WriteFn>
Bytes envelope(WriteFn&& write)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = write(std::span<std::byte>(out));
	REQUIRE(size != 0u);
	return Bytes(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(size));
}

Bytes snapshotMsg(std::uint16_t revision, const std::vector<RosterEntry>& entries)
{
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeSnapshot(revision, std::span<const RosterEntry>(entries));
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

Bytes removeMsg(std::uint16_t revision, std::uint8_t simId)
{
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeRemove(revision, simId, ogsimnet::RosterRemoveReason::Left);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

Bytes correctionMsg(std::uint8_t simId, std::uint32_t tick)
{
	LqState state;
	state.edit<LqPosition>().x = static_cast<std::int32_t>(tick);
	CorrectionStateBuffer buffer;
	buffer.write(state, tick, tick - 1u);
	return envelope([&](std::span<std::byte> out) {
		return simEnvelope::writeCorrectionState(out, simId, correctionStateBuffer::kWireFormatVersion, buffer.payload());
	});
}

Bytes ringMsg(std::uint8_t simId, std::uint32_t firstTick, std::uint32_t lastTick)
{
	RelayRingBuffer ring;
	for (std::uint32_t tick = firstTick; tick <= lastTick; ++tick)
		ring.writeLatest<LqInput>(tick, 2u, LqInput{static_cast<std::int32_t>(tick)}, 8);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRelayRing(out, simId, ring.payload()); });
}

Bytes inputSetMsg(std::uint8_t simId, std::uint32_t captureTick)
{
	InputBundleBuffer bundle;
	bundle.appendSlot<LqInput>(captureTick, LqInput{5});
	std::array<std::byte, kMaxSimMessageBytes> out{};
	std::size_t used = simEnvelope::writeInputBundleSetHeader(out);
	used = simEnvelope::appendInputBundle(out, used, simId, bundle.payload());
	REQUIRE(used != 0u);
	return Bytes(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(used));
}

SimulationTimeStep normalStep(std::uint32_t tick) { return SimulationTimeStep(tick, false, StepKind::Normal, kDeltaSeconds); }

Provider constantProvider(std::int32_t value)
{
	return [value](const SimulationTimeStep&, const LocalInputCache<LqInput>&) { return LqInput{value}; };
}

struct SimParts
{
	SimParts()
	{
		auto sink = [this](const char* line) { simLines.emplace_back(line); };
		resolution.setLogger(sink);
		netSync.setLogger(sink);
		resolution.template setNeutralInput<LqSim>(LqInput{-1});
	}

	bool hasInputFor(const ResolvedInputs<LqSim>& inputs, unsigned int id) const
	{
		const auto& map = std::get<std::unordered_map<unsigned int, LqInput>>(inputs);
		return map.find(id) != map.end();
	}

	std::size_t noCacheLines() const
	{
		std::size_t count = 0u;
		for (const std::string& line : simLines)
			count += line.find("NOCACHE") != std::string::npos ? 1u : 0u;
		return count;
	}

	std::uint32_t landings() const { return netSync.getDiagnostics().correctionLandingProbe().sampleCount(); }

	bool fullyRegistered(unsigned int id) const
	{
		return storage.template has<LqSim>(id) && reconciliation.template findCorrectionCache<LqSim>(id) != nullptr;
	}

	bool fullyGone(unsigned int id) const
	{
		return !storage.template has<LqSim>(id) && reconciliation.template findCorrectionCache<LqSim>(id) == nullptr
			&& !resolution.template isLocallyControlled<LqSim>(id) && resolution.template findRemoteInputCache<LqSim>(id) == nullptr
			&& resolution.getDiagnostics().template localInputCache<LqSim>(id) == nullptr;
	}

	std::vector<std::string> simLines;
	SimulationObjectStorage<LqSim> storage;
	SimulationReconciliation<LqSim> reconciliation{storage};
	SimulationInputResolution<LqSim> resolution{storage, reconciliation};
	SimulationNetSync<LqSim> netSync{storage, reconciliation, resolution};
};

struct ClientRig : SimParts
{
	explicit ClientRig(bool bind = true)
		: transport(std::make_unique<LqTransport>(ogsimnet::SimNetConfig{}))
	{
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		client = std::make_unique<LqClient>(host, *transport);
		if (bind)
			client->bindLifecycle(storage, reconciliation, resolution, netSync);
		transport->inbox().pushEvent(InboundKind::Connected, kServerConn, ogsimnet::kNoReceiveSeconds);
		push(SimLane::Control, snapshotMsg(1u, {RosterEntry{kLocal, 0u, 0u}, RosterEntry{kRemote, 1u, ogsimnet::kRosterNotLocal}}));
		client->drain();
	}

	void push(SimLane lane, const Bytes& bytes) { REQUIRE(transport->inbox().pushMessage(kServerConn, lane, 1.0, bytes)); }
	void correction(std::uint8_t simId, std::uint32_t tick) { push(SimLane::SimState, correctionMsg(simId, tick)); }
	void ring(std::uint8_t simId, std::uint32_t first, std::uint32_t last) { push(SimLane::SimRelay, ringMsg(simId, first, last)); }

	void registerLocal(std::int32_t value = 3) { client->registerCharacter(kLocal, LqSim{}, constantProvider(value)); }
	void registerRemote() { client->registerCharacter(kRemote, LqSim{}, nullptr); }

	LqClientHost host;
	NullEndpoint endpoint;
	std::unique_ptr<LqTransport> transport;
	std::unique_ptr<LqClient> client;
};

struct ServerRig : SimParts
{
	ServerRig()
		: transport(std::make_unique<LqTransport>())
	{
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		server = std::make_unique<LqServer>(host, *transport, config, std::size_t{4096});
		server->bindLifecycle(storage, reconciliation, resolution, netSync);
		transport->inbox().pushEvent(InboundKind::Connected, kClientConn, ogsimnet::kNoReceiveSeconds);
		server->drain();
	}

	void registerOwned(std::uint8_t simId, std::function<void(LqChannel*)> applied = nullptr)
	{
		server->registerCharacter<LqSim>(simId, simId, kClientConn, 0u, "lq" + std::to_string(simId), LqSim{},
			[applied = std::move(applied)](LqChannel* channel) {
				if (applied)
					applied(channel);
			});
	}

	TimeConfig config;
	LqServerHost host;
	NullEndpoint endpoint;
	std::unique_ptr<LqTransport> transport;
	std::unique_ptr<LqServer> server;
};
} // namespace simNetLifecycleQueue

using namespace simNetLifecycleQueue;

TEST_CASE("SimNetLifecycleQueue.ARegisterEnqueuedMidBatchIsVisibleFromTheNextDrain", "[Lifecycle]")
{
	SECTION("one thread: the enqueue between a drain and that step's collect")
	{
		ClientRig rig;
		rig.client->drain();
		rig.registerLocal(4);
		CHECK_FALSE(rig.storage.has<LqSim>(kLocal));
		CHECK_FALSE(rig.hasInputFor(rig.resolution.collectInputAll(normalStep(100u)), kLocal));
		CHECK_FALSE(rig.client->lifecycleRegistered(kLocal));

		rig.client->drain();
		CHECK(rig.client->lifecycleRegistered(kLocal));
		CHECK(rig.fullyRegistered(kLocal));
		const ResolvedInputs<LqSim> next = rig.resolution.collectInputAll(normalStep(101u));
		REQUIRE(rig.hasInputFor(next, kLocal));
		CHECK(std::get<std::unordered_map<unsigned int, LqInput>>(next).at(kLocal).move == 4);
		CHECK(rig.client->lifecycleStats().registersApplied == 1u);
		CHECK(rig.noCacheLines() == 0u);
	}
	SECTION("a step task on its own thread, the game thread enqueuing while it runs")
	{
		ClientRig rig;
		std::atomic<std::uint32_t> stepsDone{0u};
		std::atomic<bool> enqueued{false};
		std::thread::id appliedOn;
		std::uint32_t firstVisible = 0u;
		std::uint32_t visibleSteps = 0u;
		std::uint32_t gapsAfterVisible = 0u;
		std::uint32_t steps = 0u;

		std::thread stepTask([&] {
			for (std::uint32_t step = 0u; step < 1000000u && visibleSteps < 50u; ++step)
			{
				rig.client->drain();
				const bool visible = rig.hasInputFor(rig.resolution.collectInputAll(normalStep(1000u + step)), kLocal);
				if (visible && visibleSteps == 0u)
					firstVisible = step;
				if (visible)
					++visibleSteps;
				else if (visibleSteps > 0u)
					++gapsAfterVisible;
				steps = step + 1u;
				stepsDone.store(step + 1u, std::memory_order_release);
				if (!enqueued.load(std::memory_order_acquire))
					std::this_thread::yield();
			}
		});

		while (stepsDone.load(std::memory_order_acquire) < 100u)
			std::this_thread::yield();
		const std::uint32_t doneAtEnqueue = stepsDone.load(std::memory_order_acquire);
		rig.client->registerCharacter(kLocal, LqSim{}, constantProvider(6),
			[&appliedOn](LqChannel* channel) {
				if (channel != nullptr)
					appliedOn = std::this_thread::get_id();
			});
		enqueued.store(true, std::memory_order_release);
		const std::thread::id gameThread = std::this_thread::get_id();
		const std::thread::id stepThread = stepTask.get_id();
		stepTask.join();

		UNSCOPED_INFO("doneAtEnqueue=" << doneAtEnqueue << " firstVisible=" << firstVisible << " steps=" << steps);
		REQUIRE(visibleSteps == 50u);
		CHECK(firstVisible >= doneAtEnqueue);
		CHECK(gapsAfterVisible == 0u);
		CHECK(appliedOn == stepThread);
		CHECK(appliedOn != gameThread);
		CHECK(rig.noCacheLines() == 0u);
	}
}

TEST_CASE("SimNetLifecycleQueue.ARegisterThenTheFirstArrivalInTheSameDrainReachesTheRegisteredCharacter", "[Lifecycle]")
{
	ClientRig rig;
	rig.registerRemote();
	rig.correction(kRemote, 60u);
	rig.ring(kRemote, 58u, 60u);
	rig.client->drain();

	CHECK(rig.fullyRegistered(kRemote));
	CHECK(rig.landings() == 1u);
	CHECK(rig.host.countWith("[ReceiveCorrectionState] id=7 tick=60") == 1u);
	const auto* store = rig.resolution.findRemoteInputCache<LqSim>(kRemote);
	REQUIRE(store != nullptr);
	CHECK(store->findLatest().captureTick == 60u);
	CHECK(rig.client->stats().unknownSim == 0u);
}

TEST_CASE("SimNetLifecycleQueue.AnUnregisterThenALateArrivalIsACountedUnknownSimId", "[Lifecycle]")
{
	SECTION("the roster still lists the character")
	{
		ClientRig rig;
		rig.registerRemote();
		rig.client->drain();
		rig.correction(kRemote, 50u);
		rig.client->drain();
		REQUIRE(rig.landings() == 1u);

		std::vector<LqChannel*> applied;
		rig.client->unregisterCharacter(kRemote, [&applied](LqChannel* channel) { applied.push_back(channel); });
		CHECK(rig.fullyRegistered(kRemote));
		rig.client->drain();
		CHECK(rig.fullyGone(kRemote));
		REQUIRE(applied.size() == 1u);
		CHECK(applied[0] == rig.client->channel(kRemote));
		CHECK(rig.client->channel(kRemote) != nullptr);
		CHECK_FALSE(rig.client->lifecycleRegistered(kRemote));

		const std::uint64_t unknownBefore = rig.client->stats().unknownSim;
		rig.correction(kRemote, 51u);
		rig.ring(kRemote, 49u, 51u);
		rig.client->drain();
		CHECK(rig.client->stats().unknownSim == unknownBefore + 2u);
		CHECK(rig.landings() == 1u);
		CHECK(rig.fullyGone(kRemote));
		CHECK(rig.host.countWith("[ReceiveCorrectionState] id=7 tick=51") == 0u);

		rig.registerRemote();
		rig.client->drain();
		CHECK(rig.fullyRegistered(kRemote));
		rig.correction(kRemote, 52u);
		rig.client->drain();
		CHECK(rig.landings() == 2u);
		CHECK(rig.client->stats().unknownSim == unknownBefore + 2u);
	}
	SECTION("the roster removes the character before the unregister is applied")
	{
		ClientRig rig;
		rig.registerRemote();
		rig.client->drain();
		std::optional<bool> storageHadItAtTheHostEvent;
		rig.host.removed = [&](std::uint8_t simId) { storageHadItAtTheHostEvent = rig.storage.has<LqSim>(simId); };
		rig.push(SimLane::Control, removeMsg(2u, kRemote));
		rig.client->drain();
		REQUIRE(storageHadItAtTheHostEvent.has_value());
		CHECK_FALSE(*storageHadItAtTheHostEvent);
		CHECK(rig.fullyGone(kRemote));
		CHECK(rig.client->channel(kRemote) == nullptr);
		CHECK(rig.client->lifecycleStats().removalUnregisters == 1u);

		std::vector<LqChannel*> applied;
		rig.client->unregisterCharacter(kRemote, [&applied](LqChannel* channel) { applied.push_back(channel); });
		const std::uint64_t unknownBefore = rig.client->stats().unknownSim;
		rig.correction(kRemote, 53u);
		rig.client->drain();
		REQUIRE(applied.size() == 1u);
		CHECK(applied[0] == nullptr);
		CHECK(rig.client->lifecycleStats().unregistersIgnored == 1u);
		CHECK(rig.client->stats().unknownSim == unknownBefore + 1u);
		CHECK(rig.landings() == 0u);
	}
}

TEST_CASE("SimNetLifecycleQueue.PublishLastAndUnpublishFirstHoldThroughTheQueue", "[Lifecycle]")
{
	const HooksReset reset;
	ClientRig rig;
	rig.registerLocal(3);

	std::uint32_t moves = 0u;
	bool providerAtPublication = false;
	bool lineAtPublication = false;
	bool cacheAtPublication = false;
	hooks().moved = [&] {
		++moves;
		providerAtPublication = rig.resolution.isLocallyControlled<LqSim>(kLocal);
		lineAtPublication = rig.resolution.getDiagnostics().localInputCache<LqSim>(kLocal) != nullptr;
		cacheAtPublication = rig.reconciliation.findCorrectionCache<LqSim>(kLocal) != nullptr;
	};
	rig.client->drain();
	hooks().moved = nullptr;
	REQUIRE(moves == 1u);
	CHECK(providerAtPublication);
	CHECK(lineAtPublication);
	CHECK(cacheAtPublication);
	CHECK(rig.storage.has<LqSim>(kLocal));

	rig.client->unregisterCharacter(kLocal);
	std::uint32_t destroyed = 0u;
	bool providerAtUnpublication = false;
	bool lineAtUnpublication = false;
	bool cacheAtUnpublication = false;
	hooks().destroyed = [&] {
		++destroyed;
		providerAtUnpublication = rig.resolution.isLocallyControlled<LqSim>(kLocal);
		lineAtUnpublication = rig.resolution.getDiagnostics().localInputCache<LqSim>(kLocal) != nullptr;
		cacheAtUnpublication = rig.reconciliation.findCorrectionCache<LqSim>(kLocal) != nullptr;
	};
	rig.client->drain();
	hooks().destroyed = nullptr;
	REQUIRE(destroyed == 1u);
	CHECK(providerAtUnpublication);
	CHECK(lineAtUnpublication);
	CHECK(cacheAtUnpublication);
	CHECK(rig.fullyGone(kLocal));
	CHECK(rig.noCacheLines() == 0u);
}

TEST_CASE("SimNetLifecycleQueue.ARegistrationTheSessionCannotApplyIsRefusedCountedAndLogged", "[Lifecycle]")
{
	ClientRig rig;
	std::vector<LqChannel*> applied;
	auto record = [&applied](LqChannel* channel) { applied.push_back(channel); };
	rig.client->registerCharacter(kUnlisted, LqSim{}, nullptr, record);
	rig.client->registerCharacter(kLocal, LqSim{}, constantProvider(1), record);
	rig.client->registerCharacter(kLocal, LqSim{}, constantProvider(2), record);
	rig.client->drain();

	REQUIRE(applied.size() == 3u);
	CHECK(applied[0] == nullptr);
	CHECK(applied[1] == rig.client->channel(kLocal));
	CHECK(applied[2] == nullptr);
	CHECK(rig.client->lifecycleStats().registersRefused == 2u);
	CHECK(rig.client->lifecycleStats().registersApplied == 1u);
	CHECK(rig.host.countWith("[Warning][SimNet.Lifecycle] role=client simId=9 refused=noChannel") == 1u);
	CHECK(rig.host.countWith("[Warning][SimNet.Lifecycle] role=client simId=3 refused=registered") == 1u);
	CHECK_FALSE(rig.storage.has<LqSim>(kUnlisted));
	CHECK(std::get<std::unordered_map<unsigned int, LqInput>>(rig.resolution.collectInputAll(normalStep(200u))).at(kLocal).move == 1);
}

TEST_CASE("SimNetLifecycleQueue.TheServerAppliesQueuedRegistrationsAtTheTopOfItsDrain", "[Lifecycle]")
{
	SECTION("register, then unregister, then a late bundle from the owner")
	{
		ServerRig rig;
		LqChannel* registered = nullptr;
		rig.registerOwned(kRemote, [&registered](LqChannel* channel) { registered = channel; });
		CHECK(rig.server->roster().recordFor(kRemote) == nullptr);
		CHECK_FALSE(rig.storage.has<LqSim>(kRemote));

		rig.server->drain();
		REQUIRE(registered != nullptr);
		CHECK(registered == rig.server->channel<LqSim>(kRemote));
		CHECK(rig.server->roster().recordFor(kRemote) != nullptr);
		CHECK(rig.storage.has<LqSim>(kRemote));
		CHECK(rig.server->lifecycleRegistered(kRemote));

		REQUIRE(rig.transport->inbox().pushMessage(kClientConn, SimLane::SimInput, 1.0, inputSetMsg(kRemote, 100u)));
		rig.server->drain();
		CHECK(rig.server->stats().unknownSim == 0u);

		rig.server->unregisterCharacter<LqSim>(kRemote, ogsimnet::RosterRemoveReason::Left);
		REQUIRE(rig.transport->inbox().pushMessage(kClientConn, SimLane::SimInput, 1.0, inputSetMsg(kRemote, 101u)));
		rig.server->drain();
		CHECK(rig.fullyGone(kRemote));
		CHECK(rig.server->channel<LqSim>(kRemote) == nullptr);
		CHECK(rig.server->roster().recordFor(kRemote) == nullptr);
		CHECK(rig.server->stats().unknownSim == 1u);
		CHECK(rig.server->lifecycleStats().unregistersApplied == 1u);
	}
	SECTION("removeCharacter of a queue-registered character unregisters it before the channel goes")
	{
		ServerRig rig;
		rig.registerOwned(kRemote);
		rig.server->drain();
		REQUIRE(rig.storage.has<LqSim>(kRemote));
		rig.server->removeCharacter(kRemote, ogsimnet::RosterRemoveReason::Left);
		CHECK(rig.fullyGone(kRemote));
		CHECK(rig.server->lifecycleStats().removalUnregisters == 1u);

		rig.server->unregisterCharacter<LqSim>(kRemote, ogsimnet::RosterRemoveReason::Left);
		rig.server->drain();
		CHECK(rig.server->lifecycleStats().unregistersIgnored == 1u);
	}
	SECTION("a refused add reaches the hook as nullptr and registers nothing")
	{
		ServerRig rig;
		rig.registerOwned(kRemote);
		std::vector<LqChannel*> applied;
		rig.registerOwned(kRemote, [&applied](LqChannel* channel) { applied.push_back(channel); });
		rig.server->drain();
		REQUIRE(applied.size() == 1u);
		CHECK(applied[0] == nullptr);
		CHECK(rig.server->lifecycleStats().registersRefused == 1u);
		CHECK(rig.server->stats().addRefused == 1u);
		CHECK(rig.storage.has<LqSim>(kRemote));
	}
}

TEST_CASE("SimNetLifecycleQueue.Death.ARegisterBeforeTheBindFiresTheCheck", "[.][OGSimDeath]")
{
	ClientRig rig(false);
	rig.registerLocal();
}

TEST_CASE("SimNetLifecycleQueue.Death.ASecondBindFiresTheCheck", "[.][OGSimDeath]")
{
	ClientRig rig;
	rig.client->bindLifecycle(rig.storage, rig.reconciliation, rig.resolution, rig.netSync);
}
