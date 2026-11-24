// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "SimNetTestRig.h"
#include "VirtualNetwork.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace simNetClockTest
{
using simNetEquivalence::Backend;
using simNetEquivalence::Endpoint;
using simNetEquivalence::EqGame;
using simNetEquivalence::Transport;
using ognet::test::TestHost;
using ognet::test::VirtualNetwork;
using ogsimnet::SimNetClockSample;
using ogsimnet::SimNetConfig;
using ClockServer = ogsimnet::SimNetClockServer<Transport, TestHost>;
using ClockClient = ogsimnet::SimNetClockClient<Transport, TestHost>;

constexpr double kStartSeconds = 1.0;
constexpr double kStepSeconds = 1.0 / 32768.0;
constexpr double kTickSeconds = 1.0 / 60.0;
constexpr uint32_t kFirstServerTick = 500u;
constexpr uint8_t kServerHostIndex = 1u;
constexpr uint8_t kClientHostIndex = 10u;
constexpr uint16_t kServerPort = 7777u;
constexpr double kConnectBudgetSeconds = 3.0;
constexpr double kRunSeconds = 2.0;
constexpr double kEngineRttSeconds = 0.0625;
constexpr double kMs = 1e-3;
constexpr double kToleranceSeconds = 0.1 * kMs;

struct OffsetClock
{
	const VirtualNetwork* network = nullptr;
	double offsetSeconds = 0.0;

	static double read(void* ctx)
	{
		const OffsetClock& self = *static_cast<const OffsetClock*>(ctx);
		return self.network->nowSeconds() + self.offsetSeconds;
	}

	ognet::EmulatorClockFn fn() { return ognet::EmulatorClockFn{&OffsetClock::read, this}; }
};

struct Peer
{
	Peer(VirtualNetwork& network, uint8_t hostIndex, uint64_t seed, OffsetClock& clock, SimNetConfig config)
		: backend(network.addHost(hostIndex), network.clock())
		, host(clock.fn(), seed)
		, transport(std::make_unique<Transport>(config))
		, endpoint(host, backend)
	{
	}

	void start(ognet::Role role)
	{
		transport->installHandlers(endpoint);
		endpoint.setConnectionHandler(transport->connectionHandler());
		ognet::EndpointConfig config;
		config.role = role;
		if (role == ognet::Role::Server)
			config.bind.port = kServerPort;
		config.buildIdentity.fill(0x5A);
		REQUIRE(endpoint.start(config));
		transport->markEndpointStarted();
	}

	void service()
	{
		for (int pass = 0; pass < 8; ++pass)
		{
			if (endpoint.serviceUntil(host.nowSeconds()).datagrams == 0u)
				return;
		}
	}

	Backend backend;
	TestHost host;
	std::unique_ptr<Transport> transport;
	std::unique_ptr<ClockServer> clockServer;
	std::unique_ptr<ClockClient> clockClient;
	Endpoint endpoint;
};

struct LinkSpec
{
	double upMs = 20.0;
	double downMs = 30.0;
	double thetaSeconds = 0.75;
	bool clock = true;
	bool publishTicks = true;
};

struct Publication
{
	double atSeconds = 0.0;
	uint32_t tick = 0u;
};

class ClockRig
{
	LinkSpec m_spec;
	VirtualNetwork m_network;
	OffsetClock m_serverClock;
	OffsetClock m_clientClock;

public:
	explicit ClockRig(LinkSpec spec) : m_spec(spec), m_serverClock{&m_network, 0.0}, m_clientClock{&m_network, -spec.thetaSeconds}
	{
		m_network.setNow(kStartSeconds);
		server = std::make_unique<Peer>(m_network, kServerHostIndex, 100u, m_serverClock, SimNetConfig{});
		client = std::make_unique<Peer>(m_network, kClientHostIndex, 200u, m_clientClock, SimNetConfig{});
		ognet::EmulatorConfig lag;
		lag.outbound.lagMinMs = spec.upMs;
		lag.outbound.lagMaxMs = spec.upMs;
		lag.inbound.lagMinMs = spec.downMs;
		lag.inbound.lagMaxMs = spec.downMs;
		REQUIRE(client->backend.configure(lag));
		if (spec.clock)
		{
			server->clockServer = std::make_unique<ClockServer>(*server->transport, server->host);
			client->clockClient = std::make_unique<ClockClient>(*client->transport, client->host);
		}
		server->start(ognet::Role::Server);
		client->start(ognet::Role::Client);
		client->endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		REQUIRE(runUntil(kConnectBudgetSeconds, [&] { return client->transport->serverConnection() != kInvalidSimConnection; }));
		samples.clear();
	}

	double now() const { return m_network.nowSeconds(); }

	template <class DoneT>
	bool runUntil(double budgetSeconds, DoneT done)
	{
		const double until = now() + budgetSeconds;
		while (now() < until)
		{
			if (done())
				return true;
			step();
		}
		return done();
	}

	void runFor(double seconds)
	{
		const double until = now() + seconds;
		while (now() < until)
			step();
	}

	void step()
	{
		++m_step;
		m_network.setNow(kStartSeconds + static_cast<double>(m_step) * kStepSeconds);
		if (now() >= m_nextFrameSeconds)
		{
			frame();
			m_nextFrameSeconds += kTickSeconds;
		}
		client->service();
		server->service();
		client->service();
		if (client->clockClient && pinging)
			client->clockClient->servicePing();
		client->service();
	}

	uint32_t expectedTickAt(double serverSeconds) const
	{
		uint32_t tick = ogsimnet::kNoServerTick;
		for (const Publication& publication : publications)
		{
			if (publication.atSeconds <= serverSeconds)
				tick = publication.tick;
		}
		return tick;
	}

	ClockServer& clockServer() { return *server->clockServer; }
	ClockClient& clockClient() { return *client->clockClient; }

	std::unique_ptr<Peer> server;
	std::unique_ptr<Peer> client;
	std::vector<SimNetClockSample> samples;
	std::vector<std::vector<std::byte>> clockMessages;
	std::vector<Publication> publications;
	uint64_t serverMessages = 0u;
	bool pinging = true;

private:
	void frame()
	{
		if (m_spec.publishTicks && server->clockServer)
		{
			const uint32_t tick = kFirstServerTick + m_frames;
			const double tickStartSeconds = server->host.nowSeconds();
			server->clockServer->publishServerTick(tick, tickStartSeconds);
			publications.push_back(Publication{tickStartSeconds, tick});
		}
		++m_frames;
		client->transport->inbox().drain([&](const InboundSimRecord& record) {
			if (record.kind == InboundKind::Connected)
			{
				client->transport->setServerConnection(record.conn);
				return;
			}
			if (record.kind == InboundKind::Disconnected)
			{
				client->transport->setServerConnection(kInvalidSimConnection);
				return;
			}
			if (client->clockClient)
			{
				if (const std::optional<SimNetClockSample> sample = client->clockClient->readSample(record))
				{
					samples.push_back(*sample);
					return;
				}
			}
			if (record.lane == SimLane::SimClock)
			{
				const std::span<const std::byte> payload = record.payload();
				clockMessages.emplace_back(payload.begin(), payload.end());
			}
		});
		server->transport->inbox().drain([&](const InboundSimRecord& record) {
			serverMessages += record.kind == InboundKind::Message ? 1u : 0u;
		});
	}

	uint64_t m_step = 0u;
	uint32_t m_frames = 0u;
	double m_nextFrameSeconds = kStartSeconds;
};

struct Extent
{
	double min = 0.0;
	double max = 0.0;
};

template <class FieldT>
Extent extentOf(const std::vector<SimNetClockSample>& samples, FieldT field)
{
	Extent out{field(samples.front()), field(samples.front())};
	for (const SimNetClockSample& sample : samples)
	{
		out.min = std::min(out.min, field(sample));
		out.max = std::max(out.max, field(sample));
	}
	return out;
}

void quote(const char* label, const std::vector<SimNetClockSample>& samples)
{
	if (samples.empty())
	{
		std::printf("%s: 0 samples\n", label);
		return;
	}
	const Extent rtt = extentOf(samples, [](const SimNetClockSample& s) { return s.rttSeconds; });
	const Extent theta = extentOf(samples, [](const SimNetClockSample& s) { return s.thetaSeconds; });
	const Extent turnaround = extentOf(samples, [](const SimNetClockSample& s) { return s.t2 - s.t1; });
	const Extent drainDelay = extentOf(samples, [](const SimNetClockSample& s) { return s.t3 - s.t0 - s.rttSeconds; });
	std::printf("%s: %zu samples, rtt min/max = %.6f/%.6f ms, theta min/max = %.6f/%.6f ms, t2-t1 min/max = %.6f/%.6f ms, "
				"(t3-t0)-rtt max = %.6f ms\n",
		label, samples.size(), rtt.min / kMs, rtt.max / kMs, theta.min / kMs, theta.max / kMs, turnaround.min / kMs,
		turnaround.max / kMs, drainDelay.max / kMs);
}

void requireEverySample(const std::vector<SimNetClockSample>& samples, double rttSeconds, double thetaSeconds)
{
	REQUIRE(samples.size() >= static_cast<size_t>(kRunSeconds / ogsimnet::kClockPingIntervalSeconds) - 2u);
	for (const SimNetClockSample& sample : samples)
	{
		CHECK(std::abs(sample.rttSeconds - rttSeconds) <= kToleranceSeconds);
		CHECK(std::abs(sample.thetaSeconds - thetaSeconds) <= kToleranceSeconds);
	}
}

struct CountingClientHost
{
	double now = 0.0;
	uint64_t serverRttCalls = 0u;
	std::vector<std::pair<uint32_t, double>> timing;

	double nowSeconds() { return now; }
	double serverRttSeconds()
	{
		++serverRttCalls;
		return kEngineRttSeconds;
	}
	void onTimingInfo(uint32_t tick, double rttSeconds) { timing.emplace_back(tick, rttSeconds); }
	void onRelayFloor(uint8_t, bool) {}
	void onConnectionTier(uint8_t, uint8_t, bool) {}
	void onRosterAdded(uint8_t) {}
	void onRosterRemoved(uint8_t) {}
	void onWireMismatch(uint8_t, uint8_t) {}
	void log(const char*) {}
	void stampClientReceived(ogsimnet::SimNetStream, uint8_t, uint32_t, double) {}
};

struct CountingServerHost
{
	double now = 0.0;
	uint64_t rttCalls = 0u;

	double nowSeconds() { return now; }
	double rttMs(SimConnectionId)
	{
		++rttCalls;
		return kEngineRttSeconds / kMs;
	}
	int32_t receptionTick() { return 0; }
	void log(const char*) {}
	void stampServerReceived(uint8_t, uint32_t, double) {}
	void stampHanded(uint8_t, uint32_t) {}
};

static_assert(ogsimnet::SimNetClientHost<CountingClientHost> && ogsimnet::SimNetServerHost<CountingServerHost>);

class ShadowTee
{
public:
	using ChannelTransport = Transport::ChannelTransport;

	class Inbox
	{
	public:
		explicit Inbox(ShadowTee& tee) : m_tee(tee) {}

		template <class Fn>
		size_t drain(Fn&& fn)
		{
			return m_tee.m_inner.inbox().drain([&](const InboundSimRecord& record) {
				if (m_tee.m_clock != nullptr)
				{
					if (const std::optional<SimNetClockSample> sample = m_tee.m_clock->readSample(record))
					{
						m_tee.shadow.push_back(*sample);
						return;
					}
				}
				fn(record);
			});
		}

		SimInboxStats stats() const { return m_tee.m_inner.inbox().stats(); }

	private:
		ShadowTee& m_tee;
	};

	ShadowTee(Transport& inner, const ClockClient* clock) : m_inner(inner), m_clock(clock) {}

	Inbox inbox() { return Inbox(*this); }
	void endInputSends() { m_inner.endInputSends(); }
	ChannelTransport channelTransport(uint8_t simId) { return m_inner.channelTransport(simId); }
	void setServerConnection(SimConnectionId id) { m_inner.setServerConnection(id); }
	SimConnectionId serverConnection() const { return m_inner.serverConnection(); }

	std::vector<SimNetClockSample> shadow;

private:
	Transport& m_inner;
	const ClockClient* m_clock;
};

using ServerTee = simNetEquivalence::ServerTee<Transport>;
using ServerSession = ogsimnet::SimNetServer<EqGame, CountingServerHost, ServerTee>;
using ClientSession = ogsimnet::SimNetClient<EqGame, CountingClientHost, ShadowTee>;

struct SessionRun
{
	std::vector<std::pair<uint32_t, double>> timing;
	uint64_t serverRttCalls = 0u;
	uint64_t rttMsCalls = 0u;
	uint64_t clientMessages = 0u;
	uint64_t badDecode = 0u;
	uint64_t clocksDelivered = 0u;
	std::vector<SimNetClockSample> shadow;
};

SessionRun runSessions(bool clock)
{
	VirtualNetwork network;
	network.setNow(kStartSeconds);
	OffsetClock serverClock{&network, 0.0};
	OffsetClock clientClock{&network, -0.75};
	Peer server(network, kServerHostIndex, 100u, serverClock, SimNetConfig{});
	Peer client(network, kClientHostIndex, 200u, clientClock, SimNetConfig{});
	ognet::EmulatorConfig lag;
	lag.outbound.lagMinMs = lag.outbound.lagMaxMs = 20.0;
	lag.inbound.lagMinMs = lag.inbound.lagMaxMs = 30.0;
	REQUIRE(client.backend.configure(lag));
	if (clock)
	{
		server.clockServer = std::make_unique<ClockServer>(*server.transport, server.host);
		client.clockClient = std::make_unique<ClockClient>(*client.transport, client.host);
	}
	server.start(ognet::Role::Server);
	client.start(ognet::Role::Client);

	TimeConfig timeConfig;
	CountingServerHost serverHost;
	CountingClientHost clientHost;
	ServerTee serverTee(*server.transport);
	ShadowTee clientTee(*client.transport, client.clockClient.get());
	ServerSession serverSession(serverHost, serverTee, timeConfig, size_t{1u << 20});
	ClientSession clientSession(clientHost, clientTee);
	client.endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));

	uint64_t stepIndex = 0u;
	double nextFrame = kStartSeconds;
	uint32_t tick = kFirstServerTick;
	double until = kStartSeconds + kConnectBudgetSeconds;
	bool attached = false;
	while (network.nowSeconds() < until)
	{
		if (!attached && clientSession.roster().attached())
		{
			attached = true;
			until = network.nowSeconds() + kRunSeconds;
		}
		++stepIndex;
		network.setNow(kStartSeconds + static_cast<double>(stepIndex) * kStepSeconds);
		const double now = network.nowSeconds();
		client.service();
		server.service();
		client.service();
		if (client.clockClient)
			client.clockClient->servicePing();
		client.service();
		if (now < nextFrame)
			continue;
		nextFrame += kTickSeconds;
		serverHost.now = now;
		clientHost.now = client.host.nowSeconds();
		serverSession.drain();
		serverSession.publishClock(tick);
		if (server.clockServer)
			server.clockServer->publishServerTick(tick, server.host.nowSeconds());
		++tick;
		serverSession.afterInlineBatch(static_cast<int32_t>(tick));
		server.service();
		clientSession.drain();
		clientSession.flushFrame();
		client.service();
	}

	REQUIRE(attached);
	SessionRun run;
	run.timing = clientHost.timing;
	run.serverRttCalls = clientHost.serverRttCalls;
	run.rttMsCalls = serverHost.rttCalls;
	run.clientMessages = clientSession.stats().messages;
	run.badDecode = clientSession.stats().badDecode;
	run.clocksDelivered = clientSession.stats().clocksDelivered;
	run.shadow = std::move(clientTee.shadow);
	return run;
}
} // namespace simNetClockTest

