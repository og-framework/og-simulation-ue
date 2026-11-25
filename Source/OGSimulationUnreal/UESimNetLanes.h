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

#include "OGSimNet/SimNetLink.h"

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

	UESimNetLanes(UOGNetDriver* ognetDriver, bool bIsServer) : m_driver(ognetDriver), m_isServer(bIsServer), m_link(ognetDriver, bIsServer)
	{
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
		if (!m_isServer)
			m_simTrafficImmediateRefused = simTrafficImmediateKnobRefused(readSimTrafficImmediateKnob());
		m_link.beforeStart(simEndpoint);
	}

	virtual void afterStart() override
	{
		m_link.afterStart();
	}

	virtual void afterDispatch() override
	{
		if (m_isServer)
		{
			if (m_publishRunnerRtt)
				m_publishRunnerRtt();
			return;
		}
		m_link.afterDispatch([this]() {
			publishAckSrtt_GameThread();
			m_link.clientHost()->publishServerRtt_GameThread();
		});
	}

	virtual void onTickFlush() override
	{
		m_link.onTickFlush();
	}

	void drainClient_Step() { m_link.drainClient_Step(); }

	void drainClientWhileStopped_GameThread() { m_link.drainClientWhileStopped_GameThread(); }

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

	void bindClientSession(Session& worldSession) { m_link.bindClientSession(worldSession); }

	void unbindClientSession() { m_link.unbindClientSession(); }

	void setOnRegisteredRemoved(typename ClientHost::FRegisteredRemoved onRegisteredRemoved)
	{
		m_link.setOnRegisteredRemoved(MoveTemp(onRegisteredRemoved));
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
	UESimNetClockServer* clockServer() { return m_link.clockServer(); }
	const UESimNetClockClient* clockClient() const { return m_link.clockClient(); }
	double ackSrttSeconds() const { return m_ackSrttSeconds.load(std::memory_order_relaxed); }
	bool isServer() const { return m_isServer; }
	bool clientSessionBound() const { return m_link.clientSessionBound(); }
	bool simTrafficImmediateRefused() const { return m_simTrafficImmediateRefused; }
	UOGNetDriver* driver() const { return m_driver; }

	ognetunreal::SimLaneEndpoint& endpoint() { return m_link.endpoint(); }
	Transport& transport() { return m_link.transport(); }
	Client* client() { return m_link.client(); }
	ClientHost* clientHost() { return m_link.clientHost(); }
	const FLockedSimNetBindings& bindings() const { return m_link.bindings(); }
	FLockedSimNetBindings& editBindings() { return m_link.editBindings(); }

private:
	void publishAckSrtt_GameThread()
	{
		const SimConnectionId server = m_link.transport().serverConnection();
		const double srttSeconds =
			m_driver != nullptr && server != kInvalidSimConnection ? m_driver->GetSmoothedRttSeconds(server) : kNoRttReadingSeconds;
		m_ackSrttSeconds.store(srttSeconds, std::memory_order_relaxed);
	}

	UOGNetDriver* m_driver;
	bool m_isServer;
	bool m_simTrafficImmediateRefused = false;
	bool m_hostServiced = false;
	std::atomic<double> m_ackSrttSeconds{kNoRttReadingSeconds};
	ogsimnet::SimNetLink<TraitsT> m_link;
	FFrameFn m_publishRunnerRtt;
	FFrameFn m_joinRunner;
	FFrameFn m_onStop;
};
