#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionLogRoutes-rationale.md

#include "OGSimNet/SimNetLogRoutes.h"
#include "OGSimulation/HostLog.h"
#include "OGSimulation/SimulationLogRoutes.h"

#include <array>
#include <cstddef>
#include <iterator>

namespace ogsession
{
inline constexpr SimulationLogRouteEntry kSessionLogRoutes[] = {
	{ "[ReleaseBatch]",                 SimulationLogRoute::Net,             HostLogLevel::Verbose },
	{ "SimulationManager:",             SimulationLogRoute::Mgmt,            HostLogLevel::Log },
	{ "tryRegister:",                   SimulationLogRoute::Mgmt,            HostLogLevel::Log },
	{ "[SimHost.",                      SimulationLogRoute::SimHost,         HostLogLevel::Log },
	{ "[RelayDelayFloor]",              SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[StateRotation]",                SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ResimGate]",                    SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[LatencyKnobs]",                 SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ClockRecentre.Config]",         SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ClockSource.Config]",           SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[Dilation.Config]",              SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ConnectionTier]",               SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[ClockSource.Window]",           SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[Dilation.Window]",              SimulationLogRoute::Net,             HostLogLevel::Log },
	{ "[Dilation.Authority]",           SimulationLogRoute::Net,             HostLogLevel::Log }
};

namespace sessionLogRoutes
{
template <std::size_t SimRows, std::size_t NetRows, std::size_t SessionRows>
constexpr std::array<SimulationLogRouteEntry, SimRows + NetRows + SessionRows> concatenate(
	const SimulationLogRouteEntry (&simRows)[SimRows], const SimulationLogRouteEntry (&netRows)[NetRows],
	const SimulationLogRouteEntry (&sessionRows)[SessionRows])
{
	std::array<SimulationLogRouteEntry, SimRows + NetRows + SessionRows> rows{};
	std::size_t next = 0;
	for (const SimulationLogRouteEntry& row : simRows)
		rows[next++] = row;
	for (const SimulationLogRouteEntry& row : netRows)
		rows[next++] = row;
	for (const SimulationLogRouteEntry& row : sessionRows)
		rows[next++] = row;
	return rows;
}

inline constexpr std::size_t kSimulationBlockEnd = std::size(kSimulationLogRoutes);
inline constexpr std::size_t kSimNetBlockEnd     = kSimulationBlockEnd + std::size(ogsimnet::kSimNetLogRoutes);
inline constexpr std::size_t kSessionBlockEnd    = kSimNetBlockEnd + std::size(kSessionLogRoutes);
} // namespace sessionLogRoutes

inline constexpr std::array<SimulationLogRouteEntry, sessionLogRoutes::kSessionBlockEnd> kComposedLogRoutes =
	sessionLogRoutes::concatenate(kSimulationLogRoutes, ogsimnet::kSimNetLogRoutes, kSessionLogRoutes);

namespace sessionLogRoutes
{
static_assert(simulationLogRoutes::blocksAreDisjoint(kSimulationLogRoutes, ogsimnet::kSimNetLogRoutes)
		&& simulationLogRoutes::blocksAreDisjoint(kSimulationLogRoutes, kSessionLogRoutes)
		&& simulationLogRoutes::blocksAreDisjoint(ogsimnet::kSimNetLogRoutes, kSessionLogRoutes),
	"kComposedLogRoutes: a prefix in one module's block and a prefix in another's BEGIN WITH one another (ignoring case), so "
	"one line can match a row in each block and the block order decides its route. The blocks are concatenated in module "
	"order only because no line can match two of them; give the new row a tag no other block's prefix begins with.");
static_assert(simulationLogRoutes::allReachable(kComposedLogRoutes),
	"kComposedLogRoutes: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier route always wins and the "
	"later one can never be taken (a host matches prefixes ignoring case, and so does this check)");
static_assert(simulationLogRoutes::plainLevelsAreLogOrVerbose(kSessionLogRoutes),
	"kSessionLogRoutes: a row's plainLevel is the level of a line that carries no [Verbose] or [Warning] token, and it may "
	"only be Log or Verbose. A line that must reach Warning says so with its own [Warning] token.");

constexpr const char* bodyOf(const char* line)
{
	if (simulationLogRoutes::startsWithIgnoringCase(line, "[Verbose]")
		|| simulationLogRoutes::startsWithIgnoringCase(line, "[Warning]"))
		return line + 9;
	if (simulationLogRoutes::startsWithIgnoringCase(line, "[Error]"))
		return line + 7;
	return line;
}

constexpr const SimulationLogRouteEntry* rowOf(const char* line)
{
	return simulationLogRoutes::firstMatch(kComposedLogRoutes, bodyOf(line));
}
} // namespace sessionLogRoutes
} // namespace ogsession
