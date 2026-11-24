// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetConnections.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

namespace
{
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using RealEndpoint = ognet::NetEndpoint<TestHost, VirtualBackend>;
using RealTransport = ogsimnet::SimNetTransport<RealEndpoint, 2048>;

constexpr uint8_t kServerHostIndex = 1;
constexpr uint8_t kFirstClientHostIndex = 10;
constexpr uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr double kStepSeconds = 0.001;
constexpr int kMaxSteps = 5000;
constexpr uint32_t kTestInputBytes = 8u;
using TestShape = simEnvelope::Shape<kTestInputBytes, 16u>;

std::vector<uint8_t> toU8(std::span<const std::byte> bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(static_cast<uint8_t>(b));
	}
	return out;
}

std::vector<std::byte> patternBytes(size_t size, uint8_t seed)
{
	std::vector<std::byte> out(size);
	for (size_t i = 0; i < size; ++i)
	{
		out[i] = static_cast<std::byte>(static_cast<uint8_t>(seed + i * 7u));
	}
	return out;
}

std::vector<std::byte> testBundle(uint8_t slots, uint8_t seed)
{
	std::vector<std::byte> out(inputRedundancyBundle::kHeaderBytes + slots * TestShape::kBundleSlotBytes);
	out[inputRedundancyBundle::kVersionOffset] = static_cast<std::byte>(inputRedundancyBundle::kWireFormatVersion);
	out[inputRedundancyBundle::kSlotCountOffset] = static_cast<std::byte>(slots);
	for (size_t i = inputRedundancyBundle::kHeaderBytes; i < out.size(); ++i)
	{
		out[i] = static_cast<std::byte>(static_cast<uint8_t>(seed + i));
	}
	return out;
}

struct Got
{
	InboundKind kind = InboundKind::Message;
	SimLane lane = SimLane::Control;
	SimConnectionId conn = kInvalidSimConnection;
	double recvSeconds = 0.0;
	std::vector<uint8_t> bytes;
};

template <typename TransportT>
std::vector<Got> drainAll(TransportT& transport)
{
	std::vector<Got> out;
	transport.inbox().drain([&](const InboundSimRecord& record) {
		out.push_back(Got{record.kind, record.lane, record.conn, record.recvSeconds, toU8(record.payload())});
	});
	return out;
}

std::vector<Got> messagesOn(const std::vector<Got>& records, SimLane lane)
{
	std::vector<Got> out;
	for (const Got& record : records)
	{
		if (record.kind == InboundKind::Message && record.lane == lane)
		{
			out.push_back(record);
		}
	}
	return out;
}

struct RecordingEndpoint
{
	struct Call
	{
		bool broadcast = false;
		ognet::ConnectionId id = ognet::kInvalidConnection;
		ognet::Lane lane = ognet::Lane::Control;
		std::vector<uint8_t> bytes;

		friend bool operator==(const Call&, const Call&) = default;
	};

	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { calls.push_back(Call{false, id, lane, toU8(bytes)}); }
	void broadcastExcept(ognet::ConnectionId skip, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		calls.push_back(Call{true, skip, lane, toU8(bytes)});
	}
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { laneHandlers[static_cast<size_t>(lane)] = handler; }
	void setConnectionHandler(ognet::ConnectionHandler) { ++connectionHandlerSets; }
	void disconnect(ognet::ConnectionId) {}

	std::vector<Call> calls;
	std::array<ognet::LaneHandler, ognet::kLaneCount> laneHandlers{};
	int connectionHandlerSets = 0;
};

using FakeTransport = ogsimnet::SimNetTransport<RecordingEndpoint, 128>;

struct Peer
{
	Peer(VirtualNetwork& network, VirtualBackend& backendIn, uint64_t seed, ogsimnet::SimNetConfig config)
		: transport(std::make_unique<RealTransport>(config)), host(network.clock(), seed), backend(backendIn), endpoint(host, backend)
	{
	}

