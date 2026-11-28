#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionClock-rationale.md · docs/SessionClock-guards.md

#include <atomic>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "OGSimulation/Mailbox.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulation/PCTimeManagement/ClockDilationController.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimSession/SessionEngine.h"
#include "OGSimSession/SessionKnobs.h"
#include "OGSimSession/SessionThreads.h"

namespace ogsession
{
template <typename E>
concept SessionClockEngine = SessionLogSink<E> && requires(const E& e)
{
	{ e.nowSeconds() } -> std::same_as<double>;
};

template <typename L>
concept SessionClockLink = requires(const L& l)
{
	{ l.ackSrttSeconds() } -> std::same_as<double>;
	{ l.clockStats() }     -> std::same_as<ogsimnet::SimNetClockStats>;
};

template <typename S>
concept SessionClockServer = requires(S& s, uint32_t tick, double tickStartSeconds)
{
	{ s.publishServerTick(tick, tickStartSeconds) } -> std::same_as<void>;
};

namespace sessionClockSelfCheck
{
struct ConformingEngine
{
	void logRouted(const char*);
	double nowSeconds() const;
};

struct EngineWithoutLogRouted
{
	double nowSeconds() const;
};

struct EngineWithoutConstNow
{
	void logRouted(const char*);
	double nowSeconds();
};

struct EngineWithAFloatNow : ConformingEngine
{
	float nowSeconds() const;
};

struct ConformingLink
{
	double ackSrttSeconds() const;
	ogsimnet::SimNetClockStats clockStats() const;
};

struct LinkWithoutAckSrtt
{
	ogsimnet::SimNetClockStats clockStats() const;
};

struct LinkWithoutClockStats
{
	double ackSrttSeconds() const;
};

struct LinkWithAFloatAckSrtt : ConformingLink
{
	float ackSrttSeconds() const;
};

struct LinkReturningStatsByReference : ConformingLink
{
	const ogsimnet::SimNetClockStats& clockStats() const;
};

struct LinkReadableOnlyWhenMutable
{
	double ackSrttSeconds();
	ogsimnet::SimNetClockStats clockStats();
};

struct ConformingServer
{
	void publishServerTick(uint32_t, double);
};

struct ServerPublishingTheTickOnly
{
	void publishServerTick(uint32_t);
};

struct ServerWhosePublishReturnsBool
{
	bool publishServerTick(uint32_t, double);
};

static_assert(SessionClockEngine<ConformingEngine>,
	"SessionClockEngine self-check: the reference shape (logRouted, a const nowSeconds) must satisfy the concept");
static_assert(!SessionClockEngine<EngineWithoutLogRouted>,
	"SessionClockEngine self-check: logRouted is required, every clock line goes through the engine's log routes");
static_assert(!SessionClockEngine<EngineWithoutConstNow>,
	"SessionClockEngine self-check: nowSeconds must be callable on a const engine, as TickDriverEngine's is");
static_assert(!SessionClockEngine<EngineWithAFloatNow>,
	"SessionClockEngine self-check: nowSeconds returns double; a float of seconds since start loses milliseconds, and the "
	"clock's window timer is a difference of two such times");
static_assert(SessionClockLink<ConformingLink>,
	"SessionClockLink self-check: the reference shape (ackSrttSeconds, clockStats) must satisfy the concept");
static_assert(!SessionClockLink<LinkWithoutAckSrtt>,
	"SessionClockLink self-check: ackSrttSeconds is required, the [ClockSource.Window] line prints it");
static_assert(!SessionClockLink<LinkWithoutClockStats>,
	"SessionClockLink self-check: clockStats is required, the [ClockSource.Window] line prints its ping and pong counts");
static_assert(!SessionClockLink<LinkWithAFloatAckSrtt>,
	"SessionClockLink self-check: ackSrttSeconds returns double seconds, which the [ClockSource.Window] line prints in "
	"milliseconds");
static_assert(!SessionClockLink<LinkReturningStatsByReference>,
	"SessionClockLink self-check: clockStats returns a SimNetClockStats by value, one snapshot per window line, not a "
	"reference into the link");
static_assert(!SessionClockLink<LinkReadableOnlyWhenMutable>,
	"SessionClockLink self-check: both readers are callable on a const link; the clock's doors take the link by const "
	"reference");
static_assert(SessionClockServer<ConformingServer>,
	"SessionClockServer self-check: the reference shape (publishServerTick(tick, tickStartSeconds)) must satisfy the concept");
static_assert(!SessionClockServer<ServerPublishingTheTickOnly>,
	"SessionClockServer self-check: the publish carries the tick's step-start time; a tick-only publish is refused "
	"(og-time-dilation task 2b)");
static_assert(!SessionClockServer<ServerWhosePublishReturnsBool>,
	"SessionClockServer self-check: publishServerTick returns void; the clock discards any result, so a server that "
	"reported a refused publish through its return would go unheard");
} // namespace sessionClockSelfCheck

namespace sessionClock
{
inline constexpr double kNoRttReadingSeconds = -1.0;
} // namespace sessionClock

namespace sessionClockLines
{
inline constexpr char kConnectionTierTag[] = "[ConnectionTier]";
inline constexpr char kRelayDelayFloorTag[] = "[RelayDelayFloor]";
inline constexpr char kConnectionTierChange[] = "[ConnectionTier] tier %u -> %u, effective delay delta=%d";
inline constexpr char kNoTransition[] = "%s floor = %d ticks, effective delay delta=0, no stall or skip";
inline constexpr char kTransition[] = "[Warning]%s floor = %d ticks, %s, requesting %d-tick prediction %s";
inline constexpr char kUpward[] = "UPWARD";
inline constexpr char kDownward[] = "DOWNWARD";
inline constexpr char kStall[] = "stall";
inline constexpr char kSkip[] = "skip";
inline constexpr char kRelayFloorClamped[] =
	"[Warning][RelayDelayFloor] received floor %u out of range, clamped to %d ticks";
inline constexpr char kRelayFloorBelowHiccupBaseline[] =
	"[Warning][RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) \xE2\x80\x94 the one "
	"value that is neither";
inline constexpr char kRelayFloorUniformD[] =
	"[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) \xE2\x80\x94 uniform-D fairness mode active; tiers and LAN "
	"override are inert";
inline constexpr char kAppliedInputDelay[] =
	"[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)";
inline constexpr char kClockSourceWindow[] =
	"[Warning][ClockSource.Window] source=%s engineRttMs=%.3f simClockRttMs=%.3f ackSrttMs=%.3f minRttOffsetUs=%.0f "
	"owdVarUs=%.0f/%.0f smoothedRttMs=%.3f samples=%u invalid=%u pings=%llu pongs=%llu";
inline constexpr char kSourceSimClock[] = "SimClock";
inline constexpr char kSourceEngine[] = "Engine";
inline constexpr char kDilationWindow[] = "[Warning]%s";
inline constexpr char kDilationAuthority[] = "[Warning][Dilation.Authority] refusedRateChanges=%llu rateScale=%.5f";
} // namespace sessionClockLines

template <typename ManagerT, typename InputResolutionT, SessionClockEngine EngineT>
class SessionClock
{
public:
	SessionClock(EngineT& engine, ManagerT& manager, InputResolutionT& inputResolution, const SessionThreadChecks& threads)
		: m_engine(engine)
		, m_manager(manager)
		, m_inputResolution(inputResolution)
		, m_threads(threads)
	{
	}

