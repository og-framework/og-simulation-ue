// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace simNetClockHoldTest
{
struct HoldPosition
{
	std::int32_t x = 0;
};

struct HoldInput
{
	std::int32_t move = 0;
};
} // namespace simNetClockHoldTest

template <>
struct SerializableFields<simNetClockHoldTest::HoldPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetClockHoldTest::HoldPosition, x)); }
};

template <>
struct SerializableFields<simNetClockHoldTest::HoldInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetClockHoldTest::HoldInput, move)); }
};

namespace simNetClockHoldTest
{
using HoldState = SimulationComposite<HoldPosition>;

class HoldAllState
{
public:
	const HoldState& getState() const { return m_state; }
	HoldState& editState() { return m_state; }

private:
	HoldState m_state;
};

struct HoldSim
{
	using StateType = HoldState;
	using InputType = HoldInput;

	HoldAllState m_allState;
	HoldAllState m_vizState;

	const HoldAllState& getAllState() const { return m_allState; }
	HoldAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const HoldAllState& getVizState() const { return m_vizState; }
};

struct HoldGame
{
	using Simulatables = SimulatableList<HoldSim>;
};

struct HoldEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
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
};

struct HoldClock
{
	double now = 0.0;
	double nowSeconds() { return now; }
};

using HoldTransport = ogsimnet::SimNetTransport<HoldEndpoint, 256>;
using HoldClockClient = ogsimnet::SimNetClockClient<HoldTransport, HoldClock>;

struct Event
{
	bool isSample = false;
	SimClockRttSample sample;
	std::uint32_t authorityTick = 0u;
};

bool sameEvent(const Event& a, const Event& b)
{
	const auto bits = [](double value) { return std::bit_cast<std::uint64_t>(value); };
	return a.isSample == b.isSample && a.authorityTick == b.authorityTick && bits(a.sample.rttSeconds) == bits(b.sample.rttSeconds)
		&& bits(a.sample.thetaSeconds) == bits(b.sample.thetaSeconds) && bits(a.sample.t0Seconds) == bits(b.sample.t0Seconds)
		&& bits(a.sample.t1Seconds) == bits(b.sample.t1Seconds) && bits(a.sample.t2Seconds) == bits(b.sample.t2Seconds)
		&& bits(a.sample.t3Seconds) == bits(b.sample.t3Seconds) && a.sample.serverTick == b.sample.serverTick
		&& bits(a.sample.serverTickAgeSeconds) == bits(b.sample.serverTickAgeSeconds);
}

struct FakeSession
{
	std::vector<Event> events;

	void onSimClockSample(const SimClockRttSample& sample) { events.push_back(Event{true, sample, 0u}); }
	void onTimingInfoReceived(std::uint32_t authorityTick, double) { events.push_back(Event{false, SimClockRttSample{}, authorityTick}); }
};

constexpr double kServerRttSeconds = 0.084;

struct HostBase
{
	FakeSession* session = nullptr;
	HoldClock* clock = nullptr;

	double nowSeconds() { return clock->now; }
	double serverRttSeconds() { return kServerRttSeconds; }
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

struct ForwardingHost : HostBase
{
	std::vector<std::string> lines;

	void onTimingInfo(std::uint32_t authorityTick, double roundTripSeconds)
	{
		if (session != nullptr)
			session->onTimingInfoReceived(authorityTick, roundTripSeconds);
	}

	void onClockSample(const SimClockRttSample& sample)
	{
		if (session != nullptr)
			session->onSimClockSample(sample);
	}

	void log(const char* line) { lines.emplace_back(line); }
};

struct HooklessHost : HostBase
{
	std::vector<std::uint32_t> ticks;

	void onTimingInfo(std::uint32_t authorityTick, double) { ticks.push_back(authorityTick); }
	void log(const char*) {}
};

static_assert(ogsimnet::SimNetClientHost<ForwardingHost> && ogsimnet::SimNetClockRttSampleHook<ForwardingHost>);
static_assert(ogsimnet::SimNetClientHost<HooklessHost> && !ogsimnet::SimNetClockRttSampleHook<HooklessHost>
	&& ogsimnet::SimNetClockRttSampleHookWellFormed<HooklessHost>);

constexpr SimConnectionId kServerConn = 9u;

ogsimnet::SimNetClockPong pongWithRtt(double rttSeconds)
{
	return ogsimnet::SimNetClockPong{0.0, rttSeconds / 2.0, rttSeconds / 2.0};
}

template <class HostT>
struct Rig
{
	Rig()
	{
		host.clock = &clock;
		transport.installHandlers(endpoint);
		transport.markEndpointStarted();
		ognet::ConnectionEvent connected;
		connected.kind = ognet::ConnectionEventKind::Connected;
		connected.id = kServerConn;
		transport.onConnectionEvent(connected);
		client.drain();
		REQUIRE(client.serverConnection() == kServerConn);
		REQUIRE_FALSE(client.listenerBound());
	}