using namespace simNetClockTest;

TEST_CASE("SimNetClock.WithFixed20And30MsOneWayLagsTheRttIs50MsAndThetaIsTheOffsetPlusHalfTheAsymmetry", "[NetTransport]")
{
	ClockRig rig(LinkSpec{20.0, 30.0, 0.75});
	rig.runFor(kRunSeconds);
	quote("20/30 ms, offset 750 ms", rig.samples);
	requireEverySample(rig.samples, 50.0 * kMs, 0.75 + (20.0 - 30.0) / 2.0 * kMs);
	for (const SimNetClockSample& sample : rig.samples)
	{
		CHECK(sample.t2 - sample.t1 >= 0.0);
		CHECK(sample.t2 - sample.t1 < 1.0 * kMs);
		CHECK(sample.serverTick == rig.expectedTickAt(sample.t1));
	}
	CHECK(rig.clockClient().stats().pongsReceived <= rig.clockServer().stats().pingsAnswered);
	CHECK(rig.clockServer().stats().pingsAnswered - rig.clockClient().stats().pongsReceived <= 1u);
	CHECK(rig.clockClient().stats().pongsMalformed == 0u);
	CHECK(rig.serverMessages == 0u);
}

TEST_CASE("SimNetClock.WithSymmetricLagsThetaIsTheConfiguredOffset", "[NetTransport]")
{
	for (const double offset : {0.75, -0.3125, 0.0})
	{
		ClockRig rig(LinkSpec{25.0, 25.0, offset});
		rig.runFor(kRunSeconds);
		quote(offset > 0.0 ? "25/25 ms, offset 750 ms" : offset < 0.0 ? "25/25 ms, offset -312.5 ms" : "25/25 ms, offset 0", rig.samples);
		requireEverySample(rig.samples, 50.0 * kMs, offset);
	}
}

