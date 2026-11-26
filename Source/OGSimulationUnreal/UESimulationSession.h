// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/TickDriverBackendUImpl.h"
#include "OGSimulationUnreal/UEAuthorityRunner.h"
#include "OGSimulationUnreal/UEConfigSource.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESessionEngine.h"
#include "OGSimulationUnreal/UESessionRefusal.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "OGSimulationUnreal/UETickDriverHost.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimSession/SessionRuntime.h"

#include <concepts>
#include <cstdio>
#include <optional>
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

template <typename TraitsT>
class UESimulationSession
{
public:
	using SessionOwner             = typename TraitsT::SessionOwner;
	using Game                     = typename TraitsT::Game;
	using LatencyBudgetDiagnostics = UELatencyBudgetDiagnostics<TraitsT>;
	using RuntimeTraits            = UESessionRuntimeTraits<TraitsT>;
	using Runtime                  = ogsession::SessionRuntime<RuntimeTraits>;
	using EngineTraits             = typename TraitsT::EngineTraits;
	using Simulatable              = typename TraitsT::Simulatable;
	using PlayerInput              = typename TraitsT::PlayerInput;
	using Component                = typename TraitsT::Component;
	using InputProvider            = typename LatencyBudgetDiagnostics::InputProvider;
	using DisplayVizState          = typename Runtime::DisplayVizState;

	UESimulationSession(SessionOwner& owner, Game& game) : m_owner(owner), m_game(game) { static_assert(adapterContractsHold()); }

	~UESimulationSession()
	{
		// ⛔G-10  docs/UESimulationSession-guards.md
		unregisterInstance();
		m_runtime.detachLink();
	}

	UESimulationSession(const UESimulationSession&) = delete;
	UESimulationSession& operator=(const UESimulationSession&) = delete;

	SessionOwner&       owner()       { return m_owner; }
	const SessionOwner& owner() const { return m_owner; }
	const Runtime&      runtime() const { return m_runtime; }

	const LatencyBudgetDiagnostics& getLatencyBudgetDiagnostics() const { return m_latencyBudgetDiagnostics; }
	LatencyBudgetDiagnostics&       editLatencyBudgetDiagnostics()       { return m_latencyBudgetDiagnostics; }

	void beginPlay();
	void endPlay();

	bool runsPrediction() const { return m_runtime.runsPrediction(); }

	auto&       editVizQuery() { return m_runtime.editVizQuery(); }
	const auto& getVizReader() const { return m_runtime.getVizReader(); }
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
	{
		return m_bodies.registerVizVolume(descriptor, owner);
	}
	bool hasGameThreadVizWorld() const { return m_runtime.getManager().has_value() && runsPrediction(); }

	template <typename SliceT>
	std::optional<SliceT> getVizSlice(unsigned int id) const
	{
		const DisplayVizState* const vizState = m_runtime.findDisplayVizState(id);
		if (vizState == nullptr)
			return std::nullopt;

		// ⛔G-02  docs/UESimulationSession-guards.md
		return vizState->getState().template get<SliceT>();
	}

	const std::unordered_map<unsigned int, TWeakObjectPtr<Component>>& getRegisteredComponentsById() const
	{
		return m_registeredComponentsById;
	}

	// ⛔G-11  docs/UESimulationSession-guards.md
	static UESimulationSession* instanceFor(const UWorld* world, bool isAuthority)
	{
		return UESessionRegistry<UESimulationSession>::instanceFor(world, isAuthority);
	}

	TryRegisterStatus tryRegister(unsigned int id, Simulatable&& simulatable, Component& owner, InputProvider&& inputProvider,
		bool isAuthority);
	void unregisterFromNewFramework(unsigned int id, Component& owner, bool isAuthority);
	ClientBindStep clientBindStep(unsigned int id, bool engineSaysLocal);

