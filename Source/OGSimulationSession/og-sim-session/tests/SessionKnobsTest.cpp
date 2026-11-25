// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionKnobs.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
enum class Ini : uint8_t { Game, Engine };

class FakeGConfig
{
public:
	std::map<std::string, std::string> game;
	std::map<std::string, std::string> engine;

	bool GetInt(const char* section, const char* key, int32_t& value, Ini ini) const
	{
		const std::string* text = find(section, key, ini);
		if (text == nullptr)
			return false;
		const long parsed = std::strtol(text->c_str(), nullptr, 10);
		value = static_cast<int32_t>(std::clamp<long>(parsed, INT32_MIN, INT32_MAX));
		return true;
	}

	bool GetDouble(const char* section, const char* key, double& value, Ini ini) const
	{
		const std::string* text = find(section, key, ini);
		if (text == nullptr)
			return false;
		value = std::strtod(text->c_str(), nullptr);
		return true;
	}

	bool GetString(const char* section, const char* key, std::string& value, Ini ini) const
	{
		const std::string* text = find(section, key, ini);
		if (text == nullptr)
			return false;
		value = *text;
		return true;
	}

private:
	const std::string* find(const char* section, const char* key, Ini ini) const
	{
		REQUIRE(std::string(section) == "OGNetcode");
		const std::map<std::string, std::string>& file = ini == Ini::Game ? game : engine;
		const auto found = file.find(key);
		return found != file.end() ? &found->second : nullptr;
	}
};

bool isUnrealWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string trimStartAndEnd(const std::string& text)
{
	std::size_t first = 0;
	while (first < text.size() && isUnrealWhitespace(text[first]))
		++first;
	std::size_t last = text.size();
	while (last > first && isUnrealWhitespace(text[last - 1]))
		--last;
	return text.substr(first, last - first);
}

class FakeUEConfigSource
{
public:
	explicit FakeUEConfigSource(const FakeGConfig* gconfig) : m_gconfig(gconfig) {}

	std::optional<int32_t> readInt(const char* key) const
	{
		if (m_gconfig == nullptr)
			return std::nullopt;
		int32_t value = 0;
		if (m_gconfig->GetInt("OGNetcode", key, value, Ini::Game) || m_gconfig->GetInt("OGNetcode", key, value, Ini::Engine))
			return value;
		return std::nullopt;
	}

	std::optional<double> readDouble(const char* key) const
	{
		if (m_gconfig == nullptr)
			return std::nullopt;
		double value = 0.0;
		if (m_gconfig->GetDouble("OGNetcode", key, value, Ini::Game) || m_gconfig->GetDouble("OGNetcode", key, value, Ini::Engine))
			return value;
		return std::nullopt;
	}

	std::optional<std::string> readString(const char* key) const
	{
		if (m_gconfig == nullptr)
			return std::nullopt;
		std::string value;
		if (m_gconfig->GetString("OGNetcode", key, value, Ini::Game) || m_gconfig->GetString("OGNetcode", key, value, Ini::Engine))
			return trimStartAndEnd(value);
		return std::nullopt;
	}

private:
	const FakeGConfig* m_gconfig;
};

static_assert(ogsession::ConfigSource<FakeUEConfigSource>, "the test's UE config source models ConfigSource");

class FakeKnobManager
{
public:
	explicit FakeKnobManager(bool runsPrediction) : m_runsPrediction(runsPrediction) {}

	bool runsPrediction() const { return m_runsPrediction; }
	const TimeConfig& getTimeConfig() const { return m_timeConfig; }

	void setRelayDelayFloorTicks(int32_t requestedFloorTicks)
	{
		m_timeConfig.relayDelayFloorTicks = clampRelayDelayFloorTicks(requestedFloorTicks, m_timeConfig);
	}
	void setCorrectionRotationK(int32_t requestedK) { m_timeConfig.correctionRotationK = correctionRotation::clampK(requestedK); }
	void setRedundancyFrames(int32_t requestedFrames)
	{
		m_timeConfig.redundancyFrames = latencyKnobs::clampRedundancyFrames(requestedFrames);
	}
	void setRelayResendEntries(int32_t requestedEntries)
	{
		m_timeConfig.relayResendEntries = latencyKnobs::clampRelayResendEntries(requestedEntries);
	}
	void setRecentreSettleSamples(uint32_t samples) { m_timeConfig.recentreSettleSamples = samples; }
	void setClockRttSource(TimeConfig::ClockRttSource source) { m_timeConfig.clockRttSource = source; }
	void setJitterMultiplier(double multiplier)
	{
		const double clamped = clockJitter::clampJitterMultiplier(multiplier);
		m_timeConfig.jitterMultiplier         = clamped;
		m_timeConfig.simClockJitterMultiplier = clamped;
	}
	void setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy policy) { m_timeConfig.resimTriggerPolicy = policy; }

private:
	bool       m_runsPrediction;
	TimeConfig m_timeConfig;
};

struct FakeFloorServer
{
	std::vector<std::string>* events;

	void publishFloor(uint8_t floorTicks) { events->push_back("publishFloor " + std::to_string(floorTicks)); }
};

template <typename... ArgsT>
std::string printfString(const char* format, ArgsT... args)
{
	if constexpr (sizeof...(ArgsT) == 0)
	{
		return std::string(format);
	}
	else
	{
		const int length = std::snprintf(nullptr, 0, format, args...);
		std::string text(static_cast<std::size_t>(length), '\0');
		std::snprintf(text.data(), text.size() + 1u, format, args...);
		return text;
	}
}

