// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "SimNetTestRig.h"
#include "TickDriver/TickDriverFakes.h"
#include "VirtualNetwork.h"

#include "OGNet/Backend/BsdDatagramBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGNet/StdHostServices.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetIdle.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/ResimGateProbe.h"
#include "OGSimulation/SimulationManager.h"
#include "OGTickDriver/SimulationTickDriver.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGTickDriver/TickDriverConcepts.h"
#include "OGTickDriver/TickDriverThreadContext.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ogsimnet
{
namespace simNetIdleSelfCheck
{
struct ReferenceEndpoint
{
	ognet::ServiceResult serviceUntil(double);
};

struct ReturnsBool
{
	bool serviceUntil(double);
};

struct NoServiceUntil
{
};

struct ForwardingGame : tickDriverGameSelfCheck::Conforming
{
	SimNetIdle<ReferenceEndpoint>* idle = nullptr;

	decltype(auto) idleUntil(double deadlineSeconds) { return idle->idleUntil(deadlineSeconds); }
};

static_assert(SimNetIdleEndpoint<ReferenceEndpoint>, "SimNetIdleEndpoint self-check: og-net's serviceUntil shape must satisfy it");
static_assert(!SimNetIdleEndpoint<ReturnsBool> && !SimNetIdleEndpoint<NoServiceUntil>,
	"SimNetIdleEndpoint self-check: an endpoint without serviceUntil, or one whose pass does not report og-net's ServiceResult, "
	"has nothing to map");
static_assert(std::same_as<decltype(std::declval<SimNetIdle<ReferenceEndpoint>&>().idleUntil(0.0)), IdleWake>,
	"SimNetIdle::idleUntil returns the tick driver's IdleWake");
static_assert(TickDriverIdleGame<ForwardingGame>,
	"SimNetIdle: a game that forwards idleUntil to SimNetIdle satisfies TickDriverIdleGame, the FreeRunning driver's idle hook");
} // namespace simNetIdleSelfCheck
} // namespace ogsimnet

namespace simNetAuthorityLoop
{
struct LoopSim : simNetEquivalence::EqSim
{
};

struct LoopSims
{
	using Simulatables = SimulatableList<LoopSim>;
};

struct TimingSim : simNetEquivalence::EqSim
{
};

struct TimingSims
{
	using Simulatables = SimulatableList<TimingSim>;
};

class VirtualClock
{
public:
	VirtualClock(ognet::test::VirtualNetwork& network, tickDriverFakes::FakeEngine& engine)
		: m_network(network)
		, m_engine(engine)
	{
		m_engine.now = m_network.nowSeconds();
	}

	double now() const { return m_network.nowSeconds(); }

	void setNow(double seconds)
	{
		if (seconds > now())
			m_network.setNow(seconds);
		m_engine.now = now();
	}

	void at(double seconds, std::function<void()> event) { m_events.emplace(seconds, std::move(event)); }

	void clearEvents() { m_events.clear(); }

	void waitUntil(double untilSeconds)
	{
		const auto next = m_events.begin();
		if (m_firing || next == m_events.end() || next->first > untilSeconds)
		{
			setNow(untilSeconds);
			return;
		}
		setNow(next->first);
		std::function<void()> event = std::move(next->second);
		m_events.erase(next);
		m_firing = true;
		event();
		m_firing = false;
	}

private:
	ognet::test::VirtualNetwork& m_network;
	tickDriverFakes::FakeEngine& m_engine;
	std::multimap<double, std::function<void()>> m_events;
	bool m_firing = false;
};

class VirtualClockBackend
{
public:
	VirtualClockBackend(ognet::test::VirtualBackend& inner, VirtualClock& clock)
		: m_inner(inner)
		, m_clock(clock)
	{
	}

	bool open(const ognet::BindConfig& config) { return m_inner.open(config); }
	ognet::SendResult sendTo(const ognet::Address& to, std::span<const std::byte> bytes) { return m_inner.sendTo(to, bytes); }
	bool rebind() { return m_inner.rebind(); }
	void wake() { m_inner.wake(); }
	std::optional<ognet::Address> localAddress() const { return m_inner.localAddress(); }

	std::size_t receive(std::uint32_t timeoutUs, ognet::RecvFn fn)
	{
		const std::size_t ready = m_inner.receive(0u, fn);
		if (ready > 0u || timeoutUs == 0u)
			return ready;
		++waits;
		m_clock.waitUntil(m_clock.now() + static_cast<double>(timeoutUs) * 1e-6);
		return m_inner.receive(0u, fn);
	}

	std::uint64_t waits = 0u;

private:
	ognet::test::VirtualBackend& m_inner;
	VirtualClock& m_clock;
};

static_assert(ognet::DatagramBackend<VirtualClockBackend>);

using LoopEndpoint = ognet::NetEndpoint<ognet::test::TestHost, VirtualClockBackend>;
using LoopTransport = ogsimnet::SimNetTransport<LoopEndpoint, simNetEquivalence::kInboxSlots>;
using LoopChannel = ogsimnet::SessionChannel<LoopSim, LoopTransport::ChannelTransport>;
using LoopIdle = ogsimnet::SimNetIdle<LoopEndpoint>;

class RealClockHost : public ognet::StdHostServices
{
public:
	double nowSeconds() const { return tickDriverFakes::realClock::seconds(); }
};

using BsdEndpoint = ognet::NetEndpoint<RealClockHost, ognet::BsdDatagramBackend>;
using BsdTransport = ogsimnet::SimNetTransport<BsdEndpoint, simNetEquivalence::kInboxSlots>;
using TimingChannel = ogsimnet::SessionChannel<TimingSim, BsdTransport::ChannelTransport>;
using BsdIdle = ogsimnet::SimNetIdle<BsdEndpoint>;
} // namespace simNetAuthorityLoop