	void onConfigure() { registerInstance(); }
	void bindDiagnosticGates() { ueSessionEngine::bindDiagnosticGates(); }
	void afterPredictionConfigured() { ueSessionEngine::logResimProbeSession(); }
	void onSessionConfigured() { m_game.afterSessionConfigured(m_owner.GetNetMode()); }
	void beginFrames(typename Runtime::Driver& driver);
	void endFrames() { m_frameHost.unregisterTickFunctions(); }
	void sampleInput() { RuntimeTraits::GameBinding::sampleInput(*this); }
	void onPostPhysics();
	void logHost(const char* line) { RouteOGMessage(line); }
	auto makeBodyRequest(const ogsession::RegisterRequest& request) { return m_bodies.makeRequest(request); }
	uint64_t bodyKeyOf(ogsession::EntityKey key) const { return Bodies::bodyKeyOf(key); }
	InputProvider wrapInputProvider(const ogsession::RegisterRequest& request, InputProvider&& provider)
	{
		return m_latencyBudgetDiagnostics.wrapInputProvider(request.simId, Bodies::componentOf(request.entityKey), std::move(provider));
	}
	void teardownComponents();
	void forgetComponents();
	void onLinkStopping(bool isServer);

	template <typename FunctionT>
	void forEachRegisteredId(FunctionT&& function) const
	{
		for (const auto& entry : m_registeredComponentsById)
			function(static_cast<uint32_t>(entry.first));
	}

private:
	using Bodies = typename RuntimeTraits::Bodies;
	using Link   = typename RuntimeTraits::Link;

	struct RegisterHooks
	{
		UESimulationSession* session = nullptr;
		Component*           owner   = nullptr;
		unsigned int         id      = 0u;
		bool                 isAuthority = false;

		void bodiesRequested() { session->holdForRunner(id, *owner); }
		void bodiesCreated(Simulatable& simulatable) { session->bodiesCreatedFor(simulatable, *owner, id, isAuthority); }
		void collectOutcomes() { session->collectLifecycleOutcomes_GameThread(); }

		bool createBodies(Simulatable& simulatable)
		{
			typename TraitsT::Character* character = Cast<typename TraitsT::Character>(owner->GetOwner());
			checkf(character != nullptr,
				TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
				TEXT("first-call body pass reads that class's own root capsule"));
			return session->createBodiesFor(*owner, *character, simulatable, id);
		}
	};

	static Link* findDriverSimNetLanes(UWorld& world, bool isServer);
	void bodiesCreatedFor(Simulatable& simulatable, Component& owner, unsigned int id, bool isAuthority);
	bool refuseToStart(UWorld& world, ENetMode netMode);
	TryRegisterStatus tryRegisterOnRunner(unsigned int id, Simulatable&& simulatable, Component& owner);
	void requestRunnerUnregister_GameThread(unsigned int id, Component* owner);
	void unregisterCharacterHostOnly(unsigned int id, Component* owner, bool releaseBodiesNow);
	void collectLifecycleOutcomes_GameThread();
	void onPredictionOutcome_GameThread(const ogsession::LifecycleOutcome& outcome);
	void holdForRunner(unsigned int id, Component& owner) { m_componentsHeldForRunner[id] = TStrongObjectPtr<Component>(&owner); }
	bool createBodiesFor(Component& owner, typename TraitsT::Character& character, Simulatable& simulatable, unsigned int id)
	{
		return m_bodies.createBodies_GameThread(owner, character, simulatable, id);
	}
	void registerInstance() { m_registry.add(); }
	void unregisterInstance() { m_registry.remove(); }

	std::vector<unsigned int> registeredIds() const
	{
		std::vector<unsigned int> ids;
		ids.reserve(m_registeredComponentsById.size());
		for (const auto& entry : m_registeredComponentsById)
			ids.push_back(entry.first);
		return ids;
	}

	SessionOwner& m_owner;
	Game&         m_game;

	LatencyBudgetDiagnostics m_latencyBudgetDiagnostics{ std::as_const(*this) };
	UETickDriverHost         m_frameHost;
	UESessionPresentation    m_presentation;
	Runtime                  m_runtime{ *this, m_latencyBudgetDiagnostics, m_frameHost.engine() };
	Bodies                   m_bodies{ m_runtime, m_presentation.renderTargets() };

	UESessionNoSession m_noSession;

	std::unordered_map<unsigned int, TWeakObjectPtr<Component>> m_registeredComponentsById;
	std::unordered_set<unsigned int> m_simUnregisteredIds;

	UESessionRegistry<UESimulationSession> m_registry{ *this };

	ogsession::SessionHeldForUnregister<TStrongObjectPtr<Component>> m_pendingUnregisters;
	std::unordered_map<unsigned int, TStrongObjectPtr<Component>> m_componentsHeldForRunner;

