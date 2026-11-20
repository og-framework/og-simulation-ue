// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetConnections.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/Network/RelayRingStager.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
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
#include "OGSimulation/SimulationTimeContext.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simNetEquivalence
{
struct EqPosition
{
	std::int32_t x = 0;
};

struct EqInput
{
	std::int32_t move = 0;
	std::uint8_t flags = 0u;
};
} // namespace simNetEquivalence

template <>
struct SerializableFields<simNetEquivalence::EqPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetEquivalence::EqPosition, x)); }
};

template <>
struct SerializableFields<simNetEquivalence::EqInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_MEMBER(simNetEquivalence::EqInput, move), SIM_MEMBER(simNetEquivalence::EqInput, flags));
	}
};

namespace simNetEquivalence
{
using EqState = SimulationComposite<EqPosition>;

class EqAllState
{
public:
	const EqState& getState() const { return m_state; }
	EqState& editState() { return m_state; }

private:
	EqState m_state;
};

struct EqSim
{
	using StateType = EqState;
	using InputType = EqInput;

	EqAllState m_allState;
	EqAllState m_vizState;

	const EqAllState& getAllState() const { return m_allState; }
	EqAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const EqAllState& getVizState() const { return m_vizState; }
};

struct EqGame
{
	using Simulatables = SimulatableList<EqSim>;
};

using ognet::test::TestHost;
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using Backend = ognet::EmulatedBackend<VirtualBackend>;
using Endpoint = ognet::NetEndpoint<TestHost, Backend>;

inline constexpr std::size_t kInboxSlots = 2048u;
using Transport = ogsimnet::SimNetTransport<Endpoint, kInboxSlots>;
using EqBundleTransport = ogsimnet::SimNetBundleTransport<Transport::ChannelTransport>;
using EqChannel = ogsimnet::SessionChannel<EqSim, Transport::ChannelTransport>;
} // namespace simNetEquivalence

template <>
struct SimulatableOwnerTraits<simNetEquivalence::EqSim>
{
	using PredictionOwnerType = simNetEquivalence::EqChannel;
	using AuthorityOwnerType = simNetEquivalence::EqChannel;
};

namespace simNetEquivalence
{
using ogsimnet::SimNetConfig;

using Bytes = std::vector<std::uint8_t>;

inline Bytes toBytes(std::span<const std::byte> bytes)
{
	Bytes out;
	out.reserve(bytes.size());
	for (const std::byte b : bytes)
		out.push_back(std::to_integer<std::uint8_t>(b));
	return out;
}

inline std::vector<std::byte> toWire(const Bytes& bytes)
{
	std::vector<std::byte> out;
	out.reserve(bytes.size());
	for (const std::uint8_t b : bytes)
		out.push_back(std::byte{b});
	return out;
}

inline constexpr std::size_t kClientCount = 2u;
inline constexpr int kFrames = 600;
inline constexpr std::int32_t kCorrectionRotationK = 1;
inline constexpr std::uint32_t kRedundancyDepthTicks = 3u;
inline constexpr std::int32_t kRedundancyFrames = 0;
inline constexpr std::int32_t kRollbackWindowTicks = 16;
inline constexpr std::uint32_t kFirstServerTick = 100u;
inline constexpr std::uint32_t kClientLeadTicks = 6u;
inline constexpr double kTickSeconds = 1.0 / 60.0;
inline constexpr double kServerRttSeconds = 0.05;
inline constexpr std::uint8_t kServerHostIndex = 1u;
inline constexpr std::uint8_t kFirstClientHostIndex = 10u;
inline constexpr std::uint16_t kServerPort = 7777u;
inline constexpr double kStartSeconds = 1.0;
inline constexpr double kSetupStepSeconds = 0.001;
inline constexpr int kMaxSetupSteps = 5000;
inline constexpr int kMaxServicePasses = 8;
inline constexpr char kTodaysReceiveCorrectionStateFormat[] = "[ReceiveCorrectionState] id=%u tick=%u";

struct CharacterSpec
{
	std::uint8_t simId = 0u;
	std::uint8_t slot = 0u;
	std::size_t client = 0u;
	std::uint8_t localIndex = 0u;
	const char* name = "";
};

inline constexpr std::array<CharacterSpec, 3> kCharacters{{
	{1u, 0u, 0u, 0u, "a0"},
	{2u, 1u, 0u, 1u, "a1"},
	{3u, 2u, 1u, 0u, "b0"},
}};

inline constexpr std::array<double, kClientCount> kClientRttMs{30.0, 90.0};

inline std::uint32_t ticksInFrame(int frame) { return 1u + static_cast<std::uint32_t>((frame * 7 + frame / 11) % 3); }

inline EqInput inputFor(std::uint8_t simId, std::uint32_t tick)
{
	return EqInput{static_cast<std::int32_t>((tick * 3u + simId * 1000u) % 97u) - 48, static_cast<std::uint8_t>((tick + simId) & 0x3Fu)};
}

inline const CharacterSpec* specOf(std::uint8_t simId)
{
	for (const CharacterSpec& spec : kCharacters)
	{
		if (spec.simId == simId)
			return &spec;
	}
	return nullptr;
}

struct InputRow
{
	std::uint32_t tick = 0u;
	std::uint8_t simId = 0u;
	std::uint32_t captureTick = 0u;
	std::int32_t move = 0;
	std::uint8_t flags = 0u;