template <>
struct SimulatableOwnerTraits<simNetAuthorityLoop::LoopSim>
{
	using PredictionOwnerType = simNetAuthorityLoop::LoopChannel;
	using AuthorityOwnerType = simNetAuthorityLoop::LoopChannel;
};

template <>
struct SimulatableOwnerTraits<simNetAuthorityLoop::TimingSim>
{
	using PredictionOwnerType = simNetAuthorityLoop::TimingChannel;
	using AuthorityOwnerType = simNetAuthorityLoop::TimingChannel;
};

namespace simNetAuthorityLoop
{
using simNetEquivalence::CharacterSpec;
using simNetEquivalence::EqInput;
using simNetEquivalence::EqPosition;
using simNetEquivalence::InputRow;

inline constexpr float kLoopDt = 1.f / 60.f;
inline const double kLoopDtSeconds = static_cast<double>(kLoopDt);
inline constexpr double kClientPhaseSeconds = 0.0005;
inline constexpr std::uint32_t kRingDepthTicks = 64u;
inline constexpr std::uint32_t kLoopSteps = 150u;
inline constexpr double kStallStartSeconds = 1.0;
inline constexpr double kStallSeconds = 0.2;
inline constexpr std::uint32_t kTimingSteps = 601u;
inline constexpr double kTimingSlackSeconds = 0.002;
inline constexpr std::uint8_t kTimingSimId = 1u;

template <class SimT, class ChannelT>
struct LoopStack
{
	SimulationObjectStorage<SimT> storage;
	SimulationReconciliation<SimT> reconciliation{storage};
	SimulationInputResolution<SimT> resolution{storage, reconciliation};
	SimulationNetSync<SimT> netSync{storage, reconciliation, resolution};
	std::vector<std::string> trace;

	LoopStack()
	{
		auto logger = [this](const char* line) { trace.emplace_back(line); };
		netSync.setLogger(logger);
		reconciliation.setLogger(logger);
		resolution.setLogger(logger);
		resolution.template setNeutralInput<SimT>(EqInput{});
	}

	LoopStack(const LoopStack&) = delete;
	LoopStack& operator=(const LoopStack&) = delete;

	void addAuthority(std::uint8_t simId, ChannelT& channel)
	{
		registerSimulatable<SimT>(storage, reconciliation, resolution, netSync, simId, SimT{}, channel, channel);
	}

	void addPrediction(std::uint8_t simId, ChannelT& channel, bool local)
	{
		std::function<EqInput(const SimulationTimeStep&, const LocalInputCache<EqInput>&)> provider;
		if (local)
		{
			provider = [simId](const SimulationTimeStep& step, const LocalInputCache<EqInput>&) {
				return simNetEquivalence::inputFor(simId, step.getTick());
			};
		}
		registerSimulatable<SimT>(storage, reconciliation, resolution, netSync, simId, SimT{}, channel, std::move(provider));
	}

	std::vector<std::uint8_t> ids()
	{
		std::vector<std::uint8_t> out;
		storage.forEachSimulatable([&](unsigned int id, auto&) { out.push_back(static_cast<std::uint8_t>(id)); });
		std::sort(out.begin(), out.end());
		return out;
	}

	void applyInputs(std::uint32_t tick, const ResolvedInputs<SimT>& inputs, bool authority, std::vector<InputRow>& out)
	{
		const auto& map = std::get<std::unordered_map<unsigned int, EqInput>>(inputs);
		for (const std::uint8_t simId : ids())
		{
			const auto found = map.find(simId);
			const EqInput input = found == map.end() ? EqInput{} : found->second;
			storage.template get<SimT>(simId).editAllState().editState().template edit<EqPosition>().x += input.move;
			const std::uint32_t capture = authority ? resolution.template getLastUsedCaptureTick<SimT>(simId) : tick;
			out.push_back(InputRow{tick, simId, capture, input.move, input.flags});
		}
	}

	void stepClient(std::uint32_t tick, std::vector<InputRow>& out)
	{
		const SimulationTimeStep step(tick, false);
		reconciliation.allocateFrontierSlotsAll(step);
		const ResolvedInputs<SimT> inputs = resolution.collectInputAll(step);
		applyInputs(tick, inputs, false, out);
		reconciliation.postPredictionAll(step);
	}

	void stepServer(std::uint32_t tick, std::vector<InputRow>& out)
	{
		const ResolvedInputs<SimT> inputs = resolution.collectInputAll(SimulationTimeStep(tick, false));
		applyInputs(tick, inputs, true, out);
	}
};

class LoopManager
{
public:
	struct ServerClock
	{
		std::uint32_t tick = 0u;
		std::uint32_t getTick() const { return tick; }
	};

	struct ClientClock
	{
		template <class F>
		unsigned int registerResyncCallback(F&&)
		{
			return 0u;
		}
		void unregisterResyncCallback(unsigned int) {}
		unsigned int getPredictionTick() const { return 0u; }
	};

	bool runsPrediction() const { return false; }
	ClientClock& editClientClock() { return m_clientClock; }
	const ClientClock& getClientClock() const { return m_clientClock; }
	const ServerClock& getServerClock() const { return m_serverClock; }
	ResimGateProbe& editResimGateProbe() { return m_probe; }

	void onGameSimulation(const SimulationUpdateInfo&)
	{
		++m_serverClock.tick;
		m_lastStep = SimulationTimeStep(m_serverClock.tick, false, StepKind::Normal, kLoopDt);
	}

