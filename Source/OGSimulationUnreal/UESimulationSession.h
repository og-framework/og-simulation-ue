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
#include "OGSimulationUnreal/UESessionTickHost.h"
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
#include "OGSimSession/SessionLifecycle.h"
#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTickGame.h"
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
static_assert(ueSessionClockLineKeepsItsRoute(ogsession::sessionNetHooksLines::kReleaseBatch, ELogVerbosity::Verbose),
	"The net hooks' [ReleaseBatch] line (ogsession::sessionNetHooksLines) reaches LogOGNet at Verbose, the level its "
	"OG_LOG_ROUTED(LogOGNet, ...) took from the route table in releaseDelayedInputsForStep (og-session-runtime task 26)");

constexpr bool ueSessionLifecycleLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Mgmt && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kQueuedRegistrationDidNotHold,
		ELogVerbosity::Error)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRegistered, ELogVerbosity::Log)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRunnerRegisterRefused, ELogVerbosity::Error)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRosterVerdictMismatch, ELogVerbosity::Error),
	"Each lifecycle line (ogsession::sessionLifecycleLines) reaches LogOGMgmt at the level its UE_LOG had: Error for the "
	"queued registration that did not hold and for the runner registration the og-sim-net server refused, Log for "
	"tryRegister: registered (both roles). Was each line's UE_LOG(LogOGMgmt, <Level>, ...) in finishQueuedRegistration "
	"(og-session-runtime task 29) and tryRegisterOnRunner (task 30). The roster-verdict mismatch line is new (task 30): Error, "
	"before the check that stops on it, with the id and the with/without the check's old message printed");

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
	template <typename BindingT> friend struct UESessionTickHost;
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

	struct NetPort
	{
		using Lanes       = SimNetLanes;
		using Server      = SimNetServer;
		using Idle        = ogsimnet::SimNetIdle<ognetunreal::SimLaneEndpoint>;
		using Simulatable = typename TraitsT::Simulatable;

		UESimulationSession* session = nullptr;

		Lanes*  lanes() const { return session->m_simNetLanes; }
		Server* server() { return session->m_simNetServer.has_value() ? &*session->m_simNetServer : nullptr; }
		Idle*   idle() { return session->m_simNetIdle.has_value() ? &*session->m_simNetIdle : nullptr; }
		void    runRunnerRequests() { session->m_authorityLifecycle->applyRequests(); }
		void    applyUnregisterRequests(typename Lanes::Client& client) { session->m_lifecycle->applyRequests(client); }
		void    publishPresentation() { session->publishAuthorityPresentation_Runner(); }
		double  nowSeconds() const { return FPlatformTime::Seconds(); }
		void    logRouted(const char* line) { RouteOGLevelLine(line); }
	};

	using NetHooks = ogsession::SessionNetHooks<NetPort, ManagerType, NetSync, Storage, LatencyBudgetDiagnostics>;

	struct LifecyclePort
	{
		using Client        = typename SimNetLanes::Client;
		using Channel       = SimChannel;
		using Simulatable   = typename TraitsT::Simulatable;
		using InputProvider = typename LatencyBudgetDiagnostics::InputProvider;
		using Bindings      = std::remove_reference_t<decltype(std::declval<SimNetLanes&>().editBindings())>;

		UESimulationSession* session = nullptr;

		Client*   client() { return session->m_simNetLanes != nullptr ? session->m_simNetLanes->client() : nullptr; }
		Bindings* bindings() { return session->m_simNetLanes != nullptr ? &session->m_simNetLanes->editBindings() : nullptr; }

		InputProvider wrapInputProvider(const ogsession::RegisterRequest& request, InputProvider&& provider)
		{
			return session->m_latencyBudgetDiagnostics.wrapInputProvider(request.simId, componentOf(request.entityKey),
				std::move(provider));
		}

		void notifyRegistered(uint32_t id) { session->m_manager->notifyCharacterRegistered(id); }
		void notifyUnregistered(uint32_t id) { session->m_manager->notifyCharacterUnregistered(id); }
		bool inStorage(uint32_t id) const { return session->m_storage.template has<Simulatable>(id); }

		void installInputStamps(uint32_t id, Channel& channel)
		{
			UESimulationSession* const owner = session;
			channel.setOnInputHandedCallback([owner, id](const InputBundleBuffer& bundle)
			{
				bundle.template forEachSlot<PlayerInput>([owner, id](uint32 captureTick, const PlayerInput&)
				{
					owner->m_latencyBudgetDiagnostics.stampHandedAndLeft_Internal(id, captureTick);
				});
			});
		}

		void clearInputStamps(Channel& channel) { channel.clearOnInputHandedCallback(); }
		void stopSteps() { session->m_physicsBackend.onEndPlayBegin(); }

		void drainWhileStopped()
		{
			session->m_clock->beginStoppedDrain();
			session->m_simNetLanes->drainClientWhileStopped_GameThread();
			session->m_clock->endStoppedDrain();
		}

		void logRouted(const char* line) { RouteOGLevelLine(line); }
	};

	using Lifecycle = ogsession::SessionLifecycle<LifecyclePort>;

	struct AuthorityLifecyclePort
	{
		using Bodies      = PhysicsBackend;
		using Server      = SimNetServer;
		using Channel     = SimChannel;
		using Simulatable = typename TraitsT::Simulatable;

		UESimulationSession* session = nullptr;

		Server* server() { return session->m_simNetServer.has_value() ? &*session->m_simNetServer : nullptr; }
		Bodies& bodies() { return session->m_physicsBackend; }

		bool bodiesResolvable(const Simulatable& simulatable) const
		{
			if (!session->m_physAdapter->isBodyResolvable(Game::rootBodyId(simulatable)))
				return false;
			bool allResolvable = true;
			simulatable.getPhysicsComposite().forEach([&](const auto& decl)
			{
				if (!session->m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
					allResolvable = false;
			});
			return allResolvable;
		}

		uint64_t bodyKeyOf(ogsession::EntityKey key) const
		{
			const typename TraitsT::Component* const owner = reinterpret_cast<const typename TraitsT::Component*>(key.value);
			return PhysicsBackend::slotKeyFor(owner != nullptr ? owner->GetOwner() : nullptr);
		}

		bool runnerStopped() const { return session->m_runnerStopped; }
		void notifyRegistered(uint32_t id) { session->m_manager->notifyCharacterRegistered(id); }
		void notifyUnregistered(uint32_t id) { session->m_manager->notifyCharacterUnregistered(id); }
		bool inStorage(uint32_t id) const { return session->m_storage.template has<Simulatable>(id); }
		void logRouted(const char* line) { RouteOGLevelLine(line); }
	};

	using AuthorityLifecycle = ogsession::SessionAuthorityLifecycle<AuthorityLifecyclePort>;

	static_assert(ogsession::SessionBodies<PhysicsBackend>,
		"UESimulationSession: the tick backend's wrapper (TickDriverBackendUImpl) is the authority lifecycle's bodies "
		"provider, og-sim-session's SessionBodies (makeRequest, create, slotOf, onRegistered, release). Were "
		"makeBodyRequest_GameThread, createBodies_Runner, onRegistered_Runner and releaseBodies_Runner, called from the "
		"session's runner closures (og-session-runtime task 30)");

	struct AuthorityRegisterHooks
	{
		UESimulationSession*            session = nullptr;
		typename TraitsT::Component*    owner   = nullptr;
		unsigned int                    id      = 0u;

		void bodiesRequested()
		{
			session->m_componentsHeldForRunner[id] = TStrongObjectPtr<typename TraitsT::Component>(owner);
		}

		void bodiesCreated(Simulatable& simulatable)
		{
			typename TraitsT::Character* const character = Cast<typename TraitsT::Character>(owner->GetOwner());
			checkf(character != nullptr,
				TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
				TEXT("first-call body pass reads that class's own root capsule"));
#if DO_CHECK
			const unsigned int checkedId = id;
			simulatable.getPhysicsComposite().forEach([&](const auto& decl)
			{
				checkf(decl.bindings.ownBodyId.value != 0,
					TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), checkedId);
				checkf(decl.bindings.parentBodyId.value != 0,
					TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), checkedId);
			});
#endif
			session->m_game.afterBodiesCreated(simulatable, *character, id, /*isAuthority=*/true, session->m_owner.GetNetMode());
		}
	};

	static ogsession::RegisterRequest authorityRequestFor(unsigned int id, typename TraitsT::Component& owner)
	{
		AActor* const pawn = owner.GetOwner();
		const UOGNetConnection* const ownerConnection = Cast<UOGNetConnection>(GetRootNetConnection(pawn));
		const FTCHARToUTF8 debugName(pawn != nullptr ? *pawn->GetName() : TEXT("none"));
		ogsession::RegisterRequest request;
		request.simId           = id;
		request.entityKey       = entityKeyOf(owner);
		request.debugName       = std::string(debugName.Get());
		request.ownerConnection = ownerConnection != nullptr ? ownerConnection->OGNetId : kInvalidSimConnection;
		request.localIndex      = GetPlayerSlotForActor(pawn);
		return request;
	}

	static ogsession::EntityKey entityKeyOf(const typename TraitsT::Component& owner)
	{
		return ogsession::EntityKey{ reinterpret_cast<std::uintptr_t>(&owner) };
	}

	static const typename TraitsT::Component& componentOf(ogsession::EntityKey key)
	{
		checkf(key.value != 0u, TEXT("UESimulationSession: a client's lifecycle key is its simulatable component's address"));
		return *reinterpret_cast<const typename TraitsT::Component*>(key.value);
	}

	static ogsession::RegisterRequest lifecycleRequestFor(unsigned int id, const typename TraitsT::Component& owner)
	{
		ogsession::RegisterRequest request;
		request.simId     = id;
		request.entityKey = entityKeyOf(owner);
		return request;
	}

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

	void onSimNetClientLifecycleUnregistering(uint8_t simId);

