// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "SimNetTestRig.h"

#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simNetImmediate
{
enum class WriteKind : std::uint8_t
{
	RelayInput,
	CorrectionState,
	Estimator,
	TierDelay,
	Floor,
};

inline constexpr std::array<WriteKind, 5> kWriteKinds{
	WriteKind::RelayInput, WriteKind::CorrectionState, WriteKind::Estimator, WriteKind::TierDelay, WriteKind::Floor};

inline const char* writeKindName(WriteKind kind)
{
	switch (kind)
	{
	case WriteKind::RelayInput:
		return "relayStore";
	case WriteKind::CorrectionState:
		return "correctionCache";
	case WriteKind::Estimator:
		return "estimator";
	case WriteKind::TierDelay:
		return "tierDelay";
	case WriteKind::Floor:
		return "floor";
	}
	return "?";
}

struct WriteRecord
{
	WriteKind kind = WriteKind::RelayInput;
	std::thread::id thread;
};

class WriteLog
{
public:
	void note(WriteKind kind)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (m_enabled)
			m_records.push_back(WriteRecord{kind, std::this_thread::get_id()});
	}

	void enable(bool on)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_enabled = on;
		m_records.clear();
	}

	std::vector<WriteRecord> take()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return std::exchange(m_records, {});
	}

private:
	std::mutex m_mutex;
	bool m_enabled = false;
	std::vector<WriteRecord> m_records;
};

inline WriteLog& writeLog()
{
	static WriteLog log;
	return log;
}

struct HookPosition
{
	std::int32_t x = 0;

	std::int32_t getX() const { return x; }
	void setX(std::int32_t value)
	{
		x = value;
		writeLog().note(WriteKind::CorrectionState);
	}
};

struct HookInput
{
	std::int32_t move = 0;
	std::uint8_t flags = 0u;

	std::int32_t getMove() const { return move; }
	void setMove(std::int32_t value)
	{
		move = value;
		writeLog().note(WriteKind::RelayInput);
	}
	std::uint8_t getFlags() const { return flags; }
	void setFlags(std::uint8_t value) { flags = value; }
};
} // namespace simNetImmediate

template <>
struct SerializableFields<simNetImmediate::HookPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_FIELD(simNetImmediate::HookPosition, getX, setX)); }
};

template <>
struct SerializableFields<simNetImmediate::HookInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(
			SIM_FIELD(simNetImmediate::HookInput, getMove, setMove), SIM_FIELD(simNetImmediate::HookInput, getFlags, setFlags));
	}
};

namespace simNetImmediate
{
using namespace simNetEquivalence;

using HookState = SimulationComposite<HookPosition>;

class HookAllState
{
public:
	const HookState& getState() const { return m_state; }
	HookState& editState() { return m_state; }

private:
	HookState m_state;
};

struct HookSim
{
	using StateType = HookState;
	using InputType = HookInput;

	HookAllState m_allState;
	HookAllState m_vizState;

	const HookAllState& getAllState() const { return m_allState; }
	HookAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const HookAllState& getVizState() const { return m_vizState; }
};

struct HookGame
{
	using Simulatables = SimulatableList<HookSim>;
};

using HookChannel = ogsimnet::SessionChannel<HookSim, Transport::ChannelTransport>;
} // namespace simNetImmediate

template <>
struct SimulatableOwnerTraits<simNetImmediate::HookSim>
{
	using PredictionOwnerType = simNetImmediate::HookChannel;
	using AuthorityOwnerType = simNetImmediate::HookChannel;
};

namespace simNetImmediate
{
constexpr SimNetConfig kHeldSends{false};
constexpr SimNetConfig kImmediate{true};
constexpr int kThreadRigFrames = 240;
constexpr int kTierBeforeBindFrame = 5;
constexpr int kBindFrame = 10;
constexpr int kTierAfterBindFrame = 20;
constexpr std::uint8_t kPublishedFloor = 3u;
constexpr std::size_t kWorkersPerChain = 2u;

inline HookInput hookInputFor(std::uint8_t simId, std::uint32_t tick)
{
	const EqInput input = inputFor(simId, tick);
	HookInput out;
	out.move = input.move;
	out.flags = input.flags;
	return out;
}

inline const TimeConfig& estimatorConfig()
{
	static const TimeConfig config;
	return config;
}

inline const TimeConfig& sendConfig()
{
	static const TimeConfig config = [] {
		TimeConfig out;
		out.redundancyDepthTicks = static_cast<std::int32_t>(kRedundancyDepthTicks);
		out.redundancyFrames = kRedundancyFrames;
		return out;
	}();
	return config;
}

class TaskChain
{
public:
	explicit TaskChain(std::size_t workers)
	{
		for (std::size_t i = 0u; i < workers; ++i)
			m_threads.emplace_back([this] { run(); });
	}

