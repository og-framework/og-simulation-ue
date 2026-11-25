// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTickGame.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/SimulationScheduler.h"
#include "OGTickDriver/TickDriverConcepts.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
using Events = std::vector<std::string>;

std::string formatSeconds(const char* pattern, double value)
{
	char line[96];
	std::snprintf(line, sizeof(line), pattern, value);
	return line;
}

std::string format(const char* pattern, long long first, long long second = 0)
{
	char line[96];
	std::snprintf(line, sizeof(line), pattern, first, second);
	return line;
}

struct FakeTimeConfig
{
};

struct FakeNetSync
{
};

struct FakeServerClock
{
	uint32_t tick = 0u;
	uint32_t getTick() const { return tick; }
};

struct FakeManager
{
	Events*         events = nullptr;
	bool            predicts = false;
	uint32_t        integratedTick = 0u;
	FakeTimeConfig  timeConfig;
	FakeServerClock serverClock;

	bool                   runsPrediction() const { return predicts; }
	uint32_t               currentIntegratedTick() const { return integratedTick; }
	const FakeTimeConfig&  getTimeConfig() const { return timeConfig; }
	const FakeServerClock& getServerClock() const { return serverClock; }
	void onPostSimulationGameThread() { events->push_back("manager.onPostSimulationGameThread"); }
};

struct FakeSimulatable
{
};

struct FakeStorage
{
	std::vector<unsigned int> ids;

	template <typename SimT, typename F>
	void forEachSimulatable(F&& f)
	{
		const FakeSimulatable simulatable;
		for (const unsigned int id : ids)
			f(id, simulatable);
	}
};

struct FakeRenderSnapshot
{
};

struct FakeClockServer
{
};

struct FakeClock
{
	Events* events = nullptr;

	void beforePredictionPump(double, TickRateDoor&) { events->push_back("clock.beforePredictionPump"); }
	void noteAuthorityRateDoor(double, const TickRateDoor&) { events->push_back("clock.noteAuthorityRateDoor"); }
	void noteAuthorityStepStart() { events->push_back("clock.noteAuthorityStepStart"); }
	std::optional<double> takeReleaseStepSeconds()
	{
		events->push_back("clock.takeReleaseStepSeconds");
		return 0.25;
	}
	void publishServerTick(FakeClockServer* server)
	{
		events->push_back(server != nullptr ? "clock.publishServerTick server=set" : "clock.publishServerTick server=null");
	}
};

struct FakeDisplay
{
	Events* events = nullptr;
	void publishAfterBatch() { events->push_back("display.publishAfterBatch"); }
};

struct FakeDiagnostics
{
	Events* events = nullptr;
	void stampAfterStep_Internal(double seconds) { events->push_back(formatSeconds("diag.stampAfterStep %.2f", seconds)); }
	void stampStepEnd_Internal() { events->push_back("diag.stampStepEnd"); }
	void stampCorrectionHanded_Internal(unsigned int lane, uint32_t tick, double)
	{
		events->push_back(format("diag.stampCorrectionHanded id=%lld tick=%lld", lane, tick));
	}
};

struct FakeClient
{
	Events* events = nullptr;
	void afterCaptureTick_Step(FakeNetSync&, uint32_t tick, const FakeTimeConfig&)
	{
		events->push_back(format("client.afterCaptureTick tick=%lld", tick));
	}
};

struct FakeLanes
{
	using Client = FakeClient;

	Events*    events = nullptr;
	FakeClient boundClient;
	bool       bound = true;

	Client* client() { return &boundClient; }
	bool    clientSessionBound() const { return bound; }
	void    drainClient_Step() { events->push_back("lanes.drainClient"); }
};

struct FakeChannel
{
	CorrectionStateBuffer buffer;
	const CorrectionStateBuffer& getSyncedCorrectionStateBuffer() const { return buffer; }
};

struct FakeServer
{
	Events*                           events = nullptr;
	std::vector<int>                  attached;
	std::map<uint8_t, FakeChannel>    channels;

	size_t drain()
	{
		events->push_back("server.drain");
		return 0u;
	}
	void releaseForStep(int32_t firstTick, int32_t numSteps, std::optional<double> stepSeconds)
	{
		events->push_back(format("server.releaseForStep first=%lld steps=%lld", firstTick, numSteps)
			+ (stepSeconds.has_value() ? formatSeconds(" seconds=%.2f", *stepSeconds) : std::string(" seconds=none")));
	}
	const std::vector<int>& attachedConnections() const { return attached; }
	void publishClock(uint32_t tick) { events->push_back(format("server.publishClock %lld", tick)); }
	void afterInlineBatch(int32_t tick) { events->push_back(format("server.afterInlineBatch %lld", tick)); }

