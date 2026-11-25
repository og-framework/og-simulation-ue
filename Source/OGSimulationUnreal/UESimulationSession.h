// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/StrongObjectPtr.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/BodySlotOccupancy.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/SimulationComparison.h"
#include "OGSimulation/Mailbox.h"
#include "OGSimulation/SimulationLog.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UEAuthorityRunner.h"
#include "OGSimulationUnreal/UEConfigSource.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESessionRefusal.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "OGSimulationUnreal/UETickDriverHost.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetIdle.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimSession/SessionClock.h"
#include "OGSimSession/SessionDisplay.h"
#include "OGSimSession/SessionKnobs.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/CorrectionRotation.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/Network/RelayReadProbe.h"
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/PCTimeManagement/ClockDilationController.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"

#include <array>
#include <atomic>
#include <concepts>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class AActor;

#if !UE_BUILD_SHIPPING
OGSIMULATIONUNREAL_API int32 consumeTestGameThreadHitchMs();
OGSIMULATIONUNREAL_API void sleepTestGameThreadHitch(int32 hitchMs);
#endif

#define HasAuthority HasAuthority_is_not_the_peer_role_in_UESimulationSession_use_worldIsAuthority_or_runsPrediction

enum class TryRegisterStatus { Pending, Ready, Refused };

enum class ClientBindStep { Wait, Ready, Abandon };

inline constexpr char kSimNetHostLineFormat[] =
	"[Warning][SimNet.Host] role=%s immediate=%d rttSource=EnginePing driver=%s inbox=%u";
inline constexpr size_t kSimNetHostLineBytes = 128u;
inline constexpr size_t kSessionDisplayWindowTicks = kLocalInputCacheCapacityTicks;

static_assert(kSimNetHostLineBytes >= sizeof(kSimNetHostLineFormat) + sizeof("server") + sizeof("OGNet") + 10u,
	"kSimNetHostLineBytes must hold the [SimNet.Host] proof line with the longer role, the driver name and a 10-digit inbox "
	"size");

constexpr bool ueSessionKnobLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Net && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRelayDelayFloorClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRelayDelayFloorSession, ELogVerbosity::Log)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kStateRotationClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kStateRotationSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kResimGateUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kResimGateSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsRedundancyClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsResendClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRecentreClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRecentreSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kJitterNotFinite, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kJitterClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kClockRttSourceUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kClockSourceSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationNotApplicable, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationSetpointUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoors, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoorNotFinite, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoorClamped, ELogVerbosity::Warning),
	"Each session-knob line (ogsession::sessionKnobsLines) reaches LogOGNet at the level its UE_LOG had: Log for the "
	"floor's ini line, Warning for every other. Was each line's UE_LOG(LogOGNet, <Level>, ...) in UESimulationSession "
	"(og-session-runtime task 23)");
static_assert(ogsession::SessionLogSink<UESessionLogSink>,
	"UESessionLogSink is the session knobs' log sink: og-sim-session's SessionLogSink");

struct UESessionClockEngine
{
	void logRouted(const char* line) { RouteOGLevelLine(line); }
	double nowSeconds() const { return FPlatformTime::Seconds(); }
};

static_assert(ogsession::SessionClockEngine<UESessionClockEngine>,
	"UESessionClockEngine is the session clock's engine: its lines through the log routes, and FPlatformTime::Seconds(), "
	"the clock og-net stamps receive times with");
static_assert(ogsession::sessionClock::kNoRttReadingSeconds == kNoRttReadingSeconds,
	"og-sim-session's clock prints the same no-reading value as the Unreal client host's (UESimNetHost.h), -1");

constexpr bool ueSessionClockLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Net && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kConnectionTierChange, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kConnectionTierTag, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayDelayFloorTag, ELogVerbosity::Log)
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kNoTransition, TEXT("%s floor"))
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kTransition, TEXT("[Warning]%s floor"))
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kDilationWindow, TEXT("[Warning]%s"))
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorClamped, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorBelowHiccupBaseline, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorUniformD, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kAppliedInputDelay, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kClockSourceWindow, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute("[Warning][Dilation.Window] mode=%s", ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kDilationAuthority, ELogVerbosity::Warning),
	"Each session-clock line (ogsession::sessionClockLines) reaches LogOGNet at the level its UE_LOG had: Log for the tier "
	"change, the no-transition line (no token, led by its tag: [ConnectionTier] or [RelayDelayFloor]), the uniform-D "
	"advisory and the applied delay; Warning for the transition line (a token, then its tag), the received floor's clamp, "
	"the hiccup advisory, [ClockSource.Window], the dilation window's own [Dilation.Window] line after its token, and "
	"[Dilation.Authority]. Was each line's UE_LOG(LogOGNet, <Level>, ...) in UESimulationSession (og-session-runtime task "
	"24)");

namespace ueSimulationSessionHookNames
{
	struct Names
	{
		int beforeWorldBuild;
		int emplaceSystems;
		int neutralInput;
		int installGameLoggers;
		int afterSessionConfigured;
		int backendFingerprint;
		int rootBodyId;
		int afterBodiesCreated;
		int afterRegistered;
		int onUnregistered;
		int authorityPresentationSlice_Step;
		int presentAuthoritySlice;
	};

	template <typename SessionT>
	struct Probe : SessionT, Names {};

	template <typename SessionT>
	concept SharesNoHookName = requires
	{
		&Probe<SessionT>::beforeWorldBuild;
		&Probe<SessionT>::emplaceSystems;
		&Probe<SessionT>::neutralInput;
		&Probe<SessionT>::installGameLoggers;
		&Probe<SessionT>::afterSessionConfigured;
		&Probe<SessionT>::backendFingerprint;
		&Probe<SessionT>::rootBodyId;
		&Probe<SessionT>::afterBodiesCreated;
		&Probe<SessionT>::afterRegistered;
		&Probe<SessionT>::onUnregistered;
		&Probe<SessionT>::authorityPresentationSlice_Step;
		&Probe<SessionT>::presentAuthoritySlice;
	};
}

template <typename TraitsT>
class UESimulationSession
{
public:
	using SessionOwner             = typename TraitsT::SessionOwner;
	using Game                     = typename TraitsT::Game;
	using LatencyBudgetDiagnostics = UELatencyBudgetDiagnostics<TraitsT>;

	UESimulationSession(SessionOwner& owner, Game& game);
	~UESimulationSession();

	UESimulationSession(const UESimulationSession&) = delete;
	UESimulationSession& operator=(const UESimulationSession&) = delete;

	SessionOwner&       owner()       { return m_owner; }
	const SessionOwner& owner() const { return m_owner; }

	const LatencyBudgetDiagnostics& getLatencyBudgetDiagnostics() const { return m_latencyBudgetDiagnostics; }
	LatencyBudgetDiagnostics&       editLatencyBudgetDiagnostics()       { return m_latencyBudgetDiagnostics; }

	void beginPlay();

	void endPlay();

private:
	friend typename TraitsT::template PhysicsBackend<UESimulationSession>;
	friend typename TraitsT::template TickDriverGame<UESimulationSession>;
	friend LatencyBudgetDiagnostics;

	using Composition     = typename TraitsT::Composition;
	using EngineTraits    = typename TraitsT::EngineTraits;
	using StaticData      = typename TraitsT::StaticData;
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

	using SystemsExec     = apply_t<SystemsExecFor, typename Composition::Systems>;
	using IntegrationExec = apply_t<IntegrationExecFor, Simulatables>;
	using ManagerType     = SimulationManager<IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec,
	                                          Storage, StaticData>;
	using PhysicsBackend  = typename TraitsT::template PhysicsBackend<UESimulationSession>;
	using Simulatable     = typename TraitsT::Simulatable;
	using PlayerInput     = typename TraitsT::PlayerInput;
	using NetConfig       = typename TraitsT::NetConfig;
	using InputProvider   = typename LatencyBudgetDiagnostics::InputProvider;
	using SimNetLanes      = UESimNetLanes<TraitsT>;
	using SimNetServerHost = typename SimNetLanes::ServerHost;
	using SimNetServer     = ogsimnet::SimNetServer<Composition, SimNetServerHost, typename SimNetLanes::Transport>;
	using SimChannel       = typename SimNetServer::template Channel<Simulatable>;
	using Clock            = ogsession::SessionClock<ManagerType, InputResolution, UESessionClockEngine>;

	struct ClockLink
	{
		const SimNetLanes* lanes = nullptr;

		double ackSrttSeconds() const { return lanes != nullptr ? lanes->ackSrttSeconds() : kNoRttReadingSeconds; }

		ogsimnet::SimNetClockStats clockStats() const
		{
			const UESimNetClockClient* const clock = lanes != nullptr ? lanes->clockClient() : nullptr;
			return clock != nullptr ? clock->stats() : ogsimnet::SimNetClockStats{};
		}
	};

	static_assert(ogsession::SessionClockLink<ClockLink> && ogsession::SessionClockServer<UESimNetClockServer>,
		"UESimulationSession hands its clock the lanes' ack round trip and SimClock counts (ClockLink, for the "
		"[ClockSource.Window] line) and og-sim-net's SimClock server (publishServerTick(tick, tickStartSeconds))");

public:
	const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
	ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
	const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
	bool runsPrediction() const { return m_manager->runsPrediction(); }

