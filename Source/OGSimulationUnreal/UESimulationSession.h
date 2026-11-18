// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/SimulationComparison.h"
#include "OGSimulation/SimulationLog.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulation/SimulatableList.h"
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
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"

#include <concepts>
#include <cstdio>
#include <functional>
#include <optional>
#include <string_view>
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
	"[Warning][SimNet.Host] role=%s immediate=0 rttSource=EnginePing driver=%s inbox=%u";
inline constexpr size_t kSimNetHostLineBytes = 128u;
inline constexpr uint64 kSimNetDriverRefusedToastKey = 0x4F47534EF0000002ull;
inline constexpr float kSimNetDriverRefusedToastSeconds = 15.f;

static_assert(kSimNetHostLineBytes >= sizeof(kSimNetHostLineFormat) + sizeof("server") + sizeof("detached") + 10u,
	"kSimNetHostLineBytes must hold the [SimNet.Host] proof line with the longer role, the longer driver name and a 10-digit "
	"inbox size");

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

public:
	const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
	ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
	const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
	bool runsPrediction() const { return m_manager->runsPrediction(); }

	void onPostSimulationGameThread();

	typename EngineTraits::VizQuery&        editVizQuery();
	const typename EngineTraits::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

	Storage& editStorage() { return m_storage; }

	Reconciliation&       editReconciliation()       { return m_reconciliation; }
	const Reconciliation& editReconciliation() const { return m_reconciliation; }

	std::optional<PlayerInput> getLastRelayedInput(unsigned int id) const
	{
		return m_inputResolution.template getLastRelayedInput<Simulatable>(id);
	}

	const LocalInputCache<PlayerInput>* getLocalInputCache(unsigned int id) const
	{
		return m_inputResolution.getDiagnostics().template localInputCache<Simulatable>(id);
	}

	bool isLocallyControlledOnThisPeer(unsigned int id) const
	{
		return getLocalInputCache(id) != nullptr;
	}

	template <typename SliceT>
	std::optional<SliceT> getVizSlice(unsigned int id) const
	{
		if (!m_storage.template has<Simulatable>(id))
			return std::nullopt;

		// ⛔G-02  docs/UESimulationSession-guards.md
		return m_storage.template get<Simulatable>(id)
			.getVizState().getState().template get<SliceT>();
	}

	const RelayedReadObservationRing* getRelayedReadObservations(unsigned int id) const
	{
		return m_inputResolution.getDiagnostics()
			.template relayedReadObservations<Simulatable>(id);
	}

	const RelayedInputArrivalRing* getRelayedInputArrivals(unsigned int id) const
	{
		return m_inputResolution.getDiagnostics()
			.template relayedInputArrivals<Simulatable>(id);
	}

	const TimeConfig* getTimeConfigPtr() const
	{
		return m_manager.has_value() ? &m_manager->getTimeConfig() : nullptr;
	}

	const std::optional<ManagerType>& getManager() const { return m_manager; }
	const Storage&                    getStorage() const { return m_storage; }
	const std::optional<SystemsExec>& getSystemsExec() const { return m_systemsExec; }

	const std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>&
	getRegisteredComponentsById() const { return m_registeredComponentsById; }

	// ⛔G-11  docs/UESimulationSession-guards.md
	static UESimulationSession* instanceFor(bool isAuthority)
	{
		return isAuthority ? s_instances[0] : s_instances[1];
	}

	void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
	{
		m_onTimingInfoReceivedCallback = std::move(fn);
	}

	void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime)
	{
		m_latencyBudgetDiagnostics.noteWireSample(roundTripTime);
		if (m_onTimingInfoReceivedCallback)
		{
			m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
			return;
		}
		if (!m_manager.has_value())
			return;
		m_manager->editNetworkEstimator().updateRTT(roundTripTime);
		m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
	}

	void publishClientEffectiveInputDelayTicks(int32 delayTicks)
	{
		m_inputResolution.setClientEffectiveInputDelayTicks(delayTicks);
	}

	int32 getClientEffectiveInputDelayTicks() const
	{
		return m_inputResolution.getClientEffectiveInputDelayTicks();
	}

	void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier);

	const ReplicatedTierConsumer* getReplicatedTierConsumer() const
	{
		return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
	}

	void onRelayDelayFloorReceived(uint8_t floorTicks);

	using ReceptionCoordinator = typename SimNetServer::Coordinator;
	bool hasServerTierWiring() const { return m_simNetServer.has_value(); }

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