// The transcription: OGSimulation 0eb05d0's OGSimSession/SessionKnobs.h, the (a) commit's verbatim copy of
// UESimulationSession.h at OGSimulation ebd73bf (:98-143, :506-534, :1375-1844). Each Unreal call is replaced:
// UE_LOG(LogOGNet, <Level>, TEXT(f), ...) -> ueLog(<Level>, f, ...) (%hs -> %s); GConfig -> FakeGConfig; FString -> std::string,
// TrimStartAndEnd -> trimStartAndEnd, Equals(IgnoreCase) -> stricmpEquals (FGenericPlatformStricmp: ASCII folds only);
// FString::Printf -> printfString; FMath::Max -> std::max; FMath::IsFinite -> std::isfinite; checkf -> CHECK; the
// session's logRelayDelayFloorAdvisory -> an event.
namespace transcribedDoors
{
	// SessionKnobs.h:10-38 at 0eb05d0
	inline constexpr double kMinGainPerTick       = 0.0;
	inline constexpr double kMaxGainPerTick       = 0.05;
	inline constexpr double kMinMaxPercent        = 0.0;
	inline constexpr double kMaxMaxPercent        = 2.0;
	inline constexpr double kMinTargetOffsetTicks = -2.0;
	inline constexpr double kMaxTargetOffsetTicks = 2.0;
	inline constexpr double kMinCommonLeadTicks   = 0.0;
	inline constexpr double kMaxCommonLeadTicks   = 10.0;

	constexpr double clampToDoor(double requested, double low, double high)
	{
		if (!(requested >= low)) return low;
		if (requested > high) return high;
		return requested;
	}

	constexpr double clampGainPerTick(double requested) { return clampToDoor(requested, kMinGainPerTick, kMaxGainPerTick); }
	constexpr double clampMaxPercent(double requested) { return clampToDoor(requested, kMinMaxPercent, kMaxMaxPercent); }
	constexpr double clampTargetOffsetTicks(double requested)
	{
		return clampToDoor(requested, kMinTargetOffsetTicks, kMaxTargetOffsetTicks);
	}
	constexpr double clampCommonLeadTicks(double requested)
	{
		return clampToDoor(requested, kMinCommonLeadTicks, kMaxCommonLeadTicks);
	}
} // namespace transcribedDoors

bool stricmpEquals(const std::string& text, const char* word)
{
	const std::string other(word);
	if (text.size() != other.size())
		return false;
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		const unsigned char a = static_cast<unsigned char>(text[i]);
		const unsigned char b = static_cast<unsigned char>(other[i]);
		if (a == b)
			continue;
		if (a < 128u && b < 128u && std::tolower(a) == std::tolower(b))
			continue;
		return false;
	}
	return true;
}

enum class Level : uint8_t { Log, Warning };

class TranscribedSession
{
public:
	// SessionKnobs.h:57-85 at 0eb05d0
	struct SessionKnobs
	{
		int32_t     configuredRelayDelayFloorTicks;
		int32_t     configuredCorrectionRotationK;
		bool        hasIniResimTriggerPolicy;
		std::string configuredResimTriggerPolicy;
		bool        hasIniRedundancyFrames;
		int32_t     configuredRedundancyFrames;
		bool        hasIniRelayResendEntries;
		int32_t     configuredRelayResendEntries;
		bool        hasIniRecentreSettleSamples;
		int32_t     configuredRecentreSettleSamples;
		bool        hasIniClockRttSource;
		std::string configuredClockRttSource;
		bool        hasIniJitterMultiplier;
		double      configuredJitterMultiplier;
		bool        hasIniClockDilation;
		std::string configuredClockDilation;
		bool        hasIniClockDilationGainPerTick;
		double      configuredClockDilationGainPerTick;
		bool        hasIniClockDilationMaxPercent;
		double      configuredClockDilationMaxPercent;
		bool        hasIniClockDilationTargetOffsetTicks;
		double      configuredClockDilationTargetOffsetTicks;
		bool        hasIniClockDilationSetpoint;
		std::string configuredClockDilationSetpoint;
		bool        hasIniClockDilationCommonLeadTicks;
		double      configuredClockDilationCommonLeadTicks;
	};

	const FakeGConfig*                     GConfig = nullptr;
	std::optional<FakeKnobManager>         m_manager;
	std::optional<FakeFloorServer>         m_simNetServer;
	std::optional<ClockDilationController> m_clockDilation;
	std::optional<ClockDilationWindow>     m_clockDilationWindow;
	std::vector<std::string>               events;

	template <typename... ArgsT>
	void ueLog(Level level, const char* format, ArgsT... args)
	{
		events.push_back(std::string("line ") + (level == Level::Warning ? "Warning " : "Log ") + printfString(format, args...));
	}

	void logRelayDelayFloorAdvisory(int32_t floorTicks) { events.push_back("advisory " + std::to_string(floorTicks)); }

