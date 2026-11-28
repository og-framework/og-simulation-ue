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

inline constexpr std::size_t kClientCount = 2u;
inline constexpr std::int32_t kCorrectionRotationK = 1;
inline constexpr std::uint32_t kRedundancyDepthTicks = 3u;
inline constexpr std::int32_t kRedundancyFrames = 0;

inline const TimeConfig& rigSendConfig()
{
	static const TimeConfig config = [] {
		TimeConfig out;
		out.redundancyDepthTicks = static_cast<std::int32_t>(kRedundancyDepthTicks);
		out.redundancyFrames = kRedundancyFrames;
		return out;
	}();
	return config;
}

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

struct InputRow
{
	std::uint32_t tick = 0u;
	std::uint8_t simId = 0u;
	std::uint32_t captureTick = 0u;
	std::int32_t move = 0;
	std::uint8_t flags = 0u;

	friend bool operator==(const InputRow&, const InputRow&) = default;
};

struct ServerFrame
{
	std::vector<std::string> decisions;
	std::vector<std::string> trace;
	std::vector<InputRow> consumed;
	std::map<std::uint8_t, std::size_t> bundles;
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
	ChannelTransport channelTransport(std::uint8_t simId) { return m_inner.channelTransport(simId); }
	void relay(SimConnectionId skipOwner, std::span<const std::byte> bytes) { m_inner.relay(skipOwner, bytes); }
	void sendState(SimConnectionId conn, std::span<const std::byte> bytes) { m_inner.sendState(conn, bytes); }
	void sendControl(SimConnectionId conn, std::span<const std::byte> bytes) { m_inner.sendControl(conn, bytes); }
	void broadcastClock(std::span<const std::byte> bytes) { m_inner.broadcastClock(bytes); }

	ServerFrame* frame = nullptr;

private:
	InnerT& m_inner;
};

using ServerSession = ogsimnet::SimNetServer<EqGame, ServerHost, ServerTee<Transport>>;

static_assert(std::is_same_v<ServerSession::Channel<EqSim>, EqChannel>,
	"the server session's channel is the one owner type the owner traits name for both roles");

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

} // namespace simNetEquivalence
