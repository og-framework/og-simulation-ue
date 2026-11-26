// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionLatencyDiagnostics.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationTimeContext.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sessionLatencyDiagnosticsTest
{
using latencyBudget::Event;
using latencyBudget::EventKind;
using latencyBudget::Hop;
using latencyBudget::Point;
using latencyBudget::Stream;
using ogsession::SessionOp;

struct Input
{
	int value = 0;
};

struct Simulatable
{
};

struct Component
{
	const double* sampledSeconds = nullptr;
};

struct SampleSource
{
	const double* sampledSeconds = nullptr;
};

int g_sourceResolutions = 0;

struct Game
{
	static SampleSource inputSampleSource(const Component& component)
	{
		++g_sourceResolutions;
		return SampleSource{ component.sampledSeconds };
	}

	static double inputSampledSeconds(const SampleSource& source)
	{
		return source.sampledSeconds != nullptr ? *source.sampledSeconds : 0.0;
	}
};

struct Traits
{
	using PlayerInput       = Input;
	using Simulatable       = sessionLatencyDiagnosticsTest::Simulatable;
	using Component         = sessionLatencyDiagnosticsTest::Component;
	using InputSampleSource = SampleSource;
	using Game              = sessionLatencyDiagnosticsTest::Game;
};

struct ClockDiagnostics
{
	uint32_t skips = 0u;
	uint32_t stalls = 0u;
	uint32_t hardResyncs = 0u;
	uint32_t skipCount() const { return skips; }
	uint32_t stallCount() const { return stalls; }
	uint32_t hardResyncCount() const { return hardResyncs; }
};

struct Clock
{
	ClockDiagnostics diagnostics;
	const ClockDiagnostics& getDiagnostics() const { return diagnostics; }
};

struct Manager
{
	uint32_t tick = 0u;
	bool     prediction = false;
	Clock    clock;
	uint32_t currentIntegratedTick() const { return tick; }
	bool runsPrediction() const { return prediction; }
	const Clock& getClientClock() const { return clock; }
};

struct Storage
{
	std::vector<unsigned int> ids;

	template <typename S, typename F>
	void forEachSimulatable(F&& function) const
	{
		const Simulatable simulatable;
		for (const unsigned int id : ids)
			function(id, simulatable);
	}
};

struct Store
{
	struct Latest
	{
		bool         valid = false;
		std::uint8_t dA    = 0u;
	};
	Latest latest;
	Latest findLatest() const { return latest; }
};

struct ResolutionDiagnostics
{
	std::map<unsigned int, RelayedReadObservationRing> rings;

	template <typename S>
	const RelayedReadObservationRing* relayedReadObservations(unsigned int id) const
	{
		const auto found = rings.find(id);
		return found != rings.end() ? &found->second : nullptr;
	}
};

struct Resolution
{
	std::map<unsigned int, uint32_t> lastUsed;
	std::map<unsigned int, Store>    stores;
	ResolutionDiagnostics            diagnostics;

	template <typename S>
	uint32_t getLastUsedCaptureTick(unsigned int id) const
	{
		const auto found = lastUsed.find(id);
		return found != lastUsed.end() ? found->second : kNoInputCaptureTick;
	}

	template <typename S>
	const Store* findRemoteInputCache(unsigned int lane) const
	{
		const auto found = stores.find(lane);
		return found != stores.end() ? &found->second : nullptr;
	}

	const ResolutionDiagnostics& getDiagnostics() const { return diagnostics; }
};

struct Runtime
{
	std::optional<Manager> manager;
	Storage                storageValue;
	Resolution             resolution;

	const std::optional<Manager>& getManager() const { return manager; }
	const Storage& storage() const { return storageValue; }
	const Resolution& inputResolution() const { return resolution; }
};

struct Owner
{
	Runtime runtimeValue;
	const Runtime& runtime() const { return runtimeValue; }
};

struct PortLog
{
	double                   now = 0.0;
	std::vector<std::string> lines;
	std::vector<SessionOp>   ops;
};

struct Port
{
	PortLog* log = nullptr;

	double nowSeconds() const { return log->now; }
	void logLatency(const char* line) const { log->lines.emplace_back(line); }
	void checkOp(SessionOp op) const { log->ops.push_back(op); }
};

static_assert(ogsession::SessionLatencyPort<Port>, "the test port models SessionLatencyPort");

using Diagnostics = ogsession::SessionLatencyDiagnostics<Traits, Owner, Port, true>;
using DiagnosticsOff = ogsession::SessionLatencyDiagnostics<Traits, Owner, Port, false>;
using InputProvider = Diagnostics::InputProvider;

struct Posted
{
	Event    event;
	uint32_t healDelayTicks   = 0u;
	bool     healByCorrection = false;
};
} // namespace sessionLatencyDiagnosticsTest

