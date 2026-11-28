// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionDisplay.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"
#include "OGSimulation/Mailbox.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
struct SessionDisplayMockState
{
	std::int32_t position = 0;
	bool isSimilarTo(const SessionDisplayMockState& other) const { return position == other.position; }
};

struct SessionDisplayMockInput
{
	std::int32_t value = 0;
};
} // namespace

template <>
struct SerializableFields<SessionDisplayMockInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(SessionDisplayMockInput, value));
	}
};

namespace
{
class SessionDisplayMockAllState
{
public:
	const SessionDisplayMockState& getState() const { return m_state; }
	SessionDisplayMockState&       editState()       { return m_state; }

private:
	SessionDisplayMockState m_state;
};

struct SessionDisplayMockSimulatable
{
	using StateType = SessionDisplayMockState;
	using InputType = SessionDisplayMockInput;

	SessionDisplayMockAllState m_allState;
	SessionDisplayMockAllState m_vizState;

	const SessionDisplayMockAllState& getAllState() const { return m_allState; }
	SessionDisplayMockAllState&       editAllState()       { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const SessionDisplayMockAllState& getVizState() const { return m_vizState; }
	SessionDisplayMockAllState&       editVizState()       { return m_vizState; }
};

using MockStorage        = SimulationObjectStorage<SessionDisplayMockSimulatable>;
using MockReconciliation = SimulationReconciliation<SessionDisplayMockSimulatable>;
using MockResolution     = SimulationInputResolution<SessionDisplayMockSimulatable>;
using MockSnapshot       = SimulationDisplaySnapshotT<SessionDisplayMockSimulatable, 4u, 16u, 8u>;

struct FakeDisplayManager
{
	TimeConfig            config;
	NetworkTimeEstimator  estimator{ config, nullptr };
	ClientPredictionClock clock{ config, estimator, nullptr };

	const ClientPredictionClock& getClientClock() const { return clock; }
	const NetworkTimeEstimator&  getNetworkEstimator() const { return estimator; }
};

struct FakeSystems
{
	std::unordered_map<unsigned int, std::int32_t> scores;
};

struct FakeDisplayGame
{
	struct AuthorityPresentationSlice
	{
		std::int32_t score = 0;
	};

	static AuthorityPresentationSlice authorityPresentationSlice_Step(const FakeSystems& systems, unsigned int id)
	{
		const auto found = systems.scores.find(id);
		return AuthorityPresentationSlice{ found != systems.scores.end() ? found->second : -1 };
	}
};

using Display = ogsession::SessionDisplay<FakeDisplayGame, MockSnapshot, FakeDisplayManager, MockStorage, MockResolution,
	MockReconciliation>;

// One thread that runs the jobs it is given, one at a time, each to completion before run() returns.
class ThreadWorker
{
public:
	ThreadWorker()
		: m_thread([this] { loop(); })
	{
	}

	~ThreadWorker()
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		m_thread.join();
	}

	ThreadWorker(const ThreadWorker&) = delete;
	ThreadWorker& operator=(const ThreadWorker&) = delete;

	std::thread::id id() const { return m_thread.get_id(); }

	void run(const std::function<void()>& job)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_job = &job;
		m_wake.notify_all();
		m_wake.wait(lock, [this] { return m_job == nullptr; });
	}

private:
	void loop()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (true)
		{
			m_wake.wait(lock, [this] { return m_job != nullptr || m_stop; });
			if (m_job != nullptr)
			{
				const std::function<void()>* const job = m_job;
				lock.unlock();
				(*job)();
				lock.lock();
				m_job = nullptr;
				m_wake.notify_all();
				continue;
			}
			return;
		}
	}

	std::mutex                   m_mutex;
	std::condition_variable      m_wake;
	const std::function<void()>* m_job  = nullptr;
	bool                         m_stop = false;
	std::thread                  m_thread;
};

