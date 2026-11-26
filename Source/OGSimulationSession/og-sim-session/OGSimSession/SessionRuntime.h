#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

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

	std::optional<ogsession::SessionThreadChecks> m_threads;
	UESessionClockEngine m_clockEngine;
	std::optional<Clock> m_clock;
	std::optional<Display> m_display;
	std::optional<NetHooks> m_netHooks;
	std::optional<Lifecycle> m_lifecycle;
	std::optional<AuthorityLifecycle> m_authorityLifecycle;

	SimNetLanes* m_simNetLanes = nullptr;
	std::optional<SimNetServerHost> m_simNetServerHost;
	std::optional<SimNetServer> m_simNetServer;
	std::optional<ogsimnet::SimNetIdle<ognetunreal::SimLaneEndpoint>> m_simNetIdle;
	UESimNetClockServer* m_clockServer = nullptr;

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
		static_assert(std::is_same_v<ManagerType, typename PhysicsBackend::TickGame::Manager>,
			"UESimulationSession::ManagerType must BE the manager type the backend's tick game names "
			"(PhysicsBackend::TickGame::Manager, the game binding's through UESessionTickHost). ManagerType is private, so a "
			"backend host declared outside this class (the tick-driver backend, whose step driver is "
			"SimulationStepDriver<Manager, ...>) names the manager through the game binding; a ManagerType that drifts from "
			"it hands that host a different SimulationManager instantiation than m_manager. Was the owner's ManagerType row "
			"(added by og-simulationscheduler-withjolt task 81), which compared it with the game's composition struct; moved "
			"here by og-ue-simulationcomponentandmanagersplit task 24a; compared with TraitsT::TickDriverGame until "
			"og-session-runtime task 26.");
		static_assert(TickDriverSampleGame<typename PhysicsBackend::TickGame>
		           && TickDriverReceiveGame<typename PhysicsBackend::TickGame>
		           && TickDriverDisplayGame<typename PhysicsBackend::TickGame>
		           && TickDriverIdleGame<typename PhysicsBackend::TickGame>,
			"UESimulationSession: the backend's tick game (og-sim-session's SessionTickGame over UESessionTickHost) must model "
			"the four optional hooks' concepts; SessionTickGame.h states why each is needed. Were the four static_asserts on "
			"the brawler's BrawlerTickDriverGame (og-session-runtime task 26)");
		static_assert(std::is_same_v<Storage, typename PhysicsBackend::TickGame::Storage>,
			"UESimulationSession::Storage must BE the storage type the backend's tick game hands the driver; the tick host "
			"returns m_storage as it");
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
		static_assert(__builtin_offsetof(UESimulationSession, m_latencyBudgetDiagnostics) < __builtin_offsetof(UESimulationSession, m_netHooks)
		           && __builtin_offsetof(UESimulationSession, m_storage) < __builtin_offsetof(UESimulationSession, m_netHooks)
		           && __builtin_offsetof(UESimulationSession, m_netSync) < __builtin_offsetof(UESimulationSession, m_netHooks)
		           && __builtin_offsetof(UESimulationSession, m_manager) < __builtin_offsetof(UESimulationSession, m_netHooks),
			"UESimulationSession members destruct in REVERSE declaration order. The net hooks (og-sim-session's "
			"SessionNetHooks) keep references to the latency diagnostics, the storage, the net sync and the manager, so they "
			"must be destroyed before all four. Keep m_latencyBudgetDiagnostics, m_storage, m_netSync and m_manager < "
			"m_netHooks (og-session-runtime task 26: the hooks were the session's member functions until then)");
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