namespace ogsession::sessionLatencyTest
{
struct Inspector
{
	template <typename D>
	static std::vector<sessionLatencyDiagnosticsTest::Posted> drain(D& diagnostics)
	{
		std::vector<sessionLatencyDiagnosticsTest::Posted> out;
		if (diagnostics.m_mailbox)
		{
			diagnostics.m_mailbox->drain([&](const auto& posted)
			{
				out.push_back({ posted.event, posted.healDelayTicks, posted.healByCorrection });
			});
		}
		return out;
	}

	template <typename D>
	static std::vector<SessionLatencyClockCounts> drainClock(D& diagnostics)
	{
		std::vector<SessionLatencyClockCounts> out;
		if (diagnostics.m_clockMailbox)
			diagnostics.m_clockMailbox->drain([&](const SessionLatencyClockCounts& counts) { out.push_back(counts); });
		return out;
	}

	template <typename D>
	static std::size_t seenReads(const D& diagnostics) { return diagnostics.m_seenReads_Physics.size(); }

	template <typename D>
	static std::size_t lastHanded(const D& diagnostics) { return diagnostics.m_lastHandedCorrectionTick_Runner.size(); }

	template <typename D>
	static bool hasProbe(const D& diagnostics) { return diagnostics.m_probe != nullptr; }
};
} // namespace ogsession::sessionLatencyTest

namespace sessionLatencyDiagnosticsTest
{
using Inspector = ogsession::sessionLatencyTest::Inspector;

struct Rig
{
	explicit Rig(bool isAuthority, bool prediction)
	{
		owner.runtimeValue.manager = Manager{};
		owner.runtimeValue.manager->prediction = prediction;
		diagnostics.begin(isAuthority);
	}

	Owner       owner;
	PortLog     log;
	Diagnostics diagnostics{ owner, Port{ &log } };
};

bool isStamp(const Posted& posted, Stream stream, unsigned int lane, uint32_t tick, Point point, double seconds)
{
	const Event& e = posted.event;
	return e.kind == EventKind::Stamp && e.stream == stream && e.lane == lane && e.tick == tick && e.point == point
		&& e.seconds == seconds && !posted.healByCorrection;
}

RelayedReadObservation observation(uint32_t simTick, uint32_t appliedCaptureTick, bool hasApplied,
	ScheduledRelayedReadOutcome outcome)
{
	RelayedReadObservation read;
	read.simTick               = simTick;
	read.appliedCaptureTick    = appliedCaptureTick;
	read.hasAppliedCaptureTick = hasApplied;
	read.outcome               = outcome;
	return read;
}
} // namespace sessionLatencyDiagnosticsTest

using namespace sessionLatencyDiagnosticsTest;

// Each case below is transcribed from UELatencyBudgetDiagnostics.h at root 7d58a26 (file:line of that version).