	SessionClock(const SessionClock&) = delete;
	SessionClock& operator=(const SessionClock&) = delete;

	template <SessionClockLink LinkT>
	void onAuthorityTick(uint32_t authorityTick, double roundTripSeconds, const LinkT& link)
	{
		m_threads.checkOp(receiptOp());
		noteEngineRttForClockWindow(roundTripSeconds);
		m_manager.editNetworkEstimator().noteEngineRttSample(roundTripSeconds);
		m_manager.editNetworkEstimator().recordAuthorityTick(authorityTick);
		maybeLogClockSourceWindow(link);
	}

	template <SessionClockLink LinkT>
	void onClockSample(const SimClockRttSample& sample, const LinkT& link)
	{
		m_threads.checkOp(receiptOp());
		m_manager.editNetworkEstimator().noteSimClockSample(sample);
		maybeLogClockSourceWindow(link);
	}

	void onConnectionTier(uint8_t oldTier, uint8_t newTier)
	{
		m_threads.checkOp(receiptOp());
		const int32_t deltaDelayTicks = applyReplicatedConnectionTier(newTier);

		sessionKnobsLines::logLine(m_engine, sessionClockLines::kConnectionTierChange, static_cast<unsigned int>(oldTier),
			static_cast<unsigned int>(newTier), deltaDelayTicks);

		// ⛔G-07  docs/SessionClock-guards.md
		payInputDelayTransition(sessionClockLines::kConnectionTierTag, deltaDelayTicks,
			static_cast<int32_t>(m_manager.getTimeConfig().relayDelayFloorTicks));
	}