TEST_CASE("SimNetClock.AServerThatNeverRunsItsSessionAnswersEachPingAtReceiptAndCountsItAsAClockSend", "[NetTransport]")
{
	ClockRig rig(LinkSpec{20.0, 30.0, 0.75});
	rig.runFor(kRunSeconds);
	quote("server with no session pump", rig.samples);
	requireEverySample(rig.samples, 50.0 * kMs, 0.75 + (20.0 - 30.0) / 2.0 * kMs);
	for (size_t i = 0u; i < rig.samples.size(); ++i)
		CHECK(rig.samples[i].t2 - rig.samples[i].t1 < 1.0 * kMs);
	const ogsimnet::SimNetTransportStats stats = rig.server->transport->stats();
	CHECK(stats.directSends == 0u);
	CHECK(stats.clockSends == rig.clockServer().stats().pingsAnswered);
	CHECK(rig.client->transport->stats().clockSends == rig.clockClient().stats().pingsSent);
}

TEST_CASE("SimNetClock.TheClientPingsAt20HzAndAfterAStallSendsOnePingNotABurst", "[NetTransport]")
{
	ClockRig rig(LinkSpec{20.0, 30.0, 0.75});
	const uint64_t before = rig.clockClient().stats().pingsSent;
	rig.runFor(kRunSeconds);
	const uint64_t sent = rig.clockClient().stats().pingsSent - before;
	CHECK(sent >= 39u);
	CHECK(sent <= 41u);

	rig.pinging = false;
	rig.runFor(0.3);
	const uint64_t afterStall = rig.clockClient().stats().pingsSent;
	CHECK(rig.clockClient().servicePing());
	CHECK_FALSE(rig.clockClient().servicePing());
	rig.runFor(0.025);
	CHECK_FALSE(rig.clockClient().servicePing());
	CHECK(rig.clockClient().stats().pingsSent == afterStall + 1u);
	rig.runFor(0.03);
	CHECK(rig.clockClient().servicePing());
}

