#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionRuntime-rationale.md · docs/SessionRuntime-guards.md

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "glm/vec3.hpp"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/Network/RelayReadProbe.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/SimulationLog.h"
#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGTickDriver/SimulationTickBackend.h"
#include "OGTickDriver/TickDriverConcepts.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetIdle.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimSession/SessionClock.h"
#include "OGSimSession/SessionDisplay.h"
#include "OGSimSession/SessionEngine.h"
#include "OGSimSession/SessionKnobs.h"
#include "OGSimSession/SessionLifecycle.h"
#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTickGame.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"

namespace ogsession
{
inline constexpr size_t kSessionDisplayWindowTicks = kLocalInputCacheCapacityTicks;

namespace sessionRuntimeLines
{
inline constexpr char kSimNetHost[] = "[Warning][SimNet.Host] role=%s immediate=%d rttSource=EnginePing driver=%s inbox=%u";
inline constexpr size_t kSimNetHostBytes = 128u;
inline constexpr char kBackend[] = "[Warning][SimHost.Backend] backend=%s fingerprint=%.*s";
inline constexpr char kManagerInitialized[] = "SimulationManager: adapters, integration layer, and manager initialized";
inline constexpr char kClientFrameWindow[] = "[Warning][ResimProbe.Frame] role=Client simTicks=%u frames=%u "
	"meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u";
inline constexpr char kClientFrameCadence[] = "[Warning][ResimProbe.Frame] role=Client cadence dFrame1=%u dFrame0=%u "
	"dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u";
inline constexpr char kServerFrameWindow[] = "[Warning][RelayProbe.Frame] role=Server simTicks=%u frames=%u "
	"meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u";
inline constexpr char kServerFrameCadence[] = "[Warning][RelayProbe.Frame] role=Server cadence dFrame1=%u dFrame0=%u "
	"dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u";

static_assert(kSimNetHostBytes >= sizeof(kSimNetHost) + sizeof("server") + sizeof("OGNet") + 10u,
	"kSimNetHostBytes must hold the [SimNet.Host] proof line with the longer role, the driver name and a 10-digit inbox "
	"size. Was UESimulationSession's kSimNetHostLineBytes (moved by og-session-runtime task 31)");

constexpr bool isAsciiName(const char* name)
{
	for (; *name != '\0'; ++name)
		if (static_cast<unsigned char>(*name) > 0x7Fu)
			return false;
	return true;
}
static_assert(isAsciiName("jolt") && !isAsciiName("O\xC3\xB1"),
	"isAsciiName self-check: an ASCII name passes, a UTF-8 multi-byte name does not");
} // namespace sessionRuntimeLines

template <typename StaticDataT>
struct SessionBodyRequest
{
	uint64_t                key = 0u;
	std::string             name;
	bool                    isAuthority = false;
	std::optional<uint32_t> replicatedSlot;
	const StaticDataT*      staticData = nullptr;
};

template <typename H, typename DriverT, typename InputProviderT, typename StaticDataT>
concept SessionRuntimeHost = requires(H& host, const H& constHost, DriverT& driver, const char* line,
	const RegisterRequest& request, InputProviderT&& provider, EntityKey key, bool isServer, void (*eachId)(uint32_t))
{
	{ host.onConfigure() }                                   -> std::same_as<void>;
	{ host.bindDiagnosticGates() }                           -> std::same_as<void>;
	{ host.afterPredictionConfigured() }                     -> std::same_as<void>;
	{ host.onSessionConfigured() }                           -> std::same_as<void>;
	{ host.beginFrames(driver) }                             -> std::same_as<void>;
	{ host.endFrames() }                                     -> std::same_as<void>;
	{ host.sampleInput() }                                   -> std::same_as<void>;
	{ host.onPostPhysics() }                                 -> std::same_as<void>;
	{ host.logHost(line) }                                   -> std::same_as<void>;
	{ host.makeBodyRequest(request) }                        -> std::same_as<SessionBodyRequest<StaticDataT>>;
	{ constHost.bodyKeyOf(key) }                             -> std::same_as<uint64_t>;
	{ host.wrapInputProvider(request, std::move(provider)) } -> std::same_as<InputProviderT>;
	{ host.teardownComponents() }                            -> std::same_as<void>;
	{ host.forgetComponents() }                              -> std::same_as<void>;
	{ host.onLinkStopping(isServer) }                        -> std::same_as<void>;
	{ constHost.forEachRegisteredId(eachId) }                -> std::same_as<void>;
};

namespace sessionRuntimeHostSelfCheck
{
struct Driver
{
};

struct InputProvider
{
};

struct StaticData
{
};

struct ConformingHost
{
	void onConfigure();
	void bindDiagnosticGates();
	void afterPredictionConfigured();
	void onSessionConfigured();
	void beginFrames(Driver& driver);
	void endFrames();
	void sampleInput();
	void onPostPhysics();
	void logHost(const char* line);
	SessionBodyRequest<StaticData> makeBodyRequest(const RegisterRequest& request);
	uint64_t bodyKeyOf(EntityKey key) const;
	InputProvider wrapInputProvider(const RegisterRequest& request, InputProvider&& provider);
	void teardownComponents();
	void forgetComponents();
	void onLinkStopping(bool isServer);
	template <typename FunctionT>
	void forEachRegisteredId(FunctionT&& function) const;
};

struct OnConfigureReturnsBool : ConformingHost
{
	bool onConfigure();
};

struct BindDiagnosticGatesReturnsBool : ConformingHost
{
	bool bindDiagnosticGates();
};

struct AfterPredictionConfiguredReturnsBool : ConformingHost
{
	bool afterPredictionConfigured();
};

struct OnSessionConfiguredReturnsBool : ConformingHost
{
	bool onSessionConfigured();
};

struct BeginFramesReturnsBool : ConformingHost
{
	bool beginFrames(Driver& driver);
};

struct EndFramesReturnsBool : ConformingHost
{
	bool endFrames();
};

struct SampleInputReturnsBool : ConformingHost
{
	bool sampleInput();
};

struct OnPostPhysicsReturnsBool : ConformingHost
{
	bool onPostPhysics();
};

struct LogHostReturnsBool : ConformingHost
{
	bool logHost(const char* line);
};

struct TeardownComponentsReturnsBool : ConformingHost
{
	bool teardownComponents();
};

struct ForgetComponentsReturnsBool : ConformingHost
{
	bool forgetComponents();
};

struct OnLinkStoppingReturnsBool : ConformingHost
{
	bool onLinkStopping(bool isServer);
};

struct ForEachRegisteredIdReturnsBool : ConformingHost
{
	template <typename FunctionT>
	bool forEachRegisteredId(FunctionT&& function) const;
};

struct MakeBodyRequestReturnsAConvertibleType : ConformingHost
{
	struct ConvertsToTheBodyRequest
	{
		operator SessionBodyRequest<StaticData>() const;
	};
	ConvertsToTheBodyRequest makeBodyRequest(const RegisterRequest& request);
};

struct BodyKeyOfReturnsA32BitKey : ConformingHost
{
	uint32_t bodyKeyOf(EntityKey key) const;
};

struct WrapInputProviderReturnsAConvertibleType : ConformingHost
{
	struct ConvertsToTheProvider
	{
		operator InputProvider() const;
	};
	ConvertsToTheProvider wrapInputProvider(const RegisterRequest& request, InputProvider&& provider);
};

struct BodyKeyOfOnAMutableHost : ConformingHost
{
	uint64_t bodyKeyOf(EntityKey key);
};

struct MakeBodyRequestTakingAMutableRequest : ConformingHost
{
	SessionBodyRequest<StaticData> makeBodyRequest(RegisterRequest& request);
};

template <typename H>
inline constexpr bool kModels = SessionRuntimeHost<H, Driver, InputProvider, StaticData>;

static_assert(kModels<ConformingHost>,
	"SessionRuntimeHost self-check: the reference shape (thirteen void hooks, the body request, the body key, the "
	"provider's wrap) must satisfy the concept");
static_assert(!kModels<OnConfigureReturnsBool>,
	"SessionRuntimeHost self-check: onConfigure() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<BindDiagnosticGatesReturnsBool>,
	"SessionRuntimeHost self-check: bindDiagnosticGates() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<AfterPredictionConfiguredReturnsBool>,
	"SessionRuntimeHost self-check: afterPredictionConfigured() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<OnSessionConfiguredReturnsBool>,
	"SessionRuntimeHost self-check: onSessionConfigured() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<BeginFramesReturnsBool>,
	"SessionRuntimeHost self-check: beginFrames(driver) is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<EndFramesReturnsBool>,
	"SessionRuntimeHost self-check: endFrames() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<SampleInputReturnsBool>,
	"SessionRuntimeHost self-check: sampleInput() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<OnPostPhysicsReturnsBool>,
	"SessionRuntimeHost self-check: onPostPhysics() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<LogHostReturnsBool>,
	"SessionRuntimeHost self-check: logHost(line) is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<TeardownComponentsReturnsBool>,
	"SessionRuntimeHost self-check: teardownComponents() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<ForgetComponentsReturnsBool>,
	"SessionRuntimeHost self-check: forgetComponents() is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<OnLinkStoppingReturnsBool>,
	"SessionRuntimeHost self-check: onLinkStopping(isServer) is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<ForEachRegisteredIdReturnsBool>,
	"SessionRuntimeHost self-check: forEachRegisteredId(function) is required and returns void; the runtime calls it as a statement and "
	"discards any result");
static_assert(!kModels<MakeBodyRequestReturnsAConvertibleType>,
	"SessionRuntimeHost self-check: makeBodyRequest(request) is required and returns the SessionBodyRequest the "
	"authority's bodies are made from, not a type that converts to it");
static_assert(!kModels<BodyKeyOfReturnsA32BitKey>,
	"SessionRuntimeHost self-check: bodyKeyOf(key) is required and returns the 64-bit body key (the Unreal adapter's is "
	"an address); a 32-bit key would truncate it");
static_assert(!kModels<WrapInputProviderReturnsAConvertibleType>,
	"SessionRuntimeHost self-check: wrapInputProvider(request, provider) is required and returns the provider type the "
	"client queues, not a type that converts to it");
static_assert(!kModels<BodyKeyOfOnAMutableHost>,
	"SessionRuntimeHost self-check: bodyKeyOf and forEachRegisteredId are callable on a const host; the runtime's "
	"readers do not change it");
static_assert(!kModels<MakeBodyRequestTakingAMutableRequest>,
	"SessionRuntimeHost self-check: makeBodyRequest and wrapInputProvider take the request by const reference; the "
	"runtime passes the lifecycle's const RegisterRequest through");
} // namespace sessionRuntimeHostSelfCheck

template <typename TraitsT>
class SessionRuntime
{
public:
	using Host          = typename TraitsT::Host;
	using Game          = typename TraitsT::Game;
	using Composition   = typename TraitsT::Composition;
	using EngineTraits  = typename TraitsT::EngineTraits;
	using StaticData    = typename TraitsT::StaticData;
	using Simulatable   = typename TraitsT::Simulatable;
	using PlayerInput   = typename TraitsT::PlayerInput;
	using NetConfig     = typename TraitsT::NetConfig;
	using Engine        = typename TraitsT::Engine;
	using ClockEngine   = typename TraitsT::ClockEngine;
	using Link          = typename TraitsT::Link;
	using Diagnostics   = typename TraitsT::Diagnostics;
	using InputProvider = typename Diagnostics::InputProvider;
	using WorldHost     = typename EngineTraits::template WorldHost<Simulatable>;

