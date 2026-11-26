#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetLink-rationale.md · docs/SimNetLink-guards.md

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/PCTimeManagement/NetworkTimeEstimator.h"

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>

namespace ogsimnet
{
inline constexpr size_t kSimNetLinkInboxSlots = 2048u;
inline constexpr double kSimNetLinkNoRttSeconds = -1.0;
inline constexpr char kSimNetRosterLinePrefix[] = "[Roster.";
inline constexpr char kSimNetWarningToken[] = "[Warning]";
inline constexpr size_t kSimNetWarnedRosterLineBytes = kRosterLogLineBytes + sizeof(kSimNetWarningToken);
inline constexpr size_t kSimNetWarnedHeldLineBytes = kClockSourceHeldLineBytes + sizeof(kSimNetWarningToken);
inline constexpr char kSimNetDisconnectFormat[] =
	"[Error][SimNet.Host] role=client disconnect conn=%u reason=%s: the server's roster was refused, so this client leaves the server";
inline constexpr size_t kSimNetDisconnectLineBytes = 192u;

static_assert(kSimNetDisconnectLineBytes >= sizeof(kSimNetDisconnectFormat) + 10u + 32u,
	"kSimNetDisconnectLineBytes holds the disconnect line with a 10-digit connection id and a refusal name of 32 characters");
static_assert(kSimNetLinkNoRttSeconds < 0.0,
	"The link's no-reading round trip is negative: NetworkTimeEstimator::updateRTT rejects and counts a negative sample, while "
	"0 is a real 0 ms ping");

template <class L>
concept SimNetLinkLock = requires(L& lock)
{
	{ lock.lock() } -> std::same_as<void>;
	{ lock.unlock() } -> std::same_as<void>;
};

template <class S>
concept SimNetLinkServices = requires(S& services, const char* line, uint8_t value)
{
	{ services.nowSeconds() } -> std::same_as<double>;
	{ services.logRouted(line) } -> std::same_as<void>;
	{ services.logHost(line) } -> std::same_as<void>;
	{ services.serverRttSeconds() } -> std::same_as<double>;
	{ services.onWireMismatch(value, value) } -> std::same_as<void>;
};

template <class H>
concept SimNetLinkSession = requires(H& session, uint32_t tick, double seconds, uint8_t value, const SimClockRttSample& sample,
	SimNetStream stream)
{
	{ session.onTimingInfo(tick, seconds) } -> std::same_as<void>;
	{ session.onRelayFloor(value) } -> std::same_as<void>;
	{ session.onConnectionTier(value, value) } -> std::same_as<void>;
	{ session.onClockSample(sample) } -> std::same_as<void>;
	{ session.onLifecycleUnregistering(value) } -> std::same_as<void>;
	{ session.stampClientReceived(stream, value, tick, seconds) } -> std::same_as<void>;
};

template <class E>
concept SimNetLinkEndpoint = SimNetEndpoint<E> && requires(E& endpoint, const E& constEndpoint, ognet::Lane lane, double seconds)
{
	{ constEndpoint.config() } -> std::same_as<const ognet::EndpointConfig&>;
	{ endpoint.noteDelivered(lane, seconds, seconds) } -> std::same_as<void>;
};

template <class SinkFn>
void routeSimNetHostLine(const char* line, SinkFn&& sink)
{
	if (std::strncmp(line, kSimNetRosterLinePrefix, sizeof(kSimNetRosterLinePrefix) - 1u) != 0)
	{
		sink(line);
		return;
	}
	std::array<char, kSimNetWarnedRosterLineBytes> warned{};
	std::snprintf(warned.data(), warned.size(), "%s%s", kSimNetWarningToken, line);
	sink(warned.data());
}

template <SimNetLinkServices ServicesT>
void routeSimNetClientLine(ServicesT& services, const char* line)
{
	if (std::strncmp(line, kClockSourceHeldTag, sizeof(kClockSourceHeldTag) - 1u) == 0)
	{
		std::array<char, kSimNetWarnedHeldLineBytes> warned{};
		std::snprintf(warned.data(), warned.size(), "%s%s", kSimNetWarningToken, line);
		services.logRouted(warned.data());
		return;
	}
	routeSimNetHostLine(line, [&services](const char* routed) { services.logHost(routed); });
}

template <class EndpointT>
void noteSimLaneDelivered(EndpointT& endpoint, SimLane lane, double recvSeconds, double nowSeconds)
{
	endpoint.noteDelivered(static_cast<ognet::Lane>(lane), recvSeconds, nowSeconds);
}

template <SimNetLinkLock LockT>
class LockedSimNetBindings
{
public:
	void onRosterAdded(const RosterEntry& entry)
	{
		const std::scoped_lock lock(m_lock);
		m_bindings.onRosterAdded(entry);
	}