TEST_CASE("SimNetClock.BeforeTheAuthorityPublishesATickThePongCarriesNoServerTick", "[NetTransport]")
{
	LinkSpec spec;
	spec.publishTicks = false;
	ClockRig rig(spec);
	rig.runFor(0.5);
	REQUIRE_FALSE(rig.samples.empty());
	for (const SimNetClockSample& sample : rig.samples)
		CHECK(sample.serverTick == ogsimnet::kNoServerTick);
	CHECK(rig.clockServer().publishedServerTick() == ogsimnet::kNoServerTick);
}

TEST_CASE("SimNetClock.EachSamplesServerTickAgeIsThePongsSendTimeMinusTheStartTimeItsTickWasPublishedWith", "[NetTransport]")
{
	ClockRig rig(LinkSpec{20.0, 30.0, 0.75});
	rig.runFor(kRunSeconds);
	REQUIRE(rig.samples.size() >= static_cast<size_t>(kRunSeconds / ogsimnet::kClockPingIntervalSeconds) - 2u);
	double minAge = rig.samples.front().serverTickAgeSeconds;
	double maxAge = minAge;
	for (const SimNetClockSample& sample : rig.samples)
	{
		REQUIRE(sample.serverTick != ogsimnet::kNoServerTick);
		const auto publication = std::find_if(rig.publications.begin(), rig.publications.end(),
			[&](const Publication& p) { return p.tick == sample.serverTick; });
		REQUIRE(publication != rig.publications.end());
		CHECK(std::abs(sample.serverTickAgeSeconds - (sample.t2 - publication->atSeconds)) <= 1e-6);
		CHECK(sample.serverTickAgeSeconds >= 0.0);
		CHECK(sample.serverTickAgeSeconds < kTickSeconds + 2.0 * kStepSeconds);
		minAge = std::min(minAge, sample.serverTickAgeSeconds);
		maxAge = std::max(maxAge, sample.serverTickAgeSeconds);
	}
	std::printf("server tick age over %zu samples: min %.6f ms, max %.6f ms\n", rig.samples.size(), minAge / kMs, maxAge / kMs);
	CHECK(maxAge > minAge);
}

