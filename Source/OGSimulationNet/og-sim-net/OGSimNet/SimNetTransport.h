#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetTransport-rationale.md · docs/SimNetTransport-guards.md

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Handshake/Handshake.h"
#include "OGNet/Lanes/LaneScheduler.h"
#include "OGNet/NetTypes.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>

namespace ogsimnet
{
inline constexpr size_t kHoldArenaBytes = size_t{64} << 10;
inline constexpr double kNoReceiveSeconds = -1.0;
inline constexpr uint64_t kDefaultControlReceiveDepth =
	ognet::lanes::receiveDepthMessages(ognet::lanes::kDefaultLaneTable[ognet::lanes::laneIndex(ognet::Lane::Control)]);
inline constexpr double kSimNetEventGapSeconds = 1.0;
inline constexpr uint64_t kDefaultConnectsPerDrainGap =
	static_cast<uint64_t>(ognet::handshake::kCookieHellosPerSecond * (1.0 + kSimNetEventGapSeconds));
inline constexpr uint64_t kClientConnectsPerDrainGap = 4u;

static_assert(kDefaultConnectsPerDrainGap == 400u,
	"kDefaultConnectsPerDrainGap: og-net admits a new connection only on a cookie Hello, from a global bucket that holds one "
	"second of its 200 a second, so at most 200 + 200 x 1 = 400 connections open in a 1 s drain gap");

static_assert(std::is_same_v<SimConnectionId, ognet::ConnectionId> && kInvalidSimConnection == ognet::kInvalidConnection,
	"SimConnectionId: a sim connection id is og-net's connection id, unconverted, and 0 is invalid on both sides. "
	"Was SimNetTransport-rationale.md §2");
static_assert(static_cast<uint8_t>(SimLane::Control) == static_cast<uint8_t>(ognet::Lane::Control)
		&& static_cast<uint8_t>(SimLane::SimClock) == static_cast<uint8_t>(ognet::Lane::SimClock)
		&& static_cast<uint8_t>(SimLane::SimInput) == static_cast<uint8_t>(ognet::Lane::SimInput)
		&& static_cast<uint8_t>(SimLane::SimRelay) == static_cast<uint8_t>(ognet::Lane::SimRelay)
		&& static_cast<uint8_t>(SimLane::SimState) == static_cast<uint8_t>(ognet::Lane::SimState),
	"SimLane: each value is the og-net Lane of the same name, so the transport converts a lane with a cast and no table; a "
	"mismatch delivers every message on another lane. Was SimNetTransport-rationale.md §2");
static_assert(kSimLaneCount <= ognet::kLaneCount, "SimLane: the sim lanes are og-net's first lanes, so there are no more of them");
static_assert(kMaxSimMessageBytes <= ognet::kMaxUnreliablePayloadBytes,
	"kMaxSimMessageBytes: every sim message the inbox accepts fits one og-net unreliable message (1,200 B). "
	"Was SimNetTransport-rationale.md §5");
static_assert(simEnvelope::kInputBundleSetHeaderBytes + 1u + inputRedundancyBundle::kHeaderBytes <= kMaxSimMessageBytes,
	"SimNetTransport: an input set with one empty bundle fits a sim message, so a set always takes its first bundle");

template <typename E>
concept SimNetEndpoint = requires(E& e, ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes,
	ognet::LaneHandler laneHandler, ognet::ConnectionHandler connectionHandler)
{
	{ e.send(id, lane, bytes) } -> std::same_as<void>;
	{ e.broadcastExcept(id, lane, bytes) } -> std::same_as<void>;
	{ e.setLaneHandler(lane, laneHandler) } -> std::same_as<void>;
	{ e.setConnectionHandler(connectionHandler) } -> std::same_as<void>;
	{ e.disconnect(id) } -> std::same_as<void>;
};

struct SimNetConfig
{
	bool immediate = false;
	size_t maxConnections = ognet::handshake::kDefaultMaxConnections;
	uint64_t controlReceiveDepth = kDefaultControlReceiveDepth;
	uint64_t connectsPerDrainGap = kDefaultConnectsPerDrainGap;
};

constexpr SimNetConfig simNetConfigFor(const ognet::EndpointConfig& endpoint, bool immediate = false)
{
	SimNetConfig config;
	config.immediate = immediate;
	config.maxConnections = endpoint.role == ognet::Role::Client ? size_t{1} : endpoint.maxConnections;
	config.controlReceiveDepth = ognet::lanes::receiveDepthMessages(endpoint.lanes[ognet::lanes::laneIndex(ognet::Lane::Control)]);
	config.connectsPerDrainGap = endpoint.role == ognet::Role::Client ? kClientConnectsPerDrainGap : kDefaultConnectsPerDrainGap;
	return config;
}

constexpr size_t simNetConnectionSlots(const SimNetConfig& config) { return std::max<size_t>(config.maxConnections, 1u); }

constexpr size_t simNetControlSlots(const SimNetConfig& config)
{
	return simNetConnectionSlots(config) * static_cast<size_t>(config.controlReceiveDepth);
}

constexpr size_t simNetEventSlots(const SimNetConfig& config)
{
	return simNetConnectionSlots(config) + 2u * static_cast<size_t>(config.connectsPerDrainGap);
}

template <size_t InboxN>
constexpr uint32_t simNetQuotaSlots(const SimNetConfig& config)
{
	return static_cast<uint32_t>((InboxN - kReservedEventSlots) / simNetConnectionSlots(config));
}

template <size_t InboxN>
constexpr SimInboxConfig simNetInboxConfig(const SimNetConfig& config)
{
	SimInboxConfig out;
	out.controlSlots = simNetControlSlots(config);
	out.tags = simNetConnectionSlots(config);
	out.tagQuota = simNetConnectionSlots(config) > 1u ? simNetQuotaSlots<InboxN>(config) : 0u;
	out.tagControlQuota = static_cast<uint32_t>(config.controlReceiveDepth);
	out.eventSlots = simNetEventSlots(config);
	return out;
}

static_assert(kDefaultControlReceiveDepth == 456u && simNetControlSlots(SimNetConfig{}) == 7296u
		&& simNetInboxConfig<2048>(SimNetConfig{}).tagControlQuota == 456u
		&& simNetInboxConfig<2048>(SimNetConfig{}).tags * simNetInboxConfig<2048>(SimNetConfig{}).tagControlQuota
			== simNetInboxConfig<2048>(SimNetConfig{}).controlSlots,
	"SimNetConfig: each of a server's 16 connection slots may hold og-net's Control receive depth (200 + 256 = 456) undrained "
	"Control messages, and the Control ring holds exactly the 16 quotas, 7,296 slots, so it cannot overflow. Was a ring of 7,296 "
	"with no per-connection bound, which 16 connections flooding between two drains overflowed (P2-21 F1)");
static_assert(simNetQuotaSlots<2048>(SimNetConfig{}) == 124u,
	"SimNetConfig: each of a server's 16 connections may hold floor(1,984 / 16) = 124 undrained messages of the unreliable "
	"lanes, so no connection can fill the slots the others need. Was the shared inbox's first come, first served (P1-22 F1)");
static_assert([] {
		ognet::EndpointConfig server;
		server.role = ognet::Role::Server;
		const SimNetConfig onServer = simNetConfigFor(server);
		const SimNetConfig onClient = simNetConfigFor(ognet::EndpointConfig{});
		const SimInboxConfig clientInbox = simNetInboxConfig<2048>(onClient);
		return onServer.maxConnections == ognet::handshake::kDefaultMaxConnections && onServer.controlReceiveDepth == 456u
			&& onClient.maxConnections == 1u && simNetControlSlots(onClient) == 456u && clientInbox.tags == 1u
			&& clientInbox.tagQuota == 0u && clientInbox.tagControlQuota == 456u;
	}(),
	"simNetConfigFor: a server sizes its inbox for its endpoint's maxConnections, a client for its one connection to the server. "
	"The client's one slot has the same Control quota (456) and no quota on the other lanes, which have no other connection to "
	"be isolated from");
static_assert(simNetInboxConfig<2048>(SimNetConfig{}).eventSlots == 816u
		&& simNetEventSlots(simNetConfigFor(ognet::EndpointConfig{})) == 9u,
	"SimNetConfig: connection events have a ring of their own, sized for one event per connection slot (its open Disconnected) "
	"and two per connection that opens in one drain gap: 16 + 2 x 400 = 816 on a server, 1 + 2 x 4 = 9 on a client. Was the "
	"shared ring's 64-slot event reserve, which more than 32 connect and disconnect pairs between two drains, with every slot "
	"at its quota, overflowed into the inbox's event OG_CHECK (P2-21 F5)");

struct SimNetTransportStats
{
	uint64_t heldSends = 0u;
	uint64_t releasedSends = 0u;
	uint64_t directSends = 0u;
	uint64_t holdOverflows = 0u;
	uint64_t holdHighWaterBytes = 0u;
	uint64_t inputBundles = 0u;
	uint64_t inputBundlesRefused = 0u;
	uint64_t inputSets = 0u;
	uint64_t inputSetSplits = 0u;
	uint64_t inputSetsWithoutServer = 0u;
	uint64_t addressChangesIgnored = 0u;
	uint64_t unslottedRefused = 0u;
	uint64_t closingRefused = 0u;
	uint64_t controlQuotaDisconnects = 0u;
	uint64_t noSlotConnects = 0u;
	uint64_t unannouncedConnects = 0u;
	uint64_t unannouncedRefused = 0u;
	uint64_t unwindowedRefused = 0u;
	uint64_t clockSends = 0u;
};

static_assert(std::atomic<std::thread::id>::is_always_lock_free,
	"SimNetTransport: an immediate transport's input set belongs to the thread that opened it (beginInputSends), and every "
	"append and end reads that owner from one lock-free atomic, so a refused call from another thread never waits");

template <typename TransportT>
class SimNetChannelTransport
{
public:
	SimNetChannelTransport(TransportT& transport, uint8_t simId) : m_transport(&transport), m_simId(simId) {}