	void start(ognet::Role role)
	{
		transport->installHandlers(endpoint);
		endpoint.setConnectionHandler(transport->connectionHandler());
		ognet::EndpointConfig config;
		config.role = role;
		if (role == ognet::Role::Server)
		{
			config.bind.port = kServerPort;
		}
		config.buildIdentity.fill(0x5A);
		REQUIRE(endpoint.start(config));
		transport->markEndpointStarted();
	}

	std::unique_ptr<RealTransport> transport;
	TestHost host;
	VirtualBackend& backend;
	RealEndpoint endpoint;
	std::vector<Got> records;

	void collect()
	{
		const std::vector<Got> drained = drainAll(*transport);
		records.insert(records.end(), drained.begin(), drained.end());
	}

	SimConnectionId firstConnected() const
	{
		for (const Got& record : records)
		{
			if (record.kind == InboundKind::Connected)
			{
				return record.conn;
			}
		}
		return kInvalidSimConnection;
	}

	std::vector<SimConnectionId> connected() const
	{
		std::vector<SimConnectionId> out;
		for (const Got& record : records)
		{
			if (record.kind == InboundKind::Connected)
			{
				out.push_back(record.conn);
			}
		}
		return out;
	}
};

struct Session
{
	explicit Session(size_t clients, ogsimnet::SimNetConfig serverConfig = {}, ogsimnet::SimNetConfig clientConfig = {})
	{
		network.setNow(kStartSeconds);
		server = std::make_unique<Peer>(network, network.addHost(kServerHostIndex), 100, serverConfig);
		server->start(ognet::Role::Server);
		for (size_t i = 0; i < clients; ++i)
		{
			auto client = std::make_unique<Peer>(network, network.addHost(static_cast<uint8_t>(kFirstClientHostIndex + i)), 200 + i, clientConfig);
			client->start(ognet::Role::Client);
			client->endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
			clientsList.push_back(std::move(client));
			Peer& joining = *clientsList.back();
			REQUIRE(stepUntil([&] {
				joining.collect();
				return joining.firstConnected() != kInvalidSimConnection;
			}));
			joining.transport->setServerConnection(joining.firstConnected());
		}
	}

	template <typename DoneT>
	bool stepUntil(DoneT done)
	{
		for (int i = 0; i < kMaxSteps; ++i)
		{
			if (done())
			{
				return true;
			}
			servicePass();
		}
		return done();
	}

	void servicePass()
	{
		++step;
		network.setNow(kStartSeconds + static_cast<double>(step) * kStepSeconds);
		server->endpoint.serviceUntil(network.nowSeconds());
		for (auto& client : clientsList)
		{
			client->endpoint.serviceUntil(network.nowSeconds());
		}
	}

	bool connectServerSide()
	{
		for (size_t i = 0; i < clientsList.size(); ++i)
		{
			const std::array<std::byte, 1> hello{std::byte{static_cast<uint8_t>(0xC0 + i)}};
			clientsList[i]->endpoint.send(clientsList[i]->firstConnected(), ognet::Lane::Control, hello);
		}
		const bool done = stepUntil([&] {
			server->collect();
			return server->connected().size() == clientsList.size() && messagesOn(server->records, SimLane::Control).size() == clientsList.size();
		});
		serverIds.assign(clientsList.size(), kInvalidSimConnection);
		for (const Got& record : messagesOn(server->records, SimLane::Control))
		{
			for (size_t i = 0; i < clientsList.size(); ++i)
			{
				if (record.bytes.size() == 1u && record.bytes[0] == 0xC0 + i)
				{
					serverIds[i] = record.conn;
				}
			}
		}
		return done;
	}

	SimConnectionId serverIdOf(size_t client) const { return serverIds.at(client); }

	void clearRecords()
	{
		server->collect();
		server->records.clear();
		for (auto& client : clientsList)
		{
			client->collect();
			client->records.clear();
		}
	}