	using Simulatables    = typename Composition::Simulatables;
	using Storage         = apply_t<SimulationObjectStorage, Simulatables>;
	using Reconciliation  = apply_t<SimulationReconciliation, Simulatables>;
	using InputResolution = apply_t<SimulationInputResolution, Simulatables>;
	using NetSync         = apply_t<SimulationNetSync, Simulatables>;

	template <typename... SystemTs>
	using SystemsExecFor = SimulationSystemsExecutor<Simulatables, StaticData, SystemTs...>;

	template <typename... SimulatableTs>
	using IntegrationExecFor = SimulationIntegrationExecutor<StaticData, typename EngineTraits::BodyAdapter,
	                                                         typename EngineTraits::QueryAdapter, SimulatableTs...>;

	using SystemsExec      = apply_t<SystemsExecFor, typename Composition::Systems>;
	using IntegrationExec  = apply_t<IntegrationExecFor, Simulatables>;
	using ManagerType      = SimulationManager<IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec,
	                                           Storage, StaticData>;
	using SimNetServerHost = typename Link::ServerHost;
	using SimNetServer     = typename TraitsT::Server;
	using SimNetIdle       = typename TraitsT::Idle;
	using SimChannel       = typename SimNetServer::template Channel<Simulatable>;
	using Clock            = SessionClock<ManagerType, InputResolution, ClockEngine>;
	using DisplaySnapshot  = SimulationDisplaySnapshotT<Simulatable, kMaxSimulatableSlots, kSessionDisplayWindowTicks,
	                                                    kSessionDisplayWindowTicks>;
	using DisplayEntry     = typename DisplaySnapshot::Entry;
	using DisplayVizState  = typename DisplayEntry::VizState;
	using Display          = SessionDisplay<Game, DisplaySnapshot, ManagerType, Storage, InputResolution, Reconciliation>;
	using AuthorityPresentationSlice = typename Game::AuthorityPresentationSlice;
	using BodyRequest      = SessionBodyRequest<StaticData>;

	struct Worlds
	{
		explicit Worlds(Engine& engine) : host{ engine } {}

		WorldHost host;
	};

	struct TickHost
	{
		using Manager        = ManagerType;
		using Storage        = SessionRuntime::Storage;
		using RenderSnapshot = typename Composition::RenderSnapshot;

		SessionRuntime* runtime = nullptr;

		Manager& manager() { return *runtime->m_manager; }
		Storage& storage() { return runtime->m_storage; }

		const SessionThreadChecks& threads() const
		{
			OG_CHECK(runtime->m_threads.has_value(),
				"SessionRuntime::TickHost::threads: start builds the thread checks before it builds the world or starts the "
				"driver that calls the tick game. Was UESessionTickHost::threads' checkf (moved by og-session-runtime task 31)");
			return *runtime->m_threads;
		}

		Clock*        clock() { return runtime->m_clock.has_value() ? &*runtime->m_clock : nullptr; }
		Display*      display() { return runtime->m_display.has_value() ? &*runtime->m_display : nullptr; }
		auto*         netHooks() { return runtime->m_netHooks.has_value() ? &*runtime->m_netHooks : nullptr; }
		auto*         clockServer() { return runtime->m_clockServer; }
		Diagnostics&  diagnostics() { return runtime->m_diagnostics; }
		void          sampleInput() { runtime->m_host.sampleInput(); }
		void          onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps) { runtime->onFrameStepsDue_Step(firstUpcomingSimTick, numSteps); }
		void          onPostPhysics() { runtime->m_host.onPostPhysics(); }
	};

	using TickGame    = SessionTickGame<TickHost, Diagnostics>;
	using TickBackend = SimulationTickBackend<TickGame, WorldHost, Simulatable, Engine>;
	using Driver      = typename TickBackend::Driver;
	using Runner      = typename Engine::Runner;

	static_assert(SessionEngine<Engine>,
		"SessionRuntime: TraitsT::Engine is og-sim-session's SessionEngine: the tick driver's engine, the routed log sink and "
		"the runner factory (SimulationEngineUImpl on Unreal, og-session-runtime task 31)");
	SessionRuntime(Host& host, Diagnostics& diagnostics, Engine& frameEngine)
		: m_host(host)
		, m_diagnostics(diagnostics)
		, m_frameEngine(frameEngine)
		, m_physicsBackendWorlds(frameEngine)
	{
		static_assert(compositionContractsHold());
	}

	~SessionRuntime() = default;

	SessionRuntime(const SessionRuntime&) = delete;
	SessionRuntime& operator=(const SessionRuntime&) = delete;

	template <ConfigSource ConfigT>
	void start(SessionStart&& sessionStart, const ConfigT& config, Link* link);

	bool stop();

	void detachLink();

	bool configured() const { return m_manager.has_value() || m_integrationLayer.has_value(); }
	bool runsPrediction() const { return m_manager->runsPrediction(); }
	bool isAuthority() const { return m_start.role == SessionRole::Authority; }
	StaticData& editStaticData() { return m_staticData; }

	const std::optional<ManagerType>& getManager() const { return m_manager; }
	const Storage&         storage() const { return m_storage; }
	const InputResolution& inputResolution() const { return m_inputResolution; }
	Diagnostics&           diagnostics() { return m_diagnostics; }

	auto* lifecycle() { return m_lifecycle.has_value() ? &*m_lifecycle : nullptr; }
	auto* authorityLifecycle() { return m_authorityLifecycle.has_value() ? &*m_authorityLifecycle : nullptr; }