	void onPostGameSimulation(const SimulationUpdateInfo&) {}
	unsigned int onCheckIsSimilar() { return std::numeric_limits<unsigned int>::max(); }
	void prepareResimulation(std::int32_t, std::uint32_t) {}
	std::uint32_t currentIntegratedTick() const { return m_lastStep.has_value() ? m_lastStep->getTick() : 0u; }
	const std::optional<SimulationTimeStep>& lastIntegratedStep() const { return m_lastStep; }

	std::uint32_t serverTick() const { return m_serverClock.tick; }
	void setServerTick(std::uint32_t tick) { m_serverClock.tick = tick; }

private:
	ServerClock m_serverClock;
	ClientClock m_clientClock;
	ResimGateProbe m_probe;
	std::optional<SimulationTimeStep> m_lastStep;
};

struct Stamp
{
	std::uint8_t simId = 0u;
	std::uint32_t captureTick = 0u;
	double seconds = 0.0;

	friend bool operator==(const Stamp&, const Stamp&) = default;
};

struct LoopServerHost
{
	std::function<double()> clock;
	const LoopManager* manager = nullptr;
	std::map<SimConnectionId, double> rtt;
	std::vector<std::string> lines;
	std::vector<Stamp> received;
	std::vector<Stamp> handed;

	double nowSeconds() { return clock(); }
	double rttMs(SimConnectionId conn)
	{
		const auto found = rtt.find(conn);
		return found == rtt.end() ? -1.0 : found->second;
	}
	std::int32_t receptionTick() { return manager != nullptr ? static_cast<std::int32_t>(manager->serverTick()) : 0; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t simId, std::uint32_t captureTick, double recvSeconds)
	{
		received.push_back(Stamp{simId, captureTick, recvSeconds});
	}
	void stampHanded(std::uint8_t simId, std::uint32_t captureTick) { handed.push_back(Stamp{simId, captureTick, clock()}); }
};

static_assert(ogsimnet::SimNetServerHost<LoopServerHost>);

using LoopServer = ogsimnet::SimNetServer<LoopSims, LoopServerHost, LoopTransport>;
using LoopClientSession = ogsimnet::SimNetClient<LoopSims, simNetEquivalence::ClientHost, LoopTransport>;
using LoopStackT = LoopStack<LoopSim, LoopChannel>;
using TimingServer = ogsimnet::SimNetServer<TimingSims, LoopServerHost, BsdTransport>;
using TimingStack = LoopStack<TimingSim, TimingChannel>;

static_assert(std::is_same_v<LoopServer::Channel<LoopSim>, LoopChannel> && std::is_same_v<LoopClientSession::Channel, LoopChannel>);

template <class ServerT, class StackT, class IdleT>
class LoopGame
{
public:
	using Manager = LoopManager;
	using Storage = tickDriverFakes::FakeStorage;
	using RenderSnapshot = tickDriverFakes::FakeRenderSnapshot;

	LoopGame(LoopManager& manager, ServerT& server, StackT& stack, IdleT* idle)
		: m_manager(manager)
		, m_server(server)
		, m_stack(stack)
		, m_idle(idle)
	{
	}

	Manager& manager() { return m_manager; }
	Storage& storage() { return m_storage; }

	void beforePump(double, TickRateDoor&)
	{
		++wakes;
		m_server.drain();
		if (beforePumpHook)
			beforePumpHook();
	}

	void onFrameStepsDue(std::int32_t, std::int32_t steps) { batches.push_back(steps); }

	void afterInlineBatch(std::int32_t firstUpcomingSimTick)
	{
		const std::uint32_t last = m_manager.serverTick();
		m_stack.netSync.sendCorrectionAll(SimulationTimeStep(last, false), simNetEquivalence::kCorrectionRotationK);
		m_server.publishClock(last);
		m_server.afterInlineBatch(firstUpcomingSimTick);
	}

	void releaseInputsForAuthorityTick(SimTick tick)
	{
		m_stack.netSync.setAuthorityGuardContext(tick, simNetEquivalence::kRollbackWindowTicks);
		m_server.releaseForStep(static_cast<std::int32_t>(tick), 1);
		m_stack.stepServer(tick, consumed);
	}

	void onStepStarted(double) {}

	void onStepEnded()
	{
		++stepsEnded;
		if (m_stop != nullptr && stepsEnded >= m_stopAfterSteps)
			m_stop->store(true);
	}

	void onPostPhysics() { postPhysicsCalls.fetch_add(1u); }

	IdleWake idleUntil(double deadlineSeconds) { return m_idle->idleUntil(deadlineSeconds); }

	void stopAfter(std::atomic<bool>& stop, std::uint32_t steps)
	{
		m_stop = &stop;
		m_stopAfterSteps = steps;
	}

	std::function<void()> beforePumpHook;
	std::vector<InputRow> consumed;
	std::vector<std::int32_t> batches;
	std::uint32_t wakes = 0u;
	std::uint32_t stepsEnded = 0u;
	std::atomic<std::uint32_t> postPhysicsCalls{0u};

private:
	LoopManager& m_manager;
	ServerT& m_server;
	StackT& m_stack;
	IdleT* m_idle;
	Storage m_storage;
	std::atomic<bool>* m_stop = nullptr;
	std::uint32_t m_stopAfterSteps = 0u;
};

using VirtualGame = LoopGame<LoopServer, LoopStackT, LoopIdle>;
using VirtualDriver = SimulationTickDriver<VirtualGame, tickDriverFakes::FakeWorldHost, tickDriverFakes::FakePresenter,
	tickDriverFakes::FakeEngine>;
using TimingGame = LoopGame<TimingServer, TimingStack, BsdIdle>;
using TimingDriver = SimulationTickDriver<TimingGame, tickDriverFakes::FakeWorldHost, tickDriverFakes::FakePresenter,
	tickDriverFakes::RealClockFakeEngine>;

static_assert(TickDriverIdleGame<VirtualGame> && TickDriverIdleGame<TimingGame>);

struct VirtualRunnerHost
{
	VirtualClock* clock = nullptr;
	std::vector<double> sleeps;

