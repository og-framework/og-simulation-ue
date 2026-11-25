// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionClock.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Mailbox.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimSession/SimulationDisplaySnapshot.h"
#include "OGSimulation/SimulationScheduler.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sessionClockTest
{
using Events = std::vector<std::string>;

template <typename... ArgsT>
std::string format(const char* pattern, ArgsT... args)
{
	const int length = std::snprintf(nullptr, 0, pattern, args...);
	REQUIRE(length >= 0);
	std::string text(static_cast<std::size_t>(length), '\0');
	std::snprintf(text.data(), text.size() + 1u, pattern, args...);
	return text;
}

struct FakeEngine
{
	Events* events = nullptr;
	double  now = 0.0;

	void logRouted(const char* line) { events->push_back(std::string("line ") + line); }
	double nowSeconds() const { return now; }
};

struct FakeEstimator
{
	explicit FakeEstimator(const TimeConfig& config) : real(config, nullptr) {}

	void noteEngineRttSample(double roundTripSeconds)
	{
		events->push_back(format("engineRtt %a", roundTripSeconds));
		real.noteEngineRttSample(roundTripSeconds);
	}

	void recordAuthorityTick(unsigned int tick)
	{
		events->push_back(format("authorityTick %u", tick));
		real.recordAuthorityTick(tick);
	}

	void noteSimClockSample(const SimClockRttSample& sample)
	{
		events->push_back(format("clockSample rtt=%a tick=%u", sample.rttSeconds, sample.serverTick));
		real.noteSimClockSample(sample);
	}

	ClockSourceShadow getClockSourceShadow() const { return real.getClockSourceShadow(); }
	TimeConfig::ClockRttSource getActiveRttSource() const { return real.getActiveRttSource(); }
	double getSmoothedRTT() const { return real.getSmoothedRTT(); }

	NetworkTimeEstimator real;
	Events*              events = nullptr;
};

struct FakeClientClock
{
	Events* events = nullptr;
	void requestInputDelayTransition(int32_t deltaDelayTicks) { events->push_back(format("request %d", deltaDelayTicks)); }
};

struct FakeServerClock
{
	unsigned int tick = 0u;
	unsigned int getTick() const { return tick; }
};

struct FakeManager
{
	FakeManager(Events& sink, bool predicts, const TimeConfig& initial)
		: config(initial)
		, prediction(predicts)
		, estimator(config)
		, events(&sink)
	{
		estimator.events = &sink;
		clientClock.events = &sink;
	}

	bool runsPrediction() const { return prediction; }
	const TimeConfig& getTimeConfig() const { return config; }

	void setRelayDelayFloorTicks(int32_t requestedFloorTicks)
	{
		events->push_back(format("setFloor %d", requestedFloorTicks));
		config.relayDelayFloorTicks = clampRelayDelayFloorTicks(requestedFloorTicks, config);
	}

	FakeEstimator& editNetworkEstimator() { return estimator; }
	const FakeEstimator& getNetworkEstimator() const { return estimator; }
	FakeClientClock& editClientClock()
	{
		REQUIRE(prediction);
		return clientClock;
	}
	const FakeServerClock& getServerClock() const { return serverClock; }
	void noteAuthorityStepSeconds(double stepSeconds) { events->push_back(format("stepSeconds %a", stepSeconds)); }

	TimeConfig      config;
	bool            prediction;
	FakeEstimator   estimator;
	FakeClientClock clientClock;
	FakeServerClock serverClock;
	Events*         events;
};

struct FakeInputResolution
{
	Events* events = nullptr;
	void setClientEffectiveInputDelayTicks(int32_t delayTicks) { events->push_back(format("publishDelay %d", delayTicks)); }
};

struct FakeLink
{
	double                     ack = ogsession::sessionClock::kNoRttReadingSeconds;
	ogsimnet::SimNetClockStats stats{};

	double ackSrttSeconds() const { return ack; }
	ogsimnet::SimNetClockStats clockStats() const { return stats; }
};

struct FakeClockServer
{
	Events* events = nullptr;
	void publishServerTick(uint32_t tick, double tickStartSeconds)
	{
		events->push_back(format("serverTick %u start=%a", tick, tickStartSeconds));
	}
};

static_assert(ogsession::SessionClockLink<FakeLink> && ogsession::SessionClockServer<FakeClockServer>
		&& ogsession::SessionClockEngine<FakeEngine>,
	"SessionClockTest's fakes model the clock's three ports");

enum class Level
{
	Log,
	Warning
};

// The transcription: the Unreal session's functions as og-session-runtime task 24 (a) committed them (OGSimulation dae3018,
// Source/OGSimulationSession/og-sim-session/OGSimSession/SessionClock.h; each function's lines are cited), with UE_LOG a
// recorder (the [Warning] token for a Warning, none for a Log, as the clock's lines carry it), FPlatformTime::Seconds() the
// fake engine's time, FMath::IsFinite std::isfinite, TEXT literals narrow, checkf a REQUIRE, m_manager a pointer.
struct TranscribedSession
{
	template <typename... ArgsT>
	void ueLog(Level level, const char* pattern, ArgsT... args)
	{
		engine->events->push_back(std::string("line ") + (level == Level::Warning ? "[Warning]" : "") + format(pattern, args...));
	}

	// :16-30
	void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime, const FakeLink* lanes)
	{
		if (m_manager == nullptr)
			return;
		noteEngineRttForClockWindow(roundTripTime);
		m_manager->editNetworkEstimator().noteEngineRttSample(roundTripTime);
		m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
		maybeLogClockSourceWindow(lanes);
	}

	// :33-39
	void onSimClockSample(const SimClockRttSample& sample, const FakeLink* lanes)
	{
		if (m_manager == nullptr)
			return;
		m_manager->editNetworkEstimator().noteSimClockSample(sample);
		maybeLogClockSourceWindow(lanes);
	}

	// :42-45
	void publishClientEffectiveInputDelayTicks(int32_t delayTicks) { inputResolution->setClientEffectiveInputDelayTicks(delayTicks); }

	// :48-55
	void requestInputDelayTransition(int32_t deltaDelayTicks)
	{
		if (m_manager == nullptr || !m_manager->runsPrediction())
			return;

		m_manager->editClientClock().requestInputDelayTransition(deltaDelayTicks);
	}

	// :58-62
	void beginTierConsumer()
	{
		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();
	}

	// :81-93
	void noteAuthorityRateDoor_Owner(double hostNowSeconds, const TickRateDoor& rate)
	{
		const uint64_t refused = rate.refusedRateChanges();
		if (refused == m_authorityRefusedRateChangesLogged_Owner && m_authorityRateLineDueSeconds_Owner.has_value()
			&& hostNowSeconds < *m_authorityRateLineDueSeconds_Owner)
		{
			return;
		}
		m_authorityRefusedRateChangesLogged_Owner = refused;
		m_authorityRateLineDueSeconds_Owner = hostNowSeconds + clockDilation::kWindowSeconds;
		ueLog(Level::Warning, "[Dilation.Authority] refusedRateChanges=%llu rateScale=%.5f",
			static_cast<unsigned long long>(refused), rate.rateScale());
	}

	// :96-108
	void afterStep_Step(FakeClockServer* m_clockServer)
	{
		const std::optional<double> tickStartSeconds = std::exchange(m_clockTickStartSeconds_Step, std::nullopt);
		if (m_clockServer == nullptr || m_manager == nullptr || m_manager->runsPrediction())
			return;
		REQUIRE(tickStartSeconds.has_value());
		m_clockServer->publishServerTick(static_cast<uint32_t>(m_manager->getServerClock().getTick()), *tickStartSeconds);
	}

	// :111-121
	void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
	{
		const int32_t deltaDelayTicks = applyReplicatedConnectionTier(newTier);

		ueLog(Level::Log, "[ConnectionTier] tier %u -> %u, effective delay delta=%d",
			(unsigned int)oldTier, (unsigned int)newTier, deltaDelayTicks);

		payInputDelayTransition("[ConnectionTier]", deltaDelayTicks,
			m_manager != nullptr ? (int32_t)m_manager->getTimeConfig().relayDelayFloorTicks : 0);
	}

	// :124-131
	int32_t applyReplicatedConnectionTier(uint8_t tier)
	{
		if (!m_replicatedTierConsumer.has_value())
			return 0;

		m_replicatedTierConsumer->onReplicatedTierReceived((int32_t)tier);
		return recomputeAndPublishEffectiveInputDelay();
	}

	// :134-137
	void onRelayDelayFloorReceived(uint8_t floorTicks) { applyReplicatedRelayDelayFloor((uint8_t)floorTicks); }

	// :140-161
	void applyReplicatedRelayDelayFloor(uint8_t floorTicks)
	{
		if (m_manager == nullptr)
			return;

		const TimeConfig& cfg = m_manager->getTimeConfig();
		const int32_t clampedFloor = clampRelayDelayFloorTicks((int32_t)floorTicks, cfg);
		if (clampedFloor != (int32_t)floorTicks)
		{
			ueLog(Level::Warning, "[RelayDelayFloor] received floor %u out of range, clamped to %d ticks",
				(unsigned int)floorTicks, clampedFloor);
		}

		m_manager->setRelayDelayFloorTicks(clampedFloor);

		logRelayDelayFloorAdvisory(clampedFloor);

		const int32_t deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

		payInputDelayTransition("[RelayDelayFloor]", deltaDelayTicks, clampedFloor);
	}

	// :164-181
	void payInputDelayTransition(const char* lineTag, int32_t deltaDelayTicks, int32_t floorTicks)
	{
		if (deltaDelayTicks == 0)
		{
			ueLog(Level::Log, "%s floor = %d ticks, effective delay delta=0, no stall or skip", lineTag, floorTicks);
			return;
		}

		requestInputDelayTransition(deltaDelayTicks);

		ueLog(Level::Warning, "%s floor = %d ticks, %s, requesting %d-tick prediction %s",
			lineTag, floorTicks, deltaDelayTicks > 0 ? "UPWARD" : "DOWNWARD",
			deltaDelayTicks > 0 ? deltaDelayTicks : -deltaDelayTicks,
			deltaDelayTicks > 0 ? "stall" : "skip");
	}

	// :184-205
	void logRelayDelayFloorAdvisory(int32_t floorTicks)
	{
		if (m_manager == nullptr)
			return;

		switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
		{
		case RelayDelayFloorAdvisory::BelowHiccupBaseline:
			ueLog(Level::Warning,
				"[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) \xE2\x80\x94 the one "
				"value that is neither",
				floorTicks);
			break;
		case RelayDelayFloorAdvisory::UniformDFairnessActive:
			ueLog(Level::Log,
				"[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) \xE2\x80\x94 uniform-D fairness mode active; tiers and LAN "
				"override are inert",
				floorTicks);
			break;
		case RelayDelayFloorAdvisory::None:
			break;
		}
	}

	// :208-233
	int32_t recomputeAndPublishEffectiveInputDelay()
	{
		if (!m_replicatedTierConsumer.has_value())
			return 0;

		const int32_t delayTicks = (int32_t)m_replicatedTierConsumer->effectiveInputDelayTicks();
		const int32_t deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

		m_lastPublishedEffectiveInputDelayTicks = delayTicks;
		publishClientEffectiveInputDelayTicks(delayTicks);

		if (m_manager != nullptr)
		{
			m_displayTier->write() = readDisplayTier(*m_replicatedTierConsumer, m_manager->getTimeConfig(), delayTicks);
			m_displayTier->publish();
		}

		ueLog(Level::Log, "[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)",
			delayTicks,
			(int)m_replicatedTierConsumer->currentTierIndex(),
			m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
			m_manager != nullptr ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

		return deltaDelayTicks;
	}

	// :236-244
	void noteEngineRttForClockWindow(double roundTripSeconds)
	{
		if (!(roundTripSeconds > 0.0) || !std::isfinite(roundTripSeconds))
			return;
		const double alpha = m_manager->getTimeConfig().rttSmoothingAlpha;
		m_engineRttWindowSeconds = m_engineRttWindowSeconds > 0.0
			? alpha * roundTripSeconds + (1.0 - alpha) * m_engineRttWindowSeconds
			: roundTripSeconds;
	}

	// :247-281
	void maybeLogClockSourceWindow(const FakeLink* m_simNetLanes)
	{
		if (m_manager == nullptr || !m_manager->runsPrediction())
			return;
		const double now = engine->now;
		if (!m_clockWindowStartSeconds.has_value())
		{
			m_clockWindowStartSeconds = now;
			return;
		}
		if (now - *m_clockWindowStartSeconds < ogsimnet::kSimNetWindowSeconds)
			return;
		m_clockWindowStartSeconds = now;
		const FakeEstimator& estimator = m_manager->getNetworkEstimator();
		const ClockSourceShadow shadow = estimator.getClockSourceShadow();
		const double ackSrttSeconds = m_simNetLanes != nullptr ? m_simNetLanes->ackSrttSeconds() : ogsession::sessionClock::kNoRttReadingSeconds;
		const ogsimnet::SimNetClockStats clockStats = m_simNetLanes != nullptr ? m_simNetLanes->clockStats() : ogsimnet::SimNetClockStats{};
		const auto toMs = [](double seconds) { return seconds >= 0.0 ? seconds * 1000.0 : -1.0; };
		ueLog(Level::Warning,
			"[ClockSource.Window] source=%s engineRttMs=%.3f simClockRttMs=%.3f ackSrttMs=%.3f minRttOffsetUs=%.0f "
			"owdVarUs=%.0f/%.0f smoothedRttMs=%.3f samples=%u invalid=%u pings=%llu pongs=%llu",
			estimator.getActiveRttSource() == TimeConfig::ClockRttSource::SimClock ? "SimClock" : "Engine",
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

	// :284-291
	void noteAuthorityStepTime_Step()
	{
		const double stepSeconds = engine->now;
		m_releaseStepSeconds_Step = stepSeconds;
		m_clockTickStartSeconds_Step = stepSeconds;
		if (m_manager != nullptr)
			m_manager->noteAuthorityStepSeconds(stepSeconds);
	}

	// UESimulationSession::releaseDelayedInputsForStep's first statement (UE-sim/UESimulationSession.h at dae3018)
	std::optional<double> takeReleaseStepSeconds() { return std::exchange(m_releaseStepSeconds_Step, std::nullopt); }

	FakeEngine*                           engine = nullptr;
	FakeManager*                          m_manager = nullptr;
	FakeInputResolution*                  inputResolution = nullptr;
	std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;
	int32_t                               m_lastPublishedEffectiveInputDelayTicks = 0;
	double                                m_engineRttWindowSeconds = ogsession::sessionClock::kNoRttReadingSeconds;
	std::optional<double>                 m_clockWindowStartSeconds;
	std::optional<double>                 m_authorityRateLineDueSeconds_Owner;
	uint64_t                              m_authorityRefusedRateChangesLogged_Owner = 0u;
	std::optional<double>                 m_releaseStepSeconds_Step;
	std::optional<double>                 m_clockTickStartSeconds_Step;
	std::unique_ptr<TripleBuffer<DisplayTierReading>> m_displayTier = std::make_unique<TripleBuffer<DisplayTierReading>>();
};

using Clock = ogsession::SessionClock<FakeManager, FakeInputResolution, FakeEngine>;

std::string tierReading(TripleBuffer<DisplayTierReading>& buffer)
{
	const DisplayTierReading* const reading = buffer.acquireLatest();
	if (reading == nullptr)
		return "displayTier none";
	return format("displayTier arrived=%d tier=%d tierDelay=%d floor=%d effective=%d", reading->hasReceivedTier ? 1 : 0,
		reading->tierIndex, reading->tierInputDelayTicks, reading->relayDelayFloorTicks, reading->clientEffectiveInputDelayTicks);
}

// One side of the comparison: its own events, engine, manager and input resolution.
struct Side
{
	Side(bool predicts, const TimeConfig& config) : manager(events, predicts, config)
	{
		engine.events = &events;
		inputResolution.events = &events;
	}

	Events              events;
	FakeEngine          engine;
	FakeManager         manager;
	FakeInputResolution inputResolution;
	FakeClockServer     server{ &events };
	FakeLink            link;
};

// The two sides, run by one scenario through the same calls: the transcription and the session clock, on this thread as the
// frame thread; a client's step-thread doors run inside a StepScope of its configured context, an authority's on the thread
// that configured its context (its runner).
struct Rig
{
	Rig(bool predicts, const TimeConfig& config)
		: role(predicts ? ogsession::SessionRole::Prediction : ogsession::SessionRole::Authority)
		, oldSide(predicts, config)
		, newSide(predicts, config)
	{
		context.configure<true>(predicts ? tickDriverThread::kClientRoleBit : tickDriverThread::kAuthorityRoleBit,
			predicts ? StepThreading::FrameBounded : StepThreading::FreeRunning);
		transcribed.engine = &oldSide.engine;
		transcribed.m_manager = &oldSide.manager;
		transcribed.inputResolution = &oldSide.inputResolution;
		clock.emplace(newSide.engine, newSide.manager, newSide.inputResolution,
			ogsession::SessionThreadChecks(role, &context, std::this_thread::get_id()));
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

	void at(double seconds)
	{
		oldSide.engine.now = seconds;
		newSide.engine.now = seconds;
	}

	void beginTierConsumer()
	{
		transcribed.beginTierConsumer();
		clock->beginTierConsumer();
		markDisplay();
	}

	void connectionTier(uint8_t oldTier, uint8_t newTier)
	{
		transcribed.onConnectionTierReceived(oldTier, newTier);
		inStep([&] { clock->onConnectionTier(oldTier, newTier); });
		markDisplay();
	}

	void relayFloor(uint8_t floorTicks)
	{
		transcribed.onRelayDelayFloorReceived(floorTicks);
		inStep([&] { clock->onRelayFloor(floorTicks); });
		markDisplay();
	}

	void authorityTick(uint32_t tick, double roundTripSeconds)
	{
		transcribed.onTimingInfoReceived(tick, roundTripSeconds, &oldSide.link);
		inStep([&] { clock->onAuthorityTick(tick, roundTripSeconds, newSide.link); });
	}

	void clockSample(const SimClockRttSample& sample)
	{
		transcribed.onSimClockSample(sample, &oldSide.link);
		inStep([&] { clock->onClockSample(sample, newSide.link); });
	}

	void stepStart()
	{
		transcribed.noteAuthorityStepTime_Step();
		inStep([&] { clock->noteAuthorityStepStart(); });
	}

	void takeRelease()
	{
		const std::optional<double> before = transcribed.takeReleaseStepSeconds();
		oldSide.events.push_back(before.has_value() ? format("release %a", *before) : std::string("release none"));
		std::optional<double> after;
		inStep([&] { after = clock->takeReleaseStepSeconds(); });
		newSide.events.push_back(after.has_value() ? format("release %a", *after) : std::string("release none"));
	}

	void publishServerTick(bool withServer)
	{
		transcribed.afterStep_Step(withServer ? &oldSide.server : nullptr);
		inStep([&] { clock->publishServerTick(withServer ? &newSide.server : static_cast<FakeClockServer*>(nullptr)); });
	}

	void rateDoor(double hostNowSeconds, const TickRateDoor& rate)
	{
		transcribed.noteAuthorityRateDoor_Owner(hostNowSeconds, rate);
		clock->noteAuthorityRateDoor(hostNowSeconds, rate);
	}

	void markDisplay()
	{
		oldSide.events.push_back(tierReading(*transcribed.m_displayTier));
		newSide.events.push_back(tierReading(clock->displayTier()));
	}

	void compare(const char* scenario)
	{
		INFO(scenario);
		REQUIRE(oldSide.events.size() == newSide.events.size());
		for (std::size_t index = 0u; index < oldSide.events.size(); ++index)
		{
			INFO("event " << index);
			CHECK(newSide.events[index] == oldSide.events[index]);
		}
		CHECK((clock->tierConsumer() != nullptr) == transcribed.m_replicatedTierConsumer.has_value());
		CHECK(newSide.manager.config.relayDelayFloorTicks == oldSide.manager.config.relayDelayFloorTicks);
		tally.scenarios += 1u;
		tally.events += oldSide.events.size();
		for (const std::string& event : oldSide.events)
			tally.count(event);
	}

	struct Tally
	{
		void count(const std::string& event)
		{
			for (const char* key : { "[ConnectionTier] tier", "[ConnectionTier] applied", "[ConnectionTier] floor",
					 "[RelayDelayFloor] floor =", "[RelayDelayFloor] received", "[RelayDelayFloor] floor=", "UPWARD", "DOWNWARD",
					 "no stall or skip", "[ClockSource.Window]", "[Dilation.Authority]", "request ", "publishDelay ", "setFloor ",
					 "engineRtt ", "authorityTick ", "clockSample ", "stepSeconds ", "serverTick ", "release 0x", "release none",
					 "displayTier arrived" })
			{
				if (event.find(key) != std::string::npos)
					++keys[key];
			}
		}

		uint32_t                        scenarios = 0u;
		std::size_t                     events = 0u;
		std::map<std::string, uint32_t> keys;
	};

	static inline Tally tally;

	ogsession::SessionRole  role;
	TickDriverThreadContext context;
	Side                    oldSide;
	Side                    newSide;
	TranscribedSession      transcribed;
	std::optional<Clock>    clock;
};

TimeConfig configWithFloor(int32_t floorTicks)
{
	TimeConfig config;
	config.relayDelayFloorTicks = floorTicks;
	return config;
}

SimClockRttSample sampleOf(double rttSeconds, uint32_t serverTick)
{
	SimClockRttSample sample;
	sample.rttSeconds = rttSeconds;
	sample.thetaSeconds = 0.0005;
	sample.t0Seconds = 1.0;
	sample.t1Seconds = 1.0 + rttSeconds / 2.0;
	sample.t2Seconds = 1.0 + rttSeconds / 2.0 + 0.0001;
	sample.t3Seconds = 1.0 + rttSeconds + 0.0001;
	sample.serverTick = serverTick;
	sample.serverTickAgeSeconds = 0.0002;
	return sample;
}

void tierChanges(bool predicts, int32_t floorTicks)
{
	Rig rig(predicts, configWithFloor(floorTicks));
	rig.beginTierConsumer();
	const std::array<std::pair<uint8_t, uint8_t>, 8> changes{ { { 0, 0 }, { 0, 2 }, { 2, 1 }, { 1, 3 }, { 3, 3 }, { 3, 0 },
		{ 0, 7 }, { 7, 1 } } };
	for (const auto& [oldTier, newTier] : changes)
		rig.connectionTier(oldTier, newTier);
	rig.compare(format("tier changes, %s, floor %d", predicts ? "client" : "authority", floorTicks).c_str());
}

void floorChanges(bool predicts, bool tierFirst)
{
	Rig rig(predicts, configWithFloor(0));
	rig.beginTierConsumer();
	if (tierFirst)
		rig.connectionTier(0, 1);
	for (const uint8_t floorTicks : { 6, 2, 0, 1, 3, 3, 200, 4, 255, 5, 0 })
		rig.relayFloor(floorTicks);
	rig.compare(format("floor changes, %s, %s", predicts ? "client" : "authority", tierFirst ? "tier first" : "no tier").c_str());
}
void scenarioTierChanges()
{
	for (const bool predicts : { true, false })
	{
		for (const int32_t floorTicks : { 0, 2, 3, 4, 6 })
			tierChanges(predicts, floorTicks);
	}
}

void scenarioFloorChanges()
{
	for (const bool predicts : { true, false })
	{
		floorChanges(predicts, true);
		floorChanges(predicts, false);
	}
}

void scenarioSampleThenTick()
{
	for (const TimeConfig::ClockRttSource source : { TimeConfig::ClockRttSource::SimClock, TimeConfig::ClockRttSource::Engine })
	{
		TimeConfig config;
		config.clockRttSource = source;
		Rig rig(true, config);
		rig.oldSide.link.ack = 0.0412;
		rig.newSide.link.ack = 0.0412;
		rig.oldSide.link.stats.pingsSent = 7u;
		rig.newSide.link.stats.pingsSent = 7u;
		rig.oldSide.link.stats.pongsReceived = 6u;
		rig.newSide.link.stats.pongsReceived = 6u;
		rig.at(100.0);
		rig.clockSample(sampleOf(0.040, 500u));
		rig.at(100.5);
		rig.authorityTick(501u, 0.0);
		rig.at(101.0);
		rig.authorityTick(502u, 0.052);
		rig.at(110.25);
		rig.clockSample(sampleOf(0.038, 1052u));
		rig.at(110.5);
		rig.authorityTick(1053u, 0.050);
		rig.at(111.0);
		rig.authorityTick(1054u, std::numeric_limits<double>::quiet_NaN());
		rig.authorityTick(1055u, std::numeric_limits<double>::infinity());
		rig.authorityTick(1056u, -1.0);
		rig.at(120.75);
		rig.authorityTick(1600u, 0.049);
		rig.clockSample(sampleOf(0.041, 1600u));
		rig.compare(source == TimeConfig::ClockRttSource::SimClock ? "sample then tick, SimClock" : "sample then tick, Engine");
	}
}

void scenarioWindowDue()
{
	{
		Rig rig(true, TimeConfig{});
		for (const double seconds : { 5.0, 14.999, 15.0, 15.001, 24.999999, 25.0, 25.0, 34.0, 35.5 })
		{
			rig.at(seconds);
			rig.authorityTick(static_cast<uint32_t>(seconds * 60.0), 0.030);
		}
		rig.compare("the window's due rule, client");
		const std::size_t lines = static_cast<std::size_t>(std::count_if(rig.newSide.events.begin(), rig.newSide.events.end(),
			[](const std::string& event) { return event.find("[ClockSource.Window]") != std::string::npos; }));
		CHECK(lines == 3u);
	}
	{
		Rig rig(false, TimeConfig{});
		for (const double seconds : { 5.0, 15.0, 25.0, 40.0 })
		{
			rig.at(seconds);
			rig.authorityTick(static_cast<uint32_t>(seconds * 60.0), 0.030);
		}
		rig.compare("the window's due rule, authority");
		for (const std::string& event : rig.newSide.events)
			CHECK(event.find("[ClockSource.Window]") == std::string::npos);
	}
}

void scenarioStepStartThenPublish()
{
	for (const bool predicts : { false, true })
	{
		Rig rig(predicts, TimeConfig{});
		rig.oldSide.manager.serverClock.tick = 600u;
		rig.newSide.manager.serverClock.tick = 600u;
		rig.at(10.000000);
		rig.stepStart();
		rig.takeRelease();
		rig.takeRelease();
		rig.at(10.001500);
		rig.publishServerTick(true);
		rig.at(10.016667);
		rig.stepStart();
		rig.takeRelease();
		rig.at(10.017000);
		rig.publishServerTick(false);
		rig.at(10.033334);
		rig.stepStart();
		rig.oldSide.manager.serverClock.tick = 601u;
		rig.newSide.manager.serverClock.tick = 601u;
		rig.at(10.034);
		rig.publishServerTick(true);
		rig.compare(predicts ? "step start then publish, client" : "step start then publish, authority");
		if (!predicts)
		{
			CHECK(std::find(rig.newSide.events.begin(), rig.newSide.events.end(), format("serverTick 600 start=%a", 10.0))
				!= rig.newSide.events.end());
			CHECK(std::find(rig.newSide.events.begin(), rig.newSide.events.end(), format("serverTick 601 start=%a", 10.033334))
				!= rig.newSide.events.end());
		}
	}
}

void scenarioRateLine()
{
	Rig rig(false, TimeConfig{});
	SimulationScheduler scheduler(SchedulerConfig{ 1.0 / 60.0, 4u }, 0.0);
	uint64_t refused = 0u;
	TickRateDoor door(scheduler, 0, true, refused);
	for (const double hostNowSeconds : { 0.0, 3.0, 9.999, 10.0, 12.0 })
		rig.rateDoor(hostNowSeconds, door);
	door.setRateScale(1.01);
	rig.rateDoor(12.5, door);
	rig.rateDoor(13.0, door);
	door.setRateScale(0.99);
	door.setRateScale(0.98);
	rig.rateDoor(14.0, door);
	rig.rateDoor(22.5, door);
	rig.rateDoor(24.0, door);
	rig.compare("the authority's rate line");
	const std::size_t lines = static_cast<std::size_t>(std::count_if(rig.newSide.events.begin(), rig.newSide.events.end(),
		[](const std::string& event) { return event.find("[Dilation.Authority]") != std::string::npos; }));
	CHECK(lines == 5u);
}

void scenarioStoppedDrain()
{
	Rig rig(true, configWithFloor(2));
	rig.beginTierConsumer();
	rig.clock->beginStoppedDrain();
	rig.transcribed.onConnectionTierReceived(0, 1);
	rig.clock->onConnectionTier(0, 1);
	rig.transcribed.onRelayDelayFloorReceived(3);
	rig.clock->onRelayFloor(3);
	rig.transcribed.onTimingInfoReceived(40u, 0.03, &rig.oldSide.link);
	rig.clock->onAuthorityTick(40u, 0.03, rig.newSide.link);
	rig.transcribed.onSimClockSample(sampleOf(0.031, 40u), &rig.oldSide.link);
	rig.clock->onClockSample(sampleOf(0.031, 40u), rig.newSide.link);
	rig.clock->endStoppedDrain();
	rig.compare("the stopped drain, on the frame thread");
}

void scenarioAdvisoryAndStartUp()
{
	for (const bool predicts : { true, false })
	{
		for (const int32_t floorTicks : { 0, 1, 2, 4, 9 })
		{
			Rig rig(predicts, configWithFloor(floorTicks));
			rig.transcribed.logRelayDelayFloorAdvisory(floorTicks);
			rig.clock->logRelayDelayFloorAdvisory(floorTicks);
			rig.beginTierConsumer();
			REQUIRE(rig.clock->tierConsumer() != nullptr);
			CHECK_FALSE(rig.clock->tierConsumer()->hasReceivedTier());
			rig.clock->endTierConsumer();
			rig.transcribed.m_replicatedTierConsumer.reset();
			CHECK(rig.clock->tierConsumer() == nullptr);
			rig.compare(format("advisory and start-up, %s, floor %d", predicts ? "client" : "authority", floorTicks).c_str());
		}
	}
}

void runEveryScenario()
{
	Rig::tally = Rig::Tally{};
	scenarioTierChanges();
	scenarioFloorChanges();
	scenarioSampleThenTick();
	scenarioWindowDue();
	scenarioStepStartThenPublish();
	scenarioRateLine();
	scenarioStoppedDrain();
	scenarioAdvisoryAndStartUp();
}
} // namespace sessionClockTest

using namespace sessionClockTest;

TEST_CASE("SessionClock.ATierChangeRecomputesThenLogsThenPaysAsTheTranscriptionDoes", "[Session][SessionClock]")
{
	scenarioTierChanges();
}

TEST_CASE("SessionClock.AFloorChangeUpAndDownClampsAdvisesRecomputesAndPaysAsTheTranscriptionDoes", "[Session][SessionClock]")
{
	scenarioFloorChanges();
}

TEST_CASE("SessionClock.AClockSampleThenAnAuthorityTickReachTheEstimatorInOrderAsTheTranscriptionDoes", "[Session][SessionClock]")
{
	scenarioSampleThenTick();
}

TEST_CASE("SessionClock.TheWindowLineIsDueTenSecondsAfterTheFirstFeedAndAfterEachLine", "[Session][SessionClock]")
{
	scenarioWindowDue();
}

TEST_CASE("SessionClock.TheAuthoritysStepStartIsTheTimeTheServerTickIsPublishedWith", "[Session][SessionClock]")
{
	scenarioStepStartThenPublish();
}

TEST_CASE("SessionClock.TheAuthorityRateLineIsDueAtTheFirstPumpEveryWindowAndOnARefusedChange", "[Session][SessionClock]")
{
	scenarioRateLine();
}

TEST_CASE("SessionClock.TheStoppedDrainsReceiptsRunOnTheFrameThread", "[Session][SessionClock]")
{
	scenarioStoppedDrain();
}

TEST_CASE("SessionClock.TheFloorAdvisoryAndTheTierCacheAtStartUp", "[Session][SessionClock]")
{
	scenarioAdvisoryAndStartUp();
}

TEST_CASE("SessionClock.TheTranscriptionComparisonIsNotVacuous", "[Session][SessionClock]")
{
	runEveryScenario();
	const Rig::Tally& tally = Rig::tally;
	CHECK(tally.scenarios == 32u);
	CHECK(tally.events == 877u);
	for (const char* key : { "[ConnectionTier] tier", "[ConnectionTier] applied", "[ConnectionTier] floor",
			 "[RelayDelayFloor] floor =", "[RelayDelayFloor] received", "[RelayDelayFloor] floor=", "UPWARD", "DOWNWARD",
			 "no stall or skip", "[ClockSource.Window]", "[Dilation.Authority]", "request ", "publishDelay ", "setFloor ",
			 "engineRtt ", "authorityTick ", "clockSample ", "stepSeconds ", "serverTick ", "release 0x", "release none",
			 "displayTier arrived" })
	{
		INFO(key);
		CHECK(tally.keys.count(key) == 1u);
	}
}

TEST_CASE("SessionClock.Survey", "[.][SessionClockSurvey]")
{
	runEveryScenario();
	const Rig::Tally& tally = Rig::tally;
	std::printf("[SessionClockSurvey] scenarios=%u comparedEvents=%zu\n", tally.scenarios, tally.events);
	for (const auto& [key, count] : tally.keys)
		std::printf("[SessionClockSurvey] %-28s %u\n", key.c_str(), count);
}

namespace sessionClockTest
{
// og-time-dilation task 2b's case (og-sim-net's tests/SimNetClockSampleRouteTest.cpp,
// SimNetClockRoute.ThePongsServerTickAgeRunsFromTheTicksStepStartToThePongsSendNotFromThePublishCallOrToThePingsReceipt),
// transcribed through the session clock: the tick's step starts at 10.000000 s, the clock publishes it at 10.001500 s, a
// ping is received at 10.004000 s and answered at 10.004321 s. The capturing endpoint and the manual clock are that file's
// (its Sent, CapturingEndpoint and ManualClock, and its ServerRig's connection set-up).
struct Sent
{
	ognet::ConnectionId    id = ognet::kInvalidConnection;
	ognet::Lane            lane = ognet::Lane::Control;
	std::vector<std::byte> bytes;
};

struct CapturingEndpoint
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		sent.push_back(Sent{ id, lane, std::vector<std::byte>(bytes.begin(), bytes.end()) });
	}
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { handlers[static_cast<std::size_t>(lane)] = handler; }
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId) {}

	void deliver(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		const ognet::LaneHandler& handler = handlers[static_cast<std::size_t>(lane)];
		REQUIRE(handler.fn != nullptr);
		handler.fn(handler.ctx, id, lane, bytes, recvSeconds);
	}

	std::array<ognet::LaneHandler, 8> handlers{};
	std::vector<Sent>                 sent;
};

struct ManualClock
{
	double now = 0.0;
	double nowSeconds() { return now; }
};

using ClockTransport = ogsimnet::SimNetTransport<CapturingEndpoint, 256>;
using ClockServer    = ogsimnet::SimNetClockServer<ClockTransport, ManualClock>;

static_assert(ogsession::SessionClockServer<ClockServer>, "og-sim-net's SimClock server is a SessionClockServer");

constexpr SimConnectionId kClientConn = 33u;

uint32_t serverTickAgeThroughTheClock()
{
	CapturingEndpoint endpoint;
	ManualClock       serverClockSource;
	ClockTransport    transport{};
	ClockServer       clockServer{ transport, serverClockSource };
	transport.installHandlers(endpoint);
	transport.markEndpointStarted();
	ognet::ConnectionEvent connected;
	connected.kind = ognet::ConnectionEventKind::Connected;
	connected.id = kClientConn;
	transport.onConnectionEvent(connected);
	REQUIRE(transport.hasClockHandler());

	Events events;
	Side   side(false, TimeConfig{});
	side.manager.serverClock.tick = 600u;
	TickDriverThreadContext context;
	context.configure<true>(tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning);
	Clock clock(side.engine, side.manager, side.inputResolution,
		ogsession::SessionThreadChecks(ogsession::SessionRole::Authority, &context, std::this_thread::get_id()));

	side.engine.now = 10.000000;
	clock.noteAuthorityStepStart();
	side.engine.now = 10.001500;
	serverClockSource.now = 10.001500;
	clock.publishServerTick(&clockServer);

	std::array<std::byte, ogsimnet::simNetClock::kPingBytes> ping{};
	REQUIRE(ogsimnet::simNetClock::writePing(ping, 9.25) == ping.size());
	serverClockSource.now = 10.004321;
	endpoint.deliver(kClientConn, ognet::Lane::SimClock, ping, 10.004000);
	REQUIRE(endpoint.sent.size() == 1u);
	const std::optional<ogsimnet::SimNetClockPong> pong = ogsimnet::simNetClock::readPong(endpoint.sent.back().bytes);
	REQUIRE(pong.has_value());
	CHECK(pong->serverTick == 600u);
	CHECK(clockServer.publishedServerTick() == 600u);
	return pong->serverTickAgeUs;
}
} // namespace sessionClockTest

TEST_CASE("SessionClock.ThePongsServerTickAgeRunsFromTheStepStartTheClockNotedNotFromItsPublish", "[Session][SessionClock]")
{
	CHECK(serverTickAgeThroughTheClock() == 4321u);
}
