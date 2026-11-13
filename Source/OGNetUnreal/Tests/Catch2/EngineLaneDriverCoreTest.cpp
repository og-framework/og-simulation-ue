// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SwitchableBackend.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using ognet::ConnectionId;
using ognet::DisconnectReason;
using ognet::Role;
using ognet::test::NetworkMode;
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using ognetunreal::AcceptVerdict;
using ognetunreal::IoMode;
using ognetunreal::SendVerdict;

using Backend = ognetunreal::SwitchableBackend<VirtualBackend>;
using Core = ognetunreal::EngineLaneDriverCore<TestHost, Backend>;

static_assert(ognet::DatagramBackend<Backend> && ognet::ReportsLocalAddress<Backend>, "the rig's backend is the switch over VirtualBackend");

constexpr uint8_t kServerHostIndex = 1;
constexpr uint8_t kClientHostIndex = 10;
constexpr uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr int64_t kStepMicros = 1000;

enum class Kind : uint8_t { Accept, Deliver, Closed, Up, Ended };

struct Event
{
	Kind kind = Kind::Accept;
	ConnectionId id = ognet::kInvalidConnection;
	std::vector<std::byte> bytes;
	DisconnectReason reason = DisconnectReason::Timeout;
};

ognet::Address serverAddress()
{
	return ognet::test::virtualHostAddress(kServerHostIndex, kServerPort);
}

ognet::crypto::Bytes32 buildIdentity()
{
	ognet::crypto::Bytes32 identity{};
	identity.fill(0x24);
	return identity;
}

ognet::EndpointConfig configFor(Role role)
{
	ognet::EndpointConfig config;
	config.role = role;
	config.bind.port = role == Role::Server ? kServerPort : 0;
	config.buildIdentity = buildIdentity();
	config.maxConnections = role == Role::Server ? 16 : config.maxConnections;
	return config;
}

std::vector<std::byte> text(const std::string& value)
{
	std::vector<std::byte> bytes;
	for (const char c : value)
	{
		bytes.push_back(static_cast<std::byte>(c));
	}
	return bytes;
}

std::vector<std::byte> numbered(uint32_t index, std::size_t size = 8)
{
	std::vector<std::byte> bytes(std::max<std::size_t>(size, 4));
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<std::byte>((index * 31u + static_cast<uint32_t>(i) * 7u) & 0xFF);
	}
	for (std::size_t i = 0; i < 4; ++i)
	{
		bytes[i] = static_cast<std::byte>((index >> (8 * i)) & 0xFF);
	}
	return bytes;
}

uint32_t indexOf(const std::vector<std::byte>& bytes)
{
	uint32_t index = 0;
	for (std::size_t i = 0; i < 4 && i < bytes.size(); ++i)
	{
		index |= static_cast<uint32_t>(bytes[i]) << (8 * i);
	}
	return index;
}

class Side
{
public:
	Side(VirtualNetwork& net, uint8_t hostIndex, uint64_t seed)
		: network(net), host(net.clock(), seed), inner(net.addHost(hostIndex)), backend(inner)
	{
	}

	bool start(Role role, IoMode mode)
	{
		m_role = role;
		ognetunreal::DriverCallbacks callbacks;
		callbacks.ctx = this;
		callbacks.accept = &Side::onAccept;
		callbacks.deliver = &Side::onDeliver;
		callbacks.closed = &Side::onClosed;
		callbacks.serverConnectionUp = &Side::onUp;
		callbacks.connectEnded = &Side::onEnded;
		return core.start(host, backend, configFor(role), mode, callbacks);
	}

	bool enginePending() const { return m_role == Role::Client && !isUp() && count(Kind::Ended) == 0; }
	bool isUp() const { return count(Kind::Up) == 1; }

	std::size_t count(Kind kind) const
	{
		std::size_t n = 0;
		for (const Event& event : events)
		{
			n += event.kind == kind ? 1 : 0;
		}
		return n;
	}

	std::vector<Event> of(Kind kind) const
	{
		std::vector<Event> found;
		for (const Event& event : events)
		{
			if (event.kind == kind)
			{
				found.push_back(event);
			}
		}
		return found;
	}