	// SessionKnobs.h:87-254 at 0eb05d0
	SessionKnobs readSessionKnobs(bool worldIsAuthority)
	{
		int32_t configuredRelayDelayFloorTicks = -1;
		if (worldIsAuthority && GConfig != nullptr)
		{
			int32_t iniFloorTicks = 0;
			if (GConfig->GetInt("OGNetcode", "RelayDelayFloorTicks", iniFloorTicks, Ini::Game) ||
				GConfig->GetInt("OGNetcode", "RelayDelayFloorTicks", iniFloorTicks, Ini::Engine))
			{
				configuredRelayDelayFloorTicks = iniFloorTicks;
			}
		}

		int32_t configuredCorrectionRotationK = -1;
		if (worldIsAuthority && GConfig != nullptr)
		{
			int32_t iniK = 0;
			if (GConfig->GetInt("OGNetcode", "CorrectionRotationK", iniK, Ini::Game) ||
				GConfig->GetInt("OGNetcode", "CorrectionRotationK", iniK, Ini::Engine))
			{
				configuredCorrectionRotationK = iniK;
			}
		}

		bool        hasIniResimTriggerPolicy = false;
		std::string configuredResimTriggerPolicy;
		if (GConfig != nullptr)
		{
			std::string iniPolicy;
			if (GConfig->GetString("OGNetcode", "ResimTriggerPolicy", iniPolicy, Ini::Game) ||
				GConfig->GetString("OGNetcode", "ResimTriggerPolicy", iniPolicy, Ini::Engine))
			{
				hasIniResimTriggerPolicy = true;
				configuredResimTriggerPolicy = trimStartAndEnd(iniPolicy);
			}
		}

		bool    hasIniRedundancyFrames = false;
		int32_t configuredRedundancyFrames = 0;
		bool    hasIniRelayResendEntries = false;
		int32_t configuredRelayResendEntries = 0;
		if (GConfig != nullptr)
		{
			int32_t iniFrames = 0;
			if (GConfig->GetInt("OGNetcode", "RedundancyFrames", iniFrames, Ini::Game) ||
				GConfig->GetInt("OGNetcode", "RedundancyFrames", iniFrames, Ini::Engine))
			{
				hasIniRedundancyFrames = true;
				configuredRedundancyFrames = iniFrames;
			}
			int32_t iniEntries = 0;
			if (GConfig->GetInt("OGNetcode", "RelayResendEntries", iniEntries, Ini::Game) ||
				GConfig->GetInt("OGNetcode", "RelayResendEntries", iniEntries, Ini::Engine))
			{
				hasIniRelayResendEntries = true;
				configuredRelayResendEntries = iniEntries;
			}
		}

		bool    hasIniRecentreSettleSamples = false;
		int32_t configuredRecentreSettleSamples = 0;
		if (GConfig != nullptr)
		{
			int32_t iniSamples = 0;
			if (GConfig->GetInt("OGNetcode", "RecentreSettleSamples", iniSamples, Ini::Game) ||
				GConfig->GetInt("OGNetcode", "RecentreSettleSamples", iniSamples, Ini::Engine))
			{
				hasIniRecentreSettleSamples = true;
				configuredRecentreSettleSamples = iniSamples;
			}
		}

		bool        hasIniClockRttSource = false;
		std::string configuredClockRttSource;
		if (GConfig != nullptr)
		{
			std::string iniSource;
			if (GConfig->GetString("OGNetcode", "ClockRttSource", iniSource, Ini::Game) ||
				GConfig->GetString("OGNetcode", "ClockRttSource", iniSource, Ini::Engine))
			{
				hasIniClockRttSource = true;
				configuredClockRttSource = trimStartAndEnd(iniSource);
			}
		}

		bool   hasIniJitterMultiplier = false;
		double configuredJitterMultiplier = 0.0;
		if (GConfig != nullptr)
		{
			double iniMultiplier = 0.0;
			if (GConfig->GetDouble("OGNetcode", "JitterMultiplier", iniMultiplier, Ini::Game) ||
				GConfig->GetDouble("OGNetcode", "JitterMultiplier", iniMultiplier, Ini::Engine))
			{
				hasIniJitterMultiplier = true;
				configuredJitterMultiplier = iniMultiplier;
			}
		}

		bool        hasIniClockDilation = false;
		std::string configuredClockDilation;
		if (GConfig != nullptr)
		{
			std::string iniDilation;
			if (GConfig->GetString("OGNetcode", "ClockDilation", iniDilation, Ini::Game) ||
				GConfig->GetString("OGNetcode", "ClockDilation", iniDilation, Ini::Engine))
			{
				hasIniClockDilation = true;
				configuredClockDilation = trimStartAndEnd(iniDilation);
			}
		}

		bool        hasIniClockDilationGainPerTick = false;
		double      configuredClockDilationGainPerTick = 0.0;
		bool        hasIniClockDilationMaxPercent = false;
		double      configuredClockDilationMaxPercent = 0.0;
		bool        hasIniClockDilationTargetOffsetTicks = false;
		double      configuredClockDilationTargetOffsetTicks = 0.0;
		bool        hasIniClockDilationSetpoint = false;
		std::string configuredClockDilationSetpoint;
		bool        hasIniClockDilationCommonLeadTicks = false;
		double      configuredClockDilationCommonLeadTicks = 0.0;
		if (GConfig != nullptr)
		{
			hasIniClockDilationGainPerTick =
				GConfig->GetDouble("OGNetcode", "ClockDilationGainPerTick", configuredClockDilationGainPerTick, Ini::Game) ||
				GConfig->GetDouble("OGNetcode", "ClockDilationGainPerTick", configuredClockDilationGainPerTick, Ini::Engine);
			hasIniClockDilationMaxPercent =
				GConfig->GetDouble("OGNetcode", "ClockDilationMaxPercent", configuredClockDilationMaxPercent, Ini::Game) ||
				GConfig->GetDouble("OGNetcode", "ClockDilationMaxPercent", configuredClockDilationMaxPercent, Ini::Engine);
			hasIniClockDilationTargetOffsetTicks =
				GConfig->GetDouble("OGNetcode", "ClockDilationTargetOffsetTicks", configuredClockDilationTargetOffsetTicks,
					Ini::Game) ||
				GConfig->GetDouble("OGNetcode", "ClockDilationTargetOffsetTicks", configuredClockDilationTargetOffsetTicks,
					Ini::Engine);
			hasIniClockDilationCommonLeadTicks =
				GConfig->GetDouble("OGNetcode", "ClockDilationCommonLeadTicks", configuredClockDilationCommonLeadTicks,
					Ini::Game) ||
				GConfig->GetDouble("OGNetcode", "ClockDilationCommonLeadTicks", configuredClockDilationCommonLeadTicks,
					Ini::Engine);
			std::string iniSetpoint;
			if (GConfig->GetString("OGNetcode", "ClockDilationSetpoint", iniSetpoint, Ini::Game) ||
				GConfig->GetString("OGNetcode", "ClockDilationSetpoint", iniSetpoint, Ini::Engine))
			{
				hasIniClockDilationSetpoint = true;
				configuredClockDilationSetpoint = trimStartAndEnd(iniSetpoint);
			}
		}

		return SessionKnobs{ configuredRelayDelayFloorTicks, configuredCorrectionRotationK,
			hasIniResimTriggerPolicy, configuredResimTriggerPolicy,
			hasIniRedundancyFrames, configuredRedundancyFrames,
			hasIniRelayResendEntries, configuredRelayResendEntries,
			hasIniRecentreSettleSamples, configuredRecentreSettleSamples,
			hasIniClockRttSource, configuredClockRttSource,
			hasIniJitterMultiplier, configuredJitterMultiplier,
			hasIniClockDilation, configuredClockDilation,
			hasIniClockDilationGainPerTick, configuredClockDilationGainPerTick,
			hasIniClockDilationMaxPercent, configuredClockDilationMaxPercent,
			hasIniClockDilationTargetOffsetTicks, configuredClockDilationTargetOffsetTicks,
			hasIniClockDilationSetpoint, configuredClockDilationSetpoint,
			hasIniClockDilationCommonLeadTicks, configuredClockDilationCommonLeadTicks };
	}