	friend bool operator==(const InputRow&, const InputRow&) = default;
};

struct StreamItem
{
	std::uint8_t simId = 0u;
	Bytes bytes;

	friend bool operator==(const StreamItem&, const StreamItem&) = default;
};

struct ClientFrame
{
	std::vector<StreamItem> corrections;
	std::vector<StreamItem> rings;
	std::vector<std::uint32_t> clock;
	std::map<std::uint8_t, Bytes> correctionBuffers;
	std::map<std::uint8_t, Bytes> ringStores;
	std::vector<std::string> lines;
	std::vector<std::string> trace;
	std::vector<InputRow> inputs;
};

struct ServerFrame
{
	std::vector<std::string> decisions;
	std::vector<std::string> trace;
	std::vector<InputRow> consumed;
	std::map<std::uint8_t, std::size_t> bundles;
};

struct FrameRecord
{
	std::uint32_t ticks = 0u;
	ServerFrame server;
	std::array<ClientFrame, kClientCount> clients;
};

struct SimStack
{
	SimulationObjectStorage<EqSim> storage;
	SimulationReconciliation<EqSim> reconciliation{storage};
	SimulationInputResolution<EqSim> resolution{storage, reconciliation};
	SimulationNetSync<EqSim> netSync{storage, reconciliation, resolution};
	std::vector<std::string> trace;

	SimStack()
	{
		auto logger = [this](const char* line) { trace.emplace_back(line); };
		netSync.setLogger(logger);
		reconciliation.setLogger(logger);
		resolution.setLogger(logger);
		resolution.template setNeutralInput<EqSim>(EqInput{});
	}

	SimStack(const SimStack&) = delete;
	SimStack& operator=(const SimStack&) = delete;

	void addAuthority(std::uint8_t simId, EqChannel& channel)
	{
		registerSimulatable<EqSim>(storage, reconciliation, resolution, netSync, simId, EqSim{}, channel, channel);
	}

	void addPrediction(std::uint8_t simId, EqChannel& channel, bool local)
	{
		std::function<EqInput(const SimulationTimeStep&, const LocalInputCache<EqInput>&)> provider;
		if (local)
		{
			provider = [simId](const SimulationTimeStep& step, const LocalInputCache<EqInput>&) {
				return inputFor(simId, step.getTick());
			};
		}
		registerSimulatable<EqSim>(storage, reconciliation, resolution, netSync, simId, EqSim{}, channel, std::move(provider));
	}

	std::vector<std::uint8_t> ids()
	{
		std::vector<std::uint8_t> out;
		storage.forEachSimulatable([&](unsigned int id, auto&) { out.push_back(static_cast<std::uint8_t>(id)); });
		std::sort(out.begin(), out.end());
		return out;
	}

	void applyInputs(std::uint32_t tick, const ResolvedInputs<EqSim>& inputs, bool authority, std::vector<InputRow>& out)
	{
		const auto& map = std::get<std::unordered_map<unsigned int, EqInput>>(inputs);
		for (const std::uint8_t simId : ids())
		{
			const auto found = map.find(simId);
			const EqInput input = found == map.end() ? EqInput{} : found->second;
			storage.template get<EqSim>(simId).editAllState().editState().template edit<EqPosition>().x += input.move;
			const std::uint32_t capture = authority ? resolution.template getLastUsedCaptureTick<EqSim>(simId) : tick;
			out.push_back(InputRow{tick, simId, capture, input.move, input.flags});
		}
	}

