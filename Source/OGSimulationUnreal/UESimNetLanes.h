// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetLanes-rationale.md · docs/UESimNetLanes-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGNetUnreal/OGNetSimLanes.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimulationUnreal/UEFMutexLockableUImpl.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "Templates/Function.h"

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>

inline constexpr size_t kUESimNetInboxSlots = ogsimnet::kSimNetLinkInboxSlots;

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

template <typename TraitsT>
class UESimNetLanes final : public IOGNetSimLanes
{
public:
	using Link             = ogsimnet::SimNetLink<typename TraitsT::Composition, ognetunreal::SimLaneEndpoint, FUEPlatformClock,
		UESimNetLinkServices, UEFMutexLockableUImpl>;
	using Transport        = typename Link::Transport;
	using ServerHost       = UESimNetLanesServerHost<TraitsT>;
	using Client           = typename Link::Client;
	using Session          = UESessionRuntime<TraitsT>;
	using FFrameFn         = TFunction<void()>;

	static_assert(std::is_same_v<Transport, UESimNetTransport> && std::is_same_v<typename Link::ClockServer, UESimNetClockServer>
			&& std::is_same_v<typename Link::ClockClient, UESimNetClockClient>,
		"UESimNetLanes' link runs the namespace-scope transport and clock types the session and the game's owner traits name");

	UESimNetLanes(UOGNetDriver* ognetDriver, bool bIsServer) : m_driver(ognetDriver), m_services(ognetDriver), m_link(m_services, bIsServer)
	{
		static_assert(__builtin_offsetof(UESimNetLanes, m_services) < __builtin_offsetof(UESimNetLanes, m_link)
		           && __builtin_offsetof(UESimNetLanes, m_sessionHooks) < __builtin_offsetof(UESimNetLanes, m_link),
			"UESimNetLanes members destruct in REVERSE declaration order. The link holds the services and the attached session's "
			"hooks by reference, so it goes before both. Keep m_services and m_sessionHooks < m_link (og-session-runtime task 28)");
	}

	UESimNetLanes(const UESimNetLanes&) = delete;
	UESimNetLanes& operator=(const UESimNetLanes&) = delete;

	virtual ognetunreal::IoMode ioMode(bool bIsServer) override
	{
		checkf(bIsServer == m_link.isServer(),
			TEXT("UESimNetLanes::ioMode: the driver asks for its own role's I/O mode, and the game's factory made these lanes "
				 "for that role"));
		m_hostServiced = simLanesIoMode(m_link.isServer(), m_driver != nullptr && m_driver->GetNetMode() == NM_DedicatedServer)
			== ognetunreal::IoMode::HostServiced;
		return m_hostServiced ? ognetunreal::IoMode::HostServiced : ognetunreal::IoMode::OwnThread;
	}

	virtual void beforeStart(ognetunreal::SimLaneEndpoint& simEndpoint) override
	{
		if (!m_link.isServer())
			m_simTrafficImmediateRefused = simTrafficImmediateKnobRefused(readSimTrafficImmediateKnob());
		m_link.start(simEndpoint);
	}

	virtual void afterStart() override { m_link.markStarted(); }

	virtual void afterDispatch() override
	{
		if (m_link.isServer())
			return;
		m_link.serviceFrame([this]() {
			publishAckSrtt_GameThread();
			m_services.publishServerRtt_GameThread();
		});
	}

	virtual void onTickFlush() override { m_link.flushFrame(); }

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
		if (m_link.isServer())
			unbindServerSession();
		m_link.stop();
	}

	void drainClient_Step() { m_link.drainForStep(); }
	void drainClientWhileStopped_GameThread() { m_link.drainWhileStopped(); }

	void bindClientSession(Session& worldSession)
	{
		m_sessionHooks.session = &worldSession;
		m_link.attachSession(m_sessionHooks);
	}

	void unbindClientSession() { m_link.detachSession(); }

	void setOnRegisteredRemoved(typename Link::RegisteredRemovedFn fn) { m_link.setOnRegisteredRemoved(std::move(fn)); }

	void setStopHandler(FFrameFn onStop) { m_onStop = MoveTemp(onStop); }

	void unbindServerSession() { m_joinRunner = nullptr; }

	void bindAuthorityRunner(FFrameFn joinRunner)
	{
		checkf(m_link.isServer() && m_hostServiced && static_cast<bool>(joinRunner),
			TEXT("UESimNetLanes::bindAuthorityRunner: only a dedicated server's host-serviced lanes hold the runner's join. Was "
				 "the runner's RTT publish and its join (og-session-runtime task 39)"));
		m_joinRunner = MoveTemp(joinRunner);
	}

	bool hostServiced() const { return m_hostServiced; }
	UESimNetClockServer* clockServer() { return m_link.clockServer(); }
	const UESimNetClockClient* clockClient() const { return m_link.clockClient(); }
	double ackSrttSeconds() const { return m_ackSrttSeconds.load(std::memory_order_relaxed); }
	bool isServer() const { return m_link.isServer(); }
	bool clientSessionBound() const { return m_link.clientSessionBound(); }
	bool simTrafficImmediateRefused() const { return m_simTrafficImmediateRefused; }
	UOGNetDriver* driver() const { return m_driver; }
	ognetunreal::SimLaneEndpoint& endpoint() { return m_link.endpoint(); }
	Transport& transport() { return m_link.transport(); }
	Client* client() { return m_link.client(); }
	const typename Link::Bindings& bindings() const { return m_link.bindings(); }
	typename Link::Bindings& editBindings() { return m_link.editBindings(); }

private:
	void publishAckSrtt_GameThread()
	{
		const SimConnectionId server = m_link.transport().serverConnection();
		const double srttSeconds =
			m_driver != nullptr && server != kInvalidSimConnection ? m_driver->GetSmoothedRttSeconds(server) : kNoRttReadingSeconds;
		m_ackSrttSeconds.store(srttSeconds, std::memory_order_relaxed);
	}

	UOGNetDriver* m_driver;
	bool m_simTrafficImmediateRefused = false;
	bool m_hostServiced = false;
	std::atomic<double> m_ackSrttSeconds{kNoRttReadingSeconds};
	UESimNetLinkServices m_services;
	UESimNetLinkSession<TraitsT> m_sessionHooks;
	Link m_link;
	FFrameFn m_joinRunner;
	FFrameFn m_onStop;
};