TEST_CASE("SimNetClock.TheClockLanesOtherMessagesReachTheInboxUnchangedAndAForgedOrMalformedPongIsDropped", "[NetTransport]")
{
	ClockRig rig(LinkSpec{20.0, 30.0, 0.75});
	rig.pinging = false;
	rig.runFor(0.2);
	rig.clockMessages.clear();
	rig.samples.clear();
	const uint64_t pongsBefore = rig.clockClient().stats().pongsReceived;

	std::array<std::byte, 16> publish{};
	const size_t publishSize = simEnvelope::writeClockPublish(publish, 1234u);
	std::array<std::byte, ogsimnet::simNetClock::kSampleRecordBytes> forged{};
	REQUIRE(ogsimnet::simNetClock::writeSampleRecord(forged, ogsimnet::simNetClock::sampleOf(ogsimnet::SimNetClockPong{}, 1.0))
		== forged.size());
	std::array<std::byte, ogsimnet::simNetClock::kPongBytes> pong{};
	REQUIRE(ogsimnet::simNetClock::writePong(pong, ogsimnet::SimNetClockPong{}) == pong.size());

	Transport& serverTransport = *rig.server->transport;
	serverTransport.broadcastClock(std::span<const std::byte>(publish.data(), publishSize));
	serverTransport.broadcastClock(forged);
	serverTransport.broadcastClock(std::span<const std::byte>(pong).first(pong.size() - 1u));
	rig.runFor(0.1);

	REQUIRE(rig.clockMessages.size() == 1u);
	CHECK(simNetEquivalence::toBytes(rig.clockMessages.front()) == simNetEquivalence::toBytes(std::span<const std::byte>(publish.data(), publishSize)));
	CHECK(rig.samples.empty());
	const ogsimnet::SimNetClockStats stats = rig.clockClient().stats();
	CHECK(stats.forgedRecordsDropped == 1u);
	CHECK(stats.pongsMalformed == 1u);
	CHECK(stats.pongsReceived == pongsBefore);
	CHECK(stats.passedThrough >= 1u);
}