	bool onRosterRemoved(uint8_t simId)
	{
		const std::scoped_lock lock(m_lock);
		return m_bindings.onRosterRemoved(simId);
	}

	void seedFrom(const RosterMirror& mirror)
	{
		const std::scoped_lock lock(m_lock);
		m_bindings.seedFrom(mirror);
	}

	BindVerdict verdict(uint8_t simId, bool engineSaysLocal) const
	{
		const std::scoped_lock lock(m_lock);
		return m_bindings.verdict(simId, engineSaysLocal);
	}

	BindVerdict markRegisteredIfReady(uint8_t simId, bool engineSaysLocal)
	{
		const std::scoped_lock lock(m_lock);
		const BindVerdict verdict = m_bindings.verdict(simId, engineSaysLocal);
		if (verdict.step == BindStep::Ready)
			m_bindings.markRegistered(simId);
		return verdict;
	}

	void markRegistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_lock);
		m_bindings.markRegistered(simId);
	}

	bool markUnregistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_lock);
		return m_bindings.markUnregistered(simId);
	}

	bool isRegistered(uint8_t simId) const
	{
		const std::scoped_lock lock(m_lock);
		return m_bindings.isRegistered(simId);
	}

private:
	mutable LockT m_lock;
	SimNetBindings m_bindings;
};

class SimNetLinkSessionRef
{
public:
	SimNetLinkSessionRef() = default;

	template <SimNetLinkSession H>
	explicit SimNetLinkSessionRef(H& session)
		: m_self(&session)
		, m_onTimingInfo([](void* self, uint32_t tick, double rttSeconds) { static_cast<H*>(self)->onTimingInfo(tick, rttSeconds); })
		, m_onRelayFloor([](void* self, uint8_t floorTicks) { static_cast<H*>(self)->onRelayFloor(floorTicks); })
		, m_onConnectionTier([](void* self, uint8_t oldTier, uint8_t newTier) { static_cast<H*>(self)->onConnectionTier(oldTier, newTier); })
		, m_onClockSample([](void* self, const SimClockRttSample& sample) { static_cast<H*>(self)->onClockSample(sample); })
		, m_onLifecycleUnregistering([](void* self, uint8_t simId) { static_cast<H*>(self)->onLifecycleUnregistering(simId); })
		, m_stampClientReceived([](void* self, SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds) {
			static_cast<H*>(self)->stampClientReceived(stream, simId, tick, recvSeconds);
		})
	{
	}

	explicit operator bool() const { return m_self != nullptr; }

	void onTimingInfo(uint32_t tick, double rttSeconds) const { m_onTimingInfo(m_self, tick, rttSeconds); }
	void onRelayFloor(uint8_t floorTicks) const { m_onRelayFloor(m_self, floorTicks); }
	void onConnectionTier(uint8_t oldTier, uint8_t newTier) const { m_onConnectionTier(m_self, oldTier, newTier); }
	void onClockSample(const SimClockRttSample& sample) const { m_onClockSample(m_self, sample); }
	void onLifecycleUnregistering(uint8_t simId) const { m_onLifecycleUnregistering(m_self, simId); }
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds) const
	{
		m_stampClientReceived(m_self, stream, simId, tick, recvSeconds);
	}

private:
	void* m_self = nullptr;
	void (*m_onTimingInfo)(void*, uint32_t, double) = nullptr;
	void (*m_onRelayFloor)(void*, uint8_t) = nullptr;
	void (*m_onConnectionTier)(void*, uint8_t, uint8_t) = nullptr;
	void (*m_onClockSample)(void*, const SimClockRttSample&) = nullptr;
	void (*m_onLifecycleUnregistering)(void*, uint8_t) = nullptr;
	void (*m_stampClientReceived)(void*, SimNetStream, uint8_t, uint32_t, double) = nullptr;
};

