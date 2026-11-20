#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetIdle-rationale.md

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/TickDriverConcepts.h"

#include <concepts>
#include <cstddef>
#include <utility>

namespace ogsimnet
{
template <class E>
concept SimNetIdleEndpoint = requires(E& endpoint, double deadlineSeconds)
{
	{ endpoint.serviceUntil(deadlineSeconds) } -> std::same_as<ognet::ServiceResult>;
};

constexpr IdleWake idleWakeFor(const ognet::ServiceResult& result)
{
	if (result.datagrams > 0u)
		return IdleWake::Activity;
	if (result.reachedDeadline)
		return IdleWake::Deadline;
	return IdleWake::Unavailable;
}

constexpr ognet::ServiceResult simNetServiceResult(std::size_t datagrams, bool reachedDeadline, bool stopped)
{
	ognet::ServiceResult result;
	result.datagrams = datagrams;
	result.reachedDeadline = reachedDeadline;
	result.stopped = stopped;
	return result;
}

static_assert(idleWakeFor(simNetServiceResult(1u, false, false)) == IdleWake::Activity
		&& idleWakeFor(simNetServiceResult(3u, true, false)) == IdleWake::Activity,
	"idleWakeFor: a pass that handled a datagram is an Activity wake, whatever else it reached: its handlers ran on the "
	"servicing thread and the session must drain what they queued before the runner idles again");
static_assert(idleWakeFor(simNetServiceResult(0u, true, false)) == IdleWake::Deadline,
	"idleWakeFor: a pass that reached its deadline with no datagram is a Deadline wake");
static_assert(idleWakeFor(simNetServiceResult(0u, false, true)) == IdleWake::Unavailable
		&& idleWakeFor(simNetServiceResult(2u, false, true)) == IdleWake::Activity,
	"idleWakeFor: a stopped endpoint cannot wait, so a pass that only stopped is Unavailable (the driver then sleeps on its "
	"runner host); a pass that handled datagrams before it stopped is still an Activity wake");

template <SimNetIdleEndpoint EndpointT>
class SimNetIdle
{
public:
	explicit SimNetIdle(EndpointT& endpoint)
		: m_endpoint(endpoint)
	{
	}

	IdleWake idleUntil(double deadlineSeconds)
	{
		const ognet::ServiceResult result = m_endpoint.serviceUntil(deadlineSeconds);
		OG_CHECK(result.datagrams > 0u || result.reachedDeadline || result.stopped,
			"SimNetIdle::idleUntil: serviceUntil returned with no datagram, short of its deadline and not stopped; og-net's "
			"service loop returns only on one of the three, and SimNetIdle maps nothing else");
		return idleWakeFor(result);
	}

private:
	EndpointT& m_endpoint;
};

namespace simNetIdleSelfCheck
{
struct ReferenceEndpoint
{
	ognet::ServiceResult serviceUntil(double);
};

struct ReturnsBool
{
	bool serviceUntil(double);
};

struct NoServiceUntil
{
};

struct ForwardingGame : tickDriverGameSelfCheck::Conforming
{
	SimNetIdle<ReferenceEndpoint>* idle = nullptr;

	decltype(auto) idleUntil(double deadlineSeconds) { return idle->idleUntil(deadlineSeconds); }
};

static_assert(SimNetIdleEndpoint<ReferenceEndpoint>, "SimNetIdleEndpoint self-check: og-net's serviceUntil shape must satisfy it");
static_assert(!SimNetIdleEndpoint<ReturnsBool> && !SimNetIdleEndpoint<NoServiceUntil>,
	"SimNetIdleEndpoint self-check: an endpoint without serviceUntil, or one whose pass does not report og-net's ServiceResult, "
	"has nothing to map");
static_assert(std::same_as<decltype(std::declval<SimNetIdle<ReferenceEndpoint>&>().idleUntil(0.0)), IdleWake>,
	"SimNetIdle::idleUntil returns the tick driver's IdleWake");
static_assert(TickDriverIdleGame<ForwardingGame>,
	"SimNetIdle: a game that forwards idleUntil to SimNetIdle satisfies TickDriverIdleGame, the FreeRunning driver's idle hook");
} // namespace simNetIdleSelfCheck
} // namespace ogsimnet
