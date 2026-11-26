// SPDX-License-Identifier: MPL-2.0
// docs/TickDriverBackendUImpl-rationale.md · docs/TickDriverBackendUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulation/PhysicsEngineTraits.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/SimulationFrame.h"
#include "OGSimulationUnreal/UERenderTargetTable.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGSimSession/SessionRuntime.h"
#include "OGSimSession/SessionTypes.h"
#include "HAL/PlatformTime.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESessionEngine.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "Components/CapsuleComponent.h"
#include "Templates/Casts.h"

#include <cstdint>
#include <optional>
#include <string>

class AActor;
template <typename TraitsT> class UESimulationSession;

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
concept TickDriverBackendComposition = PhysicsEngineTraits<EngineTraitsT>
	&& PhysicsEngineBinding<EngineBindingT, EngineTraitsT>
	&& TickDriverWorldHost<typename EngineTraitsT::template WorldHost<typename EngineBindingT::Simulatable>>
	&& requires(const typename GameBindingT::Character& character, unsigned int id)
	{
		EngineBindingT::checkCapsuleContract(character, id);
	};

template <typename GameBindingT, typename EngineTraitsT, typename EngineBindingT>
	requires TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>
class TickDriverBackendUImpl
{
public:
	using Session     = typename GameBindingT::Session;
	using Runtime     = typename Session::Runtime;
	using Component   = typename GameBindingT::Component;
	using Character   = typename GameBindingT::Character;
	using StaticData  = typename GameBindingT::StaticData;
	using Simulatable = typename EngineBindingT::Simulatable;
	using BodyRequest = ogsession::SessionBodyRequest<StaticData>;
	using WorldConfig = typename EngineTraitsT::template WorldHost<Simulatable>::Config;

	TickDriverBackendUImpl(Runtime& runtime, UERenderTargetTable& renderTargets)
		: m_runtime(runtime)
		, m_renderTargets(renderTargets)
	{
	}

	TickDriverBackendUImpl(const TickDriverBackendUImpl&) = delete;
	TickDriverBackendUImpl& operator=(const TickDriverBackendUImpl&) = delete;

	static uint64 slotKeyFor(const AActor* owner) { return static_cast<uint64>(reinterpret_cast<UPTRINT>(owner)); }

	static ogsession::EntityKey entityKeyOf(const Component& owner) { return ogsession::EntityKey{ reinterpret_cast<std::uintptr_t>(&owner) }; }

	static const Component& componentOf(ogsession::EntityKey key)
	{
		checkf(key.value != 0u, TEXT("UESimulationSession: a client's lifecycle key is its simulatable component's address"));
		return *reinterpret_cast<const Component*>(key.value);
	}

	static uint64_t bodyKeyOf(ogsession::EntityKey key)
	{
		const Component* const owner = reinterpret_cast<const Component*>(key.value);
		return slotKeyFor(owner != nullptr ? owner->GetOwner() : nullptr);
	}

	static ogsession::RegisterRequest requestFor(unsigned int id, const Component& owner, bool withEntityFacts)
	{
		ogsession::RegisterRequest request;
		request.simId     = id;
		request.entityKey = entityKeyOf(owner);
		if (!withEntityFacts)
			return request;
		AActor* const pawn = owner.GetOwner();
		const UOGNetConnection* const ownerConnection = Cast<UOGNetConnection>(GetRootNetConnection(pawn));
		const FTCHARToUTF8 debugName(pawn != nullptr ? *pawn->GetName() : TEXT("none"));
		request.debugName       = std::string(debugName.Get());
		request.ownerConnection = ownerConnection != nullptr ? ownerConnection->OGNetId : kInvalidSimConnection;
		request.localIndex      = GetPlayerSlotForActor(pawn);
		return request;
	}

	static WorldConfig worldConfig(bool worldIsAuthority, const StaticData& staticData)
	{
		WorldConfig config;
		config.isAuthority      = worldIsAuthority;
		config.runInline        = false;
		// ⛔G-06  docs/TickDriverBackendUImpl-guards.md
		config.gravity          = simFrame::withHeight(glm::vec3(0.f), GameBindingT::gravity(staticData));
		config.contactCounts    = EngineBindingT::contactCountsEnabled();
		config.mappedCategories = EngineBindingT::mappedCategories();
		config.log              = &EngineTraitsT::logEngineMessage;
		return config;
	}

	BodyRequest makeRequest(const ogsession::RegisterRequest& request)
	{
		Component& owner = *reinterpret_cast<Component*>(request.entityKey.value);
		Character* const character = Cast<Character>(owner.GetOwner());
		checkf(character != nullptr,
			TEXT("UESimulationSession::tryRegister: the component must be attached to a TraitsT::Character — the ")
			TEXT("first-call body pass reads that class's own root capsule"));
		EngineBindingT::checkCapsuleContract(*character, request.simId);
		const StaticData* const staticData = &owner.getStaticData();
		return callWithSlotRequest(*owner.GetOwner(), [staticData](uint64_t key, const char* name, bool isAuthority,
			std::optional<uint32_t> replicatedSlot, auto&&)
		{
			return BodyRequest{ key, std::string(name), isAuthority, replicatedSlot, staticData };
		});
	}