	TaskChain(const TaskChain&) = delete;
	TaskChain& operator=(const TaskChain&) = delete;

	~TaskChain()
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		for (std::thread& thread : m_threads)
			thread.join();
	}

	void post(std::function<void()> task)
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_tasks.push_back(std::move(task));
		}
		m_wake.notify_all();
	}

	void waitIdle()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_wake.wait(lock, [this] { return m_tasks.empty() && !m_running; });
	}

	std::map<std::thread::id, std::size_t> tasksByThread()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_ranOn;
	}

	std::set<std::thread::id> threadIds() const
	{
		std::set<std::thread::id> ids;
		for (const std::thread& thread : m_threads)
			ids.insert(thread.get_id());
		return ids;
	}

private:
	void run()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		for (;;)
		{
			m_wake.wait(lock, [this] { return m_stop || (!m_tasks.empty() && !m_running); });
			if (m_stop)
				return;
			std::function<void()> task = std::move(m_tasks.front());
			m_tasks.pop_front();
			m_running = true;
			++m_ranOn[std::this_thread::get_id()];
			lock.unlock();
			task();
			lock.lock();
			m_running = false;
			m_wake.notify_all();
		}
	}

	std::mutex m_mutex;
	std::condition_variable m_wake;
	std::deque<std::function<void()>> m_tasks;
	std::map<std::thread::id, std::size_t> m_ranOn;
	bool m_running = false;
	bool m_stop = false;
	std::vector<std::thread> m_threads;
};

struct HookStack
{
	SimulationObjectStorage<HookSim> storage;
	SimulationReconciliation<HookSim> reconciliation{storage};
	SimulationInputResolution<HookSim> resolution{storage, reconciliation};
	SimulationNetSync<HookSim> netSync{storage, reconciliation, resolution};

	HookStack() { resolution.template setNeutralInput<HookSim>(HookInput{}); }

	HookStack(const HookStack&) = delete;
	HookStack& operator=(const HookStack&) = delete;

	void addAuthority(std::uint8_t simId, HookChannel& channel)
	{
		registerSimulatable<HookSim>(storage, reconciliation, resolution, netSync, simId, HookSim{}, channel, channel);
	}

	void addPrediction(std::uint8_t simId, HookChannel& channel, bool local)
	{
		std::function<HookInput(const SimulationTimeStep&, const LocalInputCache<HookInput>&)> provider;
		if (local)
		{
			provider = [simId](const SimulationTimeStep& step, const LocalInputCache<HookInput>&) {
				return hookInputFor(simId, step.getTick());
			};
		}
		registerSimulatable<HookSim>(storage, reconciliation, resolution, netSync, simId, HookSim{}, channel, std::move(provider));
	}

	void applyInputs(const ResolvedInputs<HookSim>& inputs)
	{
		const auto& map = std::get<std::unordered_map<unsigned int, HookInput>>(inputs);
		std::vector<unsigned int> ids;
		storage.forEachSimulatable([&](unsigned int id, auto&) { ids.push_back(id); });
		for (const unsigned int id : ids)
		{
			const auto found = map.find(id);
			const std::int32_t move = found == map.end() ? 0 : found->second.move;
			storage.template get<HookSim>(id).editAllState().editState().template edit<HookPosition>().x += move;
		}
	}

	void stepClient(std::uint32_t tick)
	{
		const SimulationTimeStep step(tick, false);
		reconciliation.allocateFrontierSlotsAll(step);
		applyInputs(resolution.collectInputAll(step));
		reconciliation.postPredictionAll(step);
	}

	void stepServer(std::uint32_t tick) { applyInputs(resolution.collectInputAll(SimulationTimeStep(tick, false))); }
};

struct HookClientHost
{
	NetworkTimeEstimator estimator{estimatorConfig(), nullptr};
	std::atomic<double> now{0.0};
	std::atomic<double> publishedRttSeconds{0.0};
	std::function<void(std::uint8_t)> added;
	std::size_t timings = 0u;
	std::size_t floorReplays = 0u;
	std::size_t tierReplays = 0u;
	std::size_t tierArrivals = 0u;

