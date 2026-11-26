// SPDX-License-Identifier: MPL-2.0
// docs/UETickDriverHost-rationale.md

#pragma once

#include "OGSimulationUnreal.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "OGTickDriver/TickDriverConcepts.h"
#include "OGTickDriver/TickDriverLog.h"

#include <cstdint>
#include <utility>

class UObject;

class OGSIMULATIONUNREAL_API SimulationEngineUImpl
{
public:
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
