// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
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
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace simNetUnregisteringHook
{
struct UhPosition
{
	std::int32_t x = 0;
};

struct UhInput
{
	std::int32_t move = 0;
};
} // namespace simNetUnregisteringHook

template <>
struct SerializableFields<simNetUnregisteringHook::UhPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetUnregisteringHook::UhPosition, x)); }
};

template <>
struct SerializableFields<simNetUnregisteringHook::UhInput>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetUnregisteringHook::UhInput, move)); }
};

namespace simNetUnregisteringHook
{
using UhState = SimulationComposite<UhPosition>;

class UhAllState
{
public:
	const UhState& getState() const { return m_state; }
	UhState& editState() { return m_state; }

private:
	UhState m_state;
};

struct UhSim
{
	using StateType = UhState;
	using InputType = UhInput;

	UhAllState m_allState;
	UhAllState m_vizState;

	const UhAllState& getAllState() const { return m_allState; }
	UhAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const UhAllState& getVizState() const { return m_vizState; }
};

struct UhGame
{
	using Simulatables = SimulatableList<UhSim>;
};

struct NullEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void setLaneHandler(ognet::Lane, ognet::LaneHandler) {}
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId) {}
};

using UhTransport = ogsimnet::SimNetTransport<NullEndpoint, 256>;

struct HookCall
{
	std::uint8_t simId = 0u;
	bool inStorage = false;
	bool rosterRemovedBefore = false;
};

struct UhClientHost
{
	std::function<bool(std::uint8_t)> inStorage;
	std::vector<HookCall> calls;
	std::vector<std::uint8_t> removed;

	double nowSeconds() { return 0.0; }
	double serverRttSeconds() { return 0.05; }
	void onTimingInfo(std::uint32_t, double) {}
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
	void onRosterAdded(std::uint8_t) {}
	void onRosterRemoved(std::uint8_t simId) { removed.push_back(simId); }
	void onWireMismatch(std::uint8_t, std::uint8_t) {}
	void log(const char*) {}
	void stampClientReceived(ogsimnet::SimNetStream, std::uint8_t, std::uint32_t, double) {}
	void onLifecycleUnregistering(std::uint8_t simId)
	{
		calls.push_back(HookCall{simId, inStorage(simId), !removed.empty()});
	}
};

struct UhServerHost
{
	std::function<bool(std::uint8_t)> inStorage;
	std::vector<HookCall> calls;

	double nowSeconds() { return 0.0; }
	double rttMs(SimConnectionId) { return 20.0; }
	std::int32_t receptionTick() { return 100; }
	void log(const char*) {}
	void stampServerReceived(std::uint8_t, std::uint32_t, double) {}
	void stampHanded(std::uint8_t, std::uint32_t) {}
	void onLifecycleUnregistering(std::uint8_t simId) { calls.push_back(HookCall{simId, inStorage(simId), false}); }
};

static_assert(ogsimnet::SimNetClientHost<UhClientHost> && ogsimnet::SimNetLifecycleUnregisteringHook<UhClientHost>);
static_assert(ogsimnet::SimNetServerHost<UhServerHost> && ogsimnet::SimNetLifecycleUnregisteringHook<UhServerHost>);

using UhClient = ogsimnet::SimNetClient<UhGame, UhClientHost, UhTransport>;
using UhServer = ogsimnet::SimNetServer<UhGame, UhServerHost, UhTransport>;
using UhChannel = UhClient::Channel;

static_assert(std::is_same_v<UhChannel, UhServer::Channel<UhSim>>);
} // namespace simNetUnregisteringHook

template <>
struct SimulatableOwnerTraits<simNetUnregisteringHook::UhSim>
{
	using PredictionOwnerType = simNetUnregisteringHook::UhChannel;
	using AuthorityOwnerType = simNetUnregisteringHook::UhChannel;
};

