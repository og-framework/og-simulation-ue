// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetVersion.h"
#include "OGSimSession/SessionEngine.h"
#include "OGSimSession/SessionGame.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "SessionTestKit.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
using ogsession::sessionTestKit::FakeSessionEngine;
using ogsession::sessionTestKit::FakeSessionRunner;
using ogsession::sessionTestKit::MapConfigSource;

const char* kindName(const ogsession::LifecycleOutcome& outcome)
{
	struct Name
	{
		const char* operator()(const ogsession::lifecycleOutcome::Registered&) const { return "Registered"; }
		const char* operator()(const ogsession::lifecycleOutcome::Refused&) const { return "Refused"; }
		const char* operator()(const ogsession::lifecycleOutcome::Unregistered&) const { return "Unregistered"; }
		const char* operator()(const ogsession::lifecycleOutcome::RosterRemoved&) const { return "RosterRemoved"; }
		const char* operator()(const ogsession::lifecycleOutcome::BodiesReleasable&) const { return "BodiesReleasable"; }
		const char* operator()(const ogsession::lifecycleOutcome::BodiesBound&) const { return "BodiesBound"; }
		const char* operator()(const ogsession::lifecycleOutcome::BodiesPending&) const { return "BodiesPending"; }
		const char* operator()(const ogsession::lifecycleOutcome::BodiesUnresolvable&) const { return "BodiesUnresolvable"; }
	};
	return std::visit(Name{}, outcome);
}
} // namespace

TEST_CASE("SessionSkeleton.TheModuleReachesOgSimAndOgSimNet", "[Session]")
{
	STATIC_REQUIRE(ogsession::SessionEngine<ogsession::sessionEngineSelfCheck::Conforming>);
	STATIC_REQUIRE(ogsession::SessionGameHooks<ogsession::sessionGameHooksSelfCheck::Conforming,
		ogsession::sessionGameSelfCheck::Conforming>);
	STATIC_REQUIRE(ogsession::ConfigSource<MapConfigSource>);
	STATIC_REQUIRE(ogsession::SessionEngine<FakeSessionEngine>);

	CHECK(std::string(ogsimnet::kWireTokenFragment.view()) == "ro1.ck2");
	CHECK(std::string(ogsession::sessionGameSelfCheck::Conforming::kWireTokenFragment.view()) == "g1");
}

TEST_CASE("SessionSkeleton.FakeSessionEngineRoutesLinesInOrder", "[Session]")
{
	FakeSessionEngine engine;
	engine.logRouted("[Warning][Session.A] first");
	engine.logRouted("[Log][Session.B] second");

	const std::vector<std::string> lines = engine.routedLines();
	REQUIRE(lines.size() == 2u);
	CHECK(lines[0] == "[Warning][Session.A] first");
	CHECK(lines[1] == "[Log][Session.B] second");
	CHECK(engine.lines.empty());
}

TEST_CASE("SessionSkeleton.FakeSessionEngineRunsTheRunnerBodyOnItsOwnThreadUntilJoined", "[Session]")
{
	FakeSessionEngine engine;
	std::atomic<bool> bodyStarted{ false };
	std::atomic<uint32_t> polls{ 0u };
	std::thread::id bodyThread;
	std::thread::id idSeenByBody;
	FakeSessionRunner* runnerSeenByBody = nullptr;

	std::unique_ptr<FakeSessionRunner> runner = engine.startRunner(
		[&](FakeSessionRunner& self)
		{
			idSeenByBody = self.threadId();
			bodyThread = std::this_thread::get_id();
			runnerSeenByBody = &self;
			bodyStarted.store(true, std::memory_order_release);
			while (!self.stopRequested().load(std::memory_order_acquire))
			{
				polls.fetch_add(1u, std::memory_order_relaxed);
				std::this_thread::yield();
			}
		},
		/*highResolutionWait=*/true);

	REQUIRE(runner != nullptr);
	while (!bodyStarted.load(std::memory_order_acquire))
		std::this_thread::yield();
	CHECK_FALSE(runner->stopRequested().load());

	runner->join();

	CHECK(runner->stopRequested().load());
	CHECK(bodyThread != std::this_thread::get_id());
	CHECK(bodyThread == runner->threadId());
	CHECK(idSeenByBody == bodyThread);
	CHECK(runnerSeenByBody == runner.get());
	CHECK(runner->highResolutionWait());
	CHECK(engine.runnersStarted.load() == 1u);
	CHECK(polls.load() >= 1u);
}