	std::size_t indexOfFirst(Kind kind) const
	{
		for (std::size_t i = 0; i < events.size(); ++i)
		{
			if (events[i].kind == kind)
			{
				return i;
			}
		}
		return events.size();
	}

	ConnectionId peerId() const { return m_role == Role::Client ? core.serverConnection() : m_acceptedId; }

	VirtualNetwork& network;
	TestHost host;
	VirtualBackend& inner;
	Backend backend;
	std::vector<Event> events;
	std::vector<AcceptVerdict> acceptVerdicts;
	std::function<void(const Event&)> onEvent;
	bool ticking = true;
	Core core;

private:
	void record(Event event)
	{
		events.push_back(std::move(event));
		if (onEvent)
		{
			onEvent(events.back());
		}
	}

	static AcceptVerdict onAccept(void* ctx, ConnectionId id)
	{
		Side& self = *static_cast<Side*>(ctx);
		const std::size_t call = self.count(Kind::Accept);
		const AcceptVerdict verdict = call < self.acceptVerdicts.size() ? self.acceptVerdicts[call] : AcceptVerdict::Accept;
		if (verdict == AcceptVerdict::Accept)
		{
			self.m_acceptedId = id;
		}
		self.record(Event{Kind::Accept, id, {}, DisconnectReason::Timeout});
		return verdict;
	}

	static void onDeliver(void* ctx, ConnectionId id, std::span<const std::byte> bytes)
	{
		static_cast<Side*>(ctx)->record(Event{Kind::Deliver, id, std::vector<std::byte>(bytes.begin(), bytes.end()), DisconnectReason::Timeout});
	}

	static void onClosed(void* ctx, ConnectionId id, DisconnectReason reason)
	{
		static_cast<Side*>(ctx)->record(Event{Kind::Closed, id, {}, reason});
	}

	static void onUp(void* ctx, ConnectionId id)
	{
		static_cast<Side*>(ctx)->record(Event{Kind::Up, id, {}, DisconnectReason::Timeout});
	}

	static void onEnded(void* ctx, DisconnectReason reason)
	{
		static_cast<Side*>(ctx)->record(Event{Kind::Ended, ognet::kInvalidConnection, {}, reason});
	}

	Role m_role = Role::Client;
	ConnectionId m_acceptedId = ognet::kInvalidConnection;
};

class Rig
{
public:
	Rig()
	{
		network.setNow(kStartSeconds);
		server = std::make_unique<Side>(network, kServerHostIndex, 100);
		client = std::make_unique<Side>(network, kClientHostIndex, 200);
	}

	double now() const { return network.nowSeconds(); }

	void startServer() { REQUIRE(server->start(Role::Server, IoMode::HostServiced)); }
	void startClient() { REQUIRE(client->start(Role::Client, IoMode::HostServiced)); }

	void step()
	{
		m_stepMicros += kStepMicros;
		network.setNow(kStartSeconds + static_cast<double>(m_stepMicros) * 1e-6);
		const double t = network.nowSeconds();
		for (Side* side : {server.get(), client.get()})
		{
			if (side->core.isStarted())
			{
				side->core.serviceUntil(t);
			}
		}
		for (Side* side : {server.get(), client.get()})
		{
			if (side->core.isStarted() && side->ticking)
			{
				side->core.tick(t, side->enginePending());
			}
		}
	}

	void run(double seconds)
	{
		const int64_t steps = static_cast<int64_t>(std::llround(seconds * 1e6 / static_cast<double>(kStepMicros)));
		for (int64_t i = 0; i < steps; ++i)
		{
			step();
		}
	}

	void runUntilTime(double seconds)
	{
		while (now() < seconds)
		{
			step();
		}
	}

	bool runUntil(const std::function<bool()>& done, double maxSeconds)
	{
		const int64_t steps = static_cast<int64_t>(std::llround(maxSeconds * 1e6 / static_cast<double>(kStepMicros)));
		for (int64_t i = 0; i < steps; ++i)
		{
			if (done())
			{
				return true;
			}
			step();
		}
		return done();
	}

	void connectBoth()
	{
		startServer();
		startClient();
		client->core.connect(serverAddress(), now() + 60.0);
		REQUIRE(runUntil([&] { return client->isUp() && server->count(Kind::Accept) >= 1; }, 5.0));
	}