	double nowSeconds() { return now.load(std::memory_order_relaxed); }
	double serverRttSeconds() { return publishedRttSeconds.load(std::memory_order_relaxed); }
	void onTimingInfo(std::uint32_t tick, double rttSeconds)
	{
		writeLog().note(WriteKind::Estimator);
		estimator.updateRTT(rttSeconds);
		estimator.recordAuthorityTick(tick);
		++timings;
	}
	void onRelayFloor(std::uint8_t, bool replayed)
	{
		writeLog().note(WriteKind::Floor);
		floorReplays += replayed ? 1u : 0u;
	}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool replayed)
	{
		writeLog().note(WriteKind::TierDelay);
		tierReplays += replayed ? 1u : 0u;
		tierArrivals += replayed ? 0u : 1u;
	}
	void onRosterAdded(std::uint8_t simId)
	{
		if (added)
			added(simId);
	}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char*) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

static_assert(ogsimnet::SimNetClientHost<HookClientHost>);

using HookClientSession = ogsimnet::SimNetClient<HookGame, HookClientHost, Transport>;
using HookServerSession = ogsimnet::SimNetServer<HookGame, ServerHost, Transport>;

struct HookClient
{
	std::unique_ptr<NetPeer> peer;
	HookClientHost host;
	std::unique_ptr<HookClientSession> session;
	HookStack stack;
	std::map<std::uint8_t, HookChannel*> channels;
	std::uint32_t tick = 0u;
	bool bindFailed = false;
	TaskChain chain{kWorkersPerChain};
};

struct ThreadRig
{
	ThreadRig()
	{
		network.setNow(kStartSeconds);
		serverPeer = std::make_unique<NetPeer>(network, kServerHostIndex, 100u, kHeldSends);
		serverPeer->start(ognet::Role::Server);
		server = std::make_unique<HookServerSession>(serverHost, *serverPeer->transport, timeConfig, std::size_t{1u << 20});
		server->publishFloor(kPublishedFloor);
		for (std::size_t index = 0u; index < kClientCount; ++index)
			join(index);
		serverChain.post([this] {
			for (const CharacterSpec& spec : kCharacters)
			{
				HookChannel* const channel =
					server->addCharacter<HookSim>(spec.simId, spec.slot, serverIds[spec.client], spec.localIndex, spec.name);
				if (channel == nullptr)
				{
					addFailed = true;
					continue;
				}
				serverStack.addAuthority(spec.simId, *channel);
			}
		});
		serverChain.waitIdle();
		setupUntil([&] {
			for (const auto& client : clients)
			{
				if (client->channels.size() != kCharacters.size())
					return false;
			}
			return true;
		});
		serverTick = kFirstServerTick;
		for (auto& client : clients)
			client->tick = kFirstServerTick + kClientLeadTicks;
	}

	void join(std::size_t index)
	{
		auto client = std::make_unique<HookClient>();
		client->peer = std::make_unique<NetPeer>(network, static_cast<std::uint8_t>(kFirstClientHostIndex + index), 200u + index, kImmediate);
		client->peer->start(ognet::Role::Client);
		client->session = std::make_unique<HookClientSession>(client->host, *client->peer->transport);
		HookClient& joined = *client;
		client->host.added = [&joined](std::uint8_t simId) {
			HookChannel* const channel = joined.session->channel(simId);
			const auto* entry = joined.session->roster().entryFor(simId);
			if (channel == nullptr || entry == nullptr)
			{
				joined.bindFailed = true;
				return;
			}
			joined.stack.addPrediction(simId, *channel, entry->isLocal());
			joined.channels[simId] = channel;
		};
		client->peer->endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		clients.push_back(std::move(client));
		setupUntil([&] {
			return joined.session->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == index + 1u
				&& joined.session->roster().attached();
		});
		if (server->attachedConnections().size() != index + 1u)
		{
			setupDone = false;
			return;
		}
		serverIds[index] = server->attachedConnections().back();
		serverHost.rtt[serverIds[index]] = kClientRttMs[index];
	}

	template <class DoneT>
	void setupUntil(DoneT done)
	{
		for (int i = 0; i < kMaxSetupSteps && !done(); ++i)
		{
			network.advance(kSetupStepSeconds);
			const double now = network.nowSeconds();
			serverHost.now = now;
			serverPeer->service(now);
			serverChain.post([this] {
				server->dispatchFrame();
				server->flushFrame();
			});
			serverChain.waitIdle();
			serverPeer->service(now);
			for (auto& client : clients)
			{
				client->host.now.store(now, std::memory_order_relaxed);
				client->peer->service(now);
				HookClient& c = *client;
				c.chain.post([&c] {
					c.session->drain();
					c.session->flushFrame();
				});
				c.chain.waitIdle();
				client->peer->service(now);
			}
		}
		setupDone = setupDone && done();
	}