	VirtualNetwork network;
	std::unique_ptr<Peer> server;
	std::vector<std::unique_ptr<Peer>> clientsList;
	std::vector<SimConnectionId> serverIds;
	int step = 0;
};
} // namespace

TEST_CASE("SimNetTransport.TheTransportAndItsAdaptersSatisfyThePortConcepts", "[NetTransport][SimNetTransport]")
{
	STATIC_CHECK(ogsimnet::SimNetEndpoint<RealEndpoint>);
	STATIC_CHECK(ogsimnet::SimNetEndpoint<RecordingEndpoint>);
	STATIC_CHECK(ServerSimTransport<RealTransport>);
	STATIC_CHECK(ClientSimTransport<RealTransport::ChannelTransport>);
	STATIC_CHECK(!ClientSimTransport<RealTransport>);
	STATIC_CHECK(ConnectionAddress<ogsimnet::SimNetConnectionHandle>);

	RecordingEndpoint endpoint;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	for (size_t lane = 0; lane < ognet::kLaneCount; ++lane)
	{
		CHECK((endpoint.laneHandlers[lane].fn != nullptr) == (lane < kSimLaneCount));
	}
	CHECK(endpoint.connectionHandlerSets == 0);
}

TEST_CASE("SimNetTransport.EverySimLaneDeliversInEachDirectionOgNetAllowsIntoTheInboxWithItsReceiveTimeAndConnection", "[NetTransport][SimNetTransport]")
{
	Session session(1);
	REQUIRE(session.connectServerSide());
	Peer& server = *session.server;
	Peer& client = *session.clientsList[0];
	const SimConnectionId clientOnServer = session.serverIdOf(0);
	const SimConnectionId serverOnClient = client.firstConnected();
	REQUIRE(clientOnServer != kInvalidSimConnection);
	session.clearRecords();

	const std::vector<std::byte> control = patternBytes(5, 0x10);
	const std::vector<std::byte> clock = patternBytes(6, 0x20);
	const std::vector<std::byte> input = testBundle(1, 0x60);
	const std::vector<std::byte> relay = patternBytes(8, 0x40);
	const std::vector<std::byte> state = patternBytes(9, 0x50);

	server.transport->sendControl(clientOnServer, control);
	server.transport->broadcastClock(clock);
	server.transport->relay(kInvalidSimConnection, relay);
	server.transport->sendState(clientOnServer, state);

	client.endpoint.send(serverOnClient, ognet::Lane::Control, control);
	client.endpoint.send(serverOnClient, ognet::Lane::SimClock, clock);
	client.transport->beginInputSends();
	client.transport->channelTransport(3).sendInputBundle(input);
	client.transport->endInputSends();

	REQUIRE(session.stepUntil([&] {
		const double passSeconds = session.network.nowSeconds();
		const size_t serverBefore = server.records.size();
		const size_t clientBefore = client.records.size();
		server.collect();
		client.collect();
		for (size_t i = serverBefore; i < server.records.size(); ++i)
		{
			CHECK(server.records[i].recvSeconds == passSeconds);
		}
		for (size_t i = clientBefore; i < client.records.size(); ++i)
		{
			CHECK(client.records[i].recvSeconds == passSeconds);
		}
		return server.records.size() == 3u && client.records.size() == 4u;
	}));

	const std::array<std::vector<std::byte>, kSimLaneCount> sent{control, clock, input, relay, state};
	const std::array<bool, kSimLaneCount> up{true, true, true, false, false};
	const std::array<bool, kSimLaneCount> down{true, true, false, true, true};
	for (size_t lane = 0; lane < kSimLaneCount; ++lane)
	{
		const std::vector<Got> onClient = messagesOn(client.records, static_cast<SimLane>(lane));
		REQUIRE(onClient.size() == (down[lane] ? 1u : 0u));
		const std::vector<Got> onServer = messagesOn(server.records, static_cast<SimLane>(lane));
		REQUIRE(onServer.size() == (up[lane] ? 1u : 0u));
		if (down[lane])
		{
			CHECK(onClient[0].conn == serverOnClient);
			CHECK(onClient[0].bytes == toU8(sent[lane]));
		}
		if (up[lane] && static_cast<SimLane>(lane) != SimLane::SimInput)
		{
			CHECK(onServer[0].conn == clientOnServer);
			CHECK(onServer[0].bytes == toU8(sent[lane]));
		}
	}
	const std::vector<Got> inputs = messagesOn(server.records, SimLane::SimInput);
	CHECK(inputs[0].conn == clientOnServer);
	std::vector<uint8_t> inputSet{kSimEnvelopeVersion, static_cast<uint8_t>(SimMessageKind::InputBundleSet), 1u, 3u};
	for (const uint8_t b : toU8(input))
	{
		inputSet.push_back(b);
	}
	CHECK(inputs[0].bytes == inputSet);

	const std::vector<std::byte> oversize = patternBytes(kMaxSimMessageBytes + 1u, 0x70);
	server.transport->sendControl(clientOnServer, oversize);
	REQUIRE(session.stepUntil([&] { return client.transport->inbox().stats().tooLarge == 1u; }));
	client.collect();
	CHECK(messagesOn(client.records, SimLane::Control).size() == 1u);
}

