#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionKnobs-rationale.md · docs/SessionKnobs-guards.md

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

#include "OGSimulation/OGAssert.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/CorrectionRotation.h"
#include "OGSimulation/PCTimeManagement/ClockDilationController.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimSession/SessionEngine.h"

namespace ogsession
{
namespace clockDilationDoors
{
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
} // namespace clockDilationDoors

static_assert(clockDilationDoors::kMaxMaxPercent / 100.0 < 0.2,
	"[OGNetcode] ClockDilationMaxPercent's ceiling must keep maxDeviation under the render bracket's bound, 0.2, for every "
	"value the door accepts: the runtime form of ClockDilationConfig's static_assert maxDeviation < 0.2, which checks the "
	"compiled default only");
static_assert(clockDilationDoors::kMinGainPerTick >= 0.0 && clockDilationDoors::kMinMaxPercent >= 0.0,
	"The ClockDilationGainPerTick and ClockDilationMaxPercent doors must not admit a negative value: a negative gain turns the "
	"rate law into positive feedback, and a negative deviation inverts its clamp");
static_assert(clockDilationDoors::clampGainPerTick(ClockDilationConfig{}.gainPerTick) == ClockDilationConfig{}.gainPerTick
	&& clockDilationDoors::clampMaxPercent(ClockDilationConfig{}.maxDeviation * 100.0) == ClockDilationConfig{}.maxDeviation * 100.0
	&& clockDilationDoors::clampTargetOffsetTicks(ClockDilationConfig{}.targetOffsetTicks) == ClockDilationConfig{}.targetOffsetTicks
	&& clockDilationDoors::clampCommonLeadTicks(ClockDilationConfig{}.commonLeadTicks) == ClockDilationConfig{}.commonLeadTicks,
	"ClockDilationConfig's compiled gainPerTick, maxDeviation, targetOffsetTicks and commonLeadTicks must lie inside their "
	"[OGNetcode] doors' clamps, or writing the compiled value into the ini would move it");
static_assert(clockDilationDoors::kMinCommonLeadTicks >= 0.0,
	"The ClockDilationCommonLeadTicks door must not admit a negative lead: a Common target behind the server's tick would make "
	"every client's inputs late by construction");

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

namespace sessionKnobsLines
{
inline constexpr char kRelayDelayFloorClamped[] =
	"[Warning][RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks";
inline constexpr char kRelayDelayFloorSession[] = "[RelayDelayFloor] session floor = %d ticks (ini override)";
inline constexpr char kStateRotationClamped[] =
	"[Warning][StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d";
inline constexpr char kStateRotationSession[] = "[Warning][StateRotation] session K = %d (%s)";
inline constexpr char kResimGateUnrecognised[] =
	"[Warning][ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised "
	"(expected FrontierExact or OnDisagreement) \xE2\x80\x94 keeping the compiled default";
inline constexpr char kResimGateDepthSkip[] = "skip beyond %d ticks";
inline constexpr char kResimGateSession[] =
	"[Warning][ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, "
	"one pending)";
inline constexpr char kLatencyKnobsRedundancyClamped[] =
	"[Warning][LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d";
inline constexpr char kLatencyKnobsResendClamped[] =
	"[Warning][LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d";
inline constexpr char kLatencyKnobsSession[] = "[Warning][LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)";
inline constexpr char kRecentreClamped[] =
	"[Warning][ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d";
inline constexpr char kRecentreSession[] = "[Warning][ClockRecentre.Config] recentreSettleSamples=%u (%s)";
inline constexpr char kJitterNotFinite[] =
	"[Warning][ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults";
inline constexpr char kJitterClamped[] =
	"[Warning][ClockSource.Config] ini [OGNetcode] JitterMultiplier=%.3f out of range, clamped to %.3f";
inline constexpr char kClockRttSourceUnrecognised[] =
	"[Warning][ClockSource.Config] ini [OGNetcode] ClockRttSource='%s' unrecognised (expected Engine or SimClock), keeping "
	"the compiled default";
inline constexpr char kClockSourceSession[] =
	"[Warning][ClockSource.Config] clockRttSource=%s (%s) jitterMultiplier engine=%.3f simClock=%.3f (%s)";
inline constexpr char kDilationNotApplicable[] = "[Warning][Dilation.Config] mode=n/a";
inline constexpr char kDilationUnrecognised[] =
	"[Warning][Dilation.Config] ini [OGNetcode] ClockDilation='%s' unrecognised (expected Off, Shadow or On), running Off";
inline constexpr char kDilationSetpointUnrecognised[] =
	"[Warning][Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='%s' unrecognised (expected OwnRtt or Common), "
	"running %s";
inline constexpr char kDilationSession[] =
	"[Warning][Dilation.Config] mode=%s setpoint=%s gainPerTick=%.4f maxDeviation=%.4f (%s)";
inline constexpr char kDilationDoors[] =
	"[Warning][Dilation.Config] doors gainPerTick=%.4f (%s) maxPercent=%.3f (%s) targetOffsetTicks=%.3f (%s) setpoint=%s (%s) "
	"commonLeadTicks=%.3f (%s)";
inline constexpr char kDilationDoorNotFinite[] =
	"[Warning][Dilation.Config] ini [OGNetcode] %s is not a finite number, keeping the compiled default";
inline constexpr char kDilationDoorClamped[] =
	"[Warning][Dilation.Config] ini [OGNetcode] %s=%.4f out of range, clamped to %.4f";

template <typename... ArgsT>
std::string formatLine(const char* pattern, ArgsT... args)
{
	if constexpr (sizeof...(ArgsT) == 0)
	{
		return std::string(pattern);
	}
	else
	{
		const int length = std::snprintf(nullptr, 0, pattern, args...);
		OG_CHECK(length >= 0, "sessionKnobsLines::formatLine: snprintf refused a session-knob line's format and arguments");
		std::string line(static_cast<std::size_t>(length), '\0');
		std::snprintf(line.data(), line.size() + 1u, pattern, args...);
		return line;
	}
}

template <SessionLogSink EngineT, typename... ArgsT>
void logLine(EngineT& engine, const char* pattern, ArgsT... args)
{
	const std::string line = formatLine(pattern, args...);
	engine.logRouted(line.c_str());
}

constexpr bool equalsIgnoringCase(std::string_view text, std::string_view word)
{
	if (text.size() != word.size())
		return false;
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		const char a = (text[i] >= 'A' && text[i] <= 'Z') ? static_cast<char>(text[i] - 'A' + 'a') : text[i];
		const char b = (word[i] >= 'A' && word[i] <= 'Z') ? static_cast<char>(word[i] - 'A' + 'a') : word[i];
		if (a != b)
			return false;
	}
	return true;
}

static_assert(equalsIgnoringCase("ONdisagreement", "OnDisagreement") && !equalsIgnoringCase("OnDisagreement ", "OnDisagreement")
	&& !equalsIgnoringCase("0n", "On") && !equalsIgnoringCase("O\xC3\xB1", "On"),
	"equalsIgnoringCase folds ASCII letters only, as FString::Equals(ESearchCase::IgnoreCase) does (FGenericPlatformStricmp: "
	"two characters that are not both ASCII must be equal); was the UE session's FString::Equals");
} // namespace sessionKnobsLines