struct DisplaySources
{
	FakeDisplayManager               manager;
	MockStorage                      storage;
	MockReconciliation               reconciliation{ storage };
	MockResolution                   resolution{ storage, reconciliation };
	TripleBuffer<DisplayTierReading> tier;

	void addSimulatables(unsigned int count)
	{
		for (unsigned int id = 1u; id <= count; ++id)
			storage.add<SessionDisplayMockSimulatable>(id, SessionDisplayMockSimulatable{});
	}

	void setPositions(std::int32_t base)
	{
		storage.forEachSimulatable<SessionDisplayMockSimulatable>([base](unsigned int id, SessionDisplayMockSimulatable& simulatable)
		{
			simulatable.editAllState().editState().position = base + static_cast<std::int32_t>(id);
		});
	}
};

// A client: this thread is the frame thread; the step runs on a worker inside a StepScope of the client's context.
struct PredictionRig
{
	PredictionRig()
	{
		context.configure<true>(tickDriverThread::kClientRoleBit, StepThreading::FrameBounded);
		display.emplace(ogsession::SessionThreadChecks(ogsession::SessionRole::Prediction, &context, std::this_thread::get_id()),
			sources.manager, sources.storage, sources.resolution, sources.reconciliation, sources.tier);
	}

	template <typename F>
	void onStep(F&& f)
	{
		step.run([this, &f]
		{
			const StepScope scope(context);
			f();
		});
	}

	void publishBatch(std::int32_t base)
	{
		onStep([this, base]
		{
			sources.setPositions(base);
			display->publishAfterBatch();
		});
	}

	void acquire()
	{
		display->acquireForFrame([](unsigned int, SessionDisplayMockAllState& vizState) { vizState.editState().position += 1000; });
	}

	TickDriverThreadContext context;
	DisplaySources          sources;
	ThreadWorker            step;
	std::optional<Display>  display;
};

// An authority: this thread configured its context, so it is the runner; the frame thread is a worker.
struct AuthorityRig
{
	AuthorityRig()
	{
		context.configure<true>(tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning);
		display.emplace(ogsession::SessionThreadChecks(ogsession::SessionRole::Authority, &context, frame.id()),
			sources.manager, sources.storage, sources.resolution, sources.reconciliation, sources.tier);
	}

	std::vector<std::pair<unsigned int, std::int32_t>> presentOnFrame()
	{
		std::vector<std::pair<unsigned int, std::int32_t>> presented;
		frame.run([this, &presented]
		{
			display->presentLatest([&presented](unsigned int id, const FakeDisplayGame::AuthorityPresentationSlice& slice)
			{
				presented.emplace_back(id, slice.score);
			});
		});
		return presented;
	}

	TickDriverThreadContext context;
	DisplaySources          sources;
	FakeSystems             systems;
	ThreadWorker            frame;
	std::optional<Display>  display;
};
} // namespace

TEST_CASE("SessionDisplay.NothingIsVisibleBeforeTheFirstPublish", "[Session][SessionDisplay]")
{
	PredictionRig client;
	client.sources.addSimulatables(2u);
	client.acquire();
	CHECK(client.display->snapshot() == nullptr);
	CHECK(client.display->findEntry(1u) == nullptr);
	CHECK(client.display->findVizState(1u) == nullptr);
	int visited = 0;
	client.display->forEachVizState([&visited](unsigned int, const SessionDisplayMockAllState&) { ++visited; });
	CHECK(visited == 0);

	AuthorityRig authority;
	authority.sources.addSimulatables(2u);
	CHECK(authority.presentOnFrame().empty());
}

