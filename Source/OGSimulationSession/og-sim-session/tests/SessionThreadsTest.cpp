// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <thread>

namespace
{
using ogsession::kSessionOpCount;
using ogsession::SessionOp;
using ogsession::SessionRole;
using ogsession::SessionThread;
using ogsession::SessionThreadChecks;

struct ExpectedRow
{
	SessionOp                    op;
	const char*                  name;
	std::optional<SessionThread> prediction;
	std::optional<SessionThread> authority;
};

constexpr std::array<ExpectedRow, kSessionOpCount> kExpected{ {
	{ SessionOp::SampleInput,            "SampleInput",            SessionThread::Frame, std::nullopt },
	{ SessionOp::RateDoor,               "RateDoor",               SessionThread::Frame, SessionThread::Step },
	{ SessionOp::TryRegister,            "TryRegister",            SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::RequestUnregister,      "RequestUnregister",      SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::PollOutcomes,           "PollOutcomes",           SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::AcquireDisplay,         "AcquireDisplay",         SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::Present,                "Present",                SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::DrainInbox,             "DrainInbox",             SessionThread::Step,  SessionThread::Step },
	{ SessionOp::ApplyLifecycleRequests, "ApplyLifecycleRequests", SessionThread::Step,  SessionThread::Step },
	{ SessionOp::Capture,                "Capture",                SessionThread::Step,  SessionThread::Step },
	{ SessionOp::Step,                   "Step",                   SessionThread::Step,  SessionThread::Step },
	{ SessionOp::Send,                   "Send",                   SessionThread::Step,  SessionThread::Step },
	{ SessionOp::Publish,                "Publish",                SessionThread::Step,  SessionThread::Step },
	{ SessionOp::ServiceNet,             "ServiceNet",             SessionThread::NetIo, SessionThread::NetIo },
	{ SessionOp::LinkFrameService,       "LinkFrameService",       SessionThread::Frame, SessionThread::Frame },
	{ SessionOp::DrainStopped,           "DrainStopped",           SessionThread::Frame, std::nullopt },
	{ SessionOp::FrameProbe,             "FrameProbe",             SessionThread::Frame, SessionThread::Step },
} };

using Allowed = std::array<bool, kSessionOpCount>;

Allowed allowedHere(const SessionThreadChecks& checks)
{
	Allowed allowed{};
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
		allowed[index] = checks.isAllowed(static_cast<SessionOp>(index));
	return allowed;
}

Allowed allowedOnAPlainThread(const SessionThreadChecks& checks)
{
	Allowed allowed{};
	std::thread thread([&] { allowed = allowedHere(checks); });
	thread.join();
	return allowed;
}

Allowed allowedInAStep(const SessionThreadChecks& checks, const TickDriverThreadContext& context)
{
	Allowed allowed{};
	std::thread thread(
		[&]
		{
			const StepScope step(context);
			allowed = allowedHere(checks);
		});
	thread.join();
	return allowed;
}

struct PredictionRig
{
	PredictionRig() { context.configure<true>(tickDriverThread::kClientRoleBit, StepThreading::FrameBounded); }

	TickDriverThreadContext context;
	SessionThreadChecks     checks{ SessionRole::Prediction, &context, std::this_thread::get_id() };
};

struct AuthorityRig
{
	template <typename RunnerWorkT>
	void onTheRunner(RunnerWorkT&& work)
	{
		std::thread runner(
			[&]
			{
				context.configure<true>(tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning);
				work();
			});
		runner.join();
	}

	Allowed allowedOnTheRunner()
	{
		Allowed allowed{};
		onTheRunner([&] { allowed = allowedHere(checks); });
		return allowed;
	}

	TickDriverThreadContext context;
	SessionThreadChecks     checks{ SessionRole::Authority, &context, std::this_thread::get_id() };
};
} // namespace

TEST_CASE("SessionThreads.TheTablesAreTheDesignsTable", "[Session]")
{
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
	{
		const ExpectedRow& row = kExpected[index];
		INFO(row.name);
		REQUIRE(static_cast<std::size_t>(row.op) == index);
		CHECK(ogsession::kPredictionThreads[index].op == row.op);
		CHECK(ogsession::kAuthorityThreads[index].op == row.op);
		CHECK(ogsession::kPredictionThreads[index].thread == row.prediction);
		CHECK(ogsession::kAuthorityThreads[index].thread == row.authority);
	}
}

