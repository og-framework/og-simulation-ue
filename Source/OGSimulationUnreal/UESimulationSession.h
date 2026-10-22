// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/SimulationComparison.h"
#include "OGSimulation/SimulationLog.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"
#include "OGSimulationUnreal/ISimulationConnectionRelayListener.h"
#include "OGSimulationUnreal/ISimulationInputRelayListener.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"
#include "OGSimulationUnreal/SimulationInputRelay.h"
#include "OGSimulationUnreal/SimulatableNetSyncComponent.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/CorrectionRotation.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/Network/RelayReadProbe.h"
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
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
#include <utility>

class AActor;

#if !UE_BUILD_SHIPPING
OGSIMULATIONUNREAL_API int32 consumeTestGameThreadHitchMs();
OGSIMULATIONUNREAL_API void sleepTestGameThreadHitch(int32 hitchMs);
#endif

#define HasAuthority HasAuthority_is_not_the_peer_role_in_UESimulationSession_use_worldIsAuthority_or_runsPrediction

enum class TryRegisterStatus { Pending, Ready };

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
class UESimulationSession : public ISimulationTimingRelayListener,
                            public ISimulationConnectionRelayListener,
                            public ISimulationInputRelayListener
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
	friend typename TraitsT::template Backend<UESimulationSession>;
	friend typename TraitsT::template TickDriverGame<UESimulationSession>;
	friend LatencyBudgetDiagnostics;

	using Composition     = typename TraitsT::Composition;
	using EngineTraits    = typename TraitsT::EngineTraits;
	using StaticData      = typename TraitsT::StaticData;
	using Storage         = typename Composition::Storage;
	using Reconciliation  = typename Composition::Reconciliation;
	using InputResolution = typename Composition::InputResolution;
	using NetSync         = typename Composition::NetSync;
	using SystemsExec     = typename Composition::SystemsExec;
	using IntegrationExec = typename Composition::IntegrationExec;
	using ManagerType     = typename Composition::Manager;
	using Backend         = typename TraitsT::template Backend<UESimulationSession>;
	using Simulatable     = typename TraitsT::Simulatable;
	using PlayerInput     = typename TraitsT::PlayerInput;
	using NetConfig       = typename TraitsT::NetConfig;
	using InputProvider   = typename LatencyBudgetDiagnostics::InputProvider;

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
	getDelayedInputComponentsById() const { return m_delayedInputComponentsById; }

	// ⛔G-11  docs/UESimulationSession-guards.md
	static UESimulationSession* instanceFor(bool isAuthority)
	{
		return isAuthority ? s_instances[0] : s_instances[1];
	}

	FSmallSimulationStateSyncBuffer& getSyncedTimingBuffer();
	void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
	{
		m_onTimingInfoReceivedCallback = std::move(fn);
	}

	virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) override
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

	virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier) override;

	virtual void onConnectionTierReplayed(uint8_t tier) override;

	virtual void onInputRelayHostReady(ASimulationInputRelay& host) override;

	const ReplicatedTierConsumer* getReplicatedTierConsumer() const
	{
		return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
	}

	virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;

	virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) override;

	using ReceptionCoordinator = ServerReceptionCoordinator<FUEConnectionHandle, Simulatable>;
	bool hasServerTierWiring() const { return m_receptionCoordinator.has_value(); }

	ReceptionCoordinator* getReceptionCoordinator()
	{
		return m_receptionCoordinator.has_value() ? &*m_receptionCoordinator : nullptr;
	}

	int32 getServerReceptionTick() const
	{
		return static_cast<int32>(getServerClock().getSimulationStep().getTick());
	}

	TryRegisterStatus tryRegister(unsigned int id, Simulatable&& simulatable, typename TraitsT::Component& owner,
		InputProvider&& inputProvider, bool isAuthority);

	void unregisterFromNewFramework(unsigned int id, typename TraitsT::Component& owner, bool isAuthority);

	void noteDelayedInputComponent(unsigned int id, typename TraitsT::Component& component);

	void deliverRemoteInput(unsigned int id, uint32 captureTick, const PlayerInput& input);

	void relayRemoteInput(unsigned int id, uint32 captureTick, uint8 dA, const PlayerInput& input);

