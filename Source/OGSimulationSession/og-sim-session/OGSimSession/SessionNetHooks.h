#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionNetHooks-rationale.md

#include <concepts>
#include <cstdint>
#include <cstdio>
#include <optional>

#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/SimulationLogRoutes.h"
#include "OGSimulation/TickDriverGameTypes.h"
#include "OGSimSession/SessionEngine.h"
#include "OGSimSession/SessionLogRoutes.h"
#include "OGSimSession/SessionThreads.h"

namespace ogsession
{
namespace sessionNetHooksLines
{
inline constexpr char kReleaseBatch[] = "[ReleaseBatch] first=%d numSteps=%d";
inline constexpr const SimulationLogRouteEntry* kReleaseBatchRoute = sessionLogRoutes::rowOf(kReleaseBatch);

static_assert(kReleaseBatchRoute != nullptr,
	"kReleaseBatch: the line's tag has no row in ogsession::kComposedLogRoutes, so wants() has no route and level to ask "
	"about and a host would print the line in its default category");
} // namespace sessionNetHooksLines

template <typename L>
concept SessionNetLanes = requires(L& lanes, const L& constLanes)
{
	typename L::Client;
	{ lanes.client() }                  -> std::same_as<typename L::Client*>;
	{ constLanes.clientSessionBound() } -> std::same_as<bool>;
	{ lanes.drainClient_Step() }        -> std::same_as<void>;
};

template <typename P>
concept SessionNetHooksPort = SessionNetLanes<typename P::Lanes> && SessionLogFilter<P>
	&& requires(P& p, const P& cp, typename P::Lanes::Client& client, const char* line)
{
	typename P::Server;
	typename P::Idle;
	typename P::Simulatable;
	{ cp.lanes() }                        -> std::same_as<typename P::Lanes*>;
	{ p.server() }                        -> std::same_as<typename P::Server*>;
	{ p.idle() }                          -> std::same_as<typename P::Idle*>;
	{ p.runRunnerRequests() }             -> std::same_as<void>;
	{ p.applyUnregisterRequests(client) } -> std::same_as<void>;
	{ p.publishPresentation() }           -> std::same_as<void>;
	{ cp.nowSeconds() }                   -> std::same_as<double>;
	{ p.logRouted(line) }                 -> std::same_as<void>;
};

template <typename D>
concept SessionCorrectionStamps = requires(D& diagnostics, unsigned int lane, uint32_t tick, double seconds)
{
	{ diagnostics.stampCorrectionHanded_Internal(lane, tick, seconds) } -> std::same_as<void>;
};

namespace sessionNetHooksSelfCheck
{
struct Client
{
};

struct ConformingLanes
{
	using Client = sessionNetHooksSelfCheck::Client;
	Client* client();
	bool    clientSessionBound() const;
	void    drainClient_Step();
};

struct LanesWithoutClient
{
	using Client = sessionNetHooksSelfCheck::Client;
	bool clientSessionBound() const;
	void drainClient_Step();
};

struct LanesBoundOnlyWhenMutable : ConformingLanes
{
	bool clientSessionBound();
};

struct LanesDrainReturnsCount : ConformingLanes
{
	int drainClient_Step();
};

struct Server
{
};

struct Idle
{
};

struct Simulatable
{
};

struct ConformingPort
{
	using Lanes       = ConformingLanes;
	using Server      = sessionNetHooksSelfCheck::Server;
	using Idle        = sessionNetHooksSelfCheck::Idle;
	using Simulatable = sessionNetHooksSelfCheck::Simulatable;

	Lanes*  lanes() const;
	Server* server();
	Idle*   idle();
	void    runRunnerRequests();
	void    applyUnregisterRequests(Client&);
	void    publishPresentation();
	double  nowSeconds() const;
	void    logRouted(const char*);
	bool    wants(SimulationLogRoute, HostLogLevel) const;
};

struct PortWithoutSimulatable
{
	using Lanes  = ConformingLanes;
	using Server = sessionNetHooksSelfCheck::Server;
	using Idle   = sessionNetHooksSelfCheck::Idle;

	Lanes*  lanes() const;
	Server* server();
	Idle*   idle();
	void    runRunnerRequests();
	void    applyUnregisterRequests(Client&);
	void    publishPresentation();
	double  nowSeconds() const;
	void    logRouted(const char*);
	bool    wants(SimulationLogRoute, HostLogLevel) const;
};

struct PortWithoutWants
{
	using Lanes       = ConformingLanes;
	using Server      = sessionNetHooksSelfCheck::Server;
	using Idle        = sessionNetHooksSelfCheck::Idle;
	using Simulatable = sessionNetHooksSelfCheck::Simulatable;

