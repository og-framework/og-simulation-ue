#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetServerHost-rationale.md · docs/SimNetServerHost-guards.md

#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"

#include <concepts>
#include <cstdint>
#include <thread>

namespace ogsimnet
{
template <class S>
concept SimNetServerHostServices = requires(S& services, const char* line, SimConnectionId conn)
{
	{ services.nowSeconds() } -> std::same_as<double>;
	{ services.logHost(line) } -> std::same_as<void>;
	{ services.rttMs(conn) } -> std::same_as<double>;
};

template <class H>
concept SimNetServerHostSession = requires(H& session, uint8_t simId, uint32_t tick, double seconds)
{
	{ session.receptionTick() } -> std::same_as<int32_t>;
	{ session.stampServerReceived(simId, tick, seconds) } -> std::same_as<void>;
	{ session.stampHanded(simId, tick) } -> std::same_as<void>;
	{ session.onLifecycleUnregistering(simId) } -> std::same_as<void>;
};

template <class E>
concept SimNetServerHostEndpoint = requires(E& endpoint, ognet::Lane lane, double seconds)
{
	{ endpoint.noteDelivered(lane, seconds, seconds) } -> std::same_as<void>;
};

template <SimNetServerHostServices ServicesT, SimNetServerHostSession SessionT, SimNetServerHostEndpoint EndpointT>
class SimNetServerHostCore
{
public:
	SimNetServerHostCore(ServicesT& services, EndpointT& endpoint, SessionT* session)
		: m_services(services), m_endpoint(endpoint), m_session(session), m_servingThread(std::this_thread::get_id())
	{
	}

	SimNetServerHostCore(const SimNetServerHostCore&) = delete;
	SimNetServerHostCore& operator=(const SimNetServerHostCore&) = delete;

	double nowSeconds()
	{
		checkServingThread();
		return m_services.nowSeconds();
	}

	double rttMs(SimConnectionId conn)
	{
		checkServingThread();
		return m_services.rttMs(conn);
	}

	int32_t receptionTick()
	{
		checkServingThread();
		OG_CHECK(m_session != nullptr,
			"SimNetServerHostCore::receptionTick with no session: the server session that owns this host binds it at construction, "
			"and SimNetServer asks for the tick only while it drains a received input");
		return m_session != nullptr ? m_session->receptionTick() : 0;
	}

	void log(const char* line)
	{
		checkServingThread();
		// ⛔G-01  docs/SimNetServerHost-guards.md
		routeSimNetHostLine(line, [this](const char* routed) { m_services.logHost(routed); });
	}

	void noteDelivered(SimLane lane, double recvSeconds)
	{
		checkServingThread();
		// ⛔G-02  docs/SimNetServerHost-guards.md
		noteSimLaneDelivered(m_endpoint, lane, recvSeconds, m_services.nowSeconds());
	}

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		checkServingThread();
		if (m_session == nullptr)
			return;
		m_session->stampServerReceived(simId, captureTick, recvSeconds);
	}

	void stampHanded(uint8_t simId, uint32_t captureTick)
	{
		checkServingThread();
		if (m_session != nullptr)
			m_session->stampHanded(simId, captureTick);
	}

	void onLifecycleUnregistering(uint8_t simId)
	{
		checkServingThread();
		if (m_session != nullptr)
			m_session->onLifecycleUnregistering(simId);
	}

	ServicesT& services() { return m_services; }
	SessionT* session() const { return m_session; }

private:
	void checkServingThread() const
	{
		OG_CHECK(std::this_thread::get_id() == m_servingThread,
			"SimNetServerHostCore: a door runs on the thread that built the host, the thread that drives its SimNetServer (on a "
			"dedicated server the authority runner)");
	}

	ServicesT& m_services;
	EndpointT& m_endpoint;
	SessionT* m_session;
	std::thread::id m_servingThread;
};

namespace simNetServerHostSelfCheck
{
struct ReferenceServices
{
	double nowSeconds();
	void logHost(const char* line);
	double rttMs(SimConnectionId conn);
};

struct ServicesWithoutNowSeconds
{
	void logHost(const char* line);
	double rttMs(SimConnectionId conn);
};

struct ServicesWithoutLogHost
{
	double nowSeconds();
	double rttMs(SimConnectionId conn);
};

struct ServicesWithoutRttMs
{
	double nowSeconds();
	void logHost(const char* line);
};

struct ServicesWithAFloatNowSeconds
{
	float nowSeconds();
	void logHost(const char* line);
	double rttMs(SimConnectionId conn);
};

struct ServicesWithAFloatRttMs
{
	double nowSeconds();
	void logHost(const char* line);
	float rttMs(SimConnectionId conn);
};

struct ReferenceSession
{
	int32_t receptionTick();
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void stampHanded(uint8_t simId, uint32_t captureTick);
	void onLifecycleUnregistering(uint8_t simId);
};

struct SessionWithoutReceptionTick
{
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void stampHanded(uint8_t simId, uint32_t captureTick);
	void onLifecycleUnregistering(uint8_t simId);
};

struct SessionWithoutStampServerReceived
{
	int32_t receptionTick();
	void stampHanded(uint8_t simId, uint32_t captureTick);
	void onLifecycleUnregistering(uint8_t simId);
};

struct SessionWithoutStampHanded
{
	int32_t receptionTick();
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void onLifecycleUnregistering(uint8_t simId);
};

struct SessionWithoutUnregistering
{
	int32_t receptionTick();
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void stampHanded(uint8_t simId, uint32_t captureTick);
};

struct SessionWithAnUnsignedReceptionTick
{
	uint32_t receptionTick();
	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds);
	void stampHanded(uint8_t simId, uint32_t captureTick);
	void onLifecycleUnregistering(uint8_t simId);
};