	void frame(int index)
	{
		const std::uint32_t ticks = ticksInFrame(index);
		network.advance(static_cast<double>(ticks) * kTickSeconds);
		const double now = network.nowSeconds();

		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			clients[c]->host.now.store(now, std::memory_order_relaxed);
			clients[c]->host.publishedRttSeconds.store(kClientRttMs[c] / 1000.0, std::memory_order_relaxed);
		}

		serverHost.now = now;
		serverHost.tick = static_cast<std::int32_t>(serverTick);
		serverPeer->service(now);
		serverChain.post([this, ticks, index] {
			server->dispatchFrame();
			for (std::uint32_t step = 0u; step < ticks; ++step)
			{
				serverStack.netSync.setAuthorityGuardContext(serverTick, kRollbackWindowTicks);
				server->releaseForStep(static_cast<std::int32_t>(serverTick), 1);
				serverStack.stepServer(serverTick);
				++serverTick;
			}
			server->afterBatch(static_cast<std::int32_t>(serverTick));
			serverStack.netSync.sendCorrectionAll(SimulationTimeStep(serverTick - 1u, false), kCorrectionRotationK);
			server->publishClock(serverTick - 1u);
			if (index == kTierBeforeBindFrame)
				server->publishTier(serverIds[0], 2u);
			if (index == kTierAfterBindFrame)
				server->publishTier(serverIds[1], 1u);
			server->flushFrame();
		});
		serverChain.waitIdle();
		serverPeer->service(now);

		if (index == kBindFrame)
		{
			for (auto& client : clients)
				client->session->bindListener();
		}

		for (auto& client : clients)
		{
			HookClient& c = *client;
			c.peer->service(now);
			for (std::uint32_t step = 0u; step < ticks; ++step)
			{
				c.chain.post([&c] {
					c.session->drain();
					c.stack.stepClient(c.tick++);
				});
			}
			c.chain.post([&c] {
				c.session->afterCaptureTick_Step(c.stack.netSync, c.tick - 1u, sendConfig());
				c.session->flushFrame();
			});
		}
		for (auto& client : clients)
		{
			client->chain.waitIdle();
			client->peer->service(now);
		}
	}

	TimeConfig timeConfig;
	VirtualNetwork network;
	ServerHost serverHost;
	std::unique_ptr<NetPeer> serverPeer;
	std::unique_ptr<HookServerSession> server;
	HookStack serverStack;
	TaskChain serverChain{1u};
	std::vector<std::unique_ptr<HookClient>> clients;
	std::array<SimConnectionId, kClientCount> serverIds{};
	std::uint32_t serverTick = 0u;
	bool addFailed = false;
	bool setupDone = true;
};

class ConfigTee : public ClientTee<Transport>
{
public:
	explicit ConfigTee(Transport& inner) : ClientTee<Transport>(inner), m_configured(inner) {}

	const SimNetConfig& config() const { return m_configured.config(); }
	void beginInputSends() { m_configured.beginInputSends(); }

private:
	Transport& m_configured;
};

static_assert(!ogsimnet::SimNetConfiguredTransport<ClientTee<Transport>>);
static_assert(ogsimnet::SimNetConfiguredTransport<ConfigTee>);
static_assert(ogsimnet::SimNetConfiguredTransport<Transport>);

using ImmediateSession = ogsimnet::SimNetClient<EqGame, ClientHost, ConfigTee>;

struct ImmediateClient
{
	std::unique_ptr<NetPeer> peer;
	std::unique_ptr<ConfigTee> tee;
	ClientHost host;
	std::unique_ptr<ImmediateSession> session;
	SimStack stack;
	std::map<std::uint8_t, EqChannel*> channels;
	std::uint32_t tick = 0u;
};

struct ImmediateRig
{
	ImmediateRig()
	{
		network.setNow(kStartSeconds);
		serverPeer = std::make_unique<NetPeer>(network, kServerHostIndex, 100u, kHeldSends);
		serverPeer->start(ognet::Role::Server);
		serverTee = std::make_unique<ServerTee<Transport>>(*serverPeer->transport);
		server = std::make_unique<ServerSession>(serverHost, *serverTee, timeConfig, std::size_t{1u << 20});
		for (std::size_t index = 0u; index < kClientCount; ++index)
			join(index);
		for (const CharacterSpec& spec : kCharacters)
		{
			EqChannel* const channel = server->addCharacter<EqSim>(spec.simId, spec.slot, serverIds[spec.client], spec.localIndex, spec.name);
			REQUIRE(channel != nullptr);
			serverStack.addAuthority(spec.simId, *channel);
		}
		REQUIRE(setupUntil([&] {
			for (const auto& client : clients)
			{
				if (client->channels.size() != kCharacters.size())
					return false;
			}
			return true;
		}));
		serverStack.takeTrace();
		serverHost.lines.clear();
		for (auto& client : clients)
		{
			client->stack.takeTrace();
			client->host.lines.clear();
			client->host.clock.clear();
		}
		serverTick = kFirstServerTick;
		for (auto& client : clients)
			client->tick = kFirstServerTick + kClientLeadTicks;
	}