	template <typename SimT>
	FakeChannel* channel(uint8_t simId)
	{
		const auto found = channels.find(simId);
		return found == channels.end() ? nullptr : &found->second;
	}
};

struct FakeIdle
{
	Events* events = nullptr;
	IdleWake idleUntil(double deadlineSeconds)
	{
		events->push_back(formatSeconds("idle.idleUntil %.2f", deadlineSeconds));
		return IdleWake::Deadline;
	}
};

struct Rig;

struct TestPort
{
	using Lanes       = FakeLanes;
	using Server      = FakeServer;
	using Idle        = FakeIdle;
	using Simulatable = FakeSimulatable;

	Rig* rig = nullptr;

	Lanes*  lanes() const;
	Server* server();
	Idle*   idle();
	void    runRunnerRequests();
	void    applyUnregisterRequests(FakeClient&);
	void    publishPresentation();
	double  nowSeconds() const { return 1.0; }
	void    logRouted(const char* line);
};

using NetHooks = ogsession::SessionNetHooks<TestPort, FakeManager, FakeNetSync, FakeStorage, FakeDiagnostics>;

struct TestHost
{
	using Manager        = FakeManager;
	using Storage        = FakeStorage;
	using RenderSnapshot = FakeRenderSnapshot;

	Rig* rig = nullptr;

	Manager&                              manager();
	Storage&                              storage();
	const ogsession::SessionThreadChecks& threads() const;
	FakeClock*                            clock();
	FakeDisplay*                          display();
	NetHooks*                             netHooks();
	FakeClockServer*                      clockServer();
	FakeDiagnostics&                      diagnostics();
	void                                  sampleInput();
	void                                  onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps);
	void                                  onPostPhysics();
};

using Game = ogsession::SessionTickGame<TestHost, FakeDiagnostics>;

static_assert(TickDriverGame<Game> && TickDriverSampleGame<Game> && TickDriverReceiveGame<Game> && TickDriverDisplayGame<Game>
		&& TickDriverIdleGame<Game>,
	"the order test's tick game is the runtime's SessionTickGame over a recording host, and models every hook");

// The two roles on this thread, as the session clock's test runs them: the frame thread is this thread; a client's step
// doors run inside a StepScope of its configured context, an authority's on the thread that configured its context.
struct Rig
{
	explicit Rig(bool predicts)
		: role(predicts ? ogsession::SessionRole::Prediction : ogsession::SessionRole::Authority)
	{
		context.configure<true>(predicts ? tickDriverThread::kClientRoleBit : tickDriverThread::kAuthorityRoleBit,
			predicts ? StepThreading::FrameBounded : StepThreading::FreeRunning);
		threads.emplace(role, &context, std::this_thread::get_id());
		manager.events = &events;
		manager.predicts = predicts;
		manager.integratedTick = 7u;
		manager.serverClock.tick = 42u;
		clock.events = &events;
		display.events = &events;
		diagnostics.events = &events;
		lanes.events = &events;
		lanes.boundClient.events = &events;
		server.events = &events;
		idle.events = &events;
		storage.ids = { 1u, 2u, 3u };
		server.channels[1u].buffer.writeToBuffer(correctionStateBuffer::kTickOffset, uint32_t{ 199u });
		server.channels[1u].buffer.writeToBuffer(correctionStateBuffer::kAppliedCaptureTickOffset, uint32_t{ 0u });
		server.channels[2u].buffer.writeToBuffer(correctionStateBuffer::kTickOffset, uint32_t{ 0u });
		server.channels[2u].buffer.writeToBuffer(correctionStateBuffer::kAppliedCaptureTickOffset, uint32_t{ 0u });
		netHooks.emplace(*threads, TestPort{ this }, manager, netSync, storage, diagnostics);
		game.emplace(TestHost{ this });
	}

	template <typename F>
	void inStep(F&& f)
	{
		if (role == ogsession::SessionRole::Prediction)
		{
			const StepScope step(context);
			f();
		}
		else
		{
			f();
		}
	}

	ogsession::SessionRole                        role;
	TickDriverThreadContext                       context;
	std::optional<ogsession::SessionThreadChecks> threads;
	Events                                        events;
	FakeManager                                   manager;
	FakeStorage                                   storage;
	FakeNetSync                                   netSync;
	FakeClock                                     clock;
	FakeDisplay                                   display;
	FakeDiagnostics                               diagnostics;
	FakeClockServer                               clockServer;
	FakeLanes                                     lanes;
	FakeServer                                    server;
	FakeIdle                                      idle;
	bool                                          hasServer = false;
	std::optional<NetHooks>                       netHooks;
	std::optional<Game>                           game;
};