	bool bodiesResolvable(const Simulatable& simulatable) const
	{
		if (!m_physAdapter->isBodyResolvable(Game::rootBodyId(simulatable)))
			return false;
		bool allResolvable = true;
		simulatable.getPhysicsComposite().forEach([&](const auto& decl)
		{
			if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
				allResolvable = false;
		});
		return allResolvable;
	}

	template <typename PublishF>
	bool acquireAndBind(const BodyRequest& request, PublishF&& publish, unsigned int id, Simulatable& simulatable)
	{
		return m_tickBackend.acquireAndBind(request.key, request.name.c_str(), request.isAuthority, request.replicatedSlot,
			std::forward<PublishF>(publish), id, simulatable, *request.staticData);
	}

	template <typename PublishF>
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, const BodyRequest& request,
		PublishF&& publish)
	{
		return m_tickBackend.registerVizVolume(descriptor, request.key, request.name.c_str(), request.isAuthority,
			request.replicatedSlot, std::forward<PublishF>(publish));
	}

	void bodiesRegistered(unsigned int id, uint64_t key) { m_tickBackend.onRegistered(id, key); }

	void forgetBodies(unsigned int id, uint64_t key, bool releaseSlot)
	{
		m_tickBackend.forgetPresenterTarget(id, key);
		if (releaseSlot)
			m_tickBackend.releaseSlot(key, id);
	}

	void releaseBodySlot(unsigned int id, uint64_t key) { m_tickBackend.releaseSlot(key, id); }

	template <typename SinkT>
	void present(const PresentControls& controls, SinkT& sink) { m_tickBackend.present(controls, sink); }

	auto& editVizQuery() { return m_tickBackend.editVizQuery(); }
	const auto& getVizReader() const { return m_tickBackend.getVizReader(); }

	std::optional<uint32_t> physicsSlotFor(uint64_t key, unsigned int storageKey) const;
	void recordAuthoritativeSlot(uint64_t key, uint32_t slot);
	std::optional<ogsimnet::BindStep> bindStep(unsigned int id, bool engineSaysLocal);
	void markUnregistered(unsigned int id);

	const DisplaySnapshot* getDisplaySnapshot() const { return m_display.has_value() ? m_display->snapshot() : nullptr; }
	const DisplayEntry* findDisplayEntry(unsigned int id) const { return m_display.has_value() ? m_display->findEntry(id) : nullptr; }

	const DisplayVizState* findDisplayVizState(unsigned int id) const
	{
		return m_display.has_value() ? m_display->findVizState(id) : nullptr;
	}

	template <typename FunctionT>
	void forEachDisplayVizState(FunctionT&& function) const
	{
		if (m_display.has_value())
			m_display->forEachVizState(std::forward<FunctionT>(function));
	}

	std::optional<PlayerInput> getLastRelayedInput(unsigned int id) const
	{
		const DisplayEntry* const entry = findDisplayEntry(id);
		return entry != nullptr ? entry->lastRelayedInput : std::nullopt;
	}

	bool isLocallyControlledOnThisPeer(unsigned int id) const
	{
		const DisplayEntry* const entry = findDisplayEntry(id);
		return entry != nullptr && entry->hasLocalInputCache;
	}

	template <typename PresentF>
	void presentAuthority(PresentF&& present) { m_display->presentLatest(std::forward<PresentF>(present)); }

	void refreshDisplayView()
	{
		m_display->acquireForFrame([this](unsigned int id, DisplayVizState& vizState)
		{
			m_tickBackend.overlayRenderPoses(id, vizState);
		});
	}

	const ReplicatedTierConsumer* getReplicatedTierConsumer() const
	{
		return m_clock.has_value() ? m_clock->tierConsumer() : nullptr;
	}

	void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime)
	{
		m_diagnostics.noteWireSample_Internal(roundTripTime);
		if (!m_clock.has_value())
			return;
		m_clock->onAuthorityTick(authorityTick, roundTripTime, ClockLink{ m_simNetLanes });
	}

	void onSimClockSample(const SimClockRttSample& sample)
	{
		if (!m_clock.has_value())
			return;
		m_clock->onClockSample(sample, ClockLink{ m_simNetLanes });
	}

	void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
	{
		if (m_clock.has_value())
			m_clock->onConnectionTier(oldTier, newTier);
	}

	void onRelayDelayFloorReceived(uint8_t floorTicks)
	{
		if (m_clock.has_value())
			m_clock->onRelayFloor(floorTicks);
	}

	int32_t getServerReceptionTick() const
	{
		return static_cast<int32_t>(m_manager->getServerClock().getSimulationStep().getTick());
	}

	void onSimNetLifecycleUnregistering(uint8_t simId)
	{
		OG_CHECK(m_authorityLifecycle.has_value(),
			"SessionRuntime::onSimNetLifecycleUnregistering: the authority builds its lifecycle on the runner before the "
			"runner's first drain, and only that drain applies og-sim-net's queued unregistrations");
		m_authorityLifecycle->onUnregistering(simId);
	}

	template <typename HooksT>
	RegisterStatus tryRegisterPrediction(const RegisterRequest& request, Simulatable&& simulatable, InputProvider&& inputProvider,
		HooksT& hooks);

	void forgetPendingRegistration(uint32_t simId) { m_pendingRegistrations.erase(simId); }

	void forgetQueuedRegistrations()
	{
		for (auto it = m_pendingRegistrations.begin(); it != m_pendingRegistrations.end();)
			it = it->second.queued ? m_pendingRegistrations.erase(it) : std::next(it);
	}

	void onSimNetClientLifecycleUnregistering(uint8_t simId)
	{
		OG_CHECK(m_lifecycle.has_value(),
			"SessionRuntime::onSimNetClientLifecycleUnregistering: the client binds the og-net session after it built its "
			"lifecycle, and only a bound session hears og-sim-net's unregistering hook");
		m_lifecycle->onUnregistering(simId);
	}

