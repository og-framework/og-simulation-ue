// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace simNetClockSampleRoute
{
struct CsPosition
{
	std::int32_t x = 0;
};

struct CsInput
{
	std::int32_t move = 0;
};
} // namespace simNetClockSampleRoute

template <>
struct SerializableFields<simNetClockSampleRoute::CsPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetClockSampleRoute::CsPosition, x)); }
};

template <>
struct SerializableFields<simNetClockSampleRoute::CsInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetClockSampleRoute::CsInput, move)); }
};

namespace simNetClockSampleRoute
{
using ogsimnet::SimNetClockSample;

using CsState = SimulationComposite<CsPosition>;

class CsAllState
{
public:
	const CsState& getState() const { return m_state; }
	CsState& editState() { return m_state; }

private:
	CsState m_state;
};

struct CsSim
{
	using StateType = CsState;
	using InputType = CsInput;

	CsAllState m_allState;
	CsAllState m_vizState;

	const CsAllState& getAllState() const { return m_allState; }
	CsAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const CsAllState& getVizState() const { return m_vizState; }
};

struct CsGame
{
	using Simulatables = SimulatableList<CsSim>;
};

struct Sent
{
	ognet::ConnectionId id = ognet::kInvalidConnection;
	ognet::Lane lane = ognet::Lane::Control;
	std::vector<std::byte> bytes;
};

struct CapturingEndpoint
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		sent.push_back(Sent{id, lane, std::vector<std::byte>(bytes.begin(), bytes.end())});
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
	std::vector<Sent> sent;
};

struct ManualClock
{
	double now = 0.0;
	double nowSeconds() { return now; }
};

using CsTransport = ogsimnet::SimNetTransport<CapturingEndpoint, 256>;
using CsClockClient = ogsimnet::SimNetClockClient<CsTransport, ManualClock>;

struct PlainClientHost
{
	std::vector<std::pair<std::uint32_t, double>> timing;
	std::vector<const char*> lines;

	double nowSeconds() { return 0.0; }
	double serverRttSeconds() { return 0.084; }
	void onTimingInfo(std::uint32_t tick, double rttSeconds) { timing.emplace_back(tick, rttSeconds); }
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char*) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

struct HookedClientHost : PlainClientHost
{
	std::vector<SimNetClockSample> samples;
	std::vector<std::size_t> timingCallsAtSample;

	void onClockSample(const SimNetClockSample& sample)
	{
		samples.push_back(sample);
		timingCallsAtSample.push_back(timing.size());
	}
};

static_assert(ogsimnet::SimNetClientHost<PlainClientHost> && !ogsimnet::SimNetClockSampleHook<PlainClientHost>
	&& ogsimnet::SimNetClockSampleHookWellFormed<PlainClientHost>);
static_assert(ogsimnet::SimNetClientHost<HookedClientHost> && ogsimnet::SimNetClockSampleHook<HookedClientHost>);

constexpr SimConnectionId kServerConn = 21u;

template <class HostT>
struct Rig
{
	Rig()
	{
		transport.installHandlers(endpoint);
		transport.markEndpointStarted();
		ognet::ConnectionEvent connected;
		connected.kind = ognet::ConnectionEventKind::Connected;
		connected.id = kServerConn;
		transport.onConnectionEvent(connected);
		session.drain();
		REQUIRE(session.serverConnection() == kServerConn);
	}

	double ping(double t0)
	{
		clock.now = t0;
		REQUIRE(clockClient.servicePing());
		REQUIRE(!endpoint.sent.empty());
		const Sent& ping = endpoint.sent.back();
		REQUIRE(ping.lane == ognet::Lane::SimClock);
		const std::optional<double> stamped = ogsimnet::simNetClock::readPing(ping.bytes);
		REQUIRE(stamped.has_value());
		return *stamped;
	}

	void pong(const ogsimnet::SimNetClockPong& pong, double t3)
	{
		std::array<std::byte, ogsimnet::simNetClock::kPongBytes> bytes{};
		REQUIRE(ogsimnet::simNetClock::writePong(bytes, pong) == bytes.size());
		endpoint.deliver(kServerConn, ognet::Lane::SimClock, bytes, t3);
	}