	VirtualNetwork network;
	std::unique_ptr<Side> server;
	std::unique_ptr<Side> client;

private:
	int64_t m_stepMicros = 0;
};

std::vector<std::vector<std::byte>> deliveredBytes(const Side& side)
{
	std::vector<std::vector<std::byte>> out;
	for (const Event& event : side.of(Kind::Deliver))
	{
		out.push_back(event.bytes);
	}
	return out;
}
std::vector<uint8_t> u8(const std::vector<std::byte>& bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(static_cast<uint8_t>(b));
	}
	return out;
}
} // namespace

TEST_CASE("EngineLaneDriver.PacketsSentBeforeTheServerConnectionIsUpArriveFirstInOrderAndByteExact", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.startServer();
	rig.startClient();
	const std::vector<std::byte> first = numbered(0, 1024);
	const std::vector<std::byte> second = numbered(1, 7);
	const std::vector<std::byte> third = numbered(2, ognetunreal::kPreConnectSlotBytes);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, first) == SendVerdict::Buffered);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, second) == SendVerdict::Buffered);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, third) == SendVerdict::Buffered);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, numbered(3, ognetunreal::kPreConnectSlotBytes + 1)) == SendVerdict::Refused);
	const std::vector<std::byte> fromUp = text("sent-in-serverConnectionUp");
	rig.client->onEvent = [&](const Event& event) {
		if (event.kind == Kind::Up)
		{
			CHECK(rig.client->core.send(event.id, fromUp) == SendVerdict::Sent);
		}
	};
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);

	REQUIRE(rig.runUntil([&] { return rig.server->count(Kind::Deliver) >= 4; }, 5.0));
	const std::vector<std::vector<std::byte>> delivered = deliveredBytes(*rig.server);
	REQUIRE(delivered.size() == 4);
	CHECK(u8(delivered[0]) == u8(first));
	CHECK(u8(delivered[1]) == u8(second));
	CHECK(u8(delivered[2]) == u8(third));
	CHECK(u8(delivered[3]) == u8(fromUp));
	CHECK(rig.server->indexOfFirst(Kind::Accept) < rig.server->indexOfFirst(Kind::Deliver));
	const ognetunreal::DriverStats stats = rig.client->core.stats();
	CHECK(stats.preConnectBuffered == 3);
	CHECK(stats.preConnectDropped == 0);
	CHECK(stats.sendsRefused == 1);
	CHECK(rig.client->core.serverConnection() == rig.client->of(Kind::Up)[0].id);
}

TEST_CASE("EngineLaneDriver.AFullPreConnectBufferKeepsTheOldestAndCountsTheDrop", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.startServer();
	rig.startClient();
	for (uint32_t i = 0; i < ognetunreal::kPreConnectSlots; ++i)
	{
		CHECK(rig.client->core.send(ognet::kInvalidConnection, numbered(i, 100)) == SendVerdict::Buffered);
	}
	CHECK(rig.client->core.send(ognet::kInvalidConnection, numbered(ognetunreal::kPreConnectSlots, 100)) == SendVerdict::BufferFull);
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);

	REQUIRE(rig.runUntil([&] { return rig.server->count(Kind::Deliver) >= ognetunreal::kPreConnectSlots; }, 5.0));
	rig.run(0.5);
	const std::vector<std::vector<std::byte>> delivered = deliveredBytes(*rig.server);
	REQUIRE(delivered.size() == ognetunreal::kPreConnectSlots);
	for (uint32_t i = 0; i < ognetunreal::kPreConnectSlots; ++i)
	{
		CHECK(u8(delivered[i]) == u8(numbered(i, 100)));
	}
	CHECK(rig.client->core.stats().preConnectBuffered == ognetunreal::kPreConnectSlots);
	CHECK(rig.client->core.stats().preConnectDropped == 1);
	CHECK(rig.client->core.stats().sendsRefused == 0);
}