template <ConfigSource ConfigT>
SessionKnobs readSessionKnobs(const ConfigT& config, bool worldIsAuthority)
{
	int32_t configuredRelayDelayFloorTicks = -1;
	// ⛔G-01  docs/SessionKnobs-guards.md
	if (worldIsAuthority)
	{
		if (const std::optional<int32_t> iniFloorTicks = config.readInt("RelayDelayFloorTicks"))
			configuredRelayDelayFloorTicks = *iniFloorTicks;
	}

	// ⛔G-02  docs/SessionKnobs-guards.md

	int32_t configuredCorrectionRotationK = -1;
	if (worldIsAuthority)
	{
		if (const std::optional<int32_t> iniK = config.readInt("CorrectionRotationK"))
			configuredCorrectionRotationK = *iniK;
	}

	bool        hasIniResimTriggerPolicy = false;
	std::string configuredResimTriggerPolicy;
	// ⛔G-03  docs/SessionKnobs-guards.md
	if (std::optional<std::string> iniPolicy = config.readString("ResimTriggerPolicy"))
	{
		hasIniResimTriggerPolicy = true;
		configuredResimTriggerPolicy = std::move(*iniPolicy);
	}

	bool    hasIniRedundancyFrames = false;
	int32_t configuredRedundancyFrames = 0;
	bool    hasIniRelayResendEntries = false;
	int32_t configuredRelayResendEntries = 0;
	// ⛔G-04  docs/SessionKnobs-guards.md
	if (const std::optional<int32_t> iniFrames = config.readInt("RedundancyFrames"))
	{
		hasIniRedundancyFrames = true;
		configuredRedundancyFrames = *iniFrames;
	}
	if (const std::optional<int32_t> iniEntries = config.readInt("RelayResendEntries"))
	{
		hasIniRelayResendEntries = true;
		configuredRelayResendEntries = *iniEntries;
	}

	bool    hasIniRecentreSettleSamples = false;
	int32_t configuredRecentreSettleSamples = 0;
	if (const std::optional<int32_t> iniSamples = config.readInt("RecentreSettleSamples"))
	{
		hasIniRecentreSettleSamples = true;
		configuredRecentreSettleSamples = *iniSamples;
	}

	bool        hasIniClockRttSource = false;
	std::string configuredClockRttSource;
	if (std::optional<std::string> iniSource = config.readString("ClockRttSource"))
	{
		hasIniClockRttSource = true;
		configuredClockRttSource = std::move(*iniSource);
	}

	bool   hasIniJitterMultiplier = false;
	double configuredJitterMultiplier = 0.0;
	if (const std::optional<double> iniMultiplier = config.readDouble("JitterMultiplier"))
	{
		hasIniJitterMultiplier = true;
		configuredJitterMultiplier = *iniMultiplier;
	}

	bool        hasIniClockDilation = false;
	std::string configuredClockDilation;
	if (std::optional<std::string> iniDilation = config.readString("ClockDilation"))
	{
		hasIniClockDilation = true;
		configuredClockDilation = std::move(*iniDilation);
	}

	const std::optional<double> iniGainPerTick = config.readDouble("ClockDilationGainPerTick");
	const std::optional<double> iniMaxPercent = config.readDouble("ClockDilationMaxPercent");
	const std::optional<double> iniTargetOffsetTicks = config.readDouble("ClockDilationTargetOffsetTicks");
	const std::optional<double> iniCommonLeadTicks = config.readDouble("ClockDilationCommonLeadTicks");
	std::optional<std::string> iniSetpoint = config.readString("ClockDilationSetpoint");

	return SessionKnobs{ configuredRelayDelayFloorTicks, configuredCorrectionRotationK,
		hasIniResimTriggerPolicy, std::move(configuredResimTriggerPolicy),
		hasIniRedundancyFrames, configuredRedundancyFrames,
		hasIniRelayResendEntries, configuredRelayResendEntries,
		hasIniRecentreSettleSamples, configuredRecentreSettleSamples,
		hasIniClockRttSource, std::move(configuredClockRttSource),
		hasIniJitterMultiplier, configuredJitterMultiplier,
		hasIniClockDilation, std::move(configuredClockDilation),
		iniGainPerTick.has_value(), iniGainPerTick.value_or(0.0),
		iniMaxPercent.has_value(), iniMaxPercent.value_or(0.0),
		iniTargetOffsetTicks.has_value(), iniTargetOffsetTicks.value_or(0.0),
		iniSetpoint.has_value(), iniSetpoint.has_value() ? std::move(*iniSetpoint) : std::string(),
		iniCommonLeadTicks.has_value(), iniCommonLeadTicks.value_or(0.0) };
}

