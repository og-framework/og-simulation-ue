// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetLanes-rationale.md · docs/UESimNetLanes-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGNetUnreal/OGNetSimLanes.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetClient.h"
#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

inline constexpr size_t kUESimNetInboxSlots = 2048u;

inline std::optional<int32> readSimTrafficImmediateKnob()
{
	if (GConfig == nullptr)
		return std::nullopt;
	int32 iniImmediate = 0;
	if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("SimTrafficImmediate"), iniImmediate, GGameIni) ||
		GConfig->GetInt(TEXT("OGNetcode"), TEXT("SimTrafficImmediate"), iniImmediate, GEngineIni))
	{
		return iniImmediate;
	}
	return std::nullopt;
}

constexpr bool simTrafficImmediateKnobRefused(std::optional<int32> knob)
{
	return knob.has_value() && *knob == 0;
}

constexpr ognetunreal::IoMode simLanesIoMode(bool isServer, bool dedicatedServer)
{
	return isServer && dedicatedServer ? ognetunreal::IoMode::HostServiced : ognetunreal::IoMode::OwnThread;
}

using UESimNetTransport = ogsimnet::SimNetTransport<ognetunreal::SimLaneEndpoint, kUESimNetInboxSlots>;

template <typename SimT>
using UESimNetChannel = ogsimnet::SessionChannel<SimT, typename UESimNetTransport::ChannelTransport>;

struct FUEPlatformClock
{
	double nowSeconds() { return FPlatformTime::Seconds(); }
};

using UESimNetClockServer = ogsimnet::SimNetClockServer<UESimNetTransport, FUEPlatformClock>;
using UESimNetClockClient = ogsimnet::SimNetClockClient<UESimNetTransport, FUEPlatformClock>;

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

inline constexpr size_t kRunnerRttSlots = 64u;

class FRunnerRttTable
{
public:
	static_assert(ognet::kInvalidConnection == 0u,
		"FRunnerRttTable: a word of 0 is an empty slot, and a connection's word holds its og-net id in the high half; og-net "
		"never assigns id 0 (kInvalidConnection), so no connection's word reads as empty. A connection still without an "
		"og-net id is not published, and id 0 is never read. Was review_P5-19 N2 (an id 0 with a round trip of 0 read as "
		"absent)");
	static_assert(std::atomic<uint64_t>::is_always_lock_free,
		"FRunnerRttTable: one connection's reading is one lock-free 64-bit word (og-net id, float milliseconds), so the runner "
		"never reads an id with another connection's round trip. Was the runner's call into the driver's connection map, "
		"which is game-thread state");

	void publish_GameThread(size_t slot, uint32 ogNetId, double roundTripMs)
	{
		m_slots[slot].store((static_cast<uint64_t>(ogNetId) << 32u) | std::bit_cast<uint32_t>(static_cast<float>(roundTripMs)),
			std::memory_order_release);
	}

	void clearFrom_GameThread(size_t firstSlot)
	{
		for (size_t slot = firstSlot; slot < kRunnerRttSlots; ++slot)
			m_slots[slot].store(0u, std::memory_order_release);
	}

	double read(uint32 ogNetId) const
	{
		if (ogNetId == ognet::kInvalidConnection)
			return kNoRttReadingMs;
		for (const std::atomic<uint64_t>& slot : m_slots)
		{
			const uint64_t word = slot.load(std::memory_order_acquire);
			if (word != 0u && static_cast<uint32>(word >> 32u) == ogNetId)
				return static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(word & 0xFFFFFFFFu)));
		}
		return kNoRttReadingMs;
	}

private:
	std::array<std::atomic<uint64_t>, kRunnerRttSlots> m_slots{};
};

template <typename TraitsT>
class UESimNetLanesServerHost : public UESimNetServerHost<TraitsT>
{
	using Base = UESimNetServerHost<TraitsT>;

public:
	using Base::Base;

