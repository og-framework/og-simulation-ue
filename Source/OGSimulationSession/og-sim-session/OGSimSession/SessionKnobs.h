#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/CorrectionRotation.h"
#include "OGSimulation/PCTimeManagement/ClockDilationController.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"

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
	int32   configuredRelayDelayFloorTicks;
	int32   configuredCorrectionRotationK;
	bool    hasIniResimTriggerPolicy;
	FString configuredResimTriggerPolicy;
	bool    hasIniRedundancyFrames;
	int32   configuredRedundancyFrames;
	bool    hasIniRelayResendEntries;
	int32   configuredRelayResendEntries;
	bool    hasIniRecentreSettleSamples;
	int32   configuredRecentreSettleSamples;
	bool    hasIniClockRttSource;
	FString configuredClockRttSource;
	bool    hasIniJitterMultiplier;
	double  configuredJitterMultiplier;
	bool    hasIniClockDilation;
	FString configuredClockDilation;
	bool    hasIniClockDilationGainPerTick;
	double  configuredClockDilationGainPerTick;
	bool    hasIniClockDilationMaxPercent;
	double  configuredClockDilationMaxPercent;
	bool    hasIniClockDilationTargetOffsetTicks;
	double  configuredClockDilationTargetOffsetTicks;
	bool    hasIniClockDilationSetpoint;
	FString configuredClockDilationSetpoint;
	bool    hasIniClockDilationCommonLeadTicks;
	double  configuredClockDilationCommonLeadTicks;
};