	Lanes*  lanes() const;
	Server* server();
	Idle*   idle();
	void    runRunnerRequests();
	void    applyUnregisterRequests(Client&);
	void    publishPresentation();
	double  nowSeconds() const;
	void    logRouted(const char*);
};

struct PortWantsOnlyWhenMutable : ConformingPort
{
	bool wants(SimulationLogRoute, HostLogLevel);
};

struct PortWithLanesWithoutClient : ConformingPort
{
	using Lanes = LanesWithoutClient;
	Lanes* lanes() const;
};

struct PortWithAFloatClock : ConformingPort
{
	float nowSeconds() const;
};

struct PortLanesOnlyWhenMutable : ConformingPort
{
	ConformingLanes* lanes();
};

struct PortServerByValue : ConformingPort
{
	Server server();
};

struct PortIdleByValue : ConformingPort
{
	Idle idle();
};

struct PortRequestsReturnCount : ConformingPort
{
	int runRunnerRequests();
};

struct PortUnregisterWithoutClient : ConformingPort
{
	void applyUnregisterRequests();
};

struct PortPresentationReturnsBool : ConformingPort
{
	bool publishPresentation();
};

struct PortClockOnlyWhenMutable : ConformingPort
{
	double nowSeconds();
};

struct PortLogReturnsBool : ConformingPort
{
	bool logRouted(const char*);
};

struct ConformingStamps
{
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double);
};

struct StampsWithoutTime
{
	void stampCorrectionHanded_Internal(unsigned int, uint32_t);
};

static_assert(SessionNetLanes<ConformingLanes>,
	"SessionNetLanes self-check: the reference shape (client, a const clientSessionBound, drainClient_Step) must satisfy the "
	"concept");
static_assert(!SessionNetLanes<LanesWithoutClient>,
	"SessionNetLanes self-check: client() is required, the step's receive and the capture's send go through the bound client");
static_assert(!SessionNetLanes<LanesBoundOnlyWhenMutable>,
	"SessionNetLanes self-check: clientSessionBound() must be callable on const lanes");
static_assert(!SessionNetLanes<LanesDrainReturnsCount>,
	"SessionNetLanes self-check: drainClient_Step() returns void");
static_assert(SessionNetHooksPort<ConformingPort>,
	"SessionNetHooksPort self-check: the reference shape must satisfy the concept");
static_assert(!SessionNetHooksPort<PortWithoutSimulatable>,
	"SessionNetHooksPort self-check: the port names its Simulatable, the storage's type whose og-sim-net channels the "
	"corrections-handed stamps read");
static_assert(!SessionNetHooksPort<PortWithLanesWithoutClient>,
	"SessionNetHooksPort self-check: the port's lanes must model SessionNetLanes");
static_assert(!SessionNetHooksPort<PortLanesOnlyWhenMutable>,
	"SessionNetHooksPort self-check: lanes() must be callable on a const port and return the lanes by pointer");
static_assert(!SessionNetHooksPort<PortServerByValue>,
	"SessionNetHooksPort self-check: server() returns a pointer, null while the authority has no og-sim-net server");
static_assert(!SessionNetHooksPort<PortIdleByValue>,
	"SessionNetHooksPort self-check: idle() returns a pointer, null while the runner has no idle service");
static_assert(!SessionNetHooksPort<PortRequestsReturnCount>,
	"SessionNetHooksPort self-check: runRunnerRequests() returns void");
static_assert(!SessionNetHooksPort<PortUnregisterWithoutClient>,
	"SessionNetHooksPort self-check: applyUnregisterRequests takes the bound client");
static_assert(!SessionNetHooksPort<PortPresentationReturnsBool>,
	"SessionNetHooksPort self-check: publishPresentation() returns void");
static_assert(!SessionNetHooksPort<PortClockOnlyWhenMutable>,
	"SessionNetHooksPort self-check: nowSeconds() must be callable on a const port");
static_assert(!SessionNetHooksPort<PortWithAFloatClock>,
	"SessionNetHooksPort self-check: nowSeconds() returns double; a float of seconds since start loses milliseconds, and "
	"the corrections-handed stamp is such a time");
static_assert(!SessionNetHooksPort<PortLogReturnsBool>,
	"SessionNetHooksPort self-check: logRouted(line) returns void");
static_assert(!SessionNetHooksPort<PortWithoutWants>,
	"SessionNetHooksPort self-check: wants(route, level) is required; the [ReleaseBatch] line asks it before it formats");
static_assert(!SessionNetHooksPort<PortWantsOnlyWhenMutable>,
	"SessionNetHooksPort self-check: wants(route, level) must be callable on a const port");
static_assert(SessionCorrectionStamps<ConformingStamps>,
	"SessionCorrectionStamps self-check: the reference shape must satisfy the concept");
static_assert(!SessionCorrectionStamps<StampsWithoutTime>,
	"SessionCorrectionStamps self-check: the corrections-handed stamp carries the time it was handed");
} // namespace sessionNetHooksSelfCheck

template <SessionNetHooksPort PortT, typename ManagerT, typename NetSyncT, typename StorageT,
	SessionCorrectionStamps DiagnosticsT>