template <SimNetLinkServices ServicesT, SimNetLinkLock LockT, SimNetLinkEndpoint EndpointT>
class SimNetLinkClientHost
{
public:
	using RegisteredRemovedFn = std::function<void(uint8_t simId)>;

	SimNetLinkClientHost(ServicesT& services, EndpointT& endpoint, LockedSimNetBindings<LockT>& bindings)
		: m_services(services), m_endpoint(endpoint), m_bindings(bindings)
	{
	}

	SimNetLinkClientHost(const SimNetLinkClientHost&) = delete;
	SimNetLinkClientHost& operator=(const SimNetLinkClientHost&) = delete;

	void setRoster(const RosterMirror& roster) { m_roster = &roster; }
	void setOnRegisteredRemoved(RegisteredRemovedFn onRegisteredRemoved) { m_onRegisteredRemoved = std::move(onRegisteredRemoved); }
	void attach(SimNetLinkSessionRef session) { m_session = session; }
	void detach() { m_session = SimNetLinkSessionRef{}; }

	double nowSeconds() { return m_services.nowSeconds(); }

	double serverRttSeconds()
	{
		if (!m_session)
			return kSimNetLinkNoRttSeconds;
		return m_services.serverRttSeconds();
	}

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds)
	{
		if (m_session)
			m_session.onTimingInfo(authorityTick, roundTripSeconds);
	}

	void onRelayFloor(uint8_t floorTicks, [[maybe_unused]] bool replayed)
	{
		if (!m_session)
			return;
		// ⛔G-04  docs/SimNetLink-guards.md
		m_session.onRelayFloor(floorTicks);
	}

	void onConnectionTier(uint8_t oldTier, uint8_t newTier, [[maybe_unused]] bool replayed)
	{
		if (m_session)
			m_session.onConnectionTier(oldTier, newTier);
	}

	void onClockSample(const SimClockRttSample& sample)
	{
		if (m_session)
			m_session.onClockSample(sample);
	}

	void onLifecycleUnregistering(uint8_t simId)
	{
		if (m_session)
			m_session.onLifecycleUnregistering(simId);
	}

	void onRosterAdded(uint8_t simId)
	{
		const RosterEntry* const entry = m_roster != nullptr ? m_roster->entryFor(simId) : nullptr;
		OG_CHECK(entry != nullptr,
			"SimNetLinkClientHost::onRosterAdded: the sim id is not in the roster mirror. The mirror reports an Add after it applied "
			"it, and the link sets the mirror before the first drain");
		if (entry != nullptr && m_session)
			m_bindings.onRosterAdded(*entry);
	}

	void onRosterRemoved(uint8_t simId)
	{
		if (!m_session)
			return;
		const bool wasRegistered = m_bindings.onRosterRemoved(simId);
		OG_CHECK(!wasRegistered || static_cast<bool>(m_onRegisteredRemoved),
			"SimNetLinkClientHost::onRosterRemoved: the sim id is registered and no unregister handler is set. The client session "
			"sets one (SimNetLink::setOnRegisteredRemoved) before it registers a character");
		if (wasRegistered && m_onRegisteredRemoved)
		{
			// ⛔G-03  docs/SimNetLink-guards.md
			m_onRegisteredRemoved(simId);
		}
	}

	void onWireMismatch(uint8_t received, uint8_t expected)
	{
		// ⛔G-05  docs/SimNetLink-guards.md
		m_services.onWireMismatch(received, expected);
	}

	void log(const char* line) { routeSimNetClientLine(m_services, line); }

	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds)
	{
		if (m_session)
			m_session.stampClientReceived(stream, simId, tick, recvSeconds);
	}

	void noteDelivered(SimLane lane, double recvSeconds) { noteSimLaneDelivered(m_endpoint, lane, recvSeconds, m_services.nowSeconds()); }

private:
	ServicesT& m_services;
	EndpointT& m_endpoint;
	LockedSimNetBindings<LockT>& m_bindings;
	const RosterMirror* m_roster = nullptr;
	SimNetLinkSessionRef m_session;
	RegisteredRemovedFn m_onRegisteredRemoved;
};

template <SimNetGame GameT, SimNetLinkEndpoint EndpointT, SimNetClockSource ClockT, SimNetLinkServices ServicesT,
	SimNetLinkLock LockT>