	void stepClient(std::uint32_t tick, std::vector<InputRow>& out)
	{
		const SimulationTimeStep step(tick, false);
		reconciliation.allocateFrontierSlotsAll(step);
		const ResolvedInputs<EqSim> inputs = resolution.collectInputAll(step);
		applyInputs(tick, inputs, false, out);
		reconciliation.postPredictionAll(step);
	}

	void stepServer(std::uint32_t tick, std::vector<InputRow>& out)
	{
		const ResolvedInputs<EqSim> inputs = resolution.collectInputAll(SimulationTimeStep(tick, false));
		applyInputs(tick, inputs, true, out);
	}

	std::vector<std::string> takeTrace() { return std::exchange(trace, {}); }
};

struct ServerHost
{
	double now = 0.0;
	std::int32_t tick = 0;
	std::map<SimConnectionId, double> rtt;
	std::vector<std::string> lines;

	double nowSeconds() { return now; }
	double rttMs(SimConnectionId conn)
	{
		const auto found = rtt.find(conn);
		return found == rtt.end() ? -1.0 : found->second;
	}
	std::int32_t receptionTick() { return tick; }
	void log(const char* line) { lines.emplace_back(line); }
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
};

static_assert(ogsimnet::SimNetServerHost<ServerHost>);

struct ClientHost
{
	double now = 0.0;
	std::vector<std::string> lines;
	std::vector<std::uint32_t> clock;
	std::function<void(std::uint8_t)> added;

	double nowSeconds() { return now; }
	double serverRttSeconds() { return kServerRttSeconds; }
	void onTimingInfo(std::uint32_t tick, double) { clock.push_back(tick); }
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t simId)
	{
		if (added)
			added(simId);
	}
	void onRosterRemoved(std::uint8_t) {}
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char* line) { lines.emplace_back(line); }
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
};

static_assert(ogsimnet::SimNetClientHost<ClientHost>);

inline bool sessionOnly(const std::string& line)
{
	return line.find("[SimNet.Window]") != std::string::npos || line.find("[Roster.") != std::string::npos;
}

inline std::string withoutReceiveTime(std::string line)
{
	if (line.rfind("[ServerReceive]", 0) == 0)
	{
		const std::size_t field = line.find(" recvSeconds=");
		if (field != std::string::npos)
			line.erase(field);
	}
	return line;
}

inline std::vector<std::string> takeLines(std::vector<std::string>& lines)
{
	std::vector<std::string> out;
	for (std::string& line : lines)
	{
		if (!sessionOnly(line))
			out.push_back(withoutReceiveTime(std::move(line)));
	}
	lines.clear();
	return out;
}

inline std::vector<std::string> sorted(std::vector<std::string> lines)
{
	std::sort(lines.begin(), lines.end());
	return lines;
}

template <class ChannelT>
void snapshotChannels(const std::map<std::uint8_t, ChannelT*>& channels, ClientFrame& out)
{
	for (const auto& [simId, channel] : channels)
	{
		out.correctionBuffers[simId] = toBytes(channel->getSyncedCorrectionStateBuffer().payload());
		out.ringStores[simId] = toBytes(channel->getRelayedInputRing().payload());
	}
}

template <class InnerT>
class ClientTee
{
public:
	using ChannelTransport = typename InnerT::ChannelTransport;

	class Inbox
	{
	public:
		Inbox(InnerT& inner, ClientFrame*& frame) : m_inner(inner), m_frame(frame) {}

		template <class Fn>
		std::size_t drain(Fn&& fn)
		{
			return m_inner.inbox().drain([&](const InboundSimRecord& record) {
				note(record);
				fn(record);
			});
		}

		SimInboxStats stats() const { return m_inner.inbox().stats(); }

	private:
		void note(const InboundSimRecord& record)
		{
			if (m_frame == nullptr || record.kind != InboundKind::Message)
				return;
			using Shape = simEnvelope::ShapeFor<EqInput, EqState>;
			const DecodedSimMessage message =
				simEnvelope::decode<Shape>(record.lane, SimDirection::ServerToClient, record.payload());
			if (message.refusal != DecodeRefusal::None)
				return;
			if (message.kind == SimMessageKind::CorrectionState)
				m_frame->corrections.push_back(StreamItem{message.simId, toBytes(message.body)});
			else if (message.kind == SimMessageKind::RelayRing)
				m_frame->rings.push_back(StreamItem{message.simId, toBytes(message.body)});
		}

		InnerT& m_inner;
		ClientFrame*& m_frame;
	};