TEST_CASE("EngineLaneDriver.ALastPacketSentWithEngineClosedReachesThePeerBeforeItsClose", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.connectBoth();
	const ConnectionId atServer = rig.server->peerId();
	REQUIRE(atServer != ognet::kInvalidConnection);
	rig.run(0.05);
	const std::vector<std::byte> last = text("the server's last packet");
	CHECK(rig.server->core.send(atServer, last) == SendVerdict::Sent);
	rig.server->core.engineClosed(atServer, rig.now());
	CHECK(rig.server->core.send(atServer, text("after engineClosed")) == SendVerdict::Dead);

	REQUIRE(rig.runUntil([&] { return rig.client->count(Kind::Closed) == 1; }, 5.0));
	const std::vector<Event> closed = rig.client->of(Kind::Closed);
	CHECK(closed[0].reason == DisconnectReason::ClosedByPeer);
	const std::vector<std::vector<std::byte>> delivered = deliveredBytes(*rig.client);
	REQUIRE(delivered.size() == 1);
	CHECK(u8(delivered[0]) == u8(last));
	CHECK(rig.client->indexOfFirst(Kind::Deliver) < rig.client->indexOfFirst(Kind::Closed));
	CHECK(rig.server->core.stats().closesDrained == 1);
	CHECK(rig.server->core.stats().closesTimedOut == 0);
	rig.run(0.5);
	CHECK(rig.server->count(Kind::Closed) == 0);
	CHECK(rig.client->core.send(closed[0].id, text("late")) == SendVerdict::Dead);
}

TEST_CASE("EngineLaneDriver.AClientWhoseConnectFailsBeforeTheDeadlineReconnectsAndALaterServerAcceptsIt", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.startClient();
	const double connectedAt = rig.now();
	rig.client->core.connect(serverAddress(), connectedAt + 60.0);

	rig.runUntilTime(connectedAt + 9.9);
	CHECK(rig.client->core.stats().retries == 0);
	rig.runUntilTime(connectedAt + 10.1);
	CHECK(rig.client->core.stats().retries == 1);
	CHECK(rig.client->count(Kind::Ended) == 0);

	rig.runUntilTime(connectedAt + 15.0);
	rig.startServer();
	REQUIRE(rig.runUntil([&] { return rig.client->isUp() && rig.server->count(Kind::Accept) == 1; }, 5.0));
	CHECK(rig.client->core.stats().retries == 1);
	CHECK(rig.client->count(Kind::Ended) == 0);
	CHECK(rig.server->core.stats().accepted == 1);
}

TEST_CASE("EngineLaneDriver.AClientWhoseConnectFailsAfterTheDeadlineEndsOnceWithoutARetry", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.startClient();
	const double connectedAt = rig.now();
	rig.client->core.connect(serverAddress(), connectedAt + 5.0);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, text("hello")) == SendVerdict::Buffered);

	rig.runUntilTime(connectedAt + 10.5);
	REQUIRE(rig.client->count(Kind::Ended) == 1);
	CHECK(rig.client->of(Kind::Ended)[0].reason == DisconnectReason::ConnectFailed);
	CHECK(rig.client->core.stats().retries == 0);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, text("after the end")) == SendVerdict::Dead);

	rig.startServer();
	rig.runUntilTime(connectedAt + 25.0);
	CHECK(rig.client->count(Kind::Ended) == 1);
	CHECK(rig.client->core.stats().retries == 0);
	CHECK_FALSE(rig.client->isUp());
	CHECK(rig.server->count(Kind::Accept) == 0);
}

TEST_CASE("EngineLaneDriver.ARejectedConnectionIsClosedByThePeerAndDeliversNothing", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.server->acceptVerdicts = {AcceptVerdict::Reject};
	rig.startServer();
	rig.startClient();
	rig.client->onEvent = [&](const Event& event) {
		if (event.kind == Kind::Up)
		{
			rig.client->core.send(event.id, text("from a rejected client"));
		}
	};
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);

	REQUIRE(rig.runUntil([&] { return rig.client->count(Kind::Closed) == 1; }, 5.0));
	CHECK(rig.client->of(Kind::Closed)[0].reason == DisconnectReason::ClosedByPeer);
	rig.run(0.5);
	CHECK(rig.server->count(Kind::Accept) == 1);
	CHECK(rig.server->count(Kind::Deliver) == 0);
	CHECK(rig.server->count(Kind::Closed) == 0);
	CHECK(rig.server->core.stats().rejected == 1);
	CHECK(rig.server->core.stats().accepted == 0);
}

