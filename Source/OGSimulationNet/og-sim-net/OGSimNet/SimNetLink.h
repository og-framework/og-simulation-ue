#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimNetLanes-rationale.md · ../../../OGSimulationUnreal/docs/UESimNetLanes-guards.md
// ../../../OGSimulationUnreal/docs/UESimNetHost-rationale.md · ../../../OGSimulationUnreal/docs/UESimNetHost-guards.md

#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetRoster.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>

inline constexpr char kSimNetRosterLinePrefix[] = "[Roster.";
inline constexpr char kSimNetWarningToken[] = "[Warning]";
inline constexpr size_t kSimNetWarnedRosterLineBytes = ogsimnet::kRosterLogLineBytes + sizeof(kSimNetWarningToken);

class FLockedSimNetBindings
{
public:
	void onRosterAdded(const ogsimnet::RosterEntry& entry)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.onRosterAdded(entry);
	}

	bool onRosterRemoved(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.onRosterRemoved(simId);
	}

	void seedFrom(const ogsimnet::RosterMirror& mirror)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.seedFrom(mirror);
	}

	ogsimnet::BindVerdict verdict(uint8_t simId, bool engineSaysLocal) const
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.verdict(simId, engineSaysLocal);
	}

	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t simId, bool engineSaysLocal)
	{
		const std::scoped_lock lock(m_mutex);
		const ogsimnet::BindVerdict verdict = m_bindings.verdict(simId, engineSaysLocal);
		if (verdict.step == ogsimnet::BindStep::Ready)
			m_bindings.markRegistered(simId);
		return verdict;
	}

	void markRegistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.markRegistered(simId);
	}

	bool markUnregistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.markUnregistered(simId);
	}

	bool isRegistered(uint8_t simId) const
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.isRegistered(simId);
	}

private:
	mutable std::mutex m_mutex;
	ogsimnet::SimNetBindings m_bindings;
};

namespace ueSimNetHost
{
inline void noteDelivered(ognetunreal::SimLaneEndpoint& endpoint, SimLane lane, double recvSeconds)
{
	endpoint.noteDelivered(static_cast<ognet::Lane>(lane), recvSeconds, FPlatformTime::Seconds());
}

inline void routeLine(const char* line)
{
	if (std::strncmp(line, kSimNetRosterLinePrefix, sizeof(kSimNetRosterLinePrefix) - 1u) != 0)
	{
		RouteOGMessage(line);
		return;
	}
	char warned[kSimNetWarnedRosterLineBytes] = {};
	std::snprintf(warned, sizeof(warned), "%s%s", kSimNetWarningToken, line);
	RouteOGMessage(warned);
}
} // namespace ueSimNetHost

namespace ueSimNetLanes
{
inline void routeClientLine(const char* line)
{
	if (std::strncmp(line, ogsimnet::kClockSourceHeldTag, sizeof(ogsimnet::kClockSourceHeldTag) - 1u) == 0)
	{
		UE_LOG(LogOGNet, Warning, TEXT("%s"), *FString(line));
		return;
	}
	ueSimNetHost::routeLine(line);
}
} // namespace ueSimNetLanes

template <typename TraitsT>
class UESimNetLanesClientHost : public UESimNetClientHost<TraitsT>
{
	using Base = UESimNetClientHost<TraitsT>;
	using Base::m_session;

public:
	using FRegisteredRemoved = TFunction<void(uint8_t simId)>;
	UESimNetLanesClientHost(UOGNetDriver* ognetDriver, ognetunreal::SimLaneEndpoint& laneEndpoint, FLockedSimNetBindings& simBindings)
		: Base(ognetDriver), m_endpoint(laneEndpoint), m_bindings(simBindings)
	{
	}

	void setRoster(const ogsimnet::RosterMirror& roster) { m_roster = &roster; }
	void setOnRegisteredRemoved(FRegisteredRemoved onRegisteredRemoved) { m_onRegisteredRemoved = MoveTemp(onRegisteredRemoved); }

	void onLifecycleUnregistering(uint8_t simId)
	{
		if (this->session() != nullptr)
			this->session()->onSimNetLifecycleUnregistering(simId);
	}