private:
	struct ClockLink
	{
		const Link* lanes = nullptr;

		double ackSrttSeconds() const { return lanes != nullptr ? lanes->ackSrttSeconds() : sessionClock::kNoRttReadingSeconds; }

		ogsimnet::SimNetClockStats clockStats() const
		{
			const auto* const clock = lanes != nullptr ? lanes->clockClient() : nullptr;
			return clock != nullptr ? clock->stats() : ogsimnet::SimNetClockStats{};
		}
	};

	static_assert(SessionClockLink<ClockLink> && SessionClockServer<std::remove_pointer_t<decltype(std::declval<Link&>().clockServer())>>,
		"SessionRuntime hands its clock the link's ack round trip and SimClock counts (ClockLink, for the [ClockSource.Window] "
		"line) and og-sim-net's SimClock server (publishServerTick(tick, tickStartSeconds)). Was UESimulationSession's "
		"(moved by og-session-runtime task 31)");

	struct NetPort
	{
		using Lanes       = Link;
		using Server      = SimNetServer;
		using Idle        = SessionRuntime::SimNetIdle;
		using Simulatable = SessionRuntime::Simulatable;

		SessionRuntime* runtime = nullptr;

		Lanes*  lanes() const { return runtime->m_simNetLanes; }
		Server* server() { return runtime->m_simNetServer.has_value() ? &*runtime->m_simNetServer : nullptr; }
		Idle*   idle() { return runtime->m_simNetIdle.has_value() ? &*runtime->m_simNetIdle : nullptr; }
		void    runRunnerRequests() { runtime->m_authorityLifecycle->applyRequests(); }
		void    applyUnregisterRequests(typename Lanes::Client& client) { runtime->m_lifecycle->applyRequests(client); }
		void    publishPresentation() { runtime->publishAuthorityPresentation_Runner(); }
		double  nowSeconds() const { return runtime->m_frameEngine.nowSeconds(); }
		void    logRouted(const char* line) { runtime->m_frameEngine.logRouted(line); }
		bool    wants(SimulationLogRoute route, HostLogLevel level) const { return runtime->m_frameEngine.wants(route, level); }
	};

	using NetHooks = SessionNetHooks<NetPort, ManagerType, NetSync, Storage, Diagnostics>;

	struct LifecyclePort
	{
		using Client        = typename Link::Client;
		using Channel       = SimChannel;
		using Simulatable   = SessionRuntime::Simulatable;
		using InputProvider = SessionRuntime::InputProvider;
		using Bindings      = std::remove_reference_t<decltype(std::declval<Link&>().editBindings())>;

		SessionRuntime* runtime = nullptr;

		Client*   client() { return runtime->m_simNetLanes != nullptr ? runtime->m_simNetLanes->client() : nullptr; }
		Bindings* bindings() { return runtime->m_simNetLanes != nullptr ? &runtime->m_simNetLanes->editBindings() : nullptr; }

		InputProvider wrapInputProvider(const RegisterRequest& request, InputProvider&& provider)
		{
			return runtime->m_host.wrapInputProvider(request, std::move(provider));
		}

		void notifyRegistered(uint32_t id) { runtime->m_manager->notifyCharacterRegistered(id); }
		void notifyUnregistered(uint32_t id) { runtime->m_manager->notifyCharacterUnregistered(id); }
		bool inStorage(uint32_t id) const { return runtime->m_storage.template has<Simulatable>(id); }

		void installInputStamps(uint32_t id, Channel& channel)
		{
			Diagnostics* const diagnostics = &runtime->m_diagnostics;
			channel.setOnInputHandedCallback([diagnostics, id](const InputBundleBuffer& bundle)
			{
				bundle.template forEachSlot<PlayerInput>([diagnostics, id](uint32_t captureTick, const PlayerInput&)
				{
					diagnostics->stampHandedAndLeft_Internal(id, captureTick);
				});
			});
		}

		void clearInputStamps(Channel& channel) { channel.clearOnInputHandedCallback(); }
		void stopSteps() { runtime->stopStepping(); }

		void drainWhileStopped()
		{
			const StoppedDrainScope stoppedDrain = runtime->m_clock->stoppedDrain();
			runtime->m_simNetLanes->drainClientWhileStopped_GameThread();
		}

		void logRouted(const char* line) { runtime->m_frameEngine.logRouted(line); }
	};

	using Lifecycle = SessionLifecycle<LifecyclePort>;

	struct AuthorityBodies
	{
		using Request     = BodyRequest;
		using Simulatable = SessionRuntime::Simulatable;

		SessionRuntime* runtime = nullptr;

		Request makeRequest(const RegisterRequest& request) { return runtime->m_host.makeBodyRequest(request); }

		bool create(const Request& request, Simulatable& simulatable, uint32_t id)
		{
			return runtime->acquireAndBind(request, [](uint32_t) {}, id, simulatable);
		}

		std::optional<uint32_t> slotOf(uint64_t key) { return runtime->m_physicsBackendWorlds.host.slotOf(key); }
		void onRegistered(uint64_t key, uint32_t id) { runtime->bodiesRegistered(id, key); }
		void release(uint64_t key, uint32_t id) { runtime->forgetBodies(id, key, /*releaseSlot=*/true); }
	};

	static_assert(SessionBodies<AuthorityBodies>,
		"SessionRuntime: the runtime's tick backend is the authority lifecycle's bodies provider, og-sim-session's "
		"SessionBodies (makeRequest through the engine adapter, create, slotOf, onRegistered, release). Was the tick "
		"backend's wrapper, TickDriverBackendUImpl (og-session-runtime task 30; moved by og-session-runtime task 31)");

	struct AuthorityLifecyclePort
	{
		using Bodies      = AuthorityBodies;
		using Server      = SimNetServer;
		using Channel     = SimChannel;
		using Simulatable = SessionRuntime::Simulatable;

		SessionRuntime* runtime = nullptr;

		Server* server() { return runtime->m_simNetServer.has_value() ? &*runtime->m_simNetServer : nullptr; }
		Bodies& bodies() { return runtime->m_authorityBodies; }
		bool bodiesResolvable(const Simulatable& simulatable) const { return runtime->bodiesResolvable(simulatable); }
		uint64_t bodyKeyOf(EntityKey key) const { return runtime->m_host.bodyKeyOf(key); }
		bool runnerStopped() const { return runtime->m_runnerStopped; }
		void notifyRegistered(uint32_t id) { runtime->m_manager->notifyCharacterRegistered(id); }
		void notifyUnregistered(uint32_t id) { runtime->m_manager->notifyCharacterUnregistered(id); }
		bool inStorage(uint32_t id) const { return runtime->m_storage.template has<Simulatable>(id); }
		void logRouted(const char* line) { runtime->m_frameEngine.logRouted(line); }
	};

	using AuthorityLifecycle = SessionAuthorityLifecycle<AuthorityLifecyclePort>;

	void buildWorld(bool freeRunning);

	template <typename AfterBuildF>
	void runAuthorityLoop(AfterBuildF&& afterBuild);

	void startStepping();
	void stopStepping();
	void joinRunner();

	void attachServerLink();
	void attachClientLink();
	void logSimNetHostLine(bool isServer);
	void onLinkStopping(bool isServer);
	void onFrameStepsDue_Step(int32_t firstUpcomingSimTick, int32_t numSteps);

	void publishAuthorityPresentation_Runner()
	{
		static_assert(requires(const SystemsExec& systems, unsigned int id)
			{
				{ Game::authorityPresentationSlice_Step(systems, id) } -> std::same_as<AuthorityPresentationSlice>;
			},
			"SessionRuntime<TraitsT>: the game's presentation of authority-only state is a static hook of TraitsT::Game over "
			"its AuthorityPresentationSlice, authorityPresentationSlice_Step(systems, id), read on the runner after each "
			"batch; the engine adapter applies the newest published slice on the frame thread. Was the loop's skip of "
			"afterVisualization, whose ring-out score push read the score system on the game thread (og-input-latency P5-19 "
			"F5; moved by og-session-runtime task 31)");
		OG_CHECK(m_systemsExec.has_value(),
			"SessionRuntime::publishAuthorityPresentation_Runner: the authority's systems executor is built by the session's "
			"configuration, which the runner runs before its first batch");
		m_display->publishPresentation(*m_systemsExec);
	}

	Host&        m_host;
	Diagnostics& m_diagnostics;
	Engine&      m_frameEngine;
	SessionStart m_start;

	Worlds m_physicsBackendWorlds;

	std::optional<typename EngineTraits::BodyAdapter>   m_physAdapter;
	std::optional<typename EngineTraits::ReaderAdapter> m_physReaderAdapter;
	std::optional<typename EngineTraits::QueryAdapter>  m_queryAdapter;

	Storage m_storage;

	StaticData m_staticData{ Game::makeStaticData() };

	Reconciliation  m_reconciliation{ m_storage };
	InputResolution m_inputResolution{ m_storage, m_reconciliation };
	NetSync         m_netSync{ m_storage, m_reconciliation, m_inputResolution };

	// ⛔G-01  docs/SessionRuntime-guards.md
	std::optional<SystemsExec> m_systemsExec;

	std::optional<IntegrationExec> m_integrationLayer;

	std::optional<ManagerType> m_manager;
	TickGame    m_tickDriverGame{ TickHost{ this } };
	TickBackend m_tickBackend{ m_tickDriverGame, m_physicsBackendWorlds.host, m_frameEngine };
	typename WorldHost::Config                     m_runnerWorldConfig{};
	std::optional<typename EngineTraits::Statics> m_runnerStatics;
	std::atomic<Driver*>                           m_runnerDriver{ nullptr };
	std::unique_ptr<Runner>                        m_runner;
	bool                                           m_stepping = false;

	std::optional<SessionThreadChecks> m_threads;
	ClockEngine m_clockEngine;
	std::optional<Clock> m_clock;
	std::optional<Display> m_display;
	std::optional<NetHooks> m_netHooks;
	std::optional<Lifecycle> m_lifecycle;
	AuthorityBodies m_authorityBodies{ this };
	std::optional<AuthorityLifecycle> m_authorityLifecycle;

	Link* m_simNetLanes = nullptr;
	std::optional<SimNetServerHost> m_simNetServerHost;
	std::optional<SimNetServer> m_simNetServer;
	std::optional<SimNetIdle> m_simNetIdle;
	decltype(std::declval<Link&>().clockServer()) m_clockServer = nullptr;
	bool m_runnerStopped = false;
	uint64_t m_authorityWakes_Runner = 0u;

	FrameHealthProbe m_frameHealthProbe;

	struct PendingRegistration
	{
		std::optional<Simulatable> simulatable;
		bool bodiesCreated = false;
		InputProvider inputProvider;
		bool queued = false;
	};
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(SessionRuntime, m_storage)
		                  < __builtin_offsetof(SessionRuntime, m_staticData)
		           && __builtin_offsetof(SessionRuntime, m_staticData)
		                  < __builtin_offsetof(SessionRuntime, m_reconciliation)
		           && __builtin_offsetof(SessionRuntime, m_reconciliation)
		                  < __builtin_offsetof(SessionRuntime, m_inputResolution)
		           && __builtin_offsetof(SessionRuntime, m_inputResolution)
		                  < __builtin_offsetof(SessionRuntime, m_netSync),
			"SessionRuntime members construct in DECLARATION order, and three of these read a sibling declared above "
			"them in their own initializer: m_reconciliation binds m_storage; m_inputResolution binds m_storage and "
			"m_reconciliation; m_netSync binds all three peers above it. m_staticData's initializer, the game's "
			"Game::makeStaticData() (its one-time read of its static-data console variables), runs after m_storage is "
			"built and before the peers, the order it ran in when the owner held these members. A reorder compiles (no "
			"-Wreorder-as-error in this tree) and constructs in the new, wrong order. Keep m_storage < m_staticData < "
			"m_reconciliation < m_inputResolution < m_netSync. Was the owner's construction-order row, the CONSTRUCTION "
			"ORDER prose fence of its header before that (task 11), with the owner's cvar member folded into "
			"Game::makeStaticData; moved here by og-ue-simulationcomponentandmanagersplit task 24a; moved by "
			"og-session-runtime task 31.");
		static_assert(std::is_same_v<Storage, typename Composition::Storage>,
			"SessionRuntime requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Storage must name the same type as the session's Storage, "
			"apply_t<SimulationObjectStorage, Simulatables>. Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<Reconciliation, typename Composition::Reconciliation>,
			"SessionRuntime requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Reconciliation must name the same type as the session's Reconciliation, "
			"apply_t<SimulationReconciliation, Simulatables>. Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<InputResolution, typename Composition::InputResolution>,
			"SessionRuntime requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::InputResolution must name the same type as the session's InputResolution, "
			"apply_t<SimulationInputResolution, Simulatables>. Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<NetSync, typename Composition::NetSync>,
			"SessionRuntime requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::NetSync must name the same type as the session's NetSync, "
			"apply_t<SimulationNetSync, Simulatables>. Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<ManagerType, typename Composition::Manager>,
			"SessionRuntime requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Manager must name the same type as the session's ManagerType, og-simulation's "
			"SimulationManager over the session's IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec, "
			"Storage and StaticData. Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<IntegrationExec, typename Composition::IntegrationExec>,
			"SessionRuntime requires og-simulation's own integration executor (user ruling 2026-10-24), so "
			"TraitsT::Composition::IntegrationExec must name the same type as the session's IntegrationExec, "
			"SimulationIntegrationExecutor<StaticData, EngineTraits::BodyAdapter, EngineTraits::QueryAdapter, Simulatables...>: "
			"a game chooses its static data and its engine through the traits, not the executor. Moved by og-session-runtime "
			"task 31.");
		static_assert(std::is_same_v<SystemsExec, typename Composition::SystemsExec>,
			"SessionRuntime requires og-simulation's own systems executor (user ruling 2026-10-24), so "
			"TraitsT::Composition::SystemsExec must name the same type as the session's SystemsExec, "
			"SimulationSystemsExecutor<Simulatables, StaticData, Systems...> over TraitsT::Composition::Systems in that "
			"list's order: a game chooses its systems and their order through the list, and the firing order is the "
			"template order. Moved by og-session-runtime task 31.");
		static_assert(PhysicsEngineTraits<EngineTraits>,
			"TraitsT::EngineTraits must model og-simulation's PhysicsEngineTraits (PhysicsEngineTraits.h): the session and "
			"its integration executor read the engine's adapter types, name and visualization types through it. The engine "
			"is the game's choice, made in its traits (user ruling 2026-10-24). Moved by og-session-runtime task 31.");
		static_assert(std::is_same_v<ManagerType, typename TickGame::Manager>,
			"SessionRuntime::ManagerType must BE the manager type its tick game names (TickGame::Manager, the runtime's own "
			"tick host's), so the tick backend's step driver, SimulationStepDriver<Manager, ...>, drives m_manager's "
			"instantiation. Was the owner's ManagerType row (added by og-simulationscheduler-withjolt task 81), which compared "
			"it with the game's composition struct; moved here by og-ue-simulationcomponentandmanagersplit task 24a; compared "
			"with TraitsT::TickDriverGame until og-session-runtime task 26, and with the backend wrapper's tick game until "
			"og-session-runtime task 31 moved the tick backend into the runtime.");
		static_assert(TickDriverSampleGame<TickGame> && TickDriverReceiveGame<TickGame> && TickDriverDisplayGame<TickGame>
		           && TickDriverIdleGame<TickGame>,
			"SessionRuntime: its tick game (og-sim-session's SessionTickGame over the runtime's tick host) must model the four "
			"optional hooks' concepts; SessionTickGame.h states why each is needed. Were the four static_asserts on the "
			"brawler's BrawlerTickDriverGame (og-session-runtime task 26); moved by og-session-runtime task 31");
		static_assert(std::is_same_v<Storage, typename TickGame::Storage>,
			"SessionRuntime::Storage must BE the storage type its tick game hands the driver; the tick host returns m_storage "
			"as it. Moved by og-session-runtime task 31");
		static_assert(__builtin_offsetof(SessionRuntime, m_physicsBackendWorlds)
		                  < __builtin_offsetof(SessionRuntime, m_physAdapter)
		           && __builtin_offsetof(SessionRuntime, m_physAdapter)
		                  < __builtin_offsetof(SessionRuntime, m_physReaderAdapter)
		           && __builtin_offsetof(SessionRuntime, m_physAdapter)
		                  < __builtin_offsetof(SessionRuntime, m_queryAdapter)
		           && __builtin_offsetof(SessionRuntime, m_queryAdapter)
		                  < __builtin_offsetof(SessionRuntime, m_integrationLayer),
			"SessionRuntime members destruct in REVERSE declaration order. The backend's worlds must outlive every "
			"adapter bound to them (the Jolt step world; the reader holds the body adapter's bind table, the integration "
			"layer holds the adapters), so m_physicsBackendWorlds sits above the adapters in every configuration. Keep "
			"m_physicsBackendWorlds < m_physAdapter < {m_physReaderAdapter, m_queryAdapter} < m_integrationLayer. Was the "
			"owner's destruction-order row, design D's compile-time row 'runtime < world < adapters' before that "
			"(og-simulationscheduler-withjolt task 18); its runtime < world half is og-simulation-jolt's JoltTickWorlds' own "
			"row (runtime < contactCounter < stepWorld) since og-tickdriver task 10. Moved here by "
			"og-ue-simulationcomponentandmanagersplit task 24a; moved by og-session-runtime task 31.");
		static_assert(__builtin_offsetof(SessionRuntime, m_manager) < __builtin_offsetof(SessionRuntime, m_tickBackend)
		           && __builtin_offsetof(SessionRuntime, m_physicsBackendWorlds) < __builtin_offsetof(SessionRuntime, m_tickBackend)
		           && __builtin_offsetof(SessionRuntime, m_tickDriverGame) < __builtin_offsetof(SessionRuntime, m_tickBackend)
		           && __builtin_offsetof(SessionRuntime, m_tickBackend) < __builtin_offsetof(SessionRuntime, m_runner),
			"SessionRuntime members destruct in REVERSE declaration order. The tick backend holds the step driver, which "
			"unregisters its resync callback from the manager's client clock in its destructor, so it goes before the "
			"manager; its driver locks and steps the world host and its presenter holds the tick game, so both outlive it; "
			"the runner's thread runs the backend's loop, so the runner (joined before it goes) goes first. Keep m_manager, "
			"m_physicsBackendWorlds and m_tickDriverGame < m_tickBackend < m_runner. Was the owner's m_manager < "
			"m_physicsBackend row (og-simulationscheduler-withjolt task 18), TickDriverBackendUImpl's m_tickDriverGame < "
			"m_tickBackend (og-tickdriver task 15d) and m_tickBackend < m_runner (og-input-latency P5-19) rows and the "
			"session's m_physicsBackendWorlds < m_physicsBackend row (og-tickdriver task 10); one row since "
			"og-session-runtime task 31 moved the tick backend into the runtime.");
		static_assert(__builtin_offsetof(SessionRuntime, m_manager) < __builtin_offsetof(SessionRuntime, m_clock)
		           && __builtin_offsetof(SessionRuntime, m_inputResolution) < __builtin_offsetof(SessionRuntime, m_clock)
		           && __builtin_offsetof(SessionRuntime, m_clockEngine) < __builtin_offsetof(SessionRuntime, m_clock),
			"SessionRuntime members destruct in REVERSE declaration order. The clock (og-sim-session's SessionClock) keeps "
			"references to the manager, the input-resolution peer and its engine, and its tier consumer is emplaced from the "
			"manager's TimeConfig and keeps a reference to it (ReplicatedTierConsumer borrows its config), so the clock must be "
			"destroyed before all three. Keep m_manager, m_inputResolution and m_clockEngine < m_clock. Was the "
			"m_manager < m_replicatedTierConsumer row (og-ue-simulationcomponentandmanagersplit task 25: in the owner the "
			"consumer was declared above the member that holds the manager, and only the owner's EndPlay reset kept it from "
			"outliving that TimeConfig); moved onto the clock with the consumer by og-session-runtime task 24; moved by "
			"og-session-runtime task 31.");
		static_assert(__builtin_offsetof(SessionRuntime, m_storage) < __builtin_offsetof(SessionRuntime, m_display)
		           && __builtin_offsetof(SessionRuntime, m_reconciliation) < __builtin_offsetof(SessionRuntime, m_display)
		           && __builtin_offsetof(SessionRuntime, m_inputResolution) < __builtin_offsetof(SessionRuntime, m_display)
		           && __builtin_offsetof(SessionRuntime, m_manager) < __builtin_offsetof(SessionRuntime, m_display)
		           && __builtin_offsetof(SessionRuntime, m_clock) < __builtin_offsetof(SessionRuntime, m_display),
			"SessionRuntime members destruct in REVERSE declaration order. The display (og-sim-session's SessionDisplay) "
			"keeps references to the storage, the reconciliation, the input resolution and the manager it fills from, and to "
			"the clock's display-tier buffer, so it must be destroyed before all five. Keep m_storage, m_reconciliation, "
			"m_inputResolution, m_manager and m_clock < m_display (og-session-runtime task 25: the display's members were the "
			"session's own until then, built with it; moved by og-session-runtime task 31)");
		static_assert(__builtin_offsetof(SessionRuntime, m_storage) < __builtin_offsetof(SessionRuntime, m_netHooks)
		           && __builtin_offsetof(SessionRuntime, m_netSync) < __builtin_offsetof(SessionRuntime, m_netHooks)
		           && __builtin_offsetof(SessionRuntime, m_manager) < __builtin_offsetof(SessionRuntime, m_netHooks),
			"SessionRuntime members destruct in REVERSE declaration order. The net hooks (og-sim-session's SessionNetHooks) "
			"keep references to the storage, the net sync and the manager, and to the latency diagnostics, which the engine "
			"adapter owns and declares before the runtime, so they must be destroyed before all four. Keep m_storage, "
			"m_netSync and m_manager < m_netHooks (og-session-runtime task 26: the hooks were the session's member functions "
			"until then; moved by og-session-runtime task 31, the diagnostics' half now the adapter's row)");
		static_assert(__builtin_offsetof(SessionRuntime, m_authorityBodies) < __builtin_offsetof(SessionRuntime, m_authorityLifecycle)
		           && __builtin_offsetof(SessionRuntime, m_tickBackend) < __builtin_offsetof(SessionRuntime, m_authorityLifecycle),
			"SessionRuntime members destruct in REVERSE declaration order. The authority lifecycle holds its bodies provider "
			"by reference and its queued requests reach the tick backend through it, so both outlive it. Keep "
			"m_authorityBodies and m_tickBackend < m_authorityLifecycle (og-session-runtime task 31)");
		static_assert(__builtin_offsetof(SessionRuntime, m_manager)
		                  < __builtin_offsetof(SessionRuntime, m_simNetServer)
		           && __builtin_offsetof(SessionRuntime, m_simNetServerHost)
		                  < __builtin_offsetof(SessionRuntime, m_simNetServer),
			"SessionRuntime members destruct in REVERSE declaration order. og-sim-net's server keeps a reference to the "
			"manager's TimeConfig (and its reception coordinator borrows it too) and to its host, so the server is destroyed "
			"before the manager and its host. Keep m_manager < m_simNetServer and m_simNetServerHost < m_simNetServer. Was the "
			"m_manager < m_receptionCoordinator row (task 26a); the coordinator is the server's own since og-input-latency task "
			"P5-7. The session's own detached lanes left this row with them (og-session-runtime task 14); moved by "
			"og-session-runtime task 31.");
		static_assert(latencyKnobs::kMaxRelayResendEntries == relayedInputRing::kMaxResendEntries,
			"TimeConfig::relayResendEntries' clamp and the relay codec's re-send bound must agree, or a session value the knob "
			"accepts would be cut again at og-sim-net's relay flush. og-input-latency task 8; was beside the input relay's spawn "
			"in UESimulatableOwnerBinding until task P5-7; moved by og-session-runtime task 31");
		static_assert(std::is_same_v<PredictionOwnerFor<Simulatable>, SimChannel>
		           && std::is_same_v<AuthorityOwnerFor<Simulatable>, SimChannel>
		           && std::is_same_v<SimChannel, typename Link::Client::Channel>,
			"SimulatableOwnerTraits<TraitsT::Simulatable> must name og-sim-net's one channel type for both roles: the server "
			"registers a character with its SimNetServer channel as both owners and a client with its SimNetClient channel, "
			"and those are one type (SessionChannel over the link's transport). Was the component binding as both owners, "
			"which the session's two sink assertions (RemoteInputDeliverySink, RemoteInputRelaySink) served until "
			"og-input-latency task P5-7; moved by og-session-runtime task 31 (the Unreal channel alias's half is the "
			"adapter's row)");
		static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples,
			"PROBE A's window must stay the same length as ResimGateProbe's, so one [ResimProbe.Frame] window lines up "
			"against the surrounding [ResimProbe.Gate] windows without a resim-tick count on the line. Was the COMPARABLE "
			"WITH ResimGateProbe caution in PROBE A's banner; moved by og-session-runtime task 31");
		return true;
	}
};

