#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionTickGame-rationale.md · docs/SessionTickGame-guards.md

#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "OGSimulation/OGAssert.h"
#include "OGTickDriver/TickDriverConcepts.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGSimSession/SessionThreads.h"

namespace ogsession
{
template <typename D>
concept SessionStepStamps = requires(D& diagnostics, double seconds)
{
	{ diagnostics.stampAfterStep_Internal(seconds) } -> std::same_as<void>;
	{ diagnostics.stampStepEnd_Internal() }          -> std::same_as<void>;
};

template <typename H>
concept SessionTickHost = requires(H& host)
{
	typename H::Manager;
	typename H::Storage;
	typename H::RenderSnapshot;
	{ host.manager() } -> std::same_as<typename H::Manager&>;
	{ host.storage() } -> std::same_as<typename H::Storage&>;
};

template <typename T>
concept SessionComponentPointer = std::is_pointer_v<T>;

template <typename H, typename D>
concept SessionTickComponents = SessionStepStamps<D>
	&& requires(H& host, const H& constHost, int32_t tick, int32_t steps)
{
	{ constHost.threads() }               -> std::same_as<const SessionThreadChecks&>;
	{ host.clock() }                      -> SessionComponentPointer;
	{ host.display() }                    -> SessionComponentPointer;
	{ host.netHooks() }                   -> SessionComponentPointer;
	{ host.clockServer() }                -> SessionComponentPointer;
	{ host.diagnostics() }                -> std::same_as<D&>;
	{ host.sampleInput() }                -> std::same_as<void>;
	{ host.onFrameStepsDue(tick, steps) } -> std::same_as<void>;
	{ host.onPostPhysics() }              -> std::same_as<void>;
};

template <SessionTickHost HostT, typename DiagnosticsT>
class SessionTickGame
{
public:
	using Manager        = typename HostT::Manager;
	using Storage        = typename HostT::Storage;
	using RenderSnapshot = typename HostT::RenderSnapshot;

	explicit SessionTickGame(HostT host) : m_host(host) {}

	SessionTickGame(const SessionTickGame&) = delete;
	SessionTickGame& operator=(const SessionTickGame&) = delete;

	Manager& manager() { return m_host.manager(); }
	Storage& storage() { return m_host.storage(); }

	void sampleInput()
	{
		threads().checkOp(SessionOp::SampleInput);
		m_host.sampleInput();
	}

	void beforePump(double hostNowSeconds, TickRateDoor& rate)
	{
		threads().checkOp(SessionOp::RateDoor);
		auto* const clock = m_host.clock();
		if (clock != nullptr && m_host.manager().runsPrediction())
		{
			clock->beforePredictionPump(hostNowSeconds, rate);
			return;
		}
		if (clock != nullptr)
			clock->noteAuthorityRateDoor(hostNowSeconds, rate);
		netHooks().beforePump();
	}

	void onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps)
	{
		threads().checkOp(SessionOp::FrameProbe);
		m_host.onFrameStepsDue(firstUpcomingSimTick, numSteps);
	}

	void receiveForStep()
	{
		threads().checkOp(SessionOp::DrainInbox);
		netHooks().receiveForStep();
	}

	void onStepStarted(double stepStartSeconds)
	{
		threads().checkOp(SessionOp::Send);
		m_host.diagnostics().stampAfterStep_Internal(stepStartSeconds);
		netHooks().afterCapture();
	}

	void onStepEnded()
	{
		threads().checkOp(SessionOp::Step);
		m_host.diagnostics().stampStepEnd_Internal();
		if (auto* const clock = m_host.clock())
			clock->publishServerTick(m_host.clockServer());
	}

	void publishForDisplay()
	{
		threads().checkOp(SessionOp::Publish);
		auto* const display = m_host.display();
		OG_CHECK(display != nullptr,
			"SessionTickGame::publishForDisplay: the session's display is built with its configuration, before the client "
			"steps. Was the Unreal session's m_display->publishAfterBatch()");
		display->publishAfterBatch();
	}

	void releaseInputsForAuthorityTick(SimTick authorityTick)
	{
		threads().checkOp(SessionOp::Step);
		auto* const clock = m_host.clock();
		if (clock != nullptr)
			clock->noteAuthorityStepStart();
		const std::optional<double> stepSeconds = clock != nullptr ? clock->takeReleaseStepSeconds() : std::nullopt;
		// ⛔G-01  docs/SessionTickGame-guards.md
		netHooks().releaseForTick(static_cast<int32_t>(authorityTick), 1, stepSeconds);
	}