	// SessionKnobs.h:256-300 at 0eb05d0
	void applySessionKnobs(const SessionKnobs& knobs)
	{
		if (knobs.configuredRelayDelayFloorTicks >= 0)
		{
			const int32_t clampedFloor = clampRelayDelayFloorTicks(
				knobs.configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
			if (clampedFloor != knobs.configuredRelayDelayFloorTicks)
			{
				ueLog(Level::Warning,
					"[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks",
					knobs.configuredRelayDelayFloorTicks, clampedFloor);
			}
			m_manager->setRelayDelayFloorTicks(clampedFloor);
			ueLog(Level::Log,
				"[RelayDelayFloor] session floor = %d ticks (ini override)", clampedFloor);

			logRelayDelayFloorAdvisory(clampedFloor);
		}

		if (knobs.configuredCorrectionRotationK != -1)
		{
			const int32_t clampedK = correctionRotation::clampK(knobs.configuredCorrectionRotationK);
			if (clampedK != knobs.configuredCorrectionRotationK)
			{
				ueLog(Level::Warning,
					"[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d",
					knobs.configuredCorrectionRotationK, clampedK);
			}
			m_manager->setCorrectionRotationK(clampedK);
		}

		const int32_t sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
		ueLog(Level::Warning,
			"[StateRotation] session K = %d (%s)",
			sessionCorrectionRotationK,
			knobs.configuredCorrectionRotationK != -1 ? "ini override" : "compiled default");

		const int32_t sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
		CHECK(m_simNetServer.has_value());
		m_simNetServer->publishFloor(static_cast<uint8_t>(sessionRelayDelayFloorTicks));
	}

	// SessionKnobs.h:302-337 at 0eb05d0
	void applyResimTriggerPolicy(SessionKnobs& knobs)
	{
		if (knobs.hasIniResimTriggerPolicy)
		{
			if (stricmpEquals(knobs.configuredResimTriggerPolicy, "OnDisagreement"))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
			}
			else if (stricmpEquals(knobs.configuredResimTriggerPolicy, "FrontierExact"))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
			}
			else
			{
				ueLog(Level::Warning,
					"[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised "
					"(expected FrontierExact or OnDisagreement) \xE2\x80\x94 keeping the compiled default",
					knobs.configuredResimTriggerPolicy.c_str());
				knobs.hasIniResimTriggerPolicy = false;
			}
		}