	void join(std::size_t index)
	{
		auto client = std::make_unique<ImmediateClient>();
		client->peer = std::make_unique<NetPeer>(network, static_cast<std::uint8_t>(kFirstClientHostIndex + index), 200u + index, kImmediate);
		client->peer->start(ognet::Role::Client);
		client->tee = std::make_unique<ConfigTee>(*client->peer->transport);
		client->session = std::make_unique<ImmediateSession>(client->host, *client->tee);
		REQUIRE(client->session->immediate());
		ImmediateClient& joined = *client;
		client->host.added = [&joined](std::uint8_t simId) {
			EqChannel* const channel = joined.session->channel(simId);
			REQUIRE(channel != nullptr);
			const auto* entry = joined.session->roster().entryFor(simId);
			REQUIRE(entry != nullptr);
			joined.stack.addPrediction(simId, *channel, entry->isLocal());
			joined.channels[simId] = channel;
		};
		client->peer->endpoint.connect(ognet::test::virtualHostAddress(kServerHostIndex, kServerPort));
		clients.push_back(std::move(client));
		REQUIRE(setupUntil([&] {
			return joined.session->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == index + 1u
				&& joined.session->roster().attached();
		}));
		serverIds[index] = server->attachedConnections().back();
		serverHost.rtt[serverIds[index]] = kClientRttMs[index];
	}

	template <class DoneT>
	bool setupUntil(DoneT done)
	{
		for (int i = 0; i < kMaxSetupSteps; ++i)
		{
			if (done())
				return true;
			network.advance(kSetupStepSeconds);
			const double now = network.nowSeconds();
			serverHost.now = now;
			serverPeer->service(now);
			server->dispatchFrame();
			server->flushFrame();
			serverPeer->service(now);
			for (auto& client : clients)
			{
				client->host.now = now;
				client->peer->service(now);
				client->session->drain();
				client->session->flushFrame();
				client->peer->service(now);
			}
		}
		return done();
	}

	FrameRecord frame(int index)
	{
		FrameRecord record;
		record.ticks = ticksInFrame(index);
		network.advance(static_cast<double>(record.ticks) * kTickSeconds);
		const double now = network.nowSeconds();

		for (auto& client : clients)
		{
			client->host.now = now;
			client->peer->service(now);
		}

		serverHost.now = now;
		serverHost.tick = static_cast<std::int32_t>(serverTick);
		serverTee->frame = &record.server;
		serverPeer->service(now);
		server->dispatchFrame();
		serverTee->frame = nullptr;
		for (std::uint32_t step = 0u; step < record.ticks; ++step)
		{
			serverStack.netSync.setAuthorityGuardContext(serverTick, kRollbackWindowTicks);
			server->releaseForStep(static_cast<std::int32_t>(serverTick), 1);
			serverStack.stepServer(serverTick, record.server.consumed);
			++serverTick;
		}
		server->afterBatch(static_cast<std::int32_t>(serverTick));
		serverStack.netSync.sendCorrectionAll(SimulationTimeStep(serverTick - 1u, false), kCorrectionRotationK);
		server->publishClock(serverTick - 1u);
		serverPeer->service(now);
		server->flushFrame();
		serverPeer->service(now);

		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			ImmediateClient& client = *clients[c];
			client.peer->service(now);
			client.tee->frame = &record.clients[c];
			for (std::uint32_t step = 0u; step < record.ticks; ++step)
			{
				client.session->drain();
				client.stack.stepClient(client.tick++, record.clients[c].inputs);
			}
			client.tee->frame = nullptr;
			client.session->afterCaptureTick_Step(client.stack.netSync, client.tick - 1u, sendConfig());
			client.session->flushFrame();
			client.peer->service(now);
		}