class SimNetLink
{
public:
	using Transport   = SimNetTransport<EndpointT, kSimNetLinkInboxSlots>;
	using ClockServer = SimNetClockServer<Transport, ClockT>;
	using ClockClient = SimNetClockClient<Transport, ClockT>;
	using Bindings    = LockedSimNetBindings<LockT>;
	using ClientHost  = SimNetLinkClientHost<ServicesT, LockT, EndpointT>;
	using Client      = SimNetClient<GameT, ClientHost, Transport>;
	using RegisteredRemovedFn = typename ClientHost::RegisteredRemovedFn;

	SimNetLink(ServicesT& services, bool isServer) : m_services(services), m_isServer(isServer)
	{
		static_assert(compositionContractsHold());
		static_assert(SimNetClockRttSampleHook<ClientHost>,
			"SimNetLinkClientHost has onClockSample(const SimClockRttSample&), so the client session hands it every SimClock "
			"sample and feeds the one it held while unbound ahead of the first authority tick after the attach. Without the hook "
			"the session counts the samples unheard, the clock stays on the engine ping and the join resyncs a second time "
			"(cause=firstRtt). Was the static_assert on the host's own onTimingInfo (og-input-latency P3-11), whose latch "
			"og-session-runtime task 20 moved into SimNetClient; the host moved here with og-session-runtime task 28");
	}

	SimNetLink(const SimNetLink&) = delete;
	SimNetLink& operator=(const SimNetLink&) = delete;

	void start(EndpointT& endpoint)
	{
		OG_CHECK(m_frameThread == std::thread::id{},
			"SimNetLink::start: a link starts once, and the thread that starts it is its frame thread, which every frame door "
			"checks from then on");
		m_frameThread = std::this_thread::get_id();
		OG_CHECK((endpoint.config().role == ognet::Role::Server) == m_isServer,
			"SimNetLink::start: the link was made for one role and the endpoint is the other role's");
		m_endpoint = &endpoint;
		m_transport.emplace(simNetConfigFor(endpoint.config()));
		if (m_isServer)
			m_clockServer.emplace(*m_transport, m_clock);
		else
			m_clockClient.emplace(*m_transport, m_clock);
		m_transport->installHandlers(endpoint);
		endpoint.setConnectionHandler(m_transport->connectionHandler());
		if (m_isServer)
			return;
		m_clientHost.emplace(m_services, endpoint, m_bindings);
		// ⛔G-01  docs/SimNetLink-guards.md
		m_client.emplace(*m_clientHost, *m_transport);
		m_clientHost->setRoster(m_client->roster());
	}

	void markStarted()
	{
		checkFrameThread();
		OG_CHECK(m_transport.has_value(), "SimNetLink::markStarted before start: the link builds its transport in start");
		m_transport->markEndpointStarted();
	}

	template <class AfterPingFn>
	void serviceFrame(AfterPingFn&& afterPing)
	{
		checkFrameThread();
		OG_CHECK(m_client.has_value(), "SimNetLink::serviceFrame: only a started client's link has a frame service");
		m_clockClient->servicePing();
		afterPing();
		// ⛔G-02  docs/SimNetLink-guards.md
		if (m_clientSessionBound.load(std::memory_order_relaxed))
			return;
		m_client->drain();
		actOnDisconnectRequest();
	}

	void flushFrame()
	{
		checkFrameThread();
		if (m_isServer)
			return;
		OG_CHECK(m_client.has_value(), "SimNetLink::flushFrame on a client before start");
		if (m_clientSessionBound.load(std::memory_order_relaxed))
			return;
		m_client->flushFrame();
	}

	void drainForStep()
	{
		OG_CHECK(m_frameThread != std::thread::id{} && std::this_thread::get_id() != m_frameThread,
			"SimNetLink::drainForStep: an attached client drains at the start of each step, on the step's thread; on the frame "
			"thread every consumer the drain reaches would race the step. Was the game thread's frame-point drain");
		OG_CHECK(m_client.has_value() && m_clientSessionBound.load(std::memory_order_relaxed),
			"SimNetLink::drainForStep: only an attached client's session drains from its steps");
		m_client->drain();
		actOnDisconnectRequest();
		m_client->flushFrame();
	}

	void drainWhileStopped()
	{
		checkFrameThread();
		OG_CHECK(m_client.has_value(),
			"SimNetLink::drainWhileStopped: the frame thread drains an attached client only at its session's teardown, after the "
			"steps have stopped, to apply the unregistrations it queued");
		m_client->drain();
	}

