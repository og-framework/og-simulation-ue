#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../OGSimSession/docs/HeadlessPlatform-rationale.md

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "glm/gtc/quaternion.hpp"
#include "glm/vec3.hpp"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/StdHostServices.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimSession/SessionEngine.h"
#include "OGTickDriver/ThreadChainExecutor.h"
#include "VirtualNetwork.h"

namespace headlessSession
{
struct PeerContext
{
	std::function<void(const char*)> sink;
	std::atomic<uint64_t>            frames{ 0u };
};

inline thread_local PeerContext* t_peer = nullptr;

inline void logLine(const char* line)
{
	if (t_peer != nullptr && t_peer->sink)
		t_peer->sink(line);
}

inline double monotonicSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct PeerScope
{
	explicit PeerScope(PeerContext& peer) : previous(std::exchange(t_peer, &peer)) {}
	~PeerScope() { t_peer = previous; }
	PeerContext* previous;
};

class PeerThreads : public ognet::StdHostServices
{
public:
	void startThread(const char* name, int priority, void (*fn)(void*), void* arg)
	{
		StdHostServices::startThread(name, priority, &PeerThreads::run, new Start{ fn, arg, t_peer, &m_started });
	}

	void log(int, const char* line) { logLine(line); }
	std::thread::id started() const { return m_started.load(); }

private:
	struct Start
	{
		void (*fn)(void*);
		void* arg;
		PeerContext* peer;
		std::atomic<std::thread::id>* started;
	};

	static void run(void* self)
	{
		const Start start = *static_cast<Start*>(self);
		delete static_cast<Start*>(self);
		t_peer = start.peer;
		start.started->store(std::this_thread::get_id());
		start.fn(start.arg);
	}

	std::atomic<std::thread::id> m_started{};
};

class Runner
{
public:
	struct Host
	{
		void sleepUntil(double seconds)
		{
			std::this_thread::sleep_for(std::chrono::duration<double>(std::max(0.0, seconds - monotonicSeconds())));
		}
	};

	template <typename F>
	explicit Runner(F&& body) : m_body(std::forward<F>(body)), m_thread([this, peer = t_peer] { t_peer = peer; m_body(*this); }) {}
	~Runner() { join(); }
	Runner(const Runner&) = delete;
	Runner& operator=(const Runner&) = delete;

	Host& host() { return m_host; }
	const std::atomic<bool>& stopRequested() const { return m_stopRequested; }

	void join()
	{
		m_stopRequested.store(true, std::memory_order_release);
		if (m_thread.joinable())
			m_thread.join();
	}

	void waitForStart()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_latch.wait(lock, [this] { return m_started; });
	}

	void releaseStart()
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_started = true;
		}
		m_latch.notify_all();
	}

private:
	Host                         m_host;
	std::atomic<bool>            m_stopRequested{ false };
	std::mutex                   m_mutex;
	std::condition_variable      m_latch;
	bool                         m_started = false;
	std::function<void(Runner&)> m_body;
	std::thread                  m_thread;
};

class Engine
{
public:
	static constexpr bool kHostDiagnostics = true;
	using Handle     = ThreadChainExecutor::Handle;
	using Completion = ThreadChainExecutor::Completion;
	using Runner     = headlessSession::Runner;

	Engine() { OG_CHECK(m_peer != nullptr, "headlessSession::Engine: a peer's engines are built inside its PeerScope"); }

	double   nowSeconds() const { return monotonicSeconds(); }
	uint64_t frameNumber() const { return m_peer->frames.load(std::memory_order_relaxed); }
	void     advanceFrame() { m_peer->frames.fetch_add(1u, std::memory_order_relaxed); }
	bool     logEnabled(HostLogLevel) const { return true; }
	void     log(HostLogLevel, const char* line) { logLine(line); }
	void     logRouted(const char* line) { logLine(line); }
	bool     wants(SimulationLogRoute, HostLogLevel) const { return true; }
	bool     isPending(const Handle& handle) const { return m_executor.isPending(handle); }
	void     waitFor(const Handle& handle) { m_executor.waitFor(handle); }

