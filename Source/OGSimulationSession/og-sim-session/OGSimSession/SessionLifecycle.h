#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

#include <mutex>
#include <utility>
#include <vector>

#include "OGSimNet/SimNetBindings.h"

template <typename TraitsT>
void UESimulationSession<TraitsT>::onSimNetRegisteredRemoved(uint8_t simId)
{
    const unsigned int id = simId;
    m_systemsNotified_Step.erase(id);
    postLifecycleOutcome(id, LifecycleOutcomeKind::RosterRemoved);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::applyUnregisterRequests_Step(typename SimNetLanes::Client& client)
{
    std::vector<unsigned int> requests;
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        requests.swap(m_unregisterRequests);
    }
    for (const unsigned int id : requests)
        enqueueQueuedUnregister_Step(client, id);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::enqueueQueuedUnregister_Step(typename SimNetLanes::Client& client, unsigned int id)
{
    if (id == 0u || id > 0xFFu)
        return;
    m_unregisterRequested_Step.insert(id);
    // ⛔G-31  docs/UESimulationSession-guards.md
    if (m_systemsNotified_Step.erase(id) != 0u && m_storage.template has<Simulatable>(id))
        m_manager->notifyCharacterUnregistered(id);
    client.unregisterCharacter(static_cast<uint8_t>(id), [this, id](SimChannel* channel)
    {
        if (channel != nullptr)
            channel->clearOnInputHandedCallback();
        m_unregisterRequested_Step.erase(id);
        postLifecycleOutcome(id, LifecycleOutcomeKind::UnregisterApplied);
    });
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::onQueuedRegisterApplied_Step(unsigned int id, SimChannel* channel, bool hasProvider)
{
    if (channel == nullptr)
    {
        postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterRefused);
        return;
    }
    if (hasProvider)
    {
        channel->setOnInputHandedCallback([this, id](const InputBundleBuffer& bundle)
        {
            bundle.template forEachSlot<PlayerInput>([this, id](uint32 captureTick, const PlayerInput&)
            {
                m_latencyBudgetDiagnostics.stampHandedAndLeft_Internal(id, captureTick);
            });
        });
    }
    // ⛔G-32  docs/UESimulationSession-guards.md
    if (m_unregisterRequested_Step.find(id) == m_unregisterRequested_Step.end())
    {
        m_manager->notifyCharacterRegistered(id);
        m_systemsNotified_Step.insert(id);
    }
    postLifecycleOutcome(id, LifecycleOutcomeKind::RegisterApplied);
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::tryRegisterQueued(unsigned int id, typename TraitsT::Component& owner)
{
    PendingRegistration& record = m_pendingRegistrations.at(id);
    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    if (client == nullptr || m_immediateStepsStopped || id == 0u || id > 0xFFu)
        return TryRegisterStatus::Pending;
    const uint8_t simId = static_cast<uint8_t>(id);
    const bool hasProvider = static_cast<bool>(record.inputProvider);
    const ogsimnet::BindVerdict verdict = lanes->editBindings().markRegisteredIfReady(simId, hasProvider);
    if (verdict.step == ogsimnet::BindStep::Wait)
        return TryRegisterStatus::Pending;
    checkf(verdict.step == ogsimnet::BindStep::Ready,
        TEXT("UESimulationSession::tryRegister: id=%u registers %s an input provider, and its roster entry says the other: "
             "the provider is built for the engine's autonomous proxy, which clientBindStep checked against the roster. Was "
             "the engine role alone (isLocallyPredicted)"), id, hasProvider ? TEXT("with") : TEXT("without"));
    if (verdict.step != ogsimnet::BindStep::Ready)
        return TryRegisterStatus::Pending;
    client->registerCharacter(simId, std::move(*record.simulatable),
        m_latencyBudgetDiagnostics.wrapInputProvider(id, owner, std::move(record.inputProvider)),
        [this, id, hasProvider](SimChannel* channel) { onQueuedRegisterApplied_Step(id, channel, hasProvider); });
    record.simulatable.reset();
    record.queued = true;
    return TryRegisterStatus::Pending;
}

template <typename TraitsT>
TryRegisterStatus UESimulationSession<TraitsT>::finishQueuedRegistration(unsigned int id, typename TraitsT::Component& owner)
{
    collectLifecycleOutcomes_GameThread();
    const auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
        return TryRegisterStatus::Pending;
    PendingRegistration& record = it->second;
    if (record.refused || record.rosterRemoved)
    {
        if (m_simNetLanes != nullptr && id <= 0xFFu)
            m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(id));
        UE_LOG(LogOGMgmt, Error,
            TEXT("tryRegister: id=%u: the og-sim-net client's queued registration did not hold (%s); the character stays "
                 "unsimulated"),
            id, record.rosterRemoved ? TEXT("the roster removed it") : TEXT("its [SimNet.Lifecycle] line names why"));
        m_pendingRegistrations.erase(it);
        return TryRegisterStatus::Refused;
    }
    if (!record.applied)
        return TryRegisterStatus::Pending;

    m_registeredComponentsById[id] = &owner;
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, 0);
    m_game.afterRegistered(id, /*isAuthority=*/false);
    m_physicsBackend.onRegistered_GameThread(owner, id);
    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::requestQueuedUnregister_GameThread(unsigned int id, typename TraitsT::Component& owner)
{
    const auto pending = m_pendingRegistrations.find(id);
    const bool queued = pending != m_pendingRegistrations.end() && pending->second.queued;
    const bool registered = m_registeredComponentsById.find(id) != m_registeredComponentsById.end();
    if (pending != m_pendingRegistrations.end())
        m_pendingRegistrations.erase(pending);
    if (m_immediateStepsStopped || (!queued && !registered))
    {
        unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/true);
        return;
    }
    {
        const std::scoped_lock lock(m_lifecycleBridgeMutex);
        m_unregisterRequests.push_back(id);
    }
    m_pendingUnregisters[id] = TStrongObjectPtr<typename TraitsT::Component>(&owner);
    // ⛔G-33  docs/UESimulationSession-guards.md
    unregisterCharacterHostOnly(id, &owner, /*releaseBodiesNow=*/false);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::teardownImmediateClient_GameThread()
{
    if (!runsPrediction() || m_immediateStepsStopped)
        return;
    // ⛔G-34  docs/UESimulationSession-guards.md
    m_physicsBackend.onEndPlayBegin();
    m_immediateStepsStopped = true;

    SimNetLanes* const lanes = m_simNetLanes;
    typename SimNetLanes::Client* const client = lanes != nullptr ? lanes->client() : nullptr;
    if (client != nullptr)
    {
        applyUnregisterRequests_Step(*client);
        std::vector<unsigned int> live;
        for (const auto& entry : m_pendingRegistrations)
        {
            if (entry.second.queued)
                live.push_back(entry.first);
        }
        for (const auto& entry : m_registeredComponentsById)
            live.push_back(entry.first);
        for (const unsigned int id : live)
            enqueueQueuedUnregister_Step(*client, id);
        m_clock->beginStoppedDrain();
        lanes->drainClientWhileStopped_GameThread();
        m_clock->endStoppedDrain();
    }
    collectLifecycleOutcomes_GameThread();

    for (auto& entry : m_pendingUnregisters)
    {
        if (typename TraitsT::Component* const component = entry.second.Get())
            m_physicsBackend.onUnregistered_GameThread(*component, entry.first);
    }
    m_pendingUnregisters.clear();

    std::vector<unsigned int> registered;
    registered.reserve(m_registeredComponentsById.size());
    for (const auto& entry : m_registeredComponentsById)
        registered.push_back(entry.first);
    for (const unsigned int id : registered)
    {
        unregisterCharacterHostOnly(id, m_registeredComponentsById[id].Get(), /*releaseBodiesNow=*/true);
        m_simUnregisteredIds.insert(id);
    }

    for (auto it = m_pendingRegistrations.begin(); it != m_pendingRegistrations.end();)
    {
        if (!it->second.queued)
        {
            ++it;
            continue;
        }
        if (m_simNetLanes != nullptr && it->first <= 0xFFu)
            m_simNetLanes->editBindings().markUnregistered(static_cast<uint8_t>(it->first));
        it = m_pendingRegistrations.erase(it);
    }
}