	void sleepUntil(double untilSeconds)
	{
		sleeps.push_back(untilSeconds);
		while (clock->now() < untilSeconds)
			clock->waitUntil(untilSeconds);
	}
};

static_assert(TickDriverRunnerHost<VirtualRunnerHost>);

struct LoopPeer
{
	LoopPeer(ognet::test::VirtualNetwork& network, VirtualClock& clock, std::uint8_t hostIndex, std::uint64_t seed,
		ogsimnet::SimNetConfig config)
		: backend(network.addHost(hostIndex), clock)
		, host(network.clock(), seed)
		, transport(std::make_unique<LoopTransport>(config))
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
			config.bind.port = simNetEquivalence::kServerPort;
		config.buildIdentity.fill(0x5A);
		REQUIRE(endpoint.start(config));
		transport->markEndpointStarted();
	}

	void service(double now)
	{
		for (int pass = 0; pass < simNetEquivalence::kMaxServicePasses; ++pass)
		{
			if (endpoint.serviceUntil(now).datagrams == 0u)
				return;
		}
	}

	VirtualClockBackend backend;
	ognet::test::TestHost host;
	std::unique_ptr<LoopTransport> transport;
	LoopEndpoint endpoint;
};

struct LoopClient
{
	std::unique_ptr<LoopPeer> peer;
	simNetEquivalence::ClientHost host;
	std::unique_ptr<LoopClientSession> session;
	LoopStackT stack;
	std::map<std::uint8_t, LoopChannel*> channels;
	std::uint32_t tick = 0u;
	std::vector<InputRow> inputs;
};

struct StallWindow
{
	double startSeconds = 0.0;
	double seconds = 0.0;

	bool holds(double sinceOrigin) const { return sinceOrigin >= startSeconds && sinceOrigin < startSeconds + seconds; }
};

struct ArmResult
{
	std::vector<InputRow> consumed;
	std::vector<std::string> decisions;
	std::vector<Stamp> received;
	std::vector<Stamp> handed;
	std::vector<double> relayLatencies;
	std::vector<std::int32_t> batches;
	std::uint32_t maxStepsPerFrame = 0u;
	std::uint32_t wakes = 0u;
	std::uint32_t stalledWakes = 0u;
	std::uint32_t framePasses = 0u;
	std::uint32_t postPhysicsCalls = 0u;
	std::size_t runnerSleeps = 0u;
	ogsimnet::SimNetServerStats stats;
	double originSeconds = 0.0;
};

inline std::map<std::pair<std::uint8_t, std::uint32_t>, double> firstStamps(const std::vector<Stamp>& stamps)
{
	std::map<std::pair<std::uint8_t, std::uint32_t>, double> first;
	for (const Stamp& stamp : stamps)
		first.emplace(std::make_pair(stamp.simId, stamp.captureTick), stamp.seconds);
	return first;
}

inline std::vector<double> relayLatencies(const std::vector<Stamp>& received, const std::vector<Stamp>& handed, double fromSeconds)
{
	const auto firstReceived = firstStamps(received);
	const auto firstHanded = firstStamps(handed);
	std::vector<double> latencies;
	for (const auto& [key, at] : firstReceived)
	{
		if (at < fromSeconds)
			continue;
		const auto out = firstHanded.find(key);
		if (out != firstHanded.end())
			latencies.push_back(out->second - at);
	}
	return latencies;
}

inline std::size_t unrelayedCount(const std::vector<Stamp>& received, const std::vector<Stamp>& handed, double fromSeconds)
{
	const auto firstReceived = firstStamps(received);
	const auto firstHanded = firstStamps(handed);
	std::size_t count = 0u;
	for (const auto& [key, at] : firstReceived)
		count += (at >= fromSeconds && firstHanded.find(key) == firstHanded.end()) ? 1u : 0u;
	return count;
}

inline std::vector<std::string> decisionLines(const std::vector<std::string>& lines)
{
	std::vector<std::string> out;
	for (const std::string& line : lines)
	{
		if (line.find("[RelayFlush]") == std::string::npos && line.find("[SimNet.Window]") == std::string::npos)
			out.push_back(line);
	}
	return out;
}

inline double maxOf(const std::vector<double>& values) { return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end()); }
inline double minOf(const std::vector<double>& values) { return values.empty() ? 0.0 : *std::min_element(values.begin(), values.end()); }

inline double meanOf(const std::vector<double>& values)
{
	double sum = 0.0;
	for (const double value : values)
		sum += value;
	return values.empty() ? 0.0 : sum / static_cast<double>(values.size());
}