TEST_CASE("SimNetTransport.AConnectedEventPrecedesItsConnectionsFirstMessageInDrainOrder", "[NetTransport][SimNetTransport]")
{
	Session session(2);
	REQUIRE(session.connectServerSide());
	const std::vector<Got>& records = session.server->records;
	for (size_t client = 0; client < 2; ++client)
	{
		const SimConnectionId conn = session.serverIdOf(client);
		REQUIRE(conn != kInvalidSimConnection);
		size_t connectedAt = records.size();
		size_t firstMessageAt = records.size();
		for (size_t i = 0; i < records.size(); ++i)
		{
			if (records[i].conn != conn)
			{
				continue;
			}
			if (records[i].kind == InboundKind::Connected && connectedAt == records.size())
			{
				connectedAt = i;
			}
			if (records[i].kind == InboundKind::Message && firstMessageAt == records.size())
			{
				firstMessageAt = i;
			}
		}
		REQUIRE(connectedAt < records.size());
		REQUIRE(firstMessageAt < records.size());
		CHECK(connectedAt < firstMessageAt);
		CHECK(records[connectedAt].recvSeconds == ogsimnet::kNoReceiveSeconds);
	}
}

TEST_CASE("SimNetTransport.RelaySkipsExactlyTheOwner", "[NetTransport][SimNetTransport]")
{
	Session session(3);
	REQUIRE(session.connectServerSide());
	session.clearRecords();
	const SimConnectionId owner = session.serverIdOf(1);
	REQUIRE(owner != kInvalidSimConnection);

	const std::vector<std::byte> ring = patternBytes(40, 0x11);
	session.server->transport->relay(owner, ring);
	for (int i = 0; i < 4; ++i)
	{
		session.servicePass();
	}
	for (size_t client = 0; client < 3; ++client)
	{
		session.clientsList[client]->collect();
		const std::vector<Got> relays = messagesOn(session.clientsList[client]->records, SimLane::SimRelay);
		if (client == 1)
		{
			CHECK(relays.empty());
		}
		else
		{
			REQUIRE(relays.size() == 1u);
			CHECK(relays[0].bytes == toU8(ring));
		}
	}
}