private:
	SimNetLanes* findDriverSimNetLanes(UWorld& world, ognet::Role role);

	void beginServerSimNet(UWorld& world);

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

	TryRegisterStatus tryRegisterOnRunner(unsigned int id, Simulatable&& simulatable, typename TraitsT::Component& owner);

	void requestRunnerUnregister_GameThread(unsigned int id, typename TraitsT::Component* owner);

	void onAuthorityRunnerJoined_GameThread();

	void beginClientSimNet(UWorld& world);

	void logSimNetHostLine(bool isServer) const;

	bool refuseToStart(UWorld& world, ENetMode netMode);

	bool refuseNetModeOrDriver(UWorld& world, ENetMode netMode);

	bool refuseRetiredKnobs(UWorld& world, ENetMode netMode);

	bool refuseSecondSessionOfRole(UWorld& world, ENetMode netMode);

	void refuseRegistrationWithoutSession(unsigned int id);

	void unregisterCharacterHostOnly(unsigned int id, typename TraitsT::Component* owner, bool releaseBodiesNow);

	void collectLifecycleOutcomes_GameThread();

	void onPredictionOutcome_GameThread(const ogsession::LifecycleOutcome& outcome);

	void onAuthorityOutcome_GameThread(const ogsession::LifecycleOutcome& outcome);

	void teardownImmediateClient_GameThread();

	void onSimNetLanesStopping(bool isServer);

	void detachFromSimNetLanes();

	void forgetSimNetCharacter(unsigned int id);

	void onFrameStepsDue_Step(int32 firstUpcomingSimTick, int32 numSteps);

	void onPostPhysicsStep();

	void refreshDisplayView_GameThread()
	{
		m_display->acquireForFrame([this](unsigned int id, DisplayVizState& vizState)
		{
			m_physicsBackend.overlayRenderPoses_GameThread(id, vizState);
		});
	}

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