class LoopRig
{
public:
	LoopRig()
		: clock(network, engine)
	{
		clock.setNow(simNetEquivalence::kStartSeconds);
		serverPeer = std::make_unique<LoopPeer>(network, clock, simNetEquivalence::kServerHostIndex, 100u, ogsimnet::SimNetConfig{});
		serverPeer->start(ognet::Role::Server);
		serverHost.clock = [this] { return clock.now(); };
		serverHost.manager = &manager;
		server = std::make_unique<LoopServer>(serverHost, *serverPeer->transport, timeConfig, std::size_t{1u << 20});
		idle.emplace(serverPeer->endpoint);
		for (std::size_t index = 0u; index < simNetEquivalence::kClientCount; ++index)
			join(index);
		for (const CharacterSpec& spec : simNetEquivalence::kCharacters)
		{
			LoopChannel* const channel =
				server->addCharacter<LoopSim>(spec.simId, spec.slot, serverIds[spec.client], spec.localIndex, spec.name);
			REQUIRE(channel != nullptr);
			serverStack.addAuthority(spec.simId, *channel);
		}
		REQUIRE(setupUntil([&] {
			for (const auto& client : clients)
			{
				if (client->channels.size() != simNetEquivalence::kCharacters.size())
					return false;
			}
			return true;
		}));
		serverStack.trace.clear();
		serverHost.lines.clear();
		serverHost.received.clear();
		serverHost.handed.clear();
		for (auto& client : clients)
		{
			client->stack.trace.clear();
			client->host.lines.clear();
			client->host.clock.clear();
			client->tick = simNetEquivalence::kFirstServerTick + simNetEquivalence::kClientLeadTicks;
		}
		manager.setServerTick(simNetEquivalence::kFirstServerTick);
		originSeconds = clock.now();
	}

	LoopRig(const LoopRig&) = delete;
	LoopRig& operator=(const LoopRig&) = delete;

	ArmResult runAuthorityLoop(std::uint32_t steps, std::optional<StallWindow> frameThreadStall, bool frameThread)
	{
		scheduleClientFrames(steps);
		VirtualGame game(manager, *server, serverStack, &*idle);
		std::atomic<bool> stop{false};
		game.stopAfter(stop, steps);
		tickDriverFakes::Events events;
		tickDriverFakes::FakeWorldHost host(events, tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning, kRingDepthTicks);
		tickDriverFakes::FakePresenter presenter(events);
		VirtualRunnerHost runner{&clock};
		VirtualDriver driver(game, host, presenter, engine,
			VirtualDriver::Config{kLoopDtSeconds, true, StepThreading::FreeRunning, 60});
		ArmResult result;
		if (frameThread)
		{
			game.beforePumpHook = [&]() {
				if (frameThreadStall && frameThreadStall->holds(clock.now() - originSeconds))
				{
					++result.stalledWakes;
					return;
				}
				std::thread frame([&driver]() { driver.endFrame(nullptr, 0); });
				frame.join();
				++result.framePasses;
			};
		}
		driver.runAuthorityLoop(runner, stop);
		driver.stop();
		clock.clearEvents();
		result.maxStepsPerFrame = driver.getDiagnostics().windowCounts().maxStepsPerFrame;
		result.runnerSleeps = runner.sleeps.size();
		return collect(std::move(result), game);
	}

	double originSeconds = 0.0;

private:
	void join(std::size_t index)
	{
		auto client = std::make_unique<LoopClient>();
		client->peer = std::make_unique<LoopPeer>(network, clock,
			static_cast<std::uint8_t>(simNetEquivalence::kFirstClientHostIndex + index), 200u + index, ogsimnet::SimNetConfig{});
		client->peer->start(ognet::Role::Client);
		client->session = std::make_unique<LoopClientSession>(client->host, *client->peer->transport);
		LoopClient& joined = *client;
		client->host.added = [&joined](std::uint8_t simId) {
			LoopChannel* const channel = joined.session->channel(simId);
			REQUIRE(channel != nullptr);
			const auto* entry = joined.session->roster().entryFor(simId);
			REQUIRE(entry != nullptr);
			joined.stack.addPrediction(simId, *channel, entry->isLocal());
			joined.channels[simId] = channel;
		};
		client->peer->endpoint.connect(ognet::test::virtualHostAddress(simNetEquivalence::kServerHostIndex, simNetEquivalence::kServerPort));
		clients.push_back(std::move(client));
		REQUIRE(setupUntil([&] {
			return joined.session->serverConnection() != kInvalidSimConnection && server->attachedConnections().size() == index + 1u
				&& joined.session->roster().attached();
		}));
		serverIds[index] = server->attachedConnections().back();
		serverHost.rtt[serverIds[index]] = simNetEquivalence::kClientRttMs[index];
	}