		record.server.decisions = takeLines(serverHost.lines);
		record.server.trace = serverStack.takeTrace();
		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			ImmediateClient& client = *clients[c];
			ClientFrame& out = record.clients[c];
			out.clock = std::exchange(client.host.clock, {});
			out.lines = takeLines(client.host.lines);
			out.trace = sorted(client.stack.takeTrace());
			snapshotChannels(client.channels, out);
		}
		return record;
	}

	TimeConfig timeConfig;
	VirtualNetwork network;
	ServerHost serverHost;
	std::unique_ptr<NetPeer> serverPeer;
	std::unique_ptr<ServerTee<Transport>> serverTee;
	std::unique_ptr<ServerSession> server;
	SimStack serverStack;
	std::vector<std::unique_ptr<ImmediateClient>> clients;
	std::array<SimConnectionId, kClientCount> serverIds{};
	std::uint32_t serverTick = 0u;
};

struct Delivered
{
	int frame = 0;
	std::string value;
};

using DeliveryStreams = std::map<std::string, std::vector<Delivered>>;

inline std::string itemValue(const StreamItem& item)
{
	std::string text = std::to_string(item.simId) + ":";
	for (const std::uint8_t b : item.bytes)
		text += std::to_string(b) + ",";
	return text;
}

inline void collectDeliveries(int frame, const FrameRecord& record, DeliveryStreams& out)
{
	for (std::size_t c = 0u; c < kClientCount; ++c)
	{
		const std::string prefix = "client" + std::to_string(c) + ".";
		const ClientFrame& client = record.clients[c];
		for (const StreamItem& item : client.corrections)
			out[prefix + "corrections"].push_back(Delivered{frame, itemValue(item)});
		for (const StreamItem& item : client.rings)
			out[prefix + "rings"].push_back(Delivered{frame, itemValue(item)});
		for (const std::uint32_t tick : client.clock)
			out[prefix + "clock"].push_back(Delivered{frame, std::to_string(tick)});
		for (const std::string& line : client.lines)
			out[prefix + "lines"].push_back(Delivered{frame, line});
	}
}

struct OneFrameEarlier
{
	std::size_t items = 0u;
	std::size_t earlier = 0u;
	std::size_t sameFrame = 0u;
	std::size_t outside = 0u;
	std::size_t valueMismatches = 0u;
	std::string firstProblem;
};

inline OneFrameEarlier compareDeliveries(const std::vector<Delivered>& immediate, const std::vector<Delivered>& locked)
{
	OneFrameEarlier result;
	const std::size_t count = std::min(immediate.size(), locked.size());
	for (std::size_t i = 0u; i < count; ++i)
	{
		++result.items;
		if (immediate[i].value != locked[i].value)
		{
			++result.valueMismatches;
			if (result.firstProblem.empty())
				result.firstProblem = "item " + std::to_string(i) + " value differs";
			continue;
		}
		const int lead = locked[i].frame - immediate[i].frame;
		if (lead == 1)
			++result.earlier;
		else if (lead == 0)
			++result.sameFrame;
		else
		{
			++result.outside;
			if (result.firstProblem.empty())
				result.firstProblem = "item " + std::to_string(i) + " immediate frame " + std::to_string(immediate[i].frame)
					+ " frame-locked frame " + std::to_string(locked[i].frame);
		}
	}
	return result;
}

struct ListenerHost
{
	std::vector<std::string> calls;

	double nowSeconds() { return 0.0; }
	double serverRttSeconds() { return kServerRttSeconds; }
	void onTimingInfo(std::uint32_t, double) {}
	void onRelayFloor(std::uint8_t floor, bool replayed)
	{
		calls.push_back("floor " + std::to_string(floor) + (replayed ? " replayed" : " arrived"));
	}
	void onConnectionTier(std::uint8_t oldTier, std::uint8_t newTier, bool replayed)
	{
		calls.push_back("tier " + std::to_string(oldTier) + "->" + std::to_string(newTier) + (replayed ? " replayed" : " arrived"));
	}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char*) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

static_assert(ogsimnet::SimNetClientHost<ListenerHost>);

using ListenerSession = ogsimnet::SimNetClient<EqGame, ListenerHost, Transport>;

constexpr SimConnectionId kScriptServerConn = 7u;

inline void pushFloor(Transport& transport, std::uint8_t floor)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = simEnvelope::writeRelayFloor(out, floor);
	REQUIRE(transport.inbox().pushMessage(kScriptServerConn, SimLane::Control, 0.0, std::span<const std::byte>(out.data(), size)));
}

inline void pushTier(Transport& transport, std::uint8_t tier)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = simEnvelope::writeConnectionTier(out, tier);
	REQUIRE(transport.inbox().pushMessage(kScriptServerConn, SimLane::Control, 0.0, std::span<const std::byte>(out.data(), size)));
}