	void onClockSample(const SimClockRttSample& sample)
	{
		if (this->session() != nullptr)
			this->session()->onSimClockSample(sample);
	}

	void log(const char* line) { ueSimNetLanes::routeClientLine(line); }

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds)
	{
		if (m_session != nullptr)
			m_session->onTimingInfoReceived(authorityTick, roundTripSeconds);
	}

	void onRelayFloor(uint8_t floorTicks, [[maybe_unused]] bool replayed)
	{
		if (m_session == nullptr)
			return;
		// ⛔G-05  docs/UESimNetHost-guards.md
		m_session->onRelayDelayFloorReceived(floorTicks);
	}

	void onConnectionTier(uint8_t oldTier, uint8_t newTier, [[maybe_unused]] bool replayed)
	{
		if (m_session == nullptr)
			return;
		m_session->onConnectionTierReceived(oldTier, newTier);
	}

	void onRosterAdded(uint8_t simId)
	{
		const ogsimnet::RosterEntry* const entry = m_roster != nullptr ? m_roster->entryFor(simId) : nullptr;
		checkf(entry != nullptr,
			TEXT("UESimNetClientHost::onRosterAdded: sim id %u is not in the roster mirror. The mirror reports an Add after it ")
			TEXT("applied it, and the lanes set the mirror before the first drain"),
			static_cast<unsigned>(simId));
		if (entry != nullptr)
			m_bindings.onRosterAdded(*entry);
	}

	void onRosterRemoved(uint8_t simId)
	{
		const bool wasRegistered = m_bindings.onRosterRemoved(simId);
		checkf(!wasRegistered || static_cast<bool>(m_onRegisteredRemoved),
			TEXT("UESimNetClientHost::onRosterRemoved: sim id %u is registered and no unregister handler is set. The client ")
			TEXT("session sets one (setOnRegisteredRemoved) before it registers a character"),
			static_cast<unsigned>(simId));
		if (wasRegistered && m_onRegisteredRemoved)
		{
			// ⛔G-03  docs/UESimNetHost-guards.md
			m_onRegisteredRemoved(simId);
		}
	}

	void stampClientReceived(ogsimnet::SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds)
	{
		if (m_session == nullptr)
			return;
		UELatencyBudgetDiagnostics<TraitsT>& diagnostics = m_session->editLatencyBudgetDiagnostics();
		const latencyBudget::Stream probeStream =
			stream == ogsimnet::SimNetStream::State ? latencyBudget::Stream::State : latencyBudget::Stream::Input;
		diagnostics.stampClientReceived_Internal(probeStream, simId, tick, recvSeconds);
	}

	void noteDelivered(SimLane lane, double recvSeconds) { ueSimNetHost::noteDelivered(m_endpoint, lane, recvSeconds); }

private:
	ognetunreal::SimLaneEndpoint& m_endpoint;
	FLockedSimNetBindings& m_bindings;
	const ogsimnet::RosterMirror* m_roster = nullptr;
	FRegisteredRemoved m_onRegisteredRemoved;
};

namespace ogsimnet
{
template <typename TraitsT>
class SimNetLink
{
public:
	using Transport        = UESimNetTransport;
	using ClientHost       = UESimNetLanesClientHost<TraitsT>;
	using Client           = ogsimnet::SimNetClient<typename TraitsT::Composition, ClientHost, Transport>;
	using Session          = UESimulationSession<TraitsT>;

	SimNetLink(UOGNetDriver* ognetDriver, bool bIsServer) : m_driver(ognetDriver), m_isServer(bIsServer)
	{
		static_assert(compositionContractsHold());
		static_assert(ogsimnet::SimNetClockRttSampleHook<ClientHost>,
			"UESimNetLanesClientHost has onClockSample(const SimClockRttSample&), so the client session hands it every SimClock "
			"sample and feeds the one it held while unbound ahead of the first authority tick after the bind. Without the hook "
			"the session counts the samples unheard, the clock stays on the engine ping and the join resyncs a second time "
			"(cause=firstRtt). Was the static_assert on the host's own onTimingInfo (og-input-latency P3-11), whose latch "
			"og-session-runtime task 20 moved into SimNetClient");
		static_assert(std::is_same_v<decltype(&ClientHost::log), void (ClientHost::*)(const char*)>,
			"UESimNetLanesClientHost declares its own log, so the client session's [ClockSource.Held] line keeps the "
			"LogOGNet category and the Warning level it had as a UE_LOG (og-session-runtime task 20); the base host's log "
			"routes by the og-sim prefix table, which has no row for it");
	}