	void pong(const ogsimnet::SimNetClockPong& message, double t3)
	{
		std::array<std::byte, ogsimnet::simNetClock::kPongBytes> bytes{};
		REQUIRE(ogsimnet::simNetClock::writePong(bytes, message) == bytes.size());
		endpoint.deliver(kServerConn, ognet::Lane::SimClock, bytes, t3);
	}

	void pongWithRtt(double rttSeconds) { pong(simNetClockHoldTest::pongWithRtt(rttSeconds), rttSeconds); }

	void tick(std::uint32_t authorityTick)
	{
		std::array<std::byte, kMaxSimMessageBytes> bytes{};
		const std::size_t size = simEnvelope::writeClockPublish(bytes, authorityTick);
		REQUIRE(size != 0u);
		endpoint.deliver(kServerConn, ognet::Lane::SimClock, std::span<const std::byte>(bytes.data(), size), clock.now);
	}

	void bind(FakeSession& boundSession)
	{
		host.session = &boundSession;
		client.bindListener();
	}

	void unbind()
	{
		client.unbindListener();
		host.session = nullptr;
	}

	HoldEndpoint endpoint;
	HoldClock clock;
	HoldTransport transport{ogsimnet::SimNetConfig{}};
	HoldClockClient clockClient{transport, clock};
	HostT host;
	ogsimnet::SimNetClient<HoldGame, HostT, HoldTransport> client{host, transport};
};

// Transcribed from UESimNetLanes.h at 3b8a211: the latch class (:76-107) and UESimNetLanesClientHost's onClockSample and
// onTimingInfo (:230-243). The old og-sim-net sample type is SimClockRttSample here (fields t0..t3 renamed t0Seconds..t3Seconds,
// the same values), UE_LOG is the heldLines recorder, FPlatformTime::Seconds() is the rig's clock.
class TranscribedClockSampleLatch
{
public:
	template <class SessionT>
	void onSample(SessionT* session, const SimClockRttSample& sample)
	{
		if (session == nullptr)
		{
			m_sample = sample;
			return;
		}
		m_sample.reset();
		session->onSimClockSample(sample);
	}

	template <class SessionT>
	std::optional<SimClockRttSample> onTimingInfo(SessionT* session, uint32_t authorityTick, double roundTripSeconds)
	{
		if (session == nullptr)
			return std::nullopt;
		const std::optional<SimClockRttSample> held = std::exchange(m_sample, std::nullopt);
		if (held.has_value())
			session->onSimClockSample(*held);
		session->onTimingInfoReceived(authorityTick, roundTripSeconds);
		return held;
	}

	bool holdsSample() const { return m_sample.has_value(); }

private:
	std::optional<SimClockRttSample> m_sample;
};

struct TranscribedHost
{
	FakeSession* session = nullptr;
	HoldClock* clock = nullptr;
	TranscribedClockSampleLatch clockLatch;
	std::vector<std::string> heldLines;

	void onClockSample(const SimClockRttSample& sample) { clockLatch.onSample(session, sample); }

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds)
	{
		const std::optional<SimClockRttSample> held = clockLatch.onTimingInfo(session, authorityTick, roundTripSeconds);
		if (held.has_value())
		{
			heldLines.emplace_back(ogsimnet::formatClockSourceHeld(
				authorityTick, held->rttSeconds * 1000.0, (clock->now - held->t3Seconds) * 1000.0).data());
		}
	}
};

struct ScriptRecord
{
	bool isTick = false;
	std::uint32_t authorityTick = 0u;
	ogsimnet::SimNetClockPong pong;
	double t3 = 0.0;
};

struct ScriptDrain
{
	int bindChange = 0;
	std::vector<ScriptRecord> records;
};