private:
	void requestInputDelayIncreaseStall(int32 deltaDelayTicks)
	{
		// ⛔G-03  docs/UESimulationSession-guards.md
		if (!m_manager.has_value() || !m_manager->runsPrediction())
			return;

		m_manager->editClientClock().requestInputDelayIncreaseStall(deltaDelayTicks);
	}

	void beginTierConsumer();

	void replayLatchedClientValues(UWorld* world);

	void publishServerClock_GameThread();

	void applyReplicatedConnectionTier(uint8 tier);

	void applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease);

	// ⛔G-04  docs/UESimulationSession-guards.md
	void logRelayDelayFloorAdvisory(int32 floorTicks);

	// ⛔G-05  docs/UESimulationSession-guards.md
	int32 recomputeAndPublishEffectiveInputDelay();

	void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);

	ASimulationTimingRelay* findTimingRelay();

	struct SessionKnobs
	{
		int32   configuredRelayDelayFloorTicks;
		int32   configuredCorrectionRotationK;
		bool    hasIniResimTriggerPolicy;
		FString configuredResimTriggerPolicy;
	};

	static SessionKnobs readSessionKnobs(bool worldIsAuthority);

	void applySessionKnobs(const SessionKnobs& knobs);

	void applyResimTriggerPolicy(SessionKnobs& knobs);

	void onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps);

	void onPostPhysicsStep();

	template <typename LoggerT>
	void beginReceptionCoordinator(const LoggerT& logger);

	void releaseDelayedInputsForStep(int32 firstTick, int32 numSteps);

	void forgetReceptionOwner(unsigned int id);

	static void bindCorrectionFieldDiffGate()
	{
		// ⛔G-22  docs/UESimulationSession-guards.md
		correctionFieldDiff::setEnabledPredicate(
			[]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
	}

private:
	SessionOwner& m_owner;
	Game&         m_game;

	LatencyBudgetDiagnostics m_latencyBudgetDiagnostics{ std::as_const(*this) };

	typename TraitsT::BackendWorlds m_backendWorlds;

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
	Backend m_backend{ *this, m_backendWorlds };

	std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

	int32 m_lastPublishedEffectiveInputDelayTicks = 0;

	std::optional<ReceptionCoordinator> m_receptionCoordinator;

	FrameHealthProbe m_frameHealthProbe;

	// ⛔G-12  docs/UESimulationSession-guards.md
	RelayWriteProbe       m_relayWriteProbe{
		RelayStageCapacity{ static_cast<uint32>(relayedInputRing::kMaxDepth) } };
	ConnectionBudgetProbe m_connectionBudgetProbe;

	std::unordered_map<unsigned int, TWeakObjectPtr<typename TraitsT::Component>>
		m_delayedInputComponentsById;

	std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

	ASimulationTimingRelay* m_timingRelay = nullptr;

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
		static_assert(std::is_same_v<ManagerType, typename TraitsT::template TickDriverGame<UESimulationSession>::Manager>,
			"UESimulationSession::ManagerType must BE the manager type the tick-driver game binding names "
			"(TraitsT::TickDriverGame<UESimulationSession>::Manager). ManagerType is private, so a backend host declared "
			"outside this class (the tick-driver backend, whose step driver is SimulationStepDriver<Manager, ...>) names "
			"the manager through the game binding; a ManagerType that drifts from it hands that host a different "
			"SimulationManager instantiation than m_manager. Was the owner's ManagerType row (added by "
			"og-simulationscheduler-withjolt task 81), which compared it with the game's composition struct; moved here "
			"by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(__builtin_offsetof(UESimulationSession, m_backendWorlds)
		                  < __builtin_offsetof(UESimulationSession, m_physAdapter)
		           && __builtin_offsetof(UESimulationSession, m_physAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_physReaderAdapter)
		           && __builtin_offsetof(UESimulationSession, m_physAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_queryAdapter)
		           && __builtin_offsetof(UESimulationSession, m_queryAdapter)
		                  < __builtin_offsetof(UESimulationSession, m_integrationLayer),
			"UESimulationSession members destruct in REVERSE declaration order. The backend's worlds must outlive every "
			"adapter bound to them (the Jolt step world; the reader holds the body adapter's bind table, the integration "
			"layer holds the adapters), so m_backendWorlds sits above the adapters in every configuration (it is empty "
			"on a self-scheduled engine). Keep m_backendWorlds < m_physAdapter < {m_physReaderAdapter, m_queryAdapter} < "
			"m_integrationLayer. Was the owner's destruction-order row, design D's compile-time row 'runtime < world < "
			"adapters' before that (og-simulationscheduler-withjolt task 18); its runtime < world half is "
			"og-simulation-jolt's JoltTickWorlds' own row (runtime < contactCounter < stepWorld) since og-tickdriver "
			"task 10. Moved here by og-ue-simulationcomponentandmanagersplit task 24a.");
		static_assert(__builtin_offsetof(UESimulationSession, m_manager)
		                  < __builtin_offsetof(UESimulationSession, m_backend),
			"UESimulationSession members destruct in REVERSE declaration order. The backend holds the Jolt step driver, "
			"which unregisters its resync callback from the manager's client clock in its destructor, so the backend must "
			"go before the manager. Keep m_manager < m_backend. Was the owner's row, design D's compile-time row 'manager "
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
		                  < __builtin_offsetof(UESimulationSession, m_receptionCoordinator),
			"UESimulationSession members destruct in REVERSE declaration order. The reception coordinator is emplaced "
			"from the manager's TimeConfig and keeps a reference to it (ServerReceptionCoordinator borrows its config), so it "
			"must be destroyed before the manager. Keep m_manager < m_receptionCoordinator. Added when the input reception "
			"moved here (og-ue-simulationcomponentandmanagersplit task 26a): in the owner the coordinator was declared above "
			"the member that holds the manager, and only the owner's EndPlay reset kept it from outliving that TimeConfig.");
		static_assert(RemoteInputDeliverySink<UESimulationSession, PlayerInput>,
			"UESimulationSession must satisfy RemoteInputDeliverySink so ServerReceptionCoordinator can drive "
			"remote-input delivery through it: the component binding's RPC tail hands the session to "
			"receiveInputBundle as its delivery sink. Was the assertion beside the owner's deliverRemoteInput; moved "
			"here with the sink by og-ue-simulationcomponentandmanagersplit task 26a.");
		static_assert(RemoteInputRelaySink<UESimulationSession, PlayerInput>,
			"UESimulationSession must satisfy RemoteInputRelaySink so ServerReceptionCoordinator can drive the input "
			"relay through it: the component binding's RPC tail hands the session to receiveInputBundle as its relay "
			"sink. Was the assertion beside the owner's relayRemoteInput; moved here with the sink by "
			"og-ue-simulationcomponentandmanagersplit task 26a.");
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
	return m_backend.editVizQuery();
}