	SimNetLink(const SimNetLink&) = delete;
	SimNetLink& operator=(const SimNetLink&) = delete;

	void beforeStart(ognetunreal::SimLaneEndpoint& simEndpoint)
	{
		checkf(m_endpoint == nullptr, TEXT("UESimNetLanes::beforeStart: a lanes object attaches to one endpoint, once"));
		checkf((simEndpoint.config().role == ognet::Role::Server) == m_isServer,
			TEXT("UESimNetLanes::beforeStart: the lanes were made for the %s role and the endpoint is the other role's"),
			m_isServer ? TEXT("server") : TEXT("client"));
		m_endpoint = &simEndpoint;
		m_transport.emplace(ogsimnet::simNetConfigFor(simEndpoint.config()));
		if (m_isServer)
			m_clockServer.emplace(*m_transport, m_clock);
		else
			m_clockClient.emplace(*m_transport, m_clock);
		m_transport->installHandlers(simEndpoint);
		simEndpoint.setConnectionHandler(m_transport->connectionHandler());
		if (m_isServer)
			return;
		m_clientHost.emplace(m_driver, simEndpoint, m_bindings);
		// ⛔G-01  docs/UESimNetLanes-guards.md
		m_client.emplace(*m_clientHost, *m_transport);
		m_clientHost->setRoster(m_client->roster());
	}

	void afterStart()
	{
		checkf(m_transport.has_value(), TEXT("UESimNetLanes::afterStart before beforeStart: the driver attaches the lanes first"));
		m_transport->markEndpointStarted();
	}

	template <class PublishFn>
	void afterDispatch(PublishFn&& publishRtt)
	{
		checkf(m_client.has_value(), TEXT("UESimNetLanes::afterDispatch on a client before beforeStart"));
		m_clockClient->servicePing();
		publishRtt();
		// ⛔G-02  docs/UESimNetLanes-guards.md
		if (m_clientSessionBound.load(std::memory_order_relaxed))
			return;
		m_client->drain();
		actOnDisconnectRequest();
	}

	void onTickFlush()
	{
		if (m_isServer)
			return;
		checkf(m_client.has_value(), TEXT("UESimNetLanes::onTickFlush on a client before beforeStart"));
		if (m_clientSessionBound.load(std::memory_order_relaxed))
			return;
		m_client->flushFrame();
	}

	void drainClient_Step()
	{
		checkf(!IsInGameThread(),
			TEXT("UESimNetLanes::drainClient_Step: a bound client drains at the start of each step, on the step's thread; "
				 "on the game thread every consumer the drain reaches would race the step. Was the game thread's frame-point "
				 "drain (afterDispatch)"));
		checkf(m_client.has_value() && m_clientSessionBound.load(std::memory_order_relaxed),
			TEXT("UESimNetLanes::drainClient_Step: only a bound client's session drains from its steps"));
		m_client->drain();
		actOnDisconnectRequest();
		m_client->flushFrame();
	}

	void drainClientWhileStopped_GameThread()
	{
		checkf(IsInGameThread() && m_client.has_value(),
			TEXT("UESimNetLanes::drainClientWhileStopped_GameThread: the game thread drains a bound client only at its "
				 "session's teardown, after the steps have stopped, to apply the unregistrations it queued"));
		m_client->drain();
	}

	void bindClientSession(Session& worldSession)
	{
		checkf(!m_isServer && m_client.has_value(),
			TEXT("UESimNetLanes::bindClientSession: only a started client's lanes hold a client session"));
		m_clientHost->bindSession(worldSession);
		m_client->bindListener();
		m_bindings.seedFrom(m_client->roster());
		m_clientSessionBound.store(true, std::memory_order_relaxed);
	}