		const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
		const bool policyIsOnDisagreement =
			sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
		const std::string depthPolicyText = policyIsOnDisagreement
			? printfString("skip beyond %d ticks", sessionTimeConfig.rollbackWindowTicks)
			: std::string("off (inert under FrontierExact)");
		ueLog(Level::Warning,
			"[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)",
			policyIsOnDisagreement ? "OnDisagreement" : "FrontierExact",
			knobs.hasIniResimTriggerPolicy ? "ini override" : "compiled default",
			depthPolicyText.c_str());
	}

	// SessionKnobs.h:339-414 at 0eb05d0
	void applyLatencyKnobs(const SessionKnobs& knobs)
	{
		if (knobs.hasIniRedundancyFrames)
		{
			const int32_t clampedFrames = latencyKnobs::clampRedundancyFrames(knobs.configuredRedundancyFrames);
			if (clampedFrames != knobs.configuredRedundancyFrames)
			{
				ueLog(Level::Warning,
					"[LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d",
					knobs.configuredRedundancyFrames, clampedFrames);
			}
			m_manager->setRedundancyFrames(clampedFrames);
		}

		if (knobs.hasIniRelayResendEntries)
		{
			const int32_t clampedEntries = latencyKnobs::clampRelayResendEntries(knobs.configuredRelayResendEntries);
			if (clampedEntries != knobs.configuredRelayResendEntries)
			{
				ueLog(Level::Warning,
					"[LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d",
					knobs.configuredRelayResendEntries, clampedEntries);
			}
			m_manager->setRelayResendEntries(clampedEntries);
		}

		const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
		ueLog(Level::Warning,
			"[LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)",
			sessionTimeConfig.redundancyFrames,
			sessionTimeConfig.relayResendEntries,
			(knobs.hasIniRedundancyFrames || knobs.hasIniRelayResendEntries) ? "ini" : "compiled");

		if (knobs.hasIniRecentreSettleSamples)
		{
			const int32_t clampedSamples = std::max(knobs.configuredRecentreSettleSamples, 0);
			if (clampedSamples != knobs.configuredRecentreSettleSamples)
			{
				ueLog(Level::Warning,
					"[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d",
					knobs.configuredRecentreSettleSamples, clampedSamples);
			}
			m_manager->setRecentreSettleSamples(static_cast<uint32_t>(clampedSamples));
		}
		ueLog(Level::Warning,
			"[ClockRecentre.Config] recentreSettleSamples=%u (%s)",
			m_manager->getTimeConfig().recentreSettleSamples,
			knobs.hasIniRecentreSettleSamples ? "ini" : "compiled");

		bool jitterFromIni = false;
		if (knobs.hasIniJitterMultiplier)
		{
			if (!std::isfinite(knobs.configuredJitterMultiplier))
			{
				ueLog(Level::Warning,
					"[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults");
			}
			else
			{
				const double clampedMultiplier = clockJitter::clampJitterMultiplier(knobs.configuredJitterMultiplier);
				if (clampedMultiplier != knobs.configuredJitterMultiplier)
				{
					ueLog(Level::Warning,
						"[ClockSource.Config] ini [OGNetcode] JitterMultiplier=%.3f out of range, clamped to %.3f",
						knobs.configuredJitterMultiplier, clampedMultiplier);
				}
				m_manager->setJitterMultiplier(clampedMultiplier);
				jitterFromIni = true;
			}
		}

		applyClockRttSource(knobs, jitterFromIni);

		applyClockDilation(knobs);
	}

	// SessionKnobs.h:416-447 at 0eb05d0
	void applyClockRttSource(const SessionKnobs& knobs, bool jitterFromIni)
	{
		bool fromIni = false;
		if (knobs.hasIniClockRttSource)
		{
			if (stricmpEquals(knobs.configuredClockRttSource, "Engine"))
			{
				m_manager->setClockRttSource(TimeConfig::ClockRttSource::Engine);
				fromIni = true;
			}
			else if (stricmpEquals(knobs.configuredClockRttSource, "SimClock"))
			{
				m_manager->setClockRttSource(TimeConfig::ClockRttSource::SimClock);
				fromIni = true;
			}
			else
			{
				ueLog(Level::Warning,
					"[ClockSource.Config] ini [OGNetcode] ClockRttSource='%s' unrecognised (expected Engine or SimClock), keeping "
					"the compiled default",
					knobs.configuredClockRttSource.c_str());
			}
		}
		ueLog(Level::Warning,
			"[ClockSource.Config] clockRttSource=%s (%s) jitterMultiplier engine=%.3f simClock=%.3f (%s)",
			m_manager->getTimeConfig().clockRttSource == TimeConfig::ClockRttSource::SimClock ? "SimClock" : "Engine",
			fromIni ? "ini" : "compiled",
			m_manager->getTimeConfig().jitterMultiplier,
			m_manager->getTimeConfig().simClockJitterMultiplier,
			jitterFromIni ? "ini" : "compiled");
	}

	// SessionKnobs.h:449-534 at 0eb05d0
	void applyClockDilation(const SessionKnobs& knobs)
	{
		if (!m_manager->runsPrediction())
		{
			ueLog(Level::Warning, "[Dilation.Config] mode=n/a");
			return;
		}
		ClockDilationConfig config;
		bool fromIni = false;
		if (knobs.hasIniClockDilation)
		{
			if (stricmpEquals(knobs.configuredClockDilation, "Off"))
			{
				config.mode = ClockDilationMode::Off;
				fromIni = true;
			}
			else if (stricmpEquals(knobs.configuredClockDilation, "Shadow"))
			{
				config.mode = ClockDilationMode::Shadow;
				fromIni = true;
			}
			else if (stricmpEquals(knobs.configuredClockDilation, "On"))
			{
				config.mode = ClockDilationMode::On;
				fromIni = true;
			}
			else
			{
				ueLog(Level::Warning,
					"[Dilation.Config] ini [OGNetcode] ClockDilation='%s' unrecognised (expected Off, Shadow or On), running Off",
					knobs.configuredClockDilation.c_str());
			}
		}

		const bool gainFromIni = applyClockDilationDoor("ClockDilationGainPerTick", knobs.hasIniClockDilationGainPerTick,
			knobs.configuredClockDilationGainPerTick, &transcribedDoors::clampGainPerTick, config.gainPerTick);
		double maxPercent = config.maxDeviation * 100.0;
		const bool maxFromIni = applyClockDilationDoor("ClockDilationMaxPercent", knobs.hasIniClockDilationMaxPercent,
			knobs.configuredClockDilationMaxPercent, &transcribedDoors::clampMaxPercent, maxPercent);
		if (maxFromIni)
			config.maxDeviation = maxPercent / 100.0;
		const bool offsetFromIni = applyClockDilationDoor("ClockDilationTargetOffsetTicks",
			knobs.hasIniClockDilationTargetOffsetTicks, knobs.configuredClockDilationTargetOffsetTicks,
			&transcribedDoors::clampTargetOffsetTicks, config.targetOffsetTicks);
		const bool commonLeadFromIni = applyClockDilationDoor("ClockDilationCommonLeadTicks",
			knobs.hasIniClockDilationCommonLeadTicks, knobs.configuredClockDilationCommonLeadTicks,
			&transcribedDoors::clampCommonLeadTicks, config.commonLeadTicks);
		bool setpointFromIni = false;
		if (knobs.hasIniClockDilationSetpoint)
		{
			if (stricmpEquals(knobs.configuredClockDilationSetpoint, "OwnRtt"))
			{
				config.setpoint = ClockDilationSetpoint::OwnRtt;
				setpointFromIni = true;
			}
			else if (stricmpEquals(knobs.configuredClockDilationSetpoint, "Common"))
			{
				config.setpoint = ClockDilationSetpoint::Common;
				setpointFromIni = true;
			}
			else
			{
				ueLog(Level::Warning,
					"[Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='%s' unrecognised (expected OwnRtt or Common), "
					"running %s",
					knobs.configuredClockDilationSetpoint.c_str(), clockDilation::setpointWord(config.setpoint));
			}
		}

		m_clockDilation.emplace(config);
		m_clockDilationWindow.emplace(config);
		const ClockDilationConfig& stored = m_clockDilation->config();
		ueLog(Level::Warning,
			"[Dilation.Config] mode=%s setpoint=%s gainPerTick=%.4f maxDeviation=%.4f (%s)",
			clockDilation::modeWord(stored.mode), clockDilation::setpointWord(stored.setpoint), stored.gainPerTick,
			stored.maxDeviation, fromIni ? "ini" : "compiled");
		ueLog(Level::Warning,
			"[Dilation.Config] doors gainPerTick=%.4f (%s) maxPercent=%.3f (%s) targetOffsetTicks=%.3f (%s) setpoint=%s (%s) "
			"commonLeadTicks=%.3f (%s)",
			stored.gainPerTick, gainFromIni ? "ini" : "compiled",
			stored.maxDeviation * 100.0, maxFromIni ? "ini" : "compiled",
			stored.targetOffsetTicks, offsetFromIni ? "ini" : "compiled",
			clockDilation::setpointWord(stored.setpoint), setpointFromIni ? "ini" : "compiled",
			stored.commonLeadTicks, commonLeadFromIni ? "ini" : "compiled");
	}

	// SessionKnobs.h:536-556 at 0eb05d0
	bool applyClockDilationDoor(const char* key, bool hasIni, double requested, double (*clampToDoor)(double), double& value)
	{
		if (!hasIni)
			return false;
		if (!std::isfinite(requested))
		{
			ueLog(Level::Warning,
				"[Dilation.Config] ini [OGNetcode] %s is not a finite number, keeping the compiled default", key);
			return false;
		}
		const double clamped = clampToDoor(requested);
		if (clamped != requested)
		{
			ueLog(Level::Warning,
				"[Dilation.Config] ini [OGNetcode] %s=%.4f out of range, clamped to %.4f", key, requested, clamped);
		}
		value = clamped;
		return true;
	}
};