	template <SimNetLinkSession SessionT>
	void attachSession(SessionT& session)
	{
		checkFrameThread();
		OG_CHECK(!m_isServer && m_client.has_value(), "SimNetLink::attachSession: only a started client's link holds a client session");
		m_clientHost->attach(SimNetLinkSessionRef(session));
		m_client->bindListener();
		m_bindings.seedFrom(m_client->roster());
		m_clientSessionBound.store(true, std::memory_order_relaxed);
	}

	void detachSession()
	{
		checkFrameThread();
		if (!m_client.has_value())
			return;
		m_client->unbindListener();
		m_clientHost->detach();
		m_clientSessionBound.store(false, std::memory_order_relaxed);
	}

	void setOnRegisteredRemoved(RegisteredRemovedFn onRegisteredRemoved)
	{
		checkFrameThread();
		OG_CHECK(m_clientHost.has_value(), "SimNetLink::setOnRegisteredRemoved: only a started client's link has a client host");
		m_clientHost->setOnRegisteredRemoved(std::move(onRegisteredRemoved));
	}

	void stop()
	{
		checkFrameThread();
		detachSession();
	}

	bool isServer() const { return m_isServer; }
	ClockServer* clockServer() { return m_clockServer.has_value() ? &*m_clockServer : nullptr; }
	const ClockClient* clockClient() const { return m_clockClient.has_value() ? &*m_clockClient : nullptr; }
	bool clientSessionBound() const { return m_clientSessionBound.load(std::memory_order_relaxed); }

	EndpointT& endpoint()
	{
		OG_CHECK(m_endpoint != nullptr, "SimNetLink::endpoint before start");
		return *m_endpoint;
	}

	Transport& transport()
	{
		OG_CHECK(m_transport.has_value(), "SimNetLink::transport before start");
		return *m_transport;
	}

	Client* client() { return m_client.has_value() ? &*m_client : nullptr; }
	ClientHost* clientHost() { return m_clientHost.has_value() ? &*m_clientHost : nullptr; }
	const Bindings& bindings() const { return m_bindings; }
	Bindings& editBindings() { return m_bindings; }

private:
	void checkFrameThread() const
	{
		OG_CHECK(m_frameThread != std::thread::id{} && std::this_thread::get_id() == m_frameThread,
			"SimNetLink: a frame door runs on the thread that started the link (the frame thread), and only after start");
	}

	void actOnDisconnectRequest()
	{
		const std::optional<SimNetDisconnectRequest> request = m_client->takeDisconnectRequest();
		if (!request)
			return;
		std::array<char, kSimNetDisconnectLineBytes> line{};
		std::snprintf(line.data(), line.size(), kSimNetDisconnectFormat, static_cast<unsigned>(request->conn),
			rosterRefusalName(request->reason));
		m_services.logRouted(line.data());
		if (request->conn != kInvalidSimConnection)
			m_endpoint->disconnect(request->conn);
	}

	ServicesT& m_services;
	bool m_isServer;
	std::thread::id m_frameThread;
	std::atomic<bool> m_clientSessionBound{false};
	EndpointT* m_endpoint = nullptr;
	ClockT m_clock;
	std::optional<Transport> m_transport;
	std::optional<ClockServer> m_clockServer;
	std::optional<ClockClient> m_clockClient;
	Bindings m_bindings;
	std::optional<ClientHost> m_clientHost;
	std::optional<Client> m_client;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(SimNetLink, m_clock) < __builtin_offsetof(SimNetLink, m_clockServer)
		           && __builtin_offsetof(SimNetLink, m_transport) < __builtin_offsetof(SimNetLink, m_clockServer),
			"SimNetLink members destruct in REVERSE declaration order. The clock server holds the transport and the clock by "
			"reference and is the transport's SimClock lane handler, so it goes before both. An engine destroys the link only "
			"after og-net's endpoint, so the handler outlives every call og-net can make. Keep m_clock and m_transport < "
			"m_clockServer. Added by og-input-latency task P5-19 (P2-20 F2) on UESimNetLanes; moved by og-session-runtime task 28");
		static_assert(__builtin_offsetof(SimNetLink, m_clock) < __builtin_offsetof(SimNetLink, m_clockClient)
		           && __builtin_offsetof(SimNetLink, m_transport) < __builtin_offsetof(SimNetLink, m_clockClient),
			"SimNetLink members destruct in REVERSE declaration order. The clock client holds the transport and the clock by "
			"reference and is the client transport's SimClock lane handler, so it goes before both, as the clock server does. Keep "
			"m_clock and m_transport < m_clockClient. Added by og-input-latency task P3-10 (P2-20 F2's client half) on "
			"UESimNetLanes; moved by og-session-runtime task 28");
		static_assert(__builtin_offsetof(SimNetLink, m_bindings) < __builtin_offsetof(SimNetLink, m_clientHost)
		           && __builtin_offsetof(SimNetLink, m_clientHost) < __builtin_offsetof(SimNetLink, m_client),
			"SimNetLink members destruct in REVERSE declaration order. The client session holds its host by reference and the host "
			"holds the bindings by reference, so the session goes before the host and the host before the bindings. Keep "
			"m_bindings < m_clientHost < m_client. Was UESimNetLanes-rationale.md §9's prose (og-session-runtime task 28)");
		return true;
	}
};