	void unbindClientSession()
	{
		if (!m_client.has_value())
			return;
		m_client->unbindListener();
		m_clientHost->unbindSession();
		m_clientSessionBound.store(false, std::memory_order_relaxed);
	}

	void setOnRegisteredRemoved(typename ClientHost::FRegisteredRemoved onRegisteredRemoved)
	{
		checkf(m_clientHost.has_value(), TEXT("UESimNetLanes::setOnRegisteredRemoved: only a started client's lanes have a client host"));
		m_clientHost->setOnRegisteredRemoved(MoveTemp(onRegisteredRemoved));
	}

	UESimNetClockServer* clockServer() { return m_clockServer.has_value() ? &*m_clockServer : nullptr; }
	const UESimNetClockClient* clockClient() const { return m_clockClient.has_value() ? &*m_clockClient : nullptr; }
	bool clientSessionBound() const { return m_clientSessionBound.load(std::memory_order_relaxed); }

	ognetunreal::SimLaneEndpoint& endpoint()
	{
		checkf(m_endpoint != nullptr, TEXT("UESimNetLanes::endpoint before beforeStart"));
		return *m_endpoint;
	}

	Transport& transport()
	{
		checkf(m_transport.has_value(), TEXT("UESimNetLanes::transport before beforeStart"));
		return *m_transport;
	}

	Client* client() { return m_client.has_value() ? &*m_client : nullptr; }
	ClientHost* clientHost() { return m_clientHost.has_value() ? &*m_clientHost : nullptr; }
	const FLockedSimNetBindings& bindings() const { return m_bindings; }
	FLockedSimNetBindings& editBindings() { return m_bindings; }

private:
	void actOnDisconnectRequest()
	{
		const std::optional<ogsimnet::SimNetDisconnectRequest> request = m_client->takeDisconnectRequest();
		if (!request)
			return;
		UE_LOG(LogOGNet, Error,
			TEXT("[SimNet.Host] role=client disconnect conn=%u reason=%s: the server's roster was refused, so this client leaves the server"),
			static_cast<unsigned>(request->conn), UTF8_TO_TCHAR(ogsimnet::rosterRefusalName(request->reason)));
		if (request->conn != kInvalidSimConnection)
			m_endpoint->disconnect(request->conn);
	}

	UOGNetDriver* m_driver;
	bool m_isServer;
	std::atomic<bool> m_clientSessionBound{false};
	ognetunreal::SimLaneEndpoint* m_endpoint = nullptr;
	FUEPlatformClock m_clock;
	std::optional<Transport> m_transport;
	std::optional<UESimNetClockServer> m_clockServer;
	std::optional<UESimNetClockClient> m_clockClient;
	FLockedSimNetBindings m_bindings;
	std::optional<ClientHost> m_clientHost;
	std::optional<Client> m_client;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(SimNetLink, m_clock) < __builtin_offsetof(SimNetLink, m_clockServer)
		           && __builtin_offsetof(SimNetLink, m_transport) < __builtin_offsetof(SimNetLink, m_clockServer),
			"UESimNetLanes members destruct in REVERSE declaration order. The clock server holds the transport and the clock by "
			"reference and is the transport's SimClock lane handler, so it goes before both. The driver destroys the lanes only "
			"after og-net's endpoint (DestroySimLanesAfterEndpoint), so the handler outlives every call og-net can make. Keep "
			"m_clock and m_transport < m_clockServer. Added by og-input-latency task P5-19 (P2-20 F2)");
		static_assert(__builtin_offsetof(SimNetLink, m_clock) < __builtin_offsetof(SimNetLink, m_clockClient)
		           && __builtin_offsetof(SimNetLink, m_transport) < __builtin_offsetof(SimNetLink, m_clockClient),
			"UESimNetLanes members destruct in REVERSE declaration order. The clock client holds the transport and the clock by "
			"reference and is the client transport's SimClock lane handler, so it goes before both, as the clock server does. Keep "
			"m_clock and m_transport < m_clockClient. Added by og-input-latency task P3-10 (P2-20 F2's client half)");
		return true;
	}
};
} // namespace ogsimnet
