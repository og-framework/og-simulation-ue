// SPDX-License-Identifier: MPL-2.0
// docs/TickDriverBackendUImpl-rationale.md · docs/TickDriverBackendUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/TickDriverConcepts.h"
#include "OGSimulation/SimulationTickBackend.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulation/SimulationFrame.h"
#include "OGSimulationUnreal/UEAuthorityRunner.h"
#include "OGSimulationUnreal/UEPresentControls.h"
#include "OGSimulationUnreal/UERenderTargetTable.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#include "OGSimulationUnreal/UETickDriverHost.h"
#include "Components/CapsuleComponent.h"
#include "Templates/Casts.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

class AActor;
class UWorld;

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
concept TickDriverBackendComposition = PhysicsEngineTraits<EngineTraitsT>
	&& PhysicsEngineBinding<EngineBindingT, EngineTraitsT>
	&& TickDriverWorldHost<typename EngineTraitsT::template WorldHost<typename EngineBindingT::Simulatable>>
	&& requires(const typename GameBindingT::Character& character, unsigned int id)
	{
		EngineBindingT::checkCapsuleContract(character, id);
	};

template <typename WorldHostT>
struct TickDriverBackendWorldsUImpl
{
	SimulationEngineUImpl engine;
	WorldHostT            host{ engine };
};

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
class TickDriverBackendUImpl
{
public:
	using WorldHost   = typename EngineTraitsT::template WorldHost<typename EngineBindingT::Simulatable>;
	using Worlds      = TickDriverBackendWorldsUImpl<WorldHost>;
	using Simulatable = typename EngineBindingT::Simulatable;
	using Session     = typename GameBindingT::Session;
	using Component   = typename GameBindingT::Component;
	using Character   = typename GameBindingT::Character;
	using VizState    = std::remove_cvref_t<decltype(std::declval<const Simulatable&>().getAllState())>;

	TickDriverBackendUImpl(Session& owner, Worlds& worlds)
		: m_owner(owner)
		, m_worlds(worlds)
		, m_tickDriverGame(owner)
		, m_tickBackend(m_tickDriverGame, worlds.host, m_frameHost.engine())
	{
	}

	TickDriverBackendUImpl(const TickDriverBackendUImpl&) = delete;
	TickDriverBackendUImpl& operator=(const TickDriverBackendUImpl&) = delete;

	void buildWorld(UWorld& world, float dt, bool worldIsAuthority, bool freeRunning = false);
	void startStepping(UWorld& world, float dt, bool worldIsAuthority);
	template <typename AfterBuildF>
	void runAuthorityLoop_GameThread(UWorld& world, float dt, bool highResolutionWait, double idleSlackSeconds, AfterBuildF&& afterBuild);
	void stopAuthorityLoop_GameThread();
	void onEndPlayBegin();
	void beforePresent_GameThread();
	void overlayRenderPoses_GameThread(unsigned int id, VizState& vizState);

	typename EngineTraitsT::VizQuery&        editVizQuery();
	const typename EngineTraitsT::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

	bool createBodies_GameThread(Component& owner, Character& character, Simulatable& simulatable, unsigned int id);
	void onRegistered_GameThread(Component& owner, unsigned int id);
	void onUnregistered_GameThread(Component& owner, unsigned int id);

	struct BodyRequest
	{
		uint64                  key = 0u;
		std::string             name;
		bool                    isAuthority = false;
		std::optional<uint32_t> replicatedSlot;
	};
	BodyRequest makeBodyRequest_GameThread(Component& owner, Character& character, unsigned int id);
	std::optional<uint32_t> createBodies_Runner(const BodyRequest& request, Simulatable& simulatable,
		const typename GameBindingT::StaticData& staticData, unsigned int id);
	void onRegistered_Runner(uint64 key, unsigned int id);
	void onRegisteredHost_GameThread(Component& owner, unsigned int id);
	void releaseBodies_Runner(uint64 key, unsigned int id);
	void onUnregisteredHost_GameThread(unsigned int id);
	static uint64 slotKeyFor(const AActor* owner) { return slotKeyOf(owner); }

private:
	static uint64 slotKeyOf(const AActor* owner);

	template <typename SlotCallF>
	decltype(auto) callWithSlotRequest(AActor& owner, SlotCallF&& slotCall);

	void startDriverStepping(UWorld& world, float dt, bool worldIsAuthority);

	Session& m_owner;
	Worlds&  m_worlds;

	using TickBackend = SimulationTickBackend<GameBindingT, WorldHost, Simulatable, SimulationEngineUImpl>;

