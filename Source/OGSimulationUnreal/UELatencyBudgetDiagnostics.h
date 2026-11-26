// SPDX-License-Identifier: MPL-2.0
// docs/UELatencyBudgetDiagnostics-rationale.md · docs/UELatencyBudgetDiagnostics-guards.md

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "OGSimSession/SessionLatencyDiagnostics.h"
#include "OGSimSession/SessionThreads.h"

#ifndef OG_LATENCY_BUDGET_DIAGNOSTICS
#define OG_LATENCY_BUDGET_DIAGNOSTICS (!UE_BUILD_SHIPPING)
#endif

#if OG_LATENCY_BUDGET_DIAGNOSTICS
#include "Engine/World.h"
#endif

class UWorld;

template <typename TraitsT> class UESimulationSession;

#if !UE_BUILD_SHIPPING
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGLatencyBudget, Log, All);
#endif

template <typename TraitsT>
struct UELatencyBudgetPort
{
	const UESimulationSession<TraitsT>* session = nullptr;

	double nowSeconds() const { return FPlatformTime::Seconds(); }

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	void logLatency(const char* line) const { UE_LOG(LogOGLatencyBudget, Warning, TEXT("%s"), ANSI_TO_TCHAR(line)); }
#else
	void logLatency(const char*) const {}
#endif

	void checkOp(ogsession::SessionOp op) const { session->runtime().checkDiagnosticsOp(op); }
};

template <typename TraitsT>
using UELatencyBudgetCore = ogsession::SessionLatencyDiagnostics<TraitsT, UESimulationSession<TraitsT>,
	UELatencyBudgetPort<TraitsT>, (OG_LATENCY_BUDGET_DIAGNOSTICS != 0)>;

template <typename TraitsT>
class UELatencyBudgetDiagnostics : public UELatencyBudgetCore<TraitsT>
{
	using Core = UELatencyBudgetCore<TraitsT>;

public:
	using InputProvider = typename Core::InputProvider;
	using ClockCounts   = typename Core::ClockCounts;

	explicit UELatencyBudgetDiagnostics(const UESimulationSession<TraitsT>& session)
		: Core(session, UELatencyBudgetPort<TraitsT>{ &session })
	{
	}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	void begin(UWorld& world, bool isAuthority);
	void end(UWorld& world);

	void tick_GameThread() { this->tickWindow_Frame(); }

	template <typename FunctionT>
	static void withInstance(const UWorld* world, bool isAuthority, FunctionT&& function)
	{
		if (UELatencyBudgetDiagnostics* diagnostics = instanceFor(world, isAuthority))
			function(*diagnostics);
	}

private:
	static UELatencyBudgetDiagnostics* instanceFor(const UWorld* world, bool isAuthority);

	FDelegateHandle m_postTickFlushHandle;
#else
	void begin(UWorld&, bool) {}
	void end(UWorld&) {}

	void tick_GameThread() {}

	template <typename FunctionT>
	static void withInstance(const UWorld*, bool, FunctionT&&) {}
#endif
};

#if OG_LATENCY_BUDGET_DIAGNOSTICS

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
UELatencyBudgetDiagnostics<TraitsT>* UELatencyBudgetDiagnostics<TraitsT>::instanceFor(const UWorld* world, bool isAuthority)
{
	typename TraitsT::SessionOwner* manager = TraitsT::SessionOwner::instanceFor(world, isAuthority);
	return manager != nullptr ? &manager->editLatencyBudgetDiagnostics() : nullptr;
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::begin(UWorld& world, bool isAuthority)
{
	Core::begin(isAuthority);
	m_postTickFlushHandle =
		world.OnPostTickFlush().AddWeakLambda(&this->diagnosticsOwner().owner(), [this]() { this->onPostTickFlush(); });
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::end(UWorld& world)
{
	if (m_postTickFlushHandle.IsValid())
	{
		world.OnPostTickFlush().Remove(m_postTickFlushHandle);
		m_postTickFlushHandle.Reset();
	}
}

OGSIM_OPTIMIZE_ON

#endif