struct RecordingSink
{
	std::vector<std::string>* events;

	void logRouted(const char* line)
	{
		const std::string text(line);
		if (text.rfind("[Warning]", 0) == 0)
			events->push_back("line Warning " + text.substr(9));
		else if (text.rfind("[Verbose]", 0) == 0)
			events->push_back("line Verbose " + text.substr(9));
		else
			events->push_back("line Log " + text);
	}
};

static_assert(ogsession::SessionLogSink<RecordingSink>, "the test's sink models SessionLogSink");

std::string hexOf(double value)
{
	return printfString("%a", value);
}

template <typename KnobsT>
std::string knobsText(const KnobsT& k)
{
	return printfString("floor=%d K=%d policy=%d'%s' redundancy=%d,%d resend=%d,%d recentre=%d,%d source=%d'%s' jitter=%d,%s "
		"dilation=%d'%s' gain=%d,%s max=%d,%s offset=%d,%s setpoint=%d'%s' lead=%d,%s",
		k.configuredRelayDelayFloorTicks, k.configuredCorrectionRotationK,
		k.hasIniResimTriggerPolicy ? 1 : 0, k.configuredResimTriggerPolicy.c_str(),
		k.hasIniRedundancyFrames ? 1 : 0, k.configuredRedundancyFrames,
		k.hasIniRelayResendEntries ? 1 : 0, k.configuredRelayResendEntries,
		k.hasIniRecentreSettleSamples ? 1 : 0, k.configuredRecentreSettleSamples,
		k.hasIniClockRttSource ? 1 : 0, k.configuredClockRttSource.c_str(),
		k.hasIniJitterMultiplier ? 1 : 0, hexOf(k.configuredJitterMultiplier).c_str(),
		k.hasIniClockDilation ? 1 : 0, k.configuredClockDilation.c_str(),
		k.hasIniClockDilationGainPerTick ? 1 : 0, hexOf(k.configuredClockDilationGainPerTick).c_str(),
		k.hasIniClockDilationMaxPercent ? 1 : 0, hexOf(k.configuredClockDilationMaxPercent).c_str(),
		k.hasIniClockDilationTargetOffsetTicks ? 1 : 0, hexOf(k.configuredClockDilationTargetOffsetTicks).c_str(),
		k.hasIniClockDilationSetpoint ? 1 : 0, k.configuredClockDilationSetpoint.c_str(),
		k.hasIniClockDilationCommonLeadTicks ? 1 : 0, hexOf(k.configuredClockDilationCommonLeadTicks).c_str());
}

std::string managerText(const FakeKnobManager& manager, const std::optional<ClockDilationController>& dilation,
	const std::optional<ClockDilationWindow>& window)
{
	const TimeConfig& c = manager.getTimeConfig();
	std::string text = printfString("floor=%d K=%d policy=%d redundancy=%d resend=%d recentre=%u source=%d jitter=%s/%s",
		c.relayDelayFloorTicks, c.correctionRotationK, static_cast<int>(c.resimTriggerPolicy), c.redundancyFrames,
		c.relayResendEntries, c.recentreSettleSamples, static_cast<int>(c.clockRttSource), hexOf(c.jitterMultiplier).c_str(),
		hexOf(c.simClockJitterMultiplier).c_str());
	if (dilation.has_value())
	{
		const ClockDilationConfig& d = dilation->config();
		text += printfString(" dilation mode=%d setpoint=%d gain=%s max=%s slew=%s quantum=%s hysteresis=%s targetTau=%s "
			"guardTau=%s guard=%s/%s freeze=%s lead=%s offset=%s",
			static_cast<int>(d.mode), static_cast<int>(d.setpoint), hexOf(d.gainPerTick).c_str(), hexOf(d.maxDeviation).c_str(),
			hexOf(d.slewPerCall).c_str(), hexOf(d.quantum).c_str(), hexOf(d.changeHysteresisQuanta).c_str(),
			hexOf(d.targetTauSeconds).c_str(), hexOf(d.guardTauSeconds).c_str(), hexOf(d.guardLowTicks).c_str(),
			hexOf(d.guardHighTicks).c_str(), hexOf(d.freezeAfterResyncSeconds).c_str(), hexOf(d.commonLeadTicks).c_str(),
			hexOf(d.targetOffsetTicks).c_str());
	}
	else
	{
		text += " dilation none";
	}
	text += window.has_value() ? " window" : " no window";
	return text;
}

struct Outcome
{
	std::vector<std::string> events;
	std::string              knobsRead;
	std::string              knobsAfter;
	std::string              manager;
};

Outcome runTranscription(const FakeGConfig* gconfig, bool worldIsAuthority)
{
	TranscribedSession session;
	session.GConfig = gconfig;
	auto sessionKnobs = session.readSessionKnobs(worldIsAuthority);
	Outcome outcome;
	outcome.knobsRead = knobsText(sessionKnobs);
	session.m_manager.emplace(!worldIsAuthority);
	if (worldIsAuthority)
	{
		session.m_simNetServer.emplace(FakeFloorServer{ &session.events });
		session.applySessionKnobs(sessionKnobs);
	}
	session.applyResimTriggerPolicy(sessionKnobs);
	session.applyLatencyKnobs(sessionKnobs);
	outcome.knobsAfter = knobsText(sessionKnobs);
	outcome.events = session.events;
	outcome.manager = managerText(*session.m_manager, session.m_clockDilation, session.m_clockDilationWindow);
	return outcome;
}