	void clockPublish(std::uint32_t tick, double recvSeconds)
	{
		std::array<std::byte, kMaxSimMessageBytes> bytes{};
		const std::size_t size = simEnvelope::writeClockPublish(bytes, tick);
		REQUIRE(size != 0u);
		endpoint.deliver(kServerConn, ognet::Lane::SimClock, std::span<const std::byte>(bytes.data(), size), recvSeconds);
	}

	CapturingEndpoint endpoint;
	ManualClock clock;
	CsTransport transport{ogsimnet::SimNetConfig{}};
	CsClockClient clockClient{transport, clock};
	HostT host;
	ogsimnet::SimNetClient<CsGame, HostT, CsTransport> session{host, transport};
};

bool sameBits(const SimNetClockSample& a, const SimNetClockSample& b)
{
	return a.rttSeconds == b.rttSeconds && a.thetaSeconds == b.thetaSeconds && a.serverTick == b.serverTick
		&& a.serverTickAgeSeconds == b.serverTickAgeSeconds && a.t0 == b.t0 && a.t1 == b.t1 && a.t2 == b.t2 && a.t3 == b.t3;
}

using CsClockServer = ogsimnet::SimNetClockServer<CsTransport, ManualClock>;

template <class ServerT, class... ArgsT>
concept PublishesServerTickWith = requires(ServerT& server, ArgsT... args) { server.publishServerTick(args...); };

static_assert(PublishesServerTickWith<CsClockServer, std::uint32_t, double> && !PublishesServerTickWith<CsClockServer, std::uint32_t>,
	"SimNetClockServer::publishServerTick takes the tick's step-start time and has no one-argument form. Was "
	"publishServerTick(tick), which read the server's clock at the publish, after the step (og-time-dilation task 2b)");

constexpr SimConnectionId kClientConn = 33u;

struct ServerRig
{
	ServerRig()
	{
		transport.installHandlers(endpoint);
		transport.markEndpointStarted();
		ognet::ConnectionEvent connected;
		connected.kind = ognet::ConnectionEventKind::Connected;
		connected.id = kClientConn;
		transport.onConnectionEvent(connected);
		REQUIRE(transport.hasClockHandler());
	}

	void publishAt(double tickStartSeconds, double callSeconds, std::uint32_t tick)
	{
		clock.now = callSeconds;
		clockServer.publishServerTick(tick, tickStartSeconds);
	}

	ogsimnet::SimNetClockPong answer(double t0, double recvSeconds, double sendSeconds)
	{
		std::array<std::byte, ogsimnet::simNetClock::kPingBytes> ping{};
		REQUIRE(ogsimnet::simNetClock::writePing(ping, t0) == ping.size());
		const std::size_t sentBefore = endpoint.sent.size();
		clock.now = sendSeconds;
		endpoint.deliver(kClientConn, ognet::Lane::SimClock, ping, recvSeconds);
		REQUIRE(endpoint.sent.size() == sentBefore + 1u);
		const Sent& sent = endpoint.sent.back();
		REQUIRE(sent.lane == ognet::Lane::SimClock);
		REQUIRE(sent.bytes.size() == ogsimnet::simNetClock::kPongBytes);
		const std::optional<ogsimnet::SimNetClockPong> pong = ogsimnet::simNetClock::readPong(sent.bytes);
		REQUIRE(pong.has_value());
		return *pong;
	}

	CapturingEndpoint endpoint;
	ManualClock clock;
	CsTransport transport{};
	CsClockServer clockServer{transport, clock};
};
} // namespace simNetClockSampleRoute

using namespace simNetClockSampleRoute;