	static constexpr bool adapterContractsHold()
	{
		static_assert(__builtin_offsetof(UESimulationSession, m_latencyBudgetDiagnostics) < __builtin_offsetof(UESimulationSession, m_runtime)
		           && __builtin_offsetof(UESimulationSession, m_frameHost) < __builtin_offsetof(UESimulationSession, m_runtime)
		           && __builtin_offsetof(UESimulationSession, m_presentation) < __builtin_offsetof(UESimulationSession, m_runtime)
		           && __builtin_offsetof(UESimulationSession, m_runtime) < __builtin_offsetof(UESimulationSession, m_bodies),
			"UESimulationSession members destruct in REVERSE declaration order. The runtime holds the latency diagnostics (its "
			"net hooks, its tick game's stamps) and the frame host's engine (its driver and presenter) by reference, and its "
			"presenter writes the render table, so all three outlive it; the bodies hold the runtime. Keep "
			"m_latencyBudgetDiagnostics, m_frameHost and m_presentation < m_runtime < m_bodies. Were the session's "
			"m_latencyBudgetDiagnostics < m_netHooks row (og-session-runtime task 26) and TickDriverBackendUImpl's m_frameHost < "
			"m_tickBackend row (og-tickdriver task 15d); og-session-runtime task 31");
		static_assert(std::is_same_v<typename Runtime::SimChannel, UESimNetChannel<Simulatable>>,
			"SimulatableOwnerTraits<TraitsT::Simulatable> names og-sim-net's one channel type as UESimNetChannel<Simulatable>: "
			"the runtime's channel (the SimNetServer's and the SimNetClient's, one type) is it. The runtime asserts the owner "
			"traits' half (og-session-runtime task 31)");
		static_assert(ueSimulationSessionHookNames::GameInstanceHooks<Game, typename TraitsT::StaticData, Simulatable,
				typename TraitsT::Character>,
			"UESimulationSession<TraitsT> calls six instance and static phase hooks of TraitsT::Game, each at the position of "
			"the game statements it replaced: beforeWorldBuild(world, netMode, compositionExists, staticData) and "
			"afterSessionConfigured(netMode) (beginPlay, and the runtime's start through onSessionConfigured), the static "
			"rootBodyId(simulatable) (the BodyId the resolvability gate tests first), afterBodiesCreated(simulatable, "
			"character, id, isAuthority, netMode) and afterRegistered(id, isAuthority) (tryRegister), and onUnregistered(id) "
			"(unregisterFromNewFramework: after the reception forget, before the latency forget), each on the session's Game& "
			"but rootBodyId. The runtime asserts the four static ones it calls. Were three requires-checks in beginPlay, "
			"tryRegister and unregisterFromNewFramework (og-ue-simulationcomponentandmanagersplit tasks 27b, 28; manager "
			"design 4.3); one concept since og-session-runtime task 31.");
		static_assert(ueSimulationSessionHookNames::SharesNoHookName<UESimulationSession>,
			"No member of UESimulationSession, public or private, own or inherited, may share a name with one of the phase "
			"hooks of TraitsT::Game it or the runtime calls: beforeWorldBuild, emplaceSystems, neutralInput, "
			"installGameLoggers, afterSessionConfigured and backendFingerprint (start), rootBodyId, afterBodiesCreated and "
			"afterRegistered (tryRegister), onUnregistered (unregisterFromNewFramework), authorityPresentationSlice_Step and "
			"presentAuthoritySlice. The session calls every hook qualified, as Game::hook or m_game.hook, so a clash changes "
			"no call; it makes a reader take the game's statement for the session's own. Was a sentence of beginPlay's hook "
			"check (task 27b) that nothing enforced; made a compile-time check, over every member, by "
			"og-ue-simulationcomponentandmanagersplit task 28.");
		return true;
	}
};

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
auto UESimulationSession<TraitsT>::findDriverSimNetLanes(UWorld& world, bool isServer) -> Link*
{
	UOGNetDriver* const driver = Cast<UOGNetDriver>(world.GetNetDriver());
	IOGNetSimLanes* const lanes = driver != nullptr ? driver->GetSimLanes() : nullptr;
	if (lanes == nullptr)
		return nullptr;
	Link* const simNetLanes = static_cast<Link*>(lanes);
	checkf(simNetLanes->isServer() == isServer,
		TEXT("UESimulationSession: the og-net driver's sim lanes were made for the other role. The game's factory makes them "
			 "for the driver's role, which is this world's"));
	return simNetLanes;
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::refuseToStart(UWorld& world, ENetMode netMode)
{
	m_registry.note(world, netMode != NM_Client);
	Link* const lanes = netMode == NM_Standalone || netMode == NM_ListenServer ? nullptr
		: findDriverSimNetLanes(world, m_registry.isAuthority());
	if (!ueSessionRefusal::refuseNetModeOrDriver(world, netMode, lanes != nullptr, m_noSession)
		&& !ueSessionRefusal::refuseRetiredKnobs(netMode, lanes != nullptr && lanes->simTrafficImmediateRefused())
		&& !ueSessionRefusal::refuseSecondSessionOfRole(world, m_registry.isAuthority(), m_registry.roleTaken(), m_noSession))
	{
		m_registry.markStarted();
		return false;
	}
	registerInstance();
	ueSessionRefusal::requestServerExit();
	return true;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginPlay()
{
	UWorld* uWorld = m_owner.GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("UESimulationSession: unexpected state"));

	if (refuseToStart(*uWorld, m_owner.GetNetMode()))
		return;

	m_game.beforeWorldBuild(*uWorld, m_owner.GetNetMode(), m_runtime.configured(), m_runtime.editStaticData());

	const float dt = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
	checkf(FMath::Abs(dt * static_cast<float>(TraitsT::NetConfig::tickFrequencyHz) - 1.f) < 1e-4f,
		TEXT("UESimulationSession: UPhysicsSettings::AsyncFixedTimeStepSize (%f s) is not one tick of ")
		TEXT("TraitsT::NetConfig::tickFrequencyHz (%d Hz). The manager's step dt and the tick rate the ")
		TEXT("per-connection templates are compiled for must agree. Was design D13 (task 18)."),
		dt, TraitsT::NetConfig::tickFrequencyHz);

	const bool worldIsAuthority = m_owner.GetNetMode() != NM_Client;
	m_latencyBudgetDiagnostics.begin(*uWorld, worldIsAuthority);

	Link* const lanes = findDriverSimNetLanes(*uWorld, worldIsAuthority);
	ogsession::SessionStart start;
	start.role                      = worldIsAuthority ? ogsession::SessionRole::Authority : ogsession::SessionRole::Prediction;
	start.stepSeconds               = static_cast<double>(dt);
	start.requestedMaxCatchUpSteps  = UETickDriverHost::requestedMaxCatchUpSteps();
	start.authorityIdleSlackSeconds = worldIsAuthority ? readAuthorityIdleSlackSeconds() : 0.0;
	start.highResolutionWait        = worldIsAuthority && lanes->endpoint().hasHighResolutionWait();
	ueSessionEngine::importStatics(*uWorld, TraitsT::EngineBinding::categoryChannels(), worldIsAuthority, start);
	m_runtime.start(std::move(start), UEConfigSource{}, lanes);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::endPlay()
{
	if (!m_runtime.stop())
	{
		unregisterInstance();
		return;
	}
	unregisterInstance();
	if (UWorld* World = m_owner.GetWorld())
		m_latencyBudgetDiagnostics.end(*World);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginFrames(typename Runtime::Driver& driver)
{
	UWorld* const world = m_owner.GetWorld();
	checkf(world != nullptr && world->PersistentLevel != nullptr, TEXT("beginFrames: the world has no persistent level."));
	m_frameHost.begin(m_owner, *world->PersistentLevel, driver);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onPostPhysics()
{
#if !UE_BUILD_SHIPPING
	if (const int32 hitchMs = consumeTestGameThreadHitchMs(); hitchMs > 0)
		sleepTestGameThreadHitch(hitchMs);
#endif
	m_presentation.present(m_runtime, TraitsT::EngineBinding::kParkSnap);
	collectLifecycleOutcomes_GameThread();
	if (!runsPrediction())
	{
		m_runtime.presentAuthority([this](unsigned int id, const typename Runtime::AuthorityPresentationSlice& slice)
		{
			Game::presentAuthoritySlice(*this, id, slice);
		});
		m_latencyBudgetDiagnostics.tick_GameThread();
		return;
	}
	m_runtime.refreshDisplayView();
	Game::afterVisualization(*this);
	m_latencyBudgetDiagnostics.tick_GameThread();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onLinkStopping(bool isServer)
{
	if (!isServer)
	{
		teardownComponents();
		checkf(m_registeredComponentsById.empty(),
			TEXT("UESimulationSession::onLinkStopping: a client's teardown unregisters every character it registered, so "
				 "nothing is left for the lanes' stop to unregister. Was the client arm of the in-frame unregistration "
				 "(og-session-runtime task 14)"));
	}
	for (const unsigned int id : registeredIds())
	{
		requestRunnerUnregister_GameThread(id, m_registeredComponentsById[id].Get());
		m_simUnregisteredIds.insert(id);
	}
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::forgetComponents()
{
	m_registeredComponentsById.clear();
	m_simUnregisteredIds.clear();
	m_pendingUnregisters.clear();
	m_componentsHeldForRunner.clear();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::teardownComponents()
{
	auto* const lifecycle = m_runtime.lifecycle();
	if (!runsPrediction() || lifecycle == nullptr)
		return;
	if (!lifecycle->teardown([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); }))
		return;

	m_pendingUnregisters.releaseAll([this](uint32_t id, TStrongObjectPtr<Component>& held)
	{
		if (Component* const component = held.Get())
			m_bodies.onUnregistered_GameThread(*component, id);
	});

	for (const unsigned int id : registeredIds())
	{
		unregisterCharacterHostOnly(id, m_registeredComponentsById[id].Get(), /*releaseBodiesNow=*/true);
		m_simUnregisteredIds.insert(id);
	}

	m_runtime.forgetQueuedRegistrations();
}

template <typename TraitsT>
ClientBindStep UESimulationSession<TraitsT>::clientBindStep(unsigned int id, bool engineSaysLocal)
{
	if (!m_runtime.getManager().has_value())
	{
		ueSessionRefusal::refuseRegistrationWithoutSession(id, m_noSession);
		return ClientBindStep::Abandon;
	}
	const std::optional<ogsimnet::BindStep> step = m_runtime.bindStep(id, engineSaysLocal);
	if (!step.has_value() || *step == ogsimnet::BindStep::RoleMismatch)
		return ClientBindStep::Abandon;
	return *step == ogsimnet::BindStep::Wait ? ClientBindStep::Wait : ClientBindStep::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::bodiesCreatedFor(Simulatable& simulatable, Component& owner, unsigned int id, bool isAuthority)
{
	typename TraitsT::Character* const character = Cast<typename TraitsT::Character>(owner.GetOwner());
	checkf(character != nullptr,
		TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
		TEXT("first-call body pass reads that class's own root capsule"));
#if DO_CHECK
	simulatable.getPhysicsComposite().forEach([&](const auto& decl)
	{
		checkf(decl.bindings.ownBodyId.value != 0, TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
		checkf(decl.bindings.parentBodyId.value != 0, TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
	});
#endif
	m_game.afterBodiesCreated(simulatable, *character, id, isAuthority, m_owner.GetNetMode());
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::collectLifecycleOutcomes_GameThread()
{
	if (auto* const lifecycle = m_runtime.lifecycle())
	{
		lifecycle->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome) { onPredictionOutcome_GameThread(outcome); });
		return;
	}
	if (auto* const authority = m_runtime.authorityLifecycle())
	{
		authority->pollOutcomes([this](const ogsession::LifecycleOutcome& outcome)
		{
			if (const auto* const bound = std::get_if<ogsession::lifecycleOutcome::BodiesBound>(&outcome))
				m_componentsHeldForRunner.erase(bound->simId);
			else if (const auto* const pending = std::get_if<ogsession::lifecycleOutcome::BodiesPending>(&outcome))
				m_componentsHeldForRunner.erase(pending->simId);
		});
	}
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterOnRunner(unsigned int id, Simulatable&& simulatable, Component& owner)
{
	collectLifecycleOutcomes_GameThread();
	auto* const authority = m_runtime.authorityLifecycle();
	if (authority == nullptr)
		return TryRegisterStatus::Pending;
	RegisterHooks hooks{ this, &owner, id, /*isAuthority=*/true };
	const ogsession::RegisterStatus status = authority->tryRegister(Bodies::requestFor(id, owner, /*withEntityFacts=*/true),
		std::move(simulatable), hooks);
	if (status == ogsession::RegisterStatus::Pending)
		return TryRegisterStatus::Pending;
	if (status == ogsession::RegisterStatus::Refused)
		return TryRegisterStatus::Refused;
	m_registeredComponentsById[id] = &owner;
	m_game.afterRegistered(id, /*isAuthority=*/true);
	m_bodies.onRegisteredHost_GameThread(owner, id);
	return TryRegisterStatus::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestRunnerUnregister_GameThread(unsigned int id, Component* owner)
{
	auto* const authority = m_runtime.authorityLifecycle();
	const bool registered = authority != nullptr
		&& authority->requestUnregister(id, owner != nullptr ? Bodies::entityKeyOf(*owner) : ogsession::EntityKey{});
	m_registeredComponentsById.erase(id);
	m_game.onUnregistered(id);
	m_latencyBudgetDiagnostics.forgetCharacter(id);
	if (registered)
		m_bodies.onUnregisteredHost_GameThread(id);
	UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegister(unsigned int id, Simulatable&& simulatable, Component& owner,
	InputProvider&& inputProvider, bool isAuthority)
{
	if (!m_runtime.getManager().has_value())
	{
		ueSessionRefusal::refuseRegistrationWithoutSession(id, m_noSession);
		return TryRegisterStatus::Refused;
	}
	if (isAuthority)
		return tryRegisterOnRunner(id, std::move(simulatable), owner);

	RegisterHooks hooks{ this, &owner, id, /*isAuthority=*/false };
	const ogsession::RegisterStatus status = m_runtime.tryRegisterPrediction(Bodies::requestFor(id, owner, /*withEntityFacts=*/false),
		std::move(simulatable), std::move(inputProvider), hooks);
	if (status == ogsession::RegisterStatus::Pending)
		return TryRegisterStatus::Pending;
	if (status == ogsession::RegisterStatus::Refused)
		return TryRegisterStatus::Refused;
	m_registeredComponentsById[id] = &owner;
	m_game.afterRegistered(id, /*isAuthority=*/false);
	m_bodies.onRegistered_GameThread(owner, id);
	return TryRegisterStatus::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterFromNewFramework(unsigned int id, Component& owner, bool isAuthority)
{
	if (!m_runtime.getManager().has_value())
		return;

	// ⛔G-30  docs/UESimulationSession-guards.md
	if (m_simUnregisteredIds.erase(id) != 0u)
		return;

	if (!isAuthority)
	{
		const ogsession::UnregisterRelease release = m_runtime.lifecycle()->requestUnregister(id, Bodies::entityKeyOf(owner));
		m_runtime.forgetPendingRegistration(id);
		if (release == ogsession::UnregisterRelease::AfterApplied)
			m_pendingUnregisters.hold(id, TStrongObjectPtr<Component>(&owner));
		unregisterCharacterHostOnly(id, &owner, release == ogsession::UnregisterRelease::Now);
		return;
	}

	requestRunnerUnregister_GameThread(id, &owner);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::unregisterCharacterHostOnly(unsigned int id, Component* owner, bool releaseBodiesNow)
{
	m_registeredComponentsById.erase(id);
	m_runtime.markUnregistered(id);
	m_game.onUnregistered(id);
	m_latencyBudgetDiagnostics.forgetCharacter(id);
	if (releaseBodiesNow && owner != nullptr)
		m_bodies.onUnregistered_GameThread(*owner, id);
	UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onPredictionOutcome_GameThread(const ogsession::LifecycleOutcome& outcome)
{
	if (const auto* const unregistered = std::get_if<ogsession::lifecycleOutcome::Unregistered>(&outcome))
	{
		m_pendingUnregisters.releaseOldest(unregistered->simId, [this](uint32_t id, TStrongObjectPtr<Component>& held)
		{
			if (Component* const component = held.Get())
				m_bodies.onUnregistered_GameThread(*component, id);
		});
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

OGSIM_OPTIMIZE_ON

#undef HasAuthority