	explicit ClientTee(InnerT& inner) : m_inner(inner) {}

	Inbox inbox() { return Inbox(m_inner, frame); }
	void flushFrame() { m_inner.flushFrame(); }
	void endInputSends() { m_inner.endInputSends(); }
	ChannelTransport channelTransport(std::uint8_t simId) { return m_inner.channelTransport(simId); }
	void setServerConnection(SimConnectionId id) { m_inner.setServerConnection(id); }
	SimConnectionId serverConnection() const { return m_inner.serverConnection(); }

	ClientFrame* frame = nullptr;

private:
	InnerT& m_inner;
};

template <class InnerT>
class ServerTee
{
public:
	using ChannelTransport = typename InnerT::ChannelTransport;

	class Inbox
	{
	public:
		Inbox(InnerT& inner, ServerFrame*& frame) : m_inner(inner), m_frame(frame) {}

		template <class Fn>
		std::size_t drain(Fn&& fn)
		{
			return m_inner.inbox().drain([&](const InboundSimRecord& record) {
				note(record);
				fn(record);
			});
		}

		SimInboxStats stats() const { return m_inner.inbox().stats(); }

	private:
		void note(const InboundSimRecord& record)
		{
			if (m_frame == nullptr || record.kind != InboundKind::Message)
				return;
			using Shape = simEnvelope::ShapeFor<EqInput, EqState>;
			const DecodedSimMessage message =
				simEnvelope::decode<Shape>(record.lane, SimDirection::ClientToServer, record.payload());
			if (message.refusal != DecodeRefusal::None || message.kind != SimMessageKind::InputBundleSet)
				return;
			simEnvelope::forEachInputBundle<Shape>(message.body,
				[&](std::uint8_t simId, std::span<const std::byte>) { ++m_frame->bundles[simId]; });
		}

		InnerT& m_inner;
		ServerFrame*& m_frame;
	};

	explicit ServerTee(InnerT& inner) : m_inner(inner) {}

	Inbox inbox() { return Inbox(m_inner, frame); }
	void flushFrame() { m_inner.flushFrame(); }
	ChannelTransport channelTransport(std::uint8_t simId) { return m_inner.channelTransport(simId); }
	void relay(SimConnectionId skipOwner, std::span<const std::byte> bytes) { m_inner.relay(skipOwner, bytes); }
	void sendState(SimConnectionId conn, std::span<const std::byte> bytes) { m_inner.sendState(conn, bytes); }
	void sendControl(SimConnectionId conn, std::span<const std::byte> bytes) { m_inner.sendControl(conn, bytes); }
	void broadcastClock(std::span<const std::byte> bytes) { m_inner.broadcastClock(bytes); }

	ServerFrame* frame = nullptr;

private:
	InnerT& m_inner;
};

using ClientSession = ogsimnet::SimNetClient<EqGame, ClientHost, ClientTee<Transport>>;
using ServerSession = ogsimnet::SimNetServer<EqGame, ServerHost, ServerTee<Transport>>;

static_assert(std::is_same_v<ClientSession::Channel, EqChannel> && std::is_same_v<ServerSession::Channel<EqSim>, EqChannel>,
	"both sessions' channels are the one owner type the owner traits name for both roles, so one NetSync type serves both rigs");

struct NetPeer
{
	NetPeer(VirtualNetwork& network, std::uint8_t hostIndex, std::uint64_t seed, SimNetConfig config)
		: backend(network.addHost(hostIndex), network.clock())
		, host(network.clock(), seed)
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

	void service(double now)
	{
		for (int pass = 0; pass < kMaxServicePasses; ++pass)
		{
			if (endpoint.serviceUntil(now).datagrams == 0u)
				return;
		}
	}

	Backend backend;
	TestHost host;
	std::unique_ptr<Transport> transport;
	Endpoint endpoint;
};

struct TransportClient
{
	std::unique_ptr<NetPeer> peer;
	std::unique_ptr<ClientTee<Transport>> tee;
	ClientHost host;
	std::unique_ptr<ClientSession> session;
	SimStack stack;
	std::map<std::uint8_t, EqChannel*> channels;
	std::uint32_t tick = 0u;
};

