#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionDisplay-rationale.md · docs/SessionDisplay-guards.md

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>

#include "OGSimulation/Mailbox.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"

namespace ogsession
{
template <typename M>
concept SessionDisplayManager = requires(const M& m)
{
	{ m.getClientClock() }      -> std::same_as<const ClientPredictionClock&>;
	{ m.getNetworkEstimator() } -> std::same_as<const NetworkTimeEstimator&>;
};

template <typename G, typename SystemsT>
concept SessionPresentationGame = requires(const SystemsT& systems, unsigned int id)
{
	typename G::AuthorityPresentationSlice;
	{ G::authorityPresentationSlice_Step(systems, id) } -> std::same_as<typename G::AuthorityPresentationSlice>;
};

namespace sessionDisplaySelfCheck
{
struct ConformingManager
{
	const ClientPredictionClock& getClientClock() const;
	const NetworkTimeEstimator&  getNetworkEstimator() const;
};

struct ManagerWithoutClientClock
{
	const NetworkTimeEstimator& getNetworkEstimator() const;
};

struct ManagerWithoutEstimator
{
	const ClientPredictionClock& getClientClock() const;
};

struct ManagerReadableOnlyWhenMutable
{
	const ClientPredictionClock& getClientClock();
	const NetworkTimeEstimator&  getNetworkEstimator();
};

struct Systems
{
};

struct ConformingGame
{
	struct AuthorityPresentationSlice
	{
	};
	static AuthorityPresentationSlice authorityPresentationSlice_Step(const Systems&, unsigned int);
};

struct GameWithoutSliceType
{
	static int authorityPresentationSlice_Step(const Systems&, unsigned int);
};

struct GameWithoutSliceHook
{
	struct AuthorityPresentationSlice
	{
	};
};

struct GameReturningAnotherType
{
	struct AuthorityPresentationSlice
	{
	};
	static int authorityPresentationSlice_Step(const Systems&, unsigned int);
};

static_assert(SessionDisplayManager<ConformingManager>,
	"SessionDisplayManager self-check: the reference shape (const getClientClock, const getNetworkEstimator) must satisfy the "
	"concept");
static_assert(!SessionDisplayManager<ManagerWithoutClientClock>,
	"SessionDisplayManager self-check: getClientClock is required, the snapshot's clock reading is the prediction clock's");
static_assert(!SessionDisplayManager<ManagerWithoutEstimator>,
	"SessionDisplayManager self-check: getNetworkEstimator is required, the clock reading carries the estimator's target");
static_assert(!SessionDisplayManager<ManagerReadableOnlyWhenMutable>,
	"SessionDisplayManager self-check: both readers must be callable on a const manager; the display only reads it");
static_assert(SessionPresentationGame<ConformingGame, Systems>,
	"SessionPresentationGame self-check: the reference shape (a slice type and a static authorityPresentationSlice_Step "
	"returning it) must satisfy the concept");
static_assert(!SessionPresentationGame<GameWithoutSliceType, Systems>,
	"SessionPresentationGame self-check: the game names its AuthorityPresentationSlice type");
static_assert(!SessionPresentationGame<GameWithoutSliceHook, Systems>,
	"SessionPresentationGame self-check: authorityPresentationSlice_Step(systems, id) is required, it reads each entry's slice");
static_assert(!SessionPresentationGame<GameReturningAnotherType, Systems>,
	"SessionPresentationGame self-check: authorityPresentationSlice_Step returns the game's AuthorityPresentationSlice");
} // namespace sessionDisplaySelfCheck

template <typename GameT, typename SnapshotT, SessionDisplayManager ManagerT, typename StorageT, typename ResolutionT,
	typename ReconciliationT>
class SessionDisplay
{
public:
	using Snapshot          = SnapshotT;
	using Entry             = typename SnapshotT::Entry;
	using VizState          = typename Entry::VizState;
	using Simulatable       = typename SnapshotT::Simulatable;
	using PresentationSlice = typename GameT::AuthorityPresentationSlice;

	static constexpr size_t kMaxEntries = SnapshotT::kMaxSimulatables;

	struct PresentationEntry
	{
		unsigned int      simId = 0u;
		PresentationSlice slice{};
	};

	struct Presentation
	{
		size_t                                     entryCount = 0u;
		std::array<PresentationEntry, kMaxEntries> entries{};
	};

	SessionDisplay(const SessionThreadChecks& threads, const ManagerT& manager, const StorageT& storage,
		const ResolutionT& resolution, const ReconciliationT& reconciliation, TripleBuffer<DisplayTierReading>& tier)
		: m_threads(threads)
		, m_manager(manager)
		, m_storage(storage)
		, m_resolution(resolution)
		, m_reconciliation(reconciliation)
		, m_tier(tier)
	{
	}

	SessionDisplay(const SessionDisplay&) = delete;
	SessionDisplay& operator=(const SessionDisplay&) = delete;

