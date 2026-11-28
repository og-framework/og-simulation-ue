#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetLogRoutes-rationale.md

#include "OGSimulation/HostLog.h"
#include "OGSimulation/SimulationLogRoutes.h"

namespace ogsimnet
{
inline constexpr SimulationLogRouteEntry kSimNetLogRoutes[] = {
	{ "[SimNet.",                       SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[Roster.",                       SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ReceiveCorrectionState]",       SimulationLogRoute::Net,             HostLogLevel::Verbose },
	{ "[ClockSource.Held]",             SimulationLogRoute::Net,             HostLogLevel::Log }
};

static_assert(simulationLogRoutes::allReachable(kSimNetLogRoutes),
	"kSimNetLogRoutes: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier route always wins and the "
	"later one can never be taken (a host matches prefixes ignoring case, and so does this check)");
static_assert(simulationLogRoutes::plainLevelsAreLogOrVerbose(kSimNetLogRoutes),
	"kSimNetLogRoutes: a row's plainLevel is the level of a line that carries no [Verbose] or [Warning] token, and it may "
	"only be Log or Verbose. A line that must reach Warning says so with its own [Warning] token.");
} // namespace ogsimnet