TEST_CASE("SessionLatencyDiagnostics.WrapKeepsPresenceResolvesTheSourceOnceAndStampsSampledThenCaptured", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:57-67 (wrapInputProvider), :231-258 (wrap and its lambda)
	Rig rig(false, true);
	const double sampled = 4.25;
	const Component component{ &sampled };
	g_sourceResolutions = 0;

	CHECK_FALSE(static_cast<bool>(rig.diagnostics.wrapInputProvider(3u, component, InputProvider{})));
	CHECK(g_sourceResolutions == 0);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyWrap });

	InputProvider wrapped = rig.diagnostics.wrapInputProvider(3u, component,
		[](const SimulationTimeStep&, const LocalInputCache<Input>&) { return Input{ 7 }; });
	REQUIRE(static_cast<bool>(wrapped));
	CHECK(g_sourceResolutions == 1);

	LocalInputCache<Input> cache;
	rig.log.ops.clear();
	rig.log.now = 5.5;
	CHECK(wrapped(SimulationTimeStep(41u, false), cache).value == 7);
	rig.log.now = 5.75;
	CHECK(wrapped(SimulationTimeStep(42u, false), cache).value == 7);
	CHECK(g_sourceResolutions == 1);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyCapture, SessionOp::LatencyCapture });

	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 4u);
	CHECK(isStamp(posted[0], Stream::Input, 3u, 41u, Point::InputSampled, 4.25));
	CHECK(isStamp(posted[1], Stream::Input, 3u, 41u, Point::Captured, 5.5));
	CHECK(isStamp(posted[2], Stream::Input, 3u, 42u, Point::InputSampled, 4.25));
	CHECK(isStamp(posted[3], Stream::Input, 3u, 42u, Point::Captured, 5.75));
}

TEST_CASE("SessionLatencyDiagnostics.AZeroSampleTimePostsTheCaptureAlone", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:248-255 (sampledSeconds > 0.0 gates the InputSampled stamp)
	Rig rig(false, true);
	const Component component{ nullptr };
	InputProvider wrapped = rig.diagnostics.wrapInputProvider(2u, component,
		[](const SimulationTimeStep&, const LocalInputCache<Input>&) { return Input{}; });
	LocalInputCache<Input> cache;
	rig.log.now = 1.0;
	wrapped(SimulationTimeStep(9u, false), cache);
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 1u);
	CHECK(isStamp(posted[0], Stream::Input, 2u, 9u, Point::Captured, 1.0));
}

TEST_CASE("SessionLatencyDiagnostics.PostBeforeBeginIsDroppedAndAfterBeginQueued", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:269-274 (post_Internal), :200-207 (begin's probe and mailboxes)
	Owner owner;
	PortLog log;
	Diagnostics diagnostics{ owner, Port{ &log } };
	diagnostics.post_Internal(Event::stamp(Stream::Input, 1u, 1u, Point::Handed, 1.0));
	CHECK_FALSE(Inspector::hasProbe(diagnostics));
	CHECK(Inspector::drain(diagnostics).empty());
	diagnostics.begin(true);
	CHECK(Inspector::hasProbe(diagnostics));
	diagnostics.post_Internal(Event::stamp(Stream::Input, 1u, 1u, Point::Handed, 1.0));
	CHECK(Inspector::drain(diagnostics).size() == 1u);
}

TEST_CASE("SessionLatencyDiagnostics.ClientReceivedStampsReceiptThenHealThenNetReceived", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:276-301 (stampClientReceived_Internal)
	Rig rig(false, true);
	rig.owner.runtimeValue.resolution.stores[4u].latest = Store::Latest{ true, 6u };
	rig.log.now = 2.0;

	rig.diagnostics.stampClientReceived_Internal(Stream::Input, 4u, 30u, 1.5);
	rig.diagnostics.stampClientReceived_Internal(Stream::State, 4u, 31u, 1.75);
	rig.diagnostics.stampClientReceived_Internal(Stream::State, 5u, 32u, 1.8);
	CHECK(rig.log.ops == std::vector<SessionOp>(3u, SessionOp::LatencyClientReceived));

	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 7u);
	CHECK(isStamp(posted[0], Stream::Input, 4u, 30u, Point::ClientReceived, 2.0));
	CHECK(isStamp(posted[1], Stream::Input, 4u, 30u, Point::NetReceived, 1.5));
	CHECK(isStamp(posted[2], Stream::State, 4u, 31u, Point::ClientReceived, 2.0));
	CHECK(posted[3].healByCorrection);
	CHECK(posted[3].healDelayTicks == 6u);
	CHECK(posted[3].event.stream == Stream::Input);
	CHECK(posted[3].event.point == Point::Healed);
	CHECK(posted[3].event.lane == 4u);
	CHECK(posted[3].event.tick == 31u);
	CHECK(posted[3].event.seconds == 2.0);
	CHECK(isStamp(posted[4], Stream::State, 4u, 31u, Point::NetReceived, 1.75));
	CHECK(isStamp(posted[5], Stream::State, 5u, 32u, Point::ClientReceived, 2.0));
	CHECK(isStamp(posted[6], Stream::State, 5u, 32u, Point::NetReceived, 1.8));
}