template <typename TraitsT>
auto UESimulationSession<TraitsT>::getVizReader() const -> const typename EngineTraits::VizReader&
{
	return m_backend.getVizReader();
}

template <typename TraitsT>
std::optional<QueryVolumeId> UESimulationSession<TraitsT>::registerVizVolume(const QueryVolumeDescriptor& descriptor,
	AActor& owner)
{
	return m_backend.registerVizVolume(descriptor, owner);
}

OGSIM_OPTIMIZE_ON

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
UESimulationSession<TraitsT>::~UESimulationSession()
{
	// ⛔G-10  docs/UESimulationSession-guards.md
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(true);
		ISimulationConnectionRelayListener::unregisterInstance(true);
		ISimulationInputRelayListener::unregisterInstance(true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(false);
		ISimulationConnectionRelayListener::unregisterInstance(false);
		ISimulationInputRelayListener::unregisterInstance(false);
	}
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginTierConsumer()
{
	m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
	recomputeAndPublishEffectiveInputDelay();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::replayLatchedClientValues(UWorld* world)
{
	if (ASimulationConnectionRelay* connectionRelay =
			ASimulationConnectionRelay::findLocalClientRelay(world))
	{
		connectionRelay->replayLatchedTier();
	}

	if (ASimulationTimingRelay* timingRelay = findTimingRelay())
	{
		timingRelay->replayLatchedRelayDelayFloor();
	}
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
void UESimulationSession<TraitsT>::onConnectionTierReplayed(uint8_t tier)
{
    applyReplicatedConnectionTier(tier);
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
void UESimulationSession<TraitsT>::onInputRelayHostReady(ASimulationInputRelay& host)
{
    AActor* ownerActor = host.GetOwner();
    if (ownerActor == nullptr)
        return;

    typename TraitsT::Character* character = Cast<typename TraitsT::Character>(ownerActor);
    if (character == nullptr)
        return;

    if (USimulatableNetSyncComponent* sibling =
            character->template FindComponentByClass<USimulatableNetSyncComponent>())
    {
        sibling->attachInputRelayHost(&host);
    }
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/true);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRelayDelayFloorReplayed(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/false);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease)
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

    if (!payForIncrease || deltaDelayTicks <= 0)
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
ASimulationTimingRelay* UESimulationSession<TraitsT>::findTimingRelay()
{
    if (m_timingRelay) return m_timingRelay;
    for (TActorIterator<ASimulationTimingRelay> it(m_owner.GetWorld()); it; ++it)
    {
        m_timingRelay = *it;
        UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: timing relay found"));
        return m_timingRelay;
    }
    return nullptr;
}

template <typename TraitsT>
FSmallSimulationStateSyncBuffer& UESimulationSession<TraitsT>::getSyncedTimingBuffer()
{
    ASimulationTimingRelay* relay = findTimingRelay();
    checkf(relay != nullptr, TEXT("UESimulationSession::getSyncedTimingBuffer: relay not found"));
    return relay->editBuffer();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::publishServerClock_GameThread()
{
	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}
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

	return SessionKnobs{ configuredRelayDelayFloorTicks, configuredCorrectionRotationK,
		hasIniResimTriggerPolicy, configuredResimTriggerPolicy };
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
	if (ASimulationTimingRelay* timingRelay = findTimingRelay())
	{
		timingRelay->setRelayDelayFloorTicks((uint8)sessionRelayDelayFloorTicks);
	}
	else
	{
		UE_LOG(LogOGNet, Warning,
			TEXT("[RelayDelayFloor] no timing relay at manager BeginPlay — session floor %d NOT published"),
			sessionRelayDelayFloorTicks);
	}
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
    if (!m_receptionCoordinator.has_value())
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
template <typename LoggerT>
void UESimulationSession<TraitsT>::beginReceptionCoordinator(const LoggerT& logger)
{
	m_receptionCoordinator.emplace(m_manager->getTimeConfig());
	m_receptionCoordinator->setLogger(std::function<void(const char*)>(logger));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::noteDelayedInputComponent(
    unsigned int id, typename TraitsT::Component& component)
{
    m_delayedInputComponentsById[id] = &component;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::deliverRemoteInput(
    unsigned int id, uint32 captureTick, const PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    typename TraitsT::Component* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->deliverDelayedRemoteInput(captureTick, input);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::relayRemoteInput(
    unsigned int id, uint32 captureTick, uint8 dA,
    const PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    typename TraitsT::Component* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->stageRelayedInput(captureTick, dA, input);

    {
        RelayWriteWindowSummary w;
        if (m_relayWriteProbe.noteWrite(
                id, static_cast<uint64>(GFrameCounter), captureTick, w))
        {
            char line[256];

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u runs=%u writes=%u observableWrites=%u "
                "captureSpan=%u receivedX1000=%u observableX1000=%u deliverableX1000=%u "
                "replaceLatestObservableX1000=%u",
                w.ownerId, w.runs, w.writes, w.observableWrites, w.captureSpan,
                w.receivedX1000, w.observableX1000, w.deliverableX1000,
                w.replaceLatestObservableX1000);
            RouteOGMessage(line);

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u writesPerFrame p50=%u p99=%u%s "
                "max=%u emptyFrames=%u nonConsecutive=%u missedCaptureTicks=%u "
                "captures=[%u,%u] discont=%u",
                w.ownerId, w.p50, w.p99, w.p99Saturated ? "+" : "", w.maxRun,
                w.emptyFrames, w.nonConsecutiveWrites, w.missedCaptureTicks,
                w.firstCaptureTick, w.lastCaptureTick, w.discontinuities);
            RouteOGMessage(line);
        }
    }
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::releaseDelayedInputsForStep(int32 firstTick, int32 numSteps)
{
    if (!m_receptionCoordinator.has_value())
        return;

    OG_LOG_ROUTED(LogOGNet, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    auto deliver = [this](unsigned int id, uint32 captureTick,
                          const PlayerInput& input) -> bool
    {
        const auto it = m_delayedInputComponentsById.find(id);
        if (it == m_delayedInputComponentsById.end())
            return false;

        if (it->second.Get() == nullptr)
        {
            m_delayedInputComponentsById.erase(it);
            return false;
        }

        deliverRemoteInput(id, captureTick, input);
        return true;
    };

    m_receptionCoordinator->template releaseDelayedInputs<Simulatable>(
        firstTick, numSteps, deliver);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetReceptionOwner(unsigned int id)
{
    if (m_receptionCoordinator.has_value())
    {
        m_receptionCoordinator->forgetOwner(id);
    }
    // ⛔G-13  docs/UESimulationSession-guards.md
    m_delayedInputComponentsById.erase(id);

    m_relayWriteProbe.forgetOwner(id);
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

	m_backend.buildWorld(*uWorld, dt, worldIsAuthority);

	if (worldIsAuthority)
	{
		if (s_instances[0] != nullptr)
		{
			checkf(false, TEXT("UESimulationSession: unexpected state"));
		}
		s_instances[0] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/true, this);
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

		applySessionKnobs(sessionKnobs);

		// ⛔G-06  docs/UESimulationSession-guards.md
		beginTierConsumer();
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/true, this);
		ISimulationInputRelayListener::registerInstance(/*isAuthority=*/true, this);

		beginReceptionCoordinator(pctmloggerServer);
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
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/false, this);
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

		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);
		ISimulationInputRelayListener::registerInstance(/*isAuthority=*/false, this);
		// ⛔G-07  docs/UESimulationSession-guards.md
		replayLatchedClientValues(uWorld);
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

	m_game.afterSessionConfigured(worldNetMode);

	{
		const std::string_view fingerprint = Game::backendFingerprint();
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Backend] backend=%hs fingerprint=%.*hs"),
			EngineTraits::kName, static_cast<int32>(fingerprint.size()), fingerprint.data());
	}

	m_backend.startStepping(*uWorld, dt, worldIsAuthority);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::endPlay()
{
	// ⛔G-20  docs/UESimulationSession-guards.md
	m_backend.onEndPlayBegin();

	// ⛔G-21  docs/UESimulationSession-guards.md
	m_delayedInputComponentsById.clear();
	m_receptionCoordinator.reset();

	m_replicatedTierConsumer.reset();

	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/false);
	}

	if (UWorld* World = m_owner.GetWorld())
	{
		m_latencyBudgetDiagnostics.end(*World);

		m_backend.onEndPlayWorld(*World);
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

	m_backend.beforePresent_GameThread();

	onPostSimulationGameThread();

	publishServerClock_GameThread();

	m_backend.updateVisualization_GameThread();

	Game::afterVisualization(*this);

	m_latencyBudgetDiagnostics.tick_GameThread(m_backend.editDiagnostics());
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
        if (!m_backend.createBodies_GameThread(owner, *character, *record.simulatable, id))
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

    if (record.isAuthority)
    {
        registerSimulatable<Simulatable>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<Simulatable>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/m_latencyBudgetDiagnostics.wrapInputProvider(
                id, owner, std::move(record.inputProvider)));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    m_game.afterRegistered(id, record.isAuthority);

    // ⛔G-25  docs/UESimulationSession-guards.md
    m_manager->notifyCharacterRegistered(id);

    m_backend.onRegistered_GameThread(owner, id);

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

    // ⛔G-26  docs/UESimulationSession-guards.md
    if (m_storage.template has<Simulatable>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

    unregisterSimulatable<Simulatable>(
        m_storage, m_reconciliation, m_inputResolution, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

    forgetReceptionOwner(id);

    m_game.onUnregistered(id);

    m_latencyBudgetDiagnostics.forgetCharacter(id);

    m_backend.onUnregistered_GameThread(owner, id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
