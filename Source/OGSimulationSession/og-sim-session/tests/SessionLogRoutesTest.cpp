// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionLogRoutes.h"
#include "OGSimSession/SessionNetHooks.h"
#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTypes.h"
#include "OGSimulation/HostLog.h"
#include "OGSimulation/SimulationLogRoutes.h"
#include "OGTickDriver/TickDriverThreadContext.h"
#include "SessionLogRoutesEmittedTags.h"
#include "SessionLogRoutesGolden.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
enum class Owner
{
	Simulation,
	SimNet,
	Session,
	Adapter,
};

constexpr Owner ownerOf(std::size_t goldenIndex)
{
	switch (goldenIndex)
	{
	case 27: case 28: case 34: case 53:
		return Owner::SimNet;
	case 38: case 40: case 41: case 43: case 44: case 45: case 46: case 47: case 48: case 49: case 50: case 51: case 52:
	case 54: case 55:
		return Owner::Session;
	case 30: case 31: case 32: case 33: case 35: case 36: case 37: case 39: case 42:
		return Owner::Adapter;
	default:
		return Owner::Simulation;
	}
}

const char* nameOf(SimulationLogRoute route)
{
	switch (route)
	{
	case SimulationLogRoute::SimTick:         return "SimTick";
	case SimulationLogRoute::Sim:             return "Sim";
	case SimulationLogRoute::Net:             return "Net";
	case SimulationLogRoute::DivergenceProbe: return "DivergenceProbe";
	case SimulationLogRoute::Mgmt:            return "Mgmt";
	case SimulationLogRoute::SimHost:         return "SimHost";
	}
	return "<unnamed>";
}

const char* nameOf(HostLogLevel level)
{
	switch (level)
	{
	case HostLogLevel::Verbose: return "Verbose";
	case HostLogLevel::Log:     return "Log";
	case HostLogLevel::Warning: return "Warning";
	}
	return "<unnamed>";
}

using GoldenRow = sessionLogRoutesGolden::Row;

std::vector<GoldenRow> goldenReblocked(bool withAdapter)
{
	std::vector<GoldenRow> rows;
	for (const Owner owner : { Owner::Simulation, Owner::SimNet, Owner::Session, Owner::Adapter })
	{
		if (owner == Owner::Adapter && !withAdapter)
			continue;
		for (const GoldenRow& row : sessionLogRoutesGolden::kRows)
		{
			if (ownerOf(row.index) == owner)
				rows.push_back(row);
		}
	}
	return rows;
}

const GoldenRow* goldenFirstMatch(const char* line)
{
	const char* body = ogsession::sessionLogRoutes::bodyOf(line);
	for (const GoldenRow& row : sessionLogRoutesGolden::kRows)
	{
		if (simulationLogRoutes::startsWithIgnoringCase(body, row.prefix))
			return &row;
	}
	return nullptr;
}

std::string describe(const SimulationLogRouteEntry* row)
{
	return row == nullptr ? std::string("<none>")
	                      : std::string(row->prefix) + " " + nameOf(row->route) + " " + nameOf(row->plainLevel);
}

std::string describe(const GoldenRow* row)
{
	return row == nullptr ? std::string("<none>") : std::string(row->prefix) + " " + row->route + " " + row->level;
}

struct CountingServer
{
	int releases = 0;
	void releaseForStep(int32_t, int32_t, std::optional<double>) { ++releases; }
};

struct NoClient
{
};

struct NoLanes
{
	using Client = NoClient;
	Client* client() { return nullptr; }
	bool    clientSessionBound() const { return false; }
	void    drainClient_Step() {}
};

struct NoIdle
{
};

struct NoSimulatable
{
};

struct Asked
{
	SimulationLogRoute route;
	HostLogLevel       level;
};

struct CountingPort
{
	using Lanes       = NoLanes;
	using Server      = CountingServer;
	using Idle        = NoIdle;
	using Simulatable = NoSimulatable;