TEST_CASE("SessionLatencyDiagnostics.AnInvalidRelayStoreHealsNothing", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:291-292 (latest.valid gates the heal)
	Rig rig(false, true);
	rig.owner.runtimeValue.resolution.stores[4u].latest = Store::Latest{ false, 6u };
	rig.diagnostics.stampClientReceived_Internal(Stream::State, 4u, 31u, 1.75);
	CHECK(Inspector::drain(rig.diagnostics).size() == 2u);
}

TEST_CASE("SessionLatencyDiagnostics.TheClientsWireSampleIsHalfTheRoundTrip", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:303-307 (noteWireSample_Internal)
	Rig rig(false, true);
	rig.diagnostics.noteWireSample_Internal(0.030);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyClientWireSample });
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 1u);
	CHECK(posted[0].event.kind == EventKind::Sample);
	CHECK(posted[0].event.hop == Hop::Wire);
	CHECK(posted[0].event.seconds == 0.015);
}

TEST_CASE("SessionLatencyDiagnostics.HandedAndLeftStampBothAtNow", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:309-317 (stampHandedAndLeft_Internal)
	Rig rig(true, false);
	rig.log.now = 3.125;
	rig.diagnostics.stampHandedAndLeft_Internal(2u, 77u);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyHanded });
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 2u);
	CHECK(isStamp(posted[0], Stream::Input, 2u, 77u, Point::Handed, 3.125));
	CHECK(isStamp(posted[1], Stream::Input, 2u, 77u, Point::LeftProcess, 3.125));
}

TEST_CASE("SessionLatencyDiagnostics.ServerReceivedStampsNowThenTheDatagramsTime", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:319-327 (stampServerReceived_Internal)
	Rig rig(true, false);
	rig.log.now = 8.0;
	rig.diagnostics.stampServerReceived_Internal(3u, 12u, 7.5);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyServerReceived });
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 2u);
	CHECK(isStamp(posted[0], Stream::Input, 3u, 12u, Point::ServerReceived, 8.0));
	CHECK(isStamp(posted[1], Stream::Input, 3u, 12u, Point::NetReceived, 7.5));
}

TEST_CASE("SessionLatencyDiagnostics.ACorrectionTickIsHandedOncePerLane", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:329-341 (stampCorrectionHanded_Internal and its runner-side dedup map)
	Rig rig(true, false);
	rig.diagnostics.stampCorrectionHanded_Internal(1u, 50u, 9.0);
	rig.diagnostics.stampCorrectionHanded_Internal(1u, 50u, 9.5);
	rig.diagnostics.stampCorrectionHanded_Internal(2u, 50u, 9.5);
	rig.diagnostics.stampCorrectionHanded_Internal(1u, 51u, 10.0);
	CHECK(rig.log.ops == std::vector<SessionOp>(4u, SessionOp::LatencyCorrectionHanded));
	CHECK(Inspector::lastHanded(rig.diagnostics) == 2u);
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 6u);
	CHECK(isStamp(posted[0], Stream::State, 1u, 50u, Point::Handed, 9.0));
	CHECK(isStamp(posted[1], Stream::State, 1u, 50u, Point::LeftProcess, 9.0));
	CHECK(isStamp(posted[2], Stream::State, 2u, 50u, Point::Handed, 9.5));
	CHECK(isStamp(posted[3], Stream::State, 2u, 50u, Point::LeftProcess, 9.5));
	CHECK(isStamp(posted[4], Stream::State, 1u, 51u, Point::Handed, 10.0));
	CHECK(isStamp(posted[5], Stream::State, 1u, 51u, Point::LeftProcess, 10.0));
}

