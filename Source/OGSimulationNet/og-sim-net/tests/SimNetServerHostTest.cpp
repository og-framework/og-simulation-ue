// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetServerHost.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/SimulationFieldDescriptors.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace simNetServerHostTest
{
struct HostPosition
{
	std::int32_t x = 0;
};

struct HostInput
{
	std::int32_t move = 0;
};
} // namespace simNetServerHostTest

template <>
struct SerializableFields<simNetServerHostTest::HostPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetServerHostTest::HostPosition, x)); }
};

template <>
struct SerializableFields<simNetServerHostTest::HostInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetServerHostTest::HostInput, move)); }
};

namespace simNetServerHostTest
{
using HostState = SimulationComposite<HostPosition>;

class HostAllState
{
public:
	const HostState& getState() const { return m_state; }
	HostState& editState() { return m_state; }

private:
	HostState m_state;
};

struct HostSim
{
	using StateType = HostState;
	using InputType = HostInput;

	HostAllState m_allState;
	HostAllState m_vizState;

	const HostAllState& getAllState() const { return m_allState; }
	HostAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const HostAllState& getVizState() const { return m_vizState; }
};

struct HostGame
{
	using Simulatables = SimulatableList<HostSim>;
};

using ognet::test::TestHost;
using ognet::test::VirtualNetwork;
using NetEndpoint = ognet::NetEndpoint<TestHost, ognet::test::VirtualBackend>;
using Transport = ogsimnet::SimNetTransport<NetEndpoint, ogsimnet::kSimNetLinkInboxSlots>;

inline double g_now = 0.0;
constexpr double kServicesClockOffsetSeconds = 0.25;

struct Recorder
{
	std::vector<std::string> events;

	void note(std::string event) { events.push_back(std::move(event)); }

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

	std::vector<std::string> without(const std::string& prefix) const
	{
		std::vector<std::string> out;
		for (const std::string& event : events)
		{
			if (event.rfind(prefix, 0) != 0)
				out.push_back(event);
		}
		return out;
	}
};

struct Services
{
	Recorder* recorder = nullptr;
	std::vector<std::string> lines;

	double nowSeconds() { return g_now + kServicesClockOffsetSeconds; }
	void logHost(const char* line) { lines.emplace_back(line); }
};

struct Session
{
	Recorder* recorder = nullptr;
	std::map<std::uint8_t, int> unregistering;
	std::int32_t tick = 0;

	std::int32_t receptionTick()
	{
		recorder->note("query");
		return tick;
	}
	void stampServerReceived(std::uint8_t simId, std::uint32_t captureTick, double)
	{
		recorder->note("received " + std::to_string(simId) + " " + std::to_string(captureTick));
	}
	void stampHanded(std::uint8_t simId, std::uint32_t captureTick)
	{
		recorder->note("handed " + std::to_string(simId) + " " + std::to_string(captureTick));
	}
	void onLifecycleUnregistering(std::uint8_t simId)
	{
		++unregistering[simId];
		recorder->note("unregistering " + std::to_string(simId));
	}
	void noteWireSample(double roundTripSeconds) { wireSamples.push_back(roundTripSeconds); }

	std::vector<double> wireSamples;
};

struct DeliveryEndpoint
{
	Recorder* recorder = nullptr;
	std::vector<std::pair<double, double>> recvAndNow;
	const NetEndpoint* statsSource = nullptr;

	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds)
	{
		recvAndNow.emplace_back(recvSeconds, nowSeconds);
		recorder->note("delivered " + std::to_string(static_cast<unsigned>(lane)));
	}

	ognet::ConnectionStats stats(ognet::ConnectionId id) const
	{
		recorder->note("query");
		return statsSource != nullptr ? statsSource->stats(id) : ognet::ConnectionStats{};
	}
};

using Host = ogsimnet::SimNetServerHostCore<Services, Session, DeliveryEndpoint>;
using Server = ogsimnet::SimNetServer<HostGame, Host, Transport>;
using HostChannel = Server::Channel<HostSim>;

static_assert(ogsimnet::SimNetServerHost<Host> && ogsimnet::SimNetDeliveryHook<Host>
	&& ogsimnet::SimNetLifecycleUnregisteringHook<Host>);
} // namespace simNetServerHostTest