private:
	void requestInputDelayIncreaseStall(int32 deltaDelayTicks)
	{
		// ⛔G-03  docs/UESimulationSession-guards.md
		if (!m_manager.has_value() || !m_manager->runsPrediction())
			return;

		m_manager->editClientClock().requestInputDelayIncreaseStall(deltaDelayTicks);
	}

	void beginTierConsumer();

	SimNetLanes* findDriverSimNetLanes(UWorld& world, ognet::Role role);

	void beginServerSimNet(UWorld& world, ENetMode netMode);

	void beginClientSimNet(UWorld& world);

	void logSimNetHostLine(bool isServer, bool detached) const;

	void onSimNetLanesStopping(bool isServer);

	void detachFromSimNetLanes();

	SimChannel* addSimNetCharacter(unsigned int id, typename TraitsT::Component& owner);

	SimChannel* registeredChannelFor(unsigned int id) const;

	void forgetSimNetCharacter(unsigned int id, const AActor* pawn, bool isAuthority);

	void unregisterCharacter(unsigned int id, typename TraitsT::Component* owner, bool isAuthority);

	void onSimNetRegisteredRemoved(uint8_t simId);

	void publishServerClock_GameThread();

	void applyReplicatedConnectionTier(uint8 tier);

	void applyReplicatedRelayDelayFloor(uint8 floorTicks);

	// ⛔G-04  docs/UESimulationSession-guards.md
	void logRelayDelayFloorAdvisory(int32 floorTicks);

	// ⛔G-05  docs/UESimulationSession-guards.md
	int32 recomputeAndPublishEffectiveInputDelay();

	void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);

	struct SessionKnobs
	{
		int32   configuredRelayDelayFloorTicks;
		int32   configuredCorrectionRotationK;
		bool    hasIniResimTriggerPolicy;
		FString configuredResimTriggerPolicy;
		bool    hasIniRedundancyFrames;
		int32   configuredRedundancyFrames;
		bool    hasIniRelayResendEntries;
		int32   configuredRelayResendEntries;
		bool    hasIniSimTrafficImmediate;
		int32   configuredSimTrafficImmediate;
		bool    hasIniRecentreSettleSamples;
		int32   configuredRecentreSettleSamples;
	};

	static SessionKnobs readSessionKnobs(bool worldIsAuthority);

	void applySessionKnobs(const SessionKnobs& knobs);

	void applyResimTriggerPolicy(SessionKnobs& knobs);

	void applyLatencyKnobs(const SessionKnobs& knobs);

	void onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps);

	void onPostPhysicsStep();

	void releaseDelayedInputsForStep(int32 firstTick, int32 numSteps);

	void afterAuthorityBatch(int32 firstUpcomingSimTick);

	std::optional<uint32> physicsSlotFor(AActor& owner, uint64 key, unsigned int storageKey) const;

	void recordAuthoritativeSlot(AActor& owner, uint64 key, uint32 slot);

	template <typename FunctionT>
	void forEachAuthorityCorrectionTick(FunctionT&& function) const;

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

	std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

	int32 m_lastPublishedEffectiveInputDelayTicks = 0;

	TUniquePtr<SimNetLanes> m_detachedSimNetLanes;
	SimNetLanes* m_simNetLanes = nullptr;
	std::optional<SimNetServerHost> m_simNetServerHost;
	std::optional<SimNetServer> m_simNetServer;
	bool m_detachedClientAbandonLogged = false;

	FrameHealthProbe m_frameHealthProbe;

	ConnectionBudgetProbe m_connectionBudgetProbe;

	std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>
		m_registeredComponentsById;
	std::unordered_map<unsigned int, SimChannel*> m_simChannelsById;
	std::unordered_map<const AActor*, uint32> m_authoritativeSlotsByActor;
	std::unordered_set<unsigned int> m_simUnregisteredIds;

	std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

	inline static UESimulationSession* s_instances[2] = { nullptr, nullptr };

	struct PendingRegistration
	{
		std::optional<Simulatable> simulatable;
		bool bodiesCreated = false;
		InputProvider inputProvider;
		bool isAuthority = false;
	};
	std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;

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
		static_assert(__builtin_offsetof(UESimulationSession, m_manager)
		                  < __builtin_offsetof(UESimulationSession, m_replicatedTierConsumer),
			"UESimulationSession members destruct in REVERSE declaration order. The tier consumer is emplaced from the "
			"manager's TimeConfig and keeps a reference to it (ReplicatedTierConsumer borrows its config), so it must be "
			"destroyed before the manager. Keep m_manager < m_replicatedTierConsumer. Added when the transport moved here "
			"(og-ue-simulationcomponentandmanagersplit task 25): in the owner the consumer was declared above the member "
			"that holds the manager, and only the owner's EndPlay reset kept it from outliving that TimeConfig.");
		static_assert(__builtin_offsetof(UESimulationSession, m_manager)
		                  < __builtin_offsetof(UESimulationSession, m_simNetServer)
		           && __builtin_offsetof(UESimulationSession, m_detachedSimNetLanes)
		                  < __builtin_offsetof(UESimulationSession, m_simNetServerHost)
		           && __builtin_offsetof(UESimulationSession, m_simNetServerHost)
		                  < __builtin_offsetof(UESimulationSession, m_simNetServer),
			"UESimulationSession members destruct in REVERSE declaration order. og-sim-net's server keeps a reference to the "
			"manager's TimeConfig (and its reception coordinator borrows it too), to its host and to the lanes' transport, and "
			"a standalone or foreign-driver world's lanes are the session's own detached ones. So the server is destroyed "
			"before the manager, its host and the detached lanes. Keep m_manager < m_simNetServer and m_detachedSimNetLanes < "
			"m_simNetServerHost < m_simNetServer. Was the m_manager < m_receptionCoordinator row (task 26a); the coordinator "
			"is the server's own since og-input-latency task P5-7.");
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
void UESimulationSession<TraitsT>::onPostSimulationGameThread()
{
	m_manager->onPostSimulationGameThread();
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
	if (s_instances[0] == this)
		s_instances[0] = nullptr;
	if (s_instances[1] == this)
		s_instances[1] = nullptr;
	detachFromSimNetLanes();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginTierConsumer()
{
	m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
	recomputeAndPublishEffectiveInputDelay();
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
void UESimulationSession<TraitsT>::logSimNetHostLine(bool isServer, bool detached) const
{
	char line[kSimNetHostLineBytes];
	std::snprintf(line, sizeof(line), kSimNetHostLineFormat, isServer ? "server" : "client", detached ? "detached" : "OGNet",
		static_cast<unsigned>(kUESimNetInboxSlots));
	RouteOGMessage(line);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginServerSimNet(UWorld& world, ENetMode netMode)
{
	SimNetLanes* lanes = findDriverSimNetLanes(world, ognet::Role::Server);
	const bool detached = lanes == nullptr;
	if (detached)
	{
		if (netMode != NM_Standalone)
		{
			const UNetDriver* const driver = world.GetNetDriver();
			UE_LOG(LogOGNet, Error,
				TEXT("[SimNet.Host] refused role=server driver=%s: simulation traffic needs the og-net driver"),
				driver != nullptr ? *driver->GetClass()->GetName() : TEXT("none"));
			if (GEngine != nullptr)
			{
				GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
					TEXT("Simulation traffic needs the og-net driver: this server simulates alone"));
			}
		}
		m_detachedSimNetLanes = SimNetLanes::makeDetached(ognet::Role::Server, ognet::EndpointConfig{});
		lanes = m_detachedSimNetLanes.Get();
	}
	m_simNetLanes = lanes;

	const ognet::EndpointConfig& config = lanes->endpoint().config();
	m_simNetServerHost.emplace(lanes->driver(), lanes->endpoint(), this);
	m_simNetServer.emplace(*m_simNetServerHost, lanes->transport(), m_manager->getTimeConfig(),
		ogsimnet::correctionAllowanceBytes(config.lanes, config.budget, static_cast<double>(NetConfig::tickFrequencyHz)));
	lanes->bindServerSession([this]() { m_simNetServer->dispatchFrame(); }, [this]() { m_simNetServer->flushFrame(); });
	lanes->setStopHandler([this]() { onSimNetLanesStopping(/*isServer=*/true); });
	logSimNetHostLine(/*isServer=*/true, detached);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginClientSimNet(UWorld& world)
{
	checkf(m_manager.has_value() && m_replicatedTierConsumer.has_value(),
		TEXT("UESimulationSession: the client binds the og-net session after the manager and the tier consumer exist: the "
			 "bind replays the latched relay-delay floor (the manager's) and connection tier (the tier consumer's), and either "
			 "apply returns early without its target. Was G-07's bind-then-pull order, on the timing and connection relays"));
	SimNetLanes* lanes = findDriverSimNetLanes(world, ognet::Role::Client);
	const bool detached = lanes == nullptr;
	if (detached)
	{
		const UNetDriver* const driver = world.GetNetDriver();
		UE_LOG(LogOGNet, Error,
			TEXT("[SimNet.Host] refused role=client driver=%s: simulation traffic needs the og-net driver"),
			driver != nullptr ? *driver->GetClass()->GetName() : TEXT("none"));
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
				TEXT("Simulation traffic needs the og-net driver: this client simulates nothing"));
		}
		m_detachedSimNetLanes = SimNetLanes::makeDetached(ognet::Role::Client, ognet::EndpointConfig{});
		lanes = m_detachedSimNetLanes.Get();
	}
	m_simNetLanes = lanes;
	lanes->setOnRegisteredRemoved([this](uint8_t simId) { onSimNetRegisteredRemoved(simId); });
	lanes->setStopHandler([this]() { onSimNetLanesStopping(/*isServer=*/false); });
	lanes->bindClientSession(*this);
	logSimNetHostLine(/*isServer=*/false, detached);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetLanesStopping(bool isServer)
{
	std::vector<unsigned int> ids;
	ids.reserve(m_registeredComponentsById.size());
	for (const auto& entry : m_registeredComponentsById)
		ids.push_back(entry.first);
	for (const unsigned int id : ids)
	{
		typename TraitsT::Component* const component = m_registeredComponentsById[id].Get();
		unregisterCharacter(id, component, isServer);
		m_simUnregisteredIds.insert(id);
	}
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
void UESimulationSession<TraitsT>::onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
{
    // ⛔G-08  docs/UESimulationSession-guards.md
    const bool hadAnyTier =
        m_replicatedTierConsumer.has_value() && m_replicatedTierConsumer->hasReceivedTier();

    applyReplicatedConnectionTier(newTier);

    applyTierTransitionStall(oldTier, newTier, hadAnyTier);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyReplicatedConnectionTier(uint8 tier)
{
    if (!m_replicatedTierConsumer.has_value())
        return;

    m_replicatedTierConsumer->onReplicatedTierReceived((int32)tier);
    recomputeAndPublishEffectiveInputDelay();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyReplicatedRelayDelayFloor(uint8 floorTicks)
{
    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 clampedFloor = clampRelayDelayFloorTicks((int32)floorTicks, cfg);
    if (clampedFloor != (int32)floorTicks)
    {
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
            (unsigned int)floorTicks, clampedFloor);
    }

    m_manager->setRelayDelayFloorTicks(clampedFloor);

    logRelayDelayFloorAdvisory(clampedFloor);

    const int32 deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

    if (deltaDelayTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor = %d ticks, effective delay delta=%d, no stall"),
            clampedFloor, deltaDelayTicks);
        return;
    }

    requestInputDelayIncreaseStall(deltaDelayTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[RelayDelayFloor] floor = %d ticks, UPWARD, requesting %d-tick prediction stall"),
        clampedFloor, deltaDelayTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::logRelayDelayFloorAdvisory(int32 floorTicks)
{
    if (!m_manager.has_value())
        return;

    // ⛔G-09  docs/UESimulationSession-guards.md
    switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
    {
    case RelayDelayFloorAdvisory::BelowHiccupBaseline:
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) — the one value that is neither"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::UniformDFairnessActive:
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active; tiers and LAN override are inert"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::None:
        break;
    }
}

template <typename TraitsT>
int32 UESimulationSession<TraitsT>::recomputeAndPublishEffectiveInputDelay()
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

    const int32 delayTicks = (int32)m_replicatedTierConsumer->effectiveInputDelayTicks();
    const int32 deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

    m_lastPublishedEffectiveInputDelayTicks = delayTicks;
    publishClientEffectiveInputDelayTicks(delayTicks);

    UE_LOG(LogOGNet, Log,
        TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
        delayTicks,
        (int)m_replicatedTierConsumer->currentTierIndex(),
        m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
        m_manager.has_value() ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

    return deltaDelayTicks;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier)
{
    if (oldTier == newTier)
        return;

    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 stallTicks = (int32)shouldStallForTierTransition(
        (int32)oldTier, (int32)newTier, hadAnyTier, cfg);

    if (stallTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[ConnectionTier] tier %u -> %u hadAnyTier=%d, no stall"),
            (unsigned int)oldTier, (unsigned int)newTier, hadAnyTier ? 1 : 0);
        return;
    }

    requestInputDelayIncreaseStall(stallTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[ConnectionTier] tier %u -> %u UPWARD, requesting %d-tick prediction stall"),
        (unsigned int)oldTier, (unsigned int)newTier, stallTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::publishServerClock_GameThread()
{
	if (m_manager.has_value() && !m_manager->runsPrediction() && m_simNetServer.has_value())
		m_simNetServer->publishClock(static_cast<uint32_t>(getServerClock().getTick()));
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::readSessionKnobs(bool worldIsAuthority) -> SessionKnobs
{
	int32 configuredRelayDelayFloorTicks = -1;
	// ⛔G-14  docs/UESimulationSession-guards.md
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniFloorTicks = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GEngineIni))
		{
			configuredRelayDelayFloorTicks = iniFloorTicks;
		}
	}

	// ⛔G-15  docs/UESimulationSession-guards.md

	int32 configuredCorrectionRotationK = -1;
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniK = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GEngineIni))
		{
			configuredCorrectionRotationK = iniK;
		}
	}

	bool    hasIniResimTriggerPolicy = false;
	FString configuredResimTriggerPolicy;
	// ⛔G-16  docs/UESimulationSession-guards.md
	if (GConfig != nullptr)
	{
		FString iniPolicy;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GEngineIni))
		{
			hasIniResimTriggerPolicy = true;
			configuredResimTriggerPolicy = iniPolicy.TrimStartAndEnd();
		}
	}

	bool  hasIniRedundancyFrames = false;
	int32 configuredRedundancyFrames = 0;
	bool  hasIniRelayResendEntries = false;
	int32 configuredRelayResendEntries = 0;
	// ⛔G-27  docs/UESimulationSession-guards.md
	if (GConfig != nullptr)
	{
		int32 iniFrames = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RedundancyFrames"), iniFrames, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RedundancyFrames"), iniFrames, GEngineIni))
		{
			hasIniRedundancyFrames = true;
			configuredRedundancyFrames = iniFrames;
		}
		int32 iniEntries = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayResendEntries"), iniEntries, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayResendEntries"), iniEntries, GEngineIni))
		{
			hasIniRelayResendEntries = true;
			configuredRelayResendEntries = iniEntries;
		}
	}

	bool  hasIniSimTrafficImmediate = false;
	int32 configuredSimTrafficImmediate = 0;
	if (GConfig != nullptr)
	{
		int32 iniImmediate = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("SimTrafficImmediate"), iniImmediate, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("SimTrafficImmediate"), iniImmediate, GEngineIni))
		{
			hasIniSimTrafficImmediate = true;
			configuredSimTrafficImmediate = iniImmediate;
		}
	}

	bool  hasIniRecentreSettleSamples = false;
	int32 configuredRecentreSettleSamples = 0;
	if (GConfig != nullptr)
	{
		int32 iniSamples = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RecentreSettleSamples"), iniSamples, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RecentreSettleSamples"), iniSamples, GEngineIni))
		{
			hasIniRecentreSettleSamples = true;
			configuredRecentreSettleSamples = iniSamples;
		}
	}

	return SessionKnobs{ configuredRelayDelayFloorTicks, configuredCorrectionRotationK,
		hasIniResimTriggerPolicy, configuredResimTriggerPolicy,
		hasIniRedundancyFrames, configuredRedundancyFrames,
		hasIniRelayResendEntries, configuredRelayResendEntries,
		hasIniSimTrafficImmediate, configuredSimTrafficImmediate,
		hasIniRecentreSettleSamples, configuredRecentreSettleSamples };
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applySessionKnobs(const SessionKnobs& knobs)
{
	if (knobs.configuredRelayDelayFloorTicks >= 0)
	{
		const int32 clampedFloor = clampRelayDelayFloorTicks(
			knobs.configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
		if (clampedFloor != knobs.configuredRelayDelayFloorTicks)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"),
				knobs.configuredRelayDelayFloorTicks, clampedFloor);
		}
		m_manager->setRelayDelayFloorTicks(clampedFloor);
		UE_LOG(LogOGNet, Log,
			TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), clampedFloor);

		logRelayDelayFloorAdvisory(clampedFloor);
	}

	if (knobs.configuredCorrectionRotationK != -1)
	{
		const int32 clampedK = correctionRotation::clampK(knobs.configuredCorrectionRotationK);
		if (clampedK != knobs.configuredCorrectionRotationK)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"),
				knobs.configuredCorrectionRotationK, clampedK);
		}
		m_manager->setCorrectionRotationK(clampedK);
	}

	const int32 sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
	UE_LOG(LogOGNet, Warning,
		TEXT("[StateRotation] session K = %d (%s)"),
		sessionCorrectionRotationK,
		knobs.configuredCorrectionRotationK != -1 ? TEXT("ini override") : TEXT("compiled default"));

	const int32 sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
	checkf(m_simNetServer.has_value(),
		TEXT("UESimulationSession::applySessionKnobs: the og-sim-net server publishes the session floor, so beginPlay emplaces "
			 "it before the knobs are applied. Was the timing relay's spawn-before-the-manager order (og-netcode-v2-input-relay "
			 "T11) and its 'no timing relay at manager BeginPlay' warning"));
	m_simNetServer->publishFloor(static_cast<uint8_t>(sessionRelayDelayFloorTicks));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyResimTriggerPolicy(SessionKnobs& knobs)
{
	// ⛔G-17  docs/UESimulationSession-guards.md
	if (knobs.hasIniResimTriggerPolicy)
	{
		if (knobs.configuredResimTriggerPolicy.Equals(TEXT("OnDisagreement"), ESearchCase::IgnoreCase))
		{
			m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
		}
		else if (knobs.configuredResimTriggerPolicy.Equals(TEXT("FrontierExact"), ESearchCase::IgnoreCase))
		{
			m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
				TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"),
				*knobs.configuredResimTriggerPolicy);
			knobs.hasIniResimTriggerPolicy = false;
		}
	}

	const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
	const bool policyIsOnDisagreement =
		sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
	const FString depthPolicyText = policyIsOnDisagreement
		? FString::Printf(TEXT("skip beyond %d ticks"), sessionTimeConfig.rollbackWindowTicks)
		: FString(TEXT("off (inert under FrontierExact)"));
	UE_LOG(LogOGNet, Warning,
		TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
		policyIsOnDisagreement ? TEXT("OnDisagreement") : TEXT("FrontierExact"),
		knobs.hasIniResimTriggerPolicy ? TEXT("ini override") : TEXT("compiled default"),
		*depthPolicyText);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyLatencyKnobs(const SessionKnobs& knobs)
{
	if (knobs.hasIniRedundancyFrames)
	{
		const int32 clampedFrames = latencyKnobs::clampRedundancyFrames(knobs.configuredRedundancyFrames);
		if (clampedFrames != knobs.configuredRedundancyFrames)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d"),
				knobs.configuredRedundancyFrames, clampedFrames);
		}
		m_manager->setRedundancyFrames(clampedFrames);
	}

	if (knobs.hasIniRelayResendEntries)
	{
		const int32 clampedEntries = latencyKnobs::clampRelayResendEntries(knobs.configuredRelayResendEntries);
		if (clampedEntries != knobs.configuredRelayResendEntries)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d"),
				knobs.configuredRelayResendEntries, clampedEntries);
		}
		m_manager->setRelayResendEntries(clampedEntries);
	}

	const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
	UE_LOG(LogOGNet, Warning,
		TEXT("[LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)"),
		sessionTimeConfig.redundancyFrames,
		sessionTimeConfig.relayResendEntries,
		(knobs.hasIniRedundancyFrames || knobs.hasIniRelayResendEntries) ? TEXT("ini") : TEXT("compiled"));

	if (knobs.hasIniRecentreSettleSamples)
	{
		const int32 clampedSamples = FMath::Max(knobs.configuredRecentreSettleSamples, 0);
		if (clampedSamples != knobs.configuredRecentreSettleSamples)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d"),
				knobs.configuredRecentreSettleSamples, clampedSamples);
		}
		m_manager->setRecentreSettleSamples(static_cast<uint32_t>(clampedSamples));
	}
	UE_LOG(LogOGNet, Warning,
		TEXT("[ClockRecentre.Config] recentreSettleSamples=%u (%s)"),
		m_manager->getTimeConfig().recentreSettleSamples,
		knobs.hasIniRecentreSettleSamples ? TEXT("ini") : TEXT("compiled"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps)
{
    static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples,
        "PROBE A's window must stay the same length as ResimGateProbe's, so one [ResimProbe.Frame] "
        "window lines up against the surrounding [ResimProbe.Gate] windows without a resim-tick "
        "count on the line. Was the COMPARABLE WITH ResimGateProbe caution in PROBE A's banner.");
    {
        FrameHealthWindowSummary frameSummary;
        const bool frameWindowClosed = m_frameHealthProbe.noteFrame(
            static_cast<uint64>(GFrameCounter),
            static_cast<uint32>(firstUpcomingSimTick),
            static_cast<uint32>(numSteps),
            static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0),
            frameSummary);

        if (frameWindowClosed)
        {
            const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
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

    // ⛔G-18  docs/UESimulationSession-guards.md
    if (!m_simNetServer.has_value())
        return;

    if (const UWorld* world = m_owner.GetWorld())
    {
        if (const UNetDriver* netDriver = world->GetNetDriver())
        {
            const uint32 tickRateHz =
                static_cast<uint32>(FMath::Max(1, netDriver->GetNetServerMaxTickRate()));
            const uint64 nowMicros =
                static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0);

            for (UNetConnection* conn : netDriver->ClientConnections)
            {
                if (conn == nullptr)
                    continue;

                {
                    static bool s_loggedPacketBudget = false;
                    if (!s_loggedPacketBudget)
                    {
                        s_loggedPacketBudget = true;
                        const int32 usableBits = conn->GetMaxSingleBunchSizeBits();
                        UE_LOG(LogOGNet, Warning,
                            TEXT("[PacketBudget] usableSingleBunchBytes = %d, handlerBits = %d "
                                 "(maxPacket=%d, usableBits=%d)"),
                            // ∴D-01  docs/UESimulationSession-rationale.md
                            (usableBits / 32) * 4, conn->MaxPacketHandlerBits,
                            conn->MaxPacket, usableBits);
                    }
                }

                ConnectionBudgetWindowSummary budget;
                // ⛔G-19  docs/UESimulationSession-guards.md
                const bool budgetWindowClosed = m_connectionBudgetProbe.noteSample(
                    static_cast<uint32>(conn->GetUniqueID()),
                    conn->CurrentNetSpeed,
                    conn->QueuedBits,
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalBytes)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPackets)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPacketsLost)),
                    tickRateHz, nowMicros, budget);

                if (!budgetWindowClosed)
                    continue;

                char line[256];

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u frames=%u elapsedMs=%u "
                    "netSpeedBps=%d allowanceBytesPerTick=%u outBytes=%u "
                    "bytesPerTick=%u occupancyPctX10=%u",
                    budget.connectionId, budget.samples, budget.elapsedMs,
                    budget.netSpeedBps, budget.allowanceBytesPerTick,
                    budget.outBytes, budget.bytesPerSample, budget.occupancyPctX10);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u queuedBits min=%d max=%d "
                    "mean=%d notReadyFrames=%u outPackets=%u bytesPerPacket=%u lost=%u",
                    budget.connectionId, budget.queuedBitsMin, budget.queuedBitsMax,
                    budget.queuedBitsMean, budget.notReadySamples,
                    budget.outPackets, budget.bytesPerPacket, budget.outPacketsLost);
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
    if (!m_simNetServer.has_value())
        return;

    OG_LOG_ROUTED(LogOGNet, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    m_simNetServer->releaseForStep(firstTick, numSteps);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterAuthorityBatch(int32 firstUpcomingSimTick)
{
    if (m_simNetServer.has_value())
        m_simNetServer->afterBatch(firstUpcomingSimTick);
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
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    const ogsimnet::RosterEntry* const entry =
        client != nullptr ? client->roster().entryFor(static_cast<uint8_t>(storageKey)) : nullptr;
    if (entry == nullptr || entry->physicsSlot == ogsimnet::kRosterNoSlot)
        return std::nullopt;
    return static_cast<uint32>(entry->physicsSlot);
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
template <typename FunctionT>
void UESimulationSession<TraitsT>::forEachAuthorityCorrectionTick(FunctionT&& function) const
{
    if (!m_simNetServer.has_value())
        return;
    for (const auto& entry : m_simChannelsById)
    {
        const CorrectionStateBuffer& buffer = std::as_const(*entry.second).getSyncedCorrectionStateBuffer();
        if (buffer.payload().size() < correctionStateBuffer::kHeaderBytes)
            continue;
        const uint32 correctionTick = buffer.template readFromBuffer<uint32>(correctionStateBuffer::kTickOffset);
        if (correctionTick == 0u)
            continue;
        function(entry.first, correctionTick);
    }
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::addSimNetCharacter(unsigned int id, typename TraitsT::Component& owner) -> SimChannel*
{
    if (!m_simNetServer.has_value())
        return nullptr;
    checkf(id != 0u && id <= 0xFFu,
        TEXT("UESimulationSession::addSimNetCharacter: og-sim-net's roster keys a character by its sim id (1..255); id %u is "
             "not one"), id);
    AActor* const pawn = owner.GetOwner();
    const auto foundSlot = m_authoritativeSlotsByActor.find(pawn);
    const uint8_t physicsSlot =
        foundSlot != m_authoritativeSlotsByActor.end() ? static_cast<uint8_t>(foundSlot->second) : ogsimnet::kRosterNoSlot;
    const UOGNetConnection* const ownerConnection = Cast<UOGNetConnection>(GetRootNetConnection(pawn));
    const SimConnectionId ownerConn = ownerConnection != nullptr ? ownerConnection->OGNetId : kInvalidSimConnection;
    const uint8_t localIndex = GetPlayerSlotForActor(pawn);
    const FTCHARToUTF8 debugName(pawn != nullptr ? *pawn->GetName() : TEXT("none"));
    return m_simNetServer->template addCharacter<Simulatable>(static_cast<uint8_t>(id), physicsSlot, ownerConn, localIndex,
        std::string(debugName.Get()));
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::registeredChannelFor(unsigned int id) const -> SimChannel*
{
    const auto found = m_simChannelsById.find(id);
    return found != m_simChannelsById.end() ? found->second : nullptr;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetSimNetCharacter(unsigned int id, const AActor* pawn, bool isAuthority)
{
    m_registeredComponentsById.erase(id);
    m_simChannelsById.erase(id);
    m_authoritativeSlotsByActor.erase(pawn);
    if (id == 0u || id > 0xFFu)
        return;
    const uint8_t simId = static_cast<uint8_t>(id);
    if (isAuthority)
    {
        if (m_simNetServer.has_value() && m_simNetServer->template channel<Simulatable>(simId) != nullptr)
            m_simNetServer->removeCharacter(simId, ogsimnet::RosterRemoveReason::Left);
        return;
    }
    if (m_simNetLanes != nullptr)
        m_simNetLanes->editBindings().markUnregistered(simId);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetRegisteredRemoved(uint8_t simId)
{
    const unsigned int id = simId;
    const auto found = m_registeredComponentsById.find(id);
    checkf(found != m_registeredComponentsById.end(),
        TEXT("UESimulationSession::onSimNetRegisteredRemoved: sim id %u is registered in the roster bindings and not in the "
             "session; the session marks an id registered only after it registered it (tryRegister)"), id);
    if (found == m_registeredComponentsById.end())
        return;
    unregisterCharacter(id, found->second.Get(), /*isAuthority=*/false);
    m_simUnregisteredIds.insert(id);
}

template <typename TraitsT>
ClientBindStep UESimulationSession<TraitsT>::clientBindStep(unsigned int id, bool engineSaysLocal)
{
    if (m_simNetLanes == nullptr || m_simNetLanes->isDetached())
    {
        if (!m_detachedClientAbandonLogged)
        {
            m_detachedClientAbandonLogged = true;
            UE_LOG(LogOGMgmt, Error,
                TEXT("DeferredReg: id=%u is not registered: this client's world has no og-net sim lanes, so no roster entry "
                     "will ever arrive. Characters stay unsimulated on this peer (one-shot)"), id);
        }
        return ClientBindStep::Abandon;
    }
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

	auto sessionKnobs = readSessionKnobs(worldIsAuthority);

	if (sessionKnobs.hasIniSimTrafficImmediate && sessionKnobs.configuredSimTrafficImmediate != 0)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("[SimNet.Host] ini [OGNetcode] SimTrafficImmediate=%d refused: immediate sim traffic needs Ph4's code; this "
				 "session runs frame-locked (0)"),
			sessionKnobs.configuredSimTrafficImmediate);
	}

	m_physicsBackend.buildWorld(*uWorld, dt, worldIsAuthority);

	if (worldIsAuthority)
	{
		if (s_instances[0] != nullptr)
		{
			checkf(false, TEXT("UESimulationSession: unexpected state"));
		}
		s_instances[0] = this;
		auto pctmloggerServer = [](const char* msg) { RouteOGMessage(msg); };
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(false, dt, typename ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

		// ⛔G-23  docs/UESimulationSession-guards.md
		m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());

		beginServerSimNet(*uWorld, worldNetMode);

		applySessionKnobs(sessionKnobs);

		// ⛔G-06  docs/UESimulationSession-guards.md
		beginTierConsumer();

		simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
		Game::installGameLoggers(worldIsAuthority);
		bindCorrectionFieldDiffGate();
	}
	else
	{
		if (s_instances[1] != nullptr)
		{
			checkf(false, TEXT("UESimulationSession: unexpected state"));
		}
		s_instances[1] = this;
		auto pctmlogger = [](const char* msg) { RouteOGMessage(msg); };
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		Game::emplaceSystems(m_systemsExec, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(/*usePrediction=*/true, dt, typename ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

		m_inputResolution.template setNeutralInput<Simulatable>(Game::neutralInput());

		beginTierConsumer();

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
	applyResimTriggerPolicy(sessionKnobs);

	applyLatencyKnobs(sessionKnobs);

	m_game.afterSessionConfigured(worldNetMode);

	{
		const std::string_view fingerprint = Game::backendFingerprint();
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Backend] backend=%hs fingerprint=%.*hs"),
			EngineTraits::kName, static_cast<int32>(fingerprint.size()), fingerprint.data());
	}

	m_physicsBackend.startStepping(*uWorld, dt, worldIsAuthority);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::endPlay()
{
	// ⛔G-20  docs/UESimulationSession-guards.md
	m_physicsBackend.onEndPlayBegin();

	detachFromSimNetLanes();

	// ⛔G-21  docs/UESimulationSession-guards.md
	m_simNetServer.reset();
	m_simNetServerHost.reset();
	m_registeredComponentsById.clear();
	m_simChannelsById.clear();
	m_authoritativeSlotsByActor.clear();
	m_simUnregisteredIds.clear();

	m_replicatedTierConsumer.reset();

	if (s_instances[0] == this)
		s_instances[0] = nullptr;
	if (s_instances[1] == this)
		s_instances[1] = nullptr;

	if (UWorld* World = m_owner.GetWorld())
	{
		m_latencyBudgetDiagnostics.end(*World);

		m_physicsBackend.onEndPlayWorld(*World);
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

	onPostSimulationGameThread();

	publishServerClock_GameThread();

	if (m_detachedSimNetLanes.IsValid() && m_simNetLanes == m_detachedSimNetLanes.Get())
	{
		m_detachedSimNetLanes->afterDispatch();
		m_detachedSimNetLanes->onTickFlush();
	}

	m_physicsBackend.updateVisualization_GameThread();

	Game::afterVisualization(*this);

	m_latencyBudgetDiagnostics.tick_GameThread(m_physicsBackend.editDiagnostics());
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

    SimChannel* channel = nullptr;
    if (record.isAuthority)
    {
        channel = addSimNetCharacter(id, owner);
        if (channel == nullptr)
        {
            UE_LOG(LogOGMgmt, Error,
                TEXT("tryRegister: id=%u refused by the og-sim-net server (its [Roster.Refused] line names why); the character "
                     "stays unsimulated"), id);
            m_pendingRegistrations.erase(it);
            return TryRegisterStatus::Refused;
        }
        registerSimulatable<Simulatable>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/*channel,
            /*authorityOwner=*/*channel);
    }
    else
    {
        typename SimNetLanes::Client* const client = m_simNetLanes != nullptr ? m_simNetLanes->client() : nullptr;
        channel = client != nullptr && id != 0u && id <= 0xFFu ? client->channel(static_cast<uint8_t>(id)) : nullptr;
        if (channel == nullptr)
            return TryRegisterStatus::Pending;
        const bool hasProvider = static_cast<bool>(record.inputProvider);
        checkf(m_simNetLanes->bindings().verdict(static_cast<uint8_t>(id), hasProvider).step == ogsimnet::BindStep::Ready,
            TEXT("UESimulationSession::tryRegister: id=%u registers %s an input provider, and its roster entry says the other: "
                 "the provider is built for the engine's autonomous proxy, which clientBindStep checked against the roster. Was "
                 "the engine role alone (isLocallyPredicted)"), id, hasProvider ? TEXT("with") : TEXT("without"));
        registerSimulatable<Simulatable>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/*channel,
            /*inputProvider=*/m_latencyBudgetDiagnostics.wrapInputProvider(
                id, owner, std::move(record.inputProvider)));
        if (hasProvider)
        {
            channel->setOnInputHandedCallback([this, id](const InputBundleBuffer& bundle)
            {
                bundle.template forEachSlot<PlayerInput>([this, id](uint32 captureTick, const PlayerInput&)
                {
                    m_latencyBudgetDiagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
                        latencyBudget::Point::Handed);
                });
            });
        }
        m_simNetLanes->editBindings().markRegistered(static_cast<uint8_t>(id));
    }
    m_registeredComponentsById[id] = &owner;
    m_simChannelsById[id] = channel;
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    m_game.afterRegistered(id, record.isAuthority);

    // ⛔G-25  docs/UESimulationSession-guards.md
    m_manager->notifyCharacterRegistered(id);

    m_physicsBackend.onRegistered_GameThread(owner, id);

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
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

    // ⛔G-30  docs/UESimulationSession-guards.md
    if (m_simUnregisteredIds.erase(id) != 0u)
        return;

    unregisterCharacter(id, &owner, isAuthority);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterCharacter(unsigned int id, typename TraitsT::Component* owner, bool isAuthority)
{
    // ⛔G-26  docs/UESimulationSession-guards.md
    if (m_storage.template has<Simulatable>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

    SimChannel* const channel = registeredChannelFor(id);
    unregisterSimulatable<Simulatable>(
        m_storage, m_reconciliation, m_inputResolution, m_netSync,
        id,
        /*predictionOwner=*/channel,
        /*authorityOwner=*/isAuthority ? channel : nullptr);
    if (channel != nullptr)
        channel->clearOnInputHandedCallback();

    // ⛔G-13  docs/UESimulationSession-guards.md
    forgetSimNetCharacter(id, owner != nullptr ? owner->GetOwner() : nullptr, isAuthority);

    m_game.onUnregistered(id);

    m_latencyBudgetDiagnostics.forgetCharacter(id);

    if (owner != nullptr)
        m_physicsBackend.onUnregistered_GameThread(*owner, id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