TEST_CASE("SessionLatencyDiagnostics.TheAuthoritysStepStartStampsEachUsedCaptureConsumed", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:343-365 (stampAfterStep_Internal, the authority branch)
	Rig rig(true, false);
	rig.owner.runtimeValue.storageValue.ids = { 1u, 2u, 3u };
	rig.owner.runtimeValue.resolution.lastUsed[1u] = 100u;
	rig.owner.runtimeValue.resolution.lastUsed[3u] = 98u;
	rig.diagnostics.stampAfterStep_Internal(4.5);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyAfterStep });
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 2u);
	CHECK(isStamp(posted[0], Stream::Input, 1u, 100u, Point::Consumed, 4.5));
	CHECK(isStamp(posted[1], Stream::Input, 3u, 98u, Point::Consumed, 4.5));
	CHECK(Inspector::drainClock(rig.diagnostics).empty());
}

TEST_CASE("SessionLatencyDiagnostics.WithoutAManagerTheStepDoorsPostNothing", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:346-347 and :439-440 (the no-mailbox or no-manager returns)
	Rig rig(true, false);
	rig.owner.runtimeValue.manager.reset();
	rig.owner.runtimeValue.storageValue.ids = { 1u };
	rig.owner.runtimeValue.resolution.lastUsed[1u] = 100u;
	rig.diagnostics.stampAfterStep_Internal(1.0);
	rig.diagnostics.stampStepEnd_Internal();
	CHECK(Inspector::drain(rig.diagnostics).empty());
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyAfterStep, SessionOp::LatencyStepEnd });
}

TEST_CASE("SessionLatencyDiagnostics.AClientsStepStartStampsThePendingCorrectionTheReadsAndTheClockOnChange", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:367-389 (stampAfterStep_Internal, the client branch) and :391-434 (postRelayedReads_Internal)
	Rig rig(false, true);
	Runtime& runtime = rig.owner.runtimeValue;
	runtime.manager->tick = 20u;
	runtime.storageValue.ids = { 2u };
	RelayedReadObservationRing& ring = runtime.resolution.diagnostics.rings[2u];
	ring.note(observation(18u, 14u, true, ScheduledRelayedReadOutcome::Hit));
	ring.note(observation(20u, 16u, true, ScheduledRelayedReadOutcome::Hit));

	rig.diagnostics.stampAfterStep_Internal(6.0);
	std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 3u);
	CHECK(posted[0].event.kind == EventKind::StampPending);
	CHECK(posted[0].event.stream == Stream::State);
	CHECK(posted[0].event.from == Point::ClientReceived);
	CHECK(posted[0].event.point == Point::Consumed);
	CHECK(posted[0].event.seconds == 6.0);
	CHECK(isStamp(posted[1], Stream::Input, 2u, 16u, Point::Consumed, 6.0));
	CHECK(isStamp(posted[2], Stream::Input, 2u, 16u, Point::Healed, 6.0));
	CHECK(Inspector::seenReads(rig.diagnostics) == 1u);
	CHECK(Inspector::drainClock(rig.diagnostics).empty());

	runtime.manager->tick = 21u;
	ring.note(observation(18u, 15u, true, ScheduledRelayedReadOutcome::Hit));
	ring.note(observation(21u, 17u, true, ScheduledRelayedReadOutcome::Miss));
	runtime.manager->clock.diagnostics.stalls = 2u;
	rig.diagnostics.stampAfterStep_Internal(6.5);
	posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 3u);
	CHECK(posted[0].event.kind == EventKind::StampPending);
	CHECK(posted[1].event.kind == EventKind::StampHealedIfPending);
	CHECK(posted[1].event.lane == 2u);
	CHECK(posted[1].event.tick == 15u);
	CHECK(posted[1].event.point == Point::Healed);
	CHECK(posted[1].event.seconds == 6.5);
	CHECK(isStamp(posted[2], Stream::Input, 2u, 17u, Point::Consumed, 6.5));
	const std::vector<ogsession::SessionLatencyClockCounts> counts = Inspector::drainClock(rig.diagnostics);
	REQUIRE(counts.size() == 1u);
	CHECK(counts[0].stalls == 2u);

	rig.diagnostics.stampAfterStep_Internal(7.0);
	CHECK(Inspector::drainClock(rig.diagnostics).empty());
}