template <SessionLogSink EngineT, typename ManagerT, typename FloorServerT, typename FloorAdvisoryT>
void applySessionKnobs(EngineT& engine, ManagerT& manager, FloorServerT* floorServer, const SessionKnobs& knobs,
	FloorAdvisoryT&& logRelayDelayFloorAdvisory)
{
	if (knobs.configuredRelayDelayFloorTicks >= 0)
	{
		const int32_t clampedFloor = clampRelayDelayFloorTicks(knobs.configuredRelayDelayFloorTicks, manager.getTimeConfig());
		if (clampedFloor != knobs.configuredRelayDelayFloorTicks)
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kRelayDelayFloorClamped, knobs.configuredRelayDelayFloorTicks,
				clampedFloor);
		}
		manager.setRelayDelayFloorTicks(clampedFloor);
		sessionKnobsLines::logLine(engine, sessionKnobsLines::kRelayDelayFloorSession, clampedFloor);

		logRelayDelayFloorAdvisory(clampedFloor);
	}

	if (knobs.configuredCorrectionRotationK != -1)
	{
		const int32_t clampedK = correctionRotation::clampK(knobs.configuredCorrectionRotationK);
		if (clampedK != knobs.configuredCorrectionRotationK)
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kStateRotationClamped, knobs.configuredCorrectionRotationK,
				clampedK);
		}
		manager.setCorrectionRotationK(clampedK);
	}

	const int32_t sessionCorrectionRotationK = manager.getTimeConfig().correctionRotationK;
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kStateRotationSession, sessionCorrectionRotationK,
		knobs.configuredCorrectionRotationK != -1 ? "ini override" : "compiled default");

	const int32_t sessionRelayDelayFloorTicks = manager.getTimeConfig().relayDelayFloorTicks;
	OG_CHECK(floorServer != nullptr,
		"ogsession::applySessionKnobs: the og-sim-net server publishes the session floor, so the session emplaces it before "
		"the knobs are applied. Was UESimulationSession::applySessionKnobs's checkf, and before it the timing relay's "
		"spawn-before-the-manager order (og-netcode-v2-input-relay T11)");
	floorServer->publishFloor(static_cast<uint8_t>(sessionRelayDelayFloorTicks));
}