struct TransportRig
{
	explicit TransportRig(SimNetConfig transportConfig = {}, std::optional<ognet::EmulatorConfig> clientEmulation = std::nullopt)
		: config(transportConfig)
		, emulation(clientEmulation)
	{
		network.setNow(kStartSeconds);
		serverPeer = std::make_unique<NetPeer>(network, kServerHostIndex, 100u, config);
		serverPeer->start(ognet::Role::Server);
		serverTee = std::make_unique<ServerTee<Transport>>(*serverPeer->transport);
		server = std::make_unique<ServerSession>(serverHost, *serverTee, timeConfig, std::size_t{1u << 20});
		for (std::size_t index = 0u; index < kClientCount; ++index)
			join(index);
		for (const CharacterSpec& spec : kCharacters)
		{
			EqChannel* const channel =
				server->addCharacter<EqSim>(spec.simId, spec.slot, serverIds[spec.client], spec.localIndex, spec.name);
			REQUIRE(channel != nullptr);
			serverStack.addAuthority(spec.simId, *channel);
		}
		charactersAdded = true;
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
		auto client = std::make_unique<TransportClient>();
		client->peer = std::make_unique<NetPeer>(network, static_cast<std::uint8_t>(kFirstClientHostIndex + index), 200u + index, config);
		if (emulation)
		{
			ognet::EmulatorConfig seeded = *emulation;
			seeded.seed += index;
			REQUIRE(client->peer->backend.configure(seeded));
		}
		client->peer->start(ognet::Role::Client);
		client->tee = std::make_unique<ClientTee<Transport>>(*client->peer->transport);
		client->session = std::make_unique<ClientSession>(client->host, *client->tee);
		TransportClient& joined = *client;
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
			stagerSetupFlushes += charactersAdded ? 1u : 0u;
			serverPeer->service(now);
			for (auto& client : clients)
			{
				client->host.now = now;
				client->peer->service(now);
				client->session->dispatchFrame();
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

		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			TransportClient& client = *clients[c];
			client.host.now = now;
			client.tee->frame = &record.clients[c];
			client.peer->service(now);
			client.session->dispatchFrame();
			client.tee->frame = nullptr;
			for (std::uint32_t step = 0u; step < record.ticks; ++step)
				client.stack.stepClient(client.tick++, record.clients[c].inputs);
			client.stack.netSync.sendLocalInputToAuthorityAll(client.tick - 1u, kRedundancyDepthTicks, kRedundancyFrames);
			client.session->endInputSends();
			client.peer->service(now);
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

		for (auto& client : clients)
		{
			client->session->flushFrame();
			client->peer->service(now);
		}
		server->flushFrame();
		serverPeer->service(now);

		record.server.decisions = takeLines(serverHost.lines);
		record.server.trace = serverStack.takeTrace();
		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			TransportClient& client = *clients[c];
			ClientFrame& out = record.clients[c];
			out.clock = std::exchange(client.host.clock, {});
			out.lines = takeLines(client.host.lines);
			out.trace = sorted(client.stack.takeTrace());
			snapshotChannels(client.channels, out);
		}
		return record;
	}

	SimNetConfig config;
	std::optional<ognet::EmulatorConfig> emulation;
	TimeConfig timeConfig;
	VirtualNetwork network;
	ServerHost serverHost;
	std::unique_ptr<NetPeer> serverPeer;
	std::unique_ptr<ServerTee<Transport>> serverTee;
	std::unique_ptr<ServerSession> server;
	SimStack serverStack;
	std::vector<std::unique_ptr<TransportClient>> clients;
	std::array<SimConnectionId, kClientCount> serverIds{};
	std::uint32_t serverTick = 0u;
	bool charactersAdded = false;
	std::size_t stagerSetupFlushes = 0u;
};

struct Outbound
{
	std::vector<StreamItem> rings;
	std::vector<StreamItem> corrections;
	std::vector<std::uint32_t> clock;
};

struct ReferenceClient
{
	ReferenceClient() = default;
	ReferenceClient(const ReferenceClient&) = delete;
	ReferenceClient& operator=(const ReferenceClient&) = delete;

	ClientHost host;
	Transport unsentTransport;
	std::uint64_t emptyBundles = 0u;
	SimStack stack;
	std::map<std::uint8_t, std::unique_ptr<EqChannel>> owned;
	std::map<std::uint8_t, EqChannel*> channels;
	std::vector<StreamItem> handed;
	Outbound inbound;
	std::uint32_t tick = 0u;
};

struct ReferenceRig
{
	using Coordinator = ServerReceptionCoordinator<ogsimnet::SimNetConnectionHandle, EqSim>;