template <typename TraitsT>
template <ConfigSource ConfigT>
void SessionRuntime<TraitsT>::start(SessionStart&& sessionStart, const ConfigT& config, Link* link)
{
	static_assert(requires(std::optional<SystemsExec>& systemsExec, typename EngineTraits::BodyAdapter& bodyAdapter,
			typename EngineTraits::QueryAdapter& queryAdapter, bool flag)
		{
			Game::emplaceSystems(systemsExec, bodyAdapter, queryAdapter);
			{ Game::neutralInput() } -> std::same_as<PlayerInput>;
			Game::installGameLoggers(flag);
			{ Game::backendFingerprint() } -> std::same_as<std::string_view>;
		},
		"SessionRuntime<TraitsT>::start calls four static phase hooks of TraitsT::Game, each at the position of the game "
		"statements it replaced: emplaceSystems(systemsExec, bodyAdapter, queryAdapter), neutralInput() (the PlayerInput a "
		"delay window is filled with), installGameLoggers(isAuthority) and backendFingerprint() (a std::string_view). The "
		"engine adapter calls the game's instance hooks (beforeWorldBuild, afterSessionConfigured). Added by "
		"og-ue-simulationcomponentandmanagersplit task 27b (manager design 4.3); moved by og-session-runtime task 31.");

	static_assert(SessionRuntimeHost<Host, Driver, InputProvider, StaticData>,
		"SessionRuntime: TraitsT::Host is the engine adapter the runtime calls back into: the instance registry and the "
		"engine's configuration lines, the frame entry, the input sample and the post-physics pass, the routed message "
		"sink, the bodies' engine facts, the input provider's wrap and the component bookkeeping. It is checked here, "
		"where the host is complete: the host holds the runtime as a member (og-session-runtime task 31)");

	m_start = std::move(sessionStart);
	const bool worldIsAuthority = isAuthority();
	const float dt = static_cast<float>(m_start.stepSeconds);
	m_simNetLanes = link;

	SessionKnobs sessionKnobs = readSessionKnobs(config, worldIsAuthority);
	SessionThreadChecks clockThreads(worldIsAuthority ? SessionRole::Authority : SessionRole::Prediction,
		&m_physicsBackendWorlds.host.threadContext(), std::this_thread::get_id());
	if (m_start.netIoThread != std::thread::id{})
		clockThreads.setNetIoThread(m_start.netIoThread);
	m_threads.emplace(clockThreads);

	auto configureSession = [&]()
	{
		m_host.onConfigure();
		Host* const host = &m_host;
		auto pctmlogger = [host](const char* msg) { host->logHost(msg); };
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(/*usePrediction=*/!worldIsAuthority, dt, typename ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_clock.emplace(m_clockEngine, *m_manager, m_inputResolution, clockThreads);
		m_display.emplace(clockThreads, *m_manager, m_storage, m_inputResolution, m_reconciliation, m_clock->displayTier());
		m_netHooks.emplace(clockThreads, NetPort{ this }, *m_manager, m_netSync, m_storage, m_diagnostics);
		if (worldIsAuthority)
			m_authorityLifecycle.emplace(clockThreads, AuthorityLifecyclePort{ this });
		else
			m_lifecycle.emplace(clockThreads, LifecyclePort{ this });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

		// ⛔G-02  docs/SessionRuntime-guards.md
		m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());

		if (worldIsAuthority)
		{
			attachServerLink();
			applySessionKnobs(m_clockEngine, *m_manager, m_simNetServer.has_value() ? &*m_simNetServer : nullptr,
				sessionKnobs, [this](int32_t floorTicks) { m_clock->logRelayDelayFloorAdvisory(floorTicks); });
		}

		// ⛔G-03  docs/SessionRuntime-guards.md
		m_clock->beginTierConsumer();

		if (!worldIsAuthority)
			attachClientLink();
		simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
		Game::installGameLoggers(worldIsAuthority);
		m_host.bindDiagnosticGates();
		if (!worldIsAuthority)
			m_host.afterPredictionConfigured();

		// ⛔G-04  docs/SessionRuntime-guards.md
		applyResimTriggerPolicy(m_clockEngine, *m_manager, sessionKnobs);

		applyLatencyKnobs(m_clockEngine, *m_manager, sessionKnobs, m_clock->dilationController(), m_clock->dilationWindow());

		m_host.onSessionConfigured();

		static_assert(sessionRuntimeLines::isAsciiName(EngineTraits::kName),
			"EngineTraits::kName must be ASCII: the [SimHost.Backend] line goes through logRouted as UTF-8, where the Unreal "
			"session's UE_LOG widened it with %hs byte by byte; the two print the same bytes only for an ASCII name "
			"(og-session-runtime task 31)");
		const std::string_view fingerprint = Game::backendFingerprint();
		char line[256];
		std::snprintf(line, sizeof(line), sessionRuntimeLines::kBackend, EngineTraits::kName,
			static_cast<int>(fingerprint.size()), fingerprint.data());
		m_frameEngine.logRouted(line);
	};

	if (worldIsAuthority)
	{
		buildWorld(/*freeRunning=*/true);
		runAuthorityLoop(configureSession);
		m_frameEngine.logRouted(sessionRuntimeLines::kManagerInitialized);
		return;
	}

	buildWorld(/*freeRunning=*/false);

	configureSession();

	startStepping();
	m_frameEngine.logRouted(sessionRuntimeLines::kManagerInitialized);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::buildWorld(bool freeRunning)
{
	const typename EngineTraits::Statics statics = EngineTraits::splitStatics(m_start.staticWorld, m_start.physicsOnlyShapes);
	typename WorldHost::Config config = TraitsT::worldConfig(isAuthority(), m_staticData);
	if (freeRunning)
	{
		static_assert(requires { config.freeRunning = true; },
			"SessionRuntime::buildWorld: the authority loop needs a world host that builds FreeRunning on the runner "
			"(JoltTickWorldHostConfig::freeRunning). Was P11 design section 9's UE half; moved by og-session-runtime task 31");
		OG_CHECK(isAuthority(), "SessionRuntime::buildWorld: only a dedicated server's authority runs the authority loop");
		config.freeRunning = true;
		m_runnerWorldConfig = config;
		m_runnerStatics.emplace(statics);
		return;
	}
	m_tickBackend.build(config, statics.queryable, statics.physicsOnly, m_physAdapter, m_physReaderAdapter, m_queryAdapter);
}

template <typename TraitsT>
template <typename AfterBuildF>
void SessionRuntime<TraitsT>::runAuthorityLoop(AfterBuildF&& afterBuild)
{
	OG_CHECK(m_threads->isOn(SessionThread::Frame) && m_runnerStatics.has_value() && m_runner == nullptr,
		"SessionRuntime::runAuthorityLoop: buildWorld imported the statics for the runner first, once, on the frame thread");
	const AuthorityLoopConfig loopConfig{ m_start.stepSeconds, m_start.requestedMaxCatchUpSteps,
		m_start.authorityIdleSlackSeconds };
	m_runner = m_frameEngine.startRunner(
		[this, loopConfig, afterBuild = std::forward<AfterBuildF>(afterBuild)](Runner& runner) mutable
		{
			typename EngineTraits::Statics statics = std::move(*m_runnerStatics);
			m_runnerStatics.reset();
			m_tickBackend.runAuthorityLoop(m_runnerWorldConfig, std::move(statics.queryable), std::move(statics.physicsOnly),
				loopConfig, runner.host(), runner.stopRequested(),
				[&afterBuild]() { afterBuild(); },
				[this, &runner](Driver& driver)
				{
					m_runnerDriver.store(&driver, std::memory_order_release);
					m_stepping = true;
					runner.releaseStart();
				},
				m_physAdapter, m_physReaderAdapter, m_queryAdapter);
		},
		m_start.highResolutionWait);
	m_runner->waitForStart();
	Driver* const driver = m_runnerDriver.load(std::memory_order_acquire);
	OG_CHECK(driver != nullptr, "SessionRuntime::runAuthorityLoop: the runner released its start latch without starting the driver");
	m_host.beginFrames(*driver);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::startStepping()
{
	Driver& tickDriver = m_tickBackend.start(m_start.stepSeconds, isAuthority(), m_start.requestedMaxCatchUpSteps,
		/*idleSlackSeconds=*/0.0, isAuthority() ? FrameWait::EarlierFrames : FrameWait::ThisFramesSingleStep);
	m_stepping = true;
	m_host.beginFrames(tickDriver);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::joinRunner()
{
	if (m_runner != nullptr)
		m_runner->join();
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::stopStepping()
{
	m_host.endFrames();
	joinRunner();
	// ⛔G-05  docs/SessionRuntime-guards.md
	m_tickBackend.stop();
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::attachServerLink()
{
	Link* const lanes = m_simNetLanes;
	OG_CHECK(lanes != nullptr && lanes->hostServiced(),
		"SessionRuntime::attachServerLink: the engine adapter starts an authority session only on a dedicated server with "
		"the og-net driver, whose link is host-serviced. Was the detached server and the in-frame server's frame-point lanes "
		"(og-session-runtime task 14)");
	const auto& endpointConfig = lanes->endpoint().config();
	m_simNetServerHost.emplace(lanes->driver(), lanes->endpoint(), this);
	m_simNetServer.emplace(*m_simNetServerHost, lanes->transport(), m_manager->getTimeConfig(),
		ogsimnet::correctionAllowanceBytes(endpointConfig.lanes, endpointConfig.budget,
			static_cast<double>(NetConfig::tickFrequencyHz)));
	m_simNetServer->bindLifecycle(m_storage, m_reconciliation, m_inputResolution, m_netSync);
	m_clockServer = lanes->clockServer();
	m_simNetIdle.emplace(lanes->endpoint());
	lanes->bindAuthorityRunner([this]() { m_simNetServerHost->publishRunnerRtt_GameThread(); },
		[this]()
		{
			joinRunner();
			m_runnerStopped = true;
		});
	lanes->setStopHandler([this]() { onLinkStopping(/*isServer=*/true); });
	logSimNetHostLine(/*isServer=*/true);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::attachClientLink()
{
	OG_CHECK(m_manager.has_value() && getReplicatedTierConsumer() != nullptr,
		"SessionRuntime: the client binds the og-net session after the manager and the tier consumer exist: the bind replays "
		"the latched relay-delay floor (the manager's) and connection tier (the tier consumer's), and either apply returns "
		"early without its target. Was G-07's bind-then-pull order, on the timing and connection relays");
	Link* const lanes = m_simNetLanes;
	OG_CHECK(lanes != nullptr,
		"SessionRuntime::attachClientLink: the engine adapter starts a client session only on the og-net driver. Was the "
		"detached client's lanes (og-session-runtime task 14)");
	// ⛔G-06  docs/SessionRuntime-guards.md
	OG_CHECK(!m_stepping,
		"SessionRuntime::attachClientLink: the link is attached before the step thread starts: its roster seed and bound "
		"flag are published to the steps by that start alone (SimNetLink-rationale section 5; og-session-runtime task 31)");
	lanes->setOnRegisteredRemoved([this](uint8_t simId) { m_lifecycle->onRegisteredRemoved(simId); });
	lanes->setStopHandler([this]() { onLinkStopping(/*isServer=*/false); });
	lanes->client()->bindLifecycle(m_storage, m_reconciliation, m_inputResolution, m_netSync);
	lanes->bindClientSession(*this);
	logSimNetHostLine(/*isServer=*/false);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::logSimNetHostLine(bool isServer)
{
	char line[sessionRuntimeLines::kSimNetHostBytes];
	std::snprintf(line, sizeof(line), sessionRuntimeLines::kSimNetHost, isServer ? "server" : "client", /*immediate=*/1,
		"OGNet", static_cast<unsigned>(ogsimnet::kSimNetLinkInboxSlots));
	m_host.logHost(line);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::onLinkStopping(bool isServer)
{
	m_host.onLinkStopping(isServer);
	m_simNetIdle.reset();
	m_clockServer = nullptr;
	m_simNetServer.reset();
	m_simNetServerHost.reset();
	m_simNetLanes = nullptr;
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::detachLink()
{
	Link* const lanes = std::exchange(m_simNetLanes, nullptr);
	if (lanes == nullptr)
		return;
	lanes->setStopHandler(nullptr);
	if (lanes->isServer())
	{
		lanes->unbindServerSession();
		return;
	}
	m_host.forEachRegisteredId([lanes](uint32_t id) { lanes->editBindings().markUnregistered(static_cast<uint8_t>(id)); });
	if (m_lifecycle.has_value())
		m_lifecycle->detach(lanes->editBindings());
	lanes->setOnRegisteredRemoved(nullptr);
	lanes->unbindClientSession();
}

template <typename TraitsT>
bool SessionRuntime<TraitsT>::stop()
{
	// ⛔G-07  docs/SessionRuntime-guards.md
	stopStepping();
	if (!m_manager.has_value())
		return false;
	m_runnerStopped = !m_manager->runsPrediction();

	m_host.teardownComponents();

	detachLink();

	// ⛔G-08  docs/SessionRuntime-guards.md
	m_simNetIdle.reset();
	m_clockServer = nullptr;
	m_simNetServer.reset();
	m_simNetServerHost.reset();
	m_host.forgetComponents();
	if (m_authorityLifecycle.has_value())
		m_authorityLifecycle->forgetRegistered();

	m_clock->endTierConsumer();

	if (m_display.has_value())
		m_display->releaseHeld();
	return true;
}

template <typename TraitsT>
std::optional<uint32_t> SessionRuntime<TraitsT>::physicsSlotFor(uint64_t key, unsigned int storageKey) const
{
	OG_CHECK(m_manager.has_value(), "SessionRuntime::physicsSlotFor before start built the manager");
	if (!m_manager->runsPrediction())
		return m_authorityLifecycle.has_value() ? m_authorityLifecycle->authoritativeSlotFor(key) : std::nullopt;
	if (storageKey == 0u)
		return std::nullopt;
	OG_CHECK(storageKey <= 0xFFu,
		"SessionRuntime::physicsSlotFor: a client's slot comes from the roster, which keys characters by their sim id "
		"(1..255); this storage key is not one");
	Link* const lanes = m_simNetLanes;
	if (lanes == nullptr || lanes->client() == nullptr)
		return std::nullopt;
	const ogsimnet::BindVerdict entry = lanes->bindings().verdict(static_cast<uint8_t>(storageKey), /*engineSaysLocal=*/false);
	if (entry.step == ogsimnet::BindStep::Wait || entry.physicsSlot == ogsimnet::kRosterNoSlot)
		return std::nullopt;
	return static_cast<uint32_t>(entry.physicsSlot);
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::recordAuthoritativeSlot(uint64_t key, uint32_t slot)
{
	OG_CHECK(m_authorityLifecycle.has_value(),
		"SessionRuntime::recordAuthoritativeSlot on a peer without the og-sim-net server: only the authority chooses a slot, "
		"and a client binds the slot its roster entry carries. Was SetAuthoritativePhysicsSlot's authority checkf");
	m_authorityLifecycle->recordAuthoritativeSlot(key, slot);
}

template <typename TraitsT>
std::optional<ogsimnet::BindStep> SessionRuntime<TraitsT>::bindStep(unsigned int id, bool engineSaysLocal)
{
	if (m_simNetLanes == nullptr)
		return std::nullopt;
	OG_CHECK(id != 0u && id <= 0xFFu,
		"SessionRuntime::bindStep: the roster keys a character by its sim id (1..255); this id is not one");
	const ogsimnet::BindVerdict verdict = m_simNetLanes->bindings().verdict(static_cast<uint8_t>(id), engineSaysLocal);
	if (verdict.step != ogsimnet::BindStep::RoleMismatch)
		return verdict.step;
	ogsimnet::routeSimNetHostLine(ogsimnet::formatRosterBindMismatch(verdict.simId, verdict.isLocal, engineSaysLocal).data(),
		[this](const char* routed) { m_host.logHost(routed); });
	OG_CHECK(false,
		"SessionRuntime::bindStep: a sim id is local in the roster and remote by the engine's role, or the reverse (the "
		"[Roster.BindMismatch] line above names it). Was P7's C-3 cross-check: the roster's local index decides which "
		"characters this client predicts");
	return verdict.step;
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::markUnregistered(unsigned int id)
{
	if (id == 0u || id > 0xFFu)
		return;
	if (m_simNetLanes != nullptr)
		m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
}

template <typename TraitsT>
template <typename HooksT>
RegisterStatus SessionRuntime<TraitsT>::tryRegisterPrediction(const RegisterRequest& request, Simulatable&& simulatable,
	InputProvider&& inputProvider, HooksT& hooks)
{
	static_assert(requires(HooksT& h, Simulatable& s)
		{
			{ h.collectOutcomes() } -> std::same_as<void>;
			{ h.createBodies(s) }   -> std::same_as<bool>;
			{ h.bodiesCreated(s) }  -> std::same_as<void>;
		},
		"SessionRuntime::tryRegisterPrediction: the engine adapter's hooks poll the lifecycle's outcomes, create the bodies "
		"on the first call and run the game's afterBodiesCreated (og-session-runtime task 31)");
	const unsigned int id = request.simId;
	auto it = m_pendingRegistrations.find(id);
	if (it == m_pendingRegistrations.end())
	{
		PendingRegistration record;
		record.simulatable.emplace(std::move(simulatable));
		record.inputProvider = std::move(inputProvider);
		it = m_pendingRegistrations.emplace(id, std::move(record)).first;
	}
	PendingRegistration& record = it->second;

	if (record.queued)
	{
		hooks.collectOutcomes();
		const RegisterStatus status = m_lifecycle->tryRegister(request, std::move(simulatable), std::move(inputProvider));
		if (status != RegisterStatus::Pending)
			m_pendingRegistrations.erase(id);
		return status;
	}

	if (!record.bodiesCreated)
	{
		if (!hooks.createBodies(*record.simulatable))
			return RegisterStatus::Pending;
		hooks.bodiesCreated(*record.simulatable);
		record.bodiesCreated = true;
		return RegisterStatus::Pending;
	}

	if (!bodiesResolvable(*record.simulatable))
		return RegisterStatus::Pending;

	m_lifecycle->tryRegister(request, std::move(*record.simulatable), std::move(record.inputProvider));
	if (m_lifecycle->isQueued(id))
	{
		record.simulatable.reset();
		record.queued = true;
	}
	return RegisterStatus::Pending;
}

template <typename TraitsT>
void SessionRuntime<TraitsT>::onFrameStepsDue_Step(int32_t firstUpcomingSimTick, int32_t numSteps)
{
	const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
	const uint64_t probeFrame = isClient ? m_frameEngine.frameNumber() : ++m_authorityWakes_Runner;
	FrameHealthWindowSummary frameSummary;
	const bool frameWindowClosed = m_frameHealthProbe.noteFrame(probeFrame, static_cast<uint32_t>(firstUpcomingSimTick),
		static_cast<uint32_t>(numSteps), static_cast<uint64_t>(m_frameEngine.nowSeconds() * 1000000.0), frameSummary);
	if (!frameWindowClosed)
		return;

	char line[256];
	std::snprintf(line, sizeof(line), isClient ? sessionRuntimeLines::kClientFrameWindow : sessionRuntimeLines::kServerFrameWindow,
		frameSummary.totalSimTicks, frameSummary.totalFrames, frameSummary.meanTicksPerFrameX100, frameSummary.p50,
		frameSummary.p99, frameSummary.maxTicksPerFrame, frameSummary.meanFrameMicros);
	m_host.logHost(line);
	std::snprintf(line, sizeof(line), isClient ? sessionRuntimeLines::kClientFrameCadence : sessionRuntimeLines::kServerFrameCadence,
		frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples, frameSummary.skippedFrameSamples,
		frameSummary.discontinuities, frameSummary.totalNumSteps, frameSummary.maxNumSteps, frameSummary.numStepsAboveOne);
	m_host.logHost(line);
}
} // namespace ogsession