TEST_CASE("SessionDisplay.TheFrameSeesTheBatchsState", "[Session][SessionDisplay]")
{
	PredictionRig rig;
	rig.sources.addSimulatables(2u);
	for (int advance = 0; advance < 5; ++advance)
		rig.sources.manager.clock.advancePrediction();
	rig.sources.tier.write() = DisplayTierReading{ true, 2, 3, 4, 5 };
	rig.sources.tier.publish();

	rig.publishBatch(10);
	rig.acquire();

	const MockSnapshot* const snapshot = rig.display->snapshot();
	REQUIRE(snapshot != nullptr);
	CHECK(snapshot->entryCount == 2u);
	CHECK(snapshot->clock.predictionTick == rig.sources.manager.clock.getPredictionTick());
	CHECK(snapshot->clock.predictionTick > 0u);
	CHECK(snapshot->tier.hasReceivedTier);
	CHECK(snapshot->tier.tierIndex == 2);
	CHECK(snapshot->tier.clientEffectiveInputDelayTicks == 5);

	for (unsigned int id = 1u; id <= 2u; ++id)
	{
		const MockSnapshot::Entry* const entry = rig.display->findEntry(id);
		REQUIRE(entry != nullptr);
		CHECK(entry->vizState.getState().position == 10 + static_cast<std::int32_t>(id));
		const SessionDisplayMockAllState* const vizState = rig.display->findVizState(id);
		REQUIRE(vizState != nullptr);
		CHECK(vizState->getState().position == 1010 + static_cast<std::int32_t>(id));
	}
	CHECK(rig.display->findEntry(9u) == nullptr);
	CHECK(rig.display->findVizState(9u) == nullptr);

	std::vector<std::pair<unsigned int, std::int32_t>> visited;
	rig.display->forEachVizState([&visited](unsigned int id, const SessionDisplayMockAllState& vizState)
	{
		visited.emplace_back(id, vizState.getState().position);
	});
	REQUIRE(visited.size() == 2u);
	CHECK(visited[0].second == 1000 + 10 + static_cast<std::int32_t>(visited[0].first));
	CHECK(visited[1].second == 1000 + 10 + static_cast<std::int32_t>(visited[1].first));
}

TEST_CASE("SessionDisplay.TheNewestPublishWins", "[Session][SessionDisplay]")
{
	PredictionRig rig;
	rig.sources.addSimulatables(1u);

	rig.publishBatch(100);
	rig.publishBatch(200);
	rig.publishBatch(300);
	rig.acquire();
	REQUIRE(rig.display->findEntry(1u) != nullptr);
	CHECK(rig.display->findEntry(1u)->vizState.getState().position == 301);
	const std::uint64_t held = rig.display->snapshot()->publishSequence;

	rig.acquire();
	CHECK(rig.display->snapshot()->publishSequence == held);

	for (std::int32_t batch = 4; batch <= 12; ++batch)
	{
		rig.publishBatch(batch * 100);
		rig.acquire();
		REQUIRE(rig.display->findEntry(1u) != nullptr);
		CHECK(rig.display->findEntry(1u)->vizState.getState().position == batch * 100 + 1);
		CHECK(rig.display->findVizState(1u)->getState().position == batch * 100 + 1001);
	}

	rig.display->releaseHeld();
	CHECK(rig.display->snapshot() == nullptr);
	rig.publishBatch(1300);
	rig.acquire();
	REQUIRE(rig.display->findEntry(1u) != nullptr);
	CHECK(rig.display->findEntry(1u)->vizState.getState().position == 1301);
}