	void sendInputBundle(std::span<const std::byte> bundle) { m_transport->appendInputBundle(m_simId, bundle); }
	void endInputSends() { m_transport->endInputSends(); }

	uint8_t simId() const { return m_simId; }

private:
	TransportT* m_transport;
	uint8_t m_simId;
};

template <SimNetEndpoint EndpointT, size_t InboxN>
class SimNetTransport
{
public:
	using Endpoint = EndpointT;
	using Inbox = SimInbox<InboxN>;
	using ChannelTransport = SimNetChannelTransport<SimNetTransport>;

	explicit SimNetTransport(SimNetConfig config = {})
		: m_config(config)
		, m_inbox(simNetInboxConfig<InboxN>(config))
		, m_quotaOwner(std::make_unique<std::atomic<SimConnectionId>[]>(simNetConnectionSlots(config)))
		, m_closing(std::make_unique<bool[]>(simNetConnectionSlots(config)))
		, m_presence(std::make_unique<SlotPresence[]>(simNetConnectionSlots(config)))
		, m_hold(std::make_unique<std::byte[]>(kHoldArenaBytes))
	{
		OG_CHECK(config.controlReceiveDepth != 0u,
			"SimNetTransport: Control's receive depth sizes the Control ring. An uncapped Control lane (depth 0) has no bound, so no "
			"ring size keeps a Control message from being refused");
		OG_CHECK(simNetQuotaSlots<InboxN>(config) != 0u,
			"SimNetTransport: each connection's quota is (InboxN - 64) / maxConnections undrained messages, at least one; more "
			"connections need a larger inbox");
		OG_CHECK(config.connectsPerDrainGap != 0u,
			"SimNetTransport: the event ring keeps two slots for each connection that opens between two drains, so it is sized for "
			"at least one; with none, a client's ring could never hold a Connected and its Disconnected");
		std::fill_n(m_hold.get(), kHoldArenaBytes, std::byte{0});
	}