	typename EngineTraits::VizQuery&        editVizQuery();
	const typename EngineTraits::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);
	bool hasGameThreadVizWorld() const { return m_manager.has_value() && runsPrediction(); }

	using DisplaySnapshot = SimulationDisplaySnapshotT<Simulatable, kMaxSimulatableSlots, kSessionDisplayWindowTicks,
	                                                   kSessionDisplayWindowTicks>;
	using DisplayEntry    = typename DisplaySnapshot::Entry;
	using DisplayVizState = typename DisplayEntry::VizState;
	using Display         = ogsession::SessionDisplay<Game, DisplaySnapshot, ManagerType, Storage, InputResolution,
	                                                  Reconciliation>;

	const DisplaySnapshot* getDisplaySnapshot() const { return m_display.has_value() ? m_display->snapshot() : nullptr; }

	const DisplayEntry* findDisplayEntry(unsigned int id) const
	{
		return m_display.has_value() ? m_display->findEntry(id) : nullptr;
	}

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

	template <typename SliceT>
	std::optional<SliceT> getVizSlice(unsigned int id) const
	{
		const DisplayVizState* const vizState = findDisplayVizState(id);
		if (vizState == nullptr)
			return std::nullopt;

		// ⛔G-02  docs/UESimulationSession-guards.md
		return vizState->getState().template get<SliceT>();
	}

	const TimeConfig* getTimeConfigPtr() const
	{
		return m_manager.has_value() ? &m_manager->getTimeConfig() : nullptr;
	}

	const std::optional<ManagerType>& getManager() const { return m_manager; }
	const std::optional<SystemsExec>& getSystemsExec() const { return m_systemsExec; }

	const std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>&
	getRegisteredComponentsById() const { return m_registeredComponentsById; }

	// ⛔G-11  docs/UESimulationSession-guards.md
	static UESimulationSession* instanceFor(const UWorld* world, bool isAuthority)
	{
		for (const std::atomic<UESimulationSession*>& slot : s_instances)
		{
			UESimulationSession* const session = slot.load(std::memory_order_acquire);
			if (session != nullptr && session->m_registeredWorld == world && session->m_registeredAsAuthority == isAuthority)
				return session;
		}
		return nullptr;
	}

	void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
	{
		m_onTimingInfoReceivedCallback = std::move(fn);
	}

	void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime)
	{
		m_latencyBudgetDiagnostics.noteWireSample_Internal(roundTripTime);
		if (m_onTimingInfoReceivedCallback)
		{
			m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
			return;
		}
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

	int32 getClientEffectiveInputDelayTicks() const
	{
		return m_inputResolution.getClientEffectiveInputDelayTicks();
	}

	void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
	{
		if (m_clock.has_value())
			m_clock->onConnectionTier(oldTier, newTier);
	}

	const ReplicatedTierConsumer* getReplicatedTierConsumer() const
	{
		return m_clock.has_value() ? m_clock->tierConsumer() : nullptr;
	}

	void onRelayDelayFloorReceived(uint8_t floorTicks)
	{
		if (m_clock.has_value())
			m_clock->onRelayFloor(floorTicks);
	}

	using ReceptionCoordinator = typename SimNetServer::Coordinator;

	ReceptionCoordinator* getReceptionCoordinator()
	{
		return m_simNetServer.has_value() ? &m_simNetServer->coordinator() : nullptr;
	}

	int32 getServerReceptionTick() const
	{
		return static_cast<int32>(getServerClock().getSimulationStep().getTick());
	}

	TryRegisterStatus tryRegister(unsigned int id, Simulatable&& simulatable, typename TraitsT::Component& owner,
		InputProvider&& inputProvider, bool isAuthority);

	void unregisterFromNewFramework(unsigned int id, typename TraitsT::Component& owner, bool isAuthority);

	ClientBindStep clientBindStep(unsigned int id, bool engineSaysLocal);

	void onSimNetLifecycleUnregistering(uint8_t simId);

private:
	SimNetLanes* findDriverSimNetLanes(UWorld& world, ognet::Role role);

	void beginServerSimNet(UWorld& world);

	IdleWake idleUntil_Runner(double deadlineSeconds);

	void beforePump_Owner(double hostNowSeconds, TickRateDoor& rate);

	void afterStep_Step()
	{
		if (m_clock.has_value())
			m_clock->publishServerTick(m_clockServer);
	}

	void stampCorrectionsHanded_Runner();

	void publishAuthorityPresentation_Runner()
	{
		static_assert(requires(const SystemsExec& systems, const UESimulationSession& session, unsigned int id,
				const AuthorityPresentationSlice& slice)
			{
				{ Game::authorityPresentationSlice_Step(systems, id) } -> std::same_as<AuthorityPresentationSlice>;
				Game::presentAuthoritySlice(session, id, slice);
			},
			"UESimulationSession<TraitsT>: the game's presentation of authority-only state is "
			"two static hooks of TraitsT::Game over its AuthorityPresentationSlice: authorityPresentationSlice_Step(systems, id), "
			"read on the runner after each batch, and presentAuthoritySlice(session, id, slice), applied on the game thread "
			"from the newest published snapshot. Was the loop's skip of afterVisualization, whose ring-out score push read the "
			"score system on the game thread (og-input-latency P5-19 F5)");
		checkf(m_systemsExec.has_value(),
			TEXT("UESimulationSession::publishAuthorityPresentation_Runner: the authority's systems executor is built by the "
				 "session's configuration, which the runner runs before its first batch"));
		m_display->publishPresentation(*m_systemsExec);
	}

	void presentAuthority_GameThread()
	{
		m_display->presentLatest([this](unsigned int id, const AuthorityPresentationSlice& slice)
		{
			Game::presentAuthoritySlice(*this, id, slice);
		});
	}

	static uint32 correctionTickOf(const SimChannel& channel);

	void postRunnerRequest(TUniqueFunction<void()> request);

	TryRegisterStatus tryRegisterOnRunner(unsigned int id, typename TraitsT::Component& owner);

	bool bodiesResolvable_Runner(const Simulatable& simulatable) const;

	void onRunnerRegisterApplied_Runner(unsigned int id, uint64 key, SimChannel* channel);

	void requestRunnerUnregister_GameThread(unsigned int id, typename TraitsT::Component* owner);

	void releaseOnRunner_Runner(unsigned int id, uint64 key);

	void onAuthorityRunnerJoined_GameThread();

	void beginClientSimNet(UWorld& world);

	void logSimNetHostLine(bool isServer) const;

	bool refuseToStart(UWorld& world, ENetMode netMode);

	bool refuseNetModeOrDriver(UWorld& world, ENetMode netMode);

	bool refuseRetiredKnobs(UWorld& world, ENetMode netMode);

	bool refuseSecondSessionOfRole(UWorld& world, ENetMode netMode);

	void refuseRegistrationWithoutSession(unsigned int id);

	typename SimNetLanes::Client* boundImmediateClient_Step() const;

	void afterClientCapture_Step();

	void receiveForClientStep_Step();

	void publishDisplaySnapshot_Step() { m_display->publishAfterBatch(); }

	void applyUnregisterRequests_Step(typename SimNetLanes::Client& client);

	void enqueueQueuedUnregister_Step(typename SimNetLanes::Client& client, unsigned int id);

	void onQueuedRegisterApplied_Step(unsigned int id, SimChannel* channel, bool hasProvider);

	TryRegisterStatus tryRegisterQueued(unsigned int id, typename TraitsT::Component& owner);

	TryRegisterStatus finishQueuedRegistration(unsigned int id, typename TraitsT::Component& owner);

	void requestQueuedUnregister_GameThread(unsigned int id, typename TraitsT::Component& owner);

	void unregisterCharacterHostOnly(unsigned int id, typename TraitsT::Component* owner, bool releaseBodiesNow);

	void collectLifecycleOutcomes_GameThread();

	void teardownImmediateClient_GameThread();

	void onSimNetLanesStopping(bool isServer);

	void detachFromSimNetLanes();

	void forgetSimNetCharacter(unsigned int id, const AActor* pawn);

	void onSimNetRegisteredRemoved(uint8_t simId);

	void onFrameStepsDue_Step(int32 firstUpcomingSimTick, int32 numSteps);

	void onPostPhysicsStep();

	void refreshDisplayView_GameThread()
	{
		m_display->acquireForFrame([this](unsigned int id, DisplayVizState& vizState)
		{
			m_physicsBackend.overlayRenderPoses_GameThread(id, vizState);
		});
	}

	void noteAuthorityStepTime_Step()
	{
		if (m_clock.has_value())
			m_clock->noteAuthorityStepStart();
	}

	void releaseDelayedInputsForStep(int32 firstTick, int32 numSteps);

	void afterAuthorityBatch(int32 firstUpcomingSimTick);

	std::optional<uint32> physicsSlotFor(AActor& owner, uint64 key, unsigned int storageKey) const;

	void recordAuthoritativeSlot(AActor& owner, uint64 key, uint32 slot);

	static void bindCorrectionFieldDiffGate()
	{
		// ⛔G-22  docs/UESimulationSession-guards.md
		correctionFieldDiff::setEnabledPredicate(
			[]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
		static_assert(ogLogRouteFor(TEXT("[DivergenceProbe.Margin]")) != nullptr
			&& ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))->route == OGLogRoute::DivergenceProbe
			&& ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))->plainLevel == ELogVerbosity::Log,
			"bindCorrectionFieldDiffGate: the margin predicate below asks LogOGDivergenceProbe at Log, so the "
			"[DivergenceProbe.Margin] line must route to OGLogRoute::DivergenceProbe at plain level Log. Re-route "
			"or re-level the [DivergenceProbe prefix and move the predicate with it (og-input-latency task 2h)");
		// ⛔G-28  docs/UESimulationSession-guards.md
		correctionVerdictMargin::setEnabledPredicate(
			[]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Log); });
	}