	UETickDriverHost    m_frameHost;
	GameBindingT        m_tickDriverGame;
	UERenderTargetTable m_renderTargets;
	TickBackend         m_tickBackend;

	typename WorldHost::Config                     m_runnerWorldConfig{};
	std::optional<typename EngineTraitsT::Statics> m_runnerStatics;
	std::atomic<typename TickBackend::Driver*>     m_runnerDriver{ nullptr };
	TUniquePtr<FUEAuthorityRunner>                 m_runner;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(Worlds, engine) < __builtin_offsetof(Worlds, host),
			"TickDriverBackendWorldsUImpl members construct in declaration order and destruct in REVERSE. The world host "
			"holds its engine (the clock, the frame number and the LogOGSimHost sink of its lines) by reference from its "
			"constructor on, so the engine must be built first and go last. Keep engine < host. The worlds, the runtime "
			"lease and the contact counter are the host's own members (og-simulation-jolt's two destruction-order rows), "
			"and were this struct's members and two static_asserts until og-tickdriver task 10. Was this struct's own "
			"file-scope static_assert until og-tickdriver task 15b made the struct a template over the world host.");
		static_assert(__builtin_offsetof(TickDriverBackendUImpl, m_frameHost) < __builtin_offsetof(TickDriverBackendUImpl, m_tickBackend)
		           && __builtin_offsetof(TickDriverBackendUImpl, m_tickDriverGame) < __builtin_offsetof(TickDriverBackendUImpl, m_tickBackend),
			"TickDriverBackendUImpl members destruct in REVERSE declaration order. The tick backend's driver and presenter "
			"hold the frame host's engine and the game binding, and the driver's step driver unregisters its resync "
			"callback from the manager's client clock when it goes, so the tick backend must go first. Keep m_frameHost "
			"and m_tickDriverGame < m_tickBackend; that its driver goes before its presenter and the presenter before the "
			"render source is og-simulation's SimulationTickBackend's own row; that all of them go while the manager is "
			"alive is the session's own row (UESimulationSession's m_manager < m_physicsBackend, GameBindingT::Session since "
			"og-ue-simulationcomponentandmanagersplit task 24a); and that the world host "
			"(m_physicsBackendWorlds) outlives the driver is asserted in startDriverStepping. Was the rows 'm_frameHost, "
			"m_tickDriverGame and m_presenter < m_tickDriver' and 'm_frameHost and m_renderSource < m_presenter' until "
			"og-tickdriver task 15d moved the presenter, the driver and the render source into the tick backend.");
		static_assert(__builtin_offsetof(TickDriverBackendUImpl, m_tickBackend) < __builtin_offsetof(TickDriverBackendUImpl, m_runner),
			"TickDriverBackendUImpl members destruct in REVERSE declaration order. The authority runner's thread runs the tick "
			"backend's loop, so the runner (whose destructor checks that its thread was joined) goes before the tick backend. Keep "
			"m_tickBackend < m_runner. Added by og-input-latency task P5-19");
		return true;
	}

	void assertCompositionContracts()
	{
		static_assert(compositionContractsHold());
	}
};

#define HasAuthority HasAuthority_is_not_the_peer_role_in_TickDriverBackendUImpl_use_worldIsAuthority_or_runsPrediction