	SimNetTransport(const SimNetTransport&) = delete;
	SimNetTransport& operator=(const SimNetTransport&) = delete;
	SimNetTransport(SimNetTransport&&) = delete;
	SimNetTransport& operator=(SimNetTransport&&) = delete;

	void installHandlers(EndpointT& endpoint)
	{
		OG_CHECK(!m_endpointStarted.load(std::memory_order_acquire),
			"SimNetTransport::installHandlers: the handlers are installed before the endpoint starts (og-net takes lane handlers "
			"before start only); the host calls markEndpointStarted after start");
		OG_CHECK(m_endpoint == nullptr, "SimNetTransport::installHandlers: a transport installs its handlers once, on one endpoint");
		m_endpoint = &endpoint;
		for (size_t lane = 0u; lane < kSimLaneCount; ++lane)
		{
			const bool clockLane = static_cast<SimLane>(lane) == SimLane::SimClock && m_clockHandler.fn != nullptr;
			endpoint.setLaneHandler(static_cast<ognet::Lane>(lane),
				clockLane ? m_clockHandler : ognet::LaneHandler{&SimNetTransport::onLaneMessage, this});
		}
		// ⛔G-02  docs/SimNetTransport-guards.md

	}

	void markEndpointStarted() { m_endpointStarted.store(true, std::memory_order_release); }