template <SessionLogSink EngineT, typename ManagerT>
void applyResimTriggerPolicy(EngineT& engine, ManagerT& manager, SessionKnobs& knobs)
{
	// ⛔G-05  docs/SessionKnobs-guards.md
	if (knobs.hasIniResimTriggerPolicy)
	{
		if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredResimTriggerPolicy, "OnDisagreement"))
		{
			manager.setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
		}
		else if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredResimTriggerPolicy, "FrontierExact"))
		{
			manager.setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
		}
		else
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kResimGateUnrecognised,
				knobs.configuredResimTriggerPolicy.c_str());
			knobs.hasIniResimTriggerPolicy = false;
		}
	}

	const TimeConfig& sessionTimeConfig = manager.getTimeConfig();
	const bool policyIsOnDisagreement =
		sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
	const std::string depthPolicyText = policyIsOnDisagreement
		? sessionKnobsLines::formatLine(sessionKnobsLines::kResimGateDepthSkip, sessionTimeConfig.rollbackWindowTicks)
		: std::string("off (inert under FrontierExact)");
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kResimGateSession,
		policyIsOnDisagreement ? "OnDisagreement" : "FrontierExact",
		knobs.hasIniResimTriggerPolicy ? "ini override" : "compiled default",
		depthPolicyText.c_str());
}

template <SessionLogSink EngineT>
bool applyClockDilationDoor(EngineT& engine, const char* key, bool hasIni, double requested,
	double (*clampToDoor)(double), double& value)
{
	if (!hasIni)
		return false;
	if (!std::isfinite(requested))
	{
		sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationDoorNotFinite, key);
		return false;
	}
	const double clamped = clampToDoor(requested);
	if (clamped != requested)
		sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationDoorClamped, key, requested, clamped);
	value = clamped;
	return true;
}

