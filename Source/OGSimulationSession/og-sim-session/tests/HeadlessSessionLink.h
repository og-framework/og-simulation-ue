#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../OGSimSession/docs/HeadlessSessionLink-rationale.md

#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

#include "HeadlessPlatform.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimNet/SimNetServerHost.h"
#include "OGSimSession/SessionRuntime.h"

namespace headlessSession
{
template <typename GameTraitsT> struct RuntimeTraits;
template <typename GameTraitsT> class Peer;
template <typename GameTraitsT> using Runtime = ogsession::SessionRuntime<RuntimeTraits<GameTraitsT>>;
template <typename SimT>
using Channel = ogsimnet::SessionChannel<SimT, ogsimnet::SimNetTransport<Endpoint, ogsimnet::kSimNetLinkInboxSlots>::ChannelTransport>;

template <typename RuntimeT>
struct ServerPort
{
	RuntimeT* runtime  = nullptr;
	Endpoint* endpoint = nullptr;

	double  nowSeconds() { return monotonicSeconds(); }
	void    logHost(const char* line) { logLine(line); }
	double  rttMs(SimConnectionId conn) { return endpoint->srttMs(conn); }
	int32_t receptionTick() { return runtime->getServerReceptionTick(); }
	void    stampServerReceived(uint8_t, uint32_t, double) {}
	void    stampHanded(uint8_t, uint32_t) {}
	void    onLifecycleUnregistering(uint8_t simId) { runtime->onSimNetLifecycleUnregistering(simId); }
};

template <typename RuntimeT>
struct ServerPortPart
{
	ServerPort<RuntimeT> m_port;
};

template <typename RuntimeT>
class ServerHost : private ServerPortPart<RuntimeT>, public ogsimnet::SimNetServerHostCore<ServerPort<RuntimeT>, ServerPort<RuntimeT>, Endpoint>
{
public:
	ServerHost(std::nullptr_t, Endpoint& endpoint, RuntimeT* runtime)
		: ServerPortPart<RuntimeT>{ { runtime, &endpoint } }
		, ogsimnet::SimNetServerHostCore<ServerPort<RuntimeT>, ServerPort<RuntimeT>, Endpoint>(this->m_port, endpoint, &this->m_port)
	{
	}

	void publishRunnerRtt_GameThread() {}
};

template <typename GameTraitsT>
struct LinkParts
{
	struct Services
	{
		Endpoint* endpoint = nullptr;
		double nowSeconds() { return monotonicSeconds(); }
		void   logRouted(const char* line) { logLine(line); }
		void   logHost(const char* line) { logLine(line); }
		double serverRttSeconds() { return endpoint->serverSrttSeconds(); }
		void   onWireMismatch(uint8_t, uint8_t) { logLine("[Error][SimNet.Host] role=client wire version mismatch"); }
	};

	struct SessionHooks
	{
		Runtime<GameTraitsT>* runtime = nullptr;
		void onTimingInfo(uint32_t tick, double rttSeconds) { runtime->onTimingInfoReceived(tick, rttSeconds); }
		void onRelayFloor(uint8_t floorTicks) { runtime->onRelayDelayFloorReceived(floorTicks); }
		void onConnectionTier(uint8_t oldTier, uint8_t newTier) { runtime->onConnectionTierReceived(oldTier, newTier); }
		void onClockSample(const SimClockRttSample& sample) { runtime->onSimClockSample(sample); }
		void onLifecycleUnregistering(uint8_t simId) { runtime->onSimNetClientLifecycleUnregistering(simId); }
		void stampClientReceived(ogsimnet::SimNetStream, uint8_t, uint32_t, double) {}
	};

	struct Clock
	{
		double nowSeconds() { return monotonicSeconds(); }
	};

	using Link = ogsimnet::SimNetLink<typename GameTraitsT::Composition, Endpoint, Clock, Services, std::mutex>;

	Services     m_linkServices;
	SessionHooks m_linkSessionHooks;
};

template <typename GameTraitsT>
class Lanes : private LinkParts<GameTraitsT>, public LinkParts<GameTraitsT>::Link
{
public:
	using Link       = typename LinkParts<GameTraitsT>::Link;
	using ServerHost = headlessSession::ServerHost<Runtime<GameTraitsT>>;
	using FrameFn    = std::function<void()>;

	explicit Lanes(bool isServer) : Link(this->m_linkServices, isServer) {}

	void beforeStart(Endpoint& endpoint)
	{
		this->m_linkServices.endpoint = &endpoint;
		Link::start(endpoint);
	}

	void afterDispatch()
	{
		if (!Link::isServer())
			Link::serviceFrame([] {});
		else if (m_publishRunnerRtt)
			m_publishRunnerRtt();
	}

	void beforeStop()
	{
		if (const FrameFn joinRunner = std::exchange(m_joinRunner, nullptr))
			joinRunner();
		if (const FrameFn onStop = std::exchange(m_onStop, nullptr))
			onStop();
		unbindServerSession();
		Link::stop();
	}

	bool           hostServiced() const { return Link::isServer(); }
	std::nullptr_t driver() const { return nullptr; }
	double         ackSrttSeconds() const { return Link::isServer() ? ogsimnet::kSimNetLinkNoRttSeconds : this->m_linkServices.endpoint->serverSrttSeconds(); }
	void           drainClient_Step() { Link::drainForStep(); }
	void           drainClientWhileStopped_GameThread() { Link::drainWhileStopped(); }
	void           setStopHandler(FrameFn onStop) { m_onStop = std::move(onStop); }
	void           unbindServerSession() { m_publishRunnerRtt = m_joinRunner = nullptr; }
	void           unbindClientSession() { Link::detachSession(); }

	void bindAuthorityRunner(FrameFn publishRunnerRtt, FrameFn joinRunner)
	{
		m_publishRunnerRtt = std::move(publishRunnerRtt);
		m_joinRunner = std::move(joinRunner);
	}

	void bindClientSession(Runtime<GameTraitsT>& runtime)
	{
		this->m_linkSessionHooks.runtime = &runtime;
		Link::attachSession(this->m_linkSessionHooks);
	}

private:
	FrameFn m_publishRunnerRtt;
	FrameFn m_joinRunner;
	FrameFn m_onStop;
};
} // namespace headlessSession
