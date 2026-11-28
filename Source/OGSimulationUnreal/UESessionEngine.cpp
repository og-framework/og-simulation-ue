// SPDX-License-Identifier: MPL-2.0
// docs/UESessionEngine-rationale.md

#include "UESessionEngine.h"

#include "HAL/PlatformTime.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"

namespace
{
	FAutoConsoleTaskPriority CPrio_OGSimStepTask(
		TEXT("TaskGraph.TaskPriorities.OGSimStepTask"),
		TEXT("Task and thread priority for the client's Jolt step task (mirrors TaskGraph.TaskPriorities.PhysicsTickTask)"),
		ENamedThreads::HighThreadPriority,
		ENamedThreads::NormalTaskPriority,
		ENamedThreads::HighTaskPriority);
}

double SimulationEngineUImpl::nowSeconds() const
{
	return FPlatformTime::Seconds();
}

uint64_t SimulationEngineUImpl::frameNumber() const
{
	return GFrameCounter;
}

bool SimulationEngineUImpl::logEnabled(HostLogLevel level) const
{
	switch (level)
	{
	case HostLogLevel::Verbose:
		return UE_LOG_ACTIVE(LogOGSimHost, Verbose);
	case HostLogLevel::Log:
		return UE_LOG_ACTIVE(LogOGSimHost, Log);
	case HostLogLevel::Warning:
		return UE_LOG_ACTIVE(LogOGSimHost, Warning);
	}
	return false;
}

void SimulationEngineUImpl::log(HostLogLevel level, const char* line)
{
	switch (level)
	{
	case HostLogLevel::Verbose:
		UE_LOG(LogOGSimHost, Verbose, TEXT("%hs"), line);
		return;
	case HostLogLevel::Log:
		UE_LOG(LogOGSimHost, Log, TEXT("%hs"), line);
		return;
	case HostLogLevel::Warning:
		UE_LOG(LogOGSimHost, Warning, TEXT("%hs"), line);
		return;
	}
}

ENamedThreads::Type SimulationEngineUImpl::stepTaskPriority()
{
	return CPrio_OGSimStepTask.Get();
}

void SimulationEngineUImpl::waitFor(const Handle& steps)
{
	checkf(steps.IsValid(),
		TEXT("SimulationEngineUImpl::waitFor: the tick driver waits only for a step task it dispatched, so the event is ")
		TEXT("valid. Was the frame host's IsValid filter before its waits."));
	FTaskGraphInterface::Get().WaitUntilTaskCompletes(steps, ENamedThreads::GameThread_Local);
}

void SimulationEngineUImpl::logRouted(const char* line)
{
	RouteOGLevelLine(line);
}

void ueSessionEngine::importStatics(UWorld& world, const std::vector<UEStaticCategoryChannel>& categoryChannels,
	bool isAuthority, ogsession::SessionStart& start)
{
	UEStaticImportResult import = UEStaticGeometryImporter(categoryChannels).importWorld(world);
	UEStaticGeometryImporter::logReport(import.report, isAuthority ? TEXT("authority") : TEXT("client"));
	start.staticWorld = std::move(import.description);
	start.physicsOnlyShapes = std::move(import.physicsOnly);
}

void ueSessionEngine::logResimProbeSession()
{
#if !NO_LOGGING
	UE_LOG(LogOGResimProbe, Warning,
		TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
		ToString(LogOGResimProbe.GetVerbosity()), LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
		static_cast<uint32>(kResimGateProbeWindowSamples));
#endif
}