TEST_CASE("SessionLatencyDiagnostics.TheReadShadowIsClearedAtTheProbesLaneLimit", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:395-401 (the shadow bounded at ProbeConfig{}.maxLanes)
	Rig rig(false, true);
	Runtime& runtime = rig.owner.runtimeValue;
	const uint32_t lanes = latencyBudget::ProbeConfig{}.maxLanes;
	for (unsigned int id = 1u; id <= lanes; ++id)
	{
		runtime.storageValue.ids.push_back(id);
		runtime.resolution.diagnostics.rings[id].note(observation(1u, 1u, true, ScheduledRelayedReadOutcome::Hit));
	}
	rig.diagnostics.stampAfterStep_Internal(1.0);
	CHECK(Inspector::seenReads(rig.diagnostics) == lanes);
	runtime.storageValue.ids.push_back(lanes + 1u);
	runtime.resolution.diagnostics.rings[lanes + 1u].note(observation(1u, 1u, true, ScheduledRelayedReadOutcome::Hit));
	rig.diagnostics.stampAfterStep_Internal(2.0);
	CHECK(Inspector::seenReads(rig.diagnostics) == 1u);
}

TEST_CASE("SessionLatencyDiagnostics.TheStepEndStampsEverySimulatableAtTheIntegratedTick", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:436-450 (stampStepEnd_Internal)
	Rig rig(true, false);
	rig.owner.runtimeValue.manager->tick = 33u;
	rig.owner.runtimeValue.storageValue.ids = { 1u, 4u };
	rig.log.now = 11.0;
	rig.diagnostics.stampStepEnd_Internal();
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyStepEnd });
	const std::vector<Posted> posted = Inspector::drain(rig.diagnostics);
	REQUIRE(posted.size() == 2u);
	CHECK(isStamp(posted[0], Stream::State, 1u, 33u, Point::StepEnd, 11.0));
	CHECK(isStamp(posted[1], Stream::State, 4u, 33u, Point::StepEnd, 11.0));
}

TEST_CASE("SessionLatencyDiagnostics.ForgetDropsTheLaneAndLeavesTheStepThreadsMaps", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:222-229 (forgetCharacter; UELatencyBudgetDiagnostics-guards.md G-05 at that root)
	Rig rig(false, true);
	rig.owner.runtimeValue.storageValue.ids = { 2u };
	rig.owner.runtimeValue.resolution.diagnostics.rings[2u].note(observation(1u, 1u, true, ScheduledRelayedReadOutcome::Hit));
	rig.diagnostics.stampAfterStep_Internal(1.0);
	rig.diagnostics.stampCorrectionHanded_Internal(2u, 9u, 1.0);
	rig.log.ops.clear();
	rig.diagnostics.forgetCharacter(2u);
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyForget });
	CHECK(Inspector::seenReads(rig.diagnostics) == 1u);
	CHECK(Inspector::lastHanded(rig.diagnostics) == 1u);
}

TEST_CASE("SessionLatencyDiagnostics.TheServersWireSampleGoesStraightToTheProbe", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:260-267 (noteWireSample: no mailbox; its game-thread checkf is the LatencyServerWireSample row)
	Rig rig(true, false);
	rig.log.now = 0.0;
	rig.diagnostics.tickWindow_Frame();
	rig.diagnostics.noteWireSample(0.010);
	CHECK(Inspector::drain(rig.diagnostics).empty());
	rig.log.now = 10.0;
	rig.diagnostics.tickWindow_Frame();
	bool sawWire = false;
	for (const std::string& line : rig.log.lines)
	{
		if (line.find("hop=WIRE ") != std::string::npos)
		{
			sawWire = true;
			CHECK(line.find("role=Server hop=WIRE p50=5.000 ") != std::string::npos);
			CHECK(line.find(" n=1 unit=ms ") != std::string::npos);
		}
	}
	CHECK(sawWire);
}