OGSIM_OPTIMIZE_OFF

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::buildWorld(UWorld& world, float, bool worldIsAuthority,
	bool freeRunning)
{
	const UEStaticImportResult import = UEStaticGeometryImporter(EngineBindingT::categoryChannels()).importWorld(world);
	UEStaticGeometryImporter::logReport(import.report, worldIsAuthority ? TEXT("authority") : TEXT("client"));
	const typename EngineTraitsT::Statics statics = EngineTraitsT::splitStatics(import.description, import.physicsOnly);

	typename WorldHost::Config config;
	config.isAuthority      = worldIsAuthority;
	config.runInline        = UETickDriverHost::runInline();
	// ⛔G-06  docs/TickDriverBackendUImpl-guards.md
	config.gravity          = simFrame::withHeight(glm::vec3(0.f), GameBindingT::gravity(m_owner));
	config.contactCounts    = EngineBindingT::contactCountsEnabled();
	config.mappedCategories = EngineBindingT::mappedCategories();
	config.log              = &EngineTraitsT::logEngineMessage;
	if (freeRunning)
	{
		static_assert(requires { config.freeRunning = true; },
			"TickDriverBackendUImpl::buildWorld: the authority loop needs a world host that builds FreeRunning on the runner "
			"(JoltTickWorldHostConfig::freeRunning). Was P11 design section 9's UE half");
		checkf(worldIsAuthority && !config.runInline,
			TEXT("TickDriverBackendUImpl::buildWorld: only a dedicated server's authority runs the authority loop, and not inline"));
		config.freeRunning = true;
		m_runnerWorldConfig = config;
		m_runnerStatics.emplace(statics);
		return;
	}
	m_tickBackend.build(config, statics.queryable, statics.physicsOnly, m_owner.m_physAdapter, m_owner.m_physReaderAdapter,
		m_owner.m_queryAdapter);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
template <typename AfterBuildF>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::runAuthorityLoop_GameThread(UWorld& world, float dt,
	bool highResolutionWait, double idleSlackSeconds, AfterBuildF&& afterBuild)
{
	checkf(IsInGameThread() && m_runnerStatics.has_value() && !m_runner.IsValid(),
		TEXT("TickDriverBackendUImpl::runAuthorityLoop_GameThread: buildWorld imported the statics for the runner first, once"));
	checkf(world.PersistentLevel != nullptr, TEXT("runAuthorityLoop_GameThread: the world has no persistent level."));
	const AuthorityLoopConfig loopConfig{ static_cast<double>(dt), UETickDriverHost::requestedMaxCatchUpSteps(), idleSlackSeconds };
	m_runner = MakeUnique<FUEAuthorityRunner>(
		[this, loopConfig, afterBuild = std::forward<AfterBuildF>(afterBuild)](FUEAuthorityRunner& runner) mutable
		{
			typename EngineTraitsT::Statics statics = std::move(*m_runnerStatics);
			m_runnerStatics.reset();
			m_tickBackend.runAuthorityLoop(m_runnerWorldConfig, std::move(statics.queryable), std::move(statics.physicsOnly),
				loopConfig, runner.host(), runner.stopRequested(),
				[&afterBuild]() { afterBuild(); },
				[this, &runner](typename TickBackend::Driver& driver)
				{
					m_runnerDriver.store(&driver, std::memory_order_release);
					runner.releaseStart();
				},
				m_owner.m_physAdapter, m_owner.m_physReaderAdapter, m_owner.m_queryAdapter);
		},
		highResolutionWait);
	m_runner->start_GameThread();
	m_runner->waitForStart_GameThread();
	typename TickBackend::Driver* const driver = m_runnerDriver.load(std::memory_order_acquire);
	checkf(driver != nullptr,
		TEXT("runAuthorityLoop_GameThread: the runner released its start latch without starting the driver"));
	m_frameHost.begin(m_owner.owner(), *world.PersistentLevel, *driver);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::stopAuthorityLoop_GameThread()
{
	if (m_runner.IsValid())
		m_runner->join_GameThread();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::startStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	startDriverStepping(world, dt, worldIsAuthority);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onEndPlayBegin()
{
	m_frameHost.unregisterTickFunctions();
	stopAuthorityLoop_GameThread();
	// ⛔G-01  docs/TickDriverBackendUImpl-guards.md
	m_tickBackend.stop();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::beforePresent_GameThread()
{
	m_tickBackend.present(UEPresentControls::fromConsoleVariables(EngineBindingT::kParkSnap), m_renderTargets);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::overlayRenderPoses_GameThread(unsigned int id, VizState& vizState)
{
	m_tickBackend.overlayRenderPoses(id, vizState);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
typename EngineTraitsT::VizQuery& TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::editVizQuery()
{
	return m_tickBackend.editVizQuery();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
const typename EngineTraitsT::VizReader& TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::getVizReader() const
{
	return m_tickBackend.getVizReader();
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
uint64 TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::slotKeyOf(const AActor* owner)
{
	return static_cast<uint64>(reinterpret_cast<UPTRINT>(owner));
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
template <typename SlotCallF>
decltype(auto) TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::callWithSlotRequest(AActor& owner, SlotCallF&& slotCall)
{
	return GameBindingT::callWithSlotRequest(m_owner, owner, slotKeyOf(&owner), slotCall);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
std::optional<QueryVolumeId> TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
{
	return callWithSlotRequest(owner, [this, &descriptor](uint64_t key, const char* name, bool isAuthority,
		std::optional<uint32_t> replicatedSlot, auto&& publish)
	{
		return m_tickBackend.registerVizVolume(descriptor, key, name, isAuthority, replicatedSlot, publish);
	});
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
bool TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::createBodies_GameThread(Component& owner, Character& character,
	Simulatable& simulatable, unsigned int id)
{
	EngineBindingT::checkCapsuleContract(character, id);

	AActor* ownerActor = owner.GetOwner();
	const typename GameBindingT::StaticData& staticData = owner.getStaticData();
	return callWithSlotRequest(*ownerActor, [this, id, &simulatable, &staticData](uint64_t key, const char* name,
		bool isAuthority, std::optional<uint32_t> replicatedSlot, auto&& publish)
	{
		return m_tickBackend.acquireAndBind(key, name, isAuthority, replicatedSlot, publish, id, simulatable, staticData);
	});
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onRegistered_GameThread(Component& owner, unsigned int id)
{
	m_tickBackend.onRegistered(id, slotKeyOf(owner.GetOwner()));
	Character* const character = Cast<Character>(owner.GetOwner());
	m_renderTargets.registerTarget(id, character, character != nullptr ? character->GetCapsuleComponent() : nullptr,
		m_owner.runsPrediction());
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onUnregistered_GameThread(Component& owner, unsigned int id)
{
	const uint64 key = slotKeyOf(owner.GetOwner());
	m_tickBackend.forgetPresenterTarget(id, key);
	m_renderTargets.unregisterTarget(id);
	if (owner.GetOwner() != nullptr)
		m_tickBackend.releaseSlot(key, id);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
auto TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::makeBodyRequest_GameThread(Component& owner,
	Character& character, unsigned int id) -> BodyRequest
{
	EngineBindingT::checkCapsuleContract(character, id);
	return callWithSlotRequest(*owner.GetOwner(), [](uint64_t key, const char* name, bool isAuthority,
		std::optional<uint32_t> replicatedSlot, auto&&)
	{
		return BodyRequest{ key, std::string(name), isAuthority, replicatedSlot };
	});
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
std::optional<uint32_t> TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::createBodies_Runner(
	const BodyRequest& request, Simulatable& simulatable, const typename GameBindingT::StaticData& staticData, unsigned int id)
{
	if (!m_tickBackend.acquireAndBind(request.key, request.name.c_str(), request.isAuthority, request.replicatedSlot,
			[](uint32_t) {}, id, simulatable, staticData))
	{
		return std::nullopt;
	}
	return m_worlds.host.slotOf(request.key);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onRegistered_Runner(uint64 key, unsigned int id)
{
	m_tickBackend.onRegistered(id, key);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onRegisteredHost_GameThread(Component& owner, unsigned int id)
{
	Character* const character = Cast<Character>(owner.GetOwner());
	m_renderTargets.registerTarget(id, character, character != nullptr ? character->GetCapsuleComponent() : nullptr,
		m_owner.runsPrediction());
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::releaseBodies_Runner(uint64 key, unsigned int id)
{
	m_tickBackend.forgetPresenterTarget(id, key);
	m_tickBackend.releaseSlot(key, id);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::onUnregisteredHost_GameThread(unsigned int id)
{
	m_renderTargets.unregisterTarget(id);
}

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
void TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>::startDriverStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	static_assert(__builtin_offsetof(Session, m_physicsBackendWorlds)
	                  < __builtin_offsetof(Session, m_physicsBackend),
		"The session's members (GameBindingT::Session, a UESimulationSession) destruct in REVERSE declaration order. The tick driver built here "
		"(in m_physicsBackend) holds the world host (in m_physicsBackendWorlds) by pointer and locks it and steps its world until it "
		"stops, so the world host must outlive the driver. Keep m_physicsBackendWorlds < m_physicsBackend. Was implied by the owner's "
		"two rows until og-tickdriver task 10, when the driver's world host moved out of this class.");

	checkf(world.PersistentLevel != nullptr, TEXT("startDriverStepping: the world has no persistent level."));
	checkf(worldIsAuthority || m_owner.m_simNetLanes != nullptr,
		TEXT("startDriverStepping: a client's tick driver is built after the session bound its og-sim-net lanes, which decide ")
		TEXT("whether it is an immediate client; an immediate client's end of physics waits for the step its frame dispatched ")
		TEXT("(FrameWait::ThisFramesSingleStep). Was og-input-latency task P5-35 (H7 one frame late)"));

	typename TickBackend::Driver& tickDriver = m_tickBackend.start(static_cast<double>(dt), worldIsAuthority,
		UETickDriverHost::requestedMaxCatchUpSteps(), /*idleSlackSeconds=*/0.0,
		m_owner.m_immediateClient ? FrameWait::ThisFramesSingleStep : FrameWait::EarlierFrames);

	m_frameHost.begin(m_owner.owner(), *world.PersistentLevel, tickDriver);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