// The old world's drain, transcribed from SimNetClient.h at 3b8a211 (drain :185-196, onClockSample :577-584,
// deliverNewestClock :586-593): each sample went to the host's onClockSample in drain order, and the newest admitted tick
// to its onTimingInfo after the drain. Ticks in these scripts only rise, so the newest-tick filter admits each one.
void drainTheOldWay(TranscribedHost& host, const std::vector<ScriptRecord>& records)
{
	std::optional<std::uint32_t> pendingClock;
	for (const ScriptRecord& record : records)
	{
		if (record.isTick)
			pendingClock = record.authorityTick;
		else
			host.onClockSample(ogsimnet::simNetClock::sampleOf(record.pong, record.t3));
	}
	if (pendingClock)
		host.onTimingInfo(*pendingClock, kServerRttSeconds);
}

std::vector<ScriptDrain> scriptedJoin(std::mt19937& rng)
{
	std::uniform_int_distribution<int> percent(0, 99);
	std::uniform_int_distribution<int> recordCount(0, 3);
	std::uniform_int_distribution<int> rttMicroseconds(5000, 120000);
	std::vector<ScriptDrain> drains(14);
	std::uint32_t nextTick = 1000u + static_cast<std::uint32_t>(percent(rng));
	bool bound = false;
	for (std::size_t d = 0; d < drains.size(); ++d)
	{
		const int roll = percent(rng);
		if (!bound && (roll < 25 || d == drains.size() / 2u))
		{
			drains[d].bindChange = 1;
			bound = true;
		}
		else if (bound && roll < 8)
		{
			drains[d].bindChange = -1;
			bound = false;
		}
		const int count = recordCount(rng);
		for (int r = 0; r < count; ++r)
		{
			ScriptRecord record;
			record.isTick = percent(rng) < 45;
			if (record.isTick)
			{
				record.authorityTick = nextTick;
				nextTick += 1u + static_cast<std::uint32_t>(percent(rng) % 3);
			}
			else
			{
				const double rtt = rttMicroseconds(rng) * 1e-6;
				record.t3 = 1.0 + static_cast<double>(d) * 0.05 + static_cast<double>(r) * 0.001;
				record.pong = ogsimnet::SimNetClockPong{record.t3 - rtt, record.t3 - rtt / 2.0, record.t3 - rtt / 2.0 + 0.0001,
					static_cast<std::uint32_t>(nextTick - 2u), static_cast<std::uint32_t>(percent(rng) * 100)};
			}
			drains[d].records.push_back(record);
		}
	}
	return drains;
}
} // namespace simNetClockHoldTest

using namespace simNetClockHoldTest;

TEST_CASE("ClockSampleLatch.TheNewestUnboundSampleIsHeldAndSeedsTheEstimatorBeforeTheFirstAuthorityTick",
	"[NetTransport][SimNetClockHold]")
{
	Rig<ForwardingHost> rig;
	FakeSession session;
	rig.pongWithRtt(0.087);
	rig.client.drain();
	rig.pongWithRtt(0.071);
	rig.client.drain();
	rig.tick(1500u);
	rig.client.drain();
	{
		INFO("unbound: the newest sample is held and nothing reaches a session");
		CHECK((rig.client.heldClockSample().has_value() && rig.client.heldClockSample()->rttSeconds == 0.071
			&& session.events.empty()));
	}

	rig.clock.now = 0.103;
	rig.bind(session);
	rig.tick(1502u);
	rig.client.drain();
	rig.tick(1503u);
	rig.client.drain();
	{
		INFO("the first bound tick reports the held sample it fed (the [ClockSource.Held] line's input)");
		REQUIRE(rig.host.lines.size() == 1u);
		CHECK(rig.host.lines[0]
			== "[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick 1502: "
			   "rttMs=71.000 heldMs=32.0");
	}
	{
		INFO("bound: the held sample, then the first tick, then the next tick");
		REQUIRE(session.events.size() == 3u);
	}
	{
		INFO("the first event is the newest unbound sample (71 ms, not 87 ms)");
		CHECK((session.events[0].isSample && session.events[0].sample.rttSeconds == 0.071));
	}
	{
		INFO("the first authority tick follows the sample");
		CHECK((!session.events[1].isSample && session.events[1].authorityTick == 1502u));
	}
	{
		INFO("the second tick is not preceded by the sample again");
		CHECK((!session.events[2].isSample && session.events[2].authorityTick == 1503u));
	}
	{
		INFO("the sample is fed once");
		CHECK_FALSE(rig.client.heldClockSample().has_value());
	}
}