	void onRelayFloor(uint8_t floorTicks)
	{
		m_threads.checkOp(receiptOp());
		applyReplicatedRelayDelayFloor(floorTicks);
	}

	void beforePredictionPump(double hostNowSeconds, TickRateDoor& rate)
	{
		m_threads.checkOp(SessionOp::RateDoor);
		if (!m_clockDilation.has_value() || m_clockDilation->config().mode == ClockDilationMode::Off)
			return;
		// ⛔G-05  docs/SessionClock-guards.md
		const double clockNowSeconds = m_engine.nowSeconds();
		clockDilation::beforeClientPump(rate, hostNowSeconds, clockNowSeconds, m_manager, *m_clockDilation,
			*m_clockDilationWindow);
		if (m_clockDilationWindow->isDue(clockNowSeconds))
		{
			sessionKnobsLines::logLine(m_engine, sessionClockLines::kDilationWindow, m_clockDilationWindow->formatLine());
			m_clockDilationWindow->restart();
		}
	}

	void noteAuthorityRateDoor(double hostNowSeconds, const TickRateDoor& rate)
	{
		m_threads.checkOp(SessionOp::RateDoor);
		const uint64_t refused = rate.refusedRateChanges();
		if (refused == m_authorityRefusedRateChangesLogged && m_authorityRateLineDueSeconds.has_value()
			&& hostNowSeconds < *m_authorityRateLineDueSeconds)
		{
			return;
		}
		m_authorityRefusedRateChangesLogged = refused;
		m_authorityRateLineDueSeconds = hostNowSeconds + clockDilation::kWindowSeconds;
		sessionKnobsLines::logLine(m_engine, sessionClockLines::kDilationAuthority, static_cast<unsigned long long>(refused),
			rate.rateScale());
	}

	void noteAuthorityStepStart()
	{
		m_threads.checkOp(SessionOp::Step);
		const double stepSeconds = m_engine.nowSeconds();
		m_releaseStepSeconds = stepSeconds;
		m_clockTickStartSeconds = stepSeconds;
		m_manager.noteAuthorityStepSeconds(stepSeconds);
	}

	std::optional<double> takeReleaseStepSeconds()
	{
		m_threads.checkOp(SessionOp::Step);
		return std::exchange(m_releaseStepSeconds, std::nullopt);
	}

	template <SessionClockServer ServerT>
	void publishServerTick(ServerT* clockServer)
	{
		m_threads.checkOp(SessionOp::Publish);
		const std::optional<double> tickStartSeconds = std::exchange(m_clockTickStartSeconds, std::nullopt);
		if (clockServer == nullptr || m_manager.runsPrediction())
			return;
		OG_CHECK(tickStartSeconds.has_value(),
			"SessionClock::publishServerTick: an authority step ended with no start time. The tick driver's game calls "
			"noteAuthorityStepStart when it releases the inputs for an authority tick, at the start of every authority step, "
			"and the SimClock server's tick age is measured from that time. Was publishServerTick(tick), which read the clock "
			"at the publish (og-time-dilation task 2b)");
		// ⛔G-06  docs/SessionClock-guards.md
		clockServer->publishServerTick(static_cast<uint32_t>(m_manager.getServerClock().getTick()), *tickStartSeconds);
	}

