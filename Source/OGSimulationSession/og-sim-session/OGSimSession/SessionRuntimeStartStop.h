#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

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
	m_threads.emplace(clockThreads);

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
			m_netHooks.emplace(clockThreads, NetPort{ this }, *m_manager, m_netSync, m_storage, m_latencyBudgetDiagnostics);
			m_authorityLifecycle.emplace(clockThreads, AuthorityLifecyclePort{ this });
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
			m_netHooks.emplace(clockThreads, NetPort{ this }, *m_manager, m_netSync, m_storage, m_latencyBudgetDiagnostics);
			m_lifecycle.emplace(clockThreads, LifecyclePort{ this });
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
	if (m_authorityLifecycle.has_value())
		m_authorityLifecycle->forgetRegistered();
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
