// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/Core/SwitchableBackend.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using ognet::ConnectionEventKind;
using ognet::ConnectionId;
using ognet::DisconnectReason;
using ognet::Lane;
using ognet::Role;
using ognet::test::NetworkMode;
using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using ognetunreal::AcceptVerdict;
using ognetunreal::IoMode;
using ognetunreal::SendVerdict;
using ognetunreal::SimLaneAttach;
using ognetunreal::SimLaneEndpoint;

using Backend = ognetunreal::SwitchableBackend<VirtualBackend>;
using Core = ognetunreal::EngineLaneDriverCore<TestHost, Backend>;
using SeamTransport = ogsimnet::SimNetTransport<SimLaneEndpoint, 128>;

static_assert(ogsimnet::SimNetEndpoint<SimLaneEndpoint>, "the erased endpoint is og-sim-net's SimNetEndpoint");

constexpr uint8_t kServerHostIndex = 1;
constexpr uint8_t kClientHostIndex = 10;
constexpr uint16_t kServerPort = 7777;
constexpr double kStartSeconds = 1.0;
constexpr int64_t kStepMicros = 1000;
constexpr std::array<Lane, 5> kSimLanes{Lane::Control, Lane::SimClock, Lane::SimInput, Lane::SimRelay, Lane::SimState};
constexpr std::array<Lane, 3> kUpLanes{Lane::Control, Lane::SimClock, Lane::SimInput};
constexpr std::array<Lane, 4> kDownLanes{Lane::Control, Lane::SimClock, Lane::SimRelay, Lane::SimState};

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

ognet::EndpointConfig configFor(Role role)
{
	ognet::EndpointConfig config;
	config.role = role;
	config.bind.port = role == Role::Server ? kServerPort : 0;
	config.buildIdentity.fill(0x24);
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

std::vector<uint8_t> u8(std::span<const std::byte> bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(static_cast<uint8_t>(b));
	}
	return out;
}

template <typename LanesT>
std::vector<int> laneNumbers(const LanesT& lanes)
{
	std::vector<int> out;
	for (const Lane lane : lanes)
	{
		out.push_back(static_cast<int>(lane));
	}
	return out;
}

std::vector<std::byte> laneMessage(Lane lane, Role from)
{
	return text(std::string(from == Role::Server ? "server" : "client") + "-lane-" + std::to_string(static_cast<int>(lane)));
}

struct SeamRecord
{
	bool isMessage = false;
	ConnectionEventKind kind = ConnectionEventKind::Connected;
	ConnectionId id = ognet::kInvalidConnection;
	DisconnectReason reason = DisconnectReason::Timeout;
	Lane lane = Lane::Control;
	std::vector<uint8_t> bytes;
	double recvSeconds = 0.0;
	std::thread::id thread;
};

class RecordingAttach
{
public:
	SimLaneAttach attach() { return SimLaneAttach{this, &RecordingAttach::beforeStart}; }

	std::vector<SeamRecord> records() const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_records;
	}

	std::vector<SeamRecord> messagesOn(Lane lane) const
	{
		std::vector<SeamRecord> out;
		for (const SeamRecord& record : records())
		{
			if (record.isMessage && record.lane == lane)
			{
				out.push_back(record);
			}
		}
		return out;
	}

	std::size_t connectionEvents(ConnectionEventKind kind) const
	{
		std::size_t n = 0;
		for (const SeamRecord& record : records())
		{
			n += !record.isMessage && record.kind == kind ? 1 : 0;
		}
		return n;
	}

	std::vector<Lane> installed;
	SimLaneEndpoint* endpoint = nullptr;

private:
	static void beforeStart(void* ctx, SimLaneEndpoint& endpoint)
	{
		RecordingAttach& self = *static_cast<RecordingAttach*>(ctx);
		self.endpoint = &endpoint;
		for (const Lane lane : kSimLanes)
		{
			endpoint.setLaneHandler(lane, ognet::LaneHandler{&RecordingAttach::onLane, &self});
			self.installed.push_back(lane);
		}
		endpoint.setConnectionHandler(ognet::ConnectionHandler{&RecordingAttach::onConnection, &self});
	}

	static void onLane(void* ctx, ConnectionId id, Lane lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		RecordingAttach& self = *static_cast<RecordingAttach*>(ctx);
		SeamRecord record;
		record.isMessage = true;
		record.id = id;
		record.lane = lane;
		record.bytes = u8(bytes);
		record.recvSeconds = recvSeconds;
		record.thread = std::this_thread::get_id();
		const std::lock_guard<std::mutex> lock(self.m_mutex);
		self.m_records.push_back(std::move(record));
	}

	static void onConnection(void* ctx, const ognet::ConnectionEvent& event)
	{
		RecordingAttach& self = *static_cast<RecordingAttach*>(ctx);
		SeamRecord record;
		record.kind = event.kind;
		record.id = event.id;
		record.reason = event.reason;
		record.thread = std::this_thread::get_id();
		const std::lock_guard<std::mutex> lock(self.m_mutex);
		self.m_records.push_back(std::move(record));
	}

	mutable std::mutex m_mutex;
	std::vector<SeamRecord> m_records;
};

class TransportAttach
{
public:
	SimLaneAttach attach() { return SimLaneAttach{this, &TransportAttach::beforeStart}; }

