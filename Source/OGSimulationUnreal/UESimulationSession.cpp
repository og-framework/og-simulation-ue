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

	TAutoConsoleVariable<float> CVarTestGameThreadHitchEverySeconds(
		TEXT("og.Sim.TestGameThreadHitchEverySeconds"),
		0.f,
		TEXT("Test only (non-shipping): when above 0, og.Sim.TestGameThreadHitchMs repeats: it is not reset when it fires, ")
		TEXT("and fires again at the first OnPostPhysicsStep at least this many seconds after its previous firing. 0 = once."),
		ECVF_Default);
}

OGSIM_OPTIMIZE_OFF

int32 consumeTestGameThreadHitchMs()
{
	static const double firstPassSeconds = FPlatformTime::Seconds();
	static double lastFireSeconds = -1.0;
	const int32 hitchMs = CVarTestGameThreadHitchMs.GetValueOnGameThread();
	if (hitchMs > 0)
	{
		const double nowSeconds = FPlatformTime::Seconds();
		const int32 afterSeconds = CVarTestGameThreadHitchAfterSeconds.GetValueOnGameThread();
		if (afterSeconds > 0 && nowSeconds - firstPassSeconds < static_cast<double>(afterSeconds))
		{
			return 0;
		}
		const float everySeconds = CVarTestGameThreadHitchEverySeconds.GetValueOnGameThread();
		if (everySeconds > 0.f && lastFireSeconds >= 0.0 && nowSeconds - lastFireSeconds < static_cast<double>(everySeconds))
		{
			return 0;
		}
		lastFireSeconds = nowSeconds;
		if (everySeconds <= 0.f)
		{
			CVarTestGameThreadHitchMs->Set(0, ECVF_SetByConsole);
		}
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.TestHitch] ms=%d afterSeconds=%d frame=%llu everySeconds=%.3f"), hitchMs,
			afterSeconds, static_cast<unsigned long long>(GFrameCounter), static_cast<double>(everySeconds));
	}
	return hitchMs;
}

void sleepTestGameThreadHitch(int32 hitchMs)
{
	FPlatformProcess::Sleep(static_cast<float>(hitchMs) / 1000.f);
}

OGSIM_OPTIMIZE_ON
#endif