	template <class DoneT>
	bool setupUntil(DoneT done)
	{
		for (int i = 0; i < simNetEquivalence::kMaxSetupSteps; ++i)
		{
			if (done())
				return true;
			clock.setNow(clock.now() + simNetEquivalence::kSetupStepSeconds);
			const double now = clock.now();
			serverPeer->service(now);
			server->drain();
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

	void clientFrame(LoopClient& client)
	{
		const double now = clock.now();
		client.host.now = now;
		client.peer->service(now);
		client.session->drain();
		client.stack.stepClient(client.tick++, client.inputs);
		client.session->afterCaptureTick_Step(client.stack.netSync, client.tick - 1u, simNetEquivalence::rigSendConfig());
		client.session->flushFrame();
		client.peer->service(now);
	}

	void scheduleClientFrames(std::uint32_t steps)
	{
		for (std::uint32_t k = 1u; k < steps + 2u; ++k)
		{
			clock.at(originSeconds + static_cast<double>(k) * kLoopDtSeconds + kClientPhaseSeconds, [this] {
				for (auto& client : clients)
					clientFrame(*client);
			});
		}
	}

	ArmResult collect(ArmResult result, VirtualGame& game)
	{
		result.consumed = game.consumed;
		result.batches = game.batches;
		result.wakes = game.wakes;
		result.postPhysicsCalls = game.postPhysicsCalls.load();
		result.decisions = decisionLines(serverHost.lines);
		result.received = serverHost.received;
		result.handed = serverHost.handed;
		result.relayLatencies = relayLatencies(serverHost.received, serverHost.handed, originSeconds);
		result.stats = server->stats();
		result.originSeconds = originSeconds;
		return result;
	}

public:
	ognet::test::VirtualNetwork network;
	tickDriverFakes::FakeEngine engine;
	VirtualClock clock;
	TimeConfig timeConfig;
	LoopManager manager;
	LoopServerHost serverHost;
	std::unique_ptr<LoopPeer> serverPeer;
	std::unique_ptr<LoopServer> server;
	std::optional<LoopIdle> idle;
	LoopStackT serverStack;
	std::vector<std::unique_ptr<LoopClient>> clients;
	std::array<SimConnectionId, simNetEquivalence::kClientCount> serverIds{};
};

inline std::string describeLatencies(const char* arm, const std::vector<double>& latencies)
{
	return std::string(arm) + ": " + std::to_string(latencies.size()) + " relayed captures, relay latency min/mean/max = "
		+ std::to_string(minOf(latencies) * 1e3) + "/" + std::to_string(meanOf(latencies) * 1e3) + "/"
		+ std::to_string(maxOf(latencies) * 1e3) + " ms";
}

TEST_CASE("SimNetAuthorityLoop.ASimInputArrivingMidIdleIsRelayedInTheSameWake", "[NetTransport]")
{
	LoopRig loopRig;
	const ArmResult loop = loopRig.runAuthorityLoop(kLoopSteps, std::nullopt, false);

	WARN(describeLatencies("authority loop", loop.relayLatencies));

	REQUIRE(loop.relayLatencies.size() >= (kLoopSteps - 2u) * simNetEquivalence::kCharacters.size());
	CHECK(std::all_of(loop.relayLatencies.begin(), loop.relayLatencies.end(), [](double latency) { return latency == 0.0; }));
	CHECK(maxOf(loop.relayLatencies) == 0.0);
	CHECK(unrelayedCount(loop.received, loop.handed, loop.originSeconds) == 0u);

	CHECK(std::all_of(loop.batches.begin(), loop.batches.end(), [](std::int32_t steps) { return steps == 1; }));
	CHECK(loop.batches.size() == kLoopSteps);
	CHECK(loop.wakes > kLoopSteps);
	CHECK(loop.runnerSleeps == 0u);
	CHECK(loop.stats.relaysSent > 0u);
}

TEST_CASE("SimNetAuthorityLoop.TheAuthorityConsumesAMoveForEveryCharacterEveryStepAndRefusesNoArrival", "[NetTransport]")
{
	LoopRig loopRig;
	const ArmResult loop = loopRig.runAuthorityLoop(kLoopSteps, std::nullopt, false);

	REQUIRE(loop.consumed.size() == kLoopSteps * simNetEquivalence::kCharacters.size());
	CHECK(std::count_if(loop.consumed.begin(), loop.consumed.end(), [](const InputRow& row) { return row.move != 0; })
		> static_cast<std::ptrdiff_t>(loop.consumed.size() / 2u));

	REQUIRE_FALSE(loop.decisions.empty());
	CHECK(loop.stats.badDecode == 0u);
	CHECK(loop.stats.unknownSim == 0u);
	CHECK(loop.stats.refusedNotOwner == 0u);
}

TEST_CASE("SimNetAuthorityLoop.A200MsFrameThreadStallChangesNothingOnTheRunner", "[NetTransport]")
{
	const StallWindow stall{kStallStartSeconds, kStallSeconds};
	LoopRig steadyRig;
	const ArmResult steady = steadyRig.runAuthorityLoop(kLoopSteps, std::nullopt, true);
	LoopRig stalledRig;
	const ArmResult stalled = stalledRig.runAuthorityLoop(kLoopSteps, stall, true);

	CHECK(stalled.stalledWakes >= 11u);
	CHECK(stalled.postPhysicsCalls == stalled.framePasses);
	CHECK(stalled.postPhysicsCalls < steady.postPhysicsCalls);
	CHECK(stalled.maxStepsPerFrame == 1u);
	CHECK(stalled.consumed == steady.consumed);
	CHECK(stalled.decisions == steady.decisions);
	CHECK(stalled.received == steady.received);
	CHECK(stalled.handed == steady.handed);
	CHECK(maxOf(stalled.relayLatencies) == 0.0);
	CHECK(unrelayedCount(stalled.received, stalled.handed, stalled.originSeconds) == 0u);
}

TEST_CASE("SimNetAuthorityLoop.AnImmediateServerSendsAtOnceAndRefusesNothingTheLoopNeeds", "[NetTransport]")
{
	LoopRig rig;
	const ArmResult loop = rig.runAuthorityLoop(kLoopSteps, std::nullopt, false);
	CHECK(loop.stats.statesSent >= kLoopSteps * simNetEquivalence::kClientCount);
	CHECK(loop.stats.clocksSent >= kLoopSteps);
	CHECK(loop.stats.relaysSent >= (kLoopSteps - 2u) * simNetEquivalence::kCharacters.size());
	for (const auto& client : rig.clients)
	{
		CHECK(client->host.clock.size() >= kLoopSteps - 2u);
		CHECK(client->session->stats().ringsStored > 0u);
		CHECK(client->session->stats().correctionsStored > 0u);
	}
}

struct LifecycleRig
{
	LifecycleRig()
		: transport(ogsimnet::SimNetConfig{})
	{
		host.clock = [] { return 0.0; };
		server = std::make_unique<LoopServer>(host, transport, config, std::size_t{4096});
		server->bindLifecycle(stack.storage, stack.reconciliation, stack.resolution, stack.netSync);
	}

	void registerLocal(std::uint8_t simId, std::uint8_t slot, std::vector<LoopChannel*>& applied)
	{
		server->registerCharacter<LoopSim>(simId, slot, kInvalidSimConnection, ogsimnet::kRosterNotLocal, "lc" + std::to_string(simId),
			LoopSim{}, [&applied](LoopChannel* channel) { applied.push_back(channel); });
	}

	std::size_t countWith(const std::string& text) const
	{
		return static_cast<std::size_t>(
			std::count_if(host.lines.begin(), host.lines.end(), [&](const std::string& line) { return line.find(text) != std::string::npos; }));
	}

	TimeConfig config;
	LoopServerHost host;
	LoopTransport transport;
	std::unique_ptr<LoopServer> server;
	LoopStackT stack;
};

TEST_CASE("SimNetAuthorityLoop.AServerRegistrationTheQueueCannotApplyLogsALifecycleLine", "[Lifecycle]")
{
	LifecycleRig rig;
	std::vector<LoopChannel*> applied;
	rig.registerLocal(3u, 0u, applied);
	rig.registerLocal(3u, 1u, applied);
	rig.registerLocal(4u, 0u, applied);
	rig.server->drain();

	REQUIRE(applied.size() == 3u);
	CHECK(applied[0] != nullptr);
	CHECK(applied[1] == nullptr);
	CHECK(applied[2] == nullptr);
	CHECK(rig.server->lifecycleStats().registersRefused == 2u);
	CHECK(rig.countWith("[Warning][SimNet.Lifecycle] role=server simId=3 refused=SimIdLive") == 1u);
	CHECK(rig.countWith("[Warning][SimNet.Lifecycle] role=server simId=4 refused=SlotLive") == 1u);
	CHECK(rig.countWith("[SimNet.Lifecycle]") == 2u);
	CHECK(rig.countWith("[Warning][Roster.Refused] role=server simId=3 slot=1 reason=SimIdLive") == 1u);
}

struct ScriptedEndpoint
{
	ognet::ServiceResult result;
	std::vector<double> deadlines;

	ognet::ServiceResult serviceUntil(double deadlineSeconds)
	{
		deadlines.push_back(deadlineSeconds);
		return result;
	}
};

TEST_CASE("SimNetAuthorityLoop.SimNetIdleMapsEachServiceResultOntoItsWakeAndPassesTheDeadlineThrough", "[NetTransport]")
{
	ScriptedEndpoint endpoint;
	ogsimnet::SimNetIdle<ScriptedEndpoint> idle(endpoint);

	endpoint.result = ogsimnet::simNetServiceResult(2u, false, false);
	CHECK(idle.idleUntil(1.25) == IdleWake::Activity);
	endpoint.result = ogsimnet::simNetServiceResult(0u, true, false);
	CHECK(idle.idleUntil(1.5) == IdleWake::Deadline);
	endpoint.result = ogsimnet::simNetServiceResult(0u, false, true);
	CHECK(idle.idleUntil(1.75) == IdleWake::Unavailable);
	endpoint.result = ogsimnet::simNetServiceResult(1u, true, true);
	CHECK(idle.idleUntil(2.0) == IdleWake::Activity);
	CHECK(endpoint.deadlines == std::vector<double>{1.25, 1.5, 1.75, 2.0});
}

TEST_CASE("SimNetAuthorityLoop.Death.AServiceResultWithNoWakeReasonFiresTheCheck", "[.][OGSimDeath]")
{
	ScriptedEndpoint endpoint;
	endpoint.result = ogsimnet::simNetServiceResult(0u, false, false);
	ogsimnet::SimNetIdle<ScriptedEndpoint> idle(endpoint);
	idle.idleUntil(1.0);
	FAIL("SimNetIdle::idleUntil accepted a ServiceResult with no datagram, no deadline and no stop");
}

struct TimingClient
{
	std::atomic<ognet::ConnectionId> server{ognet::kInvalidConnection};

	static void onEvent(void* ctx, const ognet::ConnectionEvent& event)
	{
		if (event.kind == ognet::ConnectionEventKind::Connected)
			static_cast<TimingClient*>(ctx)->server.store(event.id);
	}
};

inline std::vector<std::byte> timingInputSet(std::uint32_t captureTick)
{
	InputBundleBuffer bundle;
	bundle.appendSlot<EqInput>(captureTick, simNetEquivalence::inputFor(kTimingSimId, captureTick));
	std::array<std::byte, kMaxSimMessageBytes> out{};
	std::size_t used = simEnvelope::writeInputBundleSetHeader(out);
	used = simEnvelope::appendInputBundle(out, used, kTimingSimId, bundle.payload());
	return std::vector<std::byte>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(used));
}

inline std::string windowFieldOf(const std::string& line, const std::string& name)
{
	const std::string key = " " + name + "=";
	const std::size_t at = line.find(key);
	if (at == std::string::npos)
		return "<missing>";
	const std::size_t from = at + key.size();
	return line.substr(from, line.find(' ', from) - from);
}

inline double secondOfThree(const std::string& percentiles)
{
	const std::size_t first = percentiles.find('/');
	const std::size_t second = percentiles.find('/', first + 1u);
	return std::stod(percentiles.substr(first + 1u, second - first - 1u));
}

TEST_CASE("SimNetAuthorityLoop.Timing.ARealSocketIdleWithASpinPreciseApproachKeepsLateUsP99Within100Us", "[.][OGSimTiming]")
{
	const std::optional<ognet::Address> loopback = ognet::parseAddress("127.0.0.1:0");
	REQUIRE(loopback.has_value());
	ognet::BsdBackendOptions serverOptions;
	serverOptions.role = ognet::Role::Server;
	serverOptions.bindAddress = *loopback;
	ognet::BsdBackendOptions clientOptions;
	clientOptions.bindAddress = *loopback;
	ognet::BindConfig bind;
	bind.dualStack = false;

	RealClockHost serverServices;
	RealClockHost clientServices;
	ognet::BsdDatagramBackend serverBackend(serverOptions);
	ognet::BsdDatagramBackend clientBackend(clientOptions);
	BsdEndpoint serverEndpoint(serverServices, serverBackend);
	BsdEndpoint clientEndpoint(clientServices, clientBackend);

	BsdTransport transport(ogsimnet::SimNetConfig{});
	transport.installHandlers(serverEndpoint);
	serverEndpoint.setConnectionHandler(transport.connectionHandler());
	ognet::EndpointConfig serverConfig;
	serverConfig.role = ognet::Role::Server;
	serverConfig.bind = bind;
	serverConfig.buildIdentity.fill(0x5A);
	REQUIRE(serverEndpoint.start(serverConfig));
	transport.markEndpointStarted();
	const std::optional<ognet::Address> serverAddress = serverBackend.localAddress();
	REQUIRE(serverAddress.has_value());
	ognet::Address target = *loopback;
	target.port = serverAddress->port;

	TimingClient client;
	clientEndpoint.setConnectionHandler(ognet::ConnectionHandler{&TimingClient::onEvent, &client});
	ognet::EndpointConfig clientConfig;
	clientConfig.role = ognet::Role::Client;
	clientConfig.bind = bind;
	clientConfig.buildIdentity.fill(0x5A);
	REQUIRE(clientEndpoint.start(clientConfig));
	clientEndpoint.startOwnThread();
	clientEndpoint.connect(target);

	LoopManager manager;
	LoopServerHost host;
	host.clock = [] { return tickDriverFakes::realClock::seconds(); };
	host.manager = &manager;
	TimeConfig timeConfig;
	TimingServer server(host, transport, timeConfig, std::size_t{1u << 20});
	TimingStack stack;
	BsdIdle idle(serverEndpoint);

	const double setupEnd = tickDriverFakes::realClock::seconds() + 5.0;
	while (server.attachedConnections().empty() && tickDriverFakes::realClock::seconds() < setupEnd)
	{
		serverEndpoint.serviceUntil(tickDriverFakes::realClock::seconds() + 0.001);
		server.drain();
	}
	REQUIRE(server.attachedConnections().size() == 1u);
	REQUIRE(client.server.load() != ognet::kInvalidConnection);
	TimingChannel* const channel = server.addCharacter<TimingSim>(kTimingSimId, 0u, server.attachedConnections().front(), 0u, "timing");
	REQUIRE(channel != nullptr);
	stack.addAuthority(kTimingSimId, *channel);
	host.rtt[server.attachedConnections().front()] = 1.0;
	manager.setServerTick(simNetEquivalence::kFirstServerTick);

	std::atomic<bool> stop{false};
	std::atomic<bool> senderStop{false};
	std::thread sender([&]() {
		std::uint32_t captureTick = simNetEquivalence::kFirstServerTick + simNetEquivalence::kClientLeadTicks;
		auto next = std::chrono::steady_clock::now();
		while (!senderStop.load())
		{
			const std::vector<std::byte> set = timingInputSet(captureTick++);
			clientEndpoint.send(client.server.load(), ognet::Lane::SimInput, set);
			next += std::chrono::microseconds(16667);
			std::this_thread::sleep_until(next);
		}
	});

	TimingGame game(manager, server, stack, &idle);
	game.stopAfter(stop, kTimingSteps);
	tickDriverFakes::Events events;
	tickDriverFakes::RealClockFakeEngine engine;
	tickDriverFakes::FakeWorldHost worldHost(events, tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning, kRingDepthTicks);
	tickDriverFakes::FakePresenter presenter(events);
	tickDriverFakes::RealRunnerHost runner;
	TimingDriver driver(game, worldHost, presenter, engine,
		TimingDriver::Config{kLoopDtSeconds, true, StepThreading::FreeRunning, 60, 0.75, 1.25, kTimingSlackSeconds});
	driver.runAuthorityLoop(runner, stop);
	driver.stop();
	senderStop.store(true);
	sender.join();
	clientEndpoint.stop();
	clientServices.joinThreads();
	serverEndpoint.stop();

	std::vector<std::string> windows;
	for (const tickDriverFakes::LoggedLine& line : engine.snapshotLines())
	{
		if (line.text.rfind("[SimHost.Window] role=Authority stepping=runner ", 0) == 0)
			windows.push_back(line.text);
	}
	REQUIRE(windows.size() == 1u);
	WARN(windows.front());
	WARN("lateUs=" + windowFieldOf(windows.front(), "lateUs") + " wakes/messages=" + std::to_string(game.wakes) + "/"
		+ std::to_string(server.stats().messages) + " relaysSent=" + std::to_string(server.stats().relaysSent)
		+ " unknownSim=" + std::to_string(server.stats().unknownSim) + " badDecode=" + std::to_string(server.stats().badDecode));
	CHECK(windowFieldOf(windows.front(), "steps") == std::to_string(kTimingSteps));
	CHECK(server.stats().messages > kTimingSteps / 2u);
	CHECK(secondOfThree(windowFieldOf(windows.front(), "lateUs")) <= 100.0);
}
} // namespace simNetAuthorityLoop
