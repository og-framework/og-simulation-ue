#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

#include <mutex>
#include <optional>
#include <vector>

#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/TickDriverGameTypes.h"

template <typename TraitsT>
IdleWake UESimulationSession<TraitsT>::idleUntil_Runner(double deadlineSeconds)
{
	return m_simNetIdle.has_value() ? m_simNetIdle->idleUntil(deadlineSeconds) : IdleWake::Unavailable;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::beforePump_Owner(double hostNowSeconds, TickRateDoor& rate)
{
	if (m_clock.has_value() && m_manager->runsPrediction())
	{
		m_clock->beforePredictionPump(hostNowSeconds, rate);
		return;
	}
	if (m_clock.has_value())
		m_clock->noteAuthorityRateDoor(hostNowSeconds, rate);
	std::vector<TUniqueFunction<void()>> requests;
	{
		const std::scoped_lock lock(m_lifecycleBridgeMutex);
		requests.swap(m_runnerRequests);
	}
	for (TUniqueFunction<void()>& request : requests)
		request();
	if (m_simNetServer.has_value())
		m_simNetServer->drain();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::releaseDelayedInputsForStep(int32 firstTick, int32 numSteps)
{
    const std::optional<double> stepSeconds = m_clock.has_value() ? m_clock->takeReleaseStepSeconds() : std::nullopt;
    if (!m_simNetServer.has_value())
        return;

    OG_LOG_ROUTED(LogOGNet, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    m_simNetServer->releaseForStep(firstTick, numSteps, stepSeconds);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterAuthorityBatch(int32 firstUpcomingSimTick)
{
    if (!m_simNetServer.has_value())
        return;
    checkf(firstUpcomingSimTick != 0 || m_simNetServer->attachedConnections().empty(),
        TEXT("UESimulationSession::afterAuthorityBatch: the authority loop's first batch reports tick 0 (the driver publishes its "
             "offset after a tick ran), and a client capture reaped against it is refused by the coordinator's domain gate. The "
             "runner services og-net only in its idle, after its first batch, so no connection can be attached yet. Was P2-19 F2"));
    m_manager->onPostSimulationGameThread();
    m_simNetServer->publishClock(static_cast<uint32_t>(getServerClock().getTick()));
    m_simNetServer->afterInlineBatch(firstUpcomingSimTick);
    stampCorrectionsHanded_Runner();
    publishAuthorityPresentation_Runner();
}

template <typename TraitsT>
uint32 UESimulationSession<TraitsT>::correctionTickOf(const SimChannel& channel)
{
    const CorrectionStateBuffer& buffer = channel.getSyncedCorrectionStateBuffer();
    if (buffer.payload().size() < correctionStateBuffer::kHeaderBytes)
        return 0u;
    return buffer.template readFromBuffer<uint32>(correctionStateBuffer::kTickOffset);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::stampCorrectionsHanded_Runner()
{
    const double handedSeconds = FPlatformTime::Seconds();
    m_storage.template forEachSimulatable<Simulatable>([this, handedSeconds](unsigned int id, const auto&)
    {
        SimChannel* const channel =
            id != 0u && id <= 0xFFu ? m_simNetServer->template channel<Simulatable>(static_cast<uint8_t>(id)) : nullptr;
        if (channel == nullptr)
            return;
        const uint32 correctionTick = correctionTickOf(*channel);
        if (correctionTick != 0u)
            m_latencyBudgetDiagnostics.stampCorrectionHanded_Internal(id, correctionTick, handedSeconds);
    });
}

template <typename TraitsT>
typename UESimulationSession<TraitsT>::SimNetLanes::Client* UESimulationSession<TraitsT>::boundImmediateClient_Step() const
{
    if (!runsPrediction())
        return nullptr;
    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    return client != nullptr && lanes->clientSessionBound() ? client : nullptr;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::afterClientCapture_Step()
{
    typename SimNetLanes::Client* const client = boundImmediateClient_Step();
    if (client == nullptr)
        return;
    client->afterCaptureTick_Step(m_netSync, m_manager->currentIntegratedTick(), m_manager->getTimeConfig());
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::receiveForClientStep_Step()
{
    typename SimNetLanes::Client* const client = boundImmediateClient_Step();
    if (client == nullptr)
        return;
    applyUnregisterRequests_Step(*client);
    m_simNetLanes->drainClient_Step();
}