template <SessionLogSink EngineT, typename ManagerT>
void applyClockRttSource(EngineT& engine, ManagerT& manager, const SessionKnobs& knobs, bool jitterFromIni)
{
	bool fromIni = false;
	if (knobs.hasIniClockRttSource)
	{
		if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockRttSource, "Engine"))
		{
			manager.setClockRttSource(TimeConfig::ClockRttSource::Engine);
			fromIni = true;
		}
		else if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockRttSource, "SimClock"))
		{
			manager.setClockRttSource(TimeConfig::ClockRttSource::SimClock);
			fromIni = true;
		}
		else
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kClockRttSourceUnrecognised,
				knobs.configuredClockRttSource.c_str());
		}
	}
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kClockSourceSession,
		manager.getTimeConfig().clockRttSource == TimeConfig::ClockRttSource::SimClock ? "SimClock" : "Engine",
		fromIni ? "ini" : "compiled",
		manager.getTimeConfig().jitterMultiplier,
		manager.getTimeConfig().simClockJitterMultiplier,
		jitterFromIni ? "ini" : "compiled");
}

template <SessionLogSink EngineT, typename ManagerT>
void applyClockDilation(EngineT& engine, ManagerT& manager, const SessionKnobs& knobs,
	std::optional<ClockDilationController>& clockDilation, std::optional<ClockDilationWindow>& clockDilationWindow)
{
	if (!manager.runsPrediction())
	{
		sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationNotApplicable);
		return;
	}
	ClockDilationConfig config;
	bool fromIni = false;
	if (knobs.hasIniClockDilation)
	{
		if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockDilation, "Off"))
		{
			config.mode = ClockDilationMode::Off;
			fromIni = true;
		}
		else if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockDilation, "Shadow"))
		{
			config.mode = ClockDilationMode::Shadow;
			fromIni = true;
		}
		else if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockDilation, "On"))
		{
			config.mode = ClockDilationMode::On;
			fromIni = true;
		}
		else
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationUnrecognised, knobs.configuredClockDilation.c_str());
		}
	}

	const bool gainFromIni = applyClockDilationDoor(engine, "ClockDilationGainPerTick", knobs.hasIniClockDilationGainPerTick,
		knobs.configuredClockDilationGainPerTick, &clockDilationDoors::clampGainPerTick, config.gainPerTick);
	double maxPercent = config.maxDeviation * 100.0;
	const bool maxFromIni = applyClockDilationDoor(engine, "ClockDilationMaxPercent", knobs.hasIniClockDilationMaxPercent,
		knobs.configuredClockDilationMaxPercent, &clockDilationDoors::clampMaxPercent, maxPercent);
	if (maxFromIni)
		config.maxDeviation = maxPercent / 100.0;
	const bool offsetFromIni = applyClockDilationDoor(engine, "ClockDilationTargetOffsetTicks",
		knobs.hasIniClockDilationTargetOffsetTicks, knobs.configuredClockDilationTargetOffsetTicks,
		&clockDilationDoors::clampTargetOffsetTicks, config.targetOffsetTicks);
	const bool commonLeadFromIni = applyClockDilationDoor(engine, "ClockDilationCommonLeadTicks",
		knobs.hasIniClockDilationCommonLeadTicks, knobs.configuredClockDilationCommonLeadTicks,
		&clockDilationDoors::clampCommonLeadTicks, config.commonLeadTicks);
	bool setpointFromIni = false;
	if (knobs.hasIniClockDilationSetpoint)
	{
		if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockDilationSetpoint, "OwnRtt"))
		{
			config.setpoint = ClockDilationSetpoint::OwnRtt;
			setpointFromIni = true;
		}
		else if (sessionKnobsLines::equalsIgnoringCase(knobs.configuredClockDilationSetpoint, "Common"))
		{
			config.setpoint = ClockDilationSetpoint::Common;
			setpointFromIni = true;
		}
		else
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationSetpointUnrecognised,
				knobs.configuredClockDilationSetpoint.c_str(), clockDilation::setpointWord(config.setpoint));
		}
	}

	clockDilation.emplace(config);
	clockDilationWindow.emplace(config);
	const ClockDilationConfig& stored = clockDilation->config();
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationSession,
		clockDilation::modeWord(stored.mode), clockDilation::setpointWord(stored.setpoint), stored.gainPerTick,
		stored.maxDeviation, fromIni ? "ini" : "compiled");
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kDilationDoors,
		stored.gainPerTick, gainFromIni ? "ini" : "compiled",
		stored.maxDeviation * 100.0, maxFromIni ? "ini" : "compiled",
		stored.targetOffsetTicks, offsetFromIni ? "ini" : "compiled",
		clockDilation::setpointWord(stored.setpoint), setpointFromIni ? "ini" : "compiled",
		stored.commonLeadTicks, commonLeadFromIni ? "ini" : "compiled");
}