namespace simNetLinkSelfCheck
{
struct ReferenceLock
{
	void lock();
	void unlock();
};

struct LockWithoutLock
{
	void unlock();
};

struct LockWithoutUnlock
{
	void lock();
};

struct LockWhoseLockReturnsBool
{
	bool lock();
	void unlock();
};

struct LockWhoseUnlockReturnsBool : ReferenceLock
{
	bool unlock();
};

struct ReferenceServices
{
	double nowSeconds();
	void logRouted(const char* line);
	void logHost(const char* line);
	double serverRttSeconds();
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithoutNowSeconds
{
	void logRouted(const char* line);
	void logHost(const char* line);
	double serverRttSeconds();
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithoutLogRouted
{
	double nowSeconds();
	void logHost(const char* line);
	double serverRttSeconds();
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithoutLogHost
{
	double nowSeconds();
	void logRouted(const char* line);
	double serverRttSeconds();
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithoutServerRtt
{
	double nowSeconds();
	void logRouted(const char* line);
	void logHost(const char* line);
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithoutWireMismatch
{
	double nowSeconds();
	void logRouted(const char* line);
	void logHost(const char* line);
	double serverRttSeconds();
};

struct ServicesWithAFloatServerRtt
{
	double nowSeconds();
	void logRouted(const char* line);
	void logHost(const char* line);
	float serverRttSeconds();
	void onWireMismatch(uint8_t received, uint8_t expected);
};

struct ServicesWithAFloatNowSeconds : ReferenceServices
{
	float nowSeconds();
};

struct ServicesWhoseLogRoutedReturnsBool : ReferenceServices
{
	bool logRouted(const char* line);
};

struct ServicesWhoseLogHostReturnsBool : ReferenceServices
{
	bool logHost(const char* line);
};

struct ServicesWhoseWireMismatchReturnsBool : ReferenceServices
{
	bool onWireMismatch(uint8_t received, uint8_t expected);
};

struct ReferenceSession
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutTimingInfo
{
	void onRelayFloor(uint8_t floorTicks);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutRelayFloor
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutConnectionTier
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutClockSample
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutUnregistering
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWithoutStamp
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
};

struct SessionWithAReplayedFloor
{
	void onTimingInfo(uint32_t tick, double rttSeconds);
	void onRelayFloor(uint8_t floorTicks, bool replayed);
	void onConnectionTier(uint8_t oldTier, uint8_t newTier);
	void onClockSample(const SimClockRttSample& sample);
	void onLifecycleUnregistering(uint8_t simId);
	void stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionWhoseTimingInfoReturnsBool : ReferenceSession
{
	bool onTimingInfo(uint32_t tick, double rttSeconds);
};

struct SessionWhoseRelayFloorReturnsBool : ReferenceSession
{
	bool onRelayFloor(uint8_t floorTicks);
};

struct SessionWhoseConnectionTierReturnsBool : ReferenceSession
{
	bool onConnectionTier(uint8_t oldTier, uint8_t newTier);
};

struct SessionWhoseClockSampleReturnsBool : ReferenceSession
{
	bool onClockSample(const SimClockRttSample& sample);
};

struct SessionWhoseUnregisteringReturnsBool : ReferenceSession
{
	bool onLifecycleUnregistering(uint8_t simId);
};

struct SessionWhoseStampReturnsBool : ReferenceSession
{
	bool stampClientReceived(SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds);
};

struct SessionTakingAMutableSample : ReferenceSession
{
	void onClockSample(SimClockRttSample& sample);
};

struct ReferenceEndpoint
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler);
	void setConnectionHandler(ognet::ConnectionHandler handler);
	void disconnect(ognet::ConnectionId id);
	const ognet::EndpointConfig& config() const;
	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds);
};

struct EndpointWithoutDisconnect
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler);
	void setConnectionHandler(ognet::ConnectionHandler handler);
	const ognet::EndpointConfig& config() const;
	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds);
};

struct EndpointWithAMutableConfig
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler);
	void setConnectionHandler(ognet::ConnectionHandler handler);
	void disconnect(ognet::ConnectionId id);
	const ognet::EndpointConfig& config();
	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds);
};