struct ReferenceEndpoint
{
	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds);
};

struct EndpointWithATwoArgumentNoteDelivered
{
	void noteDelivered(ognet::Lane lane, double recvSeconds);
};

using ReferenceHost = SimNetServerHostCore<ReferenceServices, ReferenceSession, ReferenceEndpoint>;

static_assert(SimNetServerHostServices<ReferenceServices>,
	"SimNetServerHostServices self-check: the reference shape (nowSeconds, logHost, rttMs) must satisfy the concept");
static_assert(!SimNetServerHostServices<ServicesWithoutNowSeconds>,
	"SimNetServerHostServices self-check: nowSeconds() is required (the host's clock and the delivery note's now)");
static_assert(!SimNetServerHostServices<ServicesWithoutLogHost>,
	"SimNetServerHostServices self-check: logHost(line) is required (og-sim-net's server lines, printed with their token). Was "
	"UESimNetServerHost::log's RouteOGMessage sink");
static_assert(!SimNetServerHostServices<ServicesWithoutRttMs>,
	"SimNetServerHostServices self-check: rttMs(conn) is required (the round trip the reception coordinator files per bundle)");
static_assert(!SimNetServerHostServices<ServicesWithAFloatNowSeconds>,
	"SimNetServerHostServices self-check: nowSeconds returns double; a float clock of seconds since start loses milliseconds within "
	"hours, and the delivery note's residence is a difference of two such times");
static_assert(!SimNetServerHostServices<ServicesWithAFloatRttMs>,
	"SimNetServerHostServices self-check: rttMs returns double, the type the tier table smooths (SimNetServerHost's own rule)");

static_assert(SimNetServerHostSession<ReferenceSession>,
	"SimNetServerHostSession self-check: the reference shape (receptionTick, the two stamps, onLifecycleUnregistering) must "
	"satisfy the concept");
static_assert(!SimNetServerHostSession<SessionWithoutReceptionTick>,
	"SimNetServerHostSession self-check: receptionTick() is required (the tick the coordinator files an RTT sample under)");
static_assert(!SimNetServerHostSession<SessionWithoutStampServerReceived>,
	"SimNetServerHostSession self-check: stampServerReceived(simId, captureTick, recvSeconds) is required (the latency probe's "
	"ServerReceived stamp). Was static_assert(!SimNetServerHost<UESimNetServerHost>) in UESimNetHostsConceptTest.cpp");
static_assert(!SimNetServerHostSession<SessionWithoutStampHanded>,
	"SimNetServerHostSession self-check: stampHanded(simId, captureTick) is required (the latency probe's Handed stamp). Was "
	"static_assert(!SimNetServerHost<UESimNetServerHost>) in UESimNetHostsConceptTest.cpp");
static_assert(!SimNetServerHostSession<SessionWithoutUnregistering>,
	"SimNetServerHostSession self-check: onLifecycleUnregistering(simId) is required (the session's notify before og-sim-net's "
	"facade unregisters a queue-registered character)");
static_assert(!SimNetServerHostSession<SessionWithAnUnsignedReceptionTick>,
	"SimNetServerHostSession self-check: receptionTick returns int32_t, the tick type SimNetServerHost hands the coordinator");

static_assert(SimNetServerHostEndpoint<ReferenceEndpoint>,
	"SimNetServerHostEndpoint self-check: the reference shape (og-net's noteDelivered(lane, recvSeconds, nowSeconds)) must "
	"satisfy the concept");
static_assert(!SimNetServerHostEndpoint<EndpointWithATwoArgumentNoteDelivered>,
	"SimNetServerHostEndpoint self-check: noteDelivered takes the receive time and now (the endpoint's [OGNet.Window] residence "
	"rows)");

static_assert(SimNetServerHost<ReferenceHost> && SimNetDeliveryHook<ReferenceHost> && SimNetDeliveryHookWellFormed<ReferenceHost>
		&& SimNetLifecycleUnregisteringHook<ReferenceHost> && SimNetLifecycleUnregisteringHookWellFormed<ReferenceHost>,
	"SimNetServerHostCore is SimNetServer's HostT, with both optional hooks: the delivery note and the unregistering hook. Was "
	"UESimNetLanesServerHost's shape (og-session-runtime task 31a)");
} // namespace simNetServerHostSelfCheck
} // namespace ogsimnet
