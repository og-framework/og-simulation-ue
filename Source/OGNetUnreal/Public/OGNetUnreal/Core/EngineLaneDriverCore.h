#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/EngineLaneDriverCore-rationale.md · docs/EngineLaneDriverCore-guards.md

#include "OGNet/Connection/ConnectionStats.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/Endpoint/EngineLane.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/Endpoint/NetWindow.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetCheck.h"
#include "OGNet/NetConcepts.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>

namespace ognetunreal
{
enum class IoMode : uint8_t { OwnThread, HostServiced };
enum class AcceptVerdict : uint8_t { Accept, Reject, Ignore };
enum class SendVerdict : uint8_t { Sent, Buffered, Refused, BufferFull, Dead };

struct DriverCallbacks
{
	void* ctx = nullptr;
	AcceptVerdict (*accept)(void* ctx, ognet::ConnectionId id) = nullptr;
	void (*deliver)(void* ctx, ognet::ConnectionId id, std::span<const std::byte> bytes) = nullptr;
	void (*closed)(void* ctx, ognet::ConnectionId id, ognet::DisconnectReason reason) = nullptr;
	void (*serverConnectionUp)(void* ctx, ognet::ConnectionId id) = nullptr;
	void (*connectEnded)(void* ctx, ognet::DisconnectReason reason) = nullptr;
};

struct SimLaneAttach
{
	void* ctx = nullptr;
	void (*beforeStart)(void* ctx, SimLaneEndpoint& endpoint) = nullptr;
};

struct DriverStats
{
	uint64_t accepted = 0;
	uint64_t rejected = 0;
	uint64_t ignoredDatagrams = 0;
	uint64_t unknownDatagrams = 0;
	uint64_t preConnectBuffered = 0;
	uint64_t preConnectDropped = 0;
	uint64_t sendsRefused = 0;
	uint64_t sendsDead = 0;
	uint64_t retries = 0;
	uint64_t closesDrained = 0;
	uint64_t closesTimedOut = 0;
	uint64_t handlerQueueDropped = 0;
	uint64_t batches = 0;
	uint64_t maxBatch = 0;
};

inline constexpr std::size_t kPreConnectSlots = 32;
inline constexpr std::size_t kPreConnectSlotBytes = 1189;
inline constexpr std::size_t kHandlerQueueSlots = 64;
inline constexpr std::size_t kBatchEvents = 256;
inline constexpr std::size_t kBatchEventBytes = 1232;
inline constexpr double kDrainedCloseSeconds = 0.2;
inline constexpr std::chrono::milliseconds kStopDrainPollInterval{1};

static_assert(kPreConnectSlotBytes == ognet::EngineLane::maxPacketBytes(),
	"a pre-connect slot holds exactly one packet the engine lane accepts. Was P4-3's '32 slots of up to 1,189 B'");
static_assert(kBatchEventBytes == ognet::kEngineEventBytes && kBatchEventBytes <= UINT16_MAX,
	"a batch slot holds any event a ring slot holds, and its size fits the slot's u16. Was P4-3's 'bytes[1232]'");
static_assert(kBatchEvents <= ognet::kEngineRingSlots, "a batch never asks the ring for more events than it can hold");
static_assert(kDrainedCloseSeconds == ognet::lanes::kDefaultLaneTable[ognet::lanes::laneIndex(ognet::Lane::Engine)].maxQueueAgeSeconds,
	"a drained close waits at most the Engine lane's age limit: a packet still queued after it has been dropped by the lane anyway. "
	"Was P4-3's 'or at closedAt + 0.2 s'");

template <ognet::NetHostServices HostT, ognet::DatagramBackend BackendT>
class EngineLaneDriverCore
{
public:
	using Callbacks = DriverCallbacks;
	using Endpoint = ognet::NetEndpoint<HostT, BackendT>;

	EngineLaneDriverCore() : m_simEndpoint(this, simEndpointOps()) {}
	~EngineLaneDriverCore() { m_endpoint.reset(); }