TEST_CASE("SessionSkeleton.TheFakeRunnersHostSleepsTheFakeEngineToTheDeadline", "[Session]")
{
	FakeSessionEngine engine;
	std::unique_ptr<FakeSessionRunner> runner = engine.startRunner(
		[](FakeSessionRunner& self) { self.host().sleepUntil(2.5); }, /*highResolutionWait=*/false);
	runner->join();

	CHECK(engine.nowSeconds() == 2.5);
	REQUIRE(runner->host().sleeps.size() == 1u);
	CHECK(runner->host().sleeps[0] == 2.5);
	CHECK_FALSE(runner->highResolutionWait());
}

TEST_CASE("SessionSkeleton.MapConfigSourceReadsWholeValuesAndReportsAbsence", "[Session]")
{
	const MapConfigSource config({ { "Floor", "3" }, { "Negative", "-7" }, { "Word", "On" }, { "Gain", "0.25" },
		{ "Trailing", "4x" }, { "TooLarge", "2147483648" }, { "Empty", "" } });

	CHECK(config.readInt("Floor") == 3);
	CHECK(config.readInt("Negative") == -7);
	CHECK_FALSE(config.readInt("Word").has_value());
	CHECK_FALSE(config.readInt("Trailing").has_value());
	CHECK_FALSE(config.readInt("TooLarge").has_value());
	CHECK_FALSE(config.readInt("Missing").has_value());

	CHECK(config.readDouble("Gain") == 0.25);
	CHECK(config.readDouble("Floor") == 3.0);
	CHECK_FALSE(config.readDouble("Word").has_value());
	CHECK_FALSE(config.readDouble("Empty").has_value());
	CHECK_FALSE(config.readDouble("Missing").has_value());

	CHECK(config.readString("Word") == std::string("On"));
	CHECK(config.readString("Empty") == std::string());
	CHECK_FALSE(config.readString("Missing").has_value());
}

TEST_CASE("SessionSkeleton.TheSessionTypesDefaultToNothingRegistered", "[Session]")
{
	const ogsession::SessionStart start;
	CHECK(start.role == ogsession::SessionRole::Prediction);
	CHECK(start.stepSeconds == 0.0);
	CHECK(start.requestedMaxCatchUpSteps == 0);
	CHECK_FALSE(start.highResolutionWait);
	CHECK(start.staticWorld.shapes.empty());
	CHECK(start.physicsOnlyShapes.empty());

	const ogsession::RegisterRequest request;
	CHECK(request.simId == 0u);
	CHECK(request.entityKey == ogsession::EntityKey{});
	CHECK(request.ownerConnection == kInvalidSimConnection);
	CHECK_FALSE(request.engineSaysLocal);

	CHECK(ogsession::EntityKey{ 42u } == ogsession::EntityKey{ 42u });
	CHECK_FALSE(ogsession::EntityKey{ 42u } == ogsession::EntityKey{ 43u });
}

TEST_CASE("SessionSkeleton.ALifecycleOutcomeHoldsEachKind", "[Session]")
{
	using namespace ogsession::lifecycleOutcome;
	const std::vector<ogsession::LifecycleOutcome> outcomes{ Registered{ 3u, 1u }, Refused{ 4u, ogsession::RegisterRefusal::SimNetRefused },
		Unregistered{ 5u }, RosterRemoved{ 6u }, BodiesReleasable{ ogsession::EntityKey{ 77u } }, BodiesBound{ 7u, 2u },
		BodiesPending{ 8u }, BodiesUnresolvable{ 9u } };

	std::vector<std::string> names;
	for (const ogsession::LifecycleOutcome& outcome : outcomes)
		names.emplace_back(kindName(outcome));
	const std::vector<std::string> expected{ "Registered", "Refused", "Unregistered", "RosterRemoved", "BodiesReleasable",
		"BodiesBound", "BodiesPending", "BodiesUnresolvable" };
	CHECK(names == expected);

	CHECK(std::get<Registered>(outcomes[0]).slot == 1u);
	CHECK(std::get<Refused>(outcomes[1]).reason == ogsession::RegisterRefusal::SimNetRefused);
	CHECK(std::get<BodiesReleasable>(outcomes[4]).entityKey == ogsession::EntityKey{ 77u });
	CHECK(std::get<BodiesBound>(outcomes[5]).slot == 2u);
}