template <typename TraitsT>
auto UESimulationSession<TraitsT>::readSessionKnobs(bool worldIsAuthority) -> SessionKnobs
{
	int32 configuredRelayDelayFloorTicks = -1;
	// ⛔G-14  docs/UESimulationSession-guards.md
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniFloorTicks = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GEngineIni))
		{
			configuredRelayDelayFloorTicks = iniFloorTicks;
		}
	}

	// ⛔G-15  docs/UESimulationSession-guards.md

	int32 configuredCorrectionRotationK = -1;
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniK = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GEngineIni))
		{
			configuredCorrectionRotationK = iniK;
		}
	}

	bool    hasIniResimTriggerPolicy = false;
	FString configuredResimTriggerPolicy;
	// ⛔G-16  docs/UESimulationSession-guards.md
	if (GConfig != nullptr)
	{
		FString iniPolicy;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GEngineIni))
		{
			hasIniResimTriggerPolicy = true;
			configuredResimTriggerPolicy = iniPolicy.TrimStartAndEnd();
		}
	}

	bool  hasIniRedundancyFrames = false;
	int32 configuredRedundancyFrames = 0;
	bool  hasIniRelayResendEntries = false;
	int32 configuredRelayResendEntries = 0;
	// ⛔G-27  docs/UESimulationSession-guards.md
	if (GConfig != nullptr)
	{
		int32 iniFrames = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RedundancyFrames"), iniFrames, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RedundancyFrames"), iniFrames, GEngineIni))
		{
			hasIniRedundancyFrames = true;
			configuredRedundancyFrames = iniFrames;
		}
		int32 iniEntries = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayResendEntries"), iniEntries, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayResendEntries"), iniEntries, GEngineIni))
		{
			hasIniRelayResendEntries = true;
			configuredRelayResendEntries = iniEntries;
		}
	}

	bool  hasIniRecentreSettleSamples = false;
	int32 configuredRecentreSettleSamples = 0;
	if (GConfig != nullptr)
	{
		int32 iniSamples = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RecentreSettleSamples"), iniSamples, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RecentreSettleSamples"), iniSamples, GEngineIni))
		{
			hasIniRecentreSettleSamples = true;
			configuredRecentreSettleSamples = iniSamples;
		}
	}

	bool    hasIniClockRttSource = false;
	FString configuredClockRttSource;
	if (GConfig != nullptr)
	{
		FString iniSource;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockRttSource"), iniSource, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockRttSource"), iniSource, GEngineIni))
		{
			hasIniClockRttSource = true;
			configuredClockRttSource = iniSource.TrimStartAndEnd();
		}
	}

	bool   hasIniJitterMultiplier = false;
	double configuredJitterMultiplier = 0.0;
	if (GConfig != nullptr)
	{
		double iniMultiplier = 0.0;
		if (GConfig->GetDouble(TEXT("OGNetcode"), TEXT("JitterMultiplier"), iniMultiplier, GGameIni) ||
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("JitterMultiplier"), iniMultiplier, GEngineIni))
		{
			hasIniJitterMultiplier = true;
			configuredJitterMultiplier = iniMultiplier;
		}
	}

	bool    hasIniClockDilation = false;
	FString configuredClockDilation;
	if (GConfig != nullptr)
	{
		FString iniDilation;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockDilation"), iniDilation, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockDilation"), iniDilation, GEngineIni))
		{
			hasIniClockDilation = true;
			configuredClockDilation = iniDilation.TrimStartAndEnd();
		}
	}

	bool    hasIniClockDilationGainPerTick = false;
	double  configuredClockDilationGainPerTick = 0.0;
	bool    hasIniClockDilationMaxPercent = false;
	double  configuredClockDilationMaxPercent = 0.0;
	bool    hasIniClockDilationTargetOffsetTicks = false;
	double  configuredClockDilationTargetOffsetTicks = 0.0;
	bool    hasIniClockDilationSetpoint = false;
	FString configuredClockDilationSetpoint;
	bool    hasIniClockDilationCommonLeadTicks = false;
	double  configuredClockDilationCommonLeadTicks = 0.0;
	if (GConfig != nullptr)
	{
		hasIniClockDilationGainPerTick =
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationGainPerTick"), configuredClockDilationGainPerTick, GGameIni) ||
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationGainPerTick"), configuredClockDilationGainPerTick, GEngineIni);
		hasIniClockDilationMaxPercent =
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationMaxPercent"), configuredClockDilationMaxPercent, GGameIni) ||
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationMaxPercent"), configuredClockDilationMaxPercent, GEngineIni);
		hasIniClockDilationTargetOffsetTicks =
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationTargetOffsetTicks"), configuredClockDilationTargetOffsetTicks,
				GGameIni) ||
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationTargetOffsetTicks"), configuredClockDilationTargetOffsetTicks,
				GEngineIni);
		hasIniClockDilationCommonLeadTicks =
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationCommonLeadTicks"), configuredClockDilationCommonLeadTicks,
				GGameIni) ||
			GConfig->GetDouble(TEXT("OGNetcode"), TEXT("ClockDilationCommonLeadTicks"), configuredClockDilationCommonLeadTicks,
				GEngineIni);
		FString iniSetpoint;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockDilationSetpoint"), iniSetpoint, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ClockDilationSetpoint"), iniSetpoint, GEngineIni))
		{
			hasIniClockDilationSetpoint = true;
			configuredClockDilationSetpoint = iniSetpoint.TrimStartAndEnd();
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

template <typename TraitsT>
void UESimulationSession<TraitsT>::applySessionKnobs(const SessionKnobs& knobs)
{
	if (knobs.configuredRelayDelayFloorTicks >= 0)
	{
		const int32 clampedFloor = clampRelayDelayFloorTicks(
			knobs.configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
		if (clampedFloor != knobs.configuredRelayDelayFloorTicks)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"),
				knobs.configuredRelayDelayFloorTicks, clampedFloor);
		}
		m_manager->setRelayDelayFloorTicks(clampedFloor);
		UE_LOG(LogOGNet, Log,
			TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), clampedFloor);

		logRelayDelayFloorAdvisory(clampedFloor);
	}

	if (knobs.configuredCorrectionRotationK != -1)
	{
		const int32 clampedK = correctionRotation::clampK(knobs.configuredCorrectionRotationK);
		if (clampedK != knobs.configuredCorrectionRotationK)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"),
				knobs.configuredCorrectionRotationK, clampedK);
		}
		m_manager->setCorrectionRotationK(clampedK);
	}

	const int32 sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
	UE_LOG(LogOGNet, Warning,
		TEXT("[StateRotation] session K = %d (%s)"),
		sessionCorrectionRotationK,
		knobs.configuredCorrectionRotationK != -1 ? TEXT("ini override") : TEXT("compiled default"));

	const int32 sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
	checkf(m_simNetServer.has_value(),
		TEXT("UESimulationSession::applySessionKnobs: the og-sim-net server publishes the session floor, so beginPlay emplaces "
			 "it before the knobs are applied. Was the timing relay's spawn-before-the-manager order (og-netcode-v2-input-relay "
			 "T11) and its 'no timing relay at manager BeginPlay' warning"));
	m_simNetServer->publishFloor(static_cast<uint8_t>(sessionRelayDelayFloorTicks));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyResimTriggerPolicy(SessionKnobs& knobs)
{
	// ⛔G-17  docs/UESimulationSession-guards.md
	if (knobs.hasIniResimTriggerPolicy)
	{
		if (knobs.configuredResimTriggerPolicy.Equals(TEXT("OnDisagreement"), ESearchCase::IgnoreCase))
		{
			m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
		}
		else if (knobs.configuredResimTriggerPolicy.Equals(TEXT("FrontierExact"), ESearchCase::IgnoreCase))
		{
			m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
				TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"),
				*knobs.configuredResimTriggerPolicy);
			knobs.hasIniResimTriggerPolicy = false;
		}
	}

	const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
	const bool policyIsOnDisagreement =
		sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
	const FString depthPolicyText = policyIsOnDisagreement
		? FString::Printf(TEXT("skip beyond %d ticks"), sessionTimeConfig.rollbackWindowTicks)
		: FString(TEXT("off (inert under FrontierExact)"));
	UE_LOG(LogOGNet, Warning,
		TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
		policyIsOnDisagreement ? TEXT("OnDisagreement") : TEXT("FrontierExact"),
		knobs.hasIniResimTriggerPolicy ? TEXT("ini override") : TEXT("compiled default"),
		*depthPolicyText);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyLatencyKnobs(const SessionKnobs& knobs)
{
	if (knobs.hasIniRedundancyFrames)
	{
		const int32 clampedFrames = latencyKnobs::clampRedundancyFrames(knobs.configuredRedundancyFrames);
		if (clampedFrames != knobs.configuredRedundancyFrames)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d"),
				knobs.configuredRedundancyFrames, clampedFrames);
		}
		m_manager->setRedundancyFrames(clampedFrames);
	}

	if (knobs.hasIniRelayResendEntries)
	{
		const int32 clampedEntries = latencyKnobs::clampRelayResendEntries(knobs.configuredRelayResendEntries);
		if (clampedEntries != knobs.configuredRelayResendEntries)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d"),
				knobs.configuredRelayResendEntries, clampedEntries);
		}
		m_manager->setRelayResendEntries(clampedEntries);
	}

	const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
	UE_LOG(LogOGNet, Warning,
		TEXT("[LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)"),
		sessionTimeConfig.redundancyFrames,
		sessionTimeConfig.relayResendEntries,
		(knobs.hasIniRedundancyFrames || knobs.hasIniRelayResendEntries) ? TEXT("ini") : TEXT("compiled"));

	if (knobs.hasIniRecentreSettleSamples)
	{
		const int32 clampedSamples = FMath::Max(knobs.configuredRecentreSettleSamples, 0);
		if (clampedSamples != knobs.configuredRecentreSettleSamples)
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d"),
				knobs.configuredRecentreSettleSamples, clampedSamples);
		}
		m_manager->setRecentreSettleSamples(static_cast<uint32_t>(clampedSamples));
	}
	UE_LOG(LogOGNet, Warning,
		TEXT("[ClockRecentre.Config] recentreSettleSamples=%u (%s)"),
		m_manager->getTimeConfig().recentreSettleSamples,
		knobs.hasIniRecentreSettleSamples ? TEXT("ini") : TEXT("compiled"));

	bool jitterFromIni = false;
	if (knobs.hasIniJitterMultiplier)
	{
		if (!FMath::IsFinite(knobs.configuredJitterMultiplier))
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults"));
		}
		else
		{
			const double clampedMultiplier = clockJitter::clampJitterMultiplier(knobs.configuredJitterMultiplier);
			if (clampedMultiplier != knobs.configuredJitterMultiplier)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier=%.3f out of range, clamped to %.3f"),
					knobs.configuredJitterMultiplier, clampedMultiplier);
			}
			m_manager->setJitterMultiplier(clampedMultiplier);
			jitterFromIni = true;
		}
	}

	applyClockRttSource(knobs, jitterFromIni);

	applyClockDilation(knobs);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyClockRttSource(const SessionKnobs& knobs, bool jitterFromIni)
{
	bool fromIni = false;
	if (knobs.hasIniClockRttSource)
	{
		if (knobs.configuredClockRttSource.Equals(TEXT("Engine"), ESearchCase::IgnoreCase))
		{
			m_manager->setClockRttSource(TimeConfig::ClockRttSource::Engine);
			fromIni = true;
		}
		else if (knobs.configuredClockRttSource.Equals(TEXT("SimClock"), ESearchCase::IgnoreCase))
		{
			m_manager->setClockRttSource(TimeConfig::ClockRttSource::SimClock);
			fromIni = true;
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[ClockSource.Config] ini [OGNetcode] ClockRttSource='%s' unrecognised (expected Engine or SimClock), keeping ")
				TEXT("the compiled default"),
				*knobs.configuredClockRttSource);
		}
	}
	UE_LOG(LogOGNet, Warning,
		TEXT("[ClockSource.Config] clockRttSource=%s (%s) jitterMultiplier engine=%.3f simClock=%.3f (%s)"),
		m_manager->getTimeConfig().clockRttSource == TimeConfig::ClockRttSource::SimClock ? TEXT("SimClock") : TEXT("Engine"),
		fromIni ? TEXT("ini") : TEXT("compiled"),
		m_manager->getTimeConfig().jitterMultiplier,
		m_manager->getTimeConfig().simClockJitterMultiplier,
		jitterFromIni ? TEXT("ini") : TEXT("compiled"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyClockDilation(const SessionKnobs& knobs)
{
	if (!m_manager->runsPrediction())
	{
		UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] mode=n/a"));
		return;
	}
	ClockDilationConfig config;
	bool fromIni = false;
	if (knobs.hasIniClockDilation)
	{
		if (knobs.configuredClockDilation.Equals(TEXT("Off"), ESearchCase::IgnoreCase))
		{
			config.mode = ClockDilationMode::Off;
			fromIni = true;
		}
		else if (knobs.configuredClockDilation.Equals(TEXT("Shadow"), ESearchCase::IgnoreCase))
		{
			config.mode = ClockDilationMode::Shadow;
			fromIni = true;
		}
		else if (knobs.configuredClockDilation.Equals(TEXT("On"), ESearchCase::IgnoreCase))
		{
			config.mode = ClockDilationMode::On;
			fromIni = true;
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[Dilation.Config] ini [OGNetcode] ClockDilation='%s' unrecognised (expected Off, Shadow or On), running Off"),
				*knobs.configuredClockDilation);
		}
	}

	const bool gainFromIni = applyClockDilationDoor(TEXT("ClockDilationGainPerTick"), knobs.hasIniClockDilationGainPerTick,
		knobs.configuredClockDilationGainPerTick, &clockDilationDoors::clampGainPerTick, config.gainPerTick);
	double maxPercent = config.maxDeviation * 100.0;
	const bool maxFromIni = applyClockDilationDoor(TEXT("ClockDilationMaxPercent"), knobs.hasIniClockDilationMaxPercent,
		knobs.configuredClockDilationMaxPercent, &clockDilationDoors::clampMaxPercent, maxPercent);
	if (maxFromIni)
		config.maxDeviation = maxPercent / 100.0;
	const bool offsetFromIni = applyClockDilationDoor(TEXT("ClockDilationTargetOffsetTicks"),
		knobs.hasIniClockDilationTargetOffsetTicks, knobs.configuredClockDilationTargetOffsetTicks,
		&clockDilationDoors::clampTargetOffsetTicks, config.targetOffsetTicks);
	const bool commonLeadFromIni = applyClockDilationDoor(TEXT("ClockDilationCommonLeadTicks"),
		knobs.hasIniClockDilationCommonLeadTicks, knobs.configuredClockDilationCommonLeadTicks,
		&clockDilationDoors::clampCommonLeadTicks, config.commonLeadTicks);
	bool setpointFromIni = false;
	if (knobs.hasIniClockDilationSetpoint)
	{
		if (knobs.configuredClockDilationSetpoint.Equals(TEXT("OwnRtt"), ESearchCase::IgnoreCase))
		{
			config.setpoint = ClockDilationSetpoint::OwnRtt;
			setpointFromIni = true;
		}
		else if (knobs.configuredClockDilationSetpoint.Equals(TEXT("Common"), ESearchCase::IgnoreCase))
		{
			config.setpoint = ClockDilationSetpoint::Common;
			setpointFromIni = true;
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='%s' unrecognised (expected OwnRtt or Common), ")
				TEXT("running %hs"),
				*knobs.configuredClockDilationSetpoint, clockDilation::setpointWord(config.setpoint));
		}
	}

	m_clockDilation.emplace(config);
	m_clockDilationWindow.emplace(config);
	const ClockDilationConfig& stored = m_clockDilation->config();
	UE_LOG(LogOGNet, Warning,
		TEXT("[Dilation.Config] mode=%hs setpoint=%hs gainPerTick=%.4f maxDeviation=%.4f (%s)"),
		clockDilation::modeWord(stored.mode), clockDilation::setpointWord(stored.setpoint), stored.gainPerTick,
		stored.maxDeviation, fromIni ? TEXT("ini") : TEXT("compiled"));
	UE_LOG(LogOGNet, Warning,
		TEXT("[Dilation.Config] doors gainPerTick=%.4f (%s) maxPercent=%.3f (%s) targetOffsetTicks=%.3f (%s) setpoint=%hs (%s) ")
		TEXT("commonLeadTicks=%.3f (%s)"),
		stored.gainPerTick, gainFromIni ? TEXT("ini") : TEXT("compiled"),
		stored.maxDeviation * 100.0, maxFromIni ? TEXT("ini") : TEXT("compiled"),
		stored.targetOffsetTicks, offsetFromIni ? TEXT("ini") : TEXT("compiled"),
		clockDilation::setpointWord(stored.setpoint), setpointFromIni ? TEXT("ini") : TEXT("compiled"),
		stored.commonLeadTicks, commonLeadFromIni ? TEXT("ini") : TEXT("compiled"));
}

template <typename TraitsT>
bool UESimulationSession<TraitsT>::applyClockDilationDoor(const TCHAR* key, bool hasIni, double requested,
	double (*clampToDoor)(double), double& value)
{
	if (!hasIni)
		return false;
	if (!FMath::IsFinite(requested))
	{
		UE_LOG(LogOGNet, Warning,
			TEXT("[Dilation.Config] ini [OGNetcode] %s is not a finite number, keeping the compiled default"), key);
		return false;
	}
	const double clamped = clampToDoor(requested);
	if (clamped != requested)
	{
		UE_LOG(LogOGNet, Warning,
			TEXT("[Dilation.Config] ini [OGNetcode] %s=%.4f out of range, clamped to %.4f"), key, requested, clamped);
	}
	value = clamped;
	return true;
}