FakeLanes*  TestPort::lanes() const { return rig->role == ogsession::SessionRole::Prediction ? &rig->lanes : nullptr; }
FakeServer* TestPort::server() { return rig->hasServer ? &rig->server : nullptr; }
FakeIdle*   TestPort::idle() { return rig->hasServer ? &rig->idle : nullptr; }
void        TestPort::runRunnerRequests() { rig->events.push_back("port.runRunnerRequests"); }
void        TestPort::applyUnregisterRequests(FakeClient&) { rig->events.push_back("port.applyUnregisterRequests"); }
void        TestPort::publishPresentation() { rig->events.push_back("port.publishPresentation"); }
void        TestPort::logRouted(const char* line) { rig->events.push_back(std::string("log ") + line); }

FakeManager&                          TestHost::manager() { return rig->manager; }
FakeStorage&                          TestHost::storage() { return rig->storage; }
const ogsession::SessionThreadChecks& TestHost::threads() const { return *rig->threads; }
FakeClock*                            TestHost::clock() { return &rig->clock; }
FakeDisplay*                          TestHost::display() { return &rig->display; }
NetHooks*                             TestHost::netHooks() { return &*rig->netHooks; }
FakeClockServer*                      TestHost::clockServer() { return rig->hasServer ? &rig->clockServer : nullptr; }
FakeDiagnostics&                      TestHost::diagnostics() { return rig->diagnostics; }
void TestHost::sampleInput() { rig->events.push_back("hooks.sampleInput"); }
void TestHost::onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps)
{
	rig->events.push_back(format("host.frameProbe first=%lld steps=%lld", firstUpcomingSimTick, numSteps));
}
void TestHost::onPostPhysics() { rig->events.push_back("host.postPhysics"); }

struct Expected
{
	const char* event;
	const char* site;
};

// One prediction frame with two steps, in og-tickdriver's call order (SimulationTickDriver::startFrame, runBatch and
// endFrame; TickDriverStepHooks::beforeTick, beforePhysics, afterTick, afterBatch), each event transcribed from the code
// at the step (a) commit: root b786965 Source/OGBrawlerUnreal/BrawlerSessionHooksUImpl.h (H), OGSimulation ab09ccb
// og-sim-session/OGSimSession/SessionNetHooks.h (N) and OGSimulationUnreal/UESimulationSession.h (S).
const std::vector<Expected> kPredictionFrame = {
	{ "hooks.sampleInput",                     "H:52 sampleInput: publishSample per registered character" },
	{ "clock.beforePredictionPump",            "H:56 beforePump -> N:23 beforePump_Owner, client branch" },
	{ "host.frameProbe first=100 steps=2",     "H:62 onFrameStepsDue -> S:1160 onFrameStepsDue_Step" },
	{ "port.applyUnregisterRequests",          "H:74 receiveForStep -> N:117 receiveForClientStep_Step" },
	{ "lanes.drainClient",                     "N:118 m_simNetLanes->drainClient_Step()" },
	{ "diag.stampAfterStep 10.00",             "H:78 onStepStarted: stampAfterStep_Internal" },
	{ "client.afterCaptureTick tick=7",        "H:79 afterClientCapture_Step -> N:108 afterCaptureTick_Step" },
	{ "diag.stampStepEnd",                     "H:84 onStepEnded: stampStepEnd_Internal" },
	{ "clock.publishServerTick server=null",   "H:85 afterStep_Step -> S:455 publishServerTick(m_clockServer)" },
	{ "port.applyUnregisterRequests",          "H:74 -> N:117 (step 2)" },
	{ "lanes.drainClient",                     "N:118 (step 2)" },
	{ "diag.stampAfterStep 10.10",             "H:78 (step 2)" },
	{ "client.afterCaptureTick tick=7",        "H:79 -> N:108 (step 2)" },
	{ "diag.stampStepEnd",                     "H:84 (step 2)" },
	{ "clock.publishServerTick server=null",   "H:85 -> S:455 (step 2)" },
	{ "display.publishAfterBatch",             "H:88 publishForDisplay -> S:523 publishDisplaySnapshot_Step" },
	{ "host.postPhysics",                      "H:90 onPostPhysics -> S:1512 onPostPhysicsStep" },
};