	bool hasClockHandler() const { return m_clockHandler.fn != nullptr; }

	void onConnectionEvent(const ognet::ConnectionEvent& event)
	{
		switch (event.kind)
		{
		case ognet::ConnectionEventKind::Connected:
			onConnected(event.id);
			return;
		case ognet::ConnectionEventKind::Disconnected:
			onDisconnected(event.id);
			return;
		case ognet::ConnectionEventKind::AddressChanged:
			m_addressChangesIgnored.fetch_add(1u, std::memory_order_relaxed);
			return;
		}
	}

	ognet::ConnectionHandler connectionHandler() { return ognet::ConnectionHandler{&SimNetTransport::onConnectionEventThunk, this}; }

	Inbox& inbox() { return m_inbox; }
	const Inbox& inbox() const { return m_inbox; }
	const SimNetConfig& config() const { return m_config; }

	void setServerConnection(SimConnectionId id) { m_serverConnection.store(id, std::memory_order_release); }
	SimConnectionId serverConnection() const { return m_serverConnection.load(std::memory_order_acquire); }

	ChannelTransport channelTransport(uint8_t simId) { return ChannelTransport(*this, simId); }

	void beginInputSends()
	{
		if (!m_config.immediate)
		{
			return;
		}
		std::thread::id none{};
		const bool opened = m_inputSender.compare_exchange_strong(none, std::this_thread::get_id(), std::memory_order_acq_rel);
		OG_CHECK(opened,
			"SimNetTransport::beginInputSends: an immediate transport's input set is already open. The client session opens it "
			"once per captured tick (SimNetClient::afterCaptureTick_Step) and ends it before the next");
	}

	void appendInputBundle(uint8_t simId, std::span<const std::byte> bundle)
	{
		withInputSet([&] { appendToInputSet(simId, bundle); });
	}

	void endInputSends()
	{
		withInputSet([&] {
			emitInputSet();
			if (m_config.immediate)
			{
				m_inputSender.store(std::thread::id{}, std::memory_order_release);
			}
		});
	}

	void relay(SimConnectionId skipOwner, std::span<const std::byte> bytes) { submit(SendOp::BroadcastExcept, skipOwner, SimLane::SimRelay, bytes); }
	void sendState(SimConnectionId conn, std::span<const std::byte> bytes) { submit(SendOp::Send, conn, SimLane::SimState, bytes); }
	void sendControl(SimConnectionId conn, std::span<const std::byte> bytes) { submit(SendOp::Send, conn, SimLane::Control, bytes); }
	void broadcastClock(std::span<const std::byte> bytes) { submit(SendOp::BroadcastExcept, kInvalidSimConnection, SimLane::SimClock, bytes); }

	void flushFrame() { releaseHold(); }

	size_t heldBytes() const { return m_holdUsed; }

	uint16_t slotOf(SimConnectionId conn) const
	{
		const size_t slots = m_inbox.tagCount();
		for (size_t slot = 0u; slot < slots && conn != kInvalidSimConnection; ++slot)
		{
			if (m_quotaOwner[slot].load(std::memory_order_relaxed) == conn)
			{
				return static_cast<uint16_t>(slot);
			}
		}
		return kNoInboxTag;
	}