TEST_CASE("SimNetClockSample.APongReachesTheHostsHookAsItsSampleAndNeverTheEnvelopeDecode", "[NetTransport][SimNetClockSample]")
{
	Rig<HookedClientHost> rig;
	REQUIRE(rig.transport.hasClockHandler());

	const double t0 = rig.ping(10.0);
	const ogsimnet::SimNetClockPong pong{t0, 10.7725, 10.7730, 4321u};
	const double t3 = 10.0505;
	rig.pong(pong, t3);
	rig.clockPublish(4322u, 10.0510);
	rig.session.drain();

	const SimNetClockSample expected = ogsimnet::simNetClock::sampleOf(pong, t3);
	REQUIRE(rig.host.samples.size() == 1u);
	CHECK(sameBits(rig.host.samples[0], expected));
	CHECK(rig.host.samples[0].serverTick == 4321u);
	CHECK(rig.session.stats().clockSamples == 1u);
	CHECK(rig.session.stats().clockSamplesUnheard == 0u);
	CHECK(rig.session.stats().badDecode == 0u);
	CHECK(rig.session.stats().messages == 2u);
	CHECK(rig.clockClient.stats().pongsReceived == 1u);

	REQUIRE(rig.host.timing.size() == 1u);
	CHECK(rig.host.timing[0] == std::make_pair(std::uint32_t{4322u}, 0.084));
	CHECK(rig.host.timingCallsAtSample == std::vector<std::size_t>{0u});

	SECTION("a session that refused the server's roster ignores the sample, as it ignores every message")
	{
		const ogsimnet::RosterMessage snapshot = ogsimnet::rosterCodec::encodeSnapshot(1u, std::span<const ogsimnet::RosterEntry>());
		const ogsimnet::RosterMessage gap = ogsimnet::rosterCodec::encodeAdd(3u, ogsimnet::RosterEntry{5u, 0u, ogsimnet::kRosterNotLocal});
		for (const ogsimnet::RosterMessage* message : {&snapshot, &gap})
		{
			std::array<std::byte, kMaxSimMessageBytes> bytes{};
			const std::size_t size = simEnvelope::writeRoster(bytes, message->view());
			REQUIRE(size != 0u);
			rig.endpoint.deliver(kServerConn, ognet::Lane::Control, std::span<const std::byte>(bytes.data(), size), 11.0);
		}
		rig.session.drain();
		REQUIRE(rig.session.refused());

		rig.clock.now = 11.5;
		rig.pong(ogsimnet::SimNetClockPong{11.45, 12.2, 12.2005, 4400u}, 11.5);
		rig.session.drain();
		CHECK(rig.host.samples.size() == 1u);
		CHECK(rig.session.stats().clockSamples == 1u);
		CHECK(rig.session.stats().ignoredWhileRefused == 1u);
	}
}

TEST_CASE("SimNetClockSample.AHostWithoutTheHookStaysWellFormedAndItsSamplesAreCountedNotDecoded", "[NetTransport][SimNetClockSample]")
{
	Rig<PlainClientHost> rig;

	const double t0 = rig.ping(20.0);
	rig.pong(ogsimnet::SimNetClockPong{t0, 20.31, 20.3102, 77u}, 20.061);
	rig.clockPublish(78u, 20.062);
	rig.session.drain();

	CHECK(rig.session.stats().clockSamples == 1u);
	CHECK(rig.session.stats().clockSamplesUnheard == 1u);
	CHECK(rig.session.stats().badDecode == 0u);
	CHECK(rig.session.stats().messages == 2u);
	REQUIRE(rig.host.timing.size() == 1u);
	CHECK(rig.host.timing[0] == std::make_pair(std::uint32_t{78u}, 0.084));
}

TEST_CASE("SimNetClockSample.ThePongCodecCarriesTheServerTickAgeRightAfterTheTickLittleEndian", "[NetTransport][SimNetClockSample]")
{
	const ogsimnet::SimNetClockPong pong{1.5, 2.25, 2.375, 0x01020304u, 0xA1B2C3D4u};
	std::array<std::byte, ogsimnet::simNetClock::kPongBytes> bytes{};
	REQUIRE(bytes.size() == 34u);
	REQUIRE(ogsimnet::simNetClock::writePong(bytes, pong) == 34u);
	const std::size_t ageOffset = simEnvelope::kHeaderBytes + 3u * ogsimnet::simNetClock::kStampBytes + sizeof(std::uint32_t);
	CHECK(ageOffset == 30u);
	CHECK(std::to_integer<std::uint32_t>(bytes[ageOffset - 4u]) == 0x04u);
	CHECK(std::to_integer<std::uint32_t>(bytes[ageOffset + 0u]) == 0xD4u);
	CHECK(std::to_integer<std::uint32_t>(bytes[ageOffset + 1u]) == 0xC3u);
	CHECK(std::to_integer<std::uint32_t>(bytes[ageOffset + 2u]) == 0xB2u);
	CHECK(std::to_integer<std::uint32_t>(bytes[ageOffset + 3u]) == 0xA1u);

	const std::optional<ogsimnet::SimNetClockPong> read = ogsimnet::simNetClock::readPong(bytes);
	REQUIRE(read.has_value());
	CHECK(read->t0 == pong.t0);
	CHECK(read->t1 == pong.t1);
	CHECK(read->t2 == pong.t2);
	CHECK(read->serverTick == pong.serverTick);
	CHECK(read->serverTickAgeUs == pong.serverTickAgeUs);
	CHECK_FALSE(ogsimnet::simNetClock::readPong(std::span<const std::byte>(bytes).first(30u)).has_value());
}