template <>
struct SimulatableOwnerTraits<simNetServerHostTest::HostSim>
{
	using PredictionOwnerType = simNetServerHostTest::HostChannel;
	using AuthorityOwnerType = simNetServerHostTest::HostChannel;
};

namespace simNetServerHostTest
{
constexpr std::uint8_t kServerHostIndex = 1u;
constexpr std::uint8_t kClientHostIndex = 10u;
constexpr std::uint16_t kServerPort = 7777u;
constexpr double kStartSeconds = 1.0;
constexpr double kPassSeconds = 0.001;
constexpr int kMaxPasses = 5000;
constexpr std::uint8_t kSim = 5u;
constexpr std::uint8_t kOtherSim = 6u;
constexpr std::uint8_t kDirectSim = 9u;
constexpr std::uint32_t kMessages = 6u;
constexpr std::uint32_t kSlotsPerMessage = 3u;
constexpr std::uint32_t kFirstCaptureTick = 100u;
constexpr std::uint8_t kSimInputLane = static_cast<std::uint8_t>(SimLane::SimInput);

ognet::EndpointConfig endpointConfig(ognet::Role role)
{
	ognet::EndpointConfig config;
	config.role = role;
	if (role == ognet::Role::Server)
		config.bind.port = kServerPort;
	config.buildIdentity.fill(0x5A);
	return config;
}

std::vector<std::byte> inputSet(std::uint8_t simId, std::uint32_t firstTick, std::uint32_t slots)
{
	InputBundleBuffer bundle;
	for (std::uint32_t i = 0u; i < slots; ++i)
		bundle.appendSlot<HostInput>(firstTick + i, HostInput{static_cast<std::int32_t>(firstTick + i)});
	std::array<std::byte, kMaxSimMessageBytes> out{};
	std::size_t used = simEnvelope::writeInputBundleSetHeader(out);
	used = simEnvelope::appendInputBundle(out, used, simId, bundle.payload());
	REQUIRE(used != 0u);
	return std::vector<std::byte>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(used));
}

struct HostRig
{
	HostRig()
	{
		services.recorder = &recorder;
		session.recorder = &recorder;
		session.tick = static_cast<std::int32_t>(kFirstCaptureTick) - 1;
		delivery.recorder = &recorder;
		network.setNow(kStartSeconds);
		g_now = network.nowSeconds();

		serverNetHost = std::make_unique<TestHost>(network.clock(), 100);
		serverEndpoint = std::make_unique<NetEndpoint>(*serverNetHost, network.addHost(kServerHostIndex));
		delivery.statsSource = serverEndpoint.get();
		const ognet::EndpointConfig serverConfig = endpointConfig(ognet::Role::Server);
		serverTransport = std::make_unique<Transport>(ogsimnet::simNetConfigFor(serverConfig));
		serverTransport->installHandlers(*serverEndpoint);
		serverEndpoint->setConnectionHandler(serverTransport->connectionHandler());
		REQUIRE(serverEndpoint->start(serverConfig));
		serverTransport->markEndpointStarted();
		host = std::make_unique<Host>(services, delivery, &session);
		config.tierMinDwellTicks = 4;
		server = std::make_unique<Server>(*host, *serverTransport, config, std::size_t{1u << 20});
		server->bindLifecycle(storage, reconciliation, resolution, netSync);

		clientNetHost = std::make_unique<TestHost>(network.clock(), 200);
		clientEndpoint = std::make_unique<NetEndpoint>(*clientNetHost, network.addHost(kClientHostIndex));
		const ognet::EndpointConfig clientConfig = endpointConfig(ognet::Role::Client);
		clientTransport = std::make_unique<Transport>(ogsimnet::simNetConfigFor(clientConfig));
		clientTransport->installHandlers(*clientEndpoint);
		clientEndpoint->setConnectionHandler(clientTransport->connectionHandler());
		REQUIRE(clientEndpoint->start(clientConfig));
		clientTransport->markEndpointStarted();
		resolution.template setNeutralInput<HostSim>(HostInput{-1});
	}