class SessionNetHooks
{
public:
	using Lanes       = typename PortT::Lanes;
	using Client      = typename Lanes::Client;
	using Server      = typename PortT::Server;
	using Idle        = typename PortT::Idle;
	using Simulatable = typename PortT::Simulatable;

	SessionNetHooks(const SessionThreadChecks& threads, PortT port, ManagerT& manager, NetSyncT& netSync, StorageT& storage,
		DiagnosticsT& diagnostics)
		: m_threads(threads)
		, m_port(port)
		, m_manager(manager)
		, m_netSync(netSync)
		, m_storage(storage)
		, m_diagnostics(diagnostics)
	{
	}

	SessionNetHooks(const SessionNetHooks&) = delete;
	SessionNetHooks& operator=(const SessionNetHooks&) = delete;

	void receiveForStep()
	{
		m_threads.checkOp(SessionOp::DrainInbox);
		Client* const client = boundClient();
		if (client == nullptr)
			return;
		m_port.applyUnregisterRequests(*client);
		m_port.lanes()->drainClient_Step();
	}

	void afterCapture()
	{
		m_threads.checkOp(SessionOp::Send);
		Client* const client = boundClient();
		if (client == nullptr)
			return;
		client->afterCaptureTick_Step(m_netSync, m_manager.currentIntegratedTick(), m_manager.getTimeConfig());
	}

	void beforePump()
	{
		m_threads.checkOp(SessionOp::DrainInbox);
		m_port.runRunnerRequests();
		if (Server* const server = m_port.server())
			server->drain();
	}

	IdleWake idleUntil(double deadlineSeconds)
	{
		m_threads.checkOp(SessionOp::ServiceNet);
		Idle* const idle = m_port.idle();
		return idle != nullptr ? idle->idleUntil(deadlineSeconds) : IdleWake::Unavailable;
	}

	void releaseForTick(int32_t firstTick, int32_t numSteps, std::optional<double> stepSeconds)
	{
		m_threads.checkOp(SessionOp::Step);
		Server* const server = m_port.server();
		if (server == nullptr)
			return;

		constexpr const SimulationLogRouteEntry& route = *sessionNetHooksLines::kReleaseBatchRoute;
		if (m_port.wants(route.route, route.plainLevel))
		{
			char line[64];
			std::snprintf(line, sizeof(line), sessionNetHooksLines::kReleaseBatch, firstTick, numSteps);
			m_port.logRouted(line);
		}

		server->releaseForStep(firstTick, numSteps, stepSeconds);
	}

	void afterBatch(int32_t firstUpcomingSimTick)
	{
		m_threads.checkOp(SessionOp::Send);
		Server* const server = m_port.server();
		if (server == nullptr)
			return;
		OG_CHECK(firstUpcomingSimTick != 0 || server->attachedConnections().empty(),
			"SessionNetHooks::afterBatch: the authority loop's first batch reports tick 0 (the driver publishes its offset after "
			"a tick ran), and a client capture reaped against it is refused by the coordinator's domain gate. The runner "
			"services og-net only in its idle, after its first batch, so no connection can be attached yet. Was the Unreal "
			"session's checkf in afterAuthorityBatch (og-input-latency P2-19 F2)");
		m_manager.onPostSimulationGameThread();
		server->publishClock(static_cast<uint32_t>(m_manager.getServerClock().getTick()));
		server->afterInlineBatch(firstUpcomingSimTick);
		stampCorrectionsHanded(*server);
		m_port.publishPresentation();
	}

	template <typename ChannelT>
	static uint32_t correctionTickOf(const ChannelT& channel)
	{
		const CorrectionStateBuffer& buffer = channel.getSyncedCorrectionStateBuffer();
		if (buffer.payload().size() < correctionStateBuffer::kHeaderBytes)
			return 0u;
		return buffer.template readFromBuffer<uint32_t>(correctionStateBuffer::kTickOffset);
	}

private:
	Client* boundClient() const
	{
		if (!m_manager.runsPrediction())
			return nullptr;
		Lanes* const lanes = m_port.lanes();
		Client* const client = lanes != nullptr ? lanes->client() : nullptr;
		return client != nullptr && lanes->clientSessionBound() ? client : nullptr;
	}

	void stampCorrectionsHanded(Server& server)
	{
		const double handedSeconds = m_port.nowSeconds();
		m_storage.template forEachSimulatable<Simulatable>([this, &server, handedSeconds](unsigned int id, const auto&)
		{
			auto* const channel =
				id != 0u && id <= 0xFFu ? server.template channel<Simulatable>(static_cast<uint8_t>(id)) : nullptr;
			if (channel == nullptr)
				return;
			const uint32_t correctionTick = correctionTickOf(*channel);
			if (correctionTick != 0u)
				m_diagnostics.stampCorrectionHanded_Internal(id, correctionTick, handedSeconds);
		});
	}

	SessionThreadChecks m_threads;
	PortT               m_port;
	ManagerT&           m_manager;
	NetSyncT&           m_netSync;
	StorageT&           m_storage;
	DiagnosticsT&       m_diagnostics;
};
} // namespace ogsession