	EngineLaneDriverCore(const EngineLaneDriverCore&) = delete;
	EngineLaneDriverCore& operator=(const EngineLaneDriverCore&) = delete;
	EngineLaneDriverCore(EngineLaneDriverCore&&) = delete;
	EngineLaneDriverCore& operator=(EngineLaneDriverCore&&) = delete;

	bool start(HostT& host, BackendT& backend, const ognet::EndpointConfig& config, IoMode mode, const Callbacks& callbacks,
		const SimLaneAttach* attach = nullptr)
	{
		OGNET_CHECK(!m_started, "EngineLaneDriverCore::start called twice: a core starts one endpoint once");
		OGNET_CHECK(callbacks.accept != nullptr && callbacks.deliver != nullptr && callbacks.closed != nullptr
				&& callbacks.serverConnectionUp != nullptr && callbacks.connectEnded != nullptr,
			"EngineLaneDriverCore::start needs all five callbacks");
		OGNET_CHECK(attach == nullptr || attach->beforeStart != nullptr, "EngineLaneDriverCore::start: an attach needs its beforeStart");
		m_host = &host;
		m_backend = &backend;
		m_mode = mode;
		m_role = config.role;
		m_callbacks = callbacks;
		m_recordCapacity = config.role == ognet::Role::Server ? config.maxConnections : 1;
		m_records = std::make_unique<Record[]>(m_recordCapacity);
		m_recordCount = 0;
		m_preConnect = std::make_unique<PreConnectSlot[]>(kPreConnectSlots);
		m_preConnectCount = 0;
		m_batch = std::make_unique<BatchEvent[]>(kBatchEvents);
		m_batchCount = 0;
		m_client = ClientState::Idle;
		m_serverId = ognet::kInvalidConnection;
		m_config = config;
		m_forward = ognet::ConnectionHandler{};
		m_endpoint.emplace(host, backend);
		m_endpoint->setConnectionHandler(ognet::ConnectionHandler{&EngineLaneDriverCore::onConnectionEvent, this});
		m_lane = &m_endpoint->engineLane();
		if (attach != nullptr)
		{
			m_attaching = true;
			attach->beforeStart(attach->ctx, m_simEndpoint);
			m_attaching = false;
		}
		if (!m_endpoint->start(config))
		{
			m_lane = nullptr;
			m_forward = ognet::ConnectionHandler{};
			m_endpoint.reset();
			return false;
		}
		m_window.emplace(ognet::NetWindowConfig{.role = config.role});
		if (mode == IoMode::OwnThread)
		{
			m_endpoint->startOwnThread();
		}
		m_started = true;
		return true;
	}

	void connect(const ognet::Address& server, double engineDeadlineSeconds)
	{
		if (!isRunning() || m_role != ognet::Role::Client || m_client == ClientState::Up || m_client == ClientState::Ended)
		{
			return;
		}
		m_serverAddress = server;
		m_deadlineSeconds = engineDeadlineSeconds;
		m_client = ClientState::Connecting;
		m_endpoint->connect(m_serverAddress);
	}

	SendVerdict send(ognet::ConnectionId id, std::span<const std::byte> bytes)
	{
		if (!isRunning())
		{
			return dead();
		}
		if (m_role == ognet::Role::Client && m_client != ClientState::Up)
		{
			return bufferPreConnect(bytes);
		}
		const Record* record = find(id);
		if (record == nullptr || record->state != RecordState::Live)
		{
			return dead();
		}
		return sendThroughLane(id, bytes);
	}

	void tick(double nowSeconds, bool clientConnectionPending)
	{
		if (!isRunning())
		{
			return;
		}
		drainHandlerQueue(nowSeconds, clientConnectionPending);
		pollRing();
		serviceDrainedCloses(nowSeconds);
		closeWindowIfDue(nowSeconds);
	}