TEST_CASE("SessionLatencyDiagnostics.TheWindowDrainsStampsAndLogsTheHopsThenTheWindowLine", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:464-524 (tick_GameThread: the returns, the drain, Rendered, the window and its lines)
	Rig rig(false, true);
	rig.log.now = 100.0;
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.log.lines.empty());

	const Component component{ nullptr };
	InputProvider wrapped = rig.diagnostics.wrapInputProvider(1u, component,
		[](const SimulationTimeStep&, const LocalInputCache<Input>&) { return Input{}; });
	LocalInputCache<Input> cache;
	rig.log.now = 101.0;
	wrapped(SimulationTimeStep(5u, false), cache);
	rig.log.now = 101.0001;
	rig.diagnostics.stampHandedAndLeft_Internal(1u, 5u);
	rig.log.now = 105.0;
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.log.lines.empty());

	rig.log.now = 110.0;
	rig.diagnostics.tickWindow_Frame();
	REQUIRE(rig.log.lines.size() >= 2u);
	CHECK(rig.log.lines.back() ==
		"[LatencyBudget.Window] role=Client seconds=10.00 stale=0 duplicate=0 laneOverflow=0 mailboxDropped=0");
	bool sawH2 = false;
	bool sawH6L = false;
	for (std::size_t index = 0u; index + 1u < rig.log.lines.size(); ++index)
	{
		const std::string& line = rig.log.lines[index];
		CHECK(line.rfind("[LatencyBudget] role=Client hop=", 0) == 0);
		if (line.find("hop=H2 ") != std::string::npos)
		{
			sawH2 = true;
			CHECK(line.find("hop=H2 p50=0.100 ") != std::string::npos);
		}
		if (line.find("hop=H6L ") != std::string::npos)
		{
			sawH6L = true;
			CHECK(line.find(" healedByUse=0 healedByCorrection=0") != std::string::npos);
		}
	}
	CHECK(sawH2);
	CHECK(sawH6L);

	rig.owner.runtimeValue.manager.reset();
	rig.log.lines.clear();
	rig.log.now = 200.0;
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.log.lines.empty());
}

TEST_CASE("SessionLatencyDiagnostics.EveryDoorChecksItsOwnRow", "[Session][SessionLatency]")
{
	// og-session-runtime task 37: each door's SessionOp row (SessionThreads.h), asked first
	Rig rig(true, false);
	rig.log.ops.clear();
	rig.diagnostics.forgetCharacter(1u);
	rig.diagnostics.noteWireSample(0.01);
	rig.diagnostics.stampClientReceived_Internal(Stream::Input, 1u, 1u, 1.0);
	rig.diagnostics.noteWireSample_Internal(0.01);
	rig.diagnostics.stampHandedAndLeft_Internal(1u, 1u);
	rig.diagnostics.stampAfterStep_Internal(1.0);
	rig.diagnostics.stampStepEnd_Internal();
	rig.diagnostics.stampServerReceived_Internal(1u, 1u, 1.0);
	rig.diagnostics.stampCorrectionHanded_Internal(1u, 1u, 1.0);
	rig.diagnostics.onPostTickFlush();
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.log.ops == std::vector<SessionOp>{ SessionOp::LatencyForget, SessionOp::LatencyServerWireSample,
		SessionOp::LatencyClientReceived, SessionOp::LatencyClientWireSample, SessionOp::LatencyHanded,
		SessionOp::LatencyAfterStep, SessionOp::LatencyStepEnd, SessionOp::LatencyServerReceived,
		SessionOp::LatencyCorrectionHanded, SessionOp::LatencyPostTickFlush, SessionOp::LatencyWindow });
}