	CountingServer*           serverPtr = nullptr;
	std::vector<std::string>* lines     = nullptr;
	std::vector<Asked>*       asked     = nullptr;
	bool                      verboseOn = false;

	Lanes*  lanes() const { return nullptr; }
	Server* server() { return serverPtr; }
	Idle*   idle() { return nullptr; }
	void    runRunnerRequests() {}
	void    applyUnregisterRequests(NoClient&) {}
	void    publishPresentation() {}
	double  nowSeconds() const { return 0.0; }
	void    logRouted(const char* line) { lines->emplace_back(line); }
	bool    wants(SimulationLogRoute route, HostLogLevel level) const
	{
		asked->push_back({ route, level });
		return level != HostLogLevel::Verbose || verboseOn;
	}
};

struct NoManager
{
};

struct NoStamps
{
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
};

using CountingHooks = ogsession::SessionNetHooks<CountingPort, NoManager, int, int, NoStamps>;

struct FormatCount
{
	int lines = 0;
	int releases = 0;
	int asked = 0;
	bool everyAskWasNetVerbose = true;
	std::string first;
};

FormatCount releaseSteps(bool verboseOn, int steps)
{
	TickDriverThreadContext context;
	context.configure<true>(tickDriverThread::kAuthorityRoleBit, StepThreading::FreeRunning);
	const ogsession::SessionThreadChecks threads(ogsession::SessionRole::Authority, &context, std::this_thread::get_id());
	CountingServer server;
	std::vector<std::string> lines;
	std::vector<Asked> asked;
	NoManager manager;
	int netSync = 0;
	int storage = 0;
	NoStamps stamps;
	CountingHooks hooks(threads, CountingPort{ &server, &lines, &asked, verboseOn }, manager, netSync, storage, stamps);
	for (int step = 0; step < steps; ++step)
		hooks.releaseForTick(200 + step, 1, std::nullopt);

	FormatCount count;
	count.lines = static_cast<int>(lines.size());
	count.releases = server.releases;
	count.asked = static_cast<int>(asked.size());
	for (const Asked& a : asked)
		count.everyAskWasNetVerbose = count.everyAskWasNetVerbose && a.route == SimulationLogRoute::Net
			&& a.level == HostLogLevel::Verbose;
	count.first = lines.empty() ? std::string() : lines.front();
	return count;
}
} // namespace

TEST_CASE("SessionLogRoutes.TheComposedTableIsTheGoldenReblockedByOwnerRowByRow", "[Session][SessionLogRoutes]")
{
	REQUIRE(std::size(sessionLogRoutesGolden::kRows) == 56u);
	for (std::size_t i = 0; i < std::size(sessionLogRoutesGolden::kRows); ++i)
		REQUIRE(sessionLogRoutesGolden::kRows[i].index == i);

	const std::vector<GoldenRow> expected = goldenReblocked(/*withAdapter=*/false);
	REQUIRE(expected.size() == 47u);
	REQUIRE(ogsession::kComposedLogRoutes.size() == expected.size());
	for (std::size_t i = 0; i < expected.size(); ++i)
	{
		INFO("composed index " << i << ", golden index " << expected[i].index);
		CHECK(std::string(ogsession::kComposedLogRoutes[i].prefix) == expected[i].prefix);
		CHECK(std::string(nameOf(ogsession::kComposedLogRoutes[i].route)) == expected[i].route);
		CHECK(std::string(nameOf(ogsession::kComposedLogRoutes[i].plainLevel)) == expected[i].level);
	}

	CHECK(std::size(kSimulationLogRoutes) == 28u);
	CHECK(std::size(ogsimnet::kSimNetLogRoutes) == 4u);
	CHECK(std::size(ogsession::kSessionLogRoutes) == 15u);
	CHECK(ogsession::sessionLogRoutes::kSimulationBlockEnd == 28u);
	CHECK(ogsession::sessionLogRoutes::kSimNetBlockEnd == 32u);
	CHECK(ogsession::sessionLogRoutes::kSessionBlockEnd == 47u);
	STATIC_REQUIRE(simulationLogRoutes::allReachable(ogsession::kComposedLogRoutes));
}