TEST_CASE("SimNetClock.TheClockNeverTouchesTheEnginePingPathWhileTheSourceIsEngine", "[NetTransport]")
{
	const SessionRun off = runSessions(false);
	const SessionRun on = runSessions(true);
	std::printf("engine path: clock off %zu timing calls, %llu serverRttSeconds calls; clock on %zu timing calls, %llu calls, "
				"%zu shadow samples\n",
		off.timing.size(), static_cast<unsigned long long>(off.serverRttCalls), on.timing.size(),
		static_cast<unsigned long long>(on.serverRttCalls), on.shadow.size());
	REQUIRE(off.timing.size() > 100u);
	CHECK(on.timing == off.timing);
	for (const auto& [tick, rtt] : on.timing)
		CHECK(rtt == kEngineRttSeconds);
	CHECK(on.serverRttCalls == off.serverRttCalls);
	CHECK(on.rttMsCalls == off.rttMsCalls);
	CHECK(on.clientMessages == off.clientMessages);
	CHECK(on.clocksDelivered == off.clocksDelivered);
	CHECK(on.badDecode == 0u);
	CHECK(off.badDecode == 0u);
	CHECK(off.shadow.empty());
	REQUIRE(on.shadow.size() >= static_cast<size_t>(kRunSeconds / ogsimnet::kClockPingIntervalSeconds));
	for (const SimNetClockSample& sample : on.shadow)
		CHECK(std::abs(sample.rttSeconds - 50.0 * kMs) <= kToleranceSeconds);
}

TEST_CASE("SimNetClock.Death.AClockHandlerSetAfterInstallHandlersFiresTheCheck", "[.][OGSimDeath]")
{
	VirtualNetwork network;
	OffsetClock clock{&network, 0.0};
	Peer peer(network, kServerHostIndex, 1u, clock, SimNetConfig{});
	peer.start(ognet::Role::Server);
	peer.clockServer = std::make_unique<ClockServer>(*peer.transport, peer.host);
	FAIL("a SimNetClockServer built after installHandlers did NOT fire SimNetTransport's OG_CHECK");
}

TEST_CASE("SimNetClock.Death.ASecondClockHandlerFiresTheCheck", "[.][OGSimDeath]")
{
	VirtualNetwork network;
	OffsetClock clock{&network, 0.0};
	Peer peer(network, kClientHostIndex, 1u, clock, SimNetConfig{});
	peer.clockClient = std::make_unique<ClockClient>(*peer.transport, peer.host);
	peer.clockServer = std::make_unique<ClockServer>(*peer.transport, peer.host);
	FAIL("a second clock handler on one transport did NOT fire SimNetTransport's OG_CHECK");
}
