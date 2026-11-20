// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetChannel.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
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
#include "OGSimulation/SimulationTimeContext.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simNetSendAtCapture
{
enum class Access : std::uint8_t
{
	QueuePush,
	QueueRead,
	SenderWalk,
	Transmit,
};

inline constexpr std::array<Access, 4> kAccesses{Access::QueuePush, Access::QueueRead, Access::SenderWalk, Access::Transmit};

inline const char* accessName(Access access)
{
	switch (access)
	{
	case Access::QueuePush:
		return "queuePush";
	case Access::QueueRead:
		return "queueRead";
	case Access::SenderWalk:
		return "senderWalk";
	case Access::Transmit:
		return "transmit";
	}
	return "?";
}

struct AccessRecord
{
	Access access = Access::QueuePush;
	std::thread::id thread;
};

class AccessLog
{
public:
	void note(Access access)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (m_enabled)
			m_records.push_back(AccessRecord{access, std::this_thread::get_id()});
	}

	void enable(bool on)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_enabled = on;
		m_records.clear();
	}

	std::vector<AccessRecord> take()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return std::exchange(m_records, {});
	}

private:
	std::mutex m_mutex;
	bool m_enabled = false;
	std::vector<AccessRecord> m_records;
};

inline AccessLog& accessLog()
{
	static AccessLog log;
	return log;
}

struct CapPosition
{
	std::int32_t x = 0;
};

struct CapInput
{
	std::int32_t move = 0;
	std::uint8_t flags = 0u;

	std::int32_t getMove() const
	{
		accessLog().note(Access::QueueRead);
		return move;
	}
	void setMove(std::int32_t value) { move = value; }
	std::uint8_t getFlags() const { return flags; }
	void setFlags(std::uint8_t value) { flags = value; }
};
} // namespace simNetSendAtCapture

template <>
struct SerializableFields<simNetSendAtCapture::CapPosition>
{
	static constexpr auto get() { return std::make_tuple(SIM_MEMBER(simNetSendAtCapture::CapPosition, x)); }
};

template <>
struct SerializableFields<simNetSendAtCapture::CapInput>
{
	static constexpr auto get()
	{
		return std::make_tuple(SIM_FIELD(simNetSendAtCapture::CapInput, getMove, setMove),
			SIM_FIELD(simNetSendAtCapture::CapInput, getFlags, setFlags));
	}
};

namespace simNetSendAtCapture
{
using CapState = SimulationComposite<CapPosition>;

class CapAllState
{
public:
	const CapState& getState() const { return m_state; }
	CapState& editState() { return m_state; }

private:
	CapState m_state;
};

struct CapSim
{
	using StateType = CapState;
	using InputType = CapInput;

	CapAllState m_allState;
	CapAllState m_vizState;

	const CapAllState& getAllState() const { return m_allState; }
	CapAllState& editAllState() { return m_allState; }
	void updateVizState() { m_vizState = m_allState; }
	const CapAllState& getVizState() const { return m_vizState; }
};

struct CapGame
{
	using Simulatables = SimulatableList<CapSim>;
};

enum class Phase : std::uint8_t
{
	Step,
	GameThread,
	Flush,
};

inline const char* phaseName(Phase phase)
{
	switch (phase)
	{
	case Phase::Step:
		return "step";
	case Phase::GameThread:
		return "gameThread";
	case Phase::Flush:
		return "flush";
	}
	return "?";
}

using Bytes = std::vector<std::uint8_t>;

inline Bytes toU8(std::span<const std::byte> bytes)
{
	Bytes out;
	out.reserve(bytes.size());
	for (const std::byte b : bytes)
		out.push_back(std::to_integer<std::uint8_t>(b));
	return out;
}

struct SentSet
{
	int frame = 0;
	Phase phase = Phase::Step;
	Bytes bytes;
};

struct TapEndpoint
{
	void send(ognet::ConnectionId, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		if (lane != ognet::Lane::SimInput)
			return;
		accessLog().note(Access::Transmit);
		const std::lock_guard<std::mutex> lock(mutex);
		sets.push_back(SentSet{frame, phase, toU8(bytes)});
	}
	void broadcastExcept(ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) {}
	void setLaneHandler(ognet::Lane, ognet::LaneHandler) {}
	void setConnectionHandler(ognet::ConnectionHandler) {}
	void disconnect(ognet::ConnectionId) {}

