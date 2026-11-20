#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetServer-rationale.md · docs/SimNetServer-guards.md

#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetConnections.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/DeferredLifecycleQueue.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/Network/RelayRingStager.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ogsimnet
{
inline constexpr double kSimNetWindowSeconds = 10.0;
inline constexpr uint32_t kNeverSentAgeTicks = 0xFFFFFFFFu;
inline constexpr size_t kMaxCorrectionCandidates = kMaxSimulatableSlots;

static_assert(simEnvelope::kHeaderBytes + 2u + CorrectionStateBuffer::kBufferBytes <= kMaxSimMessageBytes,
	"SimNetServer: the largest correction (a full CorrectionStateBuffer) in its envelope fits one client inbox record "
	"(kMaxSimMessageBytes); a larger one would be refused as tooLarge on every client. Re-check before raising the buffer");
static_assert(kMaxCorrectionCandidates == kRosterMaxEntries,
	"kMaxCorrectionCandidates: one candidate per character, and the roster holds at most kRosterMaxEntries characters "
	"(its add refuses one more as Full), so a frame's candidates always fit the fixed arrays");

template <class G>
concept SimNetGame = requires { typename G::Simulatables; } && IsSimulatableList<typename G::Simulatables>;

template <class T>
concept SimNetServerTransport = ServerSimTransport<T> && requires(T& transport, uint8_t simId)
{
	transport.inbox().drain([](const InboundSimRecord&) {});
	{ transport.inbox().stats() } -> std::same_as<SimInboxStats>;
	{ transport.flushFrame() } -> std::same_as<void>;
	{ transport.channelTransport(simId) } -> ClientSimTransport;
};

struct CorrectionCandidate
{
	uint8_t simId = 0u;
	uint32_t ageTicks = kNeverSentAgeTicks;
	uint32_t bytes = 0u;
	bool substituted = false;
};

struct CorrectionSelection
{
	std::array<uint8_t, kMaxCorrectionCandidates> indices{};
	uint8_t count = 0u;

	void select(size_t candidateIndex)
	{
		OG_CHECK(count < indices.size() && candidateIndex < kMaxCorrectionCandidates,
			"CorrectionSelection::select: a policy selects each candidate at most once, and there are at most "
			"kMaxCorrectionCandidates (one per character slot)");
		// ⛔G-06  docs/SimNetServer-guards.md
		if (count >= indices.size() || candidateIndex >= kMaxCorrectionCandidates)
			return;
		indices[count] = static_cast<uint8_t>(candidateIndex);
		++count;
	}

	std::span<const uint8_t> span() const { return std::span<const uint8_t>(indices.data(), count); }
};

template <class P>
concept CorrectionPolicy = requires(P& policy, SimConnectionId receiver, std::span<const CorrectionCandidate> candidates,
	size_t budgetBytes, CorrectionSelection& out)
{
	{ policy.select(receiver, candidates, budgetBytes, out) } -> std::same_as<void>;
};

struct SendAllDirty
{
	void select(SimConnectionId, std::span<const CorrectionCandidate> candidates, size_t, CorrectionSelection& out) const
	{
		for (size_t index = 0u; index < candidates.size(); ++index)
			out.select(index);
	}
};

static_assert(CorrectionPolicy<SendAllDirty>, "SendAllDirty is the Ph3 and Ph4 correction policy, so it must satisfy the seam");

constexpr size_t correctionAllowanceBytes(const ognet::lanes::LaneTable& lanes, const ognet::lanes::BudgetConfig& budget,
	double tickFrequency)
{
	const uint8_t priority = lanes[ognet::lanes::laneIndex(ognet::Lane::SimState)].priority;
	const uint64_t bytesPerSecond = static_cast<uint64_t>(budget.bytesPerSecond)
		* ognet::lanes::classSharePermille(lanes, priority) / ognet::lanes::kPermille;
	return tickFrequency > 0.0 ? static_cast<size_t>(static_cast<double>(bytesPerSecond) / tickFrequency) : 0u;
}

struct SimNetServerStats
{
	uint64_t drains = 0u;
	uint64_t messages = 0u;
	uint64_t badDecode = 0u;
	uint64_t refusedNotOwner = 0u;
	uint64_t unknownSim = 0u;
	uint64_t statesSent = 0u;
	uint64_t relaysSent = 0u;
	uint64_t overBudget = 0u;
	uint64_t clocksSent = 0u;
	uint64_t tiersSent = 0u;
	uint64_t floorsSent = 0u;
	uint64_t rosterSent = 0u;
	uint64_t quotaDisconnects = 0u;
	uint64_t emptyBundles = 0u;
	uint64_t addRefused = 0u;
};

enum class AddCharacterRefusal : uint8_t { None, SimIdLive, SlotLive };

constexpr const char* addCharacterRefusalName(AddCharacterRefusal refusal)
{
	switch (refusal)
	{
	case AddCharacterRefusal::None:
		return "None";
	case AddCharacterRefusal::SimIdLive:
		return "SimIdLive";
	case AddCharacterRefusal::SlotLive:
		return "SlotLive";
	}
	return "Unknown";
}

inline constexpr char kSimNetAddRefusedFormat[] = "[Warning][Roster.Refused] role=server simId=%u slot=%u reason=%s";
inline constexpr size_t kSimNetAddRefusedLineBytes = 96u;

static_assert(kSimNetAddRefusedLineBytes >= sizeof(kSimNetAddRefusedFormat) + 3u + 3u + sizeof("DuplicateOwnerSlot"),
	"kSimNetAddRefusedLineBytes must hold the server's [Roster.Refused] line with a 3-digit sim id, a 3-digit slot and the "
	"longest reason (the roster's DuplicateOwnerSlot; was SimIdLive before P5-23)");

inline std::array<char, kSimNetAddRefusedLineBytes> formatAddRefused(uint8_t simId, uint8_t physicsSlot, const char* reason)
{
	std::array<char, kSimNetAddRefusedLineBytes> line{};
	std::snprintf(line.data(), line.size(), kSimNetAddRefusedFormat, static_cast<unsigned>(simId), static_cast<unsigned>(physicsSlot),
		reason);
	return line;
}

inline constexpr char kSimNetServerLifecycleRefusedFormat[] = "[Warning][SimNet.Lifecycle] role=server simId=%u refused=%s";
inline constexpr size_t kSimNetServerLifecycleRefusedLineBytes = 96u;

static_assert(kSimNetServerLifecycleRefusedLineBytes >= sizeof(kSimNetServerLifecycleRefusedFormat) + 3u + sizeof("DuplicateOwnerSlot"),
	"kSimNetServerLifecycleRefusedLineBytes must hold the server's [SimNet.Lifecycle] line with a 3-digit sim id and the longest "
	"add refusal (the roster's DuplicateOwnerSlot)");

inline std::array<char, kSimNetServerLifecycleRefusedLineBytes> formatServerLifecycleRefused(uint8_t simId, const char* reason)
{
	std::array<char, kSimNetServerLifecycleRefusedLineBytes> line{};
	std::snprintf(line.data(), line.size(), kSimNetServerLifecycleRefusedFormat, static_cast<unsigned>(simId),
		reason != nullptr ? reason : "Unknown");
	return line;
}

inline constexpr char kSimNetServerWindowFormat[] =
	"[Warning][SimNet.Window] role=server drains=%llu msgs=%llu maxDepth=%llu drops=%llu tooLarge=%llu badDecode=%llu "
	"refusedNotOwner=%llu unknownSim=%llu statesSent=%llu relaysSent=%llu overBudget=%llu quotaRefused=%llu controlHighWater=%llu "
	"quotaDisconnects=%llu eventsRefused=%llu eventHighWater=%llu";
inline constexpr size_t kSimNetWindowLineBytes = 768u;

template <size_t N>
constexpr size_t simNetWindowCounterCount(const char (&format)[N])
{
	size_t count = 0u;
	for (size_t i = 0u; i + 3u < N; ++i)
	{
		if (format[i] == '%' && format[i + 1u] == 'l' && format[i + 2u] == 'l' && format[i + 3u] == 'u')
			++count;
	}
	return count;
}

template <const auto& Format, class... Counters>
std::array<char, kSimNetWindowLineBytes> formatSimNetWindow(Counters... counters)
{
	static_assert(simNetWindowCounterCount(Format) == sizeof...(Counters),
		"formatSimNetWindow: every %llu field of a [SimNet.Window] format takes one counter, and every counter has its field. "
		"Was the hand-counted counter total in the line's size check");
	static_assert((std::is_same_v<Counters, unsigned long long> && ...),
		"formatSimNetWindow: every [SimNet.Window] counter is passed as unsigned long long, the type %llu reads");
	std::array<char, kSimNetWindowLineBytes> line{};
	std::snprintf(line.data(), line.size(), Format, counters...);
	return line;
}

static_assert(simNetWindowCounterCount(kSimNetServerWindowFormat) == 16u
		&& kSimNetWindowLineBytes >= sizeof(kSimNetServerWindowFormat) + simNetWindowCounterCount(kSimNetServerWindowFormat) * 20u,
	"kSimNetWindowLineBytes must hold the server's [SimNet.Window] line with each of its sixteen counters at 20 digits");

inline constexpr char kSimNetQuotaDisconnectFormat[] =
	"[Warning][SimNet.Disconnect] role=%s conn=%u cause=controlQuota (more undrained Control messages than its slot's quota)";
inline constexpr size_t kSimNetQuotaDisconnectLineBytes = 160u;

static_assert(kSimNetQuotaDisconnectLineBytes >= sizeof(kSimNetQuotaDisconnectFormat) + 6u + 10u,
	"kSimNetQuotaDisconnectLineBytes must hold the [SimNet.Disconnect] line with its role and a 10-digit connection id");

inline std::array<char, kSimNetQuotaDisconnectLineBytes> formatQuotaDisconnect(const char* role, SimConnectionId conn)
{
	std::array<char, kSimNetQuotaDisconnectLineBytes> line{};
	std::snprintf(line.data(), line.size(), kSimNetQuotaDisconnectFormat, role, static_cast<unsigned int>(conn));
	return line;
}

template <class SimsT>
struct SimNetLifecycleParts;

template <class... SimTs>
struct SimNetLifecycleParts<SimulatableList<SimTs...>>
{
	SimulationObjectStorage<SimTs...>* storage = nullptr;
	SimulationReconciliation<SimTs...>* reconciliation = nullptr;
	SimulationInputResolution<SimTs...>* inputResolution = nullptr;
	SimulationNetSync<SimTs...>* netSync = nullptr;

	bool bound() const { return storage != nullptr; }
};

struct SimNetNoLifecycleHook
{
	template <class ChannelT>
	void operator()(ChannelT*) const
	{
	}
};

template <class F, class ChannelT>
concept SimNetLifecycleHook = std::move_constructible<F> && std::invocable<F&, ChannelT*>;

struct SimNetLifecycleStats
{
	uint64_t registersApplied = 0u;
	uint64_t registersRefused = 0u;
	uint64_t unregistersApplied = 0u;
	uint64_t unregistersIgnored = 0u;
	uint64_t removalUnregisters = 0u;
};

template <class SimT, class ShapeT>
inline constexpr bool kSimHasShape = std::is_same_v<simEnvelope::ShapeFor<typename SimT::InputType, typename SimT::StateType>, ShapeT>;

template <class ChannelT, class SimT, class InnerT>
inline constexpr bool kIsSessionChannel = std::is_same_v<ChannelT, SessionChannel<SimT, InnerT>>;

template <SimNetGame Game, SimNetServerHost HostT, SimNetServerTransport TransportT, CorrectionPolicy PolicyT = SendAllDirty,
	class SimsT = typename Game::Simulatables>
class SimNetServer;

template <SimNetGame Game, SimNetServerHost HostT, SimNetServerTransport TransportT, CorrectionPolicy PolicyT, class... SimTs>
class SimNetServer<Game, HostT, TransportT, PolicyT, SimulatableList<SimTs...>>
{
	static_assert(std::is_same_v<typename Game::Simulatables, SimulatableList<SimTs...>>,
		"SimNetServer: the simulatable list is the game's own; it is not a parameter to pass");
	static_assert(sizeof...(SimTs) > 0u, "SimNetServer: a game with no simulatables has no characters to serve");

	using FirstSim = std::tuple_element_t<0u, std::tuple<SimTs...>>;

public:
	using Shape = simEnvelope::ShapeFor<typename FirstSim::InputType, typename FirstSim::StateType>;
	using Coordinator = ServerReceptionCoordinator<SimNetConnectionHandle, SimTs...>;
	using ChannelTransport = decltype(std::declval<TransportT&>().channelTransport(uint8_t{}));

	template <class SimT>
	using Channel = SessionChannel<SimT, ChannelTransport>;

	static_assert((kIsSessionChannel<Channel<SimTs>, SimTs, ChannelTransport> && ...),
		"SimNetServer: a character's channel is SessionChannel<SimT, ChannelTransport>, the type SimNetClient::Channel is for the "
		"same transport, so one SimulatableOwnerTraits owner type serves both roles");
	static_assert(SimNetDeliveryHookWellFormed<HostT>,
		"SimNetServer: the host's noteDelivered member, if it has one, must be callable as noteDelivered(SimLane, double "
		"recvSeconds) -> void; any other shape would never be called");
	static_assert(SimNetLifecycleUnregisteringHookWellFormed<HostT>,
		"SimNetServer: the host's onLifecycleUnregistering member, if it has one, must be callable as "
		"onLifecycleUnregistering(uint8_t simId) -> void; any other shape would never be called, and a queue-registered "
		"character's removal would leave the game's systems pointing at freed storage");

	// ⛔G-09  docs/SimNetServer-guards.md
	static_assert((kSimHasShape<SimTs, Shape> && ...),
		"SimNetServer: every simulatable of the game has one envelope shape (input and state sizes), because an input set "
		"is walked with one bundle stride before any sim id in it is looked up");
	static_assert(simEnvelope::kHeaderBytes + 1u + relayedInputRing::kHeaderBytes + relayedInputRing::kMaxDepth * Shape::kRingEntryBytes
			<= kMaxSimMessageBytes,
		"SimNetServer: a full relay ring in its envelope fits one client inbox record (kMaxSimMessageBytes); a larger one is "
		"refused as tooLarge on every client");

	SimNetServer(HostT& host, TransportT& transport, const TimeConfig& config, size_t correctionBudgetBytes, PolicyT policy = PolicyT{})
		: m_host(host)
		, m_transport(transport)
		, m_immediate(transportIsImmediate(transport))
		, m_config(config)
		, m_coordinator(config)
		, m_correctionBudgetBytes(correctionBudgetBytes)
		, m_policy(std::move(policy))
	{
		m_coordinator.setLogger([this](const char* line) { m_host.log(line); });
	}

	SimNetServer(const SimNetServer&) = delete;
	SimNetServer& operator=(const SimNetServer&) = delete;
	SimNetServer(SimNetServer&&) = delete;
	SimNetServer& operator=(SimNetServer&&) = delete;

	size_t drain()
	{
		// ⛔G-12  docs/SimNetServer-guards.md
		m_lifecycle.applyPending();
		const size_t handled = m_transport.inbox().drain([this](const InboundSimRecord& record) { onRecord(record); });
		++m_stats.drains;
		++m_window.drains;
		m_window.maxDepth = std::max<uint64_t>(m_window.maxDepth, handled);
		return handled;
	}

	void dispatchFrame()
	{
		OG_CHECK(!m_immediate,
			"SimNetServer::dispatchFrame: an immediate session (SimNetConfig::immediate) is drained by drain() in the tick "
			"driver's beforePump, on the authority's owner thread (the runner once the authority loop is on); the frame point's "
			"drain is frame-locked mode's");
		drain();
	}

	void flushFrame()
	{
		OG_CHECK(!m_immediate,
			"SimNetServer::flushFrame: an immediate session sends at once: each accepted bundle's relay in the drain, the "
			"corrections, the clock and the window in afterInlineBatch. A frame flush would send from the frame's thread, and the "
			"server's endpoint is serviced by the runner alone (mode B)");
		flushRelays();
		flushCorrections();
		flushClock();
		m_transport.flushFrame();
		maybeEmitWindow();
	}

	bool immediate() const { return m_immediate; }

	void releaseForStep(int32_t firstTick, int32_t numSteps, std::optional<double> stepSeconds = std::nullopt)
	{
		(releaseFor<SimTs>(firstTick, numSteps, stepSeconds), ...);
	}

	void afterBatch(int32_t serverTick) { m_coordinator.reapConnections(serverTick); }

	void afterInlineBatch(int32_t serverTick)
	{
		OG_CHECK(m_immediate,
			"SimNetServer::afterInlineBatch: a frame-locked session sends its corrections and clock at flushFrame and reaps at "
			"afterBatch; the send after each batch is immediate mode's (SimNetConfig::immediate)");
		flushCorrections();
		flushClock();
		afterBatch(serverTick);
		maybeEmitWindow();
	}

	template <class SimT>
	Channel<SimT>* addCharacter(uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConn, uint8_t localIndex, std::string debugName)
	{
		static_assert((std::is_same_v<SimT, SimTs> || ...), "SimNetServer::addCharacter: SimT is not one of the game's simulatables");

		const AddCharacterRefusal refusal = addRefusalFor(simId, physicsSlot);
		// ⛔G-10  docs/SimNetServer-guards.md
		if (refusal != AddCharacterRefusal::None)
		{
			refuseAdd(simId, physicsSlot, addCharacterRefusalName(refusal));
			return nullptr;
		}

		RosterRecord record;
		record.simId = simId;
		record.physicsSlot = physicsSlot;
		record.owner = ownerConn == kInvalidSimConnection ? std::nullopt : std::optional<SimConnectionId>(ownerConn);
		record.localIndex = localIndex;
		const RosterAddRefusal rosterRefusal = m_roster.add(record);
		// ⛔G-11  docs/SimNetServer-guards.md
		if (rosterRefusal != RosterAddRefusal::None)
		{
			refuseAdd(simId, physicsSlot, rosterAddRefusalName(rosterRefusal));
			return nullptr;
		}

		auto channel = std::make_unique<Channel<SimT>>(
			SimNetBundleTransport<ChannelTransport>(m_transport.channelTransport(simId), m_stats.emptyBundles), simId, debugName);
		Channel<SimT>* const added = channel.get();
		std::get<ChannelMap<SimT>>(m_channels).emplace(simId, std::move(channel));
		m_characters.emplace(simId,
			Character{ownerConn, std::make_unique<RelayRingStager>(std::move(debugName), [this](const char* line) { m_host.log(line); })});

		for (const SimConnectionId receiver : m_attached)
			sendRoster(receiver, m_roster.addFor(receiver, simId));
		return added;
	}

	void removeCharacter(uint8_t simId, RosterRemoveReason reason)
	{
		const auto found = m_characters.find(simId);
		OG_CHECK(found != m_characters.end(), "SimNetServer::removeCharacter: this sim id has no character");
		// ⛔G-07  docs/SimNetServer-guards.md
		if (found == m_characters.end())
			return;
		// ⛔G-13  docs/SimNetServer-guards.md
		if (unregisterLifecycle(simId))
			++m_lifecycleStats.removalUnregisters;
		found->second.stager->finalReport();

		const RosterMessage message = takeRemove(simId, reason);
		for (const SimConnectionId receiver : m_attached)
			sendRoster(receiver, message);

		m_coordinator.forgetOwner(simId);
		(std::get<ChannelMap<SimTs>>(m_channels).erase(simId), ...);
		m_characters.erase(found);
		for (auto& [receiver, sent] : m_lastSentCorrectionTick)
			sent.erase(simId);
	}

	void bindLifecycle(SimulationObjectStorage<SimTs...>& storage, SimulationReconciliation<SimTs...>& reconciliation,
		SimulationInputResolution<SimTs...>& inputResolution, SimulationNetSync<SimTs...>& netSync)
	{
		OG_CHECK(!m_lifecycleParts.bound(),
			"SimNetServer::bindLifecycle: a session applies its queued registrations to one simulation, bound once before the "
			"first registerCharacter; a second bind would re-point registrations already applied to the first");
		m_lifecycleParts = LifecycleParts{&storage, &reconciliation, &inputResolution, &netSync};
	}

	template <class SimT, SimNetLifecycleHook<Channel<SimT>> AppliedFn = SimNetNoLifecycleHook>
	void registerCharacter(uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConn, uint8_t localIndex, std::string debugName,
		std::type_identity_t<SimT>&& simulatable, AppliedFn applied = AppliedFn{})
	{
		static_assert((std::is_same_v<SimT, SimTs> || ...), "SimNetServer::registerCharacter: SimT is not one of the game's simulatables");
		OG_CHECK(m_lifecycleParts.bound(),
			"SimNetServer::registerCharacter: bindLifecycle first; the drain applies the registration through "
			"registerSimulatable on the bound storage, reconciliation, input resolution and net sync");
		m_lifecycle.enqueue([this, simId, physicsSlot, ownerConn, localIndex, debugName = std::move(debugName),
								simulatable = std::move(simulatable), applied = std::move(applied)]() mutable {
			applyRegister<SimT>(simId, physicsSlot, ownerConn, localIndex, std::move(debugName), std::move(simulatable), applied);
		});
	}

	template <class SimT, SimNetLifecycleHook<Channel<SimT>> AppliedFn = SimNetNoLifecycleHook>
	void unregisterCharacter(uint8_t simId, RosterRemoveReason reason, AppliedFn applied = AppliedFn{})
	{
		static_assert((std::is_same_v<SimT, SimTs> || ...), "SimNetServer::unregisterCharacter: SimT is not one of the game's simulatables");
		m_lifecycle.enqueue([this, simId, reason, applied = std::move(applied)]() mutable { applyUnregister<SimT>(simId, reason, applied); });
	}

	bool lifecycleRegistered(uint8_t simId) const { return m_lifecycleUnregister[simId] != nullptr; }
	const SimNetLifecycleStats& lifecycleStats() const { return m_lifecycleStats; }

	void publishClock(uint32_t tick) { m_clockTick = tick; }

	void publishFloor(uint8_t floorTicks)
	{
		if (m_floor && *m_floor == floorTicks)
			return;
		m_floor = floorTicks;
		for (const SimConnectionId receiver : m_attached)
			sendU8(receiver, SimMessageKind::RelayFloor, floorTicks);
	}

	void publishTier(SimConnectionId conn, uint8_t tier)
	{
		const auto found = m_lastTier.find(conn);
		if (found == m_lastTier.end() ? tier == 0u : found->second == tier)
			return;
		m_lastTier[conn] = tier;
		sendU8(conn, SimMessageKind::ConnectionTier, tier);
	}

	template <class InputT>
	void deliverRemoteInput(unsigned int id, uint32_t captureTick, const InputT& input)
	{
		const std::optional<uint8_t> simId = simIdOf(id);
		if (!simId)
			return;
		visitCharacter(*simId, [&]<class SimT>(std::type_identity<SimT>, Channel<SimT>& channel) {
			if constexpr (std::is_same_v<typename SimT::InputType, InputT>)
				channel.deliverDelayedRemoteInput(captureTick, input);
		});
	}

	template <class InputT>
	void relayRemoteInput(unsigned int id, uint32_t captureTick, uint8_t dA, const InputT& input)
	{
		const std::optional<uint8_t> simId = simIdOf(id);
		if (!simId)
			return;
		const auto found = m_characters.find(*simId);
		if (found == m_characters.end())
			return;
		found->second.stager->template stageArrival<InputT>(captureTick, dA, input);
	}

	void sendConnectionTierToOwningClient(unsigned int id, uint8_t tier)
	{
		const std::optional<uint8_t> simId = simIdOf(id);
		const std::optional<SimConnectionId> owner = simId ? m_roster.ownerOf(*simId) : std::nullopt;
		if (owner)
			publishTier(*owner, tier);
	}

	template <class SimT>
	Channel<SimT>* channel(uint8_t simId)
	{
		ChannelMap<SimT>& map = std::get<ChannelMap<SimT>>(m_channels);
		const auto found = map.find(simId);
		return found == map.end() ? nullptr : found->second.get();
	}

	const RelayRingStager* stager(uint8_t simId) const
	{
		const auto found = m_characters.find(simId);
		return found == m_characters.end() ? nullptr : found->second.stager.get();
	}

	Coordinator& coordinator() { return m_coordinator; }
	const Coordinator& coordinator() const { return m_coordinator; }
	const RosterAuthority& roster() const { return m_roster; }
	const LivenessTable& liveness() const { return m_liveness; }
	SimNetConnectionHandle handleFor(SimConnectionId conn) const { return m_liveness.handleFor(conn); }
	std::span<const SimConnectionId> attachedConnections() const { return m_attached; }
	const SimNetServerStats& stats() const { return m_stats; }

private:
	template <class SimT>
	using ChannelMap = std::map<uint8_t, std::unique_ptr<Channel<SimT>>>;

	struct Character
	{
		SimConnectionId owner = kInvalidSimConnection;
		std::unique_ptr<RelayRingStager> stager;
	};

	struct WindowCounters
	{
		uint64_t drains = 0u;
		uint64_t messages = 0u;
		uint64_t maxDepth = 0u;
		uint64_t badDecode = 0u;
		uint64_t refusedNotOwner = 0u;
		uint64_t unknownSim = 0u;
		uint64_t statesSent = 0u;
		uint64_t relaysSent = 0u;
		uint64_t overBudget = 0u;
		uint64_t quotaDisconnects = 0u;
	};

	using MessageBytes = std::array<std::byte, kMaxSimMessageBytes>;
	using LifecycleParts = SimNetLifecycleParts<SimulatableList<SimTs...>>;
	using LifecycleUnregister = void (*)(SimNetServer&, uint8_t);

	template <class SimT>
	static void unregisterAs(SimNetServer& session, uint8_t simId)
	{
		Channel<SimT>* const owner = session.template channel<SimT>(simId);
		const LifecycleParts& parts = session.m_lifecycleParts;
		::unregisterSimulatable<SimT>(
			*parts.storage, *parts.reconciliation, *parts.inputResolution, *parts.netSync, simId, owner, owner);
	}

	bool unregisterLifecycle(uint8_t simId)
	{
		const LifecycleUnregister unregister = std::exchange(m_lifecycleUnregister[simId], nullptr);
		if (unregister == nullptr)
			return false;
		// ⛔G-14  docs/SimNetServer-guards.md
		if constexpr (SimNetLifecycleUnregisteringHook<HostT>)
			m_host.onLifecycleUnregistering(simId);
		unregister(*this, simId);
		return true;
	}

	template <class SimT, class AppliedFn>
	void applyRegister(uint8_t simId, uint8_t physicsSlot, SimConnectionId ownerConn, uint8_t localIndex, std::string debugName,
		SimT&& simulatable, AppliedFn& applied)
	{
		static_assert(std::is_same_v<PredictionOwnerFor<SimT>, Channel<SimT>> && std::is_same_v<AuthorityOwnerFor<SimT>, Channel<SimT>>,
			"SimNetServer::registerCharacter: the game's SimulatableOwnerTraits must name the session's Channel<SimT> as both "
			"owners, because the drain registers the channel it just created as the prediction and the authority owner");
		Channel<SimT>* const added = addCharacter<SimT>(simId, physicsSlot, ownerConn, localIndex, std::move(debugName));
		if (added == nullptr)
		{
			++m_lifecycleStats.registersRefused;
			m_host.log(formatServerLifecycleRefused(simId, m_lastAddRefusal).data());
			applied(static_cast<Channel<SimT>*>(nullptr));
			return;
		}
		const LifecycleParts& parts = m_lifecycleParts;
		::registerSimulatable<SimT>(*parts.storage, *parts.reconciliation, *parts.inputResolution, *parts.netSync, simId,
			std::move(simulatable), *added, *added);
		m_lifecycleUnregister[simId] = &unregisterAs<SimT>;
		++m_lifecycleStats.registersApplied;
		applied(added);
	}

	template <class SimT, class AppliedFn>
	void applyUnregister(uint8_t simId, RosterRemoveReason reason, AppliedFn& applied)
	{
		Channel<SimT>* const target = channel<SimT>(simId);
		if (target == nullptr || !unregisterLifecycle(simId))
		{
			++m_lifecycleStats.unregistersIgnored;
			applied(static_cast<Channel<SimT>*>(nullptr));
			return;
		}
		++m_lifecycleStats.unregistersApplied;
		applied(target);
		removeCharacter(simId, reason);
	}

	static std::optional<uint8_t> simIdOf(unsigned int id)
	{
		if (id == 0u || id > 0xFFu)
			return std::nullopt;
		return static_cast<uint8_t>(id);
	}

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

	template <class Fn>
	bool visitCharacter(uint8_t simId, Fn&& fn)
	{
		return visitFrom<0u>(simId, fn);
	}

	template <size_t I, class Fn>
	bool visitFrom(uint8_t simId, Fn& fn)
	{
		if constexpr (I == sizeof...(SimTs))
		{
			return false;
		}
		else
		{
			using SimT = std::tuple_element_t<I, std::tuple<SimTs...>>;
			ChannelMap<SimT>& map = std::get<I>(m_channels);
			const auto found = map.find(simId);
			if (found != map.end())
			{
				fn(std::type_identity<SimT>{}, *found->second);
				return true;
			}
			return visitFrom<I + 1u>(simId, fn);
		}
	}

	void onRecord(const InboundSimRecord& record)
	{
		switch (record.kind)
		{
		case InboundKind::Connected:
			m_liveness.apply(record);
			attach(record.conn);
			return;
		case InboundKind::Disconnected:
			if (record.cause == InboundCause::ControlQuota)
			{
				++m_stats.quotaDisconnects;
				++m_window.quotaDisconnects;
				m_host.log(formatQuotaDisconnect("server", record.conn).data());
			}
			m_liveness.apply(record);
			detach(record.conn);
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

	AddCharacterRefusal addRefusalFor(uint8_t simId, uint8_t physicsSlot) const
	{
		if (m_characters.find(simId) != m_characters.end())
			return AddCharacterRefusal::SimIdLive;
		if (physicsSlot == kRosterNoSlot)
			return AddCharacterRefusal::None;
		for (const auto& entry : m_characters)
		{
			const RosterRecord* const live = m_roster.recordFor(entry.first);
			if (live != nullptr && live->physicsSlot == physicsSlot)
				return AddCharacterRefusal::SlotLive;
		}
		return AddCharacterRefusal::None;
	}

	void refuseAdd(uint8_t simId, uint8_t physicsSlot, const char* reason)
	{
		++m_stats.addRefused;
		m_lastAddRefusal = reason;
		m_host.log(formatAddRefused(simId, physicsSlot, reason).data());
	}

	static bool transportIsImmediate(const TransportT& transport)
	{
		if constexpr (requires(const TransportT& configured) {
						  { configured.config().immediate } -> std::convertible_to<bool>;
					  })
			return transport.config().immediate;
		else
			return false;
	}

	void onMessage(const InboundSimRecord& record)
	{
		const DecodedSimMessage message = simEnvelope::decode<Shape>(record.lane, SimDirection::ClientToServer, record.payload());
		if (message.refusal != DecodeRefusal::None || message.kind != SimMessageKind::InputBundleSet)
		{
			countBadDecode();
			return;
		}
		const SimNetConnectionHandle handle = m_liveness.handleFor(record.conn);
		const DecodeRefusal walk = simEnvelope::forEachInputBundle<Shape>(message.body,
			[&](uint8_t simId, std::span<const std::byte> bundle) { onBundle(record, handle, simId, bundle); });
		if (walk != DecodeRefusal::None)
			countBadDecode();
	}

	void onBundle(const InboundSimRecord& record, const SimNetConnectionHandle& handle, uint8_t simId, std::span<const std::byte> bundle)
	{
		const RosterInputVerdict verdict = m_roster.checkInput(record.conn, simId);
		// ⛔G-01  docs/SimNetServer-guards.md
		if (verdict != RosterInputVerdict::Accepted)
		{
			refuseInput(record.conn, simId, verdict);
			return;
		}
		visitCharacter(simId, [&]<class SimT>(std::type_identity<SimT>, Channel<SimT>&) {
			receiveBundle<SimT>(record, handle, simId, bundle);
		});
	}

	template <class SimT>
	void receiveBundle(const InboundSimRecord& record, const SimNetConnectionHandle& handle, uint8_t simId, std::span<const std::byte> bundle)
	{
		using InputT = typename SimT::InputType;
		if (!m_bundle.assignFromWire(bundle))
		{
			countBadDecode();
			return;
		}
		m_bundle.template forEachSlot<InputT>([&](uint32_t captureTick, const InputT&) {
			m_host.stampServerReceived(simId, captureTick, record.recvSeconds);
		});
		// ⛔G-02  docs/SimNetServer-guards.md
		m_coordinator.noteRttSample(handle, simId, m_host.receptionTick(), m_host.rttMs(record.conn), *this);
		const uint8_t localIndex = m_roster.localIndexOf(simId).value_or(kRosterNotLocal);
		m_coordinator.template receiveInputBundle<SimT>(simId, handle, localIndex, m_bundle, *this, *this, record.recvSeconds);
		if (m_immediate)
			flushRelays(simId);
	}

	void refuseInput(SimConnectionId conn, uint8_t simId, RosterInputVerdict verdict)
	{
		if (verdict == RosterInputVerdict::UnknownSim)
		{
			++m_stats.unknownSim;
			++m_window.unknownSim;
			return;
		}
		++m_stats.refusedNotOwner;
		++m_window.refusedNotOwner;
		m_host.log(formatRosterInputRefused(conn, simId, verdict).data());
	}

	void countBadDecode()
	{
		++m_stats.badDecode;
		++m_window.badDecode;
	}

	template <class SimT>
	void releaseFor(int32_t firstTick, int32_t numSteps, std::optional<double> stepSeconds)
	{
		m_coordinator.template releaseDelayedInputs<SimT>(firstTick, numSteps,
			[this](unsigned int id, uint32_t captureTick, const typename SimT::InputType& input) -> bool {
				const std::optional<uint8_t> simId = simIdOf(id);
				Channel<SimT>* const target = simId ? channel<SimT>(*simId) : nullptr;
				if (target == nullptr)
					return false;
				target->deliverDelayedRemoteInput(captureTick, input);
				return true;
			},
			stepSeconds);
	}

	RosterMessage takeRemove(uint8_t simId, RosterRemoveReason reason)
	{
		// ⛔G-03  docs/SimNetServer-guards.md
		return (m_roster.remove(simId, reason), m_roster.removeFor(simId));
	}

	void attach(SimConnectionId conn)
	{
		if (conn == kInvalidSimConnection)
			return;
		detach(conn);
		m_attached.push_back(conn);
		sendRoster(conn, m_roster.snapshotFor(conn));
		if (m_floor)
			sendU8(conn, SimMessageKind::RelayFloor, *m_floor);
	}

	void detach(SimConnectionId conn)
	{
		std::erase(m_attached, conn);
		m_lastTier.erase(conn);
		m_lastSentCorrectionTick.erase(conn);
	}

	void sendRoster(SimConnectionId conn, const RosterMessage& message)
	{
		MessageBytes out{};
		const size_t size = simEnvelope::writeRoster(out, message.view());
		m_transport.sendControl(conn, std::span<const std::byte>(out.data(), size));
		++m_stats.rosterSent;
	}

	void sendU8(SimConnectionId conn, SimMessageKind kind, uint8_t value)
	{
		MessageBytes out{};
		const size_t size = kind == SimMessageKind::RelayFloor ? simEnvelope::writeRelayFloor(out, value)
															   : simEnvelope::writeConnectionTier(out, value);
		m_transport.sendControl(conn, std::span<const std::byte>(out.data(), size));
		++(kind == SimMessageKind::RelayFloor ? m_stats.floorsSent : m_stats.tiersSent);
	}

	void flushRelays(std::optional<uint8_t> onlySimId = std::nullopt)
	{
		const uint8_t resendEntries = static_cast<uint8_t>(latencyKnobs::clampRelayResendEntries(m_config.relayResendEntries));
		for (auto& [simId, character] : m_characters)
		{
			if ((onlySimId && *onlySimId != simId) || character.stager->flush(resendEntries) == 0u)
				continue;
			const RelayRingBuffer& ring = character.stager->published();
			MessageBytes out{};
			const size_t size = simEnvelope::writeRelayRing(out, simId, ring.payload());
			// ⛔G-04  docs/SimNetServer-guards.md
			m_transport.relay(character.owner, std::span<const std::byte>(out.data(), size));
			++m_stats.relaysSent;
			++m_window.relaysSent;
			stampHanded(simId, ring);
		}
	}

	void stampHanded(uint8_t simId, const RelayRingBuffer& ring)
	{
		visitCharacter(simId, [&]<class SimT>(std::type_identity<SimT>, Channel<SimT>&) {
			ring.template forEachEntry<typename SimT::InputType>([&](uint32_t captureTick, uint8_t, const typename SimT::InputType&) {
				if (captureTick != kNoInputCaptureTick)
					m_host.stampHanded(simId, captureTick);
			});
		});
	}

	void flushCorrections()
	{
		std::array<CorrectionCandidate, kMaxCorrectionCandidates> candidates{};
		std::array<std::span<const std::byte>, kMaxCorrectionCandidates> payloads{};
		std::array<uint32_t, kMaxCorrectionCandidates> ticks{};
		size_t count = 0u;
		for (auto& [simId, character] : m_characters)
		{
			visitCharacter(simId, [&]<class SimT>(std::type_identity<SimT>, Channel<SimT>& channel) {
				if (!channel.takeDirty())
					return;
				// ⛔G-05  docs/SimNetServer-guards.md
				const CorrectionStateBuffer& buffer = std::as_const(channel).getSyncedCorrectionStateBuffer();
				payloads[count] = buffer.payload();
				ticks[count] = buffer.payload().size() >= correctionStateBuffer::kHeaderBytes
					? buffer.template readFromBuffer<uint32_t>(correctionStateBuffer::kTickOffset)
					: 0u;
				candidates[count].simId = simId;
				candidates[count].bytes = static_cast<uint32_t>(simEnvelope::kHeaderBytes + 2u + payloads[count].size());
				++count;
			});
		}
		if (count == 0u)
			return;

		for (const SimConnectionId receiver : m_attached)
		{
			std::map<uint8_t, uint32_t>& sent = m_lastSentCorrectionTick[receiver];
			for (size_t index = 0u; index < count; ++index)
				candidates[index].ageTicks = ageFor(sent, candidates[index].simId, ticks[index]);

			CorrectionSelection selection;
			m_policy.select(receiver, std::span<const CorrectionCandidate>(candidates.data(), count), m_correctionBudgetBytes, selection);

			size_t selectedBytes = 0u;
			for (const uint8_t index : selection.span())
			{
				OG_CHECK(index < count, "SimNetServer: the correction policy selected a candidate that does not exist");
				// ⛔G-08  docs/SimNetServer-guards.md
				if (index >= count)
					continue;
				MessageBytes out{};
				const size_t size = simEnvelope::writeCorrectionState(
					out, candidates[index].simId, correctionStateBuffer::kWireFormatVersion, payloads[index]);
				m_transport.sendState(receiver, std::span<const std::byte>(out.data(), size));
				sent[candidates[index].simId] = ticks[index];
				selectedBytes += size;
				++m_stats.statesSent;
				++m_window.statesSent;
			}
			if (selectedBytes > m_correctionBudgetBytes)
			{
				++m_stats.overBudget;
				++m_window.overBudget;
			}
		}
	}

	static uint32_t ageFor(const std::map<uint8_t, uint32_t>& sent, uint8_t simId, uint32_t tick)
	{
		const auto found = sent.find(simId);
		if (found == sent.end())
			return kNeverSentAgeTicks;
		return tick >= found->second ? tick - found->second : 0u;
	}

	void flushClock()
	{
		if (!m_clockTick || m_clockTick == m_lastClockSent)
			return;
		MessageBytes out{};
		const size_t size = simEnvelope::writeClockPublish(out, *m_clockTick);
		m_transport.broadcastClock(std::span<const std::byte>(out.data(), size));
		m_lastClockSent = m_clockTick;
		++m_stats.clocksSent;
	}

	void maybeEmitWindow()
	{
		const double now = m_host.nowSeconds();
		const SimInboxStats inbox = m_transport.inbox().stats();
		if (!m_windowStartSeconds)
		{
			m_windowStartSeconds = now;
			m_windowDropsBase = droppedTotal(inbox);
			m_windowTooLargeBase = inbox.tooLarge;
			m_windowQuotaRefusedBase = quotaRefusedTotal(inbox);
			m_windowEventsRefusedBase = inbox.eventsRefused;
			return;
		}
		if (now - *m_windowStartSeconds < kSimNetWindowSeconds)
			return;

		const std::array<char, kSimNetWindowLineBytes> line = formatSimNetWindow<kSimNetServerWindowFormat>(
			static_cast<unsigned long long>(m_window.drains),
			static_cast<unsigned long long>(m_window.messages),
			static_cast<unsigned long long>(m_window.maxDepth),
			static_cast<unsigned long long>(droppedTotal(inbox) - m_windowDropsBase),
			static_cast<unsigned long long>(inbox.tooLarge - m_windowTooLargeBase),
			static_cast<unsigned long long>(m_window.badDecode),
			static_cast<unsigned long long>(m_window.refusedNotOwner),
			static_cast<unsigned long long>(m_window.unknownSim),
			static_cast<unsigned long long>(m_window.statesSent),
			static_cast<unsigned long long>(m_window.relaysSent),
			static_cast<unsigned long long>(m_window.overBudget),
			static_cast<unsigned long long>(quotaRefusedTotal(inbox) - m_windowQuotaRefusedBase),
			static_cast<unsigned long long>(inbox.controlHighWater),
			static_cast<unsigned long long>(m_window.quotaDisconnects),
			static_cast<unsigned long long>(inbox.eventsRefused - m_windowEventsRefusedBase),
			static_cast<unsigned long long>(inbox.eventHighWater));
		m_host.log(line.data());

		m_window = WindowCounters{};
		m_windowStartSeconds = now;
		m_windowDropsBase = droppedTotal(inbox);
		m_windowTooLargeBase = inbox.tooLarge;
		m_windowQuotaRefusedBase = quotaRefusedTotal(inbox);
		m_windowEventsRefusedBase = inbox.eventsRefused;
	}

	HostT& m_host;
	TransportT& m_transport;
	const bool m_immediate;
	const TimeConfig& m_config;
	Coordinator m_coordinator;
	size_t m_correctionBudgetBytes;
	PolicyT m_policy;

	LivenessTable m_liveness;
	RosterAuthority m_roster;
	std::vector<SimConnectionId> m_attached;
	std::unordered_map<SimConnectionId, uint8_t> m_lastTier;
	std::unordered_map<SimConnectionId, std::map<uint8_t, uint32_t>> m_lastSentCorrectionTick;

	std::tuple<ChannelMap<SimTs>...> m_channels;
	std::map<uint8_t, Character> m_characters;
	InputBundleBuffer m_bundle;

	std::optional<uint32_t> m_clockTick;
	std::optional<uint32_t> m_lastClockSent;
	std::optional<uint8_t> m_floor;

	SimNetServerStats m_stats;
	const char* m_lastAddRefusal = nullptr;
	WindowCounters m_window;
	std::optional<double> m_windowStartSeconds;
	uint64_t m_windowDropsBase = 0u;
	uint64_t m_windowTooLargeBase = 0u;
	uint64_t m_windowQuotaRefusedBase = 0u;
	uint64_t m_windowEventsRefusedBase = 0u;

	LifecycleParts m_lifecycleParts;
	std::array<LifecycleUnregister, 256> m_lifecycleUnregister{};
	SimNetLifecycleStats m_lifecycleStats;
	DeferredLifecycleQueue m_lifecycle;
};
} // namespace ogsimnet