	void engineClosed(ognet::ConnectionId id, double nowSeconds)
	{
		if (!isRunning())
		{
			return;
		}
		if (m_role == ognet::Role::Client && m_client != ClientState::Up)
		{
			m_client = ClientState::Ended;
			m_preConnectCount = 0;
			return;
		}
		Record* record = find(id);
		if (record == nullptr || record->state == RecordState::Draining)
		{
			return;
		}
		if (m_role == ognet::Role::Client)
		{
			m_client = ClientState::Ended;
		}
		// ⛔G-02  docs/EngineLaneDriverCore-guards.md
		record->state = RecordState::Draining;
		record->closedAtSeconds = nowSeconds;
	}

	void stop(double nowSeconds, double drainSeconds)
	{
		if (!isRunning())
		{
			return;
		}
		m_stopped = true;
		if (m_mode == IoMode::OwnThread)
		{
			while (anyQueued() && m_host->nowSeconds() < nowSeconds + drainSeconds)
			{
				std::this_thread::sleep_for(kStopDrainPollInterval);
			}
		}
		m_endpoint->stop();
	}

	ognet::ServiceResult serviceUntil(double deadlineSeconds)
	{
		OGNET_CHECK(m_mode == IoMode::HostServiced, "EngineLaneDriverCore::serviceUntil in OwnThread mode: og-net's thread services it");
		if (!m_endpoint.has_value())
		{
			ognet::ServiceResult result;
			result.stopped = true;
			return result;
		}
		return m_endpoint->serviceUntil(deadlineSeconds);
	}

	bool hasHighResolutionWait() const
	{
		if constexpr (requires(const BackendT& backend) { backend.hasHighResolutionWait(); })
		{
			return m_backend != nullptr && m_backend->hasHighResolutionWait();
		}
		else
		{
			return false;
		}
	}

	bool isStarted() const { return m_started; }
	bool isStopped() const { return m_stopped; }
	IoMode mode() const { return m_mode; }
	ognet::ConnectionId serverConnection() const { return m_serverId; }

	DriverStats stats() const
	{
		DriverStats out = m_stats;
		const std::lock_guard<std::mutex> lock(m_handlerMutex);
		out.handlerQueueDropped = m_handlerDropped;
		return out;
	}

	ognet::ConnectionStats connectionStats(ognet::ConnectionId id) const
	{
		return m_endpoint.has_value() ? m_endpoint->stats(id) : ognet::ConnectionStats{};
	}

	ognet::EndpointStats endpointStats() const
	{
		return m_endpoint.has_value() ? m_endpoint->endpointStats() : ognet::EndpointStats{};
	}

	ognet::EngineLaneStats engineLaneStats() const { return m_lane != nullptr ? m_lane->stats() : ognet::EngineLaneStats{}; }

	std::optional<ognet::Address> localAddress() const
		requires ognet::ReportsLocalAddress<BackendT>
	{
		if (m_backend == nullptr)
		{
			return std::nullopt;
		}
		return m_backend->localAddress();
	}

private:
	enum class RecordState : uint8_t { Pending, Live, Draining };
	enum class ClientState : uint8_t { Idle, Connecting, Up, Ended };

	struct Record
	{
		ognet::ConnectionId id = ognet::kInvalidConnection;
		RecordState state = RecordState::Pending;
		double closedAtSeconds = 0.0;
	};

	struct PreConnectSlot
	{
		uint16_t size = 0;
		std::array<std::byte, kPreConnectSlotBytes> bytes{};
	};

	struct BatchEvent
	{
		ognet::EngineEventKind kind = ognet::EngineEventKind::Connected;
		ognet::ConnectionId id = ognet::kInvalidConnection;
		ognet::DisconnectReason reason = ognet::DisconnectReason::Timeout;
		double recvSeconds = 0.0;
		uint16_t size = 0;
		std::array<std::byte, kBatchEventBytes> bytes{};
	};

	bool isRunning() const { return m_started && !m_stopped; }

