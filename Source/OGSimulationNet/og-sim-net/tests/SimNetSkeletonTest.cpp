// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetVersion.h"
#include "OGSimulation/Network/SimTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;

constexpr uint8_t kServerHostIndex = 1;
constexpr uint8_t kClientHostIndex = 10;
constexpr uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr double kStepSeconds = 0.001;
constexpr int kMaxSteps = 5000;

struct Delivered
{
	ognet::ConnectionId id = ognet::kInvalidConnection;
	ognet::Lane lane = ognet::Lane::Control;
	std::vector<uint8_t> bytes;
};

std::vector<uint8_t> toU8(std::span<const std::byte> bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(static_cast<uint8_t>(b));
	}
	return out;
}

struct Recorder
{
	std::vector<ognet::ConnectionEvent> events;
	std::vector<Delivered> messages;

	static void onMessage(void* ctx, ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes, double)
	{
		static_cast<Recorder*>(ctx)->messages.push_back(Delivered{id, lane, toU8(bytes)});
	}

	static void onEvent(void* ctx, const ognet::ConnectionEvent& event) { static_cast<Recorder*>(ctx)->events.push_back(event); }

	ognet::ConnectionId connectedId() const
	{
		for (const ognet::ConnectionEvent& event : events)
		{
			if (event.kind == ognet::ConnectionEventKind::Connected)
			{
				return event.id;
			}
		}
		return ognet::kInvalidConnection;
	}
};

struct Peer
{
	Peer(VirtualNetwork& network, VirtualBackend& backendIn, uint64_t seed) : host(network.clock(), seed), backend(backendIn), endpoint(host, backend) {}

	void start(ognet::Role role)
	{
		for (std::size_t lane = 0; lane < ognet::kLaneCount; ++lane)
		{
			endpoint.setLaneHandler(static_cast<ognet::Lane>(lane), ognet::LaneHandler{&Recorder::onMessage, &recorder});
		}
		endpoint.setConnectionHandler(ognet::ConnectionHandler{&Recorder::onEvent, &recorder});
		ognet::EndpointConfig config;
		config.role = role;
		if (role == ognet::Role::Server)
		{
			config.bind.port = kServerPort;
		}
		config.buildIdentity.fill(0x5A);
		REQUIRE(endpoint.start(config));
	}

	TestHost host;
	VirtualBackend& backend;
	Recorder recorder;
	ognet::NetEndpoint<TestHost, VirtualBackend> endpoint;
};

template <typename DoneT>
bool stepUntil(VirtualNetwork& network, Peer& server, Peer& client, int& step, DoneT done)
{
	for (; step < kMaxSteps; ++step)
	{
		if (done())
		{
			return true;
		}
		network.setNow(kStartSeconds + static_cast<double>(step + 1) * kStepSeconds);
		server.endpoint.serviceUntil(network.nowSeconds());
		client.endpoint.serviceUntil(network.nowSeconds());
	}
	return done();
}
} // namespace

TEST_CASE("SimNetSkeleton.AVirtualNetworkEndpointPairCarriesAMessageOnTheSimInputLane", "[NetTransport][SimNetSkeleton]")
{
	STATIC_CHECK(ogsimnet::kSimNetApiVersion == 1u);
	STATIC_CHECK(static_cast<ognet::Lane>(SimLane::SimInput) == ognet::Lane::SimInput);

	VirtualNetwork network;
	network.setNow(kStartSeconds);
	Peer server(network, network.addHost(kServerHostIndex), 100);
	Peer client(network, network.addHost(kClientHostIndex), 200);
	server.start(ognet::Role::Server);
	client.start(ognet::Role::Client);

	client.endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
	int step = 0;
	REQUIRE(stepUntil(network, server, client, step, [&] {
		return server.recorder.connectedId() != ognet::kInvalidConnection && client.recorder.connectedId() != ognet::kInvalidConnection;
	}));

	const std::array<std::byte, 3> payload{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
	client.endpoint.send(client.recorder.connectedId(), static_cast<ognet::Lane>(SimLane::SimInput), payload);
	REQUIRE(stepUntil(network, server, client, step, [&] { return !server.recorder.messages.empty(); }));

	REQUIRE(server.recorder.messages.size() == 1u);
	const Delivered& delivered = server.recorder.messages.front();
	CHECK(delivered.id == server.recorder.connectedId());
	CHECK(delivered.lane == ognet::Lane::SimInput);
	CHECK(delivered.bytes == toU8(payload));
}