TEST_CASE("ClockSampleLatch.ABoundSampleSupersedesTheHeldOne", "[NetTransport][SimNetClockHold]")
{
	Rig<ForwardingHost> rig;
	FakeSession session;
	rig.pongWithRtt(0.090);
	rig.client.drain();
	REQUIRE(rig.client.heldClockSample().has_value());
	rig.bind(session);
	rig.pongWithRtt(0.075);
	rig.client.drain();
	rig.tick(1600u);
	rig.client.drain();
	{
		INFO("nothing held is reported");
		CHECK(rig.host.lines.empty());
	}
	{
		INFO("a bound sample supersedes the held one: sample, tick");
		REQUIRE(session.events.size() == 2u);
	}
	{
		INFO("the bound sample reaches the session at once");
		CHECK((session.events[0].isSample && session.events[0].sample.rttSeconds == 0.075));
	}
	{
		INFO("the tick follows with no replay of the older held sample");
		CHECK((!session.events[1].isSample && session.events[1].authorityTick == 1600u));
	}
}

TEST_CASE("ClockSampleLatch.WithNothingHeldATickArrivesAlone", "[NetTransport][SimNetClockHold]")
{
	Rig<ForwardingHost> rig;
	FakeSession session;
	rig.bind(session);
	rig.tick(1700u);
	rig.client.drain();
	INFO("with nothing held a tick arrives alone");
	CHECK((session.events.size() == 1u && !session.events[0].isSample && session.events[0].authorityTick == 1700u));
	CHECK(rig.host.lines.empty());
}

TEST_CASE("ClockSampleLatch.TheSessionsHoldMatchesTheUELatchItReplacedOnScriptedJoins", "[NetTransport][SimNetClockHold]")
{
	std::mt19937 rng(20261124u);
	std::size_t heldLines = 0u;
	std::size_t superseded = 0u;
	std::size_t rebinds = 0u;
	std::size_t events = 0u;
	for (int join = 0; join < 300; ++join)
	{
		const std::vector<ScriptDrain> drains = scriptedJoin(rng);
		Rig<ForwardingHost> rig;
		FakeSession session;
		HoldClock oldClock;
		TranscribedHost oldHost;
		oldHost.clock = &oldClock;
		FakeSession oldSession;
		bool everBound = false;
		for (std::size_t d = 0; d < drains.size(); ++d)
		{
			const ScriptDrain& drain = drains[d];
			rig.clock.now = 1.0 + static_cast<double>(d) * 0.05 + 0.04;
			oldClock.now = rig.clock.now;
			if (drain.bindChange > 0)
			{
				rebinds += everBound ? 1u : 0u;
				everBound = true;
				rig.bind(session);
				oldHost.session = &oldSession;
			}
			else if (drain.bindChange < 0)
			{
				rig.unbind();
				oldHost.session = nullptr;
			}
			if (oldHost.clockLatch.holdsSample() && oldHost.session != nullptr && !drain.records.empty() && !drain.records.front().isTick)
				++superseded;
			for (const ScriptRecord& record : drain.records)
			{
				if (record.isTick)
					rig.tick(record.authorityTick);
				else
					rig.pong(record.pong, record.t3);
			}
			rig.client.drain();
			drainTheOldWay(oldHost, drain.records);

			INFO("join " << join << ", drain " << d);
			REQUIRE(session.events.size() == oldSession.events.size());
			for (std::size_t e = 0; e < session.events.size(); ++e)
				REQUIRE(sameEvent(session.events[e], oldSession.events[e]));
			REQUIRE(rig.host.lines == oldHost.heldLines);
			REQUIRE(rig.client.heldClockSample().has_value() == oldHost.clockLatch.holdsSample());
		}
		heldLines += rig.host.lines.size();
		events += session.events.size();
	}
	INFO("the scripts exercise the hold: a held sample fed, one superseded by a bound sample, a re-bind after an unbind");
	CHECK(heldLines > 50u);
	CHECK(superseded > 10u);
	CHECK(rebinds > 10u);
	CHECK(events > 1000u);
}

TEST_CASE("ClockSampleLatch.AHostWithoutTheHookHoldsNothingAndCountsItsSamplesUnheard", "[NetTransport][SimNetClockHold]")
{
	Rig<HooklessHost> rig;
	rig.pongWithRtt(0.080);
	rig.client.drain();
	CHECK_FALSE(rig.client.heldClockSample().has_value());
	rig.client.bindListener();
	rig.tick(1800u);
	rig.client.drain();
	CHECK(rig.client.stats().clockSamples == 1u);
	CHECK(rig.client.stats().clockSamplesUnheard == 1u);
	CHECK(rig.host.ticks == std::vector<std::uint32_t>{1800u});
}
