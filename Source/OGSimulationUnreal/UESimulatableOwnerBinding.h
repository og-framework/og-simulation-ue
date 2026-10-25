// SPDX-License-Identifier: MPL-2.0
// docs/UESimulatableOwnerBinding-rationale.md · docs/UESimulatableOwnerBinding-guards.md

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

#include "InputRedundancyBundleBuilder.h"
#include "RelayedInputRing.h"
#include "SimulatableNetSyncComponent.h"
#include "SimulationInputRelay.h"
#include "SyncedSimulationStateBuffer.h"
#include "UEConnectionHandle.h"
#include "UELatencyBudgetDiagnostics.h"
#include "UESimulationLogRoutes.h"
#include "UESimulationSession.h"

#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/SimulationQueues.h"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

struct FInputRedundancyBundle;
struct FRelayedInputRing;
class USimulatableNetSyncComponent;

class ISimulatableOwnerBinding
{
	friend class USimulatableNetSyncComponent;

protected:
	~ISimulatableOwnerBinding() = default;

private:
	virtual unsigned int simulationKey() const = 0;
	virtual void onCorrectionStateAccepted() = 0;
	virtual void receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle) = 0;
	virtual void noteRelayedRingArrived(const FRelayedInputRing& ring) = 0;
	virtual std::function<void(const FRelayedInputRing&)> makeRingFlushedCallback() = 0;
};

static_assert(!std::is_destructible_v<ISimulatableOwnerBinding>
		&& !std::has_virtual_destructor_v<ISimulatableOwnerBinding>,
	"ISimulatableOwnerBinding is never destroyed through a base pointer: the sibling only calls through it, and the "
	"owning game component is destroyed as itself. Its destructor stays protected and non-virtual.");

namespace ueSimulatableOwnerRegistration
{
	enum class Initialization { Ready, Retry };

	enum class KeyStep { Ready, Retry, Abandon };

	struct ResolvedKey
	{
		KeyStep      step;
		unsigned int key;

		static constexpr ResolvedKey ready(unsigned int storageKey) { return ResolvedKey{ KeyStep::Ready, storageKey }; }
		static constexpr ResolvedKey retry() { return ResolvedKey{ KeyStep::Retry, 0u }; }
		static constexpr ResolvedKey abandon() { return ResolvedKey{ KeyStep::Abandon, 0u }; }
	};
}

template <typename TraitsT>
concept UESimulatableOwnerBindingTraits = requires
{
	typename TraitsT::Simulatable;
	typename TraitsT::PlayerInput;
	typename TraitsT::State;
	typename TraitsT::SessionOwner;
};

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
class UESimulatableOwnerBinding : public ISimulatableOwnerBinding
{
public:
	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using RelayedInputRingType        = FRelayedInputRing;

	void bindNetSync(USimulatableNetSyncComponent& sibling);

	bool isNetSyncBound() const { return m_netSync != nullptr; }

	void setOnCorrectionStateReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_netSync->setOnCorrectionStateReceivedCallback(std::move(fn));
	}

	void clearOnCorrectionStateReceivedCallback()
	{
		m_netSync->clearOnCorrectionStateReceivedCallback();
	}

	void sendLocalInputToAuthority(
		const PendingInputQueue<typename TraitsT::PlayerInput>& queue,
		uint32 currentTick,
		uint32 redundancyDepth);

	void setOnRemoteMoveReceivedCallback(
		std::function<void(uint32, const typename TraitsT::PlayerInput&)> fn)
	{
		m_onRemoteMoveReceivedCallback = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback()
	{
		m_onRemoteMoveReceivedCallback = nullptr;
	}

	void deliverDelayedRemoteInput(uint32 captureTick,
	                               const typename TraitsT::PlayerInput& input)
	{
		if (m_onRemoteMoveReceivedCallback)
			m_onRemoteMoveReceivedCallback(captureTick, input);
	}

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_netSync->getSyncedCorrectionStateBuffer(); }
	const FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() const { return static_cast<const USimulatableNetSyncComponent&>(*m_netSync).getSyncedCorrectionStateBuffer(); }

	FRelayedInputRing&       getRelayedInputRing() { return m_netSync->getRelayedInputRing(); }
	const FRelayedInputRing& getRelayedInputRing() const { return static_cast<const USimulatableNetSyncComponent&>(*m_netSync).getRelayedInputRing(); }

	relayedInputRing::StageArrivalOutcome stageRelayedInput(
		uint32 captureTick, uint8 dA, const typename TraitsT::PlayerInput& input);

	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn)
	{
		m_netSync->setOnRelayedInputReceivedCallback(std::move(fn));
	}

	void clearOnRelayedInputReceivedCallback()
	{
		m_netSync->clearOnRelayedInputReceivedCallback();
	}

	void attachInputRelayHost(ASimulationInputRelay* host) { m_netSync->attachInputRelayHost(host); }