TEST_CASE("SessionLatencyDiagnostics.TheOffArmIsEmptyKeepsPresenceAndNeverCallsThePort", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:155-186 (the OG_LATENCY_BUDGET_DIAGNOSTICS 0 arm) and :57-67 (presence in both arms)
	static_assert(std::is_empty_v<DiagnosticsOff>,
		"the diagnostics-off arm holds no state, as the Unreal class at OG_LATENCY_BUDGET_DIAGNOSTICS 0 held none");
	Owner owner;
	PortLog log;
	DiagnosticsOff diagnostics{ owner, Port{ &log } };
	const Component component{ nullptr };
	g_sourceResolutions = 0;
	CHECK_FALSE(static_cast<bool>(diagnostics.wrapInputProvider(1u, component, InputProvider{})));
	CHECK(static_cast<bool>(diagnostics.wrapInputProvider(1u, component,
		[](const SimulationTimeStep&, const LocalInputCache<Input>&) { return Input{}; })));
	diagnostics.begin(true);
	diagnostics.forgetCharacter(1u);
	diagnostics.noteWireSample(0.01);
	diagnostics.post_Internal(Event{});
	diagnostics.stampClientReceived_Internal(Stream::State, 1u, 1u, 1.0);
	diagnostics.noteWireSample_Internal(0.01);
	diagnostics.stampHandedAndLeft_Internal(1u, 1u);
	diagnostics.stampAfterStep_Internal(1.0);
	diagnostics.stampStepEnd_Internal();
	diagnostics.stampServerReceived_Internal(1u, 1u, 1.0);
	diagnostics.stampCorrectionHanded_Internal(1u, 1u, 1.0);
	diagnostics.onPostTickFlush();
	diagnostics.tickWindow_Frame();
	diagnostics.restartClockWindow();
	CHECK(diagnostics.clockCountsInWindow() == ogsession::SessionLatencyClockCounts{});
	CHECK(g_sourceResolutions == 0);
	CHECK(log.ops.empty());
	CHECK(log.lines.empty());
}

TEST_CASE("SessionLatencyDiagnostics.TheClockWindowIsTheCountsSinceItsRestart", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:526-540 (clockCountsInWindow, restartClockWindow) and :478-483 (the clock mailbox drain)
	Rig rig(false, true);
	rig.owner.runtimeValue.manager->clock.diagnostics = ClockDiagnostics{ 1u, 2u, 3u };
	rig.diagnostics.stampAfterStep_Internal(1.0);
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.diagnostics.clockCountsInWindow() == ogsession::SessionLatencyClockCounts{ 1u, 2u, 3u });
	rig.diagnostics.restartClockWindow();
	rig.owner.runtimeValue.manager->clock.diagnostics = ClockDiagnostics{ 2u, 2u, 5u };
	rig.diagnostics.stampAfterStep_Internal(2.0);
	rig.diagnostics.tickWindow_Frame();
	CHECK(rig.diagnostics.clockCountsInWindow() == ogsession::SessionLatencyClockCounts{ 1u, 0u, 2u });
}

TEST_CASE("SessionLatencyDiagnostics.TheFormatsAreTheUnrealAdaptersTextByteForByte", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h:503-504, :511-512, :518-519 (the three TEXT() formats, concatenated as the compiler joins them)
	CHECK(std::string(ogsession::sessionLatencyLines::kHealHopLine) ==
		"[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms "
		"noStart=%u noEnd=%u outOfOrder=%u overflow=%u healedByUse=%u healedByCorrection=%u");
	CHECK(std::string(ogsession::sessionLatencyLines::kHopLine) ==
		"[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms "
		"noStart=%u noEnd=%u outOfOrder=%u overflow=%u");
	CHECK(std::string(ogsession::sessionLatencyLines::kWindowLine) ==
		"[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u "
		"mailboxDropped=%llu");
	CHECK(std::string(ogsession::sessionLatencyLines::kClient) == "Client");
	CHECK(std::string(ogsession::sessionLatencyLines::kServer) == "Server");
}
