// SPDX-License-Identifier: MPL-2.0
// docs/UESessionEngine-rationale.md

#pragma once

#include "OGSimulationUnreal.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "OGTickDriver/TickDriverConcepts.h"
#include "OGTickDriver/TickDriverLog.h"
#include "OGTickDriver/TickDriverPresenter.h"
#include "OGSimSession/SessionEngine.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimulationUnreal/UEAuthorityRunner.h"
#include "OGSimulationUnreal/UEPresentControls.h"
#include "OGSimulationUnreal/UERenderTargetTable.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulation/SimulationComparison.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

class UObject;
class UWorld;

class UESessionRunner
{
public:
	using Host = FUEAuthorityRunnerHost;

	template <typename F>
	UESessionRunner(F&& body, bool highResolutionWait)
		: m_runner([this, body = std::forward<F>(body)](FUEAuthorityRunner&) mutable { body(*this); }, highResolutionWait)
	{
	}

	UESessionRunner(const UESessionRunner&) = delete;
	UESessionRunner& operator=(const UESessionRunner&) = delete;

	Host& host() { return m_runner.host(); }
	const std::atomic<bool>& stopRequested() const { return m_runner.stopRequested(); }
	void start() { m_runner.start_GameThread(); }
	void waitForStart() { m_runner.waitForStart_GameThread(); }
	void releaseStart() { m_runner.releaseStart(); }
	void join() { m_runner.join_GameThread(); }

private:
	FUEAuthorityRunner m_runner;
};

static_assert(ogsession::SessionRunner<UESessionRunner>,
	"UESessionRunner is og-sim-session's SessionRunner over FUEAuthorityRunner: the OGSimAuthority thread, its sleep host, "
	"the stop flag, the start latch and the join (og-session-runtime task 31)");

class OGSIMULATIONUNREAL_API SimulationEngineUImpl
{
public:
	using Runner = UESessionRunner;

	void logRouted(const char* line);
	bool wants(SimulationLogRoute route, HostLogLevel level) const { return ogLogWants(route, level); }

	template <typename F>
	std::unique_ptr<Runner> startRunner(F&& body, bool highResolutionWait)
	{
		std::unique_ptr<Runner> runner = std::make_unique<Runner>(std::forward<F>(body), highResolutionWait);
		runner->start();
		return runner;
	}

	using Handle     = FGraphEventRef;
	using Completion = const FGraphEventRef;

	static constexpr bool kHostDiagnostics = !UE_BUILD_SHIPPING;

	SimulationEngineUImpl() = default;
	SimulationEngineUImpl(const SimulationEngineUImpl&) = delete;
	SimulationEngineUImpl& operator=(const SimulationEngineUImpl&) = delete;

	double nowSeconds() const;
	uint64_t frameNumber() const;
	bool logEnabled(HostLogLevel level) const;
	void log(HostLogLevel level, const char* line);

	bool isPending(const Handle& steps) const { return steps.IsValid() && !steps->IsComplete(); }
	void waitFor(const Handle& steps);

	template <typename F>
	Handle dispatchAfter(const Handle& previous, F&& work)
	{
		FGraphEventArray prerequisites;
		if (previous.IsValid())
			prerequisites.Add(previous);
		return FFunctionGraphTask::CreateAndDispatchWhenReady(std::forward<F>(work), TStatId{}, &prerequisites,
			stepTaskPriority());
	}

	template <typename F>
	bool deferUntil(Completion& completion, const Handle& steps, F&& continuation)
	{
		if (!completion.IsValid())
			return false;

		FGraphEventArray prerequisites;
		prerequisites.Add(steps);
		completion->DontCompleteUntil(FFunctionGraphTask::CreateAndDispatchWhenReady(
			[owner = m_owner, continuation = std::forward<F>(continuation)]() mutable
			{
				if (owner.Get() == nullptr)
					return;
				continuation();
			},
			TStatId{}, &prerequisites, ENamedThreads::GameThread));
		return true;
	}

	void setOwner(UObject& owner) { m_owner = &owner; }

private:
	static ENamedThreads::Type stepTaskPriority();

	TWeakObjectPtr<UObject> m_owner;
};

static_assert(TickDriverEngine<SimulationEngineUImpl>,
	"SimulationEngineUImpl must model og-simulation's TickDriverEngine: the clock, the frame number, the log sink, a "
	"constant kHostDiagnostics and the task-graph executor half.");
static_assert(ogsession::SessionEngine<SimulationEngineUImpl>,
	"SimulationEngineUImpl is og-sim-session's SessionEngine: the tick driver's engine, logRouted (the log routes' "
	"RouteOGLevelLine) and startRunner over FUEAuthorityRunner (og-session-runtime task 31)");

namespace ueSessionEngine
{
OGSIMULATIONUNREAL_API void importStatics(UWorld& world, const std::vector<UEStaticCategoryChannel>& categoryChannels,
	bool isAuthority, ogsession::SessionStart& start);
// ⛔G-03  docs/UESessionEngine-guards.md
inline void bindDiagnosticGates()
{
	// ⛔G-01  docs/UESessionEngine-guards.md
	correctionFieldDiff::setEnabledPredicate([]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
	static_assert(ogLogRouteFor(TEXT("[DivergenceProbe.Margin]")) != nullptr
		&& ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))->route == OGLogRoute::DivergenceProbe
		&& ogLogRouteFor(TEXT("[DivergenceProbe.Margin]"))->plainLevel == ELogVerbosity::Log,
		"bindDiagnosticGates: the margin predicate below asks LogOGDivergenceProbe at Log, so the [DivergenceProbe.Margin] "
		"line must route to OGLogRoute::DivergenceProbe at plain level Log. Re-route or re-level the [DivergenceProbe prefix "
		"and move the predicate with it (og-input-latency task 2h)");
	// ⛔G-02  docs/UESessionEngine-guards.md
	correctionVerdictMargin::setEnabledPredicate([]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Log); });
}

OGSIMULATIONUNREAL_API void logResimProbeSession();
} // namespace ueSessionEngine

class UESessionPresentation
{
public:
	UERenderTargetTable& renderTargets() { return m_renderTargets; }

	template <typename RuntimeT>
	void present(RuntimeT& runtime, float parkSnap)
	{
		runtime.present(UEPresentControls::fromConsoleVariables(parkSnap), m_renderTargets);
	}

private:
	UERenderTargetTable m_renderTargets;
};
