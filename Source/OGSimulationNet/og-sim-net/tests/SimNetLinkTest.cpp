// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace simNetLinkTest
{
struct LinkPosition
{
	std::int32_t x = 0;
};

struct LinkInput
{
	std::int32_t move = 0;
};
} // namespace simNetLinkTest

template <>
struct SerializableFields<simNetLinkTest::LinkPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetLinkTest::LinkPosition, x)); }
};

template <>
struct SerializableFields<simNetLinkTest::LinkInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetLinkTest::LinkInput, move)); }
};

namespace simNetLinkTest
{
using LinkState = SimulationComposite<LinkPosition>;

class LinkAllState
{
public:
	const LinkState& getState() const { return m_state; }
	LinkState& editState() { return m_state; }

private:
	LinkState m_state;
};

struct LinkSim
{
	using StateType = LinkState;
	using InputType = LinkInput;

	LinkAllState m_allState;
	LinkAllState m_vizState;

	const LinkAllState& getAllState() const { return m_allState; }
	LinkAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const LinkAllState& getVizState() const { return m_vizState; }
};

struct LinkGame
{
	using Simulatables = SimulatableList<LinkSim>;
};

using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using NetEndpoint = ognet::NetEndpoint<TestHost, VirtualBackend>;

inline double g_now = 0.0;

struct LinkClock
{
	double nowSeconds() { return g_now; }
};

struct LinkEndpoint
{
	LinkEndpoint(NetEndpoint& inner, const ognet::EndpointConfig& config) : m_inner(inner), m_config(config) {}

	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { m_inner.send(id, lane, bytes); }
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		m_inner.broadcastExcept(id, lane, bytes);
	}
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { m_inner.setLaneHandler(lane, handler); }
	void setConnectionHandler(ognet::ConnectionHandler handler) { m_inner.setConnectionHandler(handler); }
	void disconnect(ognet::ConnectionId id)
	{
		disconnected.push_back(id);
		m_inner.disconnect(id);
	}
	const ognet::EndpointConfig& config() const { return m_config; }
	void noteDelivered(ognet::Lane, double, double) { ++delivered; }

	std::vector<ognet::ConnectionId> disconnected;
	std::uint64_t delivered = 0u;

private:
	NetEndpoint& m_inner;
	ognet::EndpointConfig m_config;
};

struct LinkServices
{
	std::vector<std::string> routed;
	std::vector<std::string> host;
	std::uint64_t serverRttReads = 0u;

	double nowSeconds() { return g_now; }
	void logRouted(const char* line) { routed.emplace_back(line); }
	void logHost(const char* line) { host.emplace_back(line); }
	double serverRttSeconds()
	{
		++serverRttReads;
		return 0.05;
	}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}

	std::vector<std::string> routedWith(const char* tag) const
	{
		std::vector<std::string> out;
		for (const std::string& line : routed)
		{
			if (line.find(tag) != std::string::npos)
				out.push_back(line);
		}
		return out;
	}
};

struct LinkLock
{
	void lock()
	{
		m_mutex.lock();
		++locks;
	}
	void unlock() { m_mutex.unlock(); }

	std::mutex m_mutex;
	std::uint64_t locks = 0u;
};

struct RecordingSession
{
	std::vector<std::string> events;
	std::optional<SimClockRttSample> firstSample;
	std::uint64_t stamps = 0u;

	void onTimingInfo(std::uint32_t tick, double) { events.push_back("tick " + std::to_string(tick)); }
	void onRelayFloor(std::uint8_t floorTicks) { events.push_back("floor " + std::to_string(floorTicks)); }
	void onConnectionTier(std::uint8_t oldTier, std::uint8_t newTier)
	{
		events.push_back("tier " + std::to_string(oldTier) + "->" + std::to_string(newTier));
	}
	void onClockSample(const SimClockRttSample& sample)
	{
		if (!firstSample)
			firstSample = sample;
		events.push_back("sample");
	}
	void onLifecycleUnregistering(std::uint8_t) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) { ++stamps; }

	std::size_t count(const std::string& prefix) const
	{
		std::size_t n = 0u;
		for (const std::string& event : events)
		{
			if (event.rfind(prefix, 0) == 0)
				++n;
		}
		return n;
	}
};