	double rttMs(SimConnectionId conn) { return m_runnerRtt.read(conn); }

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		if (this->session() == nullptr)
			return;
		this->session()->editLatencyBudgetDiagnostics().stampServerReceived_Internal(simId, captureTick, recvSeconds);
	}

	void stampHanded(uint8_t simId, uint32_t captureTick)
	{
		if (this->session() != nullptr)
			this->session()->editLatencyBudgetDiagnostics().stampHandedAndLeft_Internal(simId, captureTick);
	}

	void onLifecycleUnregistering(uint8_t simId)
	{
		if (this->session() != nullptr)
			this->session()->onSimNetLifecycleUnregistering(simId);
	}

	void publishRunnerRtt_GameThread()
	{
		checkf(IsInGameThread(),
			TEXT("UESimNetLanesServerHost::publishRunnerRtt_GameThread: the game thread reads the driver's connections and "
				 "publishes each one's round trip for the authority runner, which reads only the published copy"));
		size_t slot = 0u;
		if (UOGNetDriver* const driver = this->driver())
		{
			for (UNetConnection* const connection : driver->ClientConnections)
			{
				UOGNetConnection* const ognetConnection = Cast<UOGNetConnection>(connection);
				if (ognetConnection == nullptr || ognetConnection->OGNetId == ognet::kInvalidConnection || slot == kRunnerRttSlots)
					continue;
				m_runnerRtt.publish_GameThread(slot++, ognetConnection->OGNetId, publishedRttMs_GameThread(*ognetConnection));
			}
		}
		m_runnerRtt.clearFrom_GameThread(slot);
	}

private:
	double publishedRttMs_GameThread(UOGNetConnection& connection)
	{
		// ⛔G-04  docs/UESimNetLanes-guards.md
		if (connection.ClientLoginState == EClientLoginState::ReceivedJoin)
			return Base::rttMs(connection.OGNetId);
		double roundTripMs = kNoRttReadingMs;
		if (!connection.IsClosingOrClosed())
			readRoundTripMs(connection, roundTripMs);
		return roundTripMs;
	}

	FRunnerRttTable m_runnerRtt;
};

template <typename TraitsT>
class UESimNetLanesClientHost : public UESimNetClientHost<TraitsT>
{
	using Base = UESimNetClientHost<TraitsT>;

public:
	using Base::Base;

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
};

template <typename TraitsT>
class UESimNetLanes final : public IOGNetSimLanes
{
public:
	using Transport        = UESimNetTransport;
	using ChannelTransport = typename Transport::ChannelTransport;
	using ServerHost       = UESimNetLanesServerHost<TraitsT>;
	using ClientHost       = UESimNetLanesClientHost<TraitsT>;
	using Client           = ogsimnet::SimNetClient<typename TraitsT::Composition, ClientHost, Transport>;
	using Session          = UESimulationSession<TraitsT>;
	using FFrameFn         = TFunction<void()>;

	UESimNetLanes(UOGNetDriver* ognetDriver, bool bIsServer) : m_driver(ognetDriver), m_isServer(bIsServer)
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

	UESimNetLanes(const UESimNetLanes&) = delete;
	UESimNetLanes& operator=(const UESimNetLanes&) = delete;

	virtual ognetunreal::IoMode ioMode(bool bIsServer) override
	{
		checkf(bIsServer == m_isServer,
			TEXT("UESimNetLanes::ioMode: the driver asks for its own role's I/O mode, and the game's factory made these lanes "
				 "for that role"));
		m_hostServiced = simLanesIoMode(m_isServer, m_driver != nullptr && m_driver->GetNetMode() == NM_DedicatedServer)
			== ognetunreal::IoMode::HostServiced;
		return m_hostServiced ? ognetunreal::IoMode::HostServiced : ognetunreal::IoMode::OwnThread;
	}

