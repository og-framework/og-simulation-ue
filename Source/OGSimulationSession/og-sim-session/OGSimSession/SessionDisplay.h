#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationSession-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationSession-guards.md

#include <array>
#include <concepts>
#include <cstddef>

#include "OGSimulation/Mailbox.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"

template <typename TraitsT>
struct UESimulationSession<TraitsT>::AuthorityPresentationEntry
{
	unsigned int               simId = 0u;
	AuthorityPresentationSlice slice{};
};

template <typename TraitsT>
struct UESimulationSession<TraitsT>::AuthorityPresentation
{
	size_t                                                       entryCount = 0u;
	std::array<AuthorityPresentationEntry, kMaxSimulatableSlots> entries{};
};

template <typename TraitsT>
void UESimulationSession<TraitsT>::publishAuthorityPresentation_Runner()
{
    static_assert(requires(const SystemsExec& systems, const UESimulationSession& session, unsigned int id,
            const AuthorityPresentationSlice& slice)
        {
            { Game::authorityPresentationSlice_Step(systems, id) } -> std::same_as<AuthorityPresentationSlice>;
            Game::presentAuthoritySlice(session, id, slice);
        },
        "UESimulationSession<TraitsT>: the game's presentation of authority-only state is "
        "two static hooks of TraitsT::Game over its AuthorityPresentationSlice: authorityPresentationSlice_Step(systems, id), "
        "read on the runner after each batch, and presentAuthoritySlice(session, id, slice), applied on the game thread "
        "from the newest published snapshot. Was the loop's skip of afterVisualization, whose ring-out score push read the "
        "score system on the game thread (og-input-latency P5-19 F5)");
    checkf(m_systemsExec.has_value(),
        TEXT("UESimulationSession::publishAuthorityPresentation_Runner: the authority's systems executor is built by the "
             "session's configuration, which the runner runs before its first batch"));
    AuthorityPresentation& presentation = m_authorityPresentation->write();
    presentation.entryCount = 0u;
    m_storage.template forEachSimulatable<Simulatable>([this, &presentation](unsigned int id, const auto&)
    {
        checkf(presentation.entryCount < presentation.entries.size(),
            TEXT("UESimulationSession: the authority holds more simulatables than its presentation snapshot's "
                 "kMaxSimulatableSlots entries"));
        presentation.entries[presentation.entryCount++] =
            AuthorityPresentationEntry{ id, Game::authorityPresentationSlice_Step(*m_systemsExec, id) };
    });
    m_authorityPresentation->publish();
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::presentAuthority_GameThread()
{
    const AuthorityPresentation* const presentation = m_authorityPresentation->acquireLatest();
    if (presentation == nullptr)
        return;
    for (size_t index = 0u; index < presentation->entryCount; ++index)
        Game::presentAuthoritySlice(*this, presentation->entries[index].simId, presentation->entries[index].slice);
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::publishDisplaySnapshot_Step()
{
	checkf(publishesDisplayOnStep(),
		TEXT("UESimulationSession::publishDisplaySnapshot_Step is the driver-scheduled client's display publish, after its step "
			 "batch on the step thread; an authority publishes its presentation on its runner instead "
			 "(publishAuthorityPresentation_Runner) and fills no display snapshot. "
			 "Was the game-thread fill racing the worker step (og-input-latency task P5-36)"));

	const DisplayClockReading clock = readDisplayClock(m_manager->getClientClock(), m_manager->getNetworkEstimator());
	DisplayTierReading tier;
	if (const DisplayTierReading* const published = m_clock->displayTier().acquireLatest())
		tier = *published;

	m_displayPublisher->publish_Step([&](DisplaySnapshot& snapshot)
	{
		fillSimulationDisplaySnapshot(snapshot, m_storage, m_inputResolution, m_reconciliation, clock, tier);
	});
}

template <typename TraitsT>
void UESimulationSession<TraitsT>::refreshDisplayView_GameThread()
{
	m_heldDisplaySnapshot = m_displayPublisher->refreshView_GameThread(m_heldDisplaySnapshot);
	if (m_heldDisplaySnapshot == nullptr)
		return;

	for (size_t index = 0u; index < m_heldDisplaySnapshot->entryCount; ++index)
	{
		const DisplayEntry& entry = m_heldDisplaySnapshot->entries[index];
		DisplayVizState& vizState = (*m_displayVizStates)[index];
		vizState = entry.vizState;
		m_physicsBackend.overlayRenderPoses_GameThread(entry.simId, vizState);
	}
}