	static void onConnectionEvent(void* ctx, const ognet::ConnectionEvent& event)
	{
		EngineLaneDriverCore& self = *static_cast<EngineLaneDriverCore*>(ctx);
		// ⛔G-05  docs/EngineLaneDriverCore-guards.md
		if (self.m_forward.fn != nullptr)
		{
			self.m_forward.fn(self.m_forward.ctx, event);
		}
		if (event.kind != ognet::ConnectionEventKind::Disconnected || event.id != ognet::kInvalidConnection)
		{
			return;
		}
		const std::lock_guard<std::mutex> lock(self.m_handlerMutex);
		if (self.m_handlerCount == kHandlerQueueSlots)
		{
			++self.m_handlerDropped;
			return;
		}
		self.m_handlerQueue[self.m_handlerCount++] = event.reason;
	}

	static EngineLaneDriverCore& coreOf(void* ctx) { return *static_cast<EngineLaneDriverCore*>(ctx); }

	static SimLaneEndpointOps simEndpointOps()
	{
		SimLaneEndpointOps ops;
		ops.send = [](void* ctx, ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) {
			if (coreOf(ctx).m_endpoint.has_value())
			{
				coreOf(ctx).m_endpoint->send(id, lane, bytes);
			}
		};
		ops.broadcastExcept = [](void* ctx, ognet::ConnectionId skip, ognet::Lane lane, std::span<const std::byte> bytes) {
			if (coreOf(ctx).m_endpoint.has_value())
			{
				coreOf(ctx).m_endpoint->broadcastExcept(skip, lane, bytes);
			}
		};
		ops.setLaneHandler = [](void* ctx, ognet::Lane lane, ognet::LaneHandler handler) {
			OGNET_CHECK(coreOf(ctx).m_attaching,
				"SimLaneEndpoint::setLaneHandler outside the attach's beforeStart: the core's sim endpoint takes handlers only there, "
				"before og-net's endpoint starts");
			coreOf(ctx).m_endpoint->setLaneHandler(lane, handler);
		};
		ops.setConnectionHandler = [](void* ctx, ognet::ConnectionHandler handler) {
			OGNET_CHECK(coreOf(ctx).m_attaching,
				"SimLaneEndpoint::setConnectionHandler outside the attach's beforeStart: og-net's thread reads the forward target from "
				"the endpoint's start on, so it is stored before start only");
			coreOf(ctx).m_forward = handler;
		};
		ops.disconnect = [](void* ctx, ognet::ConnectionId id) {
			if (coreOf(ctx).m_endpoint.has_value())
			{
				coreOf(ctx).m_endpoint->disconnect(id);
			}
		};
		ops.noteDelivered = [](void* ctx, ognet::Lane lane, double recvSeconds, double nowSeconds) {
			if (coreOf(ctx).m_window.has_value())
			{
				coreOf(ctx).m_window->noteDelivered(lane, recvSeconds, nowSeconds);
			}
		};
		ops.config = [](const void* ctx) -> const ognet::EndpointConfig& { return static_cast<const EngineLaneDriverCore*>(ctx)->m_config; };
		ops.serviceUntil = [](void* ctx, double deadlineSeconds) { return coreOf(ctx).serviceUntil(deadlineSeconds); };
		ops.hasHighResolutionWait = [](const void* ctx) { return static_cast<const EngineLaneDriverCore*>(ctx)->hasHighResolutionWait(); };
		return ops;
	}

	static void onPolled(void* ctx, const ognet::EngineEvent& event)
	{
		EngineLaneDriverCore& self = *static_cast<EngineLaneDriverCore*>(ctx);
		// ⛔G-01  docs/EngineLaneDriverCore-guards.md
		self.copyIntoBatch(event);
	}

	void copyIntoBatch(const ognet::EngineEvent& event)
	{
		OGNET_CHECK(m_batchCount < kBatchEvents, "EngineLaneDriverCore: the ring delivered more events than the batch asked for");
		OGNET_CHECK(event.bytes.size() <= kBatchEventBytes, "EngineLaneDriverCore: an engine event is larger than a ring slot");
		BatchEvent& slot = m_batch[m_batchCount++];
		slot.kind = event.kind;
		slot.id = event.id;
		slot.reason = event.reason;
		slot.recvSeconds = event.recvSeconds;
		slot.size = static_cast<uint16_t>(event.bytes.size());
		if (!event.bytes.empty())
		{
			std::memcpy(slot.bytes.data(), event.bytes.data(), event.bytes.size());
		}
	}