	void publishAfterBatch()
	{
		m_threads.checkOp(SessionOp::Publish);
		OG_CHECK(m_threads.role() == SessionRole::Prediction,
			"SessionDisplay::publishAfterBatch is the driver-scheduled client's display publish, after its step batch on the "
			"step thread; an authority publishes its presentation on its runner instead (publishPresentation) and fills no "
			"display snapshot. Was the Unreal session's checkf in publishDisplaySnapshot_Step, the game-thread fill racing the "
			"worker step (og-input-latency task P5-36)");

		const DisplayClockReading clock = readDisplayClock(m_manager.getClientClock(), m_manager.getNetworkEstimator());
		DisplayTierReading tier;
		if (const DisplayTierReading* const published = m_tier.acquireLatest())
			tier = *published;

		m_publisher->publish_Step([&](SnapshotT& snapshot)
		{
			fillSimulationDisplaySnapshot(snapshot, m_storage, m_resolution, m_reconciliation, clock, tier);
		});
	}

	template <typename OverlayF>
		requires std::invocable<OverlayF&, unsigned int, VizState&>
	void acquireForFrame(OverlayF&& overlay)
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		// ⛔G-01  docs/SessionDisplay-guards.md
		m_held = m_publisher->refreshView_GameThread(m_held);
		if (m_held == nullptr)
			return;

		for (size_t index = 0u; index < m_held->entryCount; ++index)
		{
			const Entry& entry = m_held->entries[index];
			VizState& vizState = (*m_vizStates)[index];
			vizState = entry.vizState;
			overlay(entry.simId, vizState);
		}
	}

	void releaseHeld()
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		if (m_held != nullptr)
			m_publisher->release_GameThread(m_held);
		m_held = nullptr;
	}

	const SnapshotT* snapshot() const
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		return m_held;
	}

	const Entry* findEntry(unsigned int id) const
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		return m_held != nullptr ? m_held->find(id) : nullptr;
	}

	const VizState* findVizState(unsigned int id) const
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		const Entry* const entry = findEntry(id);
		if (entry == nullptr)
			return nullptr;
		return &(*m_vizStates)[static_cast<size_t>(entry - m_held->entries.data())];
	}

	template <typename FunctionT>
	void forEachVizState(FunctionT&& function) const
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		if (m_held == nullptr)
			return;
		for (size_t index = 0u; index < m_held->entryCount; ++index)
			function(m_held->entries[index].simId, (*m_vizStates)[index]);
	}

	template <typename SystemsT>
		requires SessionPresentationGame<GameT, SystemsT>
	void publishPresentation(const SystemsT& systems)
	{
		m_threads.checkOp(SessionOp::Publish);
		OG_CHECK(m_threads.role() == SessionRole::Authority,
			"SessionDisplay::publishPresentation is the authority's: its runner publishes the game's authority-only "
			"presentation after each batch; a client publishes its display snapshot instead (publishAfterBatch)");

		Presentation& presentation = m_presentation->write();
		presentation.entryCount = 0u;
		m_storage.template forEachSimulatable<Simulatable>([&presentation, &systems](unsigned int id, const auto&)
		{
			OG_CHECK(presentation.entryCount < presentation.entries.size(),
				"SessionDisplay::publishPresentation: the authority holds more simulatables than its presentation snapshot's "
				"kMaxEntries entries (the display snapshot's kMaxSimulatables). Was the Unreal session's checkf in "
				"publishAuthorityPresentation_Runner");
			if (presentation.entryCount >= presentation.entries.size())
				return;
			presentation.entries[presentation.entryCount++] =
				PresentationEntry{ id, GameT::authorityPresentationSlice_Step(systems, id) };
		});
		m_presentation->publish();
	}

	template <typename PresentF>
		requires std::invocable<PresentF&, unsigned int, const PresentationSlice&>
	void presentLatest(PresentF&& present)
	{
		m_threads.checkOp(SessionOp::AcquireDisplay);
		const Presentation* const presentation = m_presentation->acquireLatest();
		if (presentation == nullptr)
			return;
		for (size_t index = 0u; index < presentation->entryCount; ++index)
			present(presentation->entries[index].simId, presentation->entries[index].slice);
	}

private:
	SessionThreadChecks              m_threads;
	const ManagerT&                  m_manager;
	const StorageT&                  m_storage;
	const ResolutionT&               m_resolution;
	const ReconciliationT&           m_reconciliation;
	TripleBuffer<DisplayTierReading>& m_tier;

	std::unique_ptr<SimulationDisplayPublisher<SnapshotT>> m_publisher =
		std::make_unique<SimulationDisplayPublisher<SnapshotT>>();
	std::unique_ptr<std::array<VizState, kMaxEntries>> m_vizStates = std::make_unique<std::array<VizState, kMaxEntries>>();
	const SnapshotT*                                    m_held = nullptr;

	std::unique_ptr<TripleBuffer<Presentation>> m_presentation = std::make_unique<TripleBuffer<Presentation>>();
};
} // namespace ogsession