Outcome runEngineFree(const FakeGConfig* gconfig, bool worldIsAuthority)
{
	std::vector<std::string> events;
	RecordingSink sink{ &events };
	ogsession::SessionKnobs sessionKnobs = ogsession::readSessionKnobs(FakeUEConfigSource(gconfig), worldIsAuthority);
	Outcome outcome;
	outcome.knobsRead = knobsText(sessionKnobs);
	FakeKnobManager manager(!worldIsAuthority);
	std::optional<FakeFloorServer> server;
	std::optional<ClockDilationController> dilation;
	std::optional<ClockDilationWindow> window;
	if (worldIsAuthority)
	{
		server.emplace(FakeFloorServer{ &events });
		ogsession::applySessionKnobs(sink, manager, &*server, sessionKnobs,
			[&events](int32_t floorTicks) { events.push_back("advisory " + std::to_string(floorTicks)); });
	}
	ogsession::applyResimTriggerPolicy(sink, manager, sessionKnobs);
	ogsession::applyLatencyKnobs(sink, manager, sessionKnobs, dilation, window);
	outcome.knobsAfter = knobsText(sessionKnobs);
	outcome.events = events;
	outcome.manager = managerText(manager, dilation, window);
	return outcome;
}

struct KeyCases
{
	const char*              key;
	std::vector<std::string> values;
};

std::vector<KeyCases> keyCases()
{
	const std::vector<std::string> ints = { "0", "1", "2", "3", "6", "7", "8", "9", "13", "99", "-1", "-2", "-5", "2147483647",
		"99999999999", "nan", "inf", "Bogus", "", " 4 ", "4abc", "0x10" };
	const std::vector<std::string> doubles = { "0", "0.004", "0.005", "0.0123456", "0.05", "0.06", "1", "1.5", "2", "2.5",
		"3.65", "9.99", "10", "10.5", "-0.5", "-1", "-2", "-2.5", "-0", "1e308", "-1e308", "nan", "-nan", "inf", "-inf",
		"Bogus", "", " 1.25 " };
	const std::vector<std::string> words = { "Bogus", "", " ", "nan", "O\xC3\xB1", "\xC5\x8Cn" };
	auto with = [&words](std::vector<std::string> own)
	{
		own.insert(own.end(), words.begin(), words.end());
		return own;
	};
	return {
		{ "RelayDelayFloorTicks", ints },
		{ "CorrectionRotationK", ints },
		{ "ResimTriggerPolicy", with({ "OnDisagreement", "FrontierExact", "ondisagreement", "FRONTIEREXACT", "  OnDisagreement  ",
			"\tFrontierExact\t", "OnDisagreement2", "Frontier Exact" }) },
		{ "RedundancyFrames", ints },
		{ "RelayResendEntries", ints },
		{ "RecentreSettleSamples", ints },
		{ "ClockRttSource", with({ "Engine", "SimClock", "engine", "SIMCLOCK", " SimClock ", "Sim Clock", "EnginePing" }) },
		{ "JitterMultiplier", doubles },
		{ "ClockDilation", with({ "Off", "Shadow", "On", "off", "SHADOW", "oN", " On ", "Onn", "Of" }) },
		{ "ClockDilationGainPerTick", doubles },
		{ "ClockDilationMaxPercent", doubles },
		{ "ClockDilationTargetOffsetTicks", doubles },
		{ "ClockDilationSetpoint", with({ "OwnRtt", "Common", "ownrtt", "COMMON", " Common ", "Own Rtt", "Commons" }) },
		{ "ClockDilationCommonLeadTicks", doubles },
	};
}

std::string joined(const std::vector<std::string>& lines)
{
	std::string text;
	for (const std::string& line : lines)
		text += line + "\n";
	return text;
}

struct Survey
{
	uint32_t scenarios = 0u;
	uint32_t comparedEvents = 0u;
	std::map<std::string, uint32_t> changedPerKey;
	std::map<std::string, uint32_t> clampLinesPerKey;
	std::map<std::string, uint32_t> notFiniteLinesPerKey;
	std::map<std::string, uint32_t> unrecognisedLinesPerKey;
	std::map<std::string, uint32_t> managerChangedPerKey;
};

uint32_t countContaining(const std::vector<std::string>& events, const char* needle)
{
	return static_cast<uint32_t>(std::count_if(events.begin(), events.end(),
		[needle](const std::string& event) { return event.find(needle) != std::string::npos; }));
}

void compareScenario(const FakeGConfig* gconfig, bool authority, const std::string& name, Survey& survey,
	const Outcome* baseline, const char* key)
{
	const Outcome expected = runTranscription(gconfig, authority);
	const Outcome actual = runEngineFree(gconfig, authority);
	INFO(name << (authority ? " (authority)" : " (prediction)"));
	CHECK(joined(actual.events) == joined(expected.events));
	CHECK(actual.knobsRead == expected.knobsRead);
	CHECK(actual.knobsAfter == expected.knobsAfter);
	CHECK(actual.manager == expected.manager);
	++survey.scenarios;
	survey.comparedEvents += static_cast<uint32_t>(expected.events.size());
	if (baseline != nullptr && key != nullptr)
	{
		if (expected.events != baseline->events)
			++survey.changedPerKey[key];
		if (expected.manager != baseline->manager)
			++survey.managerChangedPerKey[key];
		survey.clampLinesPerKey[key] += countContaining(expected.events, "out of range");
		survey.notFiniteLinesPerKey[key] += countContaining(expected.events, "not a finite number");
		survey.unrecognisedLinesPerKey[key] += countContaining(expected.events, "unrecognised");
	}
}