	void afterInlineBatch(int32_t firstUpcomingSimTick)
	{
		threads().checkOp(SessionOp::Send);
		netHooks().afterBatch(firstUpcomingSimTick);
	}

	IdleWake idleUntil(double deadlineSeconds)
	{
		threads().checkOp(SessionOp::ServiceNet);
		return netHooks().idleUntil(deadlineSeconds);
	}

	void onPostPhysics()
	{
		threads().checkOp(SessionOp::Present);
		m_host.onPostPhysics();
	}

private:
	const SessionThreadChecks& threads() const
	{
		static_assert(SessionTickComponents<HostT, DiagnosticsT>,
			"SessionTickGame: its host must hand it the thread table's checks, the clock, display, net hooks and clock "
			"server (each a pointer, null until the session's configuration built it), the step stamps (the diagnostics "
			"parameter) and the engine side's sampleInput, frame probe and post-physics pass (SessionTickComponents)");
		return m_host.threads();
	}

	auto& netHooks()
	{
		auto* const hooks = m_host.netHooks();
		OG_CHECK(hooks != nullptr,
			"SessionTickGame: the session's net hooks are built with its configuration, before the driver's first frame or "
			"wake calls a hook");
		return *hooks;
	}

	HostT m_host;
};

namespace sessionTickGameSelfCheck
{
struct Manager
{
	bool runsPrediction() const;
};
struct Storage {};
struct RenderSnapshot {};
struct Clock {};
struct Display {};
struct NetHooks {};
struct ClockServer {};

struct Stamps
{
	void stampAfterStep_Internal(double);
	void stampStepEnd_Internal();
};

struct StampsWithoutStepStart
{
	void stampStepEnd_Internal();
};

struct StampsWithoutStepEnd
{
	void stampAfterStep_Internal(double);
};

struct Host
{
	using Manager        = sessionTickGameSelfCheck::Manager;
	using Storage        = sessionTickGameSelfCheck::Storage;
	using RenderSnapshot = sessionTickGameSelfCheck::RenderSnapshot;

	Manager&                   manager();
	Storage&                   storage();
	const SessionThreadChecks& threads() const;
	Clock*                     clock();
	Display*                   display();
	NetHooks*                  netHooks();
	ClockServer*               clockServer();
	Stamps&                    diagnostics();
	void                       sampleInput();
	void                       onFrameStepsDue(int32_t, int32_t);
	void                       onPostPhysics();
};

struct HostWithoutRenderSnapshot
{
	using Manager = sessionTickGameSelfCheck::Manager;
	using Storage = sessionTickGameSelfCheck::Storage;

