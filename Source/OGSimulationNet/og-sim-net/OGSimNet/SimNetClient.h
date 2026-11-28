#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetClient-rationale.md · docs/SimNetClient-guards.md

#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/Network/CorrectionStaleFilter.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/SimulatableList.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ogsimnet
{
template <class T>
concept SimNetClientTransport = requires(T& transport, uint8_t simId, SimConnectionId conn)
{
	transport.inbox().drain([](const InboundSimRecord&) {});
	{ transport.inbox().stats() } -> std::same_as<SimInboxStats>;
	{ transport.channelTransport(simId) } -> ClientSimTransport;
	{ transport.setServerConnection(conn) } -> std::same_as<void>;
	{ transport.serverConnection() } -> std::same_as<SimConnectionId>;
};

template <class T>
concept SimNetInputWindowTransport = requires(T& transport)
{
	{ transport.beginInputSends() } -> std::same_as<void>;
	{ transport.endInputSends() } -> std::same_as<void>;
};

template <class T>
concept SimNetInputSender = requires(T& sender, uint32_t tick, uint32_t redundancyDepthTicks, int32_t redundancyFrames)
{
	sender.sendLocalInputToAuthorityAll(tick, redundancyDepthTicks, redundancyFrames);
};

template <class List>
inline constexpr size_t kSimulatableCount = 0u;

template <class... SimTs>
inline constexpr size_t kSimulatableCount<SimulatableList<SimTs...>> = sizeof...(SimTs);

template <class List>
struct FirstSimulatable;

template <class FirstT, class... RestTs>
struct FirstSimulatable<SimulatableList<FirstT, RestTs...>>
{
	using type = FirstT;
};

struct SimNetDisconnectRequest
{
	SimConnectionId conn = kInvalidSimConnection;
	RosterRefusal reason = RosterRefusal::None;
};

struct SimNetConnectionTierLatch
{
	uint8_t oldTier = 0u;
	uint8_t newTier = 0u;
};

struct SimNetClientStats
{
	uint64_t drains = 0u;
	uint64_t messages = 0u;
	uint64_t badDecode = 0u;
	uint64_t unknownSim = 0u;
	uint64_t rosterApplied = 0u;
	uint64_t rosterRefused = 0u;
	uint64_t ignoredWhileRefused = 0u;
	uint64_t ringsStored = 0u;
	uint64_t ringsHeld = 0u;
	uint64_t heldRingsApplied = 0u;
	uint64_t correctionsStored = 0u;
	uint64_t mismatchIgnored = 0u;
	uint64_t clocksDelivered = 0u;
	uint64_t clocksCoalesced = 0u;
	uint64_t clockSamples = 0u;
	uint64_t clockSamplesUnheard = 0u;
	uint64_t floorsReceived = 0u;
	uint64_t tiersReceived = 0u;
	uint64_t emptyBundles = 0u;
	uint64_t connects = 0u;
	uint64_t disconnects = 0u;
	uint64_t quotaDisconnects = 0u;
};

inline constexpr char kReceiveCorrectionStateFormat[] = "[ReceiveCorrectionState] id=%u tick=%u";
inline constexpr size_t kReceiveCorrectionStateLineBytes = 64u;

static_assert(kReceiveCorrectionStateLineBytes >= sizeof(kReceiveCorrectionStateFormat) + 3u + 10u,
	"kReceiveCorrectionStateLineBytes must hold the [ReceiveCorrectionState] line with a 3-digit sim id and a 10-digit tick");

inline constexpr char kSimNetLifecycleRefusedFormat[] = "[Warning][SimNet.Lifecycle] role=client simId=%u refused=%s";
inline constexpr size_t kSimNetLifecycleRefusedLineBytes = 80u;

static_assert(kSimNetLifecycleRefusedLineBytes >= sizeof(kSimNetLifecycleRefusedFormat) + 3u + sizeof("registered"),
	"kSimNetLifecycleRefusedLineBytes must hold the client's [SimNet.Lifecycle] line with a 3-digit sim id and the longest "
	"reason (registered)");

inline constexpr char kSimNetClientWindowFormat[] =
	"[Warning][SimNet.Window] role=client drains=%llu msgs=%llu maxDepth=%llu drops=%llu tooLarge=%llu badDecode=%llu "
	"refusedNotOwner=%llu unknownSim=%llu statesSent=%llu relaysSent=%llu overBudget=%llu staleDropped=%llu "
	"duplicateDropped=%llu quotaRefused=%llu controlHighWater=%llu quotaDisconnects=%llu eventsRefused=%llu eventHighWater=%llu";

static_assert(simNetWindowCounterCount(kSimNetClientWindowFormat) == 18u
		&& kSimNetWindowLineBytes >= sizeof(kSimNetClientWindowFormat) + simNetWindowCounterCount(kSimNetClientWindowFormat) * 20u,
	"kSimNetWindowLineBytes must hold the client's [SimNet.Window] line with each of its eighteen counters at 20 digits");

inline constexpr char kClockSourceHeldTag[] = "[ClockSource.Held]";
inline constexpr char kClockSourceHeldFormat[] =
	"[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick %u: "
	"rttMs=%.3f heldMs=%.1f";
inline constexpr size_t kClockSourceHeldLineBytes = 192u;

static_assert(std::string_view(kClockSourceHeldFormat).starts_with(kClockSourceHeldTag),
	"kClockSourceHeldFormat begins with kClockSourceHeldTag: a host routes the line to its category and level by that tag");
static_assert(kClockSourceHeldLineBytes >= sizeof(kClockSourceHeldFormat) + 10u + 2u * 20u,
	"kClockSourceHeldLineBytes must hold the [ClockSource.Held] line with a 10-digit tick and each reading at 20 characters "
	"(any reading below 1e15 ms)");

inline std::array<char, kClockSourceHeldLineBytes> formatClockSourceHeld(uint32_t authorityTick, double rttMs, double heldMs)
{
	std::array<char, kClockSourceHeldLineBytes> line{};
	std::snprintf(line.data(), line.size(), kClockSourceHeldFormat, static_cast<unsigned>(authorityTick), rttMs, heldMs);
	return line;
}

template <SimNetGame Game, SimNetClientHost HostT, SimNetClientTransport TransportT>
class SimNetClient
{
	using Sims = typename Game::Simulatables;

	static_assert(kSimulatableCount<Sims> == 1u,
		"SimNetClient: the game has exactly one simulatable type. The client creates a character's channel from its roster "
		"entry, and roster v1 carries no simulatable type, so a second type could not be told apart. Was "
		"SimNetClient-rationale.md §2");

public:
	using SimT = typename FirstSimulatable<Sims>::type;
	using Shape = simEnvelope::ShapeFor<typename SimT::InputType, typename SimT::StateType>;
	using ChannelTransport = decltype(std::declval<TransportT&>().channelTransport(uint8_t{}));
	using Channel = SessionChannel<SimT, ChannelTransport>;
	using InputProvider = std::function<typename SimT::InputType(
		const SimulationTimeStep&, const LocalInputCache<typename SimT::InputType>&)>;
	using LifecycleParts = SimNetLifecycleParts<Sims>;

	static_assert(kIsSessionChannel<Channel, SimT, ChannelTransport>,
		"SimNetClient: a character's channel is SessionChannel<SimT, ChannelTransport>, the type SimNetServer::Channel<SimT> is "
		"for the same transport, so one SimulatableOwnerTraits owner type serves both roles. Was SimNetClientChannelTransport, "
		"the client's own wrapper");
	static_assert(SimNetDeliveryHookWellFormed<HostT>,
		"SimNetClient: the host's noteDelivered member, if it has one, must be callable as noteDelivered(SimLane, double "
		"recvSeconds) -> void; any other shape would never be called");
	static_assert(SimNetLifecycleUnregisteringHookWellFormed<HostT>,
		"SimNetClient: the host's onLifecycleUnregistering member, if it has one, must be callable as "
		"onLifecycleUnregistering(uint8_t simId) -> void; any other shape would never be called, and a queue-registered "
		"character's removal would leave the game's systems pointing at freed storage");
	static_assert(SimNetClockRttSampleHookWellFormed<HostT>,
		"SimNetClient: the host's onClockSample member, if it has one, must be callable as onClockSample(const "
		"SimClockRttSample&) -> void; any other shape would never be called, and the clock would stay on the engine ping");

	SimNetClient(HostT& host, TransportT& transport)
		: m_host(host)
		, m_transport(transport)
	{
		m_mirror.setAddedCallback([this](const RosterEntry& entry) { onRosterAdded(entry); });
		m_mirror.setRemovedCallback([this](const RosterEntry& entry) { onRosterRemoved(entry); });
	}

	SimNetClient(const SimNetClient&) = delete;
	SimNetClient& operator=(const SimNetClient&) = delete;
	SimNetClient(SimNetClient&&) = delete;
	SimNetClient& operator=(SimNetClient&&) = delete;

	size_t drain()
	{
		applyListenerRequest();
		// ⛔G-11  docs/SimNetClient-guards.md
		m_lifecycle.applyPending();
		const size_t handled = m_transport.inbox().drain([this](const InboundSimRecord& record) { onRecord(record); });
		deliverNewestClock();
		++m_stats.drains;
		++m_window.drains;
		m_window.maxDepth = std::max<uint64_t>(m_window.maxDepth, handled);
		return handled;
	}

	template <SimNetInputSender SenderT>
	void afterCaptureTick_Step(SenderT& sender, uint32_t tick, const TimeConfig& timeConfig)
		requires SimNetInputWindowTransport<TransportT>
	{
		m_transport.beginInputSends();
		sender.sendLocalInputToAuthorityAll(
			tick, static_cast<uint32_t>(timeConfig.redundancyDepthTicks), timeConfig.redundancyFrames);
		m_transport.endInputSends();
	}

	void flushFrame() { maybeEmitWindow(); }

	void bindListener()
	{
		// ⛔G-10  docs/SimNetClient-guards.md
		m_listenerRequest.store(ListenerRequest::Bind, std::memory_order_release);
	}

	void unbindListener() { m_listenerRequest.store(ListenerRequest::Unbind, std::memory_order_release); }

	void bindLifecycle(SimulationObjectStorage<SimT>& storage, SimulationReconciliation<SimT>& reconciliation,
		SimulationInputResolution<SimT>& inputResolution, SimulationNetSync<SimT>& netSync)
	{
		OG_CHECK(!m_lifecycleParts.bound(),
			"SimNetClient::bindLifecycle: a session applies its queued registrations to one simulation, bound once before the "
			"first registerCharacter; a second bind would re-point registrations already applied to the first");
		m_lifecycleParts = LifecycleParts{&storage, &reconciliation, &inputResolution, &netSync};
	}

	template <SimNetLifecycleHook<Channel> AppliedFn = SimNetNoLifecycleHook>
	void registerCharacter(uint8_t simId, SimT&& simulatable, InputProvider provider, AppliedFn applied = AppliedFn{})
	{
		OG_CHECK(m_lifecycleParts.bound(),
			"SimNetClient::registerCharacter: bindLifecycle first; the drain applies the registration through "
			"registerSimulatable on the bound storage, reconciliation, input resolution and net sync");
		m_lifecycle.enqueue([this, simId, simulatable = std::move(simulatable), provider = std::move(provider),
								applied = std::move(applied)]() mutable {
			applyRegister(simId, std::move(simulatable), std::move(provider), applied);
		});
	}

	template <SimNetLifecycleHook<Channel> AppliedFn = SimNetNoLifecycleHook>
	void unregisterCharacter(uint8_t simId, AppliedFn applied = AppliedFn{})
	{
		m_lifecycle.enqueue([this, simId, applied = std::move(applied)]() mutable { applyUnregister(simId, applied); });
	}

	bool lifecycleRegistered(uint8_t simId) const { return m_lifecycleRegistered.test(simId); }
	const SimNetLifecycleStats& lifecycleStats() const { return m_lifecycleStats; }

	Channel* channel(uint8_t simId)
	{
		const auto found = m_channels.find(simId);
		return found == m_channels.end() ? nullptr : found->second.get();
	}

	std::optional<SimNetDisconnectRequest> takeDisconnectRequest() { return std::exchange(m_disconnectRequest, std::nullopt); }

	const RosterMirror& roster() const { return m_mirror; }
	SimConnectionId serverConnection() const { return m_serverConnection; }
	bool refused() const { return m_refused; }
	bool listenerBound() const { return m_listenerBound; }
	bool wireMismatchLatched() const { return m_wireMismatch; }
	std::optional<uint8_t> latchedFloor() const { return m_floor; }
	std::optional<SimNetConnectionTierLatch> latchedTier() const { return m_tier; }
	std::optional<SimClockRttSample> heldClockSample() const { return m_heldClockSample; }
	size_t heldRingCount() const { return m_heldRings.size(); }
	const CorrectionStaleFilter& staleFilter() const { return m_staleFilter; }
	const NewestTickFilter& clockFilter() const { return m_clockFilter; }
	const SimNetClientStats& stats() const { return m_stats; }

private:
	enum class ListenerRequest : uint8_t
	{
		None,
		Bind,
		Unbind,
	};

	static_assert(std::atomic<ListenerRequest>::is_always_lock_free,
		"SimNetClient: the listener request crosses from the binding thread to the drain as one lock-free atomic");

	void applyBind()
	{
		if (m_listenerBound)
			return;
		m_listenerBound = true;
		if (m_floor)
			m_host.onRelayFloor(*m_floor, true);
		if (m_tier)
			m_host.onConnectionTier(m_tier->oldTier, m_tier->newTier, true);
	}

	void applyListenerRequest()
	{
		switch (m_listenerRequest.exchange(ListenerRequest::None, std::memory_order_acquire))
		{
		case ListenerRequest::None:
			return;
		case ListenerRequest::Bind:
			applyBind();
			return;
		case ListenerRequest::Unbind:
			m_listenerBound = false;
			return;
		}
	}

	struct WindowCounters
	{
		uint64_t drains = 0u;
		uint64_t messages = 0u;
		uint64_t maxDepth = 0u;
		uint64_t badDecode = 0u;
		uint64_t unknownSim = 0u;
		uint64_t quotaDisconnects = 0u;
	};

	static uint64_t droppedTotal(const SimInboxStats& stats)
	{
		uint64_t total = 0u;
		for (const uint64_t dropped : stats.dropped)
			total += dropped;
		return total;
	}

	static uint64_t quotaRefusedTotal(const SimInboxStats& stats)
	{
		uint64_t total = 0u;
		for (const uint64_t refused : stats.quotaRefused)
			total += refused;
		return total;
	}

	static RosterRefusal rosterRefusalOf(DecodeRefusal refusal)
	{
		return refusal == DecodeRefusal::Overlong ? RosterRefusal::Overlong : RosterRefusal::Truncated;
	}

	uint64_t staleDroppedTotal() const { return m_staleFilter.staleDropped() + m_clockFilter.staleDropped(); }
	uint64_t duplicateDroppedTotal() const { return m_staleFilter.duplicateDropped() + m_clockFilter.duplicateDropped(); }

	void onRecord(const InboundSimRecord& record)
	{
		switch (record.kind)
		{
		case InboundKind::Connected:
			onConnected(record.conn);
			return;
		case InboundKind::Disconnected:
			if (record.cause == InboundCause::ControlQuota)
			{
				++m_stats.quotaDisconnects;
				++m_window.quotaDisconnects;
				m_host.log(formatQuotaDisconnect("client", record.conn).data());
			}
			onDisconnected(record.conn);
			return;
		case InboundKind::Message:
			++m_stats.messages;
			++m_window.messages;
			if constexpr (SimNetDeliveryHook<HostT>)
				m_host.noteDelivered(record.lane, record.recvSeconds);
			onMessage(record);
			return;
		}
	}

	void onConnected(SimConnectionId conn)
	{
		if (conn == kInvalidSimConnection)
			return;
		++m_stats.connects;
		m_serverConnection = conn;
		m_refused = false;
		m_transport.setServerConnection(conn);
	}

	void onDisconnected(SimConnectionId conn)
	{
		if (conn != kInvalidSimConnection && conn != m_serverConnection)
			return;
		++m_stats.disconnects;
		m_serverConnection = kInvalidSimConnection;
		m_transport.setServerConnection(kInvalidSimConnection);
	}

	void onMessage(const InboundSimRecord& record)
	{
		// ⛔G-04  docs/SimNetClient-guards.md
		if (m_refused)
		{
			++m_stats.ignoredWhileRefused;
			return;
		}
		if (const std::optional<SimClockRttSample> sample = simNetClock::readSample(record))
		{
			onClockSample(*sample);
			return;
		}
		const DecodedSimMessage message = simEnvelope::decode<Shape>(record.lane, SimDirection::ServerToClient, record.payload());
		if (message.refusal != DecodeRefusal::None)
		{
			if (message.kind == SimMessageKind::Roster && record.lane == SimLane::Control)
				refuseRoster(rosterRefusalOf(message.refusal));
			else
				countBadDecode();
			return;
		}
		switch (message.kind)
		{
		case SimMessageKind::Roster:
			onRoster(message.body);
			return;
		case SimMessageKind::RelayRing:
			onRing(record, message);
			return;
		case SimMessageKind::CorrectionState:
			onCorrection(record, message);
			return;
		case SimMessageKind::ClockPublish:
			onClock(message.tick);
			return;
		case SimMessageKind::RelayFloor:
			onFloor(message.value);
			return;
		case SimMessageKind::ConnectionTier:
			onTier(message.value);
			return;
		default:
			countBadDecode();
			return;
		}
	}

	void onRoster(std::span<const std::byte> body)
	{
		const RosterApplyResult result = m_mirror.apply(body);
		if (!result.applied())
		{
			refuseRoster(result.refusal);
			return;
		}
		++m_stats.rosterApplied;
	}

	void refuseRoster(RosterRefusal refusal)
	{
		++m_stats.rosterRefused;
		m_host.log(formatRosterRefused(refusal).data());
		m_refused = true;
		m_disconnectRequest = SimNetDisconnectRequest{m_serverConnection, refusal};
		// ⛔G-03  docs/SimNetClient-guards.md
		m_transport.setServerConnection(kInvalidSimConnection);
	}

	void onRosterAdded(const RosterEntry& entry)
	{
		const uint8_t simId = entry.simId;
		OG_CHECK(m_channels.find(simId) == m_channels.end(),
			"SimNetClient: the roster mirror added a sim id that already has a channel; the mirror refuses an Add of a live "
			"entry (AddLive), so its added and removed callbacks must have gone out of step");
		// ⛔G-08  docs/SimNetClient-guards.md
		if (m_channels.find(simId) != m_channels.end())
			return;
		m_everListed.set(simId);
		auto created = std::make_unique<Channel>(
			SimNetBundleTransport<ChannelTransport>(m_transport.channelTransport(simId), m_stats.emptyBundles), simId,
			"sim" + std::to_string(simId));
		Channel& added = *created;
		m_channels.emplace(simId, std::move(created));
		if (const auto held = m_heldRings.find(simId); held != m_heldRings.end())
		{
			if (added.storeRing(held->second.payload()))
				++m_stats.heldRingsApplied;
			else
				countBadDecode();
			m_heldRings.erase(held);
		}
		m_host.onRosterAdded(simId);
	}

	void onRosterRemoved(const RosterEntry& entry)
	{
		const uint8_t simId = entry.simId;
		// ⛔G-12  docs/SimNetClient-guards.md
		if (unregisterLifecycle(simId))
			++m_lifecycleStats.removalUnregisters;
		m_lifecycleRetired.reset(simId);
		m_host.onRosterRemoved(simId);
		m_staleFilter.forget(simId);
		m_channels.erase(simId);
		m_heldRings.erase(simId);
	}

	void onRing(const InboundSimRecord& record, const DecodedSimMessage& message)
	{
		Channel* const target = arrivalChannel(message.simId);
		if (target == nullptr)
		{
			holdRing(message.simId, message.body);
			return;
		}
		if (!target->storeRing(message.body))
		{
			countBadDecode();
			return;
		}
		++m_stats.ringsStored;
		target->getRelayedInputRing().template forEachEntry<typename SimT::InputType>(
			[&](uint32_t captureTick, uint8_t, const typename SimT::InputType&) {
				if (captureTick != kNoInputCaptureTick)
					m_host.stampClientReceived(SimNetStream::Input, message.simId, captureTick, record.recvSeconds);
			});
	}

	void holdRing(uint8_t simId, std::span<const std::byte> ring)
	{
		const bool known = m_heldRings.find(simId) != m_heldRings.end();
		// ⛔G-07  docs/SimNetClient-guards.md
		if (m_everListed.test(simId) || (!known && m_heldRings.size() >= kRosterMaxEntries))
		{
			countUnknownSim();
			return;
		}
		if (!m_heldRings[simId].assignFromWire(ring))
		{
			m_heldRings.erase(simId);
			countBadDecode();
			return;
		}
		++m_stats.ringsHeld;
	}

	void onCorrection(const InboundSimRecord& record, const DecodedSimMessage& message)
	{
		// ⛔G-01  docs/SimNetClient-guards.md
		if (m_wireMismatch)
		{
			++m_stats.mismatchIgnored;
			return;
		}
		if (message.corrVersion != correctionStateBuffer::kWireFormatVersion)
		{
			m_wireMismatch = true;
			m_host.onWireMismatch(message.corrVersion, correctionStateBuffer::kWireFormatVersion);
			return;
		}
		Channel* const target = arrivalChannel(message.simId);
		if (target == nullptr)
		{
			countUnknownSim();
			return;
		}
		// ⛔G-02  docs/SimNetClient-guards.md
		if (!m_staleFilter.admit(message.simId, message.tick))
			return;
		std::array<char, kReceiveCorrectionStateLineBytes> line{};
		std::snprintf(line.data(), line.size(), kReceiveCorrectionStateFormat, static_cast<unsigned>(message.simId),
			static_cast<unsigned>(message.tick));
		m_host.log(line.data());
		m_host.stampClientReceived(SimNetStream::State, message.simId, message.tick, record.recvSeconds);
		if (!target->storeCorrection(message.body))
		{
			countBadDecode();
			return;
		}
		++m_stats.correctionsStored;
	}

	void onClock(uint32_t tick)
	{
		if (!m_clockFilter.admit(tick))
			return;
		if (m_pendingClock)
			++m_stats.clocksCoalesced;
		// ⛔G-09  docs/SimNetClient-guards.md
		m_pendingClock = tick;
	}

	void onClockSample(const SimClockRttSample& sample)
	{
		++m_stats.clockSamples;
		if constexpr (SimNetClockRttSampleHook<HostT>)
		{
			if (!m_listenerBound)
			{
				m_heldClockSample = sample;
				return;
			}
			m_heldClockSample.reset();
			m_host.onClockSample(sample);
		}
		else
		{
			++m_stats.clockSamplesUnheard;
		}
	}

	void deliverNewestClock()
	{
		const std::optional<uint32_t> tick = std::exchange(m_pendingClock, std::nullopt);
		if (!tick)
			return;
		++m_stats.clocksDelivered;
		const double roundTripSeconds = m_host.serverRttSeconds();
		// ⛔G-14  docs/SimNetClient-guards.md
		const std::optional<SimClockRttSample> held = feedHeldClockSample();
		m_host.onTimingInfo(*tick, roundTripSeconds);
		if (held)
			logHeldClockSample(*tick, *held);
	}

	std::optional<SimClockRttSample> feedHeldClockSample()
	{
		if constexpr (SimNetClockRttSampleHook<HostT>)
		{
			if (!m_listenerBound || !m_heldClockSample)
				return std::nullopt;
			const SimClockRttSample held = *std::exchange(m_heldClockSample, std::nullopt);
			m_host.onClockSample(held);
			return held;
		}
		else
		{
			return std::nullopt;
		}
	}

	void logHeldClockSample(uint32_t authorityTick, const SimClockRttSample& held)
	{
		m_host.log(formatClockSourceHeld(
			authorityTick, held.rttSeconds * 1000.0, (m_host.nowSeconds() - held.t3Seconds) * 1000.0).data());
	}

	void onFloor(uint8_t floorTicks)
	{
		++m_stats.floorsReceived;
		m_floor = floorTicks;
		if (m_listenerBound)
			m_host.onRelayFloor(floorTicks, false);
	}

	void onTier(uint8_t tier)
	{
		++m_stats.tiersReceived;
		const uint8_t oldTier = m_tier ? m_tier->newTier : uint8_t{0u};
		m_tier = SimNetConnectionTierLatch{oldTier, tier};
		if (m_listenerBound)
			m_host.onConnectionTier(oldTier, tier, false);
	}

	Channel* arrivalChannel(uint8_t simId) { return m_lifecycleRetired.test(simId) ? nullptr : channel(simId); }

	void refuseRegister(uint8_t simId, const char* reason)
	{
		++m_lifecycleStats.registersRefused;
		std::array<char, kSimNetLifecycleRefusedLineBytes> line{};
		std::snprintf(line.data(), line.size(), kSimNetLifecycleRefusedFormat, static_cast<unsigned>(simId), reason);
		m_host.log(line.data());
	}

	template <class AppliedFn>
	void applyRegister(uint8_t simId, SimT&& simulatable, InputProvider provider, AppliedFn& applied)
	{
		static_assert(std::is_same_v<PredictionOwnerFor<SimT>, Channel>,
			"SimNetClient::registerCharacter: the game's SimulatableOwnerTraits must name the session's Channel as the prediction "
			"owner, because the drain registers the channel the roster created as the character's owner");
		Channel* const target = channel(simId);
		if (target == nullptr || m_lifecycleRegistered.test(simId))
		{
			refuseRegister(simId, target == nullptr ? "noChannel" : "registered");
			applied(static_cast<Channel*>(nullptr));
			return;
		}
		const LifecycleParts& parts = m_lifecycleParts;
		::registerSimulatable<SimT>(*parts.storage, *parts.reconciliation, *parts.inputResolution, *parts.netSync, simId,
			std::move(simulatable), *target, std::move(provider));
		m_lifecycleRegistered.set(simId);
		m_lifecycleRetired.reset(simId);
		m_unregisterLifecycle = &unregisterAs;
		++m_lifecycleStats.registersApplied;
		applied(target);
	}

	template <class AppliedFn>
	void applyUnregister(uint8_t simId, AppliedFn& applied)
	{
		Channel* const target = channel(simId);
		if (!unregisterLifecycle(simId))
		{
			++m_lifecycleStats.unregistersIgnored;
			applied(static_cast<Channel*>(nullptr));
			return;
		}
		m_lifecycleRetired.set(simId);
		++m_lifecycleStats.unregistersApplied;
		applied(target);
	}

	static void unregisterAs(SimNetClient& session, uint8_t simId)
	{
		const LifecycleParts& parts = session.m_lifecycleParts;
		::unregisterSimulatable<SimT>(
			*parts.storage, *parts.reconciliation, *parts.inputResolution, *parts.netSync, simId, session.channel(simId), nullptr);
	}

	bool unregisterLifecycle(uint8_t simId)
	{
		if (!m_lifecycleRegistered.test(simId))
			return false;
		m_lifecycleRegistered.reset(simId);
		// ⛔G-13  docs/SimNetClient-guards.md
		if constexpr (SimNetLifecycleUnregisteringHook<HostT>)
			m_host.onLifecycleUnregistering(simId);
		m_unregisterLifecycle(*this, simId);
		return true;
	}

	void countBadDecode()
	{
		++m_stats.badDecode;
		++m_window.badDecode;
	}

	void countUnknownSim()
	{
		++m_stats.unknownSim;
		++m_window.unknownSim;
	}

	void maybeEmitWindow()
	{
		const double now = m_host.nowSeconds();
		const SimInboxStats inbox = m_transport.inbox().stats();
		if (!m_windowStartSeconds)
		{
			resetWindow(now, inbox);
			return;
		}
		if (now - *m_windowStartSeconds < kSimNetWindowSeconds)
			return;

		const std::array<char, kSimNetWindowLineBytes> line = formatSimNetWindow<kSimNetClientWindowFormat>(
			static_cast<unsigned long long>(m_window.drains),
			static_cast<unsigned long long>(m_window.messages),
			static_cast<unsigned long long>(m_window.maxDepth),
			static_cast<unsigned long long>(droppedTotal(inbox) - m_windowDropsBase),
			static_cast<unsigned long long>(inbox.tooLarge - m_windowTooLargeBase),
			static_cast<unsigned long long>(m_window.badDecode),
			0ull,
			static_cast<unsigned long long>(m_window.unknownSim),
			0ull,
			0ull,
			0ull,
			static_cast<unsigned long long>(staleDroppedTotal() - m_windowStaleBase),
			static_cast<unsigned long long>(duplicateDroppedTotal() - m_windowDuplicateBase),
			static_cast<unsigned long long>(quotaRefusedTotal(inbox) - m_windowQuotaRefusedBase),
			static_cast<unsigned long long>(inbox.controlHighWater),
			static_cast<unsigned long long>(m_window.quotaDisconnects),
			static_cast<unsigned long long>(inbox.eventsRefused - m_windowEventsRefusedBase),
			static_cast<unsigned long long>(inbox.eventHighWater));
		m_host.log(line.data());
		resetWindow(now, inbox);
	}

	void resetWindow(double now, const SimInboxStats& inbox)
	{
		m_window = WindowCounters{};
		m_windowStartSeconds = now;
		m_windowDropsBase = droppedTotal(inbox);
		m_windowTooLargeBase = inbox.tooLarge;
		m_windowStaleBase = staleDroppedTotal();
		m_windowDuplicateBase = duplicateDroppedTotal();
		m_windowQuotaRefusedBase = quotaRefusedTotal(inbox);
		m_windowEventsRefusedBase = inbox.eventsRefused;
	}

	HostT& m_host;
	TransportT& m_transport;
	std::atomic<ListenerRequest> m_listenerRequest{ListenerRequest::None};

	RosterMirror m_mirror;
	std::map<uint8_t, std::unique_ptr<Channel>> m_channels;
	std::map<uint8_t, RelayRingBuffer> m_heldRings;
	std::bitset<256> m_everListed;
	CorrectionStaleFilter m_staleFilter;
	NewestTickFilter m_clockFilter;
	std::optional<uint32_t> m_pendingClock;

	SimConnectionId m_serverConnection = kInvalidSimConnection;
	bool m_refused = false;
	std::optional<SimNetDisconnectRequest> m_disconnectRequest;
	bool m_wireMismatch = false;
	bool m_listenerBound = false;
	std::optional<uint8_t> m_floor;
	std::optional<SimNetConnectionTierLatch> m_tier;
	std::optional<SimClockRttSample> m_heldClockSample;

	SimNetClientStats m_stats;
	WindowCounters m_window;
	std::optional<double> m_windowStartSeconds;
	uint64_t m_windowDropsBase = 0u;
	uint64_t m_windowTooLargeBase = 0u;
	uint64_t m_windowStaleBase = 0u;
	uint64_t m_windowDuplicateBase = 0u;
	uint64_t m_windowQuotaRefusedBase = 0u;
	uint64_t m_windowEventsRefusedBase = 0u;

	LifecycleParts m_lifecycleParts;
	std::bitset<256> m_lifecycleRegistered;
	std::bitset<256> m_lifecycleRetired;
	void (*m_unregisterLifecycle)(SimNetClient&, uint8_t) = nullptr;
	SimNetLifecycleStats m_lifecycleStats;
	DeferredLifecycleQueue m_lifecycle;
};

namespace simNetClientSelfCheck
{
using ReferenceClientTransport = simNetTransportSelfCheck::ReferenceTransport;

static_assert(SimNetClientTransport<ReferenceClientTransport>,
	"SimNetTransport is the client session's transport: inbox, the per-character adapter and the server connection. "
	"Was SimNetClient-rationale.md §3");
static_assert(SimNetInputWindowTransport<ReferenceClientTransport>,
	"SimNetTransport opens an input set for the calling thread (beginInputSends) and ends it (endInputSends), so a session "
	"over it can send at capture from the step's thread (afterCaptureTick_Step, SimNetClient-rationale.md §17)");
static_assert(ClientSimTransport<SimNetBundleTransport<ReferenceClientTransport::ChannelTransport>>,
	"SimNetBundleTransport is a channel's ClientSimTransport: it wraps the transport's per-character adapter. Was "
	"SimNetClient-rationale.md §11, for SimNetClientChannelTransport");
} // namespace simNetClientSelfCheck
} // namespace ogsimnet