using Link = ogsimnet::SimNetLink<LinkGame, LinkEndpoint, LinkClock, LinkServices, LinkLock>;
using ServerTransport = ogsimnet::SimNetTransport<NetEndpoint, ogsimnet::kSimNetLinkInboxSlots>;
using ServerClock = ogsimnet::SimNetClockServer<ServerTransport, LinkClock>;

struct ServerHost
{
	std::vector<std::string> lines;

	double nowSeconds() { return g_now; }
	double rttMs(SimConnectionId) { return 50.0; }
	std::int32_t receptionTick() { return 0; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
};

using Server = ogsimnet::SimNetServer<LinkGame, ServerHost, ServerTransport>;

static_assert(ogsimnet::SimNetLinkServices<LinkServices> && ogsimnet::SimNetLinkLock<LinkLock>
	&& ogsimnet::SimNetLinkSession<RecordingSession> && ogsimnet::SimNetLinkEndpoint<LinkEndpoint>);
static_assert(!std::is_constructible_v<Server, ServerHost&, ServerTransport&, TimeConfig, std::size_t>
		&& !std::is_constructible_v<Server, ServerHost&, ServerTransport&, const TimeConfig, std::size_t>
		&& std::is_constructible_v<Server, ServerHost&, ServerTransport&, TimeConfig&, std::size_t>
		&& std::is_constructible_v<Server, ServerHost&, ServerTransport&, const TimeConfig&, std::size_t>,
	"SimNetServer holds its TimeConfig by reference, so a temporary one, const or not, is refused at compile time (its "
	"TimeConfig&& and const TimeConfig&& constructors are deleted). Was this rig's TimeConfig{} argument, a dangling "
	"reference (og-session-runtime task 32, impl_notes_SR5_31a step (b); the const one task 33b)");

constexpr std::uint8_t kServerHostIndex = 1u;
constexpr std::uint8_t kClientHostIndex = 10u;
constexpr std::uint16_t kServerPort = 7777u;
constexpr double kStartSeconds = 1.0;
constexpr double kPassSeconds = 0.001;
constexpr int kMaxPasses = 5000;
constexpr std::uint8_t kLocalSim = 5u;
constexpr std::uint8_t kRemoteSim = 6u;
constexpr std::uint8_t kLateSim = 7u;
constexpr std::uint8_t kFloorTicks = 6u;
constexpr std::uint8_t kTier = 2u;

ognet::EndpointConfig endpointConfig(ognet::Role role)
{
	ognet::EndpointConfig config;
	config.role = role;
	if (role == ognet::Role::Server)
		config.bind.port = kServerPort;
	config.buildIdentity.fill(0x5A);
	return config;
}

struct LinkRig
{
	LinkRig()
	{
		network.setNow(kStartSeconds);
		g_now = network.nowSeconds();
		serverNetHost = std::make_unique<TestHost>(network.clock(), 100);
		serverEndpoint = std::make_unique<NetEndpoint>(*serverNetHost, network.addHost(kServerHostIndex));
		const ognet::EndpointConfig serverConfig = endpointConfig(ognet::Role::Server);
		serverTransport = std::make_unique<ServerTransport>(ogsimnet::simNetConfigFor(serverConfig));
		serverClock = std::make_unique<ServerClock>(*serverTransport, clock);
		serverTransport->installHandlers(*serverEndpoint);
		serverEndpoint->setConnectionHandler(serverTransport->connectionHandler());
		REQUIRE(serverEndpoint->start(serverConfig));
		serverTransport->markEndpointStarted();
		server = std::make_unique<Server>(serverHost, *serverTransport, timeConfig, std::size_t{1u << 20});

		clientNetHost = std::make_unique<TestHost>(network.clock(), 200);
		clientNet = std::make_unique<NetEndpoint>(*clientNetHost, network.addHost(kClientHostIndex));
		const ognet::EndpointConfig clientConfig = endpointConfig(ognet::Role::Client);
		clientEndpoint = std::make_unique<LinkEndpoint>(*clientNet, clientConfig);
		link = std::make_unique<Link>(services, false);
		link->start(*clientEndpoint);
		REQUIRE(clientNet->start(clientConfig));
		link->markStarted();
	}