Survey runAllScenarios()
{
	Survey survey;
	const FakeGConfig empty;
	const Outcome baselineAuthority = runTranscription(&empty, true);
	const Outcome baselinePrediction = runTranscription(&empty, false);

	for (const bool authority : { true, false })
	{
		compareScenario(nullptr, authority, "no GConfig", survey, nullptr, nullptr);
		compareScenario(&empty, authority, "every key absent", survey, nullptr, nullptr);

		FakeGConfig shipped;
		shipped.engine = { { "RelayDelayFloorTicks", "6" }, { "ResimTriggerPolicy", "OnDisagreement" },
			{ "RedundancyFrames", "3" }, { "RelayResendEntries", "2" }, { "ClockDilation", "On" },
			{ "ClockDilationSetpoint", "Common" } };
		compareScenario(&shipped, authority, "the tree's Config/DefaultEngine.ini [OGNetcode]", survey, nullptr, nullptr);

		FakeGConfig everyKey;
		everyKey.game = { { "RelayDelayFloorTicks", "4" }, { "CorrectionRotationK", "3" }, { "ResimTriggerPolicy", "frontierexact" },
			{ "RedundancyFrames", "12" }, { "RelayResendEntries", "-1" }, { "RecentreSettleSamples", "-3" },
			{ "ClockRttSource", "Engine" }, { "JitterMultiplier", "5" }, { "ClockDilation", "Shadow" },
			{ "ClockDilationGainPerTick", "0.07" }, { "ClockDilationMaxPercent", "nan" },
			{ "ClockDilationTargetOffsetTicks", "-3" }, { "ClockDilationSetpoint", "Bogus" },
			{ "ClockDilationCommonLeadTicks", "11" } };
		compareScenario(&everyKey, authority, "every key at once", survey, nullptr, nullptr);

		for (const KeyCases& cases : keyCases())
		{
			for (const std::string& value : cases.values)
			{
				const Outcome& baseline = authority ? baselineAuthority : baselinePrediction;

				FakeGConfig inGame;
				inGame.game[cases.key] = value;
				compareScenario(&inGame, authority, std::string(cases.key) + "='" + value + "' in the game ini", survey,
					&baseline, cases.key);

				FakeGConfig inEngine;
				inEngine.engine[cases.key] = value;
				compareScenario(&inEngine, authority, std::string(cases.key) + "='" + value + "' in the engine ini", survey,
					&baseline, cases.key);

				FakeGConfig inBoth;
				inBoth.game[cases.key] = value;
				inBoth.engine[cases.key] = "7";
				compareScenario(&inBoth, authority, std::string(cases.key) + "='" + value + "' in the game ini over the engine's 7",
					survey, &baseline, cases.key);
			}
		}
	}
	return survey;
}
} // namespace

TEST_CASE("SessionKnobs.EveryKeyGivesTheTranscribedLinesAndValues", "[Session][SessionKnobs]")
{
	const Survey survey = runAllScenarios();

	std::size_t values = 0u;
	for (const KeyCases& cases : keyCases())
		values += cases.values.size();
	CHECK(keyCases().size() == 14u);
	CHECK(values == 305u);
	CHECK(survey.scenarios == 2u * (4u + 3u * values));
	CHECK(survey.comparedEvents > 10000u);

	const std::vector<std::string> clampedKeys = { "RelayDelayFloorTicks", "CorrectionRotationK", "RedundancyFrames",
		"RelayResendEntries", "RecentreSettleSamples", "JitterMultiplier", "ClockDilationGainPerTick", "ClockDilationMaxPercent",
		"ClockDilationTargetOffsetTicks", "ClockDilationCommonLeadTicks" };
	const std::vector<std::string> finiteCheckedKeys = { "JitterMultiplier", "ClockDilationGainPerTick",
		"ClockDilationMaxPercent", "ClockDilationTargetOffsetTicks", "ClockDilationCommonLeadTicks" };
	const std::vector<std::string> wordKeys = { "ResimTriggerPolicy", "ClockRttSource", "ClockDilation", "ClockDilationSetpoint" };
	for (const KeyCases& cases : keyCases())
	{
		INFO(cases.key);
		CHECK(survey.changedPerKey.count(cases.key) == 1u);
		CHECK(survey.managerChangedPerKey.count(cases.key) == 1u);
	}
	for (const std::string& key : clampedKeys)
	{
		INFO(key);
		CHECK(survey.clampLinesPerKey.at(key) > 0u);
	}
	for (const std::string& key : finiteCheckedKeys)
	{
		INFO(key);
		CHECK(survey.notFiniteLinesPerKey.at(key) > 0u);
	}
	for (const std::string& key : wordKeys)
	{
		INFO(key);
		CHECK(survey.unrecognisedLinesPerKey.at(key) > 0u);
	}
}

TEST_CASE("SessionKnobs.Survey", "[.][SessionKnobsSurvey]")
{
	const Survey survey = runAllScenarios();
	std::printf("[SessionKnobsSurvey] scenarios=%u comparedEvents=%u\n", survey.scenarios, survey.comparedEvents);
	for (const KeyCases& cases : keyCases())
	{
		const std::string key = cases.key;
		auto at = [&key](const std::map<std::string, uint32_t>& counts)
		{
			const auto found = counts.find(key);
			return found != counts.end() ? found->second : 0u;
		};
		std::printf("[SessionKnobsSurvey] %-31s values=%2zu linesChanged=%3u managerChanged=%3u clampLines=%3u notFinite=%3u "
			"unrecognised=%3u\n",
			key.c_str(), cases.values.size(), at(survey.changedPerKey), at(survey.managerChangedPerKey),
			at(survey.clampLinesPerKey), at(survey.notFiniteLinesPerKey), at(survey.unrecognisedLinesPerKey));
	}
	SUCCEED();
}

TEST_CASE("SessionKnobs.WithNoKeyTheAuthorityProvesKAndPublishesTheCompiledFloor", "[Session][SessionKnobs]")
{
	std::vector<std::string> events;
	RecordingSink sink{ &events };
	FakeKnobManager manager(false);
	const FakeGConfig empty;
	const ogsession::SessionKnobs knobs = ogsession::readSessionKnobs(FakeUEConfigSource(&empty), true);
	FakeFloorServer server{ &events };
	ogsession::applySessionKnobs(sink, manager, &server, knobs, [](int32_t) {});
	REQUIRE(events.size() == 2u);
	CHECK(events[0] == "line Warning [StateRotation] session K = 1 (compiled default)");
	CHECK(events[1] == "publishFloor 0");
}