	void drainHandlerQueue(double nowSeconds, bool clientConnectionPending)
	{
		std::size_t count = 0;
		{
			const std::lock_guard<std::mutex> lock(m_handlerMutex);
			count = m_handlerCount;
			std::copy(m_handlerQueue.begin(), m_handlerQueue.begin() + static_cast<std::ptrdiff_t>(count), m_handlerDrain.begin());
			m_handlerCount = 0;
		}
		for (std::size_t i = 0; i < count && isRunning(); ++i)
		{
			if (m_client != ClientState::Connecting)
			{
				continue;
			}
			const ognet::DisconnectReason reason = m_handlerDrain[i];
			if (reason == ognet::DisconnectReason::ConnectFailed && clientConnectionPending && nowSeconds < m_deadlineSeconds)
			{
				++m_stats.retries;
				m_endpoint->connect(m_serverAddress);
				continue;
			}
			m_client = ClientState::Ended;
			m_preConnectCount = 0;
			m_callbacks.connectEnded(m_callbacks.ctx, reason);
		}
	}

	void pollRing()
	{
		while (isRunning())
		{
			m_batchCount = 0;
			const std::size_t polled = m_lane->poll(ognet::EnginePollFn{&EngineLaneDriverCore::onPolled, this}, kBatchEvents);
			OGNET_CHECK(polled == m_batchCount, "EngineLaneDriverCore: the ring's count and the batch's count differ");
			if (polled == 0)
			{
				return;
			}
			++m_stats.batches;
			m_stats.maxBatch = std::max<uint64_t>(m_stats.maxBatch, polled);
			noteBatchInWindow(polled, m_host->nowSeconds());
			for (std::size_t i = 0; i < polled; ++i)
			{
				dispatch(m_batch[i]);
				// ⛔G-04  docs/EngineLaneDriverCore-guards.md
				if (!isRunning())
				{
					return;
				}
			}
			if (polled < kBatchEvents)
			{
				return;
			}
		}
	}