	template <typename F>
	Handle dispatchAfter(const Handle& previous, F&& task) { return m_executor.dispatchAfter(previous, std::forward<F>(task)); }

	template <typename F>
	bool deferUntil(Completion& done, const Handle& handle, F&& task) { return m_executor.deferUntil(done, handle, std::forward<F>(task)); }

	template <typename F>
	std::unique_ptr<Runner> startRunner(F&& body, bool) { return std::make_unique<Runner>(std::forward<F>(body)); }

private:
	PeerContext*        m_peer = t_peer;
	PeerThreads         m_threads;
	ThreadChainExecutor m_executor{ m_threads, "og-step", 0 };
};

static_assert(ogsession::SessionEngine<Engine>,
	"headlessSession::Engine models og-sim-session's SessionEngine over std::thread and the steady clock: the tick driver's "
	"engine (the executor over ThreadChainExecutor), the routed log sink and the runner factory");

struct ClockEngine
{
	void logRouted(const char* line) { logLine(line); }
	double nowSeconds() const { return monotonicSeconds(); }
};

class Endpoint
{
public:
	using Net = ognet::NetEndpoint<PeerThreads, ognet::test::VirtualBackend>;

	Endpoint(PeerThreads& host, ognet::test::VirtualBackend& backend, const ognet::EndpointConfig& config)
		: m_net(host, backend)
		, m_config(config)
	{
	}

	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { m_net.send(id, lane, bytes); }
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { m_net.broadcastExcept(id, lane, bytes); }
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler) { m_net.setLaneHandler(lane, handler); }
	void disconnect(ognet::ConnectionId id) { m_net.disconnect(id); }
	void noteDelivered(ognet::Lane, double, double) {}
	const ognet::EndpointConfig& config() const { return m_config; }
	ognet::ServiceResult serviceUntil(double deadlineSeconds) { return m_net.serviceUntil(deadlineSeconds); }
	bool hasHighResolutionWait() const { return false; }
	Net& net() { return m_net; }

	double serverSrttSeconds() const
	{
		const ognet::ConnectionStats stats = m_net.stats(m_serverConnection.load(std::memory_order_acquire));
		return stats.hasRttSample ? stats.smoothedRttSeconds : ogsimnet::kSimNetLinkNoRttSeconds;
	}

	ognet::ConnectionStats stats(ognet::ConnectionId id) const { return m_net.stats(id); }

	void setConnectionHandler(ognet::ConnectionHandler handler)
	{
		m_forward = handler;
		m_net.setConnectionHandler(ognet::ConnectionHandler{ &Endpoint::onConnectionEvent, this });
	}

	std::vector<ognet::ConnectionEvent> takeConnectionEvents()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return std::exchange(m_events, {});
	}

private:
	static void onConnectionEvent(void* self, const ognet::ConnectionEvent& event)
	{
		Endpoint& endpoint = *static_cast<Endpoint*>(self);
		if (endpoint.m_config.role == ognet::Role::Client && event.kind == ognet::ConnectionEventKind::Connected)
			endpoint.m_serverConnection.store(event.id, std::memory_order_release);
		{
			const std::lock_guard<std::mutex> lock(endpoint.m_mutex);
			endpoint.m_events.push_back(event);
		}
		endpoint.m_forward.fn(endpoint.m_forward.ctx, event);
	}

	Net                                 m_net;
	ognet::EndpointConfig               m_config;
	ognet::ConnectionHandler            m_forward;
	std::atomic<ognet::ConnectionId>    m_serverConnection{ ognet::kInvalidConnection };
	std::mutex                          m_mutex;
	std::vector<ognet::ConnectionEvent> m_events;
};

struct NullRenderSink
{
	std::optional<glm::vec3> applyRootPose(uint32_t, const glm::vec3&, const std::optional<glm::quat>&) { return std::nullopt; }
	std::optional<glm::vec3> readAnchor(uint32_t) const { return std::nullopt; }
	const char* name(uint32_t) const { return "headless"; }
};
} // namespace headlessSession
