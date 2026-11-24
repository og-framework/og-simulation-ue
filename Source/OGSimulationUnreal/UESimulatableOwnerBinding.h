// SPDX-License-Identifier: MPL-2.0
// docs/UESimulatableOwnerBinding-rationale.md · docs/UESimulatableOwnerBinding-guards.md

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

#include "UELatencyBudgetDiagnostics.h"
#include "UESimulationLogRoutes.h"
#include "UESimulationSession.h"

#include "OGSimulation/CompilerControl.h"

#include <concepts>
#include <utility>

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
class UESimulatableOwnerBinding
{
protected:
	~UESimulatableOwnerBinding() = default;

	void beginRegistration();

private:
	OwnerT&       derived()       { return static_cast<OwnerT&>(*this); }
	const OwnerT& derived() const { return static_cast<const OwnerT&>(*this); }

	static constexpr int32 kMaxRegistrationAttempts = 600;

	void tryInitializeWithManager();
	void tryRegisterWithNewFramework();
	void scheduleNextInitializationAttempt();
	void scheduleNextRegistrationAttempt();
	int32 m_initializationAttempts = 0;
	int32 m_registrationAttempts = 0;
};

OGSIM_OPTIMIZE_OFF

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
		"(Ready, or Retry to be called again next frame against the same attempt budget). "
		"Added by og-ue-simulationcomponentandmanagersplit task 29 (ruling Q-M6).");

	++m_initializationAttempts;

	// ⛔G-07  docs/UESimulatableOwnerBinding-guards.md
	const bool isAuthority = (derived().GetNetMode() != NM_Client);
	typename TraitsT::SessionOwner* manager = TraitsT::SessionOwner::instanceFor(derived().GetWorld(), isAuthority);
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
		"(or public) member of OwnerT, with the binding befriended. "
		"Added by og-ue-simulationcomponentandmanagersplit task 29 (ruling Q-M6).");
	static_assert(requires(const OwnerT& owner) { { owner.storageKeyOfOwner() } -> std::same_as<unsigned int>; },
		"UESimulatableOwnerBinding<OwnerT, TraitsT> needs the game component's key hook: a private (or public) "
		"'unsigned int storageKeyOfOwner() const' on OwnerT, with the binding befriended. tryRegisterWithNewFramework "
		"names the owner by it when the session owner never spawns. Was the key-hook check in the body of the net-sync "
		"interface's key override (review B-2, ruling Q-F), which og-input-latency task P5-8 deleted with that interface.");

	++m_registrationAttempts;

	const bool isAuthority = (derived().GetNetMode() != NM_Client);
	typename TraitsT::SessionOwner* regManager = TraitsT::SessionOwner::instanceFor(derived().GetWorld(), isAuthority);
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

	if (status == TryRegisterStatus::Refused)
	{
		UE_LOG(LogOGMgmt, Error,
			TEXT("DeferredReg: id=%u abandoned: the session refused its registration, so this character stays unsimulated"),
			resolved.key);
		return;
	}

	if (status == TryRegisterStatus::Pending)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: id=%u not ready after %d attempts"),
				resolved.key, kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	UE_LOG(LogOGMgmt, Log,
		TEXT("NewFramework: registered id=%u isAuthority=%d isLocallyPredicted=%d"),
		resolved.key, isAuthority ? 1 : 0, isLocallyPredicted ? 1 : 0);
}

OGSIM_OPTIMIZE_ON