	void connect()
	{
		clientEndpoint->connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		REQUIRE(stepUntil([&] {
			return clientTransport->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == 1u;
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

	void pass()
	{
		network.advance(kPassSeconds);
		g_now = network.nowSeconds();
		clientEndpoint->serviceUntil(g_now);
		clientTransport->inbox().drain([this](const InboundSimRecord& record) {
			if (record.kind == InboundKind::Connected)
				clientTransport->setServerConnection(record.conn);
		});
		serverEndpoint->serviceUntil(g_now);
		server->drain();
		server->afterInlineBatch(session.tick);
		serverEndpoint->serviceUntil(g_now);
		clientEndpoint->serviceUntil(g_now);
	}

	void registerOwned(std::uint8_t simId, std::uint8_t localIndex)
	{
		server->registerCharacter<HostSim>(simId, localIndex, serverConn, localIndex, "host" + std::to_string(simId), HostSim{});
		server->drain();
		REQUIRE(server->lifecycleRegistered(simId));
	}

	void sendInputs(std::uint8_t simId, std::uint32_t firstTick)
	{
		const std::vector<std::byte> bytes = inputSet(simId, firstTick, kSlotsPerMessage);
		const std::size_t before = recorder.count("delivered");
		session.tick = static_cast<std::int32_t>(firstTick) - 1;
		clientEndpoint->send(clientTransport->serverConnection(), ognet::Lane::SimInput, bytes);
		REQUIRE(stepUntil([&] { return recorder.count("delivered") == before + 1u; }));
	}

	Recorder recorder;
	Services services;
	Session session;
	DeliveryEndpoint delivery;
	VirtualNetwork network;
	SimulationObjectStorage<HostSim> storage;
	SimulationReconciliation<HostSim> reconciliation{storage};
	SimulationInputResolution<HostSim> resolution{storage, reconciliation};
	SimulationNetSync<HostSim> netSync{storage, reconciliation, resolution};
	TimeConfig config;
	std::unique_ptr<Transport> serverTransport;
	std::unique_ptr<Host> host;
	std::unique_ptr<Server> server;
	std::unique_ptr<Transport> clientTransport;
	std::unique_ptr<TestHost> serverNetHost;
	std::unique_ptr<NetEndpoint> serverEndpoint;
	std::unique_ptr<TestHost> clientNetHost;
	std::unique_ptr<NetEndpoint> clientEndpoint;
	SimConnectionId serverConn = kInvalidSimConnection;
};
} // namespace simNetServerHostTest

using namespace simNetServerHostTest;

TEST_CASE("SimNetServerHost.DeliveryNotesAndStampsReachTheHostInOrder", "[NetTransport][SimNetServerHost]")
{
	HostRig rig;
	rig.connect();
	rig.registerOwned(kSim, 0u);
	rig.recorder.events.clear();

	for (std::uint32_t m = 0u; m < kMessages; ++m)
		rig.sendInputs(kSim, kFirstCaptureTick + m * kSlotsPerMessage);

	std::vector<std::string> expected;
	for (std::uint32_t m = 0u; m < kMessages; ++m)
	{
		expected.push_back("delivered " + std::to_string(kSimInputLane));
		for (std::uint32_t s = 0u; s < kSlotsPerMessage; ++s)
			expected.push_back("received " + std::to_string(kSim) + " " + std::to_string(kFirstCaptureTick + m * kSlotsPerMessage + s));
		expected.push_back("query");
		expected.push_back("query");
	}
	CHECK(rig.recorder.without("handed") == expected);

	CHECK(rig.recorder.count("delivered") == kMessages);
	CHECK(rig.recorder.count("received") == kMessages * kSlotsPerMessage);
	REQUIRE(rig.delivery.recvAndNow.size() == kMessages);
	for (const auto& [recvSeconds, nowSeconds] : rig.delivery.recvAndNow)
	{
		CHECK(recvSeconds > 0.0);
		CHECK(nowSeconds - recvSeconds > kServicesClockOffsetSeconds / 2.0);
	}
	CHECK(rig.delivery.recvAndNow.back().second == g_now + kServicesClockOffsetSeconds);

	std::size_t lastDelivered = 0u;
	std::map<std::string, std::size_t> receivedAt;
	for (std::size_t i = 0u; i < rig.recorder.events.size(); ++i)
	{
		const std::string& event = rig.recorder.events[i];
		if (event.rfind("delivered", 0) == 0)
			lastDelivered = i;
		if (event.rfind("received", 0) == 0)
			receivedAt[event.substr(9)] = i;
		if (event.rfind("handed", 0) == 0)
		{
			const auto found = receivedAt.find(event.substr(7));
			REQUIRE(found != receivedAt.end());
			CHECK(found->second < i);
			CHECK(lastDelivered < i);
		}
	}
	CHECK(rig.recorder.count("handed") > 0u);
}

TEST_CASE("SimNetServerHost.TheLifecycleUnregisteringHookRunsOncePerRemoval", "[NetTransport][SimNetServerHost]")
{
	HostRig rig;
	rig.connect();
	rig.registerOwned(kSim, 0u);
	rig.registerOwned(kOtherSim, 1u);
	REQUIRE(rig.server->addCharacter<HostSim>(kDirectSim, 2u, rig.serverConn, 2u, "direct") != nullptr);
	CHECK(rig.session.unregistering.empty());

	rig.server->unregisterCharacter<HostSim>(kSim, ogsimnet::RosterRemoveReason::Left);
	rig.server->drain();
	rig.server->drain();
	rig.server->removeCharacter(kOtherSim, ogsimnet::RosterRemoveReason::Left);
	rig.server->removeCharacter(kDirectSim, ogsimnet::RosterRemoveReason::Left);
	rig.pass();

	CHECK(rig.session.unregistering == std::map<std::uint8_t, int>{{kSim, 1}, {kOtherSim, 1}});
	CHECK(rig.recorder.count("unregistering") == 2u);
	CHECK_FALSE(rig.server->lifecycleRegistered(kSim));
	CHECK_FALSE(rig.server->lifecycleRegistered(kOtherSim));
}

TEST_CASE("SimNetServerHost.WithNoSessionTheHooksAreDroppedAndTheLinesStillRoute", "[NetTransport][SimNetServerHost]")
{
	Recorder recorder;
	Services services{&recorder, {}};
	DeliveryEndpoint delivery{&recorder, {}};
	Host host(services, delivery, nullptr);
	host.stampServerReceived(kSim, 1u, 0.5);
	host.stampHanded(kSim, 1u);
	host.onLifecycleUnregistering(kSim);
	host.noteDelivered(SimLane::SimInput, 0.5);
	host.log("[Roster.Refused] reason=Truncated");
	host.log("[SimNet.Window] role=server");
	CHECK(recorder.events == std::vector<std::string>{"delivered " + std::to_string(kSimInputLane)});
	CHECK(services.lines == std::vector<std::string>{"[Warning][Roster.Refused] reason=Truncated", "[SimNet.Window] role=server"});
	CHECK(host.session() == nullptr);
}

TEST_CASE("SimNetServerHost.TheRoundTripIsTheEndpointsSmoothedRttAndEachReadingIsAWireSample", "[NetTransport][SimNetServerHost]")
{
	HostRig rig;
	rig.connect();
	REQUIRE(rig.stepUntil([&] { return rig.serverEndpoint->stats(rig.serverConn).hasRttSample; }));
	const ognet::ConnectionStats stats = rig.serverEndpoint->stats(rig.serverConn);
	REQUIRE(stats.smoothedRttSeconds > 0.0);
	rig.session.wireSamples.clear();
	CHECK(rig.host->rttMs(rig.serverConn) == stats.smoothedRttSeconds * 1000.0);
	CHECK(rig.session.wireSamples == std::vector<double>{stats.smoothedRttSeconds});
	CHECK(rig.host->rttMs(rig.serverConn + 1000u) == ogsimnet::kSimNetServerNoRttMs);
	CHECK(rig.session.wireSamples.size() == 1u);

	Recorder recorder;
	Services services{&recorder, {}};
	DeliveryEndpoint delivery{&recorder, {}, rig.serverEndpoint.get()};
	Host withoutSession(services, delivery, nullptr);
	CHECK(withoutSession.rttMs(rig.serverConn) == stats.smoothedRttSeconds * 1000.0);
	CHECK(rig.session.wireSamples.size() == 1u);
}

TEST_CASE("SimNetServerHost.ADoorOffTheServingThreadTripsTheCheck", "[.][SimNetServerHostDeath]")
{
	Recorder recorder;
	Services services{&recorder, {}};
	Session session{&recorder, {}};
	DeliveryEndpoint delivery{&recorder, {}};
	std::unique_ptr<Host> host;
	std::thread runner([&] { host = std::make_unique<Host>(services, delivery, &session); });
	runner.join();
	host->stampHanded(kSim, 1u);
	FAIL("SimNetServerHostCore::stampHanded off the thread that built the host did NOT fire the check");
}