	void connect()
	{
		clientNet->connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		REQUIRE(stepUntil([&] {
			return link->client()->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == 1u
				&& link->client()->roster().attached();
		}));
		serverConn = server->attachedConnections().front();
	}

	template <class DoneT>
	bool stepUntil(DoneT done)
	{
		for (int i = 0; i < kMaxPasses; ++i)
		{
			if (done())
				return true;
			pass();
		}
		return done();
	}

	void serviceServer()
	{
		network.advance(kPassSeconds);
		g_now = network.nowSeconds();
		serverEndpoint->serviceUntil(g_now);
		server->drain();
		if (publishTicks)
		{
			++serverTick;
			server->publishClock(serverTick);
			serverClock->publishServerTick(serverTick, g_now);
		}
		server->afterInlineBatch(static_cast<std::int32_t>(serverTick));
		serverEndpoint->serviceUntil(g_now);
		clientNet->serviceUntil(g_now);
	}

	void pass()
	{
		serviceServer();
		link->serviceFrame([this] { ++afterPings; });
		link->flushFrame();
		clientNet->serviceUntil(g_now);
	}

	void stepPass()
	{
		serviceServer();
		link->serviceFrame([this] { ++afterPings; });
		std::thread step([this] { link->drainForStep(); });
		step.join();
		link->flushFrame();
		clientNet->serviceUntil(g_now);
	}

	template <class DoneT>
	bool stepUntilAttached(DoneT done)
	{
		for (int i = 0; i < kMaxPasses; ++i)
		{
			if (done())
				return true;
			stepPass();
		}
		return done();
	}

	bool noPongInFlight() const
	{
		const ogsimnet::SimNetClockStats stats = link->clockClient()->stats();
		return stats.pingsSent > 0u && stats.pingsSent == stats.pongsReceived;
	}

	void joinBeforeAnySession()
	{
		server->publishFloor(kFloorTicks);
		connect();
		REQUIRE(server->addCharacter<LinkSim>(kLocalSim, 0u, serverConn, 0u, "local") != nullptr);
		REQUIRE(server->addCharacter<LinkSim>(kRemoteSim, 1u, kInvalidSimConnection, 0u, "remote") != nullptr);
		server->publishTier(serverConn, kTier);
		publishTicks = true;
		REQUIRE(stepUntil([&] {
			return link->client()->roster().entryFor(kRemoteSim) != nullptr && link->client()->latchedTier().has_value()
				&& link->client()->heldClockSample().has_value() && link->client()->stats().clocksDelivered > 0u;
		}));
		publishTicks = false;
		REQUIRE(stepUntil([&] { return noPongInFlight() && link->client()->heldClockSample().has_value(); }));
	}

	VirtualNetwork network;
	LinkClock clock;
	TimeConfig timeConfig;
	ServerHost serverHost;
	LinkServices services;
	std::unique_ptr<ServerTransport> serverTransport;
	std::unique_ptr<ServerClock> serverClock;
	std::unique_ptr<Server> server;
	std::unique_ptr<Link> link;
	std::unique_ptr<TestHost> serverNetHost;
	std::unique_ptr<NetEndpoint> serverEndpoint;
	std::unique_ptr<TestHost> clientNetHost;
	std::unique_ptr<NetEndpoint> clientNet;
	std::unique_ptr<LinkEndpoint> clientEndpoint;
	SimConnectionId serverConn = kInvalidSimConnection;
	std::uint32_t serverTick = 100u;
	bool publishTicks = false;
	std::uint64_t afterPings = 0u;
};

std::vector<std::byte> rosterAddAt(std::uint16_t revision, std::uint8_t simId, std::uint8_t slot)
{
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeAdd(revision, ogsimnet::RosterEntry{simId, slot, ogsimnet::kRosterNotLocal});
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = simEnvelope::writeRoster(out, message.view());
	return std::vector<std::byte>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(size));
}
} // namespace simNetLinkTest

using namespace simNetLinkTest;