TEST_CASE("EngineLaneDriver.AnIgnoredConnectionDropsItsDatagramsUntilItIsAccepted", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.server->acceptVerdicts = {AcceptVerdict::Ignore, AcceptVerdict::Ignore, AcceptVerdict::Ignore};
	rig.startServer();
	rig.startClient();
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);
	REQUIRE(rig.runUntil([&] { return rig.client->isUp(); }, 5.0));
	const ConnectionId atClient = rig.client->core.serverConnection();
	constexpr uint32_t kSent = 10;
	for (uint32_t i = 0; i < kSent; ++i)
	{
		CHECK(rig.client->core.send(atClient, numbered(i)) == SendVerdict::Sent);
		rig.step();
	}
	REQUIRE(rig.runUntil([&] { return rig.server->count(Kind::Deliver) == kSent - 2; }, 2.0));
	rig.run(0.2);

	CHECK(rig.server->count(Kind::Accept) == 4);
	const std::vector<std::vector<std::byte>> delivered = deliveredBytes(*rig.server);
	REQUIRE(delivered.size() == kSent - 2);
	for (uint32_t i = 0; i < kSent - 2; ++i)
	{
		CHECK(u8(delivered[i]) == u8(numbered(i + 2)));
	}
	CHECK(rig.server->core.stats().ignoredDatagrams == 2);
	CHECK(rig.server->core.stats().accepted == 1);
	CHECK(rig.server->core.stats().rejected == 0);
}

TEST_CASE("EngineLaneDriver.ADeliverThatStopsTheCoreRunsNoFurtherCallback", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.connectBoth();
	rig.run(0.05);
	const ConnectionId atClient = rig.client->core.serverConnection();
	std::size_t callbacksAfterStop = 0;
	bool stopped = false;
	rig.server->onEvent = [&](const Event& event) {
		if (stopped)
		{
			++callbacksAfterStop;
			return;
		}
		if (event.kind == Kind::Deliver)
		{
			stopped = true;
			rig.server->core.stop(rig.now(), 0.0);
		}
	};
	for (uint32_t i = 0; i < 5; ++i)
	{
		CHECK(rig.client->core.send(atClient, numbered(i)) == SendVerdict::Sent);
	}
	REQUIRE(rig.runUntil([&] { return stopped; }, 2.0));
	rig.run(0.5);

	CHECK(callbacksAfterStop == 0);
	CHECK(rig.server->count(Kind::Deliver) == 1);
	CHECK(rig.server->core.isStopped());
	CHECK(rig.server->core.endpointStats().stopped);
	CHECK(rig.server->core.send(rig.server->peerId(), text("after stop")) == SendVerdict::Dead);
	CHECK(rig.client->count(Kind::Closed) == 1);
}

TEST_CASE("EngineLaneDriver.SixHundredDatagramsQueuedBetweenTwoTicksArriveInOrderInBatchesOf256", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	rig.connectBoth();
	rig.run(0.05);
	const ConnectionId atClient = rig.client->core.serverConnection();
	const ognetunreal::DriverStats before = rig.server->core.stats();
	rig.server->ticking = false;
	constexpr uint32_t kSent = 600;
	for (uint32_t i = 0; i < kSent; ++i)
	{
		REQUIRE(rig.client->core.send(atClient, numbered(i)) == SendVerdict::Sent);
	}
	rig.run(0.1);
	CHECK(rig.server->count(Kind::Deliver) == 0);
	rig.server->core.tick(rig.now(), false);

	const std::vector<std::vector<std::byte>> delivered = deliveredBytes(*rig.server);
	REQUIRE(delivered.size() == kSent);
	for (uint32_t i = 0; i < kSent; ++i)
	{
		CHECK(indexOf(delivered[i]) == i);
	}
	const ognetunreal::DriverStats after = rig.server->core.stats();
	CHECK(after.batches - before.batches >= 3);
	CHECK(after.maxBatch == ognetunreal::kBatchEvents);
}

