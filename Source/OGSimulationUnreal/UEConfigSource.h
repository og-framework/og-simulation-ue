// SPDX-License-Identifier: MPL-2.0
// docs/UEConfigSource-rationale.md

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "OGSimSession/SessionEngine.h"

#include <cstdint>
#include <optional>
#include <string>

class UEConfigSource
{
public:
	std::optional<int32_t> readInt(const char* key) const
	{
		if (GConfig == nullptr)
			return std::nullopt;
		const FString keyText(UTF8_TO_TCHAR(key));
		int32 value = 0;
		if (GConfig->GetInt(kSection, *keyText, value, GGameIni) || GConfig->GetInt(kSection, *keyText, value, GEngineIni))
			return value;
		return std::nullopt;
	}

	std::optional<double> readDouble(const char* key) const
	{
		if (GConfig == nullptr)
			return std::nullopt;
		const FString keyText(UTF8_TO_TCHAR(key));
		double value = 0.0;
		if (GConfig->GetDouble(kSection, *keyText, value, GGameIni) || GConfig->GetDouble(kSection, *keyText, value, GEngineIni))
			return value;
		return std::nullopt;
	}

	std::optional<std::string> readString(const char* key) const
	{
		if (GConfig == nullptr)
			return std::nullopt;
		const FString keyText(UTF8_TO_TCHAR(key));
		FString value;
		if (GConfig->GetString(kSection, *keyText, value, GGameIni) || GConfig->GetString(kSection, *keyText, value, GEngineIni))
			return std::string(TCHAR_TO_UTF8(*value.TrimStartAndEnd()));
		return std::nullopt;
	}

private:
	static constexpr const TCHAR* kSection = TEXT("OGNetcode");
};

static_assert(ogsession::ConfigSource<UEConfigSource>,
	"UEConfigSource must model og-sim-session's ConfigSource: the [OGNetcode] section's int, double and string reads, each "
	"std::optional");