	ReferenceRig(const std::array<SimConnectionId, kClientCount>& connectionIds, std::size_t stagerSetupFlushes)
		: ids(connectionIds)
		, coordinator(timeConfig)
	{
		coordinator.setLogger([this](const char* line) { lines.emplace_back(line); });
		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			InboundSimRecord connected;
			connected.kind = InboundKind::Connected;
			connected.conn = ids[c];
			liveness.apply(connected);
			rtt[ids[c]] = kClientRttMs[c];
			clients[c] = std::make_unique<ReferenceClient>();
		}
		for (const CharacterSpec& spec : kCharacters)
		{
			auto channel = std::make_unique<EqChannel>(EqBundleTransport(unsentTransport.channelTransport(spec.simId), serverEmptyBundles),
				spec.simId, spec.name);
			serverStack.addAuthority(spec.simId, *channel);
			serverChannels.emplace(spec.simId, std::move(channel));
			stagers.emplace(spec.simId,
				std::make_unique<RelayRingStager>(spec.name, [this](const char* line) { lines.emplace_back(line); }));
		}
		for (const CharacterSpec& spec : kCharacters)
		{
			for (std::size_t c = 0u; c < kClientCount; ++c)
			{
				ReferenceClient& client = *clients[c];
				auto channel = std::make_unique<EqChannel>(
					EqBundleTransport(client.unsentTransport.channelTransport(spec.simId), client.emptyBundles), spec.simId, spec.name);
				const std::uint8_t simId = spec.simId;
				channel->setOnInputHandedCallback([&client, simId](const InputBundleBuffer& bundle) {
					if (!bundle.payload().empty())
						client.handed.push_back(StreamItem{simId, toBytes(bundle.payload())});
				});
				client.stack.addPrediction(spec.simId, *channel, spec.client == c);
				client.channels[spec.simId] = channel.get();
				client.owned.emplace(spec.simId, std::move(channel));
			}
		}
		const std::uint8_t resend = static_cast<std::uint8_t>(latencyKnobs::clampRelayResendEntries(timeConfig.relayResendEntries));
		for (std::size_t flush = 0u; flush < stagerSetupFlushes; ++flush)
		{
			for (auto& [simId, stager] : stagers)
				stager->flush(resend);
		}
		lines.clear();
		serverStack.takeTrace();
		for (auto& client : clients)
		{
			client->stack.takeTrace();
			client->tick = kFirstServerTick + kClientLeadTicks;
		}
		serverTick = kFirstServerTick;
	}

	ReferenceRig(const ReferenceRig&) = delete;
	ReferenceRig& operator=(const ReferenceRig&) = delete;

	template <class InputT>
	void deliverRemoteInput(unsigned int id, std::uint32_t captureTick, const InputT& input)
	{
		const auto found = serverChannels.find(static_cast<std::uint8_t>(id));
		if (found != serverChannels.end())
			found->second->deliverDelayedRemoteInput(captureTick, input);
	}

	template <class InputT>
	void relayRemoteInput(unsigned int id, std::uint32_t captureTick, std::uint8_t dA, const InputT& input)
	{
		const auto found = stagers.find(static_cast<std::uint8_t>(id));
		if (found != stagers.end())
			found->second->template stageArrival<InputT>(captureTick, dA, input);
	}

	void sendConnectionTierToOwningClient(unsigned int, std::uint8_t) {}

	FrameRecord frame(int index)
	{
		FrameRecord record;
		record.ticks = ticksInFrame(index);

		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			ReferenceClient& client = *clients[c];
			ClientFrame& out = record.clients[c];
			const Outbound inbound = std::exchange(client.inbound, {});
			for (const StreamItem& ring : inbound.rings)
			{
				out.rings.push_back(ring);
				REQUIRE(client.channels.at(ring.simId)->storeRing(toWire(ring.bytes)));
			}
			for (const StreamItem& correction : inbound.corrections)
			{
				out.corrections.push_back(correction);
				CorrectionStateBuffer buffer;
				REQUIRE(buffer.assignFromWire(toWire(correction.bytes)));
				std::array<char, 96> line{};
				std::snprintf(line.data(), line.size(), kTodaysReceiveCorrectionStateFormat, static_cast<unsigned>(correction.simId),
					static_cast<unsigned>(buffer.template readFromBuffer<std::uint32_t>(correctionStateBuffer::kTickOffset)));
				client.host.log(line.data());
				REQUIRE(client.channels.at(correction.simId)->storeCorrection(toWire(correction.bytes)));
			}
			for (const std::uint32_t tick : inbound.clock)
				client.host.onTimingInfo(tick, client.host.serverRttSeconds());
			for (std::uint32_t step = 0u; step < record.ticks; ++step)
				client.stack.stepClient(client.tick++, out.inputs);
			client.stack.netSync.sendLocalInputToAuthorityAll(client.tick - 1u, kRedundancyDepthTicks, kRedundancyFrames);
		}

		for (const auto& [conn, bundles] : std::exchange(toServer, {}))
		{
			for (const StreamItem& bundle : bundles)
			{
				++record.server.bundles[bundle.simId];
				InputBundleBuffer buffer;
				REQUIRE(buffer.assignFromWire(toWire(bundle.bytes)));
				const ogsimnet::SimNetConnectionHandle handle = liveness.handleFor(conn);
				coordinator.noteRttSample(handle, bundle.simId, static_cast<std::int32_t>(serverTick), rtt.at(conn), *this);
				coordinator.template receiveInputBundle<EqSim>(bundle.simId, handle, specOf(bundle.simId)->localIndex, buffer, *this, *this);
			}
		}
		for (std::uint32_t step = 0u; step < record.ticks; ++step)
		{
			serverStack.netSync.setAuthorityGuardContext(serverTick, kRollbackWindowTicks);
			coordinator.template releaseDelayedInputs<EqSim>(static_cast<std::int32_t>(serverTick), 1,
				[this](unsigned int id, std::uint32_t captureTick, const EqInput& input) -> bool {
					const auto found = serverChannels.find(static_cast<std::uint8_t>(id));
					if (found == serverChannels.end())
						return false;
					found->second->deliverDelayedRemoteInput(captureTick, input);
					return true;
				});
			serverStack.stepServer(serverTick, record.server.consumed);
			++serverTick;
		}
		coordinator.reapConnections(static_cast<std::int32_t>(serverTick));
		serverStack.netSync.sendCorrectionAll(SimulationTimeStep(serverTick - 1u, false), kCorrectionRotationK);
		clockTick = serverTick - 1u;

		for (std::size_t c = 0u; c < kClientCount; ++c)
			toServer.emplace_back(ids[c], std::exchange(clients[c]->handed, {}));

		const std::uint8_t resend = static_cast<std::uint8_t>(latencyKnobs::clampRelayResendEntries(timeConfig.relayResendEntries));
		for (auto& [simId, stager] : stagers)
		{
			if (stager->flush(resend) == 0u)
				continue;
			const StreamItem ring{simId, toBytes(stager->published().payload())};
			for (std::size_t c = 0u; c < kClientCount; ++c)
			{
				if (specOf(simId)->client != c)
					clients[c]->inbound.rings.push_back(ring);
			}
		}
		for (auto& [simId, channel] : serverChannels)
		{
			if (!channel->takeDirty())
				continue;
			const StreamItem correction{simId, toBytes(std::as_const(*channel).getSyncedCorrectionStateBuffer().payload())};
			for (auto& client : clients)
				client->inbound.corrections.push_back(correction);
		}
		if (clockTick != lastClockSent)
		{
			for (auto& client : clients)
				client->inbound.clock.push_back(*clockTick);
			lastClockSent = clockTick;
		}

		record.server.decisions = std::exchange(lines, {});
		record.server.trace = serverStack.takeTrace();
		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			ReferenceClient& client = *clients[c];
			ClientFrame& out = record.clients[c];
			out.clock = std::exchange(client.host.clock, {});
			out.lines = takeLines(client.host.lines);
			out.trace = sorted(client.stack.takeTrace());
			snapshotChannels(client.channels, out);
		}
		return record;
	}

	std::array<SimConnectionId, kClientCount> ids;
	TimeConfig timeConfig;
	ogsimnet::LivenessTable liveness;
	std::map<SimConnectionId, double> rtt;
	Coordinator coordinator;
	Transport unsentTransport;
	std::uint64_t serverEmptyBundles = 0u;
	SimStack serverStack;
	std::map<std::uint8_t, std::unique_ptr<EqChannel>> serverChannels;
	std::map<std::uint8_t, std::unique_ptr<RelayRingStager>> stagers;
	std::array<std::unique_ptr<ReferenceClient>, kClientCount> clients;
	std::vector<std::pair<SimConnectionId, std::vector<StreamItem>>> toServer;
	std::vector<std::string> lines;
	std::uint32_t serverTick = 0u;
	std::optional<std::uint32_t> clockTick;
	std::optional<std::uint32_t> lastClockSent;
};