	std::array<uint64_t, kSimLaneCount> quotaRefused(SimConnectionId conn) const { return m_inbox.quotaRefusedFor(slotOf(conn)); }

	SimNetTransportStats stats() const
	{
		SimNetTransportStats out;
		out.heldSends = m_heldSends.load(std::memory_order_relaxed);
		out.releasedSends = m_releasedSends.load(std::memory_order_relaxed);
		out.directSends = m_directSends.load(std::memory_order_relaxed);
		out.holdOverflows = m_holdOverflows.load(std::memory_order_relaxed);
		out.holdHighWaterBytes = m_holdHighWaterBytes.load(std::memory_order_relaxed);
		out.inputBundles = m_inputBundles.load(std::memory_order_relaxed);
		out.inputBundlesRefused = m_inputBundlesRefused.load(std::memory_order_relaxed);
		out.inputSets = m_inputSets.load(std::memory_order_relaxed);
		out.inputSetSplits = m_inputSetSplits.load(std::memory_order_relaxed);
		out.inputSetsWithoutServer = m_inputSetsWithoutServer.load(std::memory_order_relaxed);
		out.addressChangesIgnored = m_addressChangesIgnored.load(std::memory_order_relaxed);
		out.unslottedRefused = m_unslottedRefused.load(std::memory_order_relaxed);
		out.closingRefused = m_closingRefused.load(std::memory_order_relaxed);
		out.controlQuotaDisconnects = m_controlQuotaDisconnects.load(std::memory_order_relaxed);
		out.noSlotConnects = m_noSlotConnects.load(std::memory_order_relaxed);
		out.unannouncedConnects = m_unannouncedConnects.load(std::memory_order_relaxed);
		out.unannouncedRefused = m_unannouncedRefused.load(std::memory_order_relaxed);
		out.unwindowedRefused = m_unwindowedRefused.load(std::memory_order_relaxed);
		out.clockSends = m_clockSends.load(std::memory_order_relaxed);
		return out;
	}

private:
	template <class, class>
	friend class SimNetClockServer;
	template <class, class>
	friend class SimNetClockClient;

	enum class SendOp : uint8_t { Send, BroadcastExcept };
	enum class SlotPresence : uint8_t { Silent, Announced, Unannounced };

	void setClockHandler(ognet::LaneHandler handler)
	{
		OG_CHECK(m_endpoint == nullptr && !m_endpointStarted.load(std::memory_order_acquire),
			"SimNetTransport::setClockHandler: the SimClock lane's own handler is set before installHandlers, which gives it the "
			"lane in place of the copy-only handler (og-net takes lane handlers before start only)");
		OG_CHECK(handler.fn != nullptr && m_clockHandler.fn == nullptr,
			"SimNetTransport::setClockHandler: a transport has one clock handler (a SimNetClockServer or a SimNetClockClient), set once");
		m_clockHandler = handler;
	}

	bool admitsClockFrom(SimConnectionId conn) { return admittedSlot(conn) != kNoInboxTag; }

	void receiveClock(SimConnectionId conn, double recvSeconds, std::span<const std::byte> bytes)
	{
		receive(conn, SimLane::SimClock, recvSeconds, bytes);
	}

	void sendClockNow(SimConnectionId conn, std::span<const std::byte> bytes)
	{
		dispatch(SendOp::Send, conn, SimLane::SimClock, bytes);
		count(m_clockSends);
	}

	template <class SetFn>
	void withInputSet(SetFn&& onSet)
	{
		const bool owned = !m_config.immediate || m_inputSender.load(std::memory_order_acquire) == std::this_thread::get_id();
		OG_CHECK(owned,
			"SimNetTransport: an immediate transport's input set is appended to and ended only on the thread that opened it, "
			"inside the client session's afterCaptureTick_Step on the step's thread. A send from anywhere else (the game "
			"thread's onPostSimulationGameThread send, or SimNetClient::flushFrame or endInputSends in immediate mode) would "
			"race the step's set");
		// ⛔G-07  docs/SimNetTransport-guards.md
		if (!owned)
		{
			count(m_unwindowedRefused);
			return;
		}
		onSet();
	}