TEST_CASE("SessionLogRoutes.EveryGoldenPrefixRoutesToTheSameRow", "[Session][SessionLogRoutes]")
{
	for (const GoldenRow& row : sessionLogRoutesGolden::kRows)
	{
		INFO("golden index " << row.index << " " << row.prefix);
		const GoldenRow* today = goldenFirstMatch(row.prefix);
		REQUIRE(today != nullptr);
		CHECK(today->index == row.index);
		const SimulationLogRouteEntry* composed = ogsession::sessionLogRoutes::rowOf(row.prefix);
		if (ownerOf(row.index) == Owner::Adapter)
			CHECK(describe(composed) == "<none>");
		else
			CHECK(describe(composed) == describe(today));
	}
}

TEST_CASE("SessionLogRoutes.EveryEmittedLineRoutesToTheSameRow", "[Session][SessionLogRoutes]")
{
	REQUIRE(std::size(sessionLogRoutesGolden::kEmittedLines) > 300u);
	int routedEngineFree = 0;
	for (const char* line : sessionLogRoutesGolden::kEmittedLines)
	{
		INFO("emitted line: " << line);
		const GoldenRow* today = goldenFirstMatch(line);
		const SimulationLogRouteEntry* composed = ogsession::sessionLogRoutes::rowOf(line);
		if (today == nullptr || ownerOf(today->index) == Owner::Adapter)
		{
			CHECK(describe(composed) == "<none>");
			continue;
		}
		CHECK(describe(composed) == describe(today));
		++routedEngineFree;
	}
	CHECK(routedEngineFree > 100);
}

TEST_CASE("SessionLogRoutes.TheMovedSimNetAndRosterLinesRouteThroughOgSimNetsRows", "[Session][SessionLogRoutes]")
{
	using ogsession::sessionLogRoutes::rowOf;
	CHECK(describe(rowOf("[SimNet.Window] role=server drains=1")) == "[SimNet. Net Log");
	CHECK(describe(rowOf("[SimNet.Host] role=client immediate=0")) == "[SimNet. Net Log");
	CHECK(describe(rowOf("[Roster.Refused] role=server simId=1 slot=2 reason=SimIdLive")) == "[Roster. Net Log");
	CHECK(describe(rowOf("[Roster.InputRefused] conn=1 simId=2 reason=NotOwner")) == "[Roster. Net Log");
	CHECK(describe(rowOf("[Warning][ClockSource.Held] fed the SimClock sample")) == "[ClockSource.Held] Net Log");
	CHECK(describe(rowOf("[ReceiveCorrectionState] id=1 tick=2")) == "[ReceiveCorrectionState] Net Verbose");
	CHECK(describe(rowOf("[Warning][ClockRecentre.Config] recentreSettleSamples=30 (compiled)"))
		== "[ClockRecentre.Config] Net Log");
	CHECK(describe(rowOf("[Error]tryRegister: id=3 refused")) == "tryRegister: Mgmt Log");
	CHECK(describe(rowOf("[ReleaseBatch] first=1 numSteps=1")) == "[ReleaseBatch] Net Verbose");
	CHECK(describe(rowOf("[ResimProbe.Frame] role=Client")) == "<none>");
}

TEST_CASE("SessionLogRoutes.ReleaseBatchIsFormattedOnlyWhenItsRouteLevelIsWanted", "[Session][SessionLogRoutes]")
{
	const FormatCount off = releaseSteps(/*verboseOn=*/false, 100);
	CHECK(off.releases == 100);
	CHECK(off.asked == 100);
	CHECK(off.everyAskWasNetVerbose);
	CHECK(off.lines == 0);

	const FormatCount on = releaseSteps(/*verboseOn=*/true, 100);
	CHECK(on.releases == 100);
	CHECK(on.asked == 100);
	CHECK(on.everyAskWasNetVerbose);
	CHECK(on.lines == 100);
	CHECK(on.first == "[ReleaseBatch] first=200 numSteps=1");
}