struct FrameDiff
{
	std::map<std::string, std::size_t> framesByField;
	std::map<std::string, int> firstFrameByField;
	std::set<int> frames;
	std::string first;

	void note(const std::string& field, int frame, const std::string& detail)
	{
		frames.insert(frame);
		if (framesByField[field]++ == 0u)
			firstFrameByField[field] = frame;
		if (first.empty())
			first = "frame " + std::to_string(frame) + " " + field + ": " + detail;
	}

	bool empty() const { return framesByField.empty(); }

	std::string describe() const
	{
		std::string text = std::to_string(frames.size()) + " frames differ; " + first;
		for (const auto& [field, count] : framesByField)
			text += "\n  " + field + ": " + std::to_string(count) + " frames differ, first at frame " + std::to_string(firstFrameByField.at(field));
		return text;
	}
};

inline std::string describeItems(const std::vector<StreamItem>& items)
{
	std::string text = "{";
	for (const StreamItem& item : items)
		text += " " + std::to_string(item.simId) + ":" + std::to_string(item.bytes.size()) + "B";
	return text + " }";
}

inline std::string describeLines(const std::vector<std::string>& lines)
{
	std::string text = std::to_string(lines.size()) + " lines";
	if (!lines.empty())
		text += ", first \"" + lines.front() + "\"";
	return text;
}