TEST_CASE("SimNetTransport.EverySendReachesTheEndpointAtOnceInCallOrder", "[NetTransport][SimNetTransport]")
{
	SECTION("the calls reach the endpoint as they are made, in the order they were made")
	{
		RecordingEndpoint endpoint;
		auto transport = std::make_unique<FakeTransport>();
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		transport->setServerConnection(9u);

		std::vector<RecordingEndpoint::Call> expected;
		for (uint8_t round = 0; round < 3; ++round)
		{
			const std::vector<std::byte> a = patternBytes(10u + round, round);
			const std::vector<std::byte> b = patternBytes(20u + round, static_cast<uint8_t>(round + 50u));
			transport->sendState(4u, a);
			expected.push_back({false, 4u, ognet::Lane::SimState, toU8(a)});
			transport->relay(5u, b);
			expected.push_back({true, 5u, ognet::Lane::SimRelay, toU8(b)});
			transport->sendControl(6u, b);
			expected.push_back({false, 6u, ognet::Lane::Control, toU8(b)});
			transport->broadcastClock(a);
			expected.push_back({true, ognet::kInvalidConnection, ognet::Lane::SimClock, toU8(a)});
		}
		transport->beginInputSends();
		transport->channelTransport(2).sendInputBundle(testBundle(1, 0x22));
		transport->endInputSends();
		CHECK(transport->stats().directSends == expected.size() + 1u);

		REQUIRE(endpoint.calls.size() == expected.size() + 1u);
		for (size_t i = 0; i < expected.size(); ++i)
		{
			CHECK(endpoint.calls[i] == expected[i]);
		}
		CHECK(endpoint.calls.back().id == 9u);
		CHECK(endpoint.calls.back().lane == ognet::Lane::SimInput);
	}

	SECTION("over og-net, one lane's messages arrive in call order")
	{
		Session session(1);
		REQUIRE(session.connectServerSide());
		session.clearRecords();
		Peer& client = *session.clientsList[0];
		const SimConnectionId conn = session.serverIdOf(0);
		std::vector<std::vector<uint8_t>> sent;
		for (uint8_t i = 0; i < 12; ++i)
		{
			const std::vector<std::byte> state = patternBytes(30u + i, i);
			session.server->transport->sendState(conn, state);
			sent.push_back(toU8(state));
		}
		REQUIRE(session.stepUntil([&] {
			client.collect();
			return messagesOn(client.records, SimLane::SimState).size() == sent.size();
		}));
		const std::vector<Got> received = messagesOn(client.records, SimLane::SimState);
		for (size_t i = 0; i < sent.size(); ++i)
		{
			CHECK(received[i].bytes == sent[i]);
		}
	}
}

TEST_CASE("SimNetTransport.ImmediateSendsReachThePeerInTheSameServicePass", "[NetTransport][SimNetTransport]")
{
	SECTION("the endpoint sees the call at once")
	{
		RecordingEndpoint endpoint;
		auto transport = std::make_unique<FakeTransport>();
		transport->installHandlers(endpoint);
		const std::vector<std::byte> state = patternBytes(12, 3);
		transport->sendState(4u, state);
		REQUIRE(endpoint.calls.size() == 1u);
		CHECK(endpoint.calls[0] == RecordingEndpoint::Call{false, 4u, ognet::Lane::SimState, toU8(state)});
		CHECK(transport->stats().directSends == 1u);
	}

	SECTION("over og-net, one service pass of each peer delivers it")
	{
		Session session(1);
		REQUIRE(session.connectServerSide());
		session.clearRecords();
		Peer& client = *session.clientsList[0];
		const std::vector<std::byte> state = patternBytes(12, 3);
		session.server->transport->sendState(session.serverIdOf(0), state);
		session.servicePass();
		client.collect();
		const std::vector<Got> received = messagesOn(client.records, SimLane::SimState);
		REQUIRE(received.size() == 1u);
		CHECK(received[0].bytes == toU8(state));
	}
}