	void appendToInputSet(uint8_t simId, std::span<const std::byte> bundle)
	{
		if (simId == 0u || bundle.size() < inputRedundancyBundle::kHeaderBytes
			|| 1u + bundle.size() > kMaxSimMessageBytes - simEnvelope::kInputBundleSetHeaderBytes)
		{
			count(m_inputBundlesRefused);
			return;
		}
		const std::span<std::byte> set(m_inputSet);
		if (m_inputSetUsed == 0u)
		{
			m_inputSetUsed = simEnvelope::writeInputBundleSetHeader(set);
		}
		size_t used = simEnvelope::appendInputBundle(set, m_inputSetUsed, simId, bundle);
		if (used == 0u)
		{
			emitInputSet();
			count(m_inputSetSplits);
			m_inputSetUsed = simEnvelope::writeInputBundleSetHeader(set);
			used = simEnvelope::appendInputBundle(set, m_inputSetUsed, simId, bundle);
		}
		m_inputSetUsed = used;
		count(used == 0u ? m_inputBundlesRefused : m_inputBundles);
	}

	struct HeldHeader
	{
		SendOp op = SendOp::Send;
		SimLane lane = SimLane::Control;
		SimConnectionId conn = kInvalidSimConnection;
		uint32_t size = 0u;
	};

	static void onLaneMessage(void* ctx, ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		SimNetTransport& self = *static_cast<SimNetTransport*>(ctx);
		// ⛔G-01  docs/SimNetTransport-guards.md
		self.receive(id, static_cast<SimLane>(lane), recvSeconds, bytes);
	}

	uint16_t admittedSlot(SimConnectionId conn)
	{
		const uint16_t slot = slotFor(conn);
		if (slot == kNoInboxTag)
		{
			count(m_unslottedRefused);
			return kNoInboxTag;
		}
		if (m_closing[slot])
		{
			count(m_closingRefused);
			return kNoInboxTag;
		}
		if (m_presence[slot] == SlotPresence::Unannounced)
		{
			count(m_unannouncedRefused);
			return kNoInboxTag;
		}
		return slot;
	}

	void receive(SimConnectionId conn, SimLane lane, double recvSeconds, std::span<const std::byte> bytes)
	{
		const uint16_t slot = admittedSlot(conn);
		if (slot == kNoInboxTag)
		{
			return;
		}
		if (m_inbox.push(conn, lane, recvSeconds, bytes, slot) == SimInboxPush::ControlQuotaRefused)
		{
			m_closing[slot] = true;
			count(m_controlQuotaDisconnects);
			m_endpoint->disconnect(conn);
		}
	}

	void onConnected(SimConnectionId conn)
	{
		const uint16_t slot = conn == kInvalidSimConnection ? kNoInboxTag : slotFor(conn);
		const bool slotted = conn == kInvalidSimConnection || slot != kNoInboxTag;
		OG_CHECK(slotted,
			"SimNetTransport: a Connected found every connection slot taken. The table has one slot per connection the endpoint "
			"admits, so the transport's config was not built from its endpoint's (simNetConfigFor)");
		// ⛔G-06  docs/SimNetTransport-guards.md
		if (!slotted)
		{
			count(m_noSlotConnects);
			if (m_endpoint != nullptr)
			{
				m_endpoint->disconnect(conn);
			}
		}
		if (slot == kNoInboxTag || m_presence[slot] == SlotPresence::Announced)
		{
			m_inbox.pushEvent(InboundKind::Connected, conn, kNoReceiveSeconds);
			return;
		}
		if (m_inbox.pushPairedConnected(conn, kNoReceiveSeconds) == SimInboxPush::Pushed)
		{
			m_presence[slot] = SlotPresence::Announced;
			return;
		}
		m_presence[slot] = SlotPresence::Unannounced;
		count(m_unannouncedConnects);
		if (m_endpoint != nullptr)
		{
			m_endpoint->disconnect(conn);
		}
	}