inline std::string describeTicks(const std::vector<std::uint32_t>& ticks)
{
	std::string text = "{";
	for (const std::uint32_t tick : ticks)
		text += " " + std::to_string(tick);
	return text + " }";
}

inline void compareFrames(int frame, const FrameRecord& transport, const FrameRecord& reference, FrameDiff& diff)
{
	auto lineDiff = [&](const char* field, const std::vector<std::string>& a, const std::vector<std::string>& b) {
		if (a != b)
			diff.note(field, frame, "transport " + describeLines(a) + " / reference " + describeLines(b));
	};
	lineDiff("server.decisions", transport.server.decisions, reference.server.decisions);
	lineDiff("server.trace", transport.server.trace, reference.server.trace);
	if (transport.server.consumed != reference.server.consumed)
		diff.note("server.consumed", frame, "the server's consumed inputs differ");
	if (transport.server.bundles != reference.server.bundles)
		diff.note("server.bundles", frame, "the bundles the server received differ");
	for (std::size_t c = 0u; c < kClientCount; ++c)
	{
		const ClientFrame& a = transport.clients[c];
		const ClientFrame& b = reference.clients[c];
		const std::string prefix = "client" + std::to_string(c) + ".";
		if (a.corrections != b.corrections)
			diff.note(prefix + "corrections", frame, "transport " + describeItems(a.corrections) + " / reference " + describeItems(b.corrections));
		if (a.rings != b.rings)
			diff.note(prefix + "rings", frame, "transport " + describeItems(a.rings) + " / reference " + describeItems(b.rings));
		if (a.clock != b.clock)
			diff.note(prefix + "clock", frame, "transport " + describeTicks(a.clock) + " / reference " + describeTicks(b.clock));
		if (a.correctionBuffers != b.correctionBuffers)
			diff.note(prefix + "correctionBuffers", frame, "the injected correction buffers differ");
		if (a.ringStores != b.ringStores)
			diff.note(prefix + "ringStores", frame, "the relay stores differ");
		if (a.lines != b.lines)
			diff.note(prefix + "lines", frame, "transport " + describeLines(a.lines) + " / reference " + describeLines(b.lines));
		if (a.trace != b.trace)
			diff.note(prefix + "trace", frame, "transport " + describeLines(a.trace) + " / reference " + describeLines(b.trace));
		if (a.inputs != b.inputs)
			diff.note(prefix + "inputs", frame, "the predicted inputs differ");
	}
}

inline std::size_t countWith(const std::vector<std::string>& lines, const char* tag)
{
	std::size_t count = 0u;
	for (const std::string& line : lines)
		count += line.find(tag) != std::string::npos ? 1u : 0u;
	return count;
}

inline std::uint32_t correctionTick(const StreamItem& item)
{
	CorrectionStateBuffer buffer;
	if (!buffer.assignFromWire(toWire(item.bytes)))
		return 0u;
	return buffer.template readFromBuffer<std::uint32_t>(correctionStateBuffer::kTickOffset);
}
} // namespace simNetEquivalence