// One authority wake with two steps, in og-tickdriver's call order (SimulationTickDriver::pumpAndRunSteps, runBatch,
// idleUntilNextDeadline; TickDriverStepHooks::beforeTick, beforePhysics, afterTick), then the frame thread's endFrame.
const std::vector<Expected> kAuthorityWake = {
	{ "clock.noteAuthorityRateDoor",                        "H:56 -> N:27 beforePump_Owner, authority branch" },
	{ "port.runRunnerRequests",                             "N:30-34 the runner requests, swapped out and run" },
	{ "server.drain",                                       "N:36 m_simNetServer->drain()" },
	{ "host.frameProbe first=200 steps=2",                  "H:62 -> S:1160" },
	{ "clock.noteAuthorityStepStart",                       "H:69 noteAuthorityStepTime_Step -> S:566" },
	{ "clock.takeReleaseStepSeconds",                       "H:71 releaseDelayedInputsForStep -> N:42" },
	{ "log [ReleaseBatch] first=201 numSteps=1",            "N:46 OG_LOG_ROUTED [ReleaseBatch]" },
	{ "server.releaseForStep first=201 steps=1 seconds=0.25", "N:48 releaseForStep(firstTick, numSteps, stepSeconds)" },
	{ "diag.stampAfterStep 20.00",                          "H:78 (afterClientCapture_Step returns: no client, N:105-106)" },
	{ "diag.stampStepEnd",                                  "H:84" },
	{ "clock.publishServerTick server=set",                 "H:85 -> S:455" },
	{ "clock.noteAuthorityStepStart",                       "H:69 -> S:566 (step 2)" },
	{ "clock.takeReleaseStepSeconds",                       "N:42 (step 2)" },
	{ "log [ReleaseBatch] first=202 numSteps=1",            "N:46 (step 2)" },
	{ "server.releaseForStep first=202 steps=1 seconds=0.25", "N:48 (step 2)" },
	{ "diag.stampAfterStep 20.10",                          "H:78 (step 2)" },
	{ "diag.stampStepEnd",                                  "H:84 (step 2)" },
	{ "clock.publishServerTick server=set",                 "H:85 -> S:455 (step 2)" },
	{ "manager.onPostSimulationGameThread",                 "H:65 afterInlineBatch -> N:60 afterAuthorityBatch" },
	{ "server.publishClock 42",                             "N:61 publishClock(getServerClock().getTick())" },
	{ "server.afterInlineBatch 200",                        "N:62 afterInlineBatch(firstUpcomingSimTick)" },
	{ "diag.stampCorrectionHanded id=1 tick=199",           "N:63 stampCorrectionsHanded_Runner -> N:88 (id 2: tick 0, id 3: no channel)" },
	{ "port.publishPresentation",                           "N:64 publishAuthorityPresentation_Runner" },
	{ "idle.idleUntil 5.00",                                "H:58 idleUntil -> N:15 idleUntil_Runner" },
	{ "host.postPhysics",                                   "H:90 -> S:1512 (the frame thread's endFrame)" },
};

void drivePredictionFrame(Rig& rig)
{
	SimulationScheduler scheduler(SchedulerConfig{ 1.0 / 60.0, 4u }, 0.0);
	uint64_t refused = 0u;
	TickRateDoor rate(scheduler, 0, false, refused);
	Game& game = *rig.game;
	game.sampleInput();
	game.beforePump(1.0, rate);
	game.onFrameStepsDue(100, 2);
	for (const double stepStart : { 10.0, 10.1 })
	{
		rig.inStep([&]
		{
			game.receiveForStep();
			game.onStepStarted(stepStart);
			game.onStepEnded();
		});
	}
	rig.inStep([&] { game.publishForDisplay(); });
	game.onPostPhysics();
}

void driveAuthorityWake(Rig& rig)
{
	SimulationScheduler scheduler(SchedulerConfig{ 1.0 / 60.0, 4u }, 0.0);
	uint64_t refused = 0u;
	TickRateDoor rate(scheduler, 0, true, refused);
	Game& game = *rig.game;
	game.beforePump(2.0, rate);
	game.onFrameStepsDue(200, 2);
	const std::pair<SimTick, double> steps[] = { { 201u, 20.0 }, { 202u, 20.1 } };
	for (const std::pair<SimTick, double>& step : steps)
	{
		const SimTick tick = step.first;
		const double stepStart = step.second;
		rig.inStep([&]
		{
			game.releaseInputsForAuthorityTick(tick);
			game.onStepStarted(stepStart);
			game.onStepEnded();
		});
	}
	game.afterInlineBatch(200);
	CHECK(game.idleUntil(5.0) == IdleWake::Deadline);
	game.onPostPhysics();
}