struct EndpointWithATwoArgumentNoteDelivered
{
	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void broadcastExcept(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes);
	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler);
	void setConnectionHandler(ognet::ConnectionHandler handler);
	void disconnect(ognet::ConnectionId id);
	const ognet::EndpointConfig& config() const;
	void noteDelivered(ognet::Lane lane, double recvSeconds);
};

struct EndpointWithAConfigCopy : ReferenceEndpoint
{
	ognet::EndpointConfig config() const;
};

struct EndpointWhoseNoteDeliveredReturnsBool : ReferenceEndpoint
{
	bool noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds);
};

static_assert(SimNetLinkLock<ReferenceLock>, "SimNetLinkLock self-check: the reference shape (lock, unlock) must satisfy the concept");
static_assert(!SimNetLinkLock<LockWithoutLock>, "SimNetLinkLock self-check: lock() is required; the bindings take it around every call");
static_assert(!SimNetLinkLock<LockWithoutUnlock>, "SimNetLinkLock self-check: unlock() is required");
static_assert(!SimNetLinkLock<LockWhoseLockReturnsBool>,
	"SimNetLinkLock self-check: lock() returns void; a lock that can fail (try_lock's shape) is not BasicLockable");
static_assert(!SimNetLinkLock<LockWhoseUnlockReturnsBool>,
	"SimNetLinkLock self-check: unlock() returns void; the link takes the lock through std::scoped_lock, which discards "
	"any result");

static_assert(SimNetLinkServices<ReferenceServices>, "SimNetLinkServices self-check: the reference shape must satisfy the concept");
static_assert(!SimNetLinkServices<ServicesWithoutNowSeconds>,
	"SimNetLinkServices self-check: nowSeconds() is required (the client host's clock and the delivery note's time)");
static_assert(!SimNetLinkServices<ServicesWithoutLogRouted>,
	"SimNetLinkServices self-check: logRouted(line) is required (the link's own lines, which carry their level as a token)");
static_assert(!SimNetLinkServices<ServicesWithoutLogHost>,
	"SimNetLinkServices self-check: logHost(line) is required (og-sim-net's host lines, printed with their token)");
static_assert(!SimNetLinkServices<ServicesWithoutServerRtt>,
	"SimNetLinkServices self-check: serverRttSeconds() is required (the round trip the clock tick is delivered with)");
static_assert(!SimNetLinkServices<ServicesWithoutWireMismatch>,
	"SimNetLinkServices self-check: onWireMismatch(received, expected) is required (the engine's report of a build mismatch)");
static_assert(!SimNetLinkServices<ServicesWithAFloatServerRtt>,
	"SimNetLinkServices self-check: serverRttSeconds() returns double, the type og-sim-net's client host hands the clock");
static_assert(!SimNetLinkServices<ServicesWithAFloatNowSeconds>,
	"SimNetLinkServices self-check: nowSeconds() returns double; a float of seconds since start loses milliseconds, and "
	"the delivery note's residence is a difference of two such times");
static_assert(!SimNetLinkServices<ServicesWhoseLogRoutedReturnsBool>,
	"SimNetLinkServices self-check: logRouted(line) returns void; the link discards any result");
static_assert(!SimNetLinkServices<ServicesWhoseLogHostReturnsBool>,
	"SimNetLinkServices self-check: logHost(line) returns void; the link discards any result");