	Manager& manager();
	Storage& storage();
};

struct HostManagerByValue : Host
{
	Manager manager();
};

struct HostStorageByValue : Host
{
	Storage storage();
};

struct HostThreadsOnlyWhenMutable : Host
{
	const SessionThreadChecks& threads();
};

struct HostClockByReference : Host
{
	Clock& clock();
};

struct HostDisplayByReference : Host
{
	Display& display();
};

struct HostNetHooksByReference : Host
{
	NetHooks& netHooks();
};

struct HostClockServerByValue : Host
{
	ClockServer clockServer();
};

struct HostDiagnosticsByValue : Host
{
	Stamps diagnostics();
};

struct HostWithStampsWithoutStepEnd : Host
{
	StampsWithoutStepEnd& diagnostics();
};

struct HostSampleReturnsBool : Host
{
	bool sampleInput();
};

struct HostFrameProbeWithoutSteps : Host
{
	void onFrameStepsDue(int32_t);
};

struct HostPostPhysicsReturnsBool : Host
{
	bool onPostPhysics();
};

using Game = SessionTickGame<Host, Stamps>;

static_assert(SessionStepStamps<Stamps>,
	"SessionStepStamps self-check: the reference shape (stampAfterStep_Internal, stampStepEnd_Internal) must satisfy the "
	"concept");
static_assert(!SessionStepStamps<StampsWithoutStepStart>,
	"SessionStepStamps self-check: stampAfterStep_Internal(stepStartSeconds) is required, onStepStarted stamps the step start");
static_assert(!SessionStepStamps<StampsWithoutStepEnd>,
	"SessionStepStamps self-check: stampStepEnd_Internal() is required, onStepEnded stamps the step end");
static_assert(SessionTickHost<Host> && SessionTickComponents<Host, Stamps>,
	"SessionTickHost, SessionTickComponents self-check: the reference shape must satisfy both concepts");
static_assert(!SessionTickHost<HostWithoutRenderSnapshot>,
	"SessionTickHost self-check: the host names the render snapshot type the tick driver's presenter publishes");
static_assert(!SessionTickHost<HostManagerByValue>, "SessionTickHost self-check: manager() returns the manager by reference");
static_assert(!SessionTickHost<HostStorageByValue>, "SessionTickHost self-check: storage() returns the storage by reference");
static_assert(!SessionTickComponents<HostThreadsOnlyWhenMutable, Stamps>,
	"SessionTickComponents self-check: threads() must be callable on a const host");
static_assert(!SessionTickComponents<HostClockByReference, Stamps>,
	"SessionTickComponents self-check: clock() returns a pointer, null before the session's configuration built the clock");
static_assert(!SessionTickComponents<HostDisplayByReference, Stamps>,
	"SessionTickComponents self-check: display() returns a pointer, null before the session's configuration built the display");
static_assert(!SessionTickComponents<HostNetHooksByReference, Stamps>,
	"SessionTickComponents self-check: netHooks() returns a pointer, null before the session's configuration built them");
static_assert(!SessionTickComponents<HostClockServerByValue, Stamps>,
	"SessionTickComponents self-check: clockServer() returns a pointer, null on a client and before the server's lanes bind");
static_assert(!SessionTickComponents<HostDiagnosticsByValue, Stamps>,
	"SessionTickComponents self-check: diagnostics() returns the diagnostics parameter's object by reference");
static_assert(!SessionTickComponents<HostWithStampsWithoutStepEnd, StampsWithoutStepEnd>,
	"SessionTickComponents self-check: the diagnostics parameter must model SessionStepStamps");
static_assert(!SessionTickComponents<HostSampleReturnsBool, Stamps>,
	"SessionTickComponents self-check: sampleInput() returns void");
static_assert(!SessionTickComponents<HostFrameProbeWithoutSteps, Stamps>,
	"SessionTickComponents self-check: onFrameStepsDue takes the first upcoming tick and the step count");
static_assert(!SessionTickComponents<HostPostPhysicsReturnsBool, Stamps>,
	"SessionTickComponents self-check: onPostPhysics() returns void");

static_assert(TickDriverGame<Game>, "SessionTickGame must model og-simulation's TickDriverGame");
static_assert(TickDriverSampleGame<Game>,
	"SessionTickGame must keep a void sampleInput(): SimulationTickDriver calls it only under if constexpr, so without it "
	"nothing publishes the input sample and every client buildPlayerInput returns the neutral input. Added by "
	"og-input-latency task P5-15; was BrawlerTickDriverGame's (og-session-runtime task 26)");
static_assert(TickDriverReceiveGame<Game>,
	"SessionTickGame must keep a void receiveForStep(): TickDriverStepHooks calls it only under if constexpr, so without it "
	"an immediate client's step never drains og-sim-net and receives nothing once its session is bound. Was the drain in "
	"onStepStarted, which fed the next step (og-input-latency task P5-30); was BrawlerTickDriverGame's (og-session-runtime "
	"task 26)");
static_assert(TickDriverDisplayGame<Game>,
	"SessionTickGame must keep a void publishForDisplay(): SimulationTickDriver calls it only under if constexpr, after a "
	"client's step batch on the step thread, so without it a Jolt client publishes no display snapshot and its HUD, input "
	"history and visualization show nothing. Was the game-thread fill in onPostPhysicsStep, which raced the worker step "
	"(og-input-latency task P5-36); was BrawlerTickDriverGame's (og-session-runtime task 26)");
static_assert(TickDriverIdleGame<Game>,
	"SessionTickGame must keep IdleWake idleUntil(double): SimulationTickBackend::runAuthorityLoop requires it, and the "
	"authority runner services og-net only through it, so without it the authority loop, which every dedicated server runs, "
	"cannot build. Added by og-input-latency task P5-19 (P11 design section 4); was BrawlerTickDriverGame's "
	"(og-session-runtime task 26)");
} // namespace sessionTickGameSelfCheck
} // namespace ogsession