	void noteBatchInWindow(std::size_t count, double polledSeconds)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			const BatchEvent& event = m_batch[i];
			switch (event.kind)
			{
			case ognet::EngineEventKind::Datagram:
				m_window->noteDelivered(ognet::Lane::Engine, event.recvSeconds, polledSeconds);
				break;
			case ognet::EngineEventKind::Connected:
				m_window->noteConnection(ognet::ConnectionEvent{.kind = ognet::ConnectionEventKind::Connected, .id = event.id});
				break;
			case ognet::EngineEventKind::Disconnected:
				m_window->noteConnection(
					ognet::ConnectionEvent{.kind = ognet::ConnectionEventKind::Disconnected, .id = event.id, .reason = event.reason});
				break;
			}
		}
	}

	void closeWindowIfDue(double nowSeconds)
	{
		if (!isRunning())
		{
			return;
		}
		const ognet::EngineLaneStats engine = m_lane->stats();
		m_window->closeIfDue(
			nowSeconds, *m_endpoint, ognet::LineSink{&EngineLaneDriverCore::onWindowLine, this}, ognet::NetWindowExtras{.engine = &engine});
	}

	static void onWindowLine(void* ctx, const char* line)
	{
		static_cast<EngineLaneDriverCore*>(ctx)->m_host->log(static_cast<int>(ognet::LogLevel::Info), line);
	}

	void dispatch(const BatchEvent& event)
	{
		switch (event.kind)
		{
		case ognet::EngineEventKind::Connected:
			onConnected(event.id);
			return;
		case ognet::EngineEventKind::Datagram:
			onDatagram(event.id, std::span<const std::byte>(event.bytes.data(), event.size));
			return;
		case ognet::EngineEventKind::Disconnected:
			onDisconnected(event.id, event.reason);
			return;
		}
	}

	void onConnected(ognet::ConnectionId id)
	{
		if (m_role == ognet::Role::Client)
		{
			if (m_client == ClientState::Ended)
			{
				m_endpoint->disconnect(id);
				return;
			}
			m_serverId = id;
			m_client = ClientState::Up;
			insert(id, RecordState::Live);
			flushPreConnect(id);
			m_callbacks.serverConnectionUp(m_callbacks.ctx, id);
			return;
		}
		const AcceptVerdict verdict = m_callbacks.accept(m_callbacks.ctx, id);
		if (verdict == AcceptVerdict::Accept)
		{
			++m_stats.accepted;
			insert(id, RecordState::Live);
			return;
		}
		if (verdict == AcceptVerdict::Reject)
		{
			++m_stats.rejected;
			m_endpoint->disconnect(id);
			return;
		}
		insert(id, RecordState::Pending);
	}

	void onDatagram(ognet::ConnectionId id, std::span<const std::byte> bytes)
	{
		Record* record = find(id);
		if (record == nullptr || record->state == RecordState::Draining)
		{
			++m_stats.unknownDatagrams;
			return;
		}
		if (record->state == RecordState::Pending)
		{
			const AcceptVerdict verdict = m_callbacks.accept(m_callbacks.ctx, id);
			if (!isRunning())
			{
				return;
			}
			record = find(id);
			if (record == nullptr || record->state != RecordState::Pending)
			{
				++m_stats.ignoredDatagrams;
				return;
			}
			if (verdict == AcceptVerdict::Reject)
			{
				++m_stats.rejected;
				++m_stats.ignoredDatagrams;
				erase(*record);
				m_endpoint->disconnect(id);
				return;
			}
			if (verdict == AcceptVerdict::Ignore)
			{
				++m_stats.ignoredDatagrams;
				return;
			}
			++m_stats.accepted;
			record->state = RecordState::Live;
		}
		m_callbacks.deliver(m_callbacks.ctx, id, bytes);
	}

	void onDisconnected(ognet::ConnectionId id, ognet::DisconnectReason reason)
	{
		Record* record = find(id);
		if (record == nullptr)
		{
			return;
		}
		const bool live = record->state == RecordState::Live;
		erase(*record);
		if (m_role == ognet::Role::Client && id == m_serverId)
		{
			m_client = ClientState::Ended;
		}
		if (live)
		{
			m_callbacks.closed(m_callbacks.ctx, id, reason);
		}
	}

	void serviceDrainedCloses(double nowSeconds)
	{
		std::size_t i = 0;
		while (i < m_recordCount && isRunning())
		{
			Record& record = m_records[i];
			if (record.state != RecordState::Draining)
			{
				++i;
				continue;
			}
			if (m_lane->queuedBytes(record.id) == 0)
			{
				++m_stats.closesDrained;
			}
			else if (nowSeconds >= record.closedAtSeconds + kDrainedCloseSeconds)
			{
				++m_stats.closesTimedOut;
			}
			else
			{
				++i;
				continue;
			}
			const ognet::ConnectionId id = record.id;
			erase(record);
			m_endpoint->disconnect(id);
		}
	}

	SendVerdict bufferPreConnect(std::span<const std::byte> bytes)
	{
		if (m_client == ClientState::Ended)
		{
			return dead();
		}
		if (bytes.size() > kPreConnectSlotBytes)
		{
			++m_stats.sendsRefused;
			return SendVerdict::Refused;
		}
		if (m_preConnectCount == kPreConnectSlots)
		{
			++m_stats.preConnectDropped;
			return SendVerdict::BufferFull;
		}
		PreConnectSlot& slot = m_preConnect[m_preConnectCount++];
		slot.size = static_cast<uint16_t>(bytes.size());
		if (!bytes.empty())
		{
			std::memcpy(slot.bytes.data(), bytes.data(), bytes.size());
		}
		++m_stats.preConnectBuffered;
		return SendVerdict::Buffered;
	}

	void flushPreConnect(ognet::ConnectionId id)
	{
		// ⛔G-03  docs/EngineLaneDriverCore-guards.md
		for (std::size_t i = 0; i < m_preConnectCount; ++i)
		{
			const PreConnectSlot& slot = m_preConnect[i];
			sendThroughLane(id, std::span<const std::byte>(slot.bytes.data(), slot.size));
		}
		m_preConnectCount = 0;
	}

	SendVerdict sendThroughLane(ognet::ConnectionId id, std::span<const std::byte> bytes)
	{
		if (m_lane->send(id, bytes))
		{
			return SendVerdict::Sent;
		}
		if (endpointReportsNotConnected(id))
		{
			return dead();
		}
		++m_stats.sendsRefused;
		return SendVerdict::Refused;
	}

	bool endpointReportsNotConnected(ognet::ConnectionId id)
	{
		const uint64_t unknownSends = m_lane->stats().sendsToUnknownConnection;
		const bool laneDroppedTheId = unknownSends != m_laneUnknownSends;
		m_laneUnknownSends = unknownSends;
		return laneDroppedTheId || m_endpoint->stats(id).state != ognet::ConnectionState::Connected;
	}

	SendVerdict dead()
	{
		++m_stats.sendsDead;
		return SendVerdict::Dead;
	}

	bool anyQueued() const
	{
		for (std::size_t i = 0; i < m_recordCount; ++i)
		{
			if (m_records[i].state != RecordState::Pending && m_lane->queuedBytes(m_records[i].id) != 0)
			{
				return true;
			}
		}
		return false;
	}

	Record* find(ognet::ConnectionId id)
	{
		for (std::size_t i = 0; i < m_recordCount; ++i)
		{
			if (m_records[i].id == id)
			{
				return &m_records[i];
			}
		}
		return nullptr;
	}

	const Record* find(ognet::ConnectionId id) const { return const_cast<EngineLaneDriverCore*>(this)->find(id); }

	void insert(ognet::ConnectionId id, RecordState state)
	{
		OGNET_CHECK(m_recordCount < m_recordCapacity,
			"EngineLaneDriverCore: more engine connections than endpoint slots (og-net delivers a slot's Disconnected before its "
			"next Connected)");
		m_records[m_recordCount++] = Record{id, state, 0.0};
	}

	void erase(Record& record)
	{
		record = m_records[m_recordCount - 1];
		--m_recordCount;
	}

	HostT* m_host = nullptr;
	BackendT* m_backend = nullptr;
	IoMode m_mode = IoMode::OwnThread;
	ognet::Role m_role = ognet::Role::Client;
	Callbacks m_callbacks;
	bool m_started = false;
	bool m_stopped = false;

	ClientState m_client = ClientState::Idle;
	ognet::ConnectionId m_serverId = ognet::kInvalidConnection;
	ognet::Address m_serverAddress{};
	double m_deadlineSeconds = 0.0;

	std::unique_ptr<Record[]> m_records;
	std::size_t m_recordCapacity = 0;
	std::size_t m_recordCount = 0;

	std::unique_ptr<PreConnectSlot[]> m_preConnect;
	std::size_t m_preConnectCount = 0;

	std::unique_ptr<BatchEvent[]> m_batch;
	std::size_t m_batchCount = 0;

	mutable std::mutex m_handlerMutex;
	std::array<ognet::DisconnectReason, kHandlerQueueSlots> m_handlerQueue{};
	std::size_t m_handlerCount = 0;
	uint64_t m_handlerDropped = 0;
	std::array<ognet::DisconnectReason, kHandlerQueueSlots> m_handlerDrain{};

	DriverStats m_stats;
	uint64_t m_laneUnknownSends = 0;
	std::optional<ognet::NetWindow> m_window;

	ognet::EndpointConfig m_config{};
	bool m_attaching = false;
	ognet::ConnectionHandler m_forward{};
	SimLaneEndpoint m_simEndpoint;

	ognet::EngineLane* m_lane = nullptr;
	std::optional<Endpoint> m_endpoint;
};
} // namespace ognetunreal