protected:
	~UESimulatableOwnerBinding() = default;

	void beginRegistration();

	USimulatableNetSyncComponent& netSync();
	void unbindNetSync();

private:
	OwnerT&       derived()       { return static_cast<OwnerT&>(*this); }
	const OwnerT& derived() const { return static_cast<const OwnerT&>(*this); }

	unsigned int simulationKey() const final;
	void onCorrectionStateAccepted() final;
	void receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle) final;
	void noteRelayedRingArrived(const FRelayedInputRing& ring) final;
	std::function<void(const FRelayedInputRing&)> makeRingFlushedCallback() final;

	static constexpr int32 kMaxRegistrationAttempts = 600;

	void tryInitializeWithManager();
	void tryRegisterWithNewFramework();
	void scheduleNextInitializationAttempt();
	void scheduleNextRegistrationAttempt();
	int32 m_initializationAttempts = 0;
	int32 m_registrationAttempts = 0;

	USimulatableNetSyncComponent* m_netSync = nullptr;

	std::function<void(uint32, const typename TraitsT::PlayerInput&)> m_onRemoteMoveReceivedCallback;
};

OGSIM_OPTIMIZE_OFF

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::bindNetSync(USimulatableNetSyncComponent& sibling)
{
	m_netSync = &sibling;
	sibling.attachBinding(*this);
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
USimulatableNetSyncComponent& UESimulatableOwnerBinding<OwnerT, TraitsT>::netSync()
{
	checkf(m_netSync != nullptr,
		TEXT("UESimulatableOwnerBinding::netSync: this component is not bound to a USimulatableNetSyncComponent. The ")
		TEXT("owning actor binds the two in its PostInitializeComponents (bindNetSync), and the component unbinds in its ")
		TEXT("EndPlay, so a read before the bind or after the unbind is a lifecycle error."));
	return *m_netSync;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::unbindNetSync()
{
	if (m_netSync != nullptr)
		m_netSync->detachBinding();
	m_netSync = nullptr;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
unsigned int UESimulatableOwnerBinding<OwnerT, TraitsT>::simulationKey() const
{
	static_assert(requires(const OwnerT& owner) { { owner.storageKeyOfOwner() } -> std::same_as<unsigned int>; },
		"UESimulatableOwnerBinding<OwnerT, TraitsT> needs the game component's key hook: a private (or public) "
		"'unsigned int storageKeyOfOwner() const' on OwnerT, with the binding befriended. The binding's final "
		"simulationKey() override returns it. The hook has its own name because a game member named simulationKey() "
		"would override the interface member instead (review B-2, ruling Q-F).");
	return derived().storageKeyOfOwner();
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::onCorrectionStateAccepted()
{
	typename TraitsT::State peeked;
	const uint32 tick = m_netSync->getSyncedCorrectionStateBuffer().readInto(peeked);
	OG_LOG_ROUTED(LogOGNet,
		TEXT("[ReceiveCorrectionState] id=%u tick=%u"),
		derived().storageKeyOfOwner(), tick);
	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, tick](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			diagnostics.stamp(latencyBudget::Stream::State, derived().storageKeyOfOwner(),
				tick, latencyBudget::Point::ClientReceived);
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
relayedInputRing::StageArrivalOutcome UESimulatableOwnerBinding<OwnerT, TraitsT>::stageRelayedInput(
	uint32 captureTick, uint8 dA, const typename TraitsT::PlayerInput& input)
{
	static_assert(std::is_same_v<decltype(&UESimulatableOwnerBinding::stageRelayedInput),
			relayedInputRing::StageArrivalOutcome (UESimulatableOwnerBinding::*)(
				uint32, uint8, const typename TraitsT::PlayerInput&)>,
		"UESimulatableOwnerBinding::stageRelayedInput takes NO depth parameter. The stage's capacity is "
		"relayedInputRing::kMaxDepth, fixed inside the codec; a depth passed down here would cap every "
		"round at one entry and silently restore the replace-latest behaviour the staging removed. "
		"Was the 'NO DEPTH PARAMETER, and that is the fence' prose fence of the owning game "
		"component's header and its .cpp one-liner, which moved into this binding together "
		"with the stage.");

	ASimulationInputRelay* host = m_netSync->getInputRelayHost();
	FRelayedInputRing& stage = m_netSync->relayStagingTarget();

	const relayedInputRing::StageArrivalOutcome outcome =
		stage.stageArrival<typename TraitsT::PlayerInput>(captureTick, dA, input);

	// ⛔G-01  docs/UESimulatableOwnerBinding-guards.md
	if (outcome.droppedOldest && host != nullptr)
		host->noteStageOverflowDrop();

	return outcome;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
std::function<void(const FRelayedInputRing&)> UESimulatableOwnerBinding<OwnerT, TraitsT>::makeRingFlushedCallback()
{
	TWeakObjectPtr<OwnerT> weakSelf(&derived());
	return
			[weakSelf](const FRelayedInputRing& ring)
			{
				UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/true,
					[&weakSelf, &ring](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
					{
						OwnerT* self = weakSelf.Get();
						if (self == nullptr)
							return;
						const unsigned int id = self->storageKeyOfOwner();
						ring.forEachEntry<typename TraitsT::PlayerInput>(
							[&diagnostics, id](uint32 captureTick, uint8, const typename TraitsT::PlayerInput&)
							{
								if (captureTick != kNoInputCaptureTick)
								{
									diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
										latencyBudget::Point::Handed);
								}
							});
					});
			};
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::noteRelayedRingArrived(const FRelayedInputRing& ring)
{
	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, &ring](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			const unsigned int id = derived().storageKeyOfOwner();
			ring.forEachEntry<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, uint8, const typename TraitsT::PlayerInput&)
				{
					if (captureTick != kNoInputCaptureTick)
					{
						diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
							latencyBudget::Point::ClientReceived);
					}
				});
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::sendLocalInputToAuthority(
	const PendingInputQueue<typename TraitsT::PlayerInput>& queue,
	uint32 currentTick,
	uint32 redundancyDepth)
{
	FInputRedundancyBundle bundle;
	buildRedundancyBundle<typename TraitsT::PlayerInput>(
		queue, currentTick, static_cast<uint8>(redundancyDepth), bundle);
	m_netSync->sendInputBundleToAuthority(bundle);

	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, &bundle](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			const unsigned int id = derived().storageKeyOfOwner();
			bundle.forEachSlot<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, const typename TraitsT::PlayerInput&)
				{
					diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
						latencyBudget::Point::Handed);
				});
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle)
{
	// ⛔G-02  docs/UESimulatableOwnerBinding-guards.md
	typename TraitsT::SessionOwner* authorityManager =
		TraitsT::SessionOwner::instanceFor(/*isAuthority=*/true);
	auto* coordinator =
		(authorityManager != nullptr) ? authorityManager->session().getReceptionCoordinator() : nullptr;

	// ⛔G-03  docs/UESimulatableOwnerBinding-guards.md
	UNetConnection* rootConn =
		(coordinator != nullptr) ? GetRootNetConnection(derived().GetOwner()) : nullptr;

	const unsigned int id = derived().storageKeyOfOwner();

	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/true,
		[&bundle, id, rootConn](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			bundle.forEachSlot<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, const typename TraitsT::PlayerInput&)
				{
					diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
						latencyBudget::Point::ServerReceived);
				});
			if (rootConn != nullptr)
				diagnostics.noteWireSample(readRoundTripMs(rootConn) / 1000.0);
		});

	// ⛔G-04  docs/UESimulatableOwnerBinding-guards.md
	if (coordinator == nullptr || rootConn == nullptr)
	{
		bundle.forEachSlot<typename TraitsT::PlayerInput>(
			[this, id](uint32 captureTick, const typename TraitsT::PlayerInput& input)
			{
				OG_LOG_ROUTED(LogOGNet, TEXT("[ServerReceive] id=%u tick=%u"), id, captureTick);
				if (m_onRemoteMoveReceivedCallback)
					m_onRemoteMoveReceivedCallback(captureTick, input);
			});
		return;
	}

	const FUEConnectionHandle handle(rootConn);

	// ⛔G-05  docs/UESimulatableOwnerBinding-guards.md
	coordinator->noteRttSample(
		handle, id, authorityManager->session().getServerReceptionTick(),
		readRoundTripMs(rootConn), *m_netSync);

	const uint8 playerSlot = GetPlayerSlotForActor(derived().GetOwner());

	// ⛔G-06  docs/UESimulatableOwnerBinding-guards.md
	coordinator->template receiveInputBundle<typename TraitsT::Simulatable>(
		id, handle, playerSlot, bundle, authorityManager->session(), authorityManager->session());
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::beginRegistration()
{
	if (UWorld* world = derived().GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(&derived(), [this]() { tryInitializeWithManager(); }));
	}
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::tryInitializeWithManager()
{
	static_assert(requires(OwnerT& owner, typename TraitsT::SessionOwner& manager, AActor& ownerActor)
		{
			{ owner.initializeWithManager(manager, ownerActor) } -> std::same_as<ueSimulatableOwnerRegistration::Initialization>;
		},
		"UESimulatableOwnerBinding<OwnerT, TraitsT>::tryInitializeWithManager calls the game component's registration hook "
		"initializeWithManager(manager, owner) once the session owner exists and the component has an owner: a private (or "
		"public) member of OwnerT, with the binding befriended, that returns ueSimulatableOwnerRegistration::Initialization "
		"(Ready, or Retry to be called again next frame against the same attempt budget). Its name is its own: a game member "
		"named like an ISimulatableOwnerBinding member would override that member, which the final overrides refuse "
		"(review B-2). Added by og-ue-simulationcomponentandmanagersplit task 29 (ruling Q-M6).");

	++m_initializationAttempts;

	// ⛔G-07  docs/UESimulatableOwnerBinding-guards.md
	const bool isAuthority = (derived().GetNetMode() != NM_Client);
	typename TraitsT::SessionOwner* manager = TraitsT::SessionOwner::instanceFor(isAuthority);
	AActor* Owner = derived().GetOwner();
	if (manager == nullptr)
	{
		if (m_initializationAttempts >= kMaxRegistrationAttempts)
			checkf(false, TEXT("UESimulatableOwnerBinding: manager never spawned after %d attempts (netmode=%d)"),
				kMaxRegistrationAttempts, (int)derived().GetNetMode());
		scheduleNextInitializationAttempt();
		return;
	}

	if (Owner == nullptr)
		return;

	// ⛔G-08  docs/UESimulatableOwnerBinding-guards.md

	if (derived().initializeWithManager(*manager, *Owner) == ueSimulatableOwnerRegistration::Initialization::Retry)
	{
		if (m_initializationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("UESimulatableOwnerBinding: '%s' was not initialized after %d attempts (netmode=%d): its ")
				TEXT("initializeWithManager hook answered Retry each time (a hook that waits for a physics slot: the ")
				TEXT("manager logs a [SimHost.Slot] wait line naming what it waits for)."),
				*Owner->GetName(), kMaxRegistrationAttempts, (int)derived().GetNetMode());
		scheduleNextInitializationAttempt();
		return;
	}

	if (UWorld* world = derived().GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(&derived(), [this]() { tryRegisterWithNewFramework(); }));
	}
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::scheduleNextInitializationAttempt()
{
	if (UWorld* world = derived().GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(&derived(), [this]() { tryInitializeWithManager(); }));
	}
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::scheduleNextRegistrationAttempt()
{
	if (UWorld* world = derived().GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(&derived(), [this]() { tryRegisterWithNewFramework(); }));
	}
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::tryRegisterWithNewFramework()
{
	static_assert(requires(OwnerT& owner, typename TraitsT::SessionOwner& manager, bool isAuthority, unsigned int key)
		{
			{ owner.resolveRegistrationKey(manager, isAuthority) } -> std::same_as<ueSimulatableOwnerRegistration::ResolvedKey>;
			{ owner.makeInputProvider(key) } -> std::same_as<typename UELatencyBudgetDiagnostics<TraitsT>::InputProvider>;
			{ owner.makeSimulatable() } -> std::same_as<typename TraitsT::Simulatable>;
		},
		"UESimulatableOwnerBinding<OwnerT, TraitsT>::tryRegisterWithNewFramework calls three registration hooks of the game "
		"component, each at the position of the game statements it replaced: resolveRegistrationKey(manager, isAuthority) "
		"returning ueSimulatableOwnerRegistration::ResolvedKey (ready(storage key), retry() to be called again next frame, "
		"or abandon() to stop for good), makeInputProvider(key) returning UELatencyBudgetDiagnostics<TraitsT>::InputProvider "
		"(called for a locally predicted owner only) and makeSimulatable() returning TraitsT::Simulatable. Each is a private "
		"(or public) member of OwnerT, with the binding befriended, named unlike every ISimulatableOwnerBinding member. "
		"Added by og-ue-simulationcomponentandmanagersplit task 29 (ruling Q-M6).");

	++m_registrationAttempts;

	const bool isAuthority = (derived().GetNetMode() != NM_Client);
	typename TraitsT::SessionOwner* regManager = TraitsT::SessionOwner::instanceFor(isAuthority);
	if (regManager == nullptr)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false, TEXT("DeferredReg: manager never spawned after %d attempts (id=%u)"),
				kMaxRegistrationAttempts, derived().storageKeyOfOwner());
		scheduleNextRegistrationAttempt();
		return;
	}

	const ueSimulatableOwnerRegistration::ResolvedKey resolved = derived().resolveRegistrationKey(*regManager, isAuthority);
	if (resolved.step == ueSimulatableOwnerRegistration::KeyStep::Abandon)
		return;
	if (resolved.step == ueSimulatableOwnerRegistration::KeyStep::Retry)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: the owner's registration key was not resolved after %d attempts (its ")
				TEXT("resolveRegistrationKey hook answered Retry each time)"),
				kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	const AActor* ownerActor = derived().GetOwner();
	// ⛔G-09  docs/UESimulatableOwnerBinding-guards.md
	const bool isLocallyPredicted = !isAuthority
		&& ownerActor != nullptr
		&& ownerActor->GetLocalRole() == ROLE_AutonomousProxy;

	typename UELatencyBudgetDiagnostics<TraitsT>::InputProvider inputProvider;
	if (isLocallyPredicted)
	{
		inputProvider = derived().makeInputProvider(resolved.key);
	}

	typename TraitsT::Simulatable newSimulatable = derived().makeSimulatable();

	const TryRegisterStatus status = regManager->session().tryRegister(
		resolved.key,
		std::move(newSimulatable),
		derived(),
		std::move(inputProvider),
		isAuthority);

	if (status == TryRegisterStatus::Pending)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: id=%u not ready after %d attempts"),
				resolved.key, kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	netSync().setHasLocalInputProvider(isLocallyPredicted);

	// ⛔G-10  docs/UESimulatableOwnerBinding-guards.md
	if (isAuthority)
	{
		if (UWorld* world = derived().GetWorld())
		{
			if (AActor* ownerActor2 = derived().GetOwner())
			{
				attachInputRelayHost(
					ASimulationInputRelay::spawnForCharacter(*world, *ownerActor2));
			}
		}
	}
	else
	{
		attachInputRelayHost(
			ASimulationInputRelay::findForOwner(derived().GetWorld(), derived().GetOwner()));
	}

	// ⛔G-11  docs/UESimulatableOwnerBinding-guards.md
	if (isAuthority)
		regManager->session().noteDelayedInputComponent(resolved.key, derived());

	UE_LOG(LogOGMgmt, Log,
		TEXT("NewFramework: registered id=%u isAuthority=%d isLocallyPredicted=%d"),
		resolved.key, isAuthority ? 1 : 0, isLocallyPredicted ? 1 : 0);
}

OGSIM_OPTIMIZE_ON