	bool createBodies_GameThread(Component& owner, Character& character, Simulatable& simulatable, unsigned int id)
	{
		EngineBindingT::checkCapsuleContract(character, id);
		const StaticData* const staticData = &owner.getStaticData();
		return callWithSlotRequest(*owner.GetOwner(), [this, id, &simulatable, staticData](uint64_t key, const char* name,
			bool isAuthority, std::optional<uint32_t> replicatedSlot, auto&& publish)
		{
			return m_runtime.acquireAndBind(BodyRequest{ key, std::string(name), isAuthority, replicatedSlot, staticData },
				publish, id, simulatable);
		});
	}

	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
	{
		return callWithSlotRequest(owner, [this, &descriptor](uint64_t key, const char* name, bool isAuthority,
			std::optional<uint32_t> replicatedSlot, auto&& publish)
		{
			return m_runtime.registerVizVolume(descriptor, BodyRequest{ key, std::string(name), isAuthority, replicatedSlot },
				publish);
		});
	}

	void onRegistered_GameThread(Component& owner, unsigned int id)
	{
		m_runtime.bodiesRegistered(id, slotKeyFor(owner.GetOwner()));
		registerRenderTarget(owner, id);
	}

	void onUnregistered_GameThread(Component& owner, unsigned int id)
	{
		const uint64 key = slotKeyFor(owner.GetOwner());
		m_runtime.forgetBodies(id, key, /*releaseSlot=*/false);
		m_renderTargets.unregisterTarget(id);
		if (owner.GetOwner() != nullptr)
			m_runtime.releaseBodySlot(id, key);
	}

	void onRegisteredHost_GameThread(Component& owner, unsigned int id) { registerRenderTarget(owner, id); }
	void onUnregisteredHost_GameThread(unsigned int id) { m_renderTargets.unregisterTarget(id); }

private:
	void registerRenderTarget(Component& owner, unsigned int id)
	{
		Character* const character = Cast<Character>(owner.GetOwner());
		m_renderTargets.registerTarget(id, character, character != nullptr ? character->GetCapsuleComponent() : nullptr,
			m_runtime.runsPrediction());
	}

	template <typename SlotCallF>
	decltype(auto) callWithSlotRequest(AActor& owner, SlotCallF&& slotCall)
	{
		const uint64 key = slotKeyFor(&owner);
		return GameBindingT::callWithSlotRequest(owner, key, m_runtime.isAuthority(),
			[this, key](unsigned int storageKey) { return m_runtime.physicsSlotFor(key, storageKey); },
			[this, key](uint32_t slot) { m_runtime.recordAuthoritativeSlot(key, slot); }, slotCall);
	}

	Runtime&             m_runtime;
	UERenderTargetTable& m_renderTargets;
};

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

template <typename TraitsT>
struct UESessionRuntimeTraits
{
	using Host          = UESimulationSession<TraitsT>;
	using Game          = typename TraitsT::Game;
	using Composition   = typename TraitsT::Composition;
	using EngineTraits  = typename TraitsT::EngineTraits;
	using StaticData    = typename TraitsT::StaticData;
	using Simulatable   = typename TraitsT::Simulatable;
	using PlayerInput   = typename TraitsT::PlayerInput;
	using NetConfig     = typename TraitsT::NetConfig;
	using Engine        = SimulationEngineUImpl;
	using ClockEngine   = UESessionClockEngine;
	using Link          = UESimNetLanes<TraitsT>;
	using Server        = ogsimnet::SimNetServer<Composition, typename Link::ServerHost, typename Link::Transport>;
	using Idle          = ogsimnet::SimNetIdle<ognetunreal::SimLaneEndpoint>;
	using Diagnostics   = UELatencyBudgetDiagnostics<TraitsT>;
	using GameBinding   = typename TraitsT::template SessionHooks<UESimulationSession<TraitsT>>;
	using Bodies        = TickDriverBackendUImpl<GameBinding, EngineTraits, typename TraitsT::EngineBinding>;

	static auto worldConfig(bool isAuthority, const StaticData& staticData) { return Bodies::worldConfig(isAuthority, staticData); }
};

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

	template <typename GameT, typename StaticDataT, typename SimulatableT, typename CharacterT>
	concept GameInstanceHooks = requires(GameT& game, UWorld& world, ENetMode netMode, bool flag, StaticDataT& staticData,
		const SimulatableT& constSimulatable, SimulatableT& simulatable, CharacterT& character, unsigned int id)
	{
		game.beforeWorldBuild(world, netMode, flag, staticData);
		game.afterSessionConfigured(netMode);
		{ GameT::rootBodyId(constSimulatable) } -> std::same_as<BodyId>;
		game.afterBodiesCreated(simulatable, character, id, flag, netMode);
		game.afterRegistered(id, flag);
		game.onUnregistered(id);
	};

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
