// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationSession-rationale.md · docs/UESimulationSession-guards.md

#include "UESimulationSession.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"

#if !UE_BUILD_SHIPPING
namespace
{
	TAutoConsoleVariable<int32> CVarTestGameThreadHitchMs(
		TEXT("og.Sim.TestGameThreadHitchMs"),
		0,
		TEXT("Test only (non-shipping): when above 0, the next OnPostPhysicsStep sleeps the game thread ")
		TEXT("this many milliseconds, once, and resets this variable to 0."),
		ECVF_Default);
}

OGSIM_OPTIMIZE_OFF

int32 consumeTestGameThreadHitchMs()
{
	const int32 hitchMs = CVarTestGameThreadHitchMs.GetValueOnGameThread();
	if (hitchMs > 0)
	{
		CVarTestGameThreadHitchMs->Set(0, ECVF_SetByConsole);
	}
	return hitchMs;
}

void sleepTestGameThreadHitch(int32 hitchMs)
{
	FPlatformProcess::Sleep(static_cast<float>(hitchMs) / 1000.f);
}

OGSIM_OPTIMIZE_ON
#endif