namespace simNetUnregisteringHook
{
using ogsimnet::RosterEntry;

constexpr SimConnectionId kServerConn = 11u;
constexpr SimConnectionId kClientConn = 21u;
constexpr std::uint8_t kLocal = 3u;
constexpr std::uint8_t kRemote = 7u;

using Bytes = std::vector<std::byte>;

template <class WriteFn>
Bytes envelope(WriteFn&& write)
{
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = write(std::span<std::byte>(out));
	REQUIRE(size != 0u);
	return Bytes(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(size));
}

Bytes snapshotMsg(std::uint16_t revision, const std::vector<RosterEntry>& entries)
{
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeSnapshot(revision, std::span<const RosterEntry>(entries));
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

Bytes removeMsg(std::uint16_t revision, std::uint8_t simId)
{
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeRemove(revision, simId, ogsimnet::RosterRemoveReason::Left);
	return envelope([&](std::span<std::byte> out) { return simEnvelope::writeRoster(out, message.view()); });
}

struct SimParts
{
	SimParts() { resolution.template setNeutralInput<UhSim>(UhInput{-1}); }

	SimulationObjectStorage<UhSim> storage;
	SimulationReconciliation<UhSim> reconciliation{storage};
	SimulationInputResolution<UhSim> resolution{storage, reconciliation};
	SimulationNetSync<UhSim> netSync{storage, reconciliation, resolution};
};

struct ClientRig : SimParts
{
	ClientRig()
		: transport(std::make_unique<UhTransport>(ogsimnet::SimNetConfig{}))
	{
		host.inStorage = [this](std::uint8_t simId) { return storage.template has<UhSim>(simId); };
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		client = std::make_unique<UhClient>(host, *transport);
		client->bindLifecycle(storage, reconciliation, resolution, netSync);
		transport->inbox().pushEvent(InboundKind::Connected, kServerConn, ogsimnet::kNoReceiveSeconds);
		push(SimLane::Control, snapshotMsg(1u, {RosterEntry{kLocal, 0u, 0u}, RosterEntry{kRemote, 1u, ogsimnet::kRosterNotLocal}}));
		client->drain();
	}

	void push(SimLane lane, const Bytes& bytes) { REQUIRE(transport->inbox().pushMessage(kServerConn, lane, 1.0, bytes)); }

	UhClientHost host;
	NullEndpoint endpoint;
	std::unique_ptr<UhTransport> transport;
	std::unique_ptr<UhClient> client;
};

struct ServerRig : SimParts
{
	ServerRig()
		: transport(std::make_unique<UhTransport>())
	{
		host.inStorage = [this](std::uint8_t simId) { return storage.template has<UhSim>(simId); };
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		server = std::make_unique<UhServer>(host, *transport, config, std::size_t{4096});
		server->bindLifecycle(storage, reconciliation, resolution, netSync);
		transport->inbox().pushEvent(InboundKind::Connected, kClientConn, ogsimnet::kNoReceiveSeconds);
		server->drain();
	}

	void registerOwned(std::uint8_t simId)
	{
		server->registerCharacter<UhSim>(simId, simId, kClientConn, 0u, "uh" + std::to_string(simId), UhSim{});
	}

	TimeConfig config;
	UhServerHost host;
	NullEndpoint endpoint;
	std::unique_ptr<UhTransport> transport;
	std::unique_ptr<UhServer> server;
};
} // namespace simNetUnregisteringHook

using namespace simNetUnregisteringHook;

TEST_CASE("SimNetLifecycleUnregisteringHook.TheClientHookRunsBeforeTheFacadeWhileTheCharacterIsInStorage", "[Lifecycle]")
{
	SECTION("a roster Remove of a queue-registered character")
	{
		ClientRig rig;
		rig.client->registerCharacter(kRemote, UhSim{}, nullptr);
		rig.client->drain();
		REQUIRE(rig.client->lifecycleRegistered(kRemote));
		REQUIRE(rig.storage.has<UhSim>(kRemote));
		CHECK(rig.host.calls.empty());

		rig.push(SimLane::Control, removeMsg(2u, kRemote));
		rig.client->drain();

		REQUIRE(rig.host.calls.size() == 1u);
		CHECK(rig.host.calls[0].simId == kRemote);
		CHECK(rig.host.calls[0].inStorage);
		CHECK_FALSE(rig.host.calls[0].rosterRemovedBefore);
		CHECK_FALSE(rig.storage.has<UhSim>(kRemote));
		CHECK(rig.host.removed.size() == 1u);
	}
	SECTION("a queued unregister")
	{
		ClientRig rig;
		rig.client->registerCharacter(kRemote, UhSim{}, nullptr);
		rig.client->drain();
		rig.client->unregisterCharacter(kRemote);
		CHECK(rig.host.calls.empty());
		rig.client->drain();

		REQUIRE(rig.host.calls.size() == 1u);
		CHECK(rig.host.calls[0].simId == kRemote);
		CHECK(rig.host.calls[0].inStorage);
		CHECK_FALSE(rig.storage.has<UhSim>(kRemote));
	}
	SECTION("a character the queue never registered is not reported")
	{
		ClientRig rig;
		rig.push(SimLane::Control, removeMsg(2u, kRemote));
		rig.client->drain();
		rig.client->unregisterCharacter(kLocal);
		rig.client->drain();
		CHECK(rig.host.calls.empty());
		CHECK(rig.host.removed.size() == 1u);
	}
}

TEST_CASE("SimNetLifecycleUnregisteringHook.TheServerHookRunsBeforeTheFacadeWhileTheCharacterIsInStorage", "[Lifecycle]")
{
	SECTION("removeCharacter of a queue-registered character")
	{
		ServerRig rig;
		rig.registerOwned(kRemote);
		rig.server->drain();
		REQUIRE(rig.server->lifecycleRegistered(kRemote));
		CHECK(rig.host.calls.empty());

		rig.server->removeCharacter(kRemote, ogsimnet::RosterRemoveReason::Left);

		REQUIRE(rig.host.calls.size() == 1u);
		CHECK(rig.host.calls[0].simId == kRemote);
		CHECK(rig.host.calls[0].inStorage);
		CHECK_FALSE(rig.storage.has<UhSim>(kRemote));
	}
	SECTION("a queued unregister")
	{
		ServerRig rig;
		rig.registerOwned(kRemote);
		rig.server->drain();
		rig.server->unregisterCharacter<UhSim>(kRemote, ogsimnet::RosterRemoveReason::Left);
		CHECK(rig.host.calls.empty());
		rig.server->drain();

		REQUIRE(rig.host.calls.size() == 1u);
		CHECK(rig.host.calls[0].simId == kRemote);
		CHECK(rig.host.calls[0].inStorage);
		CHECK_FALSE(rig.storage.has<UhSim>(kRemote));
		CHECK(rig.server->channel<UhSim>(kRemote) == nullptr);
	}
	SECTION("a character added without the queue is not reported")
	{
		ServerRig rig;
		REQUIRE(rig.server->addCharacter<UhSim>(kLocal, kLocal, kClientConn, 0u, "direct") != nullptr);
		rig.server->removeCharacter(kLocal, ogsimnet::RosterRemoveReason::Left);
		CHECK(rig.host.calls.empty());
	}
}