	void onDisconnected(SimConnectionId conn)
	{
		const uint16_t slot = slotOf(conn);
		if (slot == kNoInboxTag)
		{
			m_inbox.pushEvent(InboundKind::Disconnected, conn, kNoReceiveSeconds);
			return;
		}
		const InboundCause cause = m_closing[slot] ? InboundCause::ControlQuota : InboundCause::Unstated;
		if (m_presence[slot] == SlotPresence::Announced)
		{
			m_inbox.pushPairedDisconnected(conn, kNoReceiveSeconds, cause);
		}
		else if (m_presence[slot] == SlotPresence::Silent)
		{
			m_inbox.pushEvent(InboundKind::Disconnected, conn, kNoReceiveSeconds, cause);
		}
		m_closing[slot] = false;
		m_presence[slot] = SlotPresence::Silent;
		m_quotaOwner[slot].store(kInvalidSimConnection, std::memory_order_relaxed);
	}

	uint16_t slotFor(SimConnectionId conn)
	{
		const uint16_t owned = slotOf(conn);
		if (owned != kNoInboxTag || conn == kInvalidSimConnection)
		{
			return owned;
		}
		const size_t slots = m_inbox.tagCount();
		size_t best = slots;
		uint64_t bestResidue = 0u;
		for (size_t slot = 0u; slot < slots; ++slot)
		{
			const uint16_t tag = static_cast<uint16_t>(slot);
			const uint64_t residue = uint64_t{m_inbox.undrained(tag)} + m_inbox.controlUndrained(tag);
			if (m_quotaOwner[slot].load(std::memory_order_relaxed) == kInvalidSimConnection && (best == slots || residue < bestResidue))
			{
				best = slot;
				bestResidue = residue;
			}
		}
		if (best == slots)
		{
			return kNoInboxTag;
		}
		m_inbox.resetTagCounters(static_cast<uint16_t>(best));
		m_closing[best] = false;
		m_quotaOwner[best].store(conn, std::memory_order_relaxed);
		return static_cast<uint16_t>(best);
	}

	static void onConnectionEventThunk(void* ctx, const ognet::ConnectionEvent& event)
	{
		static_cast<SimNetTransport*>(ctx)->onConnectionEvent(event);
	}

	static void count(std::atomic<uint64_t>& counter) { counter.fetch_add(1u, std::memory_order_relaxed); }

	void emitInputSet()
	{
		const size_t used = m_inputSetUsed;
		m_inputSetUsed = 0u;
		if (used <= simEnvelope::kInputBundleSetHeaderBytes)
		{
			return;
		}
		const SimConnectionId server = serverConnection();
		if (server == kInvalidSimConnection)
		{
			count(m_inputSetsWithoutServer);
			return;
		}
		count(m_inputSets);
		submit(SendOp::Send, server, SimLane::SimInput, std::span<const std::byte>(m_inputSet.data(), used));
	}

	void submit(SendOp op, SimConnectionId conn, SimLane lane, std::span<const std::byte> bytes)
	{
		if (m_config.immediate)
		{
			transmit(op, conn, lane, bytes);
			return;
		}
		const size_t need = sizeof(HeldHeader) + bytes.size();
		if (need > kHoldArenaBytes - m_holdUsed)
		{
			count(m_holdOverflows);
			// ⛔G-03  docs/SimNetTransport-guards.md
			releaseHold();
			transmit(op, conn, lane, bytes);
			return;
		}
		const HeldHeader header{op, lane, conn, static_cast<uint32_t>(bytes.size())};
		std::memcpy(m_hold.get() + m_holdUsed, &header, sizeof(header));
		if (!bytes.empty())
		{
			std::memcpy(m_hold.get() + m_holdUsed + sizeof(header), bytes.data(), bytes.size());
		}
		m_holdUsed += need;
		count(m_heldSends);
		if (m_holdUsed > m_holdHighWaterBytes.load(std::memory_order_relaxed))
		{
			m_holdHighWaterBytes.store(m_holdUsed, std::memory_order_relaxed);
		}
	}

	void releaseHold()
	{
		size_t offset = 0u;
		while (offset < m_holdUsed)
		{
			HeldHeader header;
			std::memcpy(&header, m_hold.get() + offset, sizeof(header));
			const std::span<const std::byte> bytes(m_hold.get() + offset + sizeof(header), header.size);
			dispatch(header.op, header.conn, header.lane, bytes);
			count(m_releasedSends);
			offset += sizeof(header) + header.size;
		}
		m_holdUsed = 0u;
	}