TEST_CASE("SimNetClientImmediate.EveryRelayStoreCorrectionCacheAndEstimatorWriteRunsOnAStepTaskNeverOnTheGameThread", "[NetTransport]")
{
	writeLog().enable(true);
	std::vector<WriteRecord> records;
	std::set<std::thread::id> clientThreads;
	std::set<std::thread::id> serverThreads;
	std::size_t clientTasks = 0u;
	std::vector<std::size_t> floorReplays;
	std::vector<std::size_t> tierReplays;
	std::vector<std::size_t> tierArrivals;
	std::vector<std::size_t> timings;
	std::vector<unsigned int> authorityTicks;
	bool addFailed = false;
	bool bindFailed = false;
	bool setupDone = false;
	{
		ThreadRig rig;
		for (int frame = 0; frame < kThreadRigFrames; ++frame)
			rig.frame(frame);
		records = writeLog().take();
		addFailed = rig.addFailed;
		setupDone = rig.setupDone;
		serverThreads = rig.serverChain.threadIds();
		for (auto& client : rig.clients)
		{
			bindFailed = bindFailed || client->bindFailed;
			for (const std::thread::id id : client->chain.threadIds())
				clientThreads.insert(id);
			for (const auto& [thread, ran] : client->chain.tasksByThread())
				clientTasks += ran;
			floorReplays.push_back(client->host.floorReplays);
			tierReplays.push_back(client->host.tierReplays);
			tierArrivals.push_back(client->host.tierArrivals);
			timings.push_back(client->host.timings);
			authorityTicks.push_back(client->host.estimator.getLastAuthorityTick());
		}
	}
	writeLog().enable(false);

	REQUIRE(setupDone);
	REQUIRE_FALSE(addFailed);
	REQUIRE_FALSE(bindFailed);

	const std::thread::id gameThread = std::this_thread::get_id();
	std::map<WriteKind, std::array<std::size_t, 3>> counts;
	for (const WriteRecord& record : records)
	{
		std::array<std::size_t, 3>& row = counts[record.kind];
		if (record.thread == gameThread)
			++row[0];
		else if (clientThreads.count(record.thread) != 0u)
			++row[1];
		else if (serverThreads.count(record.thread) != 0u)
			++row[2];
	}
	for (const WriteKind kind : kWriteKinds)
	{
		const std::array<std::size_t, 3>& row = counts[kind];
		std::printf("[SimNetClientImmediate.Threads] write=%s gameThread=%zu clientStepTask=%zu serverTask=%zu\n", writeKindName(kind),
			row[0], row[1], row[2]);
		INFO("write " << writeKindName(kind));
		CHECK(row[0] == 0u);
		CHECK(row[1] > 0u);
	}
	CHECK(counts[WriteKind::CorrectionState][2] == 0u);
	CHECK(counts[WriteKind::Estimator][2] == 0u);
	std::size_t unclassified = records.size();
	for (const auto& [kind, row] : counts)
		unclassified -= row[0] + row[1] + row[2];
	CHECK(unclassified == 0u);
	CHECK(clientThreads.count(gameThread) == 0u);
	CHECK(clientTasks > static_cast<std::size_t>(kThreadRigFrames) * kClientCount);

	for (std::size_t c = 0u; c < kClientCount; ++c)
	{
		INFO("client " << c);
		CHECK(floorReplays[c] == 1u);
		CHECK(timings[c] >= static_cast<std::size_t>(kThreadRigFrames - 1));
		CHECK(authorityTicks[c] > kFirstServerTick);
	}
	CHECK(tierReplays[0] == 1u);
	CHECK(tierArrivals[0] == 0u);
	CHECK(tierReplays[1] == 1u);   // og-input-latency P6-2: its first tier, 0, is sent at the server's first sample, before the bind
	CHECK(tierArrivals[1] == 1u);
	std::printf("[SimNetClientImmediate.Threads] records=%zu clientTasks=%zu clientWorkers=%zu\n", records.size(), clientTasks,
		clientThreads.size());
}