TEST_CASE("SessionDisplay.APublishOnOneThreadIsAcquiredOnAnother", "[Session][SessionDisplay]")
{
	PredictionRig rig;
	rig.sources.addSimulatables(2u);
	constexpr std::int32_t kBatches = 2000;
	std::atomic<bool> writerDone{ false };

	std::thread writer([&rig, &writerDone]
	{
		const StepScope scope(rig.context);
		for (std::int32_t batch = 1; batch <= kBatches; ++batch)
		{
			rig.sources.setPositions(batch * 10);
			rig.display->publishAfterBatch();
		}
		writerDone.store(true);
	});

	std::uint64_t lastSequence = 0u;
	std::int64_t  acquires     = 0;
	bool          consistent   = true;
	bool          ordered      = true;
	while (!writerDone.load())
	{
		rig.acquire();
		++acquires;
		const MockSnapshot* const snapshot = rig.display->snapshot();
		if (snapshot == nullptr)
			continue;
		ordered = ordered && snapshot->publishSequence >= lastSequence;
		lastSequence = snapshot->publishSequence;
		const MockSnapshot::Entry* const first  = rig.display->findEntry(1u);
		const MockSnapshot::Entry* const second = rig.display->findEntry(2u);
		consistent = consistent && first != nullptr && second != nullptr
			&& second->vizState.getState().position == first->vizState.getState().position + 1;
	}
	writer.join();

	CHECK(consistent);
	CHECK(ordered);
	CHECK(acquires > 0);
	rig.acquire();
	REQUIRE(rig.display->findEntry(1u) != nullptr);
	CHECK(rig.display->snapshot()->publishSequence == static_cast<std::uint64_t>(kBatches));
	CHECK(rig.display->findEntry(1u)->vizState.getState().position == kBatches * 10 + 1);
}

TEST_CASE("SessionDisplay.ThePresentationIsTheRunnersNewestPublishAppliedOnTheFrame", "[Session][SessionDisplay]")
{
	AuthorityRig rig;
	rig.sources.addSimulatables(3u);
	rig.systems.scores = { { 1u, 7 }, { 2u, 8 } };

	rig.display->publishPresentation(rig.systems);
	const auto first = rig.presentOnFrame();
	REQUIRE(first.size() == 3u);
	for (const auto& [id, score] : first)
		CHECK(score == (id == 1u ? 7 : id == 2u ? 8 : -1));

	rig.systems.scores = { { 1u, 70 }, { 2u, 80 }, { 3u, 90 } };
	rig.display->publishPresentation(rig.systems);
	rig.systems.scores = { { 1u, 71 }, { 2u, 81 }, { 3u, 91 } };
	rig.display->publishPresentation(rig.systems);
	const auto newest = rig.presentOnFrame();
	REQUIRE(newest.size() == 3u);
	for (const auto& [id, score] : newest)
		CHECK(score == 60 + 10 * static_cast<std::int32_t>(id) + 1);
}

TEST_CASE("SessionDisplay.ThePresentationHoldsAsManyEntriesAsTheSnapshot", "[Session][SessionDisplay]")
{
	AuthorityRig rig;
	rig.sources.addSimulatables(static_cast<unsigned int>(Display::kMaxEntries));
	rig.display->publishPresentation(rig.systems);
	CHECK(rig.presentOnFrame().size() == Display::kMaxEntries);
	CHECK(Display::kMaxEntries == MockSnapshot::kMaxSimulatables);
}

TEST_CASE("SessionDisplay.APresentationOverItsEntryCountTripsTheCheck", "[.][SessionDisplayDeath]")
{
	AuthorityRig rig;
	rig.sources.addSimulatables(static_cast<unsigned int>(Display::kMaxEntries) + 1u);
	rig.display->publishPresentation(rig.systems);
	FAIL("SessionDisplay::publishPresentation over kMaxEntries simulatables did NOT fire its entry-count check");
}

TEST_CASE("SessionDisplay.ADisplayPublishOnTheFrameThreadTripsTheCheck", "[.][SessionDisplayDeath]")
{
	PredictionRig rig;
	rig.display->publishAfterBatch();
	FAIL("SessionDisplay::publishAfterBatch on a client's frame thread did NOT fire the thread check");
}

TEST_CASE("SessionDisplay.AnAuthoritysDisplayPublishTripsTheRoleCheck", "[.][SessionDisplayDeath]")
{
	AuthorityRig rig;
	rig.display->publishAfterBatch();
	FAIL("SessionDisplay::publishAfterBatch on an authority's runner did NOT fire the role check");
}