	void transmit(SendOp op, SimConnectionId conn, SimLane lane, std::span<const std::byte> bytes)
	{
		dispatch(op, conn, lane, bytes);
		count(m_directSends);
	}

	void dispatch(SendOp op, SimConnectionId conn, SimLane lane, std::span<const std::byte> bytes)
	{
		OG_CHECK(m_endpoint != nullptr, "SimNetTransport: a send needs an endpoint, which installHandlers binds");
		// ⛔G-04  docs/SimNetTransport-guards.md
		if (m_endpoint == nullptr)
		{
			return;
		}
		if (op == SendOp::Send)
		{
			m_endpoint->send(conn, static_cast<ognet::Lane>(lane), bytes);
		}
		else
		{
			m_endpoint->broadcastExcept(conn, static_cast<ognet::Lane>(lane), bytes);
		}
	}

	SimNetConfig m_config;
	EndpointT* m_endpoint = nullptr;
	ognet::LaneHandler m_clockHandler{};
	std::atomic<bool> m_endpointStarted{false};
	Inbox m_inbox;
	std::unique_ptr<std::atomic<SimConnectionId>[]> m_quotaOwner;
	std::unique_ptr<bool[]> m_closing;
	std::unique_ptr<SlotPresence[]> m_presence;
	std::atomic<SimConnectionId> m_serverConnection{kInvalidSimConnection};

	std::unique_ptr<std::byte[]> m_hold;
	size_t m_holdUsed = 0u;
	std::array<std::byte, kMaxSimMessageBytes> m_inputSet{};
	size_t m_inputSetUsed = 0u;
	std::atomic<std::thread::id> m_inputSender{};

	std::atomic<uint64_t> m_heldSends{0};
	std::atomic<uint64_t> m_releasedSends{0};
	std::atomic<uint64_t> m_directSends{0};
	std::atomic<uint64_t> m_holdOverflows{0};
	std::atomic<uint64_t> m_holdHighWaterBytes{0};
	std::atomic<uint64_t> m_inputBundles{0};
	std::atomic<uint64_t> m_inputBundlesRefused{0};
	std::atomic<uint64_t> m_inputSets{0};
	std::atomic<uint64_t> m_inputSetSplits{0};
	std::atomic<uint64_t> m_inputSetsWithoutServer{0};
	std::atomic<uint64_t> m_addressChangesIgnored{0};
	std::atomic<uint64_t> m_unslottedRefused{0};
	std::atomic<uint64_t> m_closingRefused{0};
	std::atomic<uint64_t> m_controlQuotaDisconnects{0};
	std::atomic<uint64_t> m_noSlotConnects{0};
	std::atomic<uint64_t> m_unannouncedConnects{0};
	std::atomic<uint64_t> m_unannouncedRefused{0};
	std::atomic<uint64_t> m_unwindowedRefused{0};
	std::atomic<uint64_t> m_clockSends{0};
};

namespace simNetTransportSelfCheck
{
struct ReferenceEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>);
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>);
	void setLaneHandler(ognet::Lane, ognet::LaneHandler);
	void setConnectionHandler(ognet::ConnectionHandler);
	void disconnect(ognet::ConnectionId);
};

using ReferenceTransport = SimNetTransport<ReferenceEndpoint, 128>;

static_assert(SimNetEndpoint<ReferenceEndpoint>, "SimNetEndpoint self-check: the reference shape must satisfy the concept");
static_assert(ServerSimTransport<ReferenceTransport>,
	"SimNetTransport: the transport is the server session's ServerSimTransport. Was SimNetTransport-rationale.md §6");
static_assert(ClientSimTransport<ReferenceTransport::ChannelTransport>,
	"SimNetChannelTransport: a channel's per-character adapter is a ClientSimTransport whose bundles carry no sim id; the "
	"adapter adds it. Was SimNetTransport-rationale.md §5");
static_assert(!ClientSimTransport<ReferenceTransport>,
	"SimNetTransport: the transport's own bundle append takes a sim id, so a channel must hold its adapter, never the "
	"transport. Was SimNetTransport-rationale.md §5");
} // namespace simNetTransportSelfCheck
} // namespace ogsimnet