TEST_CASE("SimNetClientImmediate.TheLosslessScriptDeliversTheSameValuesEachUpToOneFrameEarlier", "[NetTransport]")
{
	TransportRig locked(kHeldSends);
	ImmediateRig immediate;
	REQUIRE(locked.serverIds == immediate.serverIds);

	DeliveryStreams lockedStreams;
	DeliveryStreams immediateStreams;
	FrameDiff serverDiff;
	for (int frame = 0; frame <= kFrames; ++frame)
	{
		const FrameRecord a = locked.frame(frame);
		collectDeliveries(frame, a, lockedStreams);
		if (frame == kFrames)
			break;
		const FrameRecord b = immediate.frame(frame);
		collectDeliveries(frame, b, immediateStreams);
		FrameRecord aServer;
		FrameRecord bServer;
		aServer.server = a.server;
		bServer.server = b.server;
		compareFrames(frame, bServer, aServer, serverDiff);
	}

	{
		INFO(serverDiff.describe());
		CHECK(serverDiff.empty());
	}

	REQUIRE(lockedStreams.size() == immediateStreams.size());
	std::size_t earlier = 0u;
	for (const auto& [stream, lockedItems] : lockedStreams)
	{
		const auto found = immediateStreams.find(stream);
		REQUIRE(found != immediateStreams.end());
		const std::vector<Delivered>& immediateItems = found->second;
		const OneFrameEarlier result = compareDeliveries(immediateItems, lockedItems);
		std::printf("[SimNetClientImmediate.Lossless] stream=%s immediate=%zu frameLocked=%zu compared=%zu oneFrameEarlier=%zu sameFrame=%zu "
					"outside=%zu valueMismatches=%zu\n",
			stream.c_str(), immediateItems.size(), lockedItems.size(), result.items, result.earlier, result.sameFrame, result.outside,
			result.valueMismatches);
		INFO(stream << ": " << result.firstProblem);
		CHECK(immediateItems.size() == lockedItems.size());
		CHECK(result.valueMismatches == 0u);
		CHECK(result.outside == 0u);
		CHECK(result.items > static_cast<std::size_t>(kFrames) / 5u);
		earlier += result.earlier;
	}
	CHECK(earlier > 0u);
}

TEST_CASE("SimNetClientImmediate.TheSessionTakesItsModeFromItsTransportsConfig", "[NetTransport]")
{
	Transport held(kHeldSends);
	Transport immediate(kImmediate);
	ListenerHost host;

	ListenerSession frameLocked(host, held);
	CHECK_FALSE(frameLocked.immediate());
	frameLocked.dispatchFrame();
	CHECK(frameLocked.stats().drains == 1u);

	ListenerSession stepStart(host, immediate);
	CHECK(stepStart.immediate());
	CHECK(stepStart.drain() == 0u);
	CHECK(stepStart.stats().drains == 1u);

	ClientTee<Transport> tee(immediate);
	ogsimnet::SimNetClient<EqGame, ListenerHost, ClientTee<Transport>> unconfigured(host, tee);
	CHECK_FALSE(unconfigured.immediate());

	ConfigTee configured(immediate);
	ogsimnet::SimNetClient<EqGame, ListenerHost, ConfigTee> forwarded(host, configured);
	CHECK(forwarded.immediate());
}

TEST_CASE("SimNetClientImmediate.InImmediateModeTheNextDrainAppliesTheListenerBind", "[NetTransport]")
{
	SECTION("immediate: bind and unbind are requests the next drain applies")
	{
		Transport transport(kImmediate);
		ListenerHost host;
		ListenerSession session(host, transport);
		pushFloor(transport, 4u);
		pushTier(transport, 2u);
		CHECK(session.drain() == 2u);
		CHECK(host.calls.empty());

		session.bindListener();
		CHECK(host.calls.empty());
		CHECK_FALSE(session.listenerBound());
		pushFloor(transport, 5u);
		CHECK(session.drain() == 1u);
		CHECK(host.calls == std::vector<std::string>{"floor 4 replayed", "tier 0->2 replayed", "floor 5 arrived"});
		CHECK(session.listenerBound());

		host.calls.clear();
		session.bindListener();
		session.drain();
		CHECK(host.calls.empty());

		session.unbindListener();
		CHECK(session.listenerBound());
		pushTier(transport, 3u);
		session.drain();
		CHECK_FALSE(session.listenerBound());
		CHECK(host.calls.empty());

		session.bindListener();
		session.unbindListener();
		session.drain();
		CHECK(host.calls.empty());
		CHECK_FALSE(session.listenerBound());
	}

	SECTION("frame-locked: bind replays at once, as before")
	{
		Transport transport(kHeldSends);
		ListenerHost host;
		ListenerSession session(host, transport);
		pushFloor(transport, 4u);
		session.dispatchFrame();
		session.bindListener();
		CHECK(host.calls == std::vector<std::string>{"floor 4 replayed"});
		session.unbindListener();
		CHECK_FALSE(session.listenerBound());
	}
}

TEST_CASE("SimNetClientImmediate.Death.DispatchFrameOnAnImmediateSessionFiresTheCheck", "[.][OGSimDeath]")
{
	Transport transport(kImmediate);
	ListenerHost host;
	ListenerSession session(host, transport);
	session.dispatchFrame();
}
} // namespace simNetImmediate