TEST_CASE("SimNetTransport.AHandleMadeBeforeADisconnectAndReconnectOfTheSameIdReadsDeadAfterIt", "[NetTransport][SimNetTransport]")
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	ogsimnet::LivenessTable table;
	const auto apply = [&] {
		transport->inbox().drain([&](const InboundSimRecord& record) { table.apply(record); });
	};

	CHECK_FALSE(ogsimnet::SimNetConnectionHandle{}.isAlive());
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 7u});
	apply();
	const ogsimnet::SimNetConnectionHandle before = table.handleFor(7u);
	CHECK(before.isAlive());
	CHECK(table.upCount() == 1u);

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, 7u});
	apply();
	CHECK_FALSE(before.isAlive());
	CHECK_FALSE(table.handleFor(7u).isAlive());

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Connected, 7u});
	apply();
	const ogsimnet::SimNetConnectionHandle after = table.handleFor(7u);
	CHECK(after.isAlive());
	CHECK_FALSE(before.isAlive());
	CHECK(before != after);
	CHECK(std::hash<ogsimnet::SimNetConnectionHandle>{}(before) == std::hash<ogsimnet::SimNetConnectionHandle>{}(after));

	std::unordered_set<ogsimnet::SimNetConnectionHandle> set{before, after, after};
	CHECK(set.size() == 2u);

	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::AddressChanged, 7u});
	transport->onConnectionEvent(ognet::ConnectionEvent{ognet::ConnectionEventKind::Disconnected, ognet::kInvalidConnection});
	const std::vector<Got> records = drainAll(*transport);
	REQUIRE(records.size() == 1u);
	CHECK(records[0].kind == InboundKind::Disconnected);
	CHECK(records[0].conn == kInvalidSimConnection);
	CHECK(transport->stats().addressChangesIgnored == 1u);
	CHECK(after.isAlive());
}

TEST_CASE("SimNetTransport.OneBundleSetCarriesFourLocalPlayersBundlesAsOneMessage", "[NetTransport][SimNetTransport]")
{
	Session session(1);
	REQUIRE(session.connectServerSide());
	session.clearRecords();
	Peer& client = *session.clientsList[0];

	std::vector<std::vector<std::byte>> bundles;
	client.transport->beginInputSends();
	for (uint8_t simId = 1; simId <= 4; ++simId)
	{
		bundles.push_back(testBundle(static_cast<uint8_t>(simId % 3u + 1u), static_cast<uint8_t>(simId * 16u)));
		client.transport->channelTransport(simId).sendInputBundle(bundles.back());
	}
	client.transport->channelTransport(1).endInputSends();
	REQUIRE(session.stepUntil([&] {
		session.server->collect();
		return !messagesOn(session.server->records, SimLane::SimInput).empty();
	}));
	for (int i = 0; i < 4; ++i)
	{
		session.servicePass();
	}
	session.server->collect();

	const std::vector<Got> inputs = messagesOn(session.server->records, SimLane::SimInput);
	REQUIRE(inputs.size() == 1u);
	std::vector<std::byte> wire;
	for (const uint8_t b : inputs[0].bytes)
	{
		wire.push_back(static_cast<std::byte>(b));
	}
	const DecodedSimMessage decoded = simEnvelope::decode<TestShape>(SimLane::SimInput, SimDirection::ClientToServer, wire);
	REQUIRE(decoded.refusal == DecodeRefusal::None);
	CHECK(decoded.count == 4u);
	std::vector<uint8_t> simIds;
	std::vector<std::vector<uint8_t>> carried;
	CHECK(simEnvelope::forEachInputBundle<TestShape>(decoded.body, [&](uint8_t simId, std::span<const std::byte> bundle) {
		simIds.push_back(simId);
		carried.push_back(toU8(bundle));
	}) == DecodeRefusal::None);
	CHECK(simIds == std::vector<uint8_t>{1, 2, 3, 4});
	for (size_t i = 0; i < bundles.size(); ++i)
	{
		CHECK(carried[i] == toU8(bundles[i]));
	}
	CHECK(client.transport->stats().inputSets == 1u);
	CHECK(client.transport->stats().inputBundles == 4u);
}