	void beginTierConsumer()
	{
		m_replicatedTierConsumer.emplace(m_manager.getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();
	}

	void endTierConsumer() { m_replicatedTierConsumer.reset(); }

	[[nodiscard]] StoppedDrainScope stoppedDrain() { return StoppedDrainScope(m_inStoppedDrain, m_threads); }
	bool inStoppedDrain() const { return m_inStoppedDrain.load(std::memory_order_relaxed); }

	// ⛔G-02  docs/SessionClock-guards.md
	void logRelayDelayFloorAdvisory(int32_t floorTicks)
	{
		// ⛔G-04  docs/SessionClock-guards.md
		switch (classifyRelayDelayFloor(m_manager.getTimeConfig()))
		{
		case RelayDelayFloorAdvisory::BelowHiccupBaseline:
			sessionKnobsLines::logLine(m_engine, sessionClockLines::kRelayFloorBelowHiccupBaseline, floorTicks);
			break;
		case RelayDelayFloorAdvisory::UniformDFairnessActive:
			sessionKnobsLines::logLine(m_engine, sessionClockLines::kRelayFloorUniformD, floorTicks);
			break;
		case RelayDelayFloorAdvisory::None:
			break;
		}
	}

	const ReplicatedTierConsumer* tierConsumer() const
	{
		return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
	}

	std::optional<ClockDilationController>& dilationController() { return m_clockDilation; }
	std::optional<ClockDilationWindow>&     dilationWindow() { return m_clockDilationWindow; }
	TripleBuffer<DisplayTierReading>&       displayTier() { return *m_displayTier; }

private:
	SessionOp receiptOp() const
	{
		return m_inStoppedDrain.load(std::memory_order_relaxed) ? SessionOp::DrainStopped : SessionOp::DrainInbox;
	}

	int32_t applyReplicatedConnectionTier(uint8_t tier)
	{
		if (!m_replicatedTierConsumer.has_value())
			return 0;

		m_replicatedTierConsumer->onReplicatedTierReceived(static_cast<int32_t>(tier));
		return recomputeAndPublishEffectiveInputDelay();
	}

	void applyReplicatedRelayDelayFloor(uint8_t floorTicks)
	{
		const TimeConfig& cfg = m_manager.getTimeConfig();
		const int32_t clampedFloor = clampRelayDelayFloorTicks(static_cast<int32_t>(floorTicks), cfg);
		if (clampedFloor != static_cast<int32_t>(floorTicks))
		{
			sessionKnobsLines::logLine(m_engine, sessionClockLines::kRelayFloorClamped, static_cast<unsigned int>(floorTicks),
				clampedFloor);
		}

		m_manager.setRelayDelayFloorTicks(clampedFloor);

		logRelayDelayFloorAdvisory(clampedFloor);

		const int32_t deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

		payInputDelayTransition(sessionClockLines::kRelayDelayFloorTag, deltaDelayTicks, clampedFloor);
	}

	void payInputDelayTransition(const char* lineTag, int32_t deltaDelayTicks, int32_t floorTicks)
	{
		if (deltaDelayTicks == 0)
		{
			sessionKnobsLines::logLine(m_engine, sessionClockLines::kNoTransition, lineTag, floorTicks);
			return;
		}

		requestInputDelayTransition(deltaDelayTicks);

		sessionKnobsLines::logLine(m_engine, sessionClockLines::kTransition, lineTag, floorTicks,
			deltaDelayTicks > 0 ? sessionClockLines::kUpward : sessionClockLines::kDownward,
			deltaDelayTicks > 0 ? deltaDelayTicks : -deltaDelayTicks,
			deltaDelayTicks > 0 ? sessionClockLines::kStall : sessionClockLines::kSkip);
	}

	void requestInputDelayTransition(int32_t deltaDelayTicks)
	{
		// ⛔G-01  docs/SessionClock-guards.md
		if (!m_manager.runsPrediction())
			return;

		m_manager.editClientClock().requestInputDelayTransition(deltaDelayTicks);
	}

	// ⛔G-03  docs/SessionClock-guards.md
	int32_t recomputeAndPublishEffectiveInputDelay()
	{
		if (!m_replicatedTierConsumer.has_value())
			return 0;

		const int32_t delayTicks = static_cast<int32_t>(m_replicatedTierConsumer->effectiveInputDelayTicks());
		const int32_t deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

		m_lastPublishedEffectiveInputDelayTicks = delayTicks;
		m_inputResolution.setClientEffectiveInputDelayTicks(delayTicks);

		m_displayTier->write() = readDisplayTier(*m_replicatedTierConsumer, m_manager.getTimeConfig(), delayTicks);
		m_displayTier->publish();

		sessionKnobsLines::logLine(m_engine, sessionClockLines::kAppliedInputDelay, delayTicks,
			static_cast<int>(m_replicatedTierConsumer->currentTierIndex()), m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
			static_cast<int>(m_manager.getTimeConfig().relayDelayFloorTicks));

		return deltaDelayTicks;
	}

	void noteEngineRttForClockWindow(double roundTripSeconds)
	{
		if (!(roundTripSeconds > 0.0) || !std::isfinite(roundTripSeconds))
			return;
		const double alpha = m_manager.getTimeConfig().rttSmoothingAlpha;
		m_engineRttWindowSeconds = m_engineRttWindowSeconds > 0.0
			? alpha * roundTripSeconds + (1.0 - alpha) * m_engineRttWindowSeconds
			: roundTripSeconds;
	}

	template <SessionClockLink LinkT>
	void maybeLogClockSourceWindow(const LinkT& link)
	{
		if (!m_manager.runsPrediction())
			return;
		const double now = m_engine.nowSeconds();
		if (!m_clockWindowStartSeconds.has_value())
		{
			m_clockWindowStartSeconds = now;
			return;
		}
		if (now - *m_clockWindowStartSeconds < ogsimnet::kSimNetWindowSeconds)
			return;
		m_clockWindowStartSeconds = now;
		const auto& estimator = m_manager.getNetworkEstimator();
		const ClockSourceShadow shadow = estimator.getClockSourceShadow();
		const double ackSrttSeconds = link.ackSrttSeconds();
		const ogsimnet::SimNetClockStats clockStats = link.clockStats();
		const auto toMs = [](double seconds) { return seconds >= 0.0 ? seconds * 1000.0 : -1.0; };
		sessionKnobsLines::logLine(m_engine, sessionClockLines::kClockSourceWindow,
			estimator.getActiveRttSource() == TimeConfig::ClockRttSource::SimClock ? sessionClockLines::kSourceSimClock
			                                                                       : sessionClockLines::kSourceEngine,
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

	EngineT&            m_engine;
	ManagerT&           m_manager;
	InputResolutionT&   m_inputResolution;
	SessionThreadChecks m_threads;
	std::atomic<bool>   m_inStoppedDrain{ false };

	std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;
	int32_t                               m_lastPublishedEffectiveInputDelayTicks = 0;

	double                                 m_engineRttWindowSeconds = sessionClock::kNoRttReadingSeconds;
	std::optional<double>                  m_clockWindowStartSeconds;
	std::optional<ClockDilationController> m_clockDilation;
	std::optional<ClockDilationWindow>     m_clockDilationWindow;
	std::optional<double>                  m_authorityRateLineDueSeconds;
	uint64_t                               m_authorityRefusedRateChangesLogged = 0u;
	std::optional<double>                  m_releaseStepSeconds;
	std::optional<double>                  m_clockTickStartSeconds;

	std::unique_ptr<TripleBuffer<DisplayTierReading>> m_displayTier = std::make_unique<TripleBuffer<DisplayTierReading>>();
};
} // namespace ogsession