	virtual void beforeStart(ognetunreal::SimLaneEndpoint& simEndpoint) override
	{
		checkf(m_endpoint == nullptr, TEXT("UESimNetLanes::beforeStart: a lanes object attaches to one endpoint, once"));
		checkf((simEndpoint.config().role == ognet::Role::Server) == m_isServer,
			TEXT("UESimNetLanes::beforeStart: the lanes were made for the %s role and the endpoint is the other role's"),
			m_isServer ? TEXT("server") : TEXT("client"));
		m_endpoint = &simEndpoint;
		if (!m_isServer)
			m_simTrafficImmediateRefused = simTrafficImmediateKnobRefused(readSimTrafficImmediateKnob());
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

	virtual void afterStart() override
	{
		checkf(m_transport.has_value(), TEXT("UESimNetLanes::afterStart before beforeStart: the driver attaches the lanes first"));
		m_transport->markEndpointStarted();
	}

	virtual void afterDispatch() override
	{
		if (m_isServer)
		{
			if (m_publishRunnerRtt)
				m_publishRunnerRtt();
			return;
		}
		checkf(m_client.has_value(), TEXT("UESimNetLanes::afterDispatch on a client before beforeStart"));
		m_clockClient->servicePing();
		publishAckSrtt_GameThread();
		m_clientHost->publishServerRtt_GameThread();
		// ⛔G-02  docs/UESimNetLanes-guards.md
		if (m_clientSessionBound.load(std::memory_order_relaxed))
			return;
		m_client->drain();
		actOnDisconnectRequest();
	}

	virtual void onTickFlush() override
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

	virtual void beforeStop() override
	{
		FFrameFn joinRunner = MoveTemp(m_joinRunner);
		m_joinRunner = nullptr;
		// ⛔G-03  docs/UESimNetLanes-guards.md
		if (joinRunner)
			joinRunner();
		FFrameFn onStop = MoveTemp(m_onStop);
		m_onStop = nullptr;
		if (onStop)
			onStop();
		if (m_isServer)
			unbindServerSession();
		else
			unbindClientSession();
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

	void setStopHandler(FFrameFn onStop) { m_onStop = MoveTemp(onStop); }

	void unbindServerSession()
	{
		m_publishRunnerRtt = nullptr;
		m_joinRunner = nullptr;
	}

	void bindAuthorityRunner(FFrameFn publishRunnerRtt, FFrameFn joinRunner)
	{
		checkf(m_isServer && m_hostServiced && static_cast<bool>(publishRunnerRtt) && static_cast<bool>(joinRunner),
			TEXT("UESimNetLanes::bindAuthorityRunner: only a dedicated server's host-serviced lanes hold the runner's RTT publish "
				 "and its join"));
		m_publishRunnerRtt = MoveTemp(publishRunnerRtt);
		m_joinRunner = MoveTemp(joinRunner);
	}

	bool hostServiced() const { return m_hostServiced; }
	UESimNetClockServer* clockServer() { return m_clockServer.has_value() ? &*m_clockServer : nullptr; }
	const UESimNetClockClient* clockClient() const { return m_clockClient.has_value() ? &*m_clockClient : nullptr; }
	double ackSrttSeconds() const { return m_ackSrttSeconds.load(std::memory_order_relaxed); }
	bool isServer() const { return m_isServer; }
	bool clientSessionBound() const { return m_clientSessionBound.load(std::memory_order_relaxed); }
	bool simTrafficImmediateRefused() const { return m_simTrafficImmediateRefused; }
	UOGNetDriver* driver() const { return m_driver; }

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
	void publishAckSrtt_GameThread()
	{
		const SimConnectionId server = m_transport->serverConnection();
		const double srttSeconds =
			m_driver != nullptr && server != kInvalidSimConnection ? m_driver->GetSmoothedRttSeconds(server) : kNoRttReadingSeconds;
		m_ackSrttSeconds.store(srttSeconds, std::memory_order_relaxed);
	}

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
	bool m_simTrafficImmediateRefused = false;
	bool m_hostServiced = false;
	std::atomic<bool> m_clientSessionBound{false};
	ognetunreal::SimLaneEndpoint* m_endpoint = nullptr;
	FUEPlatformClock m_clock;
	std::optional<Transport> m_transport;
	std::optional<UESimNetClockServer> m_clockServer;
	std::optional<UESimNetClockClient> m_clockClient;
	std::atomic<double> m_ackSrttSeconds{kNoRttReadingSeconds};
	FLockedSimNetBindings m_bindings;
	std::optional<ClientHost> m_clientHost;
	std::optional<Client> m_client;
	FFrameFn m_publishRunnerRtt;
	FFrameFn m_joinRunner;
	FFrameFn m_onStop;

	static constexpr bool compositionContractsHold()
	{
		static_assert(__builtin_offsetof(UESimNetLanes, m_clock) < __builtin_offsetof(UESimNetLanes, m_clockServer)
		           && __builtin_offsetof(UESimNetLanes, m_transport) < __builtin_offsetof(UESimNetLanes, m_clockServer),
			"UESimNetLanes members destruct in REVERSE declaration order. The clock server holds the transport and the clock by "
			"reference and is the transport's SimClock lane handler, so it goes before both. The driver destroys the lanes only "
			"after og-net's endpoint (DestroySimLanesAfterEndpoint), so the handler outlives every call og-net can make. Keep "
			"m_clock and m_transport < m_clockServer. Added by og-input-latency task P5-19 (P2-20 F2)");
		static_assert(__builtin_offsetof(UESimNetLanes, m_clock) < __builtin_offsetof(UESimNetLanes, m_clockClient)
		           && __builtin_offsetof(UESimNetLanes, m_transport) < __builtin_offsetof(UESimNetLanes, m_clockClient),
			"UESimNetLanes members destruct in REVERSE declaration order. The clock client holds the transport and the clock by "
			"reference and is the client transport's SimClock lane handler, so it goes before both, as the clock server does. Keep "
			"m_clock and m_transport < m_clockClient. Added by og-input-latency task P3-10 (P2-20 F2's client half)");
		return true;
	}
};