TEST_CASE("SimNetTransport.TwoChannelsBundlesInOneSendArriveAsOneSetWithBothSimIds", "[NetTransport][SimNetTransport]")
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);
	transport->setServerConnection(9u);
	FakeTransport::ChannelTransport first = transport->channelTransport(5);
	FakeTransport::ChannelTransport second = transport->channelTransport(6);
	CHECK(first.simId() == 5u);
	CHECK(second.simId() == 6u);

	const std::vector<std::byte> firstBundle = testBundle(2, 0x31);
	const std::vector<std::byte> secondBundle = testBundle(1, 0x41);
	transport->beginInputSends();
	first.sendInputBundle(firstBundle);
	second.sendInputBundle(secondBundle);
	transport->endInputSends();

	REQUIRE(endpoint.calls.size() == 1u);
	CHECK(endpoint.calls[0].id == 9u);
	CHECK(endpoint.calls[0].lane == ognet::Lane::SimInput);
	std::vector<uint8_t> expected;
	expected.push_back(kSimEnvelopeVersion);
	expected.push_back(static_cast<uint8_t>(SimMessageKind::InputBundleSet));
	expected.push_back(2u);
	expected.push_back(5u);
	for (const uint8_t b : toU8(firstBundle))
	{
		expected.push_back(b);
	}
	expected.push_back(6u);
	for (const uint8_t b : toU8(secondBundle))
	{
		expected.push_back(b);
	}
	CHECK(endpoint.calls[0].bytes == expected);
}

TEST_CASE("SimNetTransport.ASetThatWouldOutgrowOneSimMessageIsSplitAndRefusalsAreCounted", "[NetTransport][SimNetTransport]")
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<FakeTransport>();
	transport->installHandlers(endpoint);

	transport->beginInputSends();
	transport->appendInputBundle(1, testBundle(1, 1));
	transport->endInputSends();
	CHECK(endpoint.calls.empty());
	CHECK(transport->stats().inputSetsWithoutServer == 1u);

	transport->setServerConnection(9u);
	const std::vector<std::byte> large = patternBytes(300u, 9);
	transport->beginInputSends();
	transport->appendInputBundle(1, large);
	transport->appendInputBundle(2, large);
	transport->endInputSends();
	REQUIRE(endpoint.calls.size() == 2u);
	CHECK(transport->stats().inputSetSplits == 1u);
	CHECK(endpoint.calls[0].bytes.size() == simEnvelope::kInputBundleSetHeaderBytes + 1u + large.size());
	CHECK(endpoint.calls[0].bytes[simEnvelope::kHeaderBytes] == 1u);
	CHECK(endpoint.calls[1].bytes[simEnvelope::kHeaderBytes + 1u] == 2u);

	transport->beginInputSends();
	transport->appendInputBundle(0, testBundle(1, 1));
	transport->appendInputBundle(3, patternBytes(1u, 1));
	transport->appendInputBundle(3, patternBytes(kMaxSimMessageBytes, 1));
	transport->endInputSends();
	CHECK(transport->stats().inputBundlesRefused == 3u);
	CHECK(endpoint.calls.size() == 2u);

	transport->beginInputSends();
	for (uint8_t simId = 1; simId <= 9; ++simId)
	{
		transport->appendInputBundle(simId, testBundle(1, simId));
	}
	transport->endInputSends();
	REQUIRE(endpoint.calls.size() == 4u);
	CHECK(endpoint.calls[2].bytes[simEnvelope::kHeaderBytes] == simEnvelope::kMaxInputBundlesPerSet);
	CHECK(endpoint.calls[3].bytes[simEnvelope::kHeaderBytes] == 1u);
}

TEST_CASE("SimNetTransport.Death.InstallHandlersAfterTheEndpointStartedFiresTheCheck", "[.][OGSimDeath]")
{
	RecordingEndpoint endpoint;
	auto transport = std::make_unique<FakeTransport>();
	transport->markEndpointStarted();
	transport->installHandlers(endpoint);
	FAIL("installHandlers after markEndpointStarted did NOT fire SimNetTransport's OG_CHECK");
}