TEST_CASE("SimNetLink.AClientConnectedBeforeAnySessionHoldsTheRosterTheFloorTheTierAndAClockSample", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.joinBeforeAnySession();

	const ogsimnet::SimNetClientStats& stats = rig.link->client()->stats();
	CHECK(rig.link->client()->roster().entryFor(kLocalSim) != nullptr);
	CHECK(rig.link->client()->roster().entryFor(kRemoteSim) != nullptr);
	CHECK(rig.link->client()->latchedFloor() == std::optional<std::uint8_t>(kFloorTicks));
	REQUIRE(rig.link->client()->latchedTier().has_value());
	CHECK(rig.link->client()->latchedTier()->newTier == kTier);
	CHECK(rig.link->client()->heldClockSample().has_value());
	CHECK_FALSE(rig.link->clientSessionBound());
	CHECK(stats.rosterApplied >= 3u);
	CHECK(stats.floorsReceived == 1u);
	CHECK(stats.tiersReceived == 1u);
	CHECK(stats.clockSamples > 0u);
	CHECK(stats.clocksDelivered > 0u);
	CHECK(rig.services.serverRttReads == 0u);
	CHECK(rig.afterPings > 0u);
	CHECK(rig.services.routedWith("[ClockSource.Held]").empty());
	CHECK(rig.link->bindings().verdict(kLocalSim, true).step == ogsimnet::BindStep::Wait);
}

TEST_CASE("SimNetLink.AttachReplaysTheFloorAndTierAndFeedsTheHeldSampleBeforeTheFirstTick", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.joinBeforeAnySession();
	const SimClockRttSample held = *rig.link->client()->heldClockSample();

	RecordingSession session;
	rig.link->attachSession(session);
	CHECK(session.events.empty());
	rig.publishTicks = true;
	REQUIRE(rig.stepUntilAttached([&] { return rig.link->client()->listenerBound() && session.count("tick ") > 0u; }));
	rig.publishTicks = false;

	REQUIRE(session.events.size() >= 4u);
	CHECK(std::vector<std::string>(session.events.begin(), session.events.begin() + 3)
		== std::vector<std::string>{"floor 6", "tier 0->2", "sample"});
	CHECK(session.events[3].rfind("tick ", 0) == 0);
	REQUIRE(session.firstSample.has_value());
	CHECK(session.firstSample->t3Seconds == held.t3Seconds);
	CHECK(session.firstSample->rttSeconds == held.rttSeconds);
	CHECK_FALSE(rig.link->client()->heldClockSample().has_value());
	CHECK(rig.services.routedWith("[Warning][ClockSource.Held]").size() == 1u);
	CHECK(rig.services.serverRttReads > 0u);
}

TEST_CASE("SimNetLink.ACharacterAlreadyInTheRosterBindsAtOnce", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.joinBeforeAnySession();
	REQUIRE(rig.link->bindings().verdict(kLocalSim, true).step == ogsimnet::BindStep::Wait);

	RecordingSession session;
	rig.link->attachSession(session);

	const ogsimnet::BindVerdict local = rig.link->bindings().verdict(kLocalSim, true);
	const ogsimnet::BindVerdict remote = rig.link->bindings().verdict(kRemoteSim, false);
	CHECK(local.step == ogsimnet::BindStep::Ready);
	CHECK(local.isLocal);
	CHECK(remote.step == ogsimnet::BindStep::Ready);
	CHECK_FALSE(remote.isLocal);
	CHECK(rig.link->bindings().verdict(kLocalSim, false).step == ogsimnet::BindStep::RoleMismatch);
	CHECK(rig.link->editBindings().markRegisteredIfReady(kLocalSim, true).step == ogsimnet::BindStep::Ready);
	CHECK(rig.link->bindings().isRegistered(kLocalSim));
}