TEST_CASE("SessionThreads.APredictionOpPassesOnItsThreadAndOnNoOther", "[Session]")
{
	PredictionRig rig;
	const Allowed onFrame = allowedHere(rig.checks);
	const Allowed inStep  = allowedInAStep(rig.checks, rig.context);
	const Allowed onNetIo = allowedOnAPlainThread(rig.checks);

	uint32_t allowedCases = 0u;
	uint32_t forbiddenCases = 0u;
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
	{
		const ExpectedRow& row = kExpected[index];
		INFO(row.name);
		REQUIRE(row.prediction.has_value());
		CHECK(onFrame[index] == (*row.prediction == SessionThread::Frame));
		CHECK(inStep[index] == (*row.prediction == SessionThread::Step));
		CHECK(onNetIo[index] == (*row.prediction == SessionThread::NetIo));
		allowedCases += static_cast<uint32_t>(onFrame[index]) + inStep[index] + onNetIo[index];
		forbiddenCases += static_cast<uint32_t>(!onFrame[index]) + !inStep[index] + !onNetIo[index];
	}
	CHECK(allowedCases == kSessionOpCount);
	CHECK(forbiddenCases == 2u * kSessionOpCount);
}

TEST_CASE("SessionThreads.AnAuthorityOpPassesOnItsThreadAndOnNoOther", "[Session]")
{
	AuthorityRig rig;
	const Allowed onRunner = rig.allowedOnTheRunner();
	const Allowed onFrame  = allowedHere(rig.checks);
	const Allowed onOther  = allowedOnAPlainThread(rig.checks);

	uint32_t allowedCases = 0u;
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
	{
		const ExpectedRow& row = kExpected[index];
		INFO(row.name);
		const bool frameRow = row.authority == SessionThread::Frame;
		const bool runnerRow = row.authority == SessionThread::Step || row.authority == SessionThread::NetIo;
		CHECK(onFrame[index] == frameRow);
		CHECK(onRunner[index] == runnerRow);
		CHECK_FALSE(onOther[index]);
		allowedCases += static_cast<uint32_t>(onFrame[index]) + onRunner[index] + onOther[index];
	}
	CHECK(allowedCases == kSessionOpCount - 2u);
	CHECK_FALSE(onFrame[static_cast<std::size_t>(SessionOp::SampleInput)]);
	CHECK_FALSE(onRunner[static_cast<std::size_t>(SessionOp::SampleInput)]);
	CHECK_FALSE(onFrame[static_cast<std::size_t>(SessionOp::DrainStopped)]);
	CHECK_FALSE(onRunner[static_cast<std::size_t>(SessionOp::DrainStopped)]);
}

TEST_CASE("SessionThreads.TheStoppedDrainIsAClientsFrameRowAndNoStepsOrAuthoritys", "[Session]")
{
	PredictionRig client;
	const std::size_t stopped = static_cast<std::size_t>(SessionOp::DrainStopped);
	const std::size_t inbox = static_cast<std::size_t>(SessionOp::DrainInbox);

	const Allowed onFrame = allowedHere(client.checks);
	const Allowed inStep  = allowedInAStep(client.checks, client.context);
	CHECK(onFrame[stopped]);
	CHECK_FALSE(onFrame[inbox]);
	CHECK_FALSE(inStep[stopped]);
	CHECK(inStep[inbox]);
	client.checks.checkOp(SessionOp::DrainStopped);

	AuthorityRig authority;
	CHECK_FALSE(authority.allowedOnTheRunner()[stopped]);
	CHECK_FALSE(allowedHere(authority.checks)[stopped]);
}

TEST_CASE("SessionThreads.TheFrameProbeIsTheClientsFrameAndTheAuthoritysRunner", "[Session]")
{
	const std::size_t probe = static_cast<std::size_t>(SessionOp::FrameProbe);

	PredictionRig client;
	const Allowed clientOnFrame = allowedHere(client.checks);
	const Allowed clientInStep  = allowedInAStep(client.checks, client.context);
	CHECK(clientOnFrame[probe]);
	CHECK_FALSE(clientInStep[probe]);
	client.checks.checkOp(SessionOp::FrameProbe);

	AuthorityRig authority;
	CHECK(authority.allowedOnTheRunner()[probe]);
	CHECK_FALSE(allowedHere(authority.checks)[probe]);
	CHECK_FALSE(allowedOnAPlainThread(authority.checks)[probe]);
}