void requireTheTranscription(const Events& recorded, const std::vector<Expected>& expected)
{
	for (size_t index = 0u; index < expected.size() && index < recorded.size(); ++index)
	{
		INFO("entry " << index << ", transcribed from " << expected[index].site);
		CHECK(recorded[index] == expected[index].event);
	}
	REQUIRE(recorded.size() == expected.size());
}

size_t distinctKinds(const Events& recorded)
{
	std::set<std::string> kinds;
	for (const std::string& event : recorded)
		kinds.insert(event.substr(0, event.find(' ')));
	return kinds.size();
}
} // namespace

TEST_CASE("SessionTickGame.APredictionFrameWithTwoStepsCallsTheComponentsInTheUnrealOrder", "[Session][SessionTickGame]")
{
	Rig rig(/*predicts=*/true);
	drivePredictionFrame(rig);
	requireTheTranscription(rig.events, kPredictionFrame);
	CHECK(rig.events.size() == 17u);
	CHECK(distinctKinds(rig.events) == 11u);
}

TEST_CASE("SessionTickGame.AnAuthorityWakeWithTwoStepsCallsTheComponentsInTheUnrealOrder", "[Session][SessionTickGame]")
{
	Rig rig(/*predicts=*/false);
	rig.hasServer = true;
	driveAuthorityWake(rig);
	requireTheTranscription(rig.events, kAuthorityWake);
	CHECK(rig.events.size() == 25u);
	CHECK(distinctKinds(rig.events) == 18u);
}

TEST_CASE("SessionTickGame.WithoutAServerTheAuthorityReleasesNothingAndIdlesUnavailable", "[Session][SessionTickGame]")
{
	Rig rig(/*predicts=*/false);
	SimulationScheduler scheduler(SchedulerConfig{ 1.0 / 60.0, 4u }, 0.0);
	uint64_t refused = 0u;
	TickRateDoor rate(scheduler, 0, true, refused);
	Game& game = *rig.game;
	game.beforePump(2.0, rate);
	game.releaseInputsForAuthorityTick(201u);
	game.afterInlineBatch(200);
	CHECK(game.idleUntil(5.0) == IdleWake::Unavailable);
	const Events expected = { "clock.noteAuthorityRateDoor", "port.runRunnerRequests", "clock.noteAuthorityStepStart",
		"clock.takeReleaseStepSeconds" };
	CHECK(rig.events == expected);
}

TEST_CASE("SessionTickGame.AnUnboundClientStepsWithoutReceivingOrSending", "[Session][SessionTickGame]")
{
	Rig rig(/*predicts=*/true);
	rig.lanes.bound = false;
	Game& game = *rig.game;
	rig.inStep([&]
	{
		game.receiveForStep();
		game.onStepStarted(10.0);
	});
	const Events expected = { "diag.stampAfterStep 10.00" };
	CHECK(rig.events == expected);
}

TEST_CASE("SessionTickGame.ACorrectionTickIsReadFromTheBuffersHeader", "[Session][SessionTickGame]")
{
	FakeChannel empty;
	CHECK(NetHooks::correctionTickOf(empty) == 0u);
	FakeChannel written;
	written.buffer.writeToBuffer(correctionStateBuffer::kTickOffset, uint32_t{ 77u });
	CHECK(NetHooks::correctionTickOf(written) == 0u);
	written.buffer.writeToBuffer(correctionStateBuffer::kAppliedCaptureTickOffset, uint32_t{ 0u });
	CHECK(NetHooks::correctionTickOf(written) == 77u);
}

TEST_CASE("SessionTickGame.AStepDoorOnAClientsFrameThreadTripsTheCheck", "[.][SessionTickGameDeath]")
{
	Rig rig(/*predicts=*/true);
	rig.game->receiveForStep();
	FAIL("SessionTickGame::receiveForStep on a client's frame thread did NOT fire its thread check");
}

TEST_CASE("SessionTickGame.TheAuthoritysFirstBatchAtTickZeroWithAConnectionTripsTheCheck", "[.][SessionTickGameDeath]")
{
	Rig rig(/*predicts=*/false);
	rig.hasServer = true;
	rig.server.attached = { 1 };
	rig.game->afterInlineBatch(0);
	FAIL("SessionNetHooks::afterBatch at tick 0 with an attached connection did NOT fire its check");
}
