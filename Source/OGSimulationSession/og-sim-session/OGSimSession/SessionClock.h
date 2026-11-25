#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulation/PCTimeManagement/ClockDilationController.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulationDisplaySnapshot.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetServer.h"

template <typename TraitsT>
void UESimulationSession<TraitsT>::onTimingInfoReceived(uint32_t authorityTick, double roundTripTime)
{
	m_latencyBudgetDiagnostics.noteWireSample_Internal(roundTripTime);
	if (m_onTimingInfoReceivedCallback)
	{
		m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
		return;
	}
	if (!m_manager.has_value())
		return;
	noteEngineRttForClockWindow(roundTripTime);
	m_manager->editNetworkEstimator().noteEngineRttSample(roundTripTime);
	m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
	maybeLogClockSourceWindow();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimClockSample(const SimClockRttSample& sample)
{
	if (!m_manager.has_value())
		return;
	m_manager->editNetworkEstimator().noteSimClockSample(sample);
	maybeLogClockSourceWindow();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::publishClientEffectiveInputDelayTicks(int32 delayTicks)
{
	m_inputResolution.setClientEffectiveInputDelayTicks(delayTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestInputDelayTransition(int32 deltaDelayTicks)
{
	// ⛔G-03  docs/UESimulationSession-guards.md
	if (!m_manager.has_value() || !m_manager->runsPrediction())
		return;

	m_manager->editClientClock().requestInputDelayTransition(deltaDelayTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beginTierConsumer()
{
	m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
	recomputeAndPublishEffectiveInputDelay();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::runClockDilation_GameThread(double hostNowSeconds, TickRateDoor& rate)
{
	if (!m_clockDilation.has_value() || m_clockDilation->config().mode == ClockDilationMode::Off)
		return;
	// ⛔G-38  docs/UESimulationSession-guards.md
	const double clockNowSeconds = FPlatformTime::Seconds();
	clockDilation::beforeClientPump(rate, hostNowSeconds, clockNowSeconds, *m_manager, *m_clockDilation,
		*m_clockDilationWindow);
	if (m_clockDilationWindow->isDue(clockNowSeconds))
	{
		UE_LOG(LogOGNet, Warning, TEXT("%hs"), m_clockDilationWindow->formatLine());
		m_clockDilationWindow->restart();
	}
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::noteAuthorityRateDoor_Owner(double hostNowSeconds, const TickRateDoor& rate)
{
	const uint64_t refused = rate.refusedRateChanges();
	if (refused == m_authorityRefusedRateChangesLogged_Owner && m_authorityRateLineDueSeconds_Owner.has_value()
		&& hostNowSeconds < *m_authorityRateLineDueSeconds_Owner)
	{
		return;
	}
	m_authorityRefusedRateChangesLogged_Owner = refused;
	m_authorityRateLineDueSeconds_Owner = hostNowSeconds + clockDilation::kWindowSeconds;
	UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Authority] refusedRateChanges=%llu rateScale=%.5f"),
		static_cast<unsigned long long>(refused), rate.rateScale());
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterStep_Step()
{
	const std::optional<double> tickStartSeconds = std::exchange(m_clockTickStartSeconds_Step, std::nullopt);
	if (m_clockServer == nullptr || !m_manager.has_value() || m_manager->runsPrediction())
		return;
	checkf(tickStartSeconds.has_value(),
		TEXT("UESimulationSession::afterStep_Step: an authority step ended with no start time. The brawler's tick-driver binding "
			 "calls noteAuthorityStepTime_Step in releaseInputsForAuthorityTick, at the start of every authority step, and the "
			 "SimClock server's tick age is measured from that time. Was publishServerTick(tick), which read the clock at the "
			 "publish (og-time-dilation task 2b)"));
	// ⛔G-39  docs/UESimulationSession-guards.md
	m_clockServer->publishServerTick(static_cast<uint32_t>(getServerClock().getTick()), *tickStartSeconds);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
{
    const int32 deltaDelayTicks = applyReplicatedConnectionTier(newTier);

    UE_LOG(LogOGNet, Log, TEXT("[ConnectionTier] tier %u -> %u, effective delay delta=%d"),
        (unsigned int)oldTier, (unsigned int)newTier, deltaDelayTicks);

    // ⛔G-40  docs/UESimulationSession-guards.md
    payInputDelayTransition(TEXT("[ConnectionTier]"), deltaDelayTicks,
        m_manager.has_value() ? (int32)m_manager->getTimeConfig().relayDelayFloorTicks : 0);
}

template <typename TraitsT>
int32 UESimulationSession<TraitsT>::applyReplicatedConnectionTier(uint8 tier)
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

    m_replicatedTierConsumer->onReplicatedTierReceived((int32)tier);
    return recomputeAndPublishEffectiveInputDelay();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyReplicatedRelayDelayFloor(uint8 floorTicks)
{
    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 clampedFloor = clampRelayDelayFloorTicks((int32)floorTicks, cfg);
    if (clampedFloor != (int32)floorTicks)
    {
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
            (unsigned int)floorTicks, clampedFloor);
    }

    m_manager->setRelayDelayFloorTicks(clampedFloor);

    logRelayDelayFloorAdvisory(clampedFloor);

    const int32 deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

    payInputDelayTransition(TEXT("[RelayDelayFloor]"), deltaDelayTicks, clampedFloor);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::payInputDelayTransition(const TCHAR* lineTag, int32 deltaDelayTicks, int32 floorTicks)
{
    if (deltaDelayTicks == 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("%s floor = %d ticks, effective delay delta=0, no stall or skip"),
            lineTag, floorTicks);
        return;
    }

    requestInputDelayTransition(deltaDelayTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
        lineTag, floorTicks, deltaDelayTicks > 0 ? TEXT("UPWARD") : TEXT("DOWNWARD"),
        deltaDelayTicks > 0 ? deltaDelayTicks : -deltaDelayTicks,
        deltaDelayTicks > 0 ? TEXT("stall") : TEXT("skip"));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::logRelayDelayFloorAdvisory(int32 floorTicks)
{
    if (!m_manager.has_value())
        return;

    // ⛔G-09  docs/UESimulationSession-guards.md
    switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
    {
    case RelayDelayFloorAdvisory::BelowHiccupBaseline:
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) — the one value that is neither"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::UniformDFairnessActive:
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active; tiers and LAN override are inert"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::None:
        break;
    }
}

template <typename TraitsT>
int32 UESimulationSession<TraitsT>::recomputeAndPublishEffectiveInputDelay()
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

    const int32 delayTicks = (int32)m_replicatedTierConsumer->effectiveInputDelayTicks();
    const int32 deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

    m_lastPublishedEffectiveInputDelayTicks = delayTicks;
    publishClientEffectiveInputDelayTicks(delayTicks);

    if (m_manager.has_value())
    {
        m_displayTier->write() = readDisplayTier(*m_replicatedTierConsumer, m_manager->getTimeConfig(), delayTicks);
        m_displayTier->publish();
    }

    UE_LOG(LogOGNet, Log,
        TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
        delayTicks,
        (int)m_replicatedTierConsumer->currentTierIndex(),
        m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
        m_manager.has_value() ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

    return deltaDelayTicks;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::noteEngineRttForClockWindow(double roundTripSeconds)
{
	if (!(roundTripSeconds > 0.0) || !FMath::IsFinite(roundTripSeconds))
		return;
	const double alpha = m_manager->getTimeConfig().rttSmoothingAlpha;
	m_engineRttWindowSeconds = m_engineRttWindowSeconds > 0.0
		? alpha * roundTripSeconds + (1.0 - alpha) * m_engineRttWindowSeconds
		: roundTripSeconds;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::maybeLogClockSourceWindow()
{
	if (!m_manager.has_value() || !m_manager->runsPrediction())
		return;
	const double now = FPlatformTime::Seconds();
	if (!m_clockWindowStartSeconds.has_value())
	{
		m_clockWindowStartSeconds = now;
		return;
	}
	if (now - *m_clockWindowStartSeconds < ogsimnet::kSimNetWindowSeconds)
		return;
	m_clockWindowStartSeconds = now;
	const NetworkTimeEstimator& estimator = m_manager->getNetworkEstimator();
	const ClockSourceShadow shadow = estimator.getClockSourceShadow();
	const double ackSrttSeconds = m_simNetLanes != nullptr ? m_simNetLanes->ackSrttSeconds() : kNoRttReadingSeconds;
	const UESimNetClockClient* const clock = m_simNetLanes != nullptr ? m_simNetLanes->clockClient() : nullptr;
	const ogsimnet::SimNetClockStats clockStats = clock != nullptr ? clock->stats() : ogsimnet::SimNetClockStats{};
	const auto toMs = [](double seconds) { return seconds >= 0.0 ? seconds * 1000.0 : -1.0; };
	UE_LOG(LogOGNet, Warning,
		TEXT("[ClockSource.Window] source=%s engineRttMs=%.3f simClockRttMs=%.3f ackSrttMs=%.3f minRttOffsetUs=%.0f ")
		TEXT("owdVarUs=%.0f/%.0f smoothedRttMs=%.3f samples=%u invalid=%u pings=%llu pongs=%llu"),
		estimator.getActiveRttSource() == TimeConfig::ClockRttSource::SimClock ? TEXT("SimClock") : TEXT("Engine"),
		toMs(m_engineRttWindowSeconds),
		shadow.samples > 0u ? shadow.simClockRttSeconds * 1000.0 : -1.0,
		toMs(ackSrttSeconds),
		shadow.offsetSeconds * 1000000.0,
		shadow.owdVarUpSeconds * 1000000.0,
		shadow.owdVarDownSeconds * 1000000.0,
		estimator.getSmoothedRTT() * 1000.0,
		shadow.samples,
		shadow.invalidSamples,
		static_cast<unsigned long long>(clockStats.pingsSent),
		static_cast<unsigned long long>(clockStats.pongsReceived));
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::noteAuthorityStepTime_Step()
{
    const double stepSeconds = FPlatformTime::Seconds();
    m_releaseStepSeconds_Step = stepSeconds;
    m_clockTickStartSeconds_Step = stepSeconds;
    if (m_manager.has_value())
        m_manager->noteAuthorityStepSeconds(stepSeconds);
}
