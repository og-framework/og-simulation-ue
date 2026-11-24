// SPDX-License-Identifier: MPL-2.0
// docs/UESessionRefusal-rationale.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CoreMisc.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"

#include <cstddef>
#include <cstdio>

inline constexpr uint64 kSimNetDriverRefusedToastKey = 0x4F47534EF0000002ull;
inline constexpr float kSimNetDriverRefusedToastSeconds = 15.f;

namespace ueSessionRefusal
{
	inline constexpr char kRefusedLineFormat[] = "[SimHost.Refused] %s value=%s reason=%s";
	inline constexpr size_t kRefusedLineBytes = 160u;
	inline constexpr uint8 kServerExitCode = 3u;

	static_assert(kRefusedLineBytes >= sizeof(kRefusedLineFormat) + sizeof("role=authority netMode=DedicatedServer")
			+ sizeof("-2147483648") + sizeof("one-session-per-role-per-process"),
		"kRefusedLineBytes must hold the [SimHost.Refused] line with the longest subject (a second dedicated server's role), "
		"an int32 value and the longest reason (a second session of a role in one process)");

	inline void logRefusal(const char* subject, const char* value, const char* reason)
	{
		char line[kRefusedLineBytes];
		std::snprintf(line, sizeof(line), kRefusedLineFormat, subject, value, reason);
		UE_LOG(LogOGSimHost, Error, TEXT("%hs"), line);
	}

	inline void refuse(const char* subject, const char* value, const char* reason)
	{
		logRefusal(subject, value, reason);
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
				FString::Printf(TEXT("Simulation refused: %hs value=%hs reason=%hs. This world runs no simulation session"),
					subject, value, reason));
		}
	}

	inline void requestServerExit()
	{
		if (IsRunningDedicatedServer())
			FPlatformMisc::RequestExitWithStatus(/*Force=*/true, kServerExitCode);
	}
}