TEST_CASE("EngineLaneDriver.AClientWhoseEmulatorDropsEveryOutboundDatagramNeverReachesTheServer", "[OGNet][EngineLaneDriver]")
{
	Rig rig;
	ognet::EmulatorConfig lossy;
	lossy.outbound.lossPercent = 100.0;
	REQUIRE(rig.client->backend.enableEmulator(lossy, rig.network.clock()));
	rig.startServer();
	rig.startClient();
	CHECK(rig.client->backend.enableEmulator(lossy, rig.network.clock()) == false);
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);
	CHECK(rig.client->core.send(ognet::kInvalidConnection, text("never")) == SendVerdict::Buffered);
	rig.run(3.0);

	CHECK_FALSE(rig.client->isUp());
	CHECK(rig.server->count(Kind::Accept) == 0);
	CHECK(rig.server->inner.stats().received == 0);
	REQUIRE(rig.client->backend.emulatorStats().has_value());
	CHECK(rig.client->backend.emulatorStats()->outbound.lost > 0);
	CHECK(rig.client->backend.emulatorStats()->outbound.lost == rig.client->backend.emulatorStats()->outbound.offered);
}

TEST_CASE("EngineLaneDriver.OnItsOwnThreadEachSideSendsFiveHundredInOrderAndStopDrainsTheLastPacket", "[OGNet][EngineLaneDriver]")
{
	constexpr uint32_t kEachWay = 500;
	constexpr double kGiveUpSeconds = 30.0;
	VirtualNetwork network(NetworkMode::Steady);
	Side server(network, kServerHostIndex, 300);
	Side client(network, kClientHostIndex, 400);
	REQUIRE(server.start(Role::Server, IoMode::OwnThread));
	REQUIRE(client.start(Role::Client, IoMode::OwnThread));
	const double giveUpAt = network.nowSeconds() + kGiveUpSeconds;
	client.core.connect(serverAddress(), network.nowSeconds() + kGiveUpSeconds);

	uint32_t sentByServer = 0;
	uint32_t sentByClient = 0;
	bool clientStopped = false;
	auto frame = [&] {
		const double now = network.nowSeconds();
		server.core.tick(now, false);
		client.core.tick(now, client.enginePending());
		const ConnectionId serverPeer = server.peerId();
		if (serverPeer != ognet::kInvalidConnection && !clientStopped)
		{
			for (int i = 0; i < 10 && sentByServer < kEachWay; ++i, ++sentByServer)
			{
				CHECK(server.core.send(serverPeer, numbered(sentByServer, 64)) == SendVerdict::Sent);
			}
		}
		if (client.isUp() && !clientStopped)
		{
			for (int i = 0; i < 10 && sentByClient < kEachWay - 1; ++i, ++sentByClient)
			{
				CHECK(client.core.send(client.core.serverConnection(), numbered(sentByClient, 64)) == SendVerdict::Sent);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	};

	while (network.nowSeconds() < giveUpAt
		&& (client.count(Kind::Deliver) < kEachWay || server.count(Kind::Deliver) < kEachWay - 1))
	{
		frame();
	}
	REQUIRE(client.count(Kind::Deliver) == kEachWay);
	REQUIRE(server.count(Kind::Deliver) == kEachWay - 1);

	CHECK(client.core.send(client.core.serverConnection(), numbered(kEachWay - 1, 64)) == SendVerdict::Sent);
	client.core.stop(network.nowSeconds(), 0.2);
	clientStopped = true;
	CHECK(client.core.isStopped());
	while (network.nowSeconds() < giveUpAt && server.count(Kind::Closed) == 0)
	{
		frame();
	}
	REQUIRE(server.count(Kind::Closed) == 1);

	const std::vector<std::vector<std::byte>> atServer = deliveredBytes(server);
	REQUIRE(atServer.size() == kEachWay);
	for (uint32_t i = 0; i < kEachWay; ++i)
	{
		CHECK(u8(atServer[i]) == u8(numbered(i, 64)));
	}
	CHECK(server.indexOfFirst(Kind::Closed) == server.events.size() - 1);
	CHECK(server.of(Kind::Closed)[0].reason == DisconnectReason::ClosedByPeer);
	const std::vector<std::vector<std::byte>> atClient = deliveredBytes(client);
	for (uint32_t i = 0; i < kEachWay; ++i)
	{
		CHECK(u8(atClient[i]) == u8(numbered(i, 64)));
	}
	CHECK(server.core.stats().sendsRefused == 0);
	CHECK(client.core.stats().sendsRefused == 0);
	server.core.stop(network.nowSeconds(), 0.2);
}