#include "OGSimSession/SessionRuntime.h"
	bool m_noSessionRegisterRefused = false;
	const char* m_noSessionReason = "no-session";
	bool m_noSessionRegisterRaisesMessage = false;
	bool m_runnerStopped = false;
	uint64 m_authorityWakes_Runner = 0u;

	using AuthorityPresentationSlice = typename Game::AuthorityPresentationSlice;

	FrameHealthProbe m_frameHealthProbe;

	std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>
		m_registeredComponentsById;
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
		bool queued = false;
	};
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;

	std::unordered_map<unsigned int, TStrongObjectPtr<typename TraitsT::Component>> m_pendingUnregisters;
	std::unordered_map<unsigned int, TStrongObjectPtr<typename TraitsT::Component>> m_componentsHeldForRunner;
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
void UESimulationSession<TraitsT>::onSimNetLifecycleUnregistering(uint8_t simId)
{
	checkf(m_authorityLifecycle.has_value(),
		TEXT("UESimulationSession::onSimNetLifecycleUnregistering: the authority builds its lifecycle on the runner before the "
			 "runner's first drain, and only that drain applies og-sim-net's queued unregistrations"));
	m_authorityLifecycle->onUnregistering(simId);
}

template <typename TraitsT>
std::optional<uint32> UESimulationSession<TraitsT>::physicsSlotFor(AActor& owner, uint64 key, unsigned int storageKey) const
{
	checkf(m_manager.has_value(), TEXT("UESimulationSession::physicsSlotFor before beginPlay built the manager"));
	if (!m_manager->runsPrediction())
		return m_authorityLifecycle.has_value() ? m_authorityLifecycle->authoritativeSlotFor(key) : std::nullopt;
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
void UESimulationSession<TraitsT>::recordAuthoritativeSlot([[maybe_unused]] AActor& owner, uint64 key, uint32 slot)
{
	checkf(m_authorityLifecycle.has_value(),
		TEXT("UESimulationSession::recordAuthoritativeSlot on a peer without the og-sim-net server: only the authority "
			 "chooses a slot, and a client binds the slot its roster entry carries. Was SetAuthoritativePhysicsSlot's "
			 "authority checkf"));
	m_authorityLifecycle->recordAuthoritativeSlot(key, slot);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetSimNetCharacter(unsigned int id)
{
	m_registeredComponentsById.erase(id);
	if (id == 0u || id > 0xFFu)
		return;
	if (m_simNetLanes != nullptr)
		m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::collectLifecycleOutcomes_GameThread()
{
	if (m_lifecycle.has_value())
	{
		m_lifecycle->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); });
		return;
	}
	if (m_authorityLifecycle.has_value())
		m_authorityLifecycle->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onAuthorityOutcome_GameThread(outcome); });
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onAuthorityOutcome_GameThread(const ogsession::LifecycleOutcome& outcome)
{
	if (const auto* const bound = std::get_if<ogsession::lifecycleOutcome::BodiesBound>(&outcome))
		m_componentsHeldForRunner.erase(bound->simId);
	else if (const auto* const pending = std::get_if<ogsession::lifecycleOutcome::BodiesPending>(&outcome))
		m_componentsHeldForRunner.erase(pending->simId);
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterOnRunner(unsigned int id, Simulatable&& simulatable,
	typename TraitsT::Component& owner)
{
	collectLifecycleOutcomes_GameThread();
	if (!m_authorityLifecycle.has_value())
		return TryRegisterStatus::Pending;
	AuthorityRegisterHooks hooks{ this, &owner, id };
	const ogsession::RegisterStatus status =
		m_authorityLifecycle->tryRegister(authorityRequestFor(id, owner), std::move(simulatable), hooks);
	if (status == ogsession::RegisterStatus::Pending)
		return TryRegisterStatus::Pending;
	if (status == ogsession::RegisterStatus::Refused)
		return TryRegisterStatus::Refused;
	m_registeredComponentsById[id] = &owner;
	m_game.afterRegistered(id, /*isAuthority=*/true);
	m_physicsBackend.onRegisteredHost_GameThread(owner, id);
	return TryRegisterStatus::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestRunnerUnregister_GameThread(unsigned int id, typename TraitsT::Component* owner)
{
	const bool registered = m_authorityLifecycle.has_value()
		&& m_authorityLifecycle->requestUnregister(id, owner != nullptr ? entityKeyOf(*owner) : ogsession::EntityKey{});
	m_registeredComponentsById.erase(id);
	m_game.onUnregistered(id);
	m_latencyBudgetDiagnostics.forgetCharacter(id);
	if (registered)
		m_physicsBackend.onUnregisteredHost_GameThread(id);
	UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetClientLifecycleUnregistering(uint8_t simId)
{
	checkf(m_lifecycle.has_value(),
		TEXT("UESimulationSession::onSimNetClientLifecycleUnregistering: the client binds the og-net session after it built "
			 "its lifecycle, and only a bound session hears og-sim-net's unregistering hook"));
	m_lifecycle->onUnregistering(simId);
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
	lanes->setOnRegisteredRemoved([this](uint8_t simId) { m_lifecycle->onRegisteredRemoved(simId); });
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
	if (m_lifecycle.has_value())
		m_lifecycle->detach(lanes->editBindings());
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
void UESimulationSession<TraitsT>::teardownImmediateClient_GameThread()
{
    if (!runsPrediction() || !m_lifecycle.has_value())
        return;
    if (!m_lifecycle->teardown([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); }))
        return;

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
        if (it->second.queued)
            it = m_pendingRegistrations.erase(it);
        else
            ++it;
    }
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

#include "OGSimSession/SessionRuntimeStartStop.h"

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

    if (isAuthority)
        return tryRegisterOnRunner(id, std::move(simulatable), owner);

    auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
    {
        PendingRegistration record;
        record.simulatable.emplace(std::move(simulatable));
        record.inputProvider = std::move(inputProvider);
        auto inserted = m_pendingRegistrations.emplace(id, std::move(record));
        it = inserted.first;
    }

    PendingRegistration& record = it->second;

    if (record.queued)
    {
        collectLifecycleOutcomes_GameThread();
        const ogsession::RegisterStatus status =
            m_lifecycle->tryRegister(lifecycleRequestFor(id, owner), std::move(simulatable), std::move(inputProvider));
        if (status == ogsession::RegisterStatus::Pending)
            return TryRegisterStatus::Pending;
        if (status == ogsession::RegisterStatus::Refused)
        {
            m_pendingRegistrations.erase(id);
            return TryRegisterStatus::Refused;
        }
        m_registeredComponentsById[id] = &owner;
        m_game.afterRegistered(id, /*isAuthority=*/false);
        m_physicsBackend.onRegistered_GameThread(owner, id);
        m_pendingRegistrations.erase(id);
        return TryRegisterStatus::Ready;
    }

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

        m_game.afterBodiesCreated(*record.simulatable, *character, id, /*isAuthority=*/false, m_owner.GetNetMode());

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

    m_lifecycle->tryRegister(lifecycleRequestFor(id, owner), std::move(*record.simulatable), std::move(record.inputProvider));
    if (m_lifecycle->isQueued(id))
    {
        record.simulatable.reset();
        record.queued = true;
    }
    return TryRegisterStatus::Pending;
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
        const ogsession::UnregisterRelease release = m_lifecycle->requestUnregister(id, entityKeyOf(owner));
        m_pendingRegistrations.erase(id);
        if (release == ogsession::UnregisterRelease::AfterApplied)
            m_pendingUnregisters[id] = TStrongObjectPtr<typename TraitsT::Component>(&owner);
        unregisterCharacterHostOnly(id, &owner, release == ogsession::UnregisterRelease::Now);
        return;
    }

    requestRunnerUnregister_GameThread(id, &owner);
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterCharacterHostOnly(unsigned int id, typename TraitsT::Component* owner,
    bool releaseBodiesNow)
{
    forgetSimNetCharacter(id);
    m_game.onUnregistered(id);
    m_latencyBudgetDiagnostics.forgetCharacter(id);
    if (releaseBodiesNow && owner != nullptr)
        m_physicsBackend.onUnregistered_GameThread(*owner, id);
    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onPredictionOutcome_GameThread(const ogsession::LifecycleOutcome& outcome)
{
    if (const auto* const unregistered = std::get_if<ogsession::lifecycleOutcome::Unregistered>(&outcome))
    {
        const auto found = m_pendingUnregisters.find(unregistered->simId);
        if (found == m_pendingUnregisters.end())
            return;
        if (typename TraitsT::Component* const component = found->second.Get())
            m_physicsBackend.onUnregistered_GameThread(*component, unregistered->simId);
        m_pendingUnregisters.erase(found);
        return;
    }
    if (const auto* const removed = std::get_if<ogsession::lifecycleOutcome::RosterRemoved>(&outcome))
    {
        const auto found = m_registeredComponentsById.find(removed->simId);
        if (found == m_registeredComponentsById.end())
            return;
        unregisterCharacterHostOnly(removed->simId, found->second.Get(), /*releaseBodiesNow=*/true);
        m_simUnregisteredIds.insert(removed->simId);
    }
}

template <typename BindingT>
typename UESessionTickHost<BindingT>::Manager& UESessionTickHost<BindingT>::manager()
{
	return *session->m_manager;
}

template <typename BindingT>
typename UESessionTickHost<BindingT>::Storage& UESessionTickHost<BindingT>::storage()
{
	return session->m_storage;
}

template <typename BindingT>
const ogsession::SessionThreadChecks& UESessionTickHost<BindingT>::threads() const
{
	checkf(session->m_threads.has_value(),
		TEXT("UESessionTickHost::threads: the session builds its thread checks in beginPlay, before it builds the world or "
			 "starts the driver that calls the tick game"));
	return *session->m_threads;
}

template <typename BindingT>
auto* UESessionTickHost<BindingT>::clock()
{
	return session->m_clock.has_value() ? &*session->m_clock : nullptr;
}

template <typename BindingT>
auto* UESessionTickHost<BindingT>::display()
{
	return session->m_display.has_value() ? &*session->m_display : nullptr;
}

template <typename BindingT>
auto* UESessionTickHost<BindingT>::netHooks()
{
	return session->m_netHooks.has_value() ? &*session->m_netHooks : nullptr;
}

template <typename BindingT>
auto* UESessionTickHost<BindingT>::clockServer()
{
	return session->m_clockServer;
}

template <typename BindingT>
typename UESessionTickHost<BindingT>::Diagnostics& UESessionTickHost<BindingT>::diagnostics()
{
	return session->m_latencyBudgetDiagnostics;
}

template <typename BindingT>
void UESessionTickHost<BindingT>::sampleInput()
{
	BindingT::sampleInput(*session);
}

template <typename BindingT>
void UESessionTickHost<BindingT>::onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps)
{
	session->onFrameStepsDue_Step(firstUpcomingSimTick, numSteps);
}

template <typename BindingT>
void UESessionTickHost<BindingT>::onPostPhysics()
{
	session->onPostPhysicsStep();
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
