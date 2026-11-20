// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#include "UESimulationSession.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#if !UE_BUILD_SHIPPING
namespace
{
	TAutoConsoleVariable<int32> CVarTestGameThreadHitchMs(
		TEXT("og.Sim.TestGameThreadHitchMs"),
		0,
		TEXT("Test only (non-shipping): when above 0, the next OnPostPhysicsStep sleeps the game thread ")
		TEXT("this many milliseconds, once, and resets this variable to 0."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarTestGameThreadHitchAfterSeconds(
		TEXT("og.Sim.TestGameThreadHitchAfterSeconds"),
		0,
		TEXT("Test only (non-shipping): og.Sim.TestGameThreadHitchMs fires at the first OnPostPhysicsStep at least this many ")
		TEXT("seconds after the process's first OnPostPhysicsStep. 0 = at the next OnPostPhysicsStep."),
		ECVF_Default);
}

OGSIM_OPTIMIZE_OFF

int32 consumeTestGameThreadHitchMs()
{
	static const double firstPassSeconds = FPlatformTime::Seconds();
	const int32 hitchMs = CVarTestGameThreadHitchMs.GetValueOnGameThread();
	if (hitchMs > 0)
	{
		const int32 afterSeconds = CVarTestGameThreadHitchAfterSeconds.GetValueOnGameThread();
		if (afterSeconds > 0 && FPlatformTime::Seconds() - firstPassSeconds < static_cast<double>(afterSeconds))
		{
			return 0;
		}
		CVarTestGameThreadHitchMs->Set(0, ECVF_SetByConsole);
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.TestHitch] ms=%d afterSeconds=%d frame=%llu"), hitchMs, afterSeconds,
			static_cast<unsigned long long>(GFrameCounter));
	}
	return hitchMs;
}

void sleepTestGameThreadHitch(int32 hitchMs)
{
	FPlatformProcess::Sleep(static_cast<float>(hitchMs) / 1000.f);
}

OGSIM_OPTIMIZE_ON
#endif