TEST_CASE("SessionThreads.WithoutTheRolesContextOnlyTheFrameRowsPass", "[Session]")
{
	const SessionThreadChecks noContext(SessionRole::Prediction, nullptr, std::this_thread::get_id());
	TickDriverThreadContext clientContext;
	clientContext.configure<true>(tickDriverThread::kClientRoleBit, StepThreading::FrameBounded);
	const SessionThreadChecks otherRole(SessionRole::Authority, &clientContext, std::this_thread::get_id());

	const Allowed noContextInStep = allowedInAStep(noContext, clientContext);
	const Allowed noContextOffFrame = allowedOnAPlainThread(noContext);
	const Allowed otherRoleInStep = allowedInAStep(otherRole, clientContext);
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
	{
		INFO(kExpected[index].name);
		CHECK_FALSE(noContextInStep[index]);
		CHECK_FALSE(noContextOffFrame[index]);
		CHECK_FALSE(otherRoleInStep[index]);
	}
	CHECK(noContext.isAllowed(SessionOp::SampleInput));
	CHECK_FALSE(noContext.isAllowed(SessionOp::DrainInbox));
}

TEST_CASE("SessionThreads.CheckOpIsSilentOnTheRowsThreadAndTheSentinelHasNoRow", "[Session]")
{
	PredictionRig rig;
	rig.checks.checkOp(SessionOp::SampleInput);
	rig.checks.checkOp(SessionOp::TryRegister);
	rig.checks.checkOp(SessionOp::LinkFrameService);
	CHECK(rig.checks.role() == SessionRole::Prediction);

	CHECK_FALSE(rig.checks.threadOf(SessionOp::Count).has_value());
	CHECK_FALSE(rig.checks.isAllowed(SessionOp::Count));
}

TEST_CASE("SessionThreads.DrainInboxOnTheFrameTripsTheCheck", "[.][SessionThreadsDeath]")
{
	PredictionRig rig;
	rig.checks.checkOp(SessionOp::DrainInbox);
	FAIL("SessionThreadChecks::checkOp(DrainInbox) on the frame thread did NOT fire - the check is vacuous in this target");
}

TEST_CASE("SessionThreads.DrainStoppedOnTheAuthorityTripsTheCheck", "[.][SessionThreadsDeath]")
{
	AuthorityRig rig;
	rig.checks.checkOp(SessionOp::DrainStopped);
	FAIL("SessionThreadChecks::checkOp(DrainStopped) on an authority's frame thread did NOT fire - the authority has no "
		 "stopped drain");
}

TEST_CASE("SessionThreads.FrameProbeOnAnAuthoritysFrameThreadTripsTheCheck", "[.][SessionThreadsDeath]")
{
	AuthorityRig rig;
	rig.onTheRunner([] {});
	REQUIRE(rig.context.roleBit() == tickDriverThread::kAuthorityRoleBit);
	rig.checks.checkOp(SessionOp::FrameProbe);
	FAIL("SessionThreadChecks::checkOp(FrameProbe) on an authority's frame thread did NOT fire - the authority's frame "
		 "probe runs on its runner");
}

TEST_CASE("SessionThreads.WithOgNetsIoThreadKnownAPredictionNetIoOpPassesOnThatThreadAlone", "[Session]")
{
	PredictionRig rig;
	std::thread::id netIoThread;
	Allowed onTheNetIoThread{};
	std::mutex gate;
	std::condition_variable known;
	bool idKnown = false;
	bool checked = false;
	std::thread io(
		[&]
		{
			{
				const std::lock_guard<std::mutex> lock(gate);
				netIoThread = std::this_thread::get_id();
				idKnown = true;
			}
			known.notify_all();
			std::unique_lock<std::mutex> lock(gate);
			known.wait(lock, [&] { return checked; });
			onTheNetIoThread = allowedHere(rig.checks);
		});
	{
		std::unique_lock<std::mutex> lock(gate);
		known.wait(lock, [&] { return idKnown; });
	}
	rig.checks.setNetIoThread(netIoThread);
	{
		const std::lock_guard<std::mutex> lock(gate);
		checked = true;
	}
	known.notify_all();
	io.join();
	const Allowed onAnotherThread = allowedOnAPlainThread(rig.checks);
	const Allowed onFrame = allowedHere(rig.checks);
	const std::size_t serviceNet = static_cast<std::size_t>(SessionOp::ServiceNet);
	CHECK(onTheNetIoThread[serviceNet]);
	CHECK_FALSE(onAnotherThread[serviceNet]);
	CHECK_FALSE(onFrame[serviceNet]);
	uint32_t allowedElsewhere = 0u;
	for (std::size_t index = 0u; index < kSessionOpCount; ++index)
		allowedElsewhere += static_cast<uint32_t>(onAnotherThread[index]);
	CHECK(allowedElsewhere == 0u);
}
