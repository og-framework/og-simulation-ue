// SPDX-License-Identifier: MPL-2.0
// docs/UEAuthorityRunner-rationale.md

#include "OGSimulationUnreal/UEAuthorityRunner.h"

#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"

#if PLATFORM_LINUX
#include <cerrno>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarSimAuthorityLoop(
		TEXT("og.Sim.AuthorityLoop"),
		1,
		TEXT("Retired: a dedicated server always steps its authority on its own thread (OGSimAuthority) and services og-net ")
		TEXT("there. 0 is refused at the simulation manager's BeginPlay ([SimHost.Refused] knob=og.Sim.AuthorityLoop): the ")
		TEXT("dedicated server starts no simulation session and exits with code 3. Any other value is accepted silently."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarSimAuthorityIdleSlackMs(
		TEXT("og.Sim.AuthorityIdleSlackMs"),
		PLATFORM_WINDOWS ? 0.5f : 0.f,
		TEXT("The authority loop wakes this many milliseconds before each step deadline and polls og-net until the deadline. ")
		TEXT("0 = no polling. Default 0.5 on Win64 (Ph4's gate), 0 elsewhere (not measured). Read once when the loop starts; ")
		TEXT("clamped below one step."),
		ECVF_Default);
}

OGSIM_OPTIMIZE_OFF

int32 retiredAuthorityLoopValue()
{
	return CVarSimAuthorityLoop.GetValueOnGameThread();
}

double readAuthorityIdleSlackSeconds()
{
	return FMath::Max(0.0, static_cast<double>(CVarSimAuthorityIdleSlackMs.GetValueOnGameThread())) / 1000.0;
}

void FUEAuthorityRunnerHost::sleepUntil(double deadlineSeconds)
{
	const double remainingSeconds = deadlineSeconds - FPlatformTime::Seconds();
	if (remainingSeconds > 0.0)
		FPlatformProcess::Sleep(static_cast<float>(remainingSeconds));
}

FUEAuthorityRunner::FUEAuthorityRunner(FBody body, bool highResolutionWait)
	: m_body(MoveTemp(body))
	, m_highResolutionWait(highResolutionWait)
	, m_startLatch(FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/true))
{
	checkf(static_cast<bool>(m_body), TEXT("FUEAuthorityRunner: a runner runs one body: the session's authority loop"));
}

FUEAuthorityRunner::~FUEAuthorityRunner()
{
	checkf(m_thread == nullptr || m_joined,
		TEXT("FUEAuthorityRunner: destroyed with its thread still running; the loop would step a destroyed session. Was P11 "
			 "design D7's stop: requestStop, the runner returns, the host joins it"));
	FPlatformProcess::ReturnSynchEventToPool(m_startLatch);
	m_startLatch = nullptr;
}

void FUEAuthorityRunner::start_GameThread()
{
	checkf(IsInGameThread() && m_thread == nullptr,
		TEXT("FUEAuthorityRunner::start_GameThread: the game thread starts the runner once, at the session's BeginPlay"));
	m_thread = FRunnableThread::Create(this, TEXT("OGSimAuthority"), 0, TPri_AboveNormal);
	checkf(m_thread != nullptr, TEXT("FUEAuthorityRunner: the OGSimAuthority thread could not be created"));
}

void FUEAuthorityRunner::waitForStart_GameThread()
{
	checkf(IsInGameThread() && m_thread != nullptr,
		TEXT("FUEAuthorityRunner::waitForStart_GameThread: the game thread waits on the start latch after it started the runner"));
	const double waitStartSeconds = FPlatformTime::Seconds();
	m_startLatch->Wait();
	UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Runner] startLatch waitMs=%.3f"),
		(FPlatformTime::Seconds() - waitStartSeconds) * 1000.0);
}

void FUEAuthorityRunner::releaseStart()
{
	if (m_startReleased.exchange(true, std::memory_order_acq_rel))
		return;
	m_startLatch->Trigger();
}

void FUEAuthorityRunner::join_GameThread()
{
	checkf(IsInGameThread(), TEXT("FUEAuthorityRunner::join_GameThread: the game thread stops and joins the runner"));
	if (m_thread == nullptr || m_joined)
		return;
	m_stopRequested.store(true, std::memory_order_release);
	m_thread->WaitForCompletion();
	delete m_thread;
	m_thread = nullptr;
	m_joined = true;
}

uint32 FUEAuthorityRunner::Run()
{
	logStartLine();
	m_body(*this);
	releaseStart();
	return 0u;
}

void FUEAuthorityRunner::logStartLine() const
{
#if PLATFORM_LINUX
	errno = 0;
	const int nice = getpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)));
	const FString niceText = errno == 0 ? FString::FromInt(nice) : FString(TEXT("-"));
#else
	const FString niceText(TEXT("-"));
#endif
	UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Runner] priority=AboveNormal nice=%s highResWait=%d"), *niceText,
		m_highResolutionWait ? 1 : 0);
}

OGSIM_OPTIMIZE_ON