private:
	SessionOwner& m_owner;
	Game&         m_game;

	LatencyBudgetDiagnostics m_latencyBudgetDiagnostics{ std::as_const(*this) };

	typename TraitsT::PhysicsBackendWorlds m_physicsBackendWorlds;

	std::optional<typename EngineTraits::BodyAdapter>   m_physAdapter;
	std::optional<typename EngineTraits::ReaderAdapter> m_physReaderAdapter;
	std::optional<typename EngineTraits::QueryAdapter>  m_queryAdapter;

	Storage m_storage;

	StaticData m_staticData{ Game::makeStaticData() };

	Reconciliation  m_reconciliation{ m_storage };
	InputResolution m_inputResolution{ m_storage, m_reconciliation };
	NetSync         m_netSync{ m_storage, m_reconciliation, m_inputResolution };

	// ⛔G-01  docs/UESimulationSession-guards.md
	std::optional<SystemsExec> m_systemsExec;

	std::optional<IntegrationExec> m_integrationLayer;

	std::optional<ManagerType> m_manager;
	PhysicsBackend m_physicsBackend{ *this, m_physicsBackendWorlds };

	UESessionClockEngine m_clockEngine;
	std::optional<Clock> m_clock;
	std::optional<Display> m_display;

	SimNetLanes* m_simNetLanes = nullptr;
	std::optional<SimNetServerHost> m_simNetServerHost;
	std::optional<SimNetServer> m_simNetServer;
	std::optional<ogsimnet::SimNetIdle<ognetunreal::SimLaneEndpoint>> m_simNetIdle;
	UESimNetClockServer* m_clockServer = nullptr;
	bool m_noSessionRegisterRefused = false;
	const char* m_noSessionReason = "no-session";
	bool m_noSessionRegisterRaisesMessage = false;
	bool m_runnerStopped = false;
	std::unordered_map<unsigned int, uint64> m_runnerBodyKeysById;
	uint64 m_authorityWakes_Runner = 0u;

	using AuthorityPresentationSlice = typename Game::AuthorityPresentationSlice;

	FrameHealthProbe m_frameHealthProbe;

	std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>
		m_registeredComponentsById;
	std::unordered_map<const AActor*, uint32> m_authoritativeSlotsByActor;
	std::unordered_set<unsigned int> m_simUnregisteredIds;

	std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

	inline static constexpr size_t kInstanceSlots = 128u;
	inline static std::atomic<UESimulationSession*> s_instances[kInstanceSlots] = {};
	inline static UESimulationSession* s_startedSessions[2] = { nullptr, nullptr };
	const UWorld* m_registeredWorld = nullptr;
	bool m_registeredAsAuthority = false;

	void registerInstance();
	void unregisterInstance();

	struct PendingRegistration
	{
		std::optional<Simulatable> simulatable;
		bool bodiesCreated = false;
		InputProvider inputProvider;
		bool isAuthority = false;
		bool queued = false;
		bool applied = false;
		bool refused = false;
		bool rosterRemoved = false;
		bool onRunner = false;
		bool runnerBusy = false;
		bool bodiesBound = false;
		bool registerPosted = false;
		uint32 boundSlot = 0u;
		uint64 bodyKey = 0u;
		std::shared_ptr<Simulatable> runnerSimulatable;
	};
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;

	enum class LifecycleOutcomeKind : uint8
	{
		RegisterApplied,
		RegisterRefused,
		UnregisterApplied,
		RosterRemoved,
		BodiesBound,
		BodiesPending,
		BodiesUnresolvable
	};

	struct LifecycleOutcome
	{
		unsigned int         id = 0u;
		LifecycleOutcomeKind kind = LifecycleOutcomeKind::RegisterApplied;
		uint32               slot = 0u;
	};

	void postLifecycleOutcome(unsigned int id, LifecycleOutcomeKind kind, uint32 slot = 0u)
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		m_lifecycleOutcomes.push_back(LifecycleOutcome{ id, kind, slot });
	}

	bool m_immediateStepsStopped = false;
	std::mutex m_lifecycleBridgeMutex;
	std::vector<unsigned int> m_unregisterRequests;
	std::vector<LifecycleOutcome> m_lifecycleOutcomes;
	std::vector<TUniqueFunction<void()>> m_runnerRequests;
	std::unordered_set<unsigned int> m_systemsNotified_Step;
	std::unordered_set<unsigned int> m_unregisterRequested_Step;
	std::unordered_map<unsigned int, TStrongObjectPtr<typename TraitsT::Component>> m_pendingUnregisters;
	std::unordered_map<unsigned int, TStrongObjectPtr<typename TraitsT::Component>> m_componentsHeldForRunner;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(UESimulationSession, m_storage)
		                  < __builtin_offsetof(UESimulationSession, m_staticData)
		           && __builtin_offsetof(UESimulationSession, m_staticData)
		                  < __builtin_offsetof(UESimulationSession, m_reconciliation)
		           && __builtin_offsetof(UESimulationSession, m_reconciliation)
		                  < __builtin_offsetof(UESimulationSession, m_inputResolution)
		           && __builtin_offsetof(UESimulationSession, m_inputResolution)
		                  < __builtin_offsetof(UESimulationSession, m_netSync),
			"UESimulationSession members construct in DECLARATION order, and three of these read a sibling declared above "
			"them in their own initializer: m_reconciliation binds m_storage; m_inputResolution binds m_storage and "
			"m_reconciliation; m_netSync binds all three peers above it. m_staticData's initializer, the game's "
			"Game::makeStaticData() (its one-time read of its static-data console variables), runs after m_storage is "
			"built and before the peers, the order it ran in when the owner held these members. A reorder compiles (no "
			"-Wreorder-as-error in this tree) and constructs in the new, wrong order. Keep m_storage < m_staticData < "
			"m_reconciliation < m_inputResolution < m_netSync. Was the owner's construction-order row, the CONSTRUCTION "
			"ORDER prose fence of its header before that (task 11), with the owner's cvar member folded into "
			"Game::makeStaticData; moved here by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(std::is_same_v<Storage, typename Composition::Storage>,
			"UESimulationSession requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Storage must name the same type as the session's Storage, "
			"apply_t<SimulationObjectStorage, Simulatables>.");
		static_assert(std::is_same_v<Reconciliation, typename Composition::Reconciliation>,
			"UESimulationSession requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Reconciliation must name the same type as the session's Reconciliation, "
			"apply_t<SimulationReconciliation, Simulatables>.");
		static_assert(std::is_same_v<InputResolution, typename Composition::InputResolution>,
			"UESimulationSession requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::InputResolution must name the same type as the session's InputResolution, "
			"apply_t<SimulationInputResolution, Simulatables>.");
		static_assert(std::is_same_v<NetSync, typename Composition::NetSync>,
			"UESimulationSession requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::NetSync must name the same type as the session's NetSync, "
			"apply_t<SimulationNetSync, Simulatables>.");
		static_assert(std::is_same_v<ManagerType, typename Composition::Manager>,
			"UESimulationSession requires og-simulation's own core systems (user ruling 2026-10-24), so "
			"TraitsT::Composition::Manager must name the same type as the session's ManagerType, og-simulation's "
			"SimulationManager over the session's IntegrationExec, NetSync, InputResolution, Reconciliation, SystemsExec, "
			"Storage and StaticData.");
		static_assert(std::is_same_v<IntegrationExec, typename Composition::IntegrationExec>,
			"UESimulationSession requires og-simulation's own integration executor (user ruling 2026-10-24), so "
			"TraitsT::Composition::IntegrationExec must name the same type as the session's IntegrationExec, "
			"SimulationIntegrationExecutor<StaticData, EngineTraits::BodyAdapter, EngineTraits::QueryAdapter, Simulatables...>: "
			"a game chooses its static data and its engine through the traits, not the executor.");
		static_assert(std::is_same_v<SystemsExec, typename Composition::SystemsExec>,
			"UESimulationSession requires og-simulation's own systems executor (user ruling 2026-10-24), so "
			"TraitsT::Composition::SystemsExec must name the same type as the session's SystemsExec, "
			"SimulationSystemsExecutor<Simulatables, StaticData, Systems...> over TraitsT::Composition::Systems in that "
			"list's order: a game chooses its systems and their order through the list, and the firing order is the "
			"template order.");
		static_assert(PhysicsEngineTraits<EngineTraits>,
			"TraitsT::EngineTraits must model og-simulation's PhysicsEngineTraits (PhysicsEngineTraits.h): the session and "
			"its integration executor read the engine's adapter types, name and visualization types through it. The engine "
			"is the game's choice, made in its traits (user ruling 2026-10-24).");
		static_assert(std::is_same_v<ManagerType, typename TraitsT::template TickDriverGame<UESimulationSession>::Manager>,
			"UESimulationSession::ManagerType must BE the manager type the tick-driver game binding names "
			"(TraitsT::TickDriverGame<UESimulationSession>::Manager). ManagerType is private, so a backend host declared "
			"outside this class (the tick-driver backend, whose step driver is SimulationStepDriver<Manager, ...>) names "
			"the manager through the game binding; a ManagerType that drifts from it hands that host a different "
			"SimulationManager instantiation than m_manager. Was the owner's ManagerType row (added by "
			"og-simulationscheduler-withjolt task 81), which compared it with the game's composition struct; moved here "
			"by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(__builtin_offsetof(UESimulationSession, m_physicsBackendWorlds)
		                  < __builtin_offsetof(UESimulationSession, m_physAdapter)
		           && __builtin_offsetof(UESimulationSession, m_physAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_physReaderAdapter)
		           && __builtin_offsetof(UESimulationSession, m_physAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_queryAdapter)
		           && __builtin_offsetof(UESimulationSession, m_queryAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_integrationLayer),
			"UESimulationSession members destruct in REVERSE declaration order. The backend's worlds must outlive every "
			"adapter bound to them (the Jolt step world; the reader holds the body adapter's bind table, the integration "
			"layer holds the adapters), so m_physicsBackendWorlds sits above the adapters in every configuration (it is empty "
			"on a self-scheduled engine). Keep m_physicsBackendWorlds < m_physAdapter < {m_physReaderAdapter, m_queryAdapter} < "
			"m_integrationLayer. Was the owner's destruction-order row, design D's compile-time row 'runtime < world < "
			"adapters' before that (og-simulationscheduler-withjolt task 18); its runtime < world half is "
			"og-simulation-jolt's JoltTickWorlds' own row (runtime < contactCounter < stepWorld) since og-tickdriver "
			"task 10. Moved here by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(__builtin_offsetof(UESimulationSession, m_manager)
		                  < __builtin_offsetof(UESimulationSession, m_physicsBackend),
			"UESimulationSession members destruct in REVERSE declaration order. The backend holds the Jolt step driver, "
			"which unregisters its resync callback from the manager's client clock in its destructor, so the backend must "
			"go before the manager. Keep m_manager < m_physicsBackend. Was the owner's row, design D's compile-time row 'manager "
			"< hooks < driver' before that (og-simulationscheduler-withjolt task 18); its hooks < driver half is "
			"og-simulation's SimulationTickDriver's own row, and the tick driver last is TickDriverBackendUImpl's, since "
			"og-tickdriver task 8. Moved here by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(__builtin_offsetof(UESimulationSession, m_manager) < __builtin_offsetof(UESimulationSession, m_clock)
		           && __builtin_offsetof(UESimulationSession, m_inputResolution) < __builtin_offsetof(UESimulationSession, m_clock)
		           && __builtin_offsetof(UESimulationSession, m_clockEngine) < __builtin_offsetof(UESimulationSession, m_clock),
			"UESimulationSession members destruct in REVERSE declaration order. The clock (og-sim-session's SessionClock) keeps "
			"references to the manager, the input-resolution peer and its engine, and its tier consumer is emplaced from the "
			"manager's TimeConfig and keeps a reference to it (ReplicatedTierConsumer borrows its config), so the clock must be "
			"destroyed before all three. Keep m_manager, m_inputResolution and m_clockEngine < m_clock. Was the "
			"m_manager < m_replicatedTierConsumer row (og-ue-simulationcomponentandmanagersplit task 25: in the owner the "
			"consumer was declared above the member that holds the manager, and only the owner's EndPlay reset kept it from "
			"outliving that TimeConfig); moved onto the clock with the consumer by og-session-runtime task 24.");
		static_assert(__builtin_offsetof(UESimulationSession, m_storage) < __builtin_offsetof(UESimulationSession, m_display)
		           && __builtin_offsetof(UESimulationSession, m_reconciliation) < __builtin_offsetof(UESimulationSession, m_display)
		           && __builtin_offsetof(UESimulationSession, m_inputResolution) < __builtin_offsetof(UESimulationSession, m_display)
		           && __builtin_offsetof(UESimulationSession, m_manager) < __builtin_offsetof(UESimulationSession, m_display)
		           && __builtin_offsetof(UESimulationSession, m_clock) < __builtin_offsetof(UESimulationSession, m_display),
			"UESimulationSession members destruct in REVERSE declaration order. The display (og-sim-session's SessionDisplay) "
			"keeps references to the storage, the reconciliation, the input resolution and the manager it fills from, and to "
			"the clock's display-tier buffer, so it must be destroyed before all five. Keep m_storage, m_reconciliation, "
			"m_inputResolution, m_manager and m_clock < m_display (og-session-runtime task 25: the display's members were the "
			"session's own until then, built with it)");
		static_assert(__builtin_offsetof(UESimulationSession, m_manager)
		                  < __builtin_offsetof(UESimulationSession, m_simNetServer)
		           && __builtin_offsetof(UESimulationSession, m_simNetServerHost)
		                  < __builtin_offsetof(UESimulationSession, m_simNetServer),
			"UESimulationSession members destruct in REVERSE declaration order. og-sim-net's server keeps a reference to the "
			"manager's TimeConfig (and its reception coordinator borrows it too) and to its host, so the server is destroyed "
			"before the manager and its host. Keep m_manager < m_simNetServer and m_simNetServerHost < m_simNetServer. Was the "
			"m_manager < m_receptionCoordinator row (task 26a); the coordinator is the server's own since og-input-latency task "
			"P5-7. The session's own detached lanes left this row with them (og-session-runtime task 14).");
		static_assert(latencyKnobs::kMaxRelayResendEntries == relayedInputRing::kMaxResendEntries,
			"TimeConfig::relayResendEntries' clamp and the relay codec's re-send bound must agree, or a session value the knob "
			"accepts would be cut again at og-sim-net's relay flush. og-input-latency task 8; was beside the input relay's spawn "
			"in UESimulatableOwnerBinding until task P5-7");
		static_assert(std::is_same_v<PredictionOwnerFor<Simulatable>, SimChannel>
		           && std::is_same_v<AuthorityOwnerFor<Simulatable>, SimChannel>
		           && std::is_same_v<SimChannel, typename SimNetLanes::Client::Channel>
		           && std::is_same_v<SimChannel, UESimNetChannel<Simulatable>>,
			"SimulatableOwnerTraits<TraitsT::Simulatable> must name og-sim-net's one channel type, UESimNetChannel<Simulatable>, "
			"for both roles: the server registers a character with its SimNetServer channel as both owners and a client with its "
			"SimNetClient channel, and those are one type (SessionChannel over the lanes' transport). Was the component binding "
			"as both owners, which the session's two sink assertions (RemoteInputDeliverySink, RemoteInputRelaySink) served "
			"until og-input-latency task P5-7.");
		static_assert(std::is_same_v<decltype(&UESimulationSession::getTimeConfigPtr),
				const TimeConfig* (UESimulationSession::*)() const>,
			"UESimulationSession::getTimeConfigPtr returns a POINTER so that the state before the manager is built - no "
			"manager, therefore no TimeConfig - is a representable nullptr rather than a reference into an empty "
			"optional. Callers bind lazily and retry. Was the static_assert on the owner's getTimeConfigPtr after the "
			"owner's class, itself the 'Pointer, so pre-construction is not UB' prose fence of the owner's header "
			"(og-netcode-v2-field-defects task 11); re-expressed on the session by "
			"og-ue-simulationcomponentandmanagersplit task 24b.");
		static_assert(ueSimulationSessionHookNames::SharesNoHookName<UESimulationSession>,
			"No member of UESimulationSession, public or private, own or inherited, may share a name with one of the ten "
			"phase hooks of TraitsT::Game it calls: beforeWorldBuild, emplaceSystems, neutralInput, installGameLoggers, "
			"afterSessionConfigured and backendFingerprint (beginPlay), rootBodyId, afterBodiesCreated and afterRegistered "
			"(tryRegister), onUnregistered (unregisterFromNewFramework). The session calls every hook qualified, as "
			"Game::hook or m_game.hook, so a clash changes no call; it makes a reader take the game's statement for the "
			"session's own. Was a sentence of beginPlay's hook check (task 27b) that nothing enforced; made a compile-time "
			"check, over every member, by og-ue-simulationcomponentandmanagersplit task 28.");
		return true;
	}
};

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
UESimulationSession<TraitsT>::UESimulationSession(SessionOwner& owner, Game& game)
	: m_owner(owner)
	, m_game(game)
{
	static_assert(compositionContractsHold());
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::editVizQuery() -> typename EngineTraits::VizQuery&
{
	return m_physicsBackend.editVizQuery();
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::getVizReader() const -> const typename EngineTraits::VizReader&
{
	return m_physicsBackend.getVizReader();
}

template <typename TraitsT>
std::optional<QueryVolumeId> UESimulationSession<TraitsT>::registerVizVolume(const QueryVolumeDescriptor& descriptor,
	AActor& owner)
{
	return m_physicsBackend.registerVizVolume(descriptor, owner);
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
UESimulationSession<TraitsT>::~UESimulationSession()
{
	// ⛔G-10  docs/UESimulationSession-guards.md
	unregisterInstance();
	detachFromSimNetLanes();
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::findDriverSimNetLanes(UWorld& world, ognet::Role role) -> SimNetLanes*
{
	UOGNetDriver* const driver = Cast<UOGNetDriver>(world.GetNetDriver());
	IOGNetSimLanes* const lanes = driver != nullptr ? driver->GetSimLanes() : nullptr;
	if (lanes == nullptr)
		return nullptr;
	SimNetLanes* const simNetLanes = static_cast<SimNetLanes*>(lanes);
	checkf(simNetLanes->isServer() == (role == ognet::Role::Server),
		TEXT("UESimulationSession: the og-net driver's sim lanes were made for the other role. The game's factory makes them "
			 "for the driver's role, which is this world's"));
	return simNetLanes;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::logSimNetHostLine(bool isServer) const
{
	char line[kSimNetHostLineBytes];
	std::snprintf(line, sizeof(line), kSimNetHostLineFormat, isServer ? "server" : "client", /*immediate=*/1, "OGNet",
		static_cast<unsigned>(kUESimNetInboxSlots));
	RouteOGMessage(line);
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::refuseToStart(UWorld& world, ENetMode netMode)
{
	m_registeredWorld = &world;
	m_registeredAsAuthority = netMode != NM_Client;
	if (!refuseNetModeOrDriver(world, netMode) && !refuseRetiredKnobs(world, netMode)
		&& !refuseSecondSessionOfRole(world, netMode))
	{
		s_startedSessions[m_registeredAsAuthority ? 0 : 1] = this;
		return false;
	}
	registerInstance();
	ueSessionRefusal::requestServerExit();
	return true;
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::refuseSecondSessionOfRole(UWorld& world, ENetMode netMode)
{
	if (s_startedSessions[m_registeredAsAuthority ? 0 : 1] == nullptr)
		return false;
	ueSessionRefusal::refuse(m_registeredAsAuthority ? "role=authority netMode=DedicatedServer" : "role=client netMode=Client",
		world.IsPlayInEditor() ? "pie" : "-", "one-session-per-role-per-process");
	m_noSessionReason = "one-session-per-role-per-process";
	return true;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::registerInstance()
{
	for (std::atomic<UESimulationSession*>& slot : s_instances)
	{
		UESimulationSession* expected = nullptr;
		if (slot.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
			return;
	}
	checkf(false, TEXT("UESimulationSession::registerInstance: all %d registry slots are taken, one per world with a session ")
		TEXT("owner in this process"), static_cast<int32>(kInstanceSlots));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterInstance()
{
	for (std::atomic<UESimulationSession*>& slot : s_instances)
	{
		UESimulationSession* expected = this;
		slot.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
	}
	for (UESimulationSession*& started : s_startedSessions)
	{
		if (started == this)
			started = nullptr;
	}
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::refuseNetModeOrDriver(UWorld& world, ENetMode netMode)
{
	if (netMode == NM_Standalone)
	{
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Session] none netMode=Standalone"));
		m_noSessionReason = "dedicated-server-only";
		m_noSessionRegisterRaisesMessage = true;
		return true;
	}
	if (netMode == NM_ListenServer)
	{
		ueSessionRefusal::refuse("role=authority netMode=ListenServer", "-", "dedicated-server-only");
		m_noSessionReason = "dedicated-server-only";
		return true;
	}
	const bool isServer = netMode != NM_Client;
	if (findDriverSimNetLanes(world, isServer ? ognet::Role::Server : ognet::Role::Client) != nullptr)
		return false;
	const UNetDriver* const driver = world.GetNetDriver();
	UE_LOG(LogOGNet, Error, TEXT("[SimNet.Host] refused role=%s driver=%s: simulation traffic needs the og-net driver"),
		isServer ? TEXT("server") : TEXT("client"), driver != nullptr ? *driver->GetClass()->GetName() : TEXT("none"));
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
			isServer ? TEXT("Simulation traffic needs the og-net driver: this server runs no simulation session")
			         : TEXT("Simulation traffic needs the og-net driver: this client simulates nothing"));
	}
	return true;
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::refuseRetiredKnobs(UWorld& world, ENetMode netMode)
{
	const int32 runInline = UETickDriverHost::retiredRunInlineValue();
	if (runInline != 0)
	{
		char value[16];
		std::snprintf(value, sizeof(value), "%d", static_cast<int>(runInline));
		ueSessionRefusal::refuse("knob=og.Sim.RunInline", value, "retired");
		return true;
	}
	if (netMode == NM_Client)
	{
		if (!findDriverSimNetLanes(world, ognet::Role::Client)->simTrafficImmediateRefused())
			return false;
		ueSessionRefusal::refuse("knob=SimTrafficImmediate", "0", "retired");
		return true;
	}
	if (retiredAuthorityLoopValue() != 0)
		return false;
	ueSessionRefusal::refuse("knob=og.Sim.AuthorityLoop", "0", "retired");
	return true;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::refuseRegistrationWithoutSession(unsigned int id)
{
	if (std::exchange(m_noSessionRegisterRefused, true))
		return;
	char subject[32];
	std::snprintf(subject, sizeof(subject), "register id=%u", id);
	if (m_noSessionRegisterRaisesMessage)
		ueSessionRefusal::refuse(subject, "-", m_noSessionReason);
	else
		ueSessionRefusal::logRefusal(subject, "-", m_noSessionReason);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginServerSimNet(UWorld& world)
{
	SimNetLanes* const lanes = findDriverSimNetLanes(world, ognet::Role::Server);
	checkf(lanes != nullptr && lanes->hostServiced(),
		TEXT("UESimulationSession::beginServerSimNet: refuseToStart starts an authority session only on a dedicated server "
			 "with the og-net driver, whose lanes are host-serviced. Was the detached server and the in-frame server's "
			 "frame-point lanes (og-session-runtime task 14)"));
	m_simNetLanes = lanes;

	const ognet::EndpointConfig& config = lanes->endpoint().config();
	m_simNetServerHost.emplace(lanes->driver(), lanes->endpoint(), this);
	m_simNetServer.emplace(*m_simNetServerHost, lanes->transport(), m_manager->getTimeConfig(),
		ogsimnet::correctionAllowanceBytes(config.lanes, config.budget, static_cast<double>(NetConfig::tickFrequencyHz)));
	m_simNetServer->bindLifecycle(m_storage, m_reconciliation, m_inputResolution, m_netSync);
	m_clockServer = lanes->clockServer();
	m_simNetIdle.emplace(lanes->endpoint());
	lanes->bindAuthorityRunner([this]() { m_simNetServerHost->publishRunnerRtt_GameThread(); },
		[this]() { onAuthorityRunnerJoined_GameThread(); });
	lanes->setStopHandler([this]() { onSimNetLanesStopping(/*isServer=*/true); });
	logSimNetHostLine(/*isServer=*/true);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onAuthorityRunnerJoined_GameThread()
{
	m_physicsBackend.stopAuthorityLoop_GameThread();
	m_runnerStopped = true;
}

template <typename TraitsT>
IdleWake UESimulationSession<TraitsT>::idleUntil_Runner(double deadlineSeconds)
{
	return m_simNetIdle.has_value() ? m_simNetIdle->idleUntil(deadlineSeconds) : IdleWake::Unavailable;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::postRunnerRequest(TUniqueFunction<void()> request)
{
	const std::scoped_lock lock(m_lifecycleBridgeMutex);
	m_runnerRequests.push_back(MoveTemp(request));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beforePump_Owner(double hostNowSeconds, TickRateDoor& rate)
{
	if (m_clock.has_value() && m_manager->runsPrediction())
	{
		m_clock->beforePredictionPump(hostNowSeconds, rate);
		return;
	}
	if (m_clock.has_value())
		m_clock->noteAuthorityRateDoor(hostNowSeconds, rate);
	std::vector<TUniqueFunction<void()>> requests;
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		requests.swap(m_runnerRequests);
	}
	for (TUniqueFunction<void()>& request : requests)
		request();
	if (m_simNetServer.has_value())
		m_simNetServer->drain();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetLifecycleUnregistering(uint8_t simId)
{
	const unsigned int id = simId;
	// ⛔G-35  docs/UESimulationSession-guards.md
	if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))
		m_manager->notifyCharacterUnregistered(id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginClientSimNet(UWorld& world)
{
	checkf(m_manager.has_value() && getReplicatedTierConsumer() != nullptr,
		TEXT("UESimulationSession: the client binds the og-net session after the manager and the tier consumer exist: the "
			 "bind replays the latched relay-delay floor (the manager's) and connection tier (the tier consumer's), and either "
			 "apply returns early without its target. Was G-07's bind-then-pull order, on the timing and connection relays"));
	SimNetLanes* const lanes = findDriverSimNetLanes(world, ognet::Role::Client);
	checkf(lanes != nullptr,
		TEXT("UESimulationSession::beginClientSimNet: refuseToStart starts a client session only on the og-net driver. Was the "
			 "detached client's lanes (og-session-runtime task 14)"));
	m_simNetLanes = lanes;
	lanes->setOnRegisteredRemoved([this](uint8_t simId) { onSimNetRegisteredRemoved(simId); });
	lanes->setStopHandler([this]() { onSimNetLanesStopping(/*isServer=*/false); });
	lanes->client()->bindLifecycle(m_storage, m_reconciliation, m_inputResolution, m_netSync);
	lanes->bindClientSession(*this);
	logSimNetHostLine(/*isServer=*/false);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetLanesStopping(bool isServer)
{
	if (!isServer)
	{
		teardownImmediateClient_GameThread();
		checkf(m_registeredComponentsById.empty(),
			TEXT("UESimulationSession::onSimNetLanesStopping: a client's teardown unregisters every character it registered, so "
				 "nothing is left for the lanes' stop to unregister. Was the client arm of the in-frame unregistration "
				 "(og-session-runtime task 14)"));
	}
	std::vector<unsigned int> ids;
	ids.reserve(m_registeredComponentsById.size());
	for (const auto& entry : m_registeredComponentsById)
		ids.push_back(entry.first);
	for (const unsigned int id : ids)
	{
		requestRunnerUnregister_GameThread(id, m_registeredComponentsById[id].Get());
		m_simUnregisteredIds.insert(id);
	}
	m_simNetIdle.reset();
	m_clockServer = nullptr;
	m_simNetServer.reset();
	m_simNetServerHost.reset();
	m_simNetLanes = nullptr;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::detachFromSimNetLanes()
{
	SimNetLanes* const lanes = std::exchange(m_simNetLanes, nullptr);
	if (lanes == nullptr)
		return;
	lanes->setStopHandler(nullptr);
	if (lanes->isServer())
	{
		lanes->unbindServerSession();
		return;
	}
	for (const auto& entry : m_registeredComponentsById)
		lanes->editBindings().markUnregistered(static_cast<uint8_t>(entry.first));
	lanes->setOnRegisteredRemoved(nullptr);
	lanes->unbindClientSession();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onFrameStepsDue_Step(int32 firstUpcomingSimTick, int32 numSteps)
{
    static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples,
        "PROBE A's window must stay the same length as ResimGateProbe's, so one [ResimProbe.Frame] "
        "window lines up against the surrounding [ResimProbe.Gate] windows without a resim-tick "
        "count on the line. Was the COMPARABLE WITH ResimGateProbe caution in PROBE A's banner.");
    {
        const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
        const uint64 probeFrame = isClient ? static_cast<uint64>(GFrameCounter) : ++m_authorityWakes_Runner;
        FrameHealthWindowSummary frameSummary;
        const bool frameWindowClosed = m_frameHealthProbe.noteFrame(
            probeFrame,
            static_cast<uint32>(firstUpcomingSimTick),
            static_cast<uint32>(numSteps),
            static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0),
            frameSummary);

        if (frameWindowClosed)
        {
            char line[256];

            if (isClient)
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
            else
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
        }
    }
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
void UESimulationSession<TraitsT>::releaseDelayedInputsForStep(int32 firstTick, int32 numSteps)
{
    const std::optional<double> stepSeconds = m_clock.has_value() ? m_clock->takeReleaseStepSeconds() : std::nullopt;
    if (!m_simNetServer.has_value())
        return;

    OG_LOG_ROUTED(LogOGNet, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    m_simNetServer->releaseForStep(firstTick, numSteps, stepSeconds);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterAuthorityBatch(int32 firstUpcomingSimTick)
{
    if (!m_simNetServer.has_value())
        return;
    checkf(firstUpcomingSimTick != 0 || m_simNetServer->attachedConnections().empty(),
        TEXT("UESimulationSession::afterAuthorityBatch: the authority loop's first batch reports tick 0 (the driver publishes its "
             "offset after a tick ran), and a client capture reaped against it is refused by the coordinator's domain gate. The "
             "runner services og-net only in its idle, after its first batch, so no connection can be attached yet. Was P2-19 F2"));
    m_manager->onPostSimulationGameThread();
    m_simNetServer->publishClock(static_cast<uint32_t>(getServerClock().getTick()));
    m_simNetServer->afterInlineBatch(firstUpcomingSimTick);
    stampCorrectionsHanded_Runner();
    publishAuthorityPresentation_Runner();
}

template <typename TraitsT>
uint32 UESimulationSession<TraitsT>::correctionTickOf(const SimChannel& channel)
{
    const CorrectionStateBuffer& buffer = channel.getSyncedCorrectionStateBuffer();
    if (buffer.payload().size() < correctionStateBuffer::kHeaderBytes)
        return 0u;
    return buffer.template readFromBuffer<uint32>(correctionStateBuffer::kTickOffset);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::stampCorrectionsHanded_Runner()
{
    const double handedSeconds = FPlatformTime::Seconds();
    m_storage.template forEachSimulatable<Simulatable>([this, handedSeconds](unsigned int id, const auto&)
    {
        SimChannel* const channel =
            id != 0u && id <= 0xFFu ? m_simNetServer->template channel<Simulatable>(static_cast<uint8_t>(id)) : nullptr;
        if (channel == nullptr)
            return;
        const uint32 correctionTick = correctionTickOf(*channel);
        if (correctionTick != 0u)
            m_latencyBudgetDiagnostics.stampCorrectionHanded_Internal(id, correctionTick, handedSeconds);
    });
}

template <typename TraitsT>
std::optional<uint32> UESimulationSession<TraitsT>::physicsSlotFor(AActor& owner, [[maybe_unused]] uint64 key,
    unsigned int storageKey) const
{
    checkf(m_manager.has_value(), TEXT("UESimulationSession::physicsSlotFor before beginPlay built the manager"));
    if (!m_manager->runsPrediction())
    {
        const auto found = m_authoritativeSlotsByActor.find(&owner);
        return found != m_authoritativeSlotsByActor.end() ? std::optional<uint32>(found->second) : std::nullopt;
    }
    if (storageKey == 0u)
        return std::nullopt;
    checkf(storageKey <= 0xFFu,
        TEXT("UESimulationSession::physicsSlotFor: a client's slot comes from the roster, which keys characters by their sim "
             "id (1..255); storage key %u is not one"), storageKey);
    SimNetLanes* const lanes = m_simNetLanes;
    if (lanes == nullptr || lanes->client() == nullptr)
        return std::nullopt;
    const ogsimnet::BindVerdict entry = lanes->bindings().verdict(static_cast<uint8_t>(storageKey), /*engineSaysLocal=*/false);
    if (entry.step == ogsimnet::BindStep::Wait || entry.physicsSlot == ogsimnet::kRosterNoSlot)
        return std::nullopt;
    return static_cast<uint32>(entry.physicsSlot);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::recordAuthoritativeSlot(AActor& owner, [[maybe_unused]] uint64 key, uint32 slot)
{
    checkf(m_simNetServer.has_value(),
        TEXT("UESimulationSession::recordAuthoritativeSlot on a peer without the og-sim-net server: only the authority "
             "chooses a slot, and a client binds the slot its roster entry carries. Was SetAuthoritativePhysicsSlot's "
             "authority checkf"));
    checkf(slot < kMaxSimulatableSlots,
        TEXT("UESimulationSession::recordAuthoritativeSlot: slot %u is not a slot of the physics world (kMaxSimulatableSlots "
             "= %u). Was SetAuthoritativePhysicsSlot's range checkf"), slot, static_cast<uint32>(kMaxSimulatableSlots));
    checkf(m_authoritativeSlotsByActor.find(&owner) == m_authoritativeSlotsByActor.end(),
        TEXT("UESimulationSession::recordAuthoritativeSlot: '%s' already holds slot %u and was handed slot %u; a character "
             "takes one slot for its lifetime. Was SetAuthoritativePhysicsSlot's once-only checkf"),
        *owner.GetName(), m_authoritativeSlotsByActor.count(&owner) != 0u ? m_authoritativeSlotsByActor.at(&owner) : 0u, slot);
    m_authoritativeSlotsByActor[&owner] = slot;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetSimNetCharacter(unsigned int id, const AActor* pawn)
{
    m_registeredComponentsById.erase(id);
    m_authoritativeSlotsByActor.erase(pawn);
    if (id == 0u || id > 0xFFu)
        return;
    if (m_simNetLanes != nullptr)
        m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetRegisteredRemoved(uint8_t simId)
{
    const unsigned int id = simId;
    m_systemsNotified_Step.erase(id);
    postLifecycleOutcome(id, LifecycleOutcomeKind::RosterRemoved);
}

template <typename TraitsT>
ClientBindStep UESimulationSession<TraitsT>::clientBindStep(unsigned int id, bool engineSaysLocal)
{
    if (!m_manager.has_value())
    {
        refuseRegistrationWithoutSession(id);
        return ClientBindStep::Abandon;
    }
    if (m_simNetLanes == nullptr)
        return ClientBindStep::Abandon;
    checkf(id != 0u && id <= 0xFFu,
        TEXT("UESimulationSession::clientBindStep: the roster keys a character by its sim id (1..255); id %u is not one"), id);
    const ogsimnet::BindVerdict verdict = m_simNetLanes->bindings().verdict(static_cast<uint8_t>(id), engineSaysLocal);
    switch (verdict.step)
    {
    case ogsimnet::BindStep::Wait:
        return ClientBindStep::Wait;
    case ogsimnet::BindStep::Ready:
        return ClientBindStep::Ready;
    case ogsimnet::BindStep::RoleMismatch:
        break;
    }
    ueSimNetHost::routeLine(ogsimnet::formatRosterBindMismatch(verdict.simId, verdict.isLocal, engineSaysLocal).data());
    checkf(false,
        TEXT("UESimulationSession::clientBindStep: sim id %u is %s in the roster and %s by the engine's role. Was P7's C-3 "
             "cross-check: the roster's local index decides which characters this client predicts"),
        id, verdict.isLocal ? TEXT("local") : TEXT("remote"), engineSaysLocal ? TEXT("local") : TEXT("remote"));
    return ClientBindStep::Abandon;
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginPlay()
{
	static_assert(requires(Game& game, UWorld& world, ENetMode netMode, bool flag, StaticData& staticData,
			std::optional<SystemsExec>& systemsExec, typename EngineTraits::BodyAdapter& bodyAdapter,
			typename EngineTraits::QueryAdapter& queryAdapter)
		{
			game.beforeWorldBuild(world, netMode, flag, staticData);
			Game::emplaceSystems(systemsExec, bodyAdapter, queryAdapter);
			{ Game::neutralInput() } -> std::same_as<PlayerInput>;
			Game::installGameLoggers(flag);
			game.afterSessionConfigured(netMode);
			{ Game::backendFingerprint() } -> std::same_as<std::string_view>;
		},
		"UESimulationSession<TraitsT>::beginPlay calls six phase hooks of TraitsT::Game, each at the position of the game "
		"statements it replaced: beforeWorldBuild(world, netMode, compositionExists, staticData) and "
		"afterSessionConfigured(netMode) on the session's Game& (they reach the game's own parts), and the static "
		"emplaceSystems(systemsExec, bodyAdapter, queryAdapter), neutralInput() (the PlayerInput a delay window is filled "
		"with), installGameLoggers(isAuthority) and backendFingerprint() (a std::string_view). No member of "
		"UESimulationSession shares a hook's name (compositionContractsHold asserts it). Added by "
		"og-ue-simulationcomponentandmanagersplit task 27b (manager design 4.3).");

	UWorld* uWorld = m_owner.GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("UESimulationSession: unexpected state"));

	if (refuseToStart(*uWorld, m_owner.GetNetMode()))
		return;

	m_game.beforeWorldBuild(*uWorld, m_owner.GetNetMode(),
		m_manager.has_value() || m_integrationLayer.has_value(), m_staticData);

	const float dt = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
	checkf(FMath::Abs(dt * static_cast<float>(NetConfig::tickFrequencyHz) - 1.f) < 1e-4f,
		TEXT("UESimulationSession: UPhysicsSettings::AsyncFixedTimeStepSize (%f s) is not one tick of ")
		TEXT("TraitsT::NetConfig::tickFrequencyHz (%d Hz). The manager's step dt and the tick rate the ")
		TEXT("per-connection templates are compiled for must agree. Was design D13 (task 18)."),
		dt, NetConfig::tickFrequencyHz);

	const ENetMode worldNetMode = m_owner.GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);

	m_latencyBudgetDiagnostics.begin(*uWorld, worldIsAuthority);

	ogsession::SessionKnobs sessionKnobs = ogsession::readSessionKnobs(UEConfigSource{}, worldIsAuthority);
	UESessionLogSink knobLog;
	const ogsession::SessionThreadChecks clockThreads(
		worldIsAuthority ? ogsession::SessionRole::Authority : ogsession::SessionRole::Prediction,
		&m_physicsBackendWorlds.host.threadContext(), std::this_thread::get_id());

	auto configureSession = [&]()
	{
		if (worldIsAuthority)
		{
			registerInstance();
			auto pctmloggerServer = [](const char* msg) { RouteOGMessage(msg); };
			m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
			Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter);
			m_manager.emplace(false, dt, typename ManagerType::Params{
				*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
				m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
			m_clock.emplace(m_clockEngine, *m_manager, m_inputResolution, clockThreads);
			m_display.emplace(clockThreads, *m_manager, m_storage, m_inputResolution, m_reconciliation, m_clock->displayTier());
			m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
			m_inputResolution.setLogger(std::function<void(const char*)>(pctmloggerServer));
			m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

			// ⛔G-23  docs/UESimulationSession-guards.md
			m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());

			beginServerSimNet(*uWorld);

			ogsession::applySessionKnobs(knobLog, *m_manager, m_simNetServer.has_value() ? &*m_simNetServer : nullptr,
				sessionKnobs, [this](int32 floorTicks) { m_clock->logRelayDelayFloorAdvisory(floorTicks); });

			// ⛔G-06  docs/UESimulationSession-guards.md
			m_clock->beginTierConsumer();

			simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
			Game::installGameLoggers(worldIsAuthority);
			bindCorrectionFieldDiffGate();
		}
		else
		{
			registerInstance();
			auto pctmlogger = [](const char* msg) { RouteOGMessage(msg); };
			m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
			Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter);
			m_manager.emplace(/*usePrediction=*/true, dt, typename ManagerType::Params{
				*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
				m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
			m_clock.emplace(m_clockEngine, *m_manager, m_inputResolution, clockThreads);
			m_display.emplace(clockThreads, *m_manager, m_storage, m_inputResolution, m_reconciliation, m_clock->displayTier());
			m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
			m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
			m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

			m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());

			m_clock->beginTierConsumer();

			beginClientSimNet(*uWorld);
			simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
			Game::installGameLoggers(worldIsAuthority);
			bindCorrectionFieldDiffGate();

#if !NO_LOGGING
			UE_LOG(LogOGResimProbe, Warning,
				TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
				ToString(LogOGResimProbe.GetVerbosity()),
				LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
				static_cast<uint32>(kResimGateProbeWindowSamples));
#endif
		}

		// ⛔G-24  docs/UESimulationSession-guards.md
		ogsession::applyResimTriggerPolicy(knobLog, *m_manager, sessionKnobs);

		ogsession::applyLatencyKnobs(knobLog, *m_manager, sessionKnobs, m_clock->dilationController(),
			m_clock->dilationWindow());

		m_game.afterSessionConfigured(worldNetMode);

		{
			const std::string_view fingerprint = Game::backendFingerprint();
			UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Backend] backend=%hs fingerprint=%.*hs"),
				EngineTraits::kName, static_cast<int32>(fingerprint.size()), fingerprint.data());
		}
	};

	if (worldIsAuthority)
	{
		const bool highResolutionWait = findDriverSimNetLanes(*uWorld, ognet::Role::Server)->endpoint().hasHighResolutionWait();
		m_physicsBackend.buildWorld(*uWorld, dt, worldIsAuthority, /*freeRunning=*/true);
		m_physicsBackend.runAuthorityLoop_GameThread(*uWorld, dt, highResolutionWait, readAuthorityIdleSlackSeconds(),
			configureSession);
		UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
		return;
	}

	m_physicsBackend.buildWorld(*uWorld, dt, worldIsAuthority);

	configureSession();

	m_physicsBackend.startStepping(*uWorld, dt, worldIsAuthority);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::endPlay()
{
	// ⛔G-20  docs/UESimulationSession-guards.md
	m_physicsBackend.onEndPlayBegin();
	if (!m_manager.has_value())
	{
		unregisterInstance();
		return;
	}
	m_runnerStopped = !m_manager->runsPrediction();

	teardownImmediateClient_GameThread();

	detachFromSimNetLanes();

	// ⛔G-21  docs/UESimulationSession-guards.md
	m_simNetIdle.reset();
	m_clockServer = nullptr;
	m_simNetServer.reset();
	m_simNetServerHost.reset();
	m_registeredComponentsById.clear();
	m_authoritativeSlotsByActor.clear();
	m_simUnregisteredIds.clear();
	m_pendingUnregisters.clear();
	m_componentsHeldForRunner.clear();

	m_clock->endTierConsumer();

	if (m_display.has_value())
		m_display->releaseHeld();

	unregisterInstance();

	if (UWorld* World = m_owner.GetWorld())
	{
		m_latencyBudgetDiagnostics.end(*World);
	}
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onPostPhysicsStep()
{
#if !UE_BUILD_SHIPPING
	if (const int32 hitchMs = consumeTestGameThreadHitchMs(); hitchMs > 0)
	{
		sleepTestGameThreadHitch(hitchMs);
	}
#endif

	m_physicsBackend.beforePresent_GameThread();

	collectLifecycleOutcomes_GameThread();

	if (!runsPrediction())
	{
		presentAuthority_GameThread();
		m_latencyBudgetDiagnostics.tick_GameThread();
		return;
	}

	refreshDisplayView_GameThread();

	Game::afterVisualization(*this);

	m_latencyBudgetDiagnostics.tick_GameThread();
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegister(
    unsigned int id,
    Simulatable&& simulatable,
    typename TraitsT::Component& owner,
    InputProvider&& inputProvider,
    bool isAuthority)
{
    static_assert(requires(Game& game, const Simulatable& constSimulatable, Simulatable& simulatable,
            typename TraitsT::Character& character, unsigned int id, bool flag, ENetMode netMode)
        {
            { Game::rootBodyId(constSimulatable) } -> std::same_as<BodyId>;
            game.afterBodiesCreated(simulatable, character, id, flag, netMode);
            game.afterRegistered(id, flag);
        },
        "UESimulationSession<TraitsT>::tryRegister calls three registration hooks of TraitsT::Game, each at the position "
        "of the game statements it replaced: the static rootBodyId(simulatable) (the BodyId the resolvability gate tests "
        "first), and afterBodiesCreated(simulatable, character, id, isAuthority, netMode) and afterRegistered(id, "
        "isAuthority) on the session's Game& (they reach the game's own parts). Added by "
        "og-ue-simulationcomponentandmanagersplit task 28 (manager design 4.3).");

    if (!m_manager.has_value())
    {
        refuseRegistrationWithoutSession(id);
        return TryRegisterStatus::Refused;
    }

    auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
    {
        PendingRegistration record;
        record.simulatable.emplace(std::move(simulatable));
        record.inputProvider = std::move(inputProvider);
        record.isAuthority   = isAuthority;
        auto inserted = m_pendingRegistrations.emplace(id, std::move(record));
        it = inserted.first;
    }

    PendingRegistration& record = it->second;

    if (record.queued)
        return finishQueuedRegistration(id, owner);

    if (record.isAuthority)
        return tryRegisterOnRunner(id, owner);

    if (!record.bodiesCreated)
    {
        typename TraitsT::Character* character = Cast<typename TraitsT::Character>(owner.GetOwner());
        checkf(character != nullptr,
               TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
               TEXT("first-call body pass reads that class's own root capsule"));
        if (!m_physicsBackend.createBodies_GameThread(owner, *character, *record.simulatable, id))
            return TryRegisterStatus::Pending;

#if DO_CHECK
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif

        m_game.afterBodiesCreated(*record.simulatable, *character, id, record.isAuthority, m_owner.GetNetMode());

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    const BodyId rootId = Game::rootBodyId(*record.simulatable);
    bool allResolvable = m_physAdapter->isBodyResolvable(rootId);
    if (allResolvable)
    {
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
                allResolvable = false;
        });
    }

    if (!allResolvable)
        return TryRegisterStatus::Pending;

    return tryRegisterQueued(id, owner);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterFromNewFramework(
    unsigned int id, typename TraitsT::Component& owner, bool isAuthority)
{
    static_assert(requires(Game& game, unsigned int id) { game.onUnregistered(id); },
        "UESimulationSession<TraitsT>::unregisterFromNewFramework calls the registration hook onUnregistered(id) of "
        "TraitsT::Game on the session's Game& (it reaches the game's own parts), at the position of the game statements "
        "it replaced: after the reception forget, before the latency forget. Added by "
        "og-ue-simulationcomponentandmanagersplit task 28 (manager design 4.3).");

    if (!m_manager.has_value())
        return;

    // ⛔G-30  docs/UESimulationSession-guards.md
    if (m_simUnregisteredIds.erase(id) != 0u)
        return;

    if (!isAuthority)
    {
        requestQueuedUnregister_GameThread(id, owner);
        return;
    }

    requestRunnerUnregister_GameThread(id, &owner);
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
typename UESimulationSession<TraitsT>::SimNetLanes::Client* UESimulationSession<TraitsT>::boundImmediateClient_Step() const
{
    if (!runsPrediction())
        return nullptr;
    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    return client != nullptr && lanes->clientSessionBound() ? client : nullptr;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterClientCapture_Step()
{
    typename SimNetLanes::Client* const client = boundImmediateClient_Step();
    if (client == nullptr)
        return;
    client->afterCaptureTick_Step(m_netSync, m_manager->currentIntegratedTick(), m_manager->getTimeConfig());
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::receiveForClientStep_Step()
{
    typename SimNetLanes::Client* const client = boundImmediateClient_Step();
    if (client == nullptr)
        return;
    applyUnregisterRequests_Step(*client);
    m_simNetLanes->drainClient_Step();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyUnregisterRequests_Step(typename SimNetLanes::Client& client)
{
    std::vector<unsigned int> requests;
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        requests.swap(m_unregisterRequests);
    }
    for (const unsigned int id : requests)
        enqueueQueuedUnregister_Step(client, id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::enqueueQueuedUnregister_Step(typename SimNetLanes::Client& client, unsigned int id)
{
    if (id == 0u || id > 0xFFu)
        return;
    m_unregisterRequested_Step.insert(id);
    // ⛔G-31  docs/UESimulationSession-guards.md
    if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))
        m_manager->notifyCharacterUnregistered(id);
    client.unregisterCharacter(static_cast<uint8_t>(id), [this, id](SimChannel* channel)
    {
        if (channel != nullptr)
            channel->clearOnInputHandedCallback();
        m_unregisterRequested_Step.erase(id);
        postLifecycleOutcome(id, LifecycleOutcomeKind::UnregisterApplied);
    });
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onQueuedRegisterApplied_Step(unsigned int id, SimChannel* channel, bool hasProvider)
{
    if (channel == nullptr)
    {
        postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterRefused);
        return;
    }
    if (hasProvider)
    {
        channel->setOnInputHandedCallback([this, id](const InputBundleBuffer& bundle)
        {
            bundle.template forEachSlot<PlayerInput>([this, id](uint32 captureTick, const PlayerInput&)
            {
                m_latencyBudgetDiagnostics.stampHandedAndLeft_Internal(id, captureTick);
            });
        });
    }
    // ⛔G-32  docs/UESimulationSession-guards.md
    if (m_unregisterRequested_Step.find(id) == m_unregisterRequested_Step.end())
    {
        m_manager->notifyCharacterRegistered(id);
        m_systemsNotified_Step.insert(id);
    }
    postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterApplied);
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterQueued(unsigned int id, typename TraitsT::Component& owner)
{
    PendingRegistration& record = m_pendingRegistrations.at(id);
    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    if (client == nullptr || m_immediateStepsStopped || id == 0u || id > 0xFFu)
        return TryRegisterStatus::Pending;
    const uint8_t simId = static_cast<uint8_t>(id);
    const bool hasProvider = static_cast<bool>(record.inputProvider);
    const ogsimnet::BindVerdict verdict = lanes->editBindings().markRegisteredIfReady(simId, hasProvider);
    if (verdict.step == ogsimnet::BindStep::Wait)
        return TryRegisterStatus::Pending;
    checkf(verdict.step == ogsimnet::BindStep::Ready,
        TEXT("UESimulationSession::tryRegister: id=%u registers %s an input provider, and its roster entry says the other: "
             "the provider is built for the engine's autonomous proxy, which clientBindStep checked against the roster. Was "
             "the engine role alone (isLocallyPredicted)"), id, hasProvider ? TEXT("with") : TEXT("without"));
    if (verdict.step != ogsimnet::BindStep::Ready)
        return TryRegisterStatus::Pending;
    client->registerCharacter(simId, std::move(*record.simulatable),
        m_latencyBudgetDiagnostics.wrapInputProvider(id, owner, std::move(record.inputProvider)),
        [this, id, hasProvider](SimChannel* channel) { onQueuedRegisterApplied_Step(id, channel, hasProvider); });
    record.simulatable.reset();
    record.queued = true;
    return TryRegisterStatus::Pending;
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::finishQueuedRegistration(unsigned int id, typename TraitsT::Component& owner)
{
    collectLifecycleOutcomes_GameThread();
    const auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
        return TryRegisterStatus::Pending;
    PendingRegistration& record = it->second;
    if (record.refused || record.rosterRemoved)
    {
        if (m_simNetLanes != nullptr && id <= 0xFFu)
            m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
        UE_LOG(LogOGMgmt, Error,
            TEXT("tryRegister: id=%u: the og-sim-net client's queued registration did not hold (%s); the character stays "
                 "unsimulated"),
            id, record.rosterRemoved ? TEXT("the roster removed it") : TEXT("its [SimNet.Lifecycle] line names why"));
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Refused;
    }
    if (!record.applied)
        return TryRegisterStatus::Pending;

    m_registeredComponentsById[id] = &owner;
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, 0);
    m_game.afterRegistered(id, /*isAuthority=*/false);
    m_physicsBackend.onRegistered_GameThread(owner, id);
    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestQueuedUnregister_GameThread(unsigned int id, typename TraitsT::Component& owner)
{
    const auto pending = m_pendingRegistrations.find(id);
    const bool queued = pending != m_pendingRegistrations.end() && pending->second.queued;
    const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
    if (pending != m_pendingRegistrations.end())
        m_pendingRegistrations.erase(pending);
    if (m_immediateStepsStopped || (!queued && !registered))
    {
        unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/true);
        return;
    }
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        m_unregisterRequests.push_back(id);
    }
    m_pendingUnregisters[id] = TStrongObjectPtr<typename TraitsT::Component>(&owner);
    // ⛔G-33  docs/UESimulationSession-guards.md
    unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/false);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterCharacterHostOnly(unsigned int id, typename TraitsT::Component* owner,
    bool releaseBodiesNow)
{
    forgetSimNetCharacter(id, owner != nullptr ? owner->GetOwner() : nullptr);
    m_game.onUnregistered(id);
    m_latencyBudgetDiagnostics.forgetCharacter(id);
    if (releaseBodiesNow && owner != nullptr)
        m_physicsBackend.onUnregistered_GameThread(*owner, id);
    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::collectLifecycleOutcomes_GameThread()
{
    std::vector<LifecycleOutcome> outcomes;
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        outcomes.swap(m_lifecycleOutcomes);
    }
    for (const LifecycleOutcome& outcome : outcomes)
    {
        switch (outcome.kind)
        {
        case LifecycleOutcomeKind::RegisterApplied:
        case LifecycleOutcomeKind::RegisterRefused:
        {
            const auto pending = m_pendingRegistrations.find(outcome.id);
            if (pending == m_pendingRegistrations.end() || !(pending->second.queued || pending->second.onRunner))
                break;
            pending->second.runnerBusy = false;
            if (outcome.kind == LifecycleOutcomeKind::RegisterApplied)
                pending->second.applied = true;
            else
                pending->second.refused = true;
            break;
        }
        case LifecycleOutcomeKind::BodiesBound:
        case LifecycleOutcomeKind::BodiesPending:
        case LifecycleOutcomeKind::BodiesUnresolvable:
        {
            if (outcome.kind != LifecycleOutcomeKind::BodiesUnresolvable)
                m_componentsHeldForRunner.erase(outcome.id);
            const auto pending = m_pendingRegistrations.find(outcome.id);
            if (pending == m_pendingRegistrations.end() || !pending->second.onRunner)
                break;
            pending->second.runnerBusy = false;
            if (outcome.kind == LifecycleOutcomeKind::BodiesBound)
            {
                pending->second.bodiesBound = true;
                pending->second.boundSlot = outcome.slot;
            }
            break;
        }
        case LifecycleOutcomeKind::UnregisterApplied:
        {
            const auto found = m_pendingUnregisters.find(outcome.id);
            if (found == m_pendingUnregisters.end())
                break;
            if (typename TraitsT::Component* const component = found->second.Get())
                m_physicsBackend.onUnregistered_GameThread(*component, outcome.id);
            m_pendingUnregisters.erase(found);
            break;
        }
        case LifecycleOutcomeKind::RosterRemoved:
        {
            if (m_pendingUnregisters.find(outcome.id) != m_pendingUnregisters.end())
                break;
            const auto pending = m_pendingRegistrations.find(outcome.id);
            if (pending != m_pendingRegistrations.end() && pending->second.queued)
            {
                pending->second.rosterRemoved = true;
                break;
            }
            const auto found = m_registeredComponentsById.find(outcome.id);
            if (found == m_registeredComponentsById.end())
                break;
            unregisterCharacterHostOnly(outcome.id, found->second.Get(), /*releaseBodiesNow=*/true);
            m_simUnregisteredIds.insert(outcome.id);
            break;
        }
        }
    }
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterOnRunner(unsigned int id, typename TraitsT::Component& owner)
{
    collectLifecycleOutcomes_GameThread();
    const auto it = m_pendingRegistrations.find(id);
    checkf(it != m_pendingRegistrations.end(), TEXT("UESimulationSession::tryRegisterOnRunner: tryRegister made the record"));
    PendingRegistration& record = it->second;
    record.onRunner = true;
    if (record.refused)
    {
        UE_LOG(LogOGMgmt, Error,
            TEXT("tryRegister: id=%u refused by the og-sim-net server (its [Roster.Refused] line names why); the character "
                 "stays unsimulated"), id);
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Refused;
    }
    if (record.applied)
    {
        m_registeredComponentsById[id] = &owner;
        m_runnerBodyKeysById[id] = record.bodyKey;
        UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, 1);
        m_game.afterRegistered(id, /*isAuthority=*/true);
        m_physicsBackend.onRegisteredHost_GameThread(owner, id);
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Ready;
    }
    if (record.runnerBusy || m_runnerStopped)
        return TryRegisterStatus::Pending;

    typename TraitsT::Character* const character = Cast<typename TraitsT::Character>(owner.GetOwner());
    checkf(character != nullptr,
           TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
           TEXT("first-call body pass reads that class's own root capsule"));
    if (!record.runnerSimulatable)
    {
        record.runnerSimulatable = std::make_shared<Simulatable>(std::move(*record.simulatable));
        record.simulatable.reset();
    }

    if (!record.bodiesCreated)
    {
        if (!record.bodiesBound)
        {
            typename PhysicsBackend::BodyRequest request = m_physicsBackend.makeBodyRequest_GameThread(owner, *character, id);
            record.bodyKey = request.key;
            record.runnerBusy = true;
            m_componentsHeldForRunner[id] = TStrongObjectPtr<typename TraitsT::Component>(&owner);
            postRunnerRequest([this, id, request = MoveTemp(request), simulatable = record.runnerSimulatable,
                staticData = &owner.getStaticData()]()
            {
                const std::optional<uint32_t> slot = m_physicsBackend.createBodies_Runner(request, *simulatable, *staticData, id);
                postLifecycleOutcome(id, slot.has_value() ? LifecycleOutcomeKind::BodiesBound : LifecycleOutcomeKind::BodiesPending,
                    slot.value_or(0u));
            });
            return TryRegisterStatus::Pending;
        }
        recordAuthoritativeSlot(*owner.GetOwner(), record.bodyKey, record.boundSlot);
#if DO_CHECK
        record.runnerSimulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif
        m_game.afterBodiesCreated(*record.runnerSimulatable, *character, id, /*isAuthority=*/true, m_owner.GetNetMode());
        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    checkf(id != 0u && id <= 0xFFu,
        TEXT("UESimulationSession::tryRegister: og-sim-net's roster keys a character by its sim id (1..255); id %u is not one"), id);
    AActor* const pawn = owner.GetOwner();
    const UOGNetConnection* const ownerConnection = Cast<UOGNetConnection>(GetRootNetConnection(pawn));
    const SimConnectionId ownerConn = ownerConnection != nullptr ? ownerConnection->OGNetId : kInvalidSimConnection;
    const uint8_t localIndex = GetPlayerSlotForActor(pawn);
    const FTCHARToUTF8 debugName(pawn != nullptr ? *pawn->GetName() : TEXT("none"));
    record.runnerBusy = true;
    record.registerPosted = true;
    postRunnerRequest([this, id, key = record.bodyKey, physicsSlot = static_cast<uint8_t>(record.boundSlot), ownerConn, localIndex,
        name = std::string(debugName.Get()), simulatable = record.runnerSimulatable]() mutable
    {
        if (!bodiesResolvable_Runner(*simulatable))
        {
            postLifecycleOutcome(id, LifecycleOutcomeKind::BodiesUnresolvable);
            return;
        }
        m_simNetServer->template registerCharacter<Simulatable>(static_cast<uint8_t>(id), physicsSlot, ownerConn, localIndex,
            std::move(name), std::move(*simulatable),
            [this, id, key](SimChannel* channel) { onRunnerRegisterApplied_Runner(id, key, channel); });
    });
    return TryRegisterStatus::Pending;
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::bodiesResolvable_Runner(const Simulatable& simulatable) const
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

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRunnerRegisterApplied_Runner(unsigned int id, uint64 key, SimChannel* channel)
{
    if (channel == nullptr)
    {
        m_physicsBackend.releaseBodies_Runner(key, id);
        postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterRefused);
        return;
    }
    // ⛔G-36  docs/UESimulationSession-guards.md
    m_manager->notifyCharacterRegistered(id);
    m_systemsNotified_Step.insert(id);
    m_physicsBackend.onRegistered_Runner(key, id);
    postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterApplied);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestRunnerUnregister_GameThread(unsigned int id, typename TraitsT::Component* owner)
{
    AActor* const pawn = owner != nullptr ? owner->GetOwner() : nullptr;
    const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
    const auto pending = m_pendingRegistrations.find(id);
    const bool pendingOnRunner = pending != m_pendingRegistrations.end() && pending->second.onRunner;
    const bool registerPosted = registered || (pendingOnRunner && pending->second.registerPosted);
    const bool bodiesHeld = registered || (pendingOnRunner && (pending->second.bodiesBound || pending->second.runnerBusy));
    const auto knownKey = m_runnerBodyKeysById.find(id);
    const uint64 key = knownKey != m_runnerBodyKeysById.end() ? knownKey->second
        : pendingOnRunner ? pending->second.bodyKey : PhysicsBackend::slotKeyFor(pawn);
    if (pending != m_pendingRegistrations.end())
        m_pendingRegistrations.erase(pending);
    if (knownKey != m_runnerBodyKeysById.end())
        m_runnerBodyKeysById.erase(knownKey);

    if (!m_runnerStopped && bodiesHeld)
    {
        postRunnerRequest([this, id, key, registerPosted]()
        {
            // ⛔G-37  docs/UESimulationSession-guards.md
            if (!registerPosted)
            {
                releaseOnRunner_Runner(id, key);
                return;
            }
            m_simNetServer->template unregisterCharacter<Simulatable>(static_cast<uint8_t>(id), ogsimnet::RosterRemoveReason::Left,
                [this, id, key](SimChannel*) { releaseOnRunner_Runner(id, key); });
        });
    }

    m_registeredComponentsById.erase(id);
    m_authoritativeSlotsByActor.erase(pawn);
    m_game.onUnregistered(id);
    m_latencyBudgetDiagnostics.forgetCharacter(id);
    if (registered)
        m_physicsBackend.onUnregisteredHost_GameThread(id);
    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::releaseOnRunner_Runner(unsigned int id, uint64 key)
{
    m_systemsNotified_Step.erase(id);
    m_physicsBackend.releaseBodies_Runner(key, id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::teardownImmediateClient_GameThread()
{
    if (!runsPrediction() || m_immediateStepsStopped)
        return;
    // ⛔G-34  docs/UESimulationSession-guards.md
    m_physicsBackend.onEndPlayBegin();
    m_immediateStepsStopped = true;

    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    if (client != nullptr)
    {
        applyUnregisterRequests_Step(*client);
        std::vector<unsigned int> live;
        for (const auto& entry : m_pendingRegistrations)
        {
            if (entry.second.queued)
                live.push_back(entry.first);
        }
        for (const auto& entry : m_registeredComponentsById)
            live.push_back(entry.first);
        for (const unsigned int id : live)
            enqueueQueuedUnregister_Step(*client, id);
        m_clock->beginStoppedDrain();
        lanes->drainClientWhileStopped_GameThread();
        m_clock->endStoppedDrain();
    }
    collectLifecycleOutcomes_GameThread();

    for (auto& entry : m_pendingUnregisters)
    {
        if (typename TraitsT::Component* const component = entry.second.Get())
            m_physicsBackend.onUnregistered_GameThread(*component, entry.first);
    }
    m_pendingUnregisters.clear();

    std::vector<unsigned int> registered;
    registered.reserve(m_registeredComponentsById.size());
    for (const auto& entry : m_registeredComponentsById)
        registered.push_back(entry.first);
    for (const unsigned int id : registered)
    {
        unregisterCharacterHostOnly(id, m_registeredComponentsById[id].Get(), /*releaseBodiesNow=*/true);
        m_simUnregisteredIds.insert(id);
    }

    for (auto it = m_pendingRegistrations.begin(); it != m_pendingRegistrations.end();)
    {
        if (!it->second.queued)
        {
            ++it;
            continue;
        }
        if (m_simNetLanes != nullptr && it->first <= 0xFFu)
            m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(it->first));
        it = m_pendingRegistrations.erase(it);
    }
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