template <SessionLogSink EngineT, typename ManagerT>
void applyLatencyKnobs(EngineT& engine, ManagerT& manager, const SessionKnobs& knobs,
	std::optional<ClockDilationController>& clockDilation, std::optional<ClockDilationWindow>& clockDilationWindow)
{
	if (knobs.hasIniRedundancyFrames)
	{
		const int32_t clampedFrames = latencyKnobs::clampRedundancyFrames(knobs.configuredRedundancyFrames);
		if (clampedFrames != knobs.configuredRedundancyFrames)
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kLatencyKnobsRedundancyClamped,
				knobs.configuredRedundancyFrames, clampedFrames);
		}
		manager.setRedundancyFrames(clampedFrames);
	}

	if (knobs.hasIniRelayResendEntries)
	{
		const int32_t clampedEntries = latencyKnobs::clampRelayResendEntries(knobs.configuredRelayResendEntries);
		if (clampedEntries != knobs.configuredRelayResendEntries)
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kLatencyKnobsResendClamped,
				knobs.configuredRelayResendEntries, clampedEntries);
		}
		manager.setRelayResendEntries(clampedEntries);
	}

	const TimeConfig& sessionTimeConfig = manager.getTimeConfig();
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kLatencyKnobsSession,
		sessionTimeConfig.redundancyFrames,
		sessionTimeConfig.relayResendEntries,
		(knobs.hasIniRedundancyFrames || knobs.hasIniRelayResendEntries) ? "ini" : "compiled");

	if (knobs.hasIniRecentreSettleSamples)
	{
		const int32_t clampedSamples = std::max(knobs.configuredRecentreSettleSamples, int32_t{ 0 });
		if (clampedSamples != knobs.configuredRecentreSettleSamples)
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kRecentreClamped, knobs.configuredRecentreSettleSamples,
				clampedSamples);
		}
		manager.setRecentreSettleSamples(static_cast<uint32_t>(clampedSamples));
	}
	sessionKnobsLines::logLine(engine, sessionKnobsLines::kRecentreSession,
		manager.getTimeConfig().recentreSettleSamples,
		knobs.hasIniRecentreSettleSamples ? "ini" : "compiled");

	bool jitterFromIni = false;
	if (knobs.hasIniJitterMultiplier)
	{
		if (!std::isfinite(knobs.configuredJitterMultiplier))
		{
			sessionKnobsLines::logLine(engine, sessionKnobsLines::kJitterNotFinite);
		}
		else
		{
			const double clampedMultiplier = clockJitter::clampJitterMultiplier(knobs.configuredJitterMultiplier);
			if (clampedMultiplier != knobs.configuredJitterMultiplier)
			{
				sessionKnobsLines::logLine(engine, sessionKnobsLines::kJitterClamped, knobs.configuredJitterMultiplier,
					clampedMultiplier);
			}
			manager.setJitterMultiplier(clampedMultiplier);
			jitterFromIni = true;
		}
	}

	applyClockRttSource(engine, manager, knobs, jitterFromIni);

	applyClockDilation(engine, manager, knobs, clockDilation, clockDilationWindow);
}
} // namespace ogsession