TEST_CASE("SimNetLink.ACharacterAddedAfterTheAttachBindsAfterTheStepDrain", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.joinBeforeAnySession();
	RecordingSession session;
	rig.link->attachSession(session);
	REQUIRE(rig.link->bindings().verdict(kLateSim, false).step == ogsimnet::BindStep::Wait);

	REQUIRE(rig.server->addCharacter<LinkSim>(kLateSim, 2u, kInvalidSimConnection, 0u, "late") != nullptr);
	REQUIRE(rig.stepUntilAttached([&] { return rig.link->bindings().verdict(kLateSim, false).step == ogsimnet::BindStep::Ready; }));
	rig.server->removeCharacter(kLateSim, ogsimnet::RosterRemoveReason::Left);
	REQUIRE(rig.stepUntilAttached([&] { return rig.link->bindings().verdict(kLateSim, false).step == ogsimnet::BindStep::Wait; }));
	CHECK(rig.link->bindings().verdict(kLocalSim, true).step == ogsimnet::BindStep::Ready);
	CHECK(rig.clientEndpoint->delivered > 0u);
}

TEST_CASE("SimNetLink.DetachThenAttachAgainReplaysOnce", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.joinBeforeAnySession();
	RecordingSession first;
	rig.link->attachSession(first);
	REQUIRE(rig.stepUntilAttached([&] { return rig.link->client()->listenerBound(); }));
	CHECK(first.count("floor ") == 1u);
	CHECK(first.count("tier ") == 1u);

	rig.link->detachSession();
	CHECK_FALSE(rig.link->clientSessionBound());
	REQUIRE(rig.stepUntil([&] { return !rig.link->client()->listenerBound(); }));
	for (int i = 0; i < 20; ++i)
		rig.pass();
	const std::size_t firstEventsAfterDetach = first.events.size();

	RecordingSession second;
	rig.link->attachSession(second);
	REQUIRE(rig.stepUntilAttached([&] { return rig.link->client()->listenerBound(); }));
	for (int i = 0; i < 20; ++i)
		rig.stepPass();
	CHECK(second.count("floor ") == 1u);
	CHECK(second.count("tier ") == 1u);
	CHECK(std::vector<std::string>(second.events.begin(), second.events.begin() + 2)
		== std::vector<std::string>{"floor 6", "tier 0->2"});
	CHECK(first.events.size() == firstEventsAfterDetach);
	CHECK(first.count("floor ") == 1u);
	CHECK(rig.link->bindings().verdict(kLocalSim, true).step == ogsimnet::BindStep::Ready);
}

TEST_CASE("SimNetLink.TheDisconnectRequestDisconnects", "[NetTransport][SimNetLink]")
{
	LinkRig rig;
	rig.connect();
	REQUIRE(rig.server->addCharacter<LinkSim>(kRemoteSim, 1u, kInvalidSimConnection, 0u, "remote") != nullptr);
	REQUIRE(rig.stepUntil([&] { return rig.link->client()->roster().entryFor(kRemoteSim) != nullptr; }));

	const SimConnectionId serverOnClient = rig.link->client()->serverConnection();
	const std::uint16_t revision = rig.link->client()->roster().revision();
	const std::vector<std::byte> gap = rosterAddAt(static_cast<std::uint16_t>(revision + 2u), kLateSim, 2u);
	REQUIRE(rig.link->transport().inbox().pushMessage(serverOnClient, SimLane::Control, 0.0, gap));
	rig.link->serviceFrame([] {});

	CHECK(rig.clientEndpoint->disconnected == std::vector<ognet::ConnectionId>{serverOnClient});
	const std::vector<std::string> lines = rig.services.routedWith("role=client disconnect");
	REQUIRE(lines.size() == 1u);
	CHECK(lines[0] == "[Error][SimNet.Host] role=client disconnect conn=" + std::to_string(serverOnClient)
		+ " reason=RevisionGap: the server's roster was refused, so this client leaves the server");
	CHECK(rig.services.host.size() > 0u);
	REQUIRE(rig.stepUntil([&] { return rig.server->attachedConnections().empty(); }));
	CHECK(rig.link->client()->serverConnection() == kInvalidSimConnection);
}

TEST_CASE("SimNetLink.DrainForStepOnTheFrameThreadTripsTheCheck", "[.][SimNetLinkDeath]")
{
	LinkRig rig;
	rig.connect();
	RecordingSession session;
	rig.link->attachSession(session);
	rig.link->drainForStep();
	FAIL("SimNetLink::drainForStep on the frame thread did NOT fire its thread check - the check is vacuous in this target");
}