	std::vector<SentSet> takeSets()
	{
		const std::lock_guard<std::mutex> lock(mutex);
		return std::exchange(sets, {});
	}

	int frame = 0;
	Phase phase = Phase::Step;
	std::mutex mutex;
	std::vector<SentSet> sets;
};

using Transport = ogsimnet::SimNetTransport<TapEndpoint, 256>;

struct CapHost
{
	std::function<void(std::uint8_t)> added;

	double nowSeconds() { return 0.0; }
	double serverRttSeconds() { return 0.05; }
	void onTimingInfo(std::uint32_t, double) {}
	void onRelayFloor(std::uint8_t, bool) {}
	void onConnectionTier(std::uint8_t, std::uint8_t, bool) {}
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

static_assert(ogsimnet::SimNetClientHost<CapHost>);

using Session = ogsimnet::SimNetClient<CapGame, CapHost, Transport>;
using CapChannel = Session::Channel;
using Shape = Session::Shape;
} // namespace simNetSendAtCapture

template <>
struct SimulatableOwnerTraits<simNetSendAtCapture::CapSim>
{
	using PredictionOwnerType = simNetSendAtCapture::CapChannel;
	using AuthorityOwnerType = simNetSendAtCapture::CapChannel;
};

namespace simNetSendAtCapture
{
using ogsimnet::RosterEntry;
using ogsimnet::SimNetConfig;

constexpr SimConnectionId kServerConn = 11u;
constexpr std::uint8_t kSimA = 3u;
constexpr std::uint8_t kSimB = 4u;
constexpr std::uint8_t kSimRemote = 7u;
constexpr std::uint32_t kFirstTick = 100u;
constexpr std::uint32_t kTickRateHz = 60u;
constexpr std::uint32_t kHighFrameRateHz = 144u;
constexpr int kSeconds = 3;
constexpr std::size_t kWorkersPerChain = 2u;
constexpr std::uint32_t kStride = static_cast<std::uint32_t>(sizeof(std::uint32_t))
	+ inputRedundancyBundle::detail::slotInputSize<CapInput>();

inline CapInput inputFor(std::uint8_t simId, std::uint32_t tick)
{
	CapInput input;
	input.move = static_cast<std::int32_t>((tick * 5u + simId * 101u) % 89u) - 44;
	input.flags = static_cast<std::uint8_t>((tick + simId) & 0x3Fu);
	return input;
}

inline std::vector<std::byte> snapshotMessage()
{
	const std::array<RosterEntry, 3> entries{
		{RosterEntry{kSimA, 0u, 0u}, RosterEntry{kSimB, 1u, 1u}, RosterEntry{kSimRemote, 2u, ogsimnet::kRosterNotLocal}}};
	const ogsimnet::RosterMessage message = ogsimnet::rosterCodec::encodeSnapshot(1u, std::span<const RosterEntry>(entries));
	std::array<std::byte, kMaxSimMessageBytes> out{};
	const std::size_t size = simEnvelope::writeRoster(std::span<std::byte>(out), message.view());
	return std::vector<std::byte>(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(size));
}

inline TimeConfig sendConfig(std::int32_t redundancyDepthTicks = 3, std::int32_t redundancyFrames = 0)
{
	TimeConfig config;
	config.redundancyDepthTicks = redundancyDepthTicks;
	config.redundancyFrames = redundancyFrames;
	return config;
}

inline std::uint32_t ticksInFrame(int frame, std::uint32_t frameRateHz)
{
	const std::uint64_t before = static_cast<std::uint64_t>(frame) * kTickRateHz / frameRateHz;
	const std::uint64_t after = static_cast<std::uint64_t>(frame + 1) * kTickRateHz / frameRateHz;
	return static_cast<std::uint32_t>(after - before);
}

struct Bundle
{
	int frame = 0;
	std::uint8_t simId = 0u;
	std::uint8_t slots = 0u;
	std::uint32_t newestTick = 0u;
};

inline std::uint32_t readU32(const Bytes& bytes, std::size_t offset)
{
	std::uint32_t value = 0u;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

inline std::vector<Bundle> bundlesOf(const std::vector<SentSet>& sets)
{
	std::vector<Bundle> out;
	for (const SentSet& set : sets)
	{
		std::vector<std::byte> wire;
		for (const std::uint8_t b : set.bytes)
			wire.push_back(std::byte{b});
		const DecodedSimMessage message = simEnvelope::decode<Shape>(SimLane::SimInput, SimDirection::ClientToServer, wire);
		if (message.refusal != DecodeRefusal::None || message.kind != SimMessageKind::InputBundleSet)
			continue;
		simEnvelope::forEachInputBundle<Shape>(message.body, [&](std::uint8_t simId, std::span<const std::byte> bundle) {
			const Bytes bytes = toU8(bundle);
			Bundle row;
			row.frame = set.frame;
			row.simId = simId;
			row.slots = bytes.size() > inputRedundancyBundle::kSlotCountOffset ? bytes[inputRedundancyBundle::kSlotCountOffset] : 0u;
			if (row.slots != 0u)
				row.newestTick = readU32(bytes, inputRedundancyBundle::kHeaderBytes + (row.slots - 1u) * kStride);
			out.push_back(row);
		});
	}
	return out;
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

	std::set<std::thread::id> threadIds() const
	{
		std::set<std::thread::id> ids;
		for (const std::thread& thread : m_threads)
			ids.insert(thread.get_id());
		return ids;
	}

	std::map<std::thread::id, std::size_t> tasksByThread()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_ranOn;
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

struct CaptureClient
{
	explicit CaptureClient(bool immediateIn)
		: immediate(immediateIn)
		, transport(std::make_unique<Transport>(SimNetConfig{immediateIn}))
	{
		auto logger = [this](const char* line) {
			if (std::strstr(line, "[SendLocalInputToServer]") == nullptr)
				return;
			accessLog().note(Access::SenderWalk);
			const std::lock_guard<std::mutex> lock(sendLinesMutex);
			sendLines.emplace_back(line);
		};
		netSync.setLogger(logger);
		resolution.template setNeutralInput<CapSim>(CapInput{});
		transport->installHandlers(endpoint);
		transport->markEndpointStarted();
		session = std::make_unique<Session>(host, *transport);
		host.added = [this](std::uint8_t simId) { addCharacter(simId); };
		transport->inbox().pushEvent(InboundKind::Connected, kServerConn, ogsimnet::kNoReceiveSeconds);
		pushed = transport->inbox().pushMessage(kServerConn, SimLane::Control, 0.0, snapshotMessage());
		session->drain();
	}

	CaptureClient(const CaptureClient&) = delete;
	CaptureClient& operator=(const CaptureClient&) = delete;

	void addCharacter(std::uint8_t simId)
	{
		CapChannel* const channel = session->channel(simId);
		const RosterEntry* const entry = session->roster().entryFor(simId);
		if (channel == nullptr || entry == nullptr)
		{
			addFailed = true;
			return;
		}
		std::function<CapInput(const SimulationTimeStep&, const LocalInputCache<CapInput>&)> provider;
		if (entry->isLocal())
		{
			provider = [simId](const SimulationTimeStep& step, const LocalInputCache<CapInput>&) {
				accessLog().note(Access::QueuePush);
				return inputFor(simId, step.getTick());
			};
		}
		registerSimulatable<CapSim>(storage, reconciliation, resolution, netSync, simId, CapSim{}, *channel, std::move(provider));
		++characters;
	}

	void step(std::uint32_t stepTick)
	{
		const SimulationTimeStep timeStep(stepTick, false);
		reconciliation.allocateFrontierSlotsAll(timeStep);
		const ResolvedInputs<CapSim> inputs = resolution.collectInputAll(timeStep);
		const auto& map = std::get<std::unordered_map<unsigned int, CapInput>>(inputs);
		std::vector<unsigned int> ids;
		storage.forEachSimulatable([&](unsigned int id, auto&) { ids.push_back(id); });
		for (const unsigned int id : ids)
		{
			const auto found = map.find(id);
			const std::int32_t move = found == map.end() ? 0 : found->second.move;
			storage.template get<CapSim>(id).editAllState().editState().template edit<CapPosition>().x += move;
		}
		reconciliation.postPredictionAll(timeStep);
	}

	void stepAndSend(const TimeConfig& config)
	{
		session->drain();
		step(tick);
		session->afterCaptureTick_Step(netSync, tick, config);
		lastTick = tick;
		++tick;
	}

	void frame(int index, std::uint32_t ticks, const TimeConfig& config)
	{
		endpoint.frame = index;
		endpoint.phase = Phase::Step;
		for (std::uint32_t k = 0u; k < ticks; ++k)
		{
			if (immediate)
			{
				stepAndSend(config);
				continue;
			}
			session->drain();
			step(tick);
			lastTick = tick;
			++tick;
		}
		endpoint.phase = Phase::GameThread;
		if (!immediate && lastTick)
			netSync.sendLocalInputToAuthorityAll(*lastTick, static_cast<std::uint32_t>(config.redundancyDepthTicks), config.redundancyFrames);
		endpoint.phase = Phase::Flush;
		session->flushFrame();
	}

	std::vector<std::string> takeSendLines()
	{
		const std::lock_guard<std::mutex> lock(sendLinesMutex);
		return std::exchange(sendLines, {});
	}

	bool ready() const { return pushed && !addFailed && characters == 3u; }

	bool immediate = false;
	SimulationObjectStorage<CapSim> storage;
	SimulationReconciliation<CapSim> reconciliation{storage};
	SimulationInputResolution<CapSim> resolution{storage, reconciliation};
	SimulationNetSync<CapSim> netSync{storage, reconciliation, resolution};
	CapHost host;
	TapEndpoint endpoint;
	std::unique_ptr<Transport> transport;
	std::unique_ptr<Session> session;
	std::uint32_t tick = kFirstTick;
	std::optional<std::uint32_t> lastTick;
	std::size_t characters = 0u;
	bool pushed = false;
	bool addFailed = false;
	std::mutex sendLinesMutex;
	std::vector<std::string> sendLines;
};

struct RateResult
{
	std::map<std::uint8_t, std::vector<std::size_t>> perSecond;
	std::vector<Bundle> bundles;
	std::vector<std::string> lines;
};

inline RateResult runAt(bool immediate, std::uint32_t frameRateHz, const TimeConfig& config)
{
	CaptureClient client(immediate);
	REQUIRE(client.ready());
	const int frames = kSeconds * static_cast<int>(frameRateHz);
	for (int f = 0; f < frames; ++f)
		client.frame(f, ticksInFrame(f, frameRateHz), config);
	RateResult result;
	result.bundles = bundlesOf(client.endpoint.takeSets());
	result.lines = client.takeSendLines();
	for (const std::uint8_t simId : {kSimA, kSimB})
		result.perSecond[simId].assign(static_cast<std::size_t>(kSeconds), 0u);
	for (const Bundle& bundle : result.bundles)
		++result.perSecond[bundle.simId][static_cast<std::size_t>(bundle.frame) / frameRateHz];
	CHECK(client.transport->stats().unwindowedRefused == 0u);
	return result;
}

inline std::string fieldOf(const std::string& line, const char* name)
{
	const std::string key = std::string(" ") + name + "=";
	const std::size_t at = line.find(key);
	if (at == std::string::npos)
		return "";
	const std::size_t begin = at + key.size();
	const std::size_t end = line.find(' ', begin);
	return line.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

struct ThreadCounts
{
	std::size_t gameThread = 0u;
	std::size_t stepTask = 0u;
	std::size_t other = 0u;
};

inline std::map<Access, ThreadCounts> classify(
	const std::vector<AccessRecord>& records, std::thread::id gameThread, const std::set<std::thread::id>& stepThreads)
{
	std::map<Access, ThreadCounts> out;
	for (const Access access : kAccesses)
		out[access] = ThreadCounts{};
	for (const AccessRecord& record : records)
	{
		ThreadCounts& counts = out[record.access];
		if (record.thread == gameThread)
			++counts.gameThread;
		else if (stepThreads.count(record.thread) != 0u)
			++counts.stepTask;
		else
			++counts.other;
	}
	return out;
}
} // namespace simNetSendAtCapture

using namespace simNetSendAtCapture;

TEST_CASE("SimNetClientSendAtCapture.AtOneTickPerFrameImmediateSendsTheFrameLockedBundlesOneFramePhaseEarlier", "[NetTransport]")
{
	constexpr int kFrames = 300;
	const TimeConfig config = sendConfig();
	CaptureClient locked(false);
	CaptureClient immediate(true);
	REQUIRE(locked.ready());
	REQUIRE(immediate.ready());
	REQUIRE(immediate.session->immediate());
	REQUIRE_FALSE(locked.session->immediate());

	for (int f = 0; f < kFrames; ++f)
	{
		locked.frame(f, 1u, config);
		immediate.frame(f, 1u, config);
	}

	const std::vector<SentSet> lockedSets = locked.endpoint.takeSets();
	const std::vector<SentSet> immediateSets = immediate.endpoint.takeSets();
	std::size_t sameBytes = 0u;
	std::size_t sameFrame = 0u;
	std::size_t lockedAtFlush = 0u;
	std::size_t immediateAtStep = 0u;
	const std::size_t count = std::min(lockedSets.size(), immediateSets.size());
	for (std::size_t i = 0u; i < count; ++i)
	{
		sameBytes += lockedSets[i].bytes == immediateSets[i].bytes ? 1u : 0u;
		sameFrame += lockedSets[i].frame == immediateSets[i].frame ? 1u : 0u;
		lockedAtFlush += lockedSets[i].phase == Phase::Flush ? 1u : 0u;
		immediateAtStep += immediateSets[i].phase == Phase::Step ? 1u : 0u;
	}
	std::printf("[SimNetClientSendAtCapture.OneTickPerFrame] frames=%d frameLockedSets=%zu immediateSets=%zu sameBytes=%zu sameFrame=%zu "
				"frameLockedAtFlush=%zu immediateAtStep=%zu\n",
		kFrames, lockedSets.size(), immediateSets.size(), sameBytes, sameFrame, lockedAtFlush, immediateAtStep);

	CHECK(lockedSets.size() == static_cast<std::size_t>(kFrames));
	CHECK(immediateSets.size() == lockedSets.size());
	CHECK(sameBytes == count);
	CHECK(sameFrame == count);
	CHECK(lockedAtFlush == count);
	CHECK(immediateAtStep == count);
	CHECK(locked.takeSendLines() == immediate.takeSendLines());

	const ogsimnet::SimNetTransportStats lockedStats = locked.transport->stats();
	const ogsimnet::SimNetTransportStats immediateStats = immediate.transport->stats();
	CHECK(lockedStats.heldSends == lockedStats.inputSets);
	CHECK(lockedStats.directSends == 0u);
	CHECK(immediateStats.heldSends == 0u);
	CHECK(immediateStats.directSends == immediateStats.inputSets);
	CHECK(immediateStats.inputSets == lockedStats.inputSets);
	CHECK(immediateStats.unwindowedRefused == 0u);
}

TEST_CASE("SimNetClientSendAtCapture.At144FpsTheBundleCountPerSecondIsTheTickRateAndRedundancyKeepsItsDepth", "[NetTransport]")
{
	const TimeConfig config = sendConfig();
	const RateResult immediate = runAt(true, kHighFrameRateHz, config);
	const RateResult locked = runAt(false, kHighFrameRateHz, config);

	for (const std::uint8_t simId : {kSimA, kSimB})
	{
		for (int second = 0; second < kSeconds; ++second)
		{
			const std::size_t index = static_cast<std::size_t>(second);
			std::printf("[SimNetClientSendAtCapture.Rate144] simId=%u second=%d immediateBundles=%zu frameLockedBundles=%zu tickRate=%u "
						"frameRate=%u\n",
				static_cast<unsigned>(simId), second, immediate.perSecond.at(simId)[index], locked.perSecond.at(simId)[index], kTickRateHz,
				kHighFrameRateHz);
			CHECK(immediate.perSecond.at(simId)[index] == kTickRateHz);
		}
		CHECK(locked.perSecond.at(simId)[1] == kHighFrameRateHz);
	}

	std::size_t warm = 0u;
	std::size_t atDepth = 0u;
	std::uint32_t previousNewest = 0u;
	std::size_t consecutive = 0u;
	for (const Bundle& bundle : immediate.bundles)
	{
		if (bundle.simId != kSimA)
			continue;
		consecutive += previousNewest != 0u && bundle.newestTick == previousNewest + 1u ? 1u : 0u;
		previousNewest = bundle.newestTick;
		if (bundle.newestTick < kFirstTick + 2u)
			continue;
		++warm;
		atDepth += bundle.slots == static_cast<std::uint8_t>(config.redundancyDepthTicks) ? 1u : 0u;
	}
	std::size_t linesAtDepth = 0u;
	for (const std::string& line : immediate.lines)
		linesAtDepth += fieldOf(line, "slots") == std::to_string(config.redundancyDepthTicks) ? 1u : 0u;
	std::printf("[SimNetClientSendAtCapture.Rate144] simId=%u warmBundles=%zu atDepth=%zu consecutiveNewestTicks=%zu sendLines=%zu "
				"linesWithSlots%d=%zu\n",
		static_cast<unsigned>(kSimA), warm, atDepth, consecutive, immediate.lines.size(), config.redundancyDepthTicks, linesAtDepth);
	CHECK(warm > 0u);
	CHECK(atDepth == warm);
	CHECK(consecutive + 1u == static_cast<std::size_t>(kSeconds) * kTickRateHz);
	CHECK(linesAtDepth == immediate.lines.size());

	const TimeConfig frames4 = sendConfig(3, 4);
	const RateResult framesArm = runAt(true, kHighFrameRateHz, frames4);
	std::size_t framesWarm = 0u;
	std::size_t framesAtFour = 0u;
	for (const Bundle& bundle : framesArm.bundles)
	{
		if (bundle.simId != kSimA || bundle.newestTick < kFirstTick + 3u)
			continue;
		++framesWarm;
		framesAtFour += bundle.slots == 4u ? 1u : 0u;
	}
	std::size_t framesLinesAtFour = 0u;
	for (const std::string& line : framesArm.lines)
		framesLinesAtFour += fieldOf(line, "slots") == "4" ? 1u : 0u;
	std::printf("[SimNetClientSendAtCapture.Rate144] redundancyFrames=4 warmBundles=%zu atFour=%zu sendLines=%zu linesWithSlots4=%zu\n",
		framesWarm, framesAtFour, framesArm.lines.size(), framesLinesAtFour);
	CHECK(framesWarm > 0u);
	CHECK(framesAtFour == framesWarm);
	CHECK(framesLinesAtFour == framesArm.lines.size());
}

TEST_CASE("SimNetClientSendAtCapture.TheStepHookReadsTheRedundancyKnobsLiveFromTimeConfig", "[NetTransport]")
{
	CaptureClient client(true);
	REQUIRE(client.ready());
	TimeConfig config = sendConfig(3, 0);
	for (int f = 0; f < 20; ++f)
		client.frame(f, 1u, config);
	client.takeSendLines();
	client.endpoint.takeSets();

	config.redundancyDepthTicks = 5;
	for (int f = 20; f < 30; ++f)
		client.frame(f, 1u, config);

	const std::vector<std::string> lines = client.takeSendLines();
	REQUIRE_FALSE(lines.empty());
	for (const std::string& line : lines)
	{
		CHECK(fieldOf(line, "depth") == "5");
		CHECK(fieldOf(line, "slots") == "5");
	}
	std::vector<std::uint8_t> slots;
	for (const Bundle& bundle : bundlesOf(client.endpoint.takeSets()))
	{
		if (bundle.simId == kSimA)
			slots.push_back(bundle.slots);
	}
	REQUIRE(slots.size() == 10u);
	CHECK(slots[0] == 4u);
	for (std::size_t i = 1u; i < slots.size(); ++i)
		CHECK(slots[i] == 5u);
}

TEST_CASE("SimNetClientSendAtCapture.NoSenderMapOrPendingQueueAccessRunsOnTheGameThreadInImmediateMode", "[NetTransport]")
{
	constexpr int kFrames = 2 * static_cast<int>(kHighFrameRateHz);
	const TimeConfig config = sendConfig();
	const std::thread::id gameThread = std::this_thread::get_id();

	CaptureClient client(true);
	REQUIRE(client.ready());
	TaskChain chain(kWorkersPerChain);
	const std::set<std::thread::id> stepThreads = chain.threadIds();

	accessLog().enable(true);
	for (int f = 0; f < kFrames; ++f)
	{
		client.endpoint.frame = f;
		const std::uint32_t ticks = ticksInFrame(f, kHighFrameRateHz);
		for (std::uint32_t k = 0u; k < ticks; ++k)
			chain.post([&client, &config] { client.stepAndSend(config); });
		chain.post([&client] { client.session->flushFrame(); });
		chain.waitIdle();
	}
	const std::map<Access, ThreadCounts> immediateCounts = classify(accessLog().take(), gameThread, stepThreads);

	CaptureClient locked(false);
	REQUIRE(locked.ready());
	accessLog().enable(true);
	for (int f = 0; f < kFrames; ++f)
	{
		const std::uint32_t ticks = ticksInFrame(f, kHighFrameRateHz);
		for (std::uint32_t k = 0u; k < ticks; ++k)
		{
			chain.post([&locked] {
				locked.session->drain();
				locked.step(locked.tick);
				locked.lastTick = locked.tick;
				++locked.tick;
			});
		}
		chain.waitIdle();
		if (locked.lastTick)
		{
			locked.netSync.sendLocalInputToAuthorityAll(
				*locked.lastTick, static_cast<std::uint32_t>(config.redundancyDepthTicks), config.redundancyFrames);
		}
		locked.session->flushFrame();
	}
	const std::map<Access, ThreadCounts> lockedCounts = classify(accessLog().take(), gameThread, stepThreads);
	accessLog().enable(false);

	std::size_t workersUsed = 0u;
	for (const auto& [thread, tasks] : chain.tasksByThread())
		workersUsed += tasks > 0u ? 1u : 0u;
	for (const Access access : kAccesses)
	{
		const ThreadCounts& im = immediateCounts.at(access);
		const ThreadCounts& fl = lockedCounts.at(access);
		std::printf("[SimNetClientSendAtCapture.Threads] access=%s immediate: gameThread=%zu stepTask=%zu other=%zu | frameLocked: "
					"gameThread=%zu stepTask=%zu other=%zu\n",
			accessName(access), im.gameThread, im.stepTask, im.other, fl.gameThread, fl.stepTask, fl.other);
		CHECK(im.gameThread == 0u);
		CHECK(im.other == 0u);
		CHECK(im.stepTask > 0u);
	}
	std::printf("[SimNetClientSendAtCapture.Threads] workers=%zu workersUsed=%zu unwindowedRefused=%llu\n", stepThreads.size(), workersUsed,
		static_cast<unsigned long long>(client.transport->stats().unwindowedRefused));
	CHECK(workersUsed == kWorkersPerChain);
	CHECK(client.transport->stats().unwindowedRefused == 0u);
	CHECK(lockedCounts.at(Access::SenderWalk).gameThread > 0u);
	CHECK(lockedCounts.at(Access::QueueRead).gameThread > 0u);
	CHECK(lockedCounts.at(Access::Transmit).gameThread > 0u);
	CHECK(lockedCounts.at(Access::QueuePush).stepTask > 0u);
}

TEST_CASE("SimNetClientSendAtCapture.Death.TheGameThreadsSendOnAnImmediateSessionFiresTheTransportsCheck", "[.][OGSimDeath]")
{
	CaptureClient client(true);
	REQUIRE(client.ready());
	client.frame(0, 1u, sendConfig());
	client.netSync.sendLocalInputToAuthorityAll(*client.lastTick, 3u, 0);
	FAIL("a send outside afterCaptureTick_Step on an immediate session did NOT fire SimNetTransport's OG_CHECK");
}

TEST_CASE("SimNetClientSendAtCapture.Death.EndInputSendsOnAnImmediateSessionFiresTheCheck", "[.][OGSimDeath]")
{
	CaptureClient client(true);
	REQUIRE(client.ready());
	client.session->endInputSends();
	FAIL("endInputSends on an immediate session did NOT fire SimNetClient's OG_CHECK");
}

TEST_CASE("SimNetClientSendAtCapture.Death.AfterCaptureTickStepOnAFrameLockedSessionFiresTheCheck", "[.][OGSimDeath]")
{
	CaptureClient client(false);
	REQUIRE(client.ready());
	client.step(client.tick);
	client.session->afterCaptureTick_Step(client.netSync, client.tick, sendConfig());
	FAIL("afterCaptureTick_Step on a frame-locked session did NOT fire SimNetClient's OG_CHECK");
}
