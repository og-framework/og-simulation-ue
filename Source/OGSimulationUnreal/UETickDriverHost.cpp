// SPDX-License-Identifier: MPL-2.0
// docs/UETickDriverHost-rationale.md · docs/UETickDriverHost-guards.md

#include "UETickDriverHost.h"

#include "Engine/Level.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSimMaxCatchUpSteps(
		TEXT("og.Sim.MaxCatchUpSteps"),
		60,
		TEXT("Jolt configuration only. The most physics steps one frame may run; the scheduler drops the rest of a ")
		TEXT("longer frame and counts it as lost time. Read once, at the simulation manager's BeginPlay. 60 is ")
		TEXT("Chaos's one-second clamp at 60 Hz, on both roles."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSimBlockMode(
		TEXT("og.Sim.BlockMode"),
		0,
		TEXT("Jolt configuration only, for debugging. 0 = end of physics waits only for the step tasks of earlier ")
		TEXT("frames (Chaos block mode 0). 1 = it also waits for this frame's step task."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSimRunInline(
		TEXT("og.Sim.RunInline"),
		0,
		TEXT("Retired: a client always steps on a worker task. Any value but 0 is refused at the simulation manager's ")
		TEXT("BeginPlay ([SimHost.Refused] knob=og.Sim.RunInline): that world starts no simulation session, and a dedicated ")
		TEXT("server exits with code 3."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarTestClientClockRatePpm(
		TEXT("og.Sim.TestClientClockRatePpm"),
		0,
		TEXT("Test only (non-shipping): when not 0, the start-physics tick function hands the tick driver the frame's ")
		TEXT("DeltaTime times (1 + this * 1e-6), an emulated clock-rate offset in parts per million. Read every frame. A ")
		TEXT("free-running authority does not read the frame delta, so it reaches clients only."),
		ECVF_Default);

	double testClientClockRateFrameDelta(double frameDeltaSeconds)
	{
		const int32 ppm = CVarTestClientClockRatePpm.GetValueOnGameThread();
		if (ppm == 0)
			return frameDeltaSeconds;

		static bool logged = false;
		if (!logged)
		{
			logged = true;
			UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.TestClockRate] ppm=%d"), ppm);
		}
		return frameDeltaSeconds * (1.0 + static_cast<double>(ppm) * 1e-6);
	}
#endif
}

void FSimulationStartStepsTickFunctionUImpl::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(startFrame != nullptr);
	double frameDeltaSeconds = static_cast<double>(DeltaTime);
#if !UE_BUILD_SHIPPING
	frameDeltaSeconds = testClientClockRateFrameDelta(frameDeltaSeconds);
#endif
	startFrame(target, frameDeltaSeconds);
}

FString FSimulationStartStepsTickFunctionUImpl::DiagnosticMessage()
{
	return TEXT("FSimulationStartStepsTickFunctionUImpl");
}

FName FSimulationStartStepsTickFunctionUImpl::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("OGSimStartSteps"));
}

void FSimulationEndStepsTickFunctionUImpl::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(endFrame != nullptr);
	endFrame(target, &MyCompletionGraphEvent, CVarSimBlockMode.GetValueOnGameThread());
}

FString FSimulationEndStepsTickFunctionUImpl::DiagnosticMessage()
{
	return TEXT("FSimulationEndStepsTickFunctionUImpl");
}

FName FSimulationEndStepsTickFunctionUImpl::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("OGSimEndSteps"));
}

int32 UETickDriverHost::requestedMaxCatchUpSteps()
{
	return CVarSimMaxCatchUpSteps.GetValueOnGameThread();
}

int32 UETickDriverHost::retiredRunInlineValue()
{
	return CVarSimRunInline.GetValueOnGameThread();
}

void UETickDriverHost::registerTickFunctions(UObject& owner, ULevel& level, void* target,
	StartFrameFn startFrame, EndFrameFn endFrame)
{
	checkf(m_owner == nullptr, TEXT("UETickDriverHost::begin: the frame host is already running."));

	m_owner = &owner;
	m_engine.setOwner(owner);

	m_startTick.target               = target;
	m_startTick.startFrame           = startFrame;
	m_startTick.bCanEverTick         = true;
	m_startTick.bStartWithTickEnabled = true;
	m_startTick.TickGroup            = TG_StartPhysics;
	m_startTick.EndTickGroup         = TG_StartPhysics;
	m_startTick.RegisterTickFunction(&level);

	m_endTick.target               = target;
	m_endTick.endFrame             = endFrame;
	m_endTick.bCanEverTick         = true;
	m_endTick.bStartWithTickEnabled = true;
	m_endTick.TickGroup            = TG_EndPhysics;
	m_endTick.EndTickGroup         = TG_EndPhysics;
	m_endTick.RegisterTickFunction(&level);
	m_endTick.AddPrerequisite(&owner, m_startTick);
}

void UETickDriverHost::unregisterTickFunctions()
{
	if (m_owner == nullptr)
		return;

	if (m_endTick.IsTickFunctionRegistered())
	{
		m_endTick.RemovePrerequisite(m_owner, m_startTick);
		m_endTick.UnRegisterTickFunction();
	}
	if (m_startTick.IsTickFunctionRegistered())
		m_startTick.UnRegisterTickFunction();
}