TEST_CASE("SimNetClockSample.ThePongsServerTickAgeRunsFromTheTicksStepStartToThePongsSendNotFromThePublishCallOrToThePingsReceipt", "[NetTransport][SimNetClockSample]")
{
	ServerRig rig;
	rig.publishAt(10.000000, 10.001500, 600u);
	const ogsimnet::SimNetClockPong pong = rig.answer(9.25, 10.004000, 10.004321);
	CHECK(pong.serverTick == 600u);
	CHECK(pong.t1 == 10.004000);
	CHECK(pong.t2 == 10.004321);
	CHECK(pong.serverTickAgeUs == 4321u);
	CHECK(rig.clockServer.publishedServerTick() == 600u);
	CHECK(rig.clockServer.stats().pingsAnswered == 1u);
}

TEST_CASE("SimNetClockSample.TheServerTickAgeWrapsWithTheLow32BitsOfTheMicroseconds", "[NetTransport][SimNetClockSample]")
{
	constexpr double kMicrosecondsPerWrap = 4294967296.0;
	ServerRig rig;
	rig.publishAt((kMicrosecondsPerWrap - 100.0) * 1e-6, (kMicrosecondsPerWrap - 100.0) * 1e-6, 7u);
	const ogsimnet::SimNetClockPong pong = rig.answer(4000.0, (kMicrosecondsPerWrap - 50.0) * 1e-6, (kMicrosecondsPerWrap + 150.0) * 1e-6);
	CHECK(pong.serverTick == 7u);
	CHECK(pong.serverTickAgeUs == 250u);
}

TEST_CASE("SimNetClockSample.BeforeTheFirstPublishThePongCarriesNoServerTickAndAZeroAge", "[NetTransport][SimNetClockSample]")
{
	ServerRig rig;
	const ogsimnet::SimNetClockPong pong = rig.answer(11.0, 12.345, 12.345678);
	CHECK(pong.serverTick == ogsimnet::kNoServerTick);
	CHECK(pong.serverTickAgeUs == 0u);
	CHECK(rig.clockServer.publishedServerTick() == ogsimnet::kNoServerTick);
}

TEST_CASE("SimNetClockSample.TheSampleRecordCarriesTheServerTickAgeFromThePongToTheHostsHook", "[NetTransport][SimNetClockSample]")
{
	const ogsimnet::SimNetClockPong pong{5.0, 5.75, 5.7505, 900u, 4321u};
	const SimNetClockSample sample = ogsimnet::simNetClock::sampleOf(pong, 5.051);
	CHECK(sample.serverTickAgeSeconds == 4321.0 * 1e-6);
	CHECK(std::abs(sample.serverTickAgeSeconds - 0.004321) < 1e-12);

	std::array<std::byte, ogsimnet::simNetClock::kSampleRecordBytes> record{};
	REQUIRE(record.size() == 58u);
	REQUIRE(ogsimnet::simNetClock::writeSampleRecord(record, sample) == record.size());
	const std::optional<SimNetClockSample> read = ogsimnet::simNetClock::readSampleRecord(record);
	REQUIRE(read.has_value());
	CHECK(sameBits(*read, sample));
	CHECK(read->serverTickAgeSeconds == sample.serverTickAgeSeconds);

	Rig<HookedClientHost> rig;
	const double t0 = rig.ping(5.0);
	const ogsimnet::SimNetClockPong routed{t0, 5.75, 5.7505, 900u, 4321u};
	rig.pong(routed, 5.051);
	rig.session.drain();
	REQUIRE(rig.host.samples.size() == 1u);
	CHECK(sameBits(rig.host.samples[0], ogsimnet::simNetClock::sampleOf(routed, 5.051)));
	CHECK(rig.host.samples[0].serverTickAgeSeconds == 4321.0 * 1e-6);
}