static_assert(!SimNetLinkServices<ServicesWhoseWireMismatchReturnsBool>,
	"SimNetLinkServices self-check: onWireMismatch(received, expected) returns void; the link discards any result");

static_assert(SimNetLinkSession<ReferenceSession>, "SimNetLinkSession self-check: the reference shape must satisfy the concept");
static_assert(!SimNetLinkSession<SessionWithoutTimingInfo>, "SimNetLinkSession self-check: onTimingInfo(tick, rtt) is required");
static_assert(!SimNetLinkSession<SessionWithoutRelayFloor>, "SimNetLinkSession self-check: onRelayFloor(floor) is required");
static_assert(!SimNetLinkSession<SessionWithoutConnectionTier>,
	"SimNetLinkSession self-check: onConnectionTier(oldTier, newTier) is required");
static_assert(!SimNetLinkSession<SessionWithoutClockSample>, "SimNetLinkSession self-check: onClockSample(sample) is required");
static_assert(!SimNetLinkSession<SessionWithoutUnregistering>,
	"SimNetLinkSession self-check: onLifecycleUnregistering(simId) is required");
static_assert(!SimNetLinkSession<SessionWithoutStamp>,
	"SimNetLinkSession self-check: stampClientReceived(stream, simId, tick, recvSeconds) is required");
static_assert(!SimNetLinkSession<SessionWithAReplayedFloor>,
	"SimNetLinkSession self-check: onRelayFloor takes the floor alone; a session that could tell a replay apart could skip "
	"the stall an attach-time floor pays (SimNetLink-guards.md G-04)");
static_assert(!SimNetLinkSession<SessionWhoseTimingInfoReturnsBool>,
	"SimNetLinkSession self-check: onTimingInfo(tick, rtt) returns void; the link discards any result");
static_assert(!SimNetLinkSession<SessionWhoseRelayFloorReturnsBool>,
	"SimNetLinkSession self-check: onRelayFloor(floor) returns void; the link discards any result");
static_assert(!SimNetLinkSession<SessionWhoseConnectionTierReturnsBool>,
	"SimNetLinkSession self-check: onConnectionTier(oldTier, newTier) returns void; the link discards any result");
static_assert(!SimNetLinkSession<SessionWhoseClockSampleReturnsBool>,
	"SimNetLinkSession self-check: onClockSample(sample) returns void; the link discards any result");
static_assert(!SimNetLinkSession<SessionWhoseUnregisteringReturnsBool>,
	"SimNetLinkSession self-check: onLifecycleUnregistering(simId) returns void; the link discards any result");
static_assert(!SimNetLinkSession<SessionWhoseStampReturnsBool>,
	"SimNetLinkSession self-check: stampClientReceived(stream, simId, tick, recvSeconds) returns void; the link discards "
	"any result");
static_assert(!SimNetLinkSession<SessionTakingAMutableSample>,
	"SimNetLinkSession self-check: onClockSample takes the sample by const reference, as the link hands it over");

static_assert(SimNetLinkEndpoint<ReferenceEndpoint>, "SimNetLinkEndpoint self-check: the reference shape must satisfy the concept");
static_assert(!SimNetLinkEndpoint<EndpointWithoutDisconnect>,
	"SimNetLinkEndpoint self-check: it refines SimNetEndpoint, the transport's endpoint (the disconnect request needs disconnect)");
static_assert(!SimNetLinkEndpoint<EndpointWithAMutableConfig>,
	"SimNetLinkEndpoint self-check: config() is callable on a const endpoint and returns the endpoint's og-net config");
static_assert(!SimNetLinkEndpoint<EndpointWithATwoArgumentNoteDelivered>,
	"SimNetLinkEndpoint self-check: noteDelivered(lane, recvSeconds, nowSeconds) is required, the endpoint's [OGNet.Window] row");
static_assert(!SimNetLinkEndpoint<EndpointWithAConfigCopy>,
	"SimNetLinkEndpoint self-check: config() returns a const reference to the endpoint's own og-net config, not a copy");
static_assert(!SimNetLinkEndpoint<EndpointWhoseNoteDeliveredReturnsBool>,
	"SimNetLinkEndpoint self-check: noteDelivered(lane, recvSeconds, nowSeconds) returns void; the link discards any "
	"result");
} // namespace simNetLinkSelfCheck
} // namespace ogsimnet