	std::unique_ptr<SeamTransport> transport;
	SimLaneEndpoint* endpoint = nullptr;

private:
	static void beforeStart(void* ctx, SimLaneEndpoint& endpoint)
	{
		TransportAttach& self = *static_cast<TransportAttach*>(ctx);
		self.endpoint = &endpoint;
		self.transport = std::make_unique<SeamTransport>(ogsimnet::simNetConfigFor(endpoint.config()));
		self.transport->installHandlers(endpoint);
		endpoint.setConnectionHandler(self.transport->connectionHandler());
	}
};

class Side
{
public:
	Side(VirtualNetwork& net, uint8_t hostIndex, uint64_t seed) : host(net.clock(), seed), inner(net.addHost(hostIndex)), backend(inner) {}

	bool start(Role role, IoMode mode, const SimLaneAttach* attach)
	{
		m_role = role;
		ognetunreal::DriverCallbacks callbacks;
		callbacks.ctx = this;
		callbacks.accept = &Side::onAccept;
		callbacks.deliver = &Side::onDeliver;
		callbacks.closed = &Side::onClosed;
		callbacks.serverConnectionUp = &Side::onUp;
		callbacks.connectEnded = &Side::onEnded;
		return core.start(host, backend, configFor(role), mode, callbacks, attach);
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

	std::vector<std::vector<std::byte>> delivered() const
	{
		std::vector<std::vector<std::byte>> out;
		for (const Event& event : of(Kind::Deliver))
		{
			out.push_back(event.bytes);
		}
		return out;
	}

	ConnectionId peerId() const { return m_role == Role::Client ? core.serverConnection() : m_acceptedId; }

	TestHost host;
	VirtualBackend& inner;
	Backend backend;
	std::vector<Event> events;
	std::vector<AcceptVerdict> acceptVerdicts;
	std::function<void(const Event&)> onEvent;
	bool ticking = true;
	RecordingAttach recording;
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

	static void onUp(void* ctx, ConnectionId id) { static_cast<Side*>(ctx)->record(Event{Kind::Up, id, {}, DisconnectReason::Timeout}); }

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
	explicit Rig(bool withAttach) : m_withAttach(withAttach)
	{
		network.setNow(kStartSeconds);
		server = std::make_unique<Side>(network, kServerHostIndex, 100);
		client = std::make_unique<Side>(network, kClientHostIndex, 200);
	}

	double now() const { return network.nowSeconds(); }

	void startServer() { startSide(*server, Role::Server); }
	void startClient() { startSide(*client, Role::Client); }

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
	void startSide(Side& side, Role role)
	{
		const SimLaneAttach attach = side.recording.attach();
		REQUIRE(side.start(role, IoMode::HostServiced, m_withAttach ? &attach : nullptr));
		CHECK(side.recording.installed.size() == (m_withAttach ? kSimLanes.size() : 0u));
	}

	bool m_withAttach = false;
	int64_t m_stepMicros = 0;
};

std::string fieldOf(const std::string& line, const std::string& name)
{
	const std::string key = " " + name + "=";
	const std::size_t at = line.find(key);
	if (at == std::string::npos)
	{
		return {};
	}
	const std::size_t begin = at + key.size();
	const std::size_t end = line.find(' ', begin);
	return line.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

uint64_t histogramTotal(const std::string& histogram)
{
	uint64_t total = 0;
	std::size_t begin = 0;
	while (!histogram.empty())
	{
		const std::size_t end = histogram.find('/', begin);
		total += std::stoull(histogram.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
		if (end == std::string::npos)
		{
			break;
		}
		begin = end + 1;
	}
	return total;
}

struct Drained
{
	InboundKind kind = InboundKind::Message;
	SimLane lane = SimLane::Control;
	SimConnectionId conn = kInvalidSimConnection;
	double recvSeconds = 0.0;
	std::vector<uint8_t> bytes;
};

std::vector<Drained> drainAll(SeamTransport& transport)
{
	std::vector<Drained> out;
	transport.inbox().drain([&](const InboundSimRecord& record) {
		out.push_back(Drained{record.kind, record.lane, record.conn, record.recvSeconds, u8(record.payload())});
	});
	return out;
}

template <bool HighResolution>
class HighResolutionVirtualBackend
{
public:
	explicit HighResolutionVirtualBackend(VirtualBackend& inner) : m_inner(inner) {}

	bool open(const ognet::BindConfig& config) { return m_inner.open(config); }
	ognet::SendResult sendTo(const ognet::Address& to, std::span<const std::byte> bytes) { return m_inner.sendTo(to, bytes); }
	std::size_t receive(uint32_t timeoutUs, ognet::RecvFn fn) { return m_inner.receive(timeoutUs, fn); }
	bool rebind() { return m_inner.rebind(); }
	void wake() { m_inner.wake(); }
	bool hasHighResolutionWait() const { return HighResolution; }

private:
	VirtualBackend& m_inner;
};

template <typename B>
concept ReportsHighResolutionWait = requires(const B& backend) { backend.hasHighResolutionWait(); };

static_assert(!ReportsHighResolutionWait<Backend>, "the switch offers hasHighResolutionWait only when its inner backend has it");
static_assert(ReportsHighResolutionWait<ognetunreal::SwitchableBackend<HighResolutionVirtualBackend<true>>>,
	"the switch forwards hasHighResolutionWait when its inner backend has it");

ognetunreal::DriverCallbacks inertCallbacks()
{
	ognetunreal::DriverCallbacks callbacks;
	callbacks.accept = [](void*, ConnectionId) { return AcceptVerdict::Accept; };
	callbacks.deliver = [](void*, ConnectionId, std::span<const std::byte>) {};
	callbacks.closed = [](void*, ConnectionId, DisconnectReason) {};
	callbacks.serverConnectionUp = [](void*, ConnectionId) {};
	callbacks.connectEnded = [](void*, DisconnectReason) {};
	return callbacks;
}

template <bool HighResolution>
std::pair<bool, bool> hostServicedHighResolutionWait()
{
	using InnerBackend = HighResolutionVirtualBackend<HighResolution>;
	using SwitchedBackend = ognetunreal::SwitchableBackend<InnerBackend>;
	VirtualNetwork network;
	network.setNow(kStartSeconds);
	TestHost host(network.clock(), 700);
	InnerBackend inner(network.addHost(kServerHostIndex));
	SwitchedBackend backend(inner);
	ognetunreal::EngineLaneDriverCore<TestHost, SwitchedBackend> core;
	RecordingAttach recording;
	const SimLaneAttach attach = recording.attach();
	REQUIRE(core.start(host, backend, configFor(Role::Server), IoMode::HostServiced, inertCallbacks(), &attach));
	REQUIRE(recording.endpoint != nullptr);
	const std::pair<bool, bool> reported{core.hasHighResolutionWait(), recording.endpoint->hasHighResolutionWait()};
	core.stop(network.nowSeconds(), 0.0);
	return reported;
}
} // namespace

TEST_CASE("EngineLaneDriver.ARecordingAttachInstallsTheFiveSimLaneHandlersBeforeStartAndEachLaneDeliversWithItsReceiveTime",
	"[OGNet][EngineLaneDriver]")
{
	Rig rig(true);
	rig.connectBoth();
	REQUIRE(rig.server->recording.endpoint != nullptr);
	REQUIRE(rig.client->recording.endpoint != nullptr);
	CHECK(laneNumbers(rig.server->recording.installed) == laneNumbers(kSimLanes));
	CHECK(laneNumbers(rig.client->recording.installed) == laneNumbers(kSimLanes));
	CHECK(rig.server->recording.endpoint->config().role == Role::Server);
	CHECK(rig.server->recording.endpoint->config().maxConnections == 16);
	CHECK(rig.client->recording.endpoint->config().role == Role::Client);
	rig.run(0.05);

	const ConnectionId atServer = rig.server->peerId();
	const ConnectionId atClient = rig.client->core.serverConnection();
	const double sentAt = rig.now();
	for (const Lane lane : kUpLanes)
	{
		rig.client->recording.endpoint->send(atClient, lane, laneMessage(lane, Role::Client));
	}
	for (const Lane lane : kDownLanes)
	{
		rig.server->recording.endpoint->send(atServer, lane, laneMessage(lane, Role::Server));
	}
	REQUIRE(rig.runUntil(
		[&] {
			return rig.server->recording.messagesOn(Lane::SimInput).size() == 1 && rig.client->recording.messagesOn(Lane::SimState).size() == 1
				&& rig.client->recording.messagesOn(Lane::SimRelay).size() == 1 && rig.server->recording.messagesOn(Lane::Control).size() == 1
				&& rig.client->recording.messagesOn(Lane::Control).size() == 1 && rig.server->recording.messagesOn(Lane::SimClock).size() == 1
				&& rig.client->recording.messagesOn(Lane::SimClock).size() == 1;
		},
		2.0));
	rig.run(0.1);

	for (const Lane lane : kUpLanes)
	{
		const std::vector<SeamRecord> got = rig.server->recording.messagesOn(lane);
		REQUIRE(got.size() == 1);
		CHECK(got[0].id == atServer);
		CHECK(got[0].bytes == u8(laneMessage(lane, Role::Client)));
		CHECK(got[0].recvSeconds > sentAt);
		CHECK(got[0].recvSeconds <= rig.now());
	}
	for (const Lane lane : kDownLanes)
	{
		const std::vector<SeamRecord> got = rig.client->recording.messagesOn(lane);
		REQUIRE(got.size() == 1);
		CHECK(got[0].id == atClient);
		CHECK(got[0].bytes == u8(laneMessage(lane, Role::Server)));
		CHECK(got[0].recvSeconds > sentAt);
		CHECK(got[0].recvSeconds <= rig.now());
	}
	CHECK(rig.server->count(Kind::Deliver) == 0);
	CHECK(rig.client->count(Kind::Deliver) == 0);
}

TEST_CASE("EngineLaneDriver.EveryConnectionEventIsForwardedAndConnectedPrecedesItsConnectionsFirstMessage", "[OGNet][EngineLaneDriver]")
{
	Rig rig(true);
	rig.startServer();
	rig.startClient();
	rig.client->onEvent = [&](const Event& event) {
		if (event.kind == Kind::Up)
		{
			rig.client->recording.endpoint->send(event.id, Lane::SimInput, text("first, sent at up"));
		}
	};
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);
	REQUIRE(rig.runUntil([&] { return rig.server->recording.messagesOn(Lane::SimInput).size() == 1; }, 5.0));
	const ConnectionId atServer = rig.server->peerId();
	const ConnectionId atClient = rig.client->core.serverConnection();
	rig.server->recording.endpoint->send(atServer, Lane::SimState, text("first down"));
	REQUIRE(rig.runUntil([&] { return rig.client->recording.messagesOn(Lane::SimState).size() == 1; }, 2.0));

	rig.client->core.engineClosed(atClient, rig.now());
	REQUIRE(rig.runUntil(
		[&] {
			return rig.server->recording.connectionEvents(ConnectionEventKind::Disconnected) == 1
				&& rig.client->recording.connectionEvents(ConnectionEventKind::Disconnected) == 1;
		},
		2.0));
	rig.run(0.5);

	const std::vector<SeamRecord> atServerSide = rig.server->recording.records();
	REQUIRE(atServerSide.size() == 3);
	CHECK_FALSE(atServerSide[0].isMessage);
	CHECK(atServerSide[0].kind == ConnectionEventKind::Connected);
	CHECK(atServerSide[0].id == atServer);
	CHECK(atServerSide[1].isMessage);
	CHECK(atServerSide[1].id == atServer);
	CHECK_FALSE(atServerSide[2].isMessage);
	CHECK(atServerSide[2].kind == ConnectionEventKind::Disconnected);
	CHECK(atServerSide[2].id == atServer);
	CHECK(atServerSide[2].reason == DisconnectReason::ClosedByPeer);

	const std::vector<SeamRecord> atClientSide = rig.client->recording.records();
	REQUIRE(atClientSide.size() == 3);
	CHECK_FALSE(atClientSide[0].isMessage);
	CHECK(atClientSide[0].kind == ConnectionEventKind::Connected);
	CHECK(atClientSide[0].id == atClient);
	CHECK(atClientSide[1].isMessage);
	CHECK(atClientSide[1].lane == Lane::SimState);
	CHECK_FALSE(atClientSide[2].isMessage);
	CHECK(atClientSide[2].kind == ConnectionEventKind::Disconnected);
	CHECK(atClientSide[2].id == atClient);

	CHECK(rig.server->count(Kind::Closed) == 1);
	CHECK(rig.server->core.stats().handlerQueueDropped == 0);
}

TEST_CASE("EngineLaneDriver.AFailedConnectsIdZeroDisconnectedIsForwardedAndStillEndsTheEngineConnectOnce", "[OGNet][EngineLaneDriver]")
{
	Rig rig(true);
	rig.startClient();
	const double connectedAt = rig.now();
	rig.client->core.connect(serverAddress(), connectedAt + 5.0);
	rig.runUntilTime(connectedAt + 10.5);

	const std::vector<SeamRecord> records = rig.client->recording.records();
	REQUIRE(records.size() == 1);
	CHECK_FALSE(records[0].isMessage);
	CHECK(records[0].kind == ConnectionEventKind::Disconnected);
	CHECK(records[0].id == ognet::kInvalidConnection);
	CHECK(records[0].reason == DisconnectReason::ConnectFailed);
	REQUIRE(rig.client->count(Kind::Ended) == 1);
	CHECK(rig.client->of(Kind::Ended)[0].reason == DisconnectReason::ConnectFailed);
	CHECK(rig.client->core.stats().retries == 0);
}

TEST_CASE("EngineLaneDriver.OnItsOwnThreadTheForwardRunsOnOGNetsThreadBeforeTheConnectionsFirstMessage", "[OGNet][EngineLaneDriver]")
{
	constexpr double kGiveUpSeconds = 30.0;
	VirtualNetwork network(NetworkMode::Steady);
	Side server(network, kServerHostIndex, 300);
	Side client(network, kClientHostIndex, 400);
	const SimLaneAttach serverAttach = server.recording.attach();
	const SimLaneAttach clientAttach = client.recording.attach();
	REQUIRE(server.start(Role::Server, IoMode::OwnThread, &serverAttach));
	REQUIRE(client.start(Role::Client, IoMode::OwnThread, &clientAttach));
	const double giveUpAt = network.nowSeconds() + kGiveUpSeconds;
	client.core.connect(serverAddress(), giveUpAt);
	const std::thread::id testThread = std::this_thread::get_id();

	bool sent = false;
	while (network.nowSeconds() < giveUpAt && server.recording.messagesOn(Lane::SimInput).empty())
	{
		server.core.tick(network.nowSeconds(), false);
		client.core.tick(network.nowSeconds(), client.enginePending());
		if (client.isUp() && !sent)
		{
			client.recording.endpoint->send(client.core.serverConnection(), Lane::SimInput, text("own thread"));
			sent = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	REQUIRE(server.recording.messagesOn(Lane::SimInput).size() == 1);

	const std::vector<SeamRecord> atServer = server.recording.records();
	REQUIRE(atServer.size() == 2);
	CHECK_FALSE(atServer[0].isMessage);
	CHECK(atServer[0].kind == ConnectionEventKind::Connected);
	CHECK(atServer[1].isMessage);
	CHECK(atServer[0].thread != testThread);
	CHECK(atServer[0].thread == atServer[1].thread);
	const std::vector<SeamRecord> atClient = client.recording.records();
	REQUIRE_FALSE(atClient.empty());
	CHECK(atClient[0].kind == ConnectionEventKind::Connected);
	CHECK(atClient[0].thread != testThread);
	CHECK(atClient[0].thread != atServer[0].thread);

	client.core.stop(network.nowSeconds(), 0.2);
	server.core.stop(network.nowSeconds(), 0.2);
}

TEST_CASE("EngineLaneDriver.AHostServicedServerServicedOnlyByARunnerThreadDeliversEngineAndSimLaneTraffic", "[OGNet][EngineLaneDriver]")
{
	constexpr double kGiveUpSeconds = 30.0;
	constexpr double kRunnerSliceSeconds = 0.002;
	VirtualNetwork network(NetworkMode::Steady);
	Side server(network, kServerHostIndex, 500);
	Side client(network, kClientHostIndex, 600);
	const SimLaneAttach serverAttach = server.recording.attach();
	const SimLaneAttach clientAttach = client.recording.attach();
	REQUIRE(server.start(Role::Server, IoMode::HostServiced, &serverAttach));
	REQUIRE(client.start(Role::Client, IoMode::OwnThread, &clientAttach));
	CHECK(server.core.mode() == IoMode::HostServiced);
	SimLaneEndpoint& serverLanes = *server.recording.endpoint;
	CHECK_FALSE(serverLanes.hasHighResolutionWait());

	std::atomic<bool> runnerStop{false};
	std::atomic<uint64_t> runnerServices{0};
	std::atomic<uint64_t> runnerSawStopped{0};
	std::thread runner([&] {
		while (!runnerStop.load(std::memory_order_acquire))
		{
			const ognet::ServiceResult result = serverLanes.serviceUntil(network.nowSeconds() + kRunnerSliceSeconds);
			runnerServices.fetch_add(1, std::memory_order_relaxed);
			if (result.stopped)
			{
				runnerSawStopped.fetch_add(1, std::memory_order_relaxed);
				return;
			}
		}
	});
	const std::thread::id runnerThread = runner.get_id();
	const std::thread::id testThread = std::this_thread::get_id();

	const double giveUpAt = network.nowSeconds() + kGiveUpSeconds;
	client.core.connect(serverAddress(), giveUpAt);
	bool clientSent = false;
	bool serverSent = false;
	auto done = [&] {
		return server.count(Kind::Deliver) == 1 && client.count(Kind::Deliver) == 1 && server.recording.messagesOn(Lane::SimInput).size() == 1
			&& client.recording.messagesOn(Lane::SimState).size() == 1;
	};
	while (network.nowSeconds() < giveUpAt && !done())
	{
		server.core.tick(network.nowSeconds(), false);
		client.core.tick(network.nowSeconds(), client.enginePending());
		if (client.isUp() && !clientSent)
		{
			CHECK(client.core.send(client.core.serverConnection(), text("engine up")) == SendVerdict::Sent);
			client.recording.endpoint->send(client.core.serverConnection(), Lane::SimInput, text("sim up"));
			clientSent = true;
		}
		if (server.peerId() != ognet::kInvalidConnection && !serverSent)
		{
			CHECK(server.core.send(server.peerId(), text("engine down")) == SendVerdict::Sent);
			serverLanes.send(server.peerId(), Lane::SimState, text("sim down"));
			serverSent = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	runnerStop.store(true, std::memory_order_release);
	runner.join();
	REQUIRE(done());

	CHECK(u8(server.delivered()[0]) == u8(text("engine up")));
	CHECK(u8(client.delivered()[0]) == u8(text("engine down")));
	CHECK(client.recording.messagesOn(Lane::SimState)[0].bytes == u8(text("sim down")));
	CHECK(runnerThread != testThread);
	CHECK(runnerServices.load() > 0);
	CHECK(runnerSawStopped.load() == 0);

	const std::vector<SeamRecord> beforeStop = server.recording.records();
	REQUIRE(beforeStop.size() == 2);
	CHECK_FALSE(beforeStop[0].isMessage);
	CHECK(beforeStop[0].kind == ConnectionEventKind::Connected);
	CHECK(beforeStop[1].isMessage);
	CHECK(beforeStop[1].lane == Lane::SimInput);
	CHECK(beforeStop[1].bytes == u8(text("sim up")));
	for (const SeamRecord& record : beforeStop)
	{
		CHECK(record.thread == runnerThread);
	}

	server.core.stop(network.nowSeconds(), 0.2);
	CHECK(server.core.isStopped());
	const std::vector<SeamRecord> afterStop = server.recording.records();
	REQUIRE(afterStop.size() == 3);
	CHECK(afterStop[2].kind == ConnectionEventKind::Disconnected);
	CHECK(afterStop[2].reason == DisconnectReason::LocalClose);
	CHECK(afterStop[2].thread == testThread);
	client.core.stop(network.nowSeconds(), 0.2);
}

TEST_CASE("EngineLaneDriver.Death.ASecondThreadsServiceUntilOnAHostServicedEndpointAborts", "[.][OGSimDeath]")
{
#if defined(_MSC_VER)
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
	Rig rig(true);
	rig.startServer();
	SimLaneEndpoint& serverLanes = *rig.server->recording.endpoint;
	std::thread runner([&] { serverLanes.serviceUntil(rig.now()); });
	runner.join();
	std::printf("the runner serviced the endpoint; the test thread calls serviceUntil next\n");
	std::fflush(stdout);
	serverLanes.serviceUntil(rig.now());
	std::printf("serviceUntil returned on a second thread\n");
	std::fflush(stdout);
	FAIL("a second thread's serviceUntil on a host-serviced endpoint did NOT fire og-net's servicing-thread OGNET_CHECK");
}

TEST_CASE("EngineLaneDriver.TheSimEndpointReportsTheBackendsHighResolutionWaitThroughTheSwitch", "[OGNet][EngineLaneDriver]")
{
	const std::pair<bool, bool> withTimer = hostServicedHighResolutionWait<true>();
	CHECK(withTimer.first);
	CHECK(withTimer.second);
	const std::pair<bool, bool> withoutTimer = hostServicedHighResolutionWait<false>();
	CHECK_FALSE(withoutTimer.first);
	CHECK_FALSE(withoutTimer.second);
}

TEST_CASE("EngineLaneDriver.AnAttachedSimNetTransportReceivesEachSimLaneIntoItsInboxWithConnectedFirst", "[OGNet][EngineLaneDriver]")
{
	TransportAttach serverLanes;
	TransportAttach clientLanes;
	Rig rig(false);
	const SimLaneAttach serverAttach = serverLanes.attach();
	const SimLaneAttach clientAttach = clientLanes.attach();
	REQUIRE(rig.server->start(Role::Server, IoMode::HostServiced, &serverAttach));
	REQUIRE(rig.client->start(Role::Client, IoMode::HostServiced, &clientAttach));
	REQUIRE(serverLanes.transport != nullptr);
	REQUIRE(clientLanes.transport != nullptr);
	serverLanes.transport->markEndpointStarted();
	clientLanes.transport->markEndpointStarted();
	CHECK(serverLanes.transport->config().maxConnections == 16);
	CHECK(clientLanes.transport->config().maxConnections == 1);
	rig.client->core.connect(serverAddress(), rig.now() + 60.0);
	REQUIRE(rig.runUntil([&] { return rig.client->isUp() && rig.server->count(Kind::Accept) == 1; }, 5.0));
	rig.run(0.05);

	const ConnectionId atServer = rig.server->peerId();
	const ConnectionId atClient = rig.client->core.serverConnection();
	for (const Lane lane : kUpLanes)
	{
		clientLanes.endpoint->send(atClient, lane, laneMessage(lane, Role::Client));
	}
	serverLanes.transport->sendControl(atServer, laneMessage(Lane::Control, Role::Server));
	serverLanes.transport->broadcastClock(laneMessage(Lane::SimClock, Role::Server));
	serverLanes.transport->relay(kInvalidSimConnection, laneMessage(Lane::SimRelay, Role::Server));
	serverLanes.transport->sendState(atServer, laneMessage(Lane::SimState, Role::Server));
	rig.run(0.2);

	const std::vector<Drained> atServerSide = drainAll(*serverLanes.transport);
	REQUIRE(atServerSide.size() == 1 + kUpLanes.size());
	CHECK(atServerSide[0].kind == InboundKind::Connected);
	CHECK(atServerSide[0].conn == atServer);
	for (const Lane lane : kUpLanes)
	{
		const auto found = std::find_if(atServerSide.begin() + 1, atServerSide.end(),
			[&](const Drained& record) { return record.kind == InboundKind::Message && record.lane == static_cast<SimLane>(lane); });
		REQUIRE(found != atServerSide.end());
		CHECK(found->conn == atServer);
		CHECK(found->bytes == u8(laneMessage(lane, Role::Client)));
		CHECK(found->recvSeconds > 0.0);
	}

	const std::vector<Drained> atClientSide = drainAll(*clientLanes.transport);
	REQUIRE(atClientSide.size() == 1 + kDownLanes.size());
	CHECK(atClientSide[0].kind == InboundKind::Connected);
	CHECK(atClientSide[0].conn == atClient);
	for (const Lane lane : kDownLanes)
	{
		const auto found = std::find_if(atClientSide.begin() + 1, atClientSide.end(),
			[&](const Drained& record) { return record.kind == InboundKind::Message && record.lane == static_cast<SimLane>(lane); });
		REQUIRE(found != atClientSide.end());
		CHECK(found->conn == atClient);
		CHECK(found->bytes == u8(laneMessage(lane, Role::Server)));
	}
	CHECK(rig.server->count(Kind::Deliver) == 0);
	CHECK(rig.client->count(Kind::Deliver) == 0);
	CHECK(serverLanes.transport->stats().unslottedRefused == 0);
	CHECK(clientLanes.transport->stats().unslottedRefused == 0);
}

TEST_CASE("EngineLaneDriver.NoteDeliveredOnTheSimEndpointWritesTheSimLaneRxLineOfTheCoresWindow", "[OGNet][EngineLaneDriver]")
{
	Rig rig(true);
	rig.connectBoth();
	SimLaneEndpoint& atServer = *rig.server->recording.endpoint;
	for (int i = 0; i < 7; ++i)
	{
		atServer.noteDelivered(Lane::SimInput, rig.now() - 0.020, rig.now());
	}
	rig.run(ognet::NetWindowConfig{}.windowSeconds * 2.0 + 0.5);

	uint64_t samples = 0;
	std::size_t lines = 0;
	for (const TestHost::LogLine& line : rig.server->host.logLines())
	{
		if (line.text.rfind("[OGNet.Window.Rx] role=Server lane=SimInput ", 0) == 0)
		{
			++lines;
			samples += histogramTotal(fieldOf(line.text, "residence"));
			CHECK(fieldOf(line.text, "residenceP99Us") == "32000");
		}
	}
	std::string windowText;
	for (const TestHost::LogLine& line : rig.server->host.logLines())
	{
		if (line.text.rfind("[OGNet.Window", 0) == 0)
		{
			windowText += line.text + "\n";
		}
	}
	INFO(windowText);
	CHECK(lines == 1);
	CHECK(samples == 7);
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.PreConnectPacketsArriveFirstInOrderAndAFullBufferKeepsTheOldest", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	{
		Rig rig(withAttach);
		rig.startServer();
		rig.startClient();
		const std::vector<std::byte> first = numbered(0, 1024);
		const std::vector<std::byte> second = numbered(1, 7);
		const std::vector<std::byte> third = numbered(2, ognetunreal::kPreConnectSlotBytes);
		CHECK(rig.client->core.send(ognet::kInvalidConnection, first) == SendVerdict::Buffered);
		CHECK(rig.client->core.send(ognet::kInvalidConnection, second) == SendVerdict::Buffered);
		CHECK(rig.client->core.send(ognet::kInvalidConnection, third) == SendVerdict::Buffered);
		const std::vector<std::byte> fromUp = text("sent-in-serverConnectionUp");
		rig.client->onEvent = [&](const Event& event) {
			if (event.kind == Kind::Up)
			{
				CHECK(rig.client->core.send(event.id, fromUp) == SendVerdict::Sent);
			}
		};
		rig.client->core.connect(serverAddress(), rig.now() + 60.0);
		REQUIRE(rig.runUntil([&] { return rig.server->count(Kind::Deliver) >= 4; }, 5.0));
		const std::vector<std::vector<std::byte>> delivered = rig.server->delivered();
		REQUIRE(delivered.size() == 4);
		CHECK(u8(delivered[0]) == u8(first));
		CHECK(u8(delivered[1]) == u8(second));
		CHECK(u8(delivered[2]) == u8(third));
		CHECK(u8(delivered[3]) == u8(fromUp));
		CHECK(rig.server->indexOfFirst(Kind::Accept) < rig.server->indexOfFirst(Kind::Deliver));
		CHECK(rig.client->core.stats().preConnectBuffered == 3);
		CHECK(rig.server->recording.connectionEvents(ConnectionEventKind::Connected) == (withAttach ? 1u : 0u));
	}
	{
		Rig rig(withAttach);
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
		const std::vector<std::vector<std::byte>> delivered = rig.server->delivered();
		REQUIRE(delivered.size() == ognetunreal::kPreConnectSlots);
		for (uint32_t i = 0; i < ognetunreal::kPreConnectSlots; ++i)
		{
			CHECK(u8(delivered[i]) == u8(numbered(i, 100)));
		}
		CHECK(rig.client->core.stats().preConnectDropped == 1);
	}
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.ALastPacketSentWithEngineClosedReachesThePeerBeforeItsClose", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
	rig.connectBoth();
	const ConnectionId atServer = rig.server->peerId();
	rig.run(0.05);
	const std::vector<std::byte> last = text("the server's last packet");
	CHECK(rig.server->core.send(atServer, last) == SendVerdict::Sent);
	rig.server->core.engineClosed(atServer, rig.now());
	REQUIRE(rig.runUntil([&] { return rig.client->count(Kind::Closed) == 1; }, 5.0));
	CHECK(rig.client->of(Kind::Closed)[0].reason == DisconnectReason::ClosedByPeer);
	const std::vector<std::vector<std::byte>> delivered = rig.client->delivered();
	REQUIRE(delivered.size() == 1);
	CHECK(u8(delivered[0]) == u8(last));
	CHECK(rig.client->indexOfFirst(Kind::Deliver) < rig.client->indexOfFirst(Kind::Closed));
	CHECK(rig.server->core.stats().closesDrained == 1);
	CHECK(rig.client->recording.connectionEvents(ConnectionEventKind::Disconnected) == (withAttach ? 1u : 0u));
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.AFailedConnectRetriesBeforeTheDeadlineAndEndsOnceAfterIt", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	{
		Rig rig(withAttach);
		rig.startClient();
		const double connectedAt = rig.now();
		rig.client->core.connect(serverAddress(), connectedAt + 60.0);
		rig.runUntilTime(connectedAt + 9.9);
		CHECK(rig.client->core.stats().retries == 0);
		rig.runUntilTime(connectedAt + 10.1);
		CHECK(rig.client->core.stats().retries == 1);
		rig.runUntilTime(connectedAt + 15.0);
		rig.startServer();
		REQUIRE(rig.runUntil([&] { return rig.client->isUp() && rig.server->count(Kind::Accept) == 1; }, 5.0));
		CHECK(rig.client->core.stats().retries == 1);
		CHECK(rig.client->count(Kind::Ended) == 0);
		CHECK(rig.client->recording.connectionEvents(ConnectionEventKind::Disconnected) == (withAttach ? 1u : 0u));
	}
	{
		Rig rig(withAttach);
		rig.startClient();
		const double connectedAt = rig.now();
		rig.client->core.connect(serverAddress(), connectedAt + 5.0);
		rig.runUntilTime(connectedAt + 10.5);
		REQUIRE(rig.client->count(Kind::Ended) == 1);
		CHECK(rig.client->of(Kind::Ended)[0].reason == DisconnectReason::ConnectFailed);
		CHECK(rig.client->core.stats().retries == 0);
		rig.startServer();
		rig.runUntilTime(connectedAt + 25.0);
		CHECK(rig.client->count(Kind::Ended) == 1);
		CHECK_FALSE(rig.client->isUp());
		CHECK(rig.server->count(Kind::Accept) == 0);
	}
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.ARejectedConnectionIsClosedByThePeerAndDeliversNothing", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
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
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.AnIgnoredConnectionDropsItsDatagramsUntilItIsAccepted", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
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
	const std::vector<std::vector<std::byte>> delivered = rig.server->delivered();
	REQUIRE(delivered.size() == kSent - 2);
	for (uint32_t i = 0; i < kSent - 2; ++i)
	{
		CHECK(u8(delivered[i]) == u8(numbered(i + 2)));
	}
	CHECK(rig.server->core.stats().ignoredDatagrams == 2);
	CHECK(rig.server->core.stats().accepted == 1);
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.ADeliverThatStopsTheCoreRunsNoFurtherCallback", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
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
	CHECK(rig.client->count(Kind::Closed) == 1);
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.SixHundredDatagramsQueuedBetweenTwoTicksArriveInOrderInBatchesOf256", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
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
	const std::vector<std::vector<std::byte>> delivered = rig.server->delivered();
	REQUIRE(delivered.size() == kSent);
	for (uint32_t i = 0; i < kSent; ++i)
	{
		CHECK(indexOf(delivered[i]) == i);
	}
	const ognetunreal::DriverStats after = rig.server->core.stats();
	CHECK(after.batches - before.batches >= 3);
	CHECK(after.maxBatch == ognetunreal::kBatchEvents);
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.AClientWhoseEmulatorDropsEveryOutboundDatagramNeverReachesTheServer", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	Rig rig(withAttach);
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
	CHECK(rig.server->recording.records().empty());
}

TEST_CASE("EngineLaneDriver.WithAndWithoutAnAttach.OnItsOwnThreadEachSideSendsFiveHundredInOrderAndStopDrainsTheLastPacket", "[OGNet][EngineLaneDriver]")
{
	const bool withAttach = GENERATE(false, true);
	constexpr uint32_t kEachWay = 500;
	constexpr double kGiveUpSeconds = 30.0;
	VirtualNetwork network(NetworkMode::Steady);
	Side server(network, kServerHostIndex, 300);
	Side client(network, kClientHostIndex, 400);
	const SimLaneAttach serverAttach = server.recording.attach();
	const SimLaneAttach clientAttach = client.recording.attach();
	REQUIRE(server.start(Role::Server, IoMode::OwnThread, withAttach ? &serverAttach : nullptr));
	REQUIRE(client.start(Role::Client, IoMode::OwnThread, withAttach ? &clientAttach : nullptr));
	const double giveUpAt = network.nowSeconds() + kGiveUpSeconds;
	client.core.connect(serverAddress(), giveUpAt);

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
	while (network.nowSeconds() < giveUpAt && (client.count(Kind::Deliver) < kEachWay || server.count(Kind::Deliver) < kEachWay - 1))
	{
		frame();
	}
	REQUIRE(client.count(Kind::Deliver) == kEachWay);
	REQUIRE(server.count(Kind::Deliver) == kEachWay - 1);
	CHECK(client.core.send(client.core.serverConnection(), numbered(kEachWay - 1, 64)) == SendVerdict::Sent);
	client.core.stop(network.nowSeconds(), 0.2);
	clientStopped = true;
	while (network.nowSeconds() < giveUpAt && server.count(Kind::Closed) == 0)
	{
		frame();
	}
	REQUIRE(server.count(Kind::Closed) == 1);
	const std::vector<std::vector<std::byte>> atServer = server.delivered();
	REQUIRE(atServer.size() == kEachWay);
	for (uint32_t i = 0; i < kEachWay; ++i)
	{
		CHECK(u8(atServer[i]) == u8(numbered(i, 64)));
	}
	CHECK(server.indexOfFirst(Kind::Closed) == server.events.size() - 1);
	CHECK(server.of(Kind::Closed)[0].reason == DisconnectReason::ClosedByPeer);
	CHECK(server.core.stats().sendsRefused == 0);
	CHECK(client.core.stats().sendsRefused == 0);
	CHECK(server.recording.connectionEvents(ConnectionEventKind::Disconnected) == (withAttach ? 1u : 0u));
	server.core.stop(network.nowSeconds(), 0.2);
}
