// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationLogRoutes-rationale.md · docs/UESimulationLogRoutes-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulation/SimulationLogRoutes.h"

#include <array>
#include <cstddef>
#include <iterator>

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGSim, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGSimTick, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGMgmt, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGNet, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOG, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGRelayProbe, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGDivergenceProbe, Log, All);
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGResimProbe, Log, All);

enum class OGLogRoute : uint8
{
	SimTick, ResimProbe, Sim, Net, RelayProbe, DivergenceProbe, Mgmt
};

struct OGLogRouteEntry
{
	const TCHAR*        prefix;
	OGLogRoute          route;
	ELogVerbosity::Type plainLevel;
};

inline constexpr OGLogRouteEntry kOGAdapterLogRoutes[] = {
	{ TEXT("[ResimProbe"),                       OGLogRoute::ResimProbe,      ELogVerbosity::Log },
	{ TEXT("[ResimCheck.TriggerRewind]"),        OGLogRoute::SimTick,         ELogVerbosity::Verbose },
	{ TEXT("[ClientPrediction]"),                OGLogRoute::SimTick,         ELogVerbosity::Verbose },
	{ TEXT("[ServerReceive]"),                   OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[SendRemoteInputToClients]"),        OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ReceiveCorrectionState]"),          OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[ReceiveCorrectionInput]"),          OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[InjectCorrectionInput]"),           OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[DrainOutOfOrder]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ReleaseBatch]"),                    OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[RelayProbe"),                       OGLogRoute::RelayProbe,      ELogVerbosity::Log },
	{ TEXT("SimulationManager:"),                OGLogRoute::Mgmt,            ELogVerbosity::Log },
	{ TEXT("tryRegister:"),                      OGLogRoute::Mgmt,            ELogVerbosity::Log },
	{ TEXT("NewFramework:"),                     OGLogRoute::Mgmt,            ELogVerbosity::Log }
};

inline constexpr OGLogRoute kOGLogRouteUnmapped = static_cast<OGLogRoute>(0xFF);

constexpr OGLogRoute ogLogRouteOf(SimulationLogRoute route)
{
	switch (route)
	{
	case SimulationLogRoute::SimTick:         return OGLogRoute::SimTick;
	case SimulationLogRoute::Sim:             return OGLogRoute::Sim;
	case SimulationLogRoute::Net:             return OGLogRoute::Net;
	case SimulationLogRoute::DivergenceProbe: return OGLogRoute::DivergenceProbe;
	}
	return kOGLogRouteUnmapped;
}

constexpr ELogVerbosity::Type ogLogVerbosityOf(HostLogLevel level)
{
	switch (level)
	{
	case HostLogLevel::Verbose: return ELogVerbosity::Verbose;
	case HostLogLevel::Log:     return ELogVerbosity::Log;
	case HostLogLevel::Warning: return ELogVerbosity::Warning;
	}
	return ELogVerbosity::NoLogging;
}

constexpr std::size_t ogSimulationLogRoutePrefixLength(const char* prefix)
{
	std::size_t length = 0;
	while (prefix[length] != 0)
		++length;
	return length;
}

constexpr std::size_t ogLongestSimulationLogRoutePrefix()
{
	std::size_t longest = 0;
	for (const SimulationLogRouteEntry& entry : kSimulationLogRoutes)
	{
		if (ogSimulationLogRoutePrefixLength(entry.prefix) > longest)
			longest = ogSimulationLogRoutePrefixLength(entry.prefix);
	}
	return longest;
}

constexpr bool ogSimulationLogRoutePrefixesArePrintableAscii()
{
	for (const SimulationLogRouteEntry& entry : kSimulationLogRoutes)
	{
		for (const char* c = entry.prefix; *c != 0; ++c)
		{
			if (*c < 0x20 || *c > 0x7E)
				return false;
		}
	}
	return true;
}

static_assert(ogSimulationLogRoutePrefixesArePrintableAscii(),
	"kSimulationLogRoutes: a prefix holds a character outside printable ASCII. Each prefix is widened "
	"to TCHAR one char at a time, which is exact for ASCII only.");

struct OGSimulationLogRouteWidePrefix
{
	TCHAR text[ogLongestSimulationLogRoutePrefix() + 1];
};

constexpr std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> ogWidenSimulationLogRoutePrefixes()
{
	std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> wide{};
	for (std::size_t row = 0; row < std::size(kSimulationLogRoutes); ++row)
	{
		const char* prefix = kSimulationLogRoutes[row].prefix;
		for (std::size_t i = 0; prefix[i] != 0; ++i)
			wide[row].text[i] = static_cast<TCHAR>(prefix[i]);
	}
	return wide;
}

inline constexpr std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> kOGSimulationLogRoutePrefixes =
	ogWidenSimulationLogRoutePrefixes();

inline constexpr std::size_t kOGSimulationLogRouteCount = std::size(kSimulationLogRoutes);

constexpr std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGAdapterLogRoutes)> ogConcatenateLogRoutes()
{
	std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGAdapterLogRoutes)> routes{};
	for (std::size_t row = 0; row < kOGSimulationLogRouteCount; ++row)
	{
		routes[row] = { kOGSimulationLogRoutePrefixes[row].text,
			ogLogRouteOf(kSimulationLogRoutes[row].route),
			ogLogVerbosityOf(kSimulationLogRoutes[row].plainLevel) };
	}
	for (std::size_t row = 0; row < std::size(kOGAdapterLogRoutes); ++row)
		routes[kOGSimulationLogRouteCount + row] = kOGAdapterLogRoutes[row];
	return routes;
}

inline constexpr std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGAdapterLogRoutes)> kOGLogRoutes =
	ogConcatenateLogRoutes();

constexpr bool ogLogRoutesAreAllMapped()
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (entry.route == kOGLogRouteUnmapped)
			return false;
	}
	return true;
}

static_assert(ogLogRoutesAreAllMapped(),
	"kOGLogRoutes: an og-simulation row's SimulationLogRoute has no OGLogRoute in ogLogRouteOf, so "
	"RouteOGMessage would drop its lines. Map the new route to a LogOG* category.");

constexpr TCHAR ogLogRouteFoldCase(TCHAR c)
{
	return (c >= TEXT('A') && c <= TEXT('Z')) ? static_cast<TCHAR>(c - TEXT('A') + TEXT('a')) : c;
}

constexpr bool ogLogRouteStartsWith(const TCHAR* text, const TCHAR* prefix)
{
	for (; *prefix != 0; ++text, ++prefix)
	{
		if (*text == 0 || ogLogRouteFoldCase(*text) != ogLogRouteFoldCase(*prefix))
			return false;
	}
	return true;
}

constexpr bool ogLogRouteBlocksAreDisjoint()
{
	for (std::size_t simRow = 0; simRow < kOGSimulationLogRouteCount; ++simRow)
	{
		for (std::size_t gameRow = kOGSimulationLogRouteCount; gameRow < std::size(kOGLogRoutes); ++gameRow)
		{
			if (ogLogRouteStartsWith(kOGLogRoutes[simRow].prefix, kOGLogRoutes[gameRow].prefix)
				|| ogLogRouteStartsWith(kOGLogRoutes[gameRow].prefix, kOGLogRoutes[simRow].prefix))
				return false;
		}
	}
	return true;
}

static_assert(ogLogRouteBlocksAreDisjoint(),
	"kOGLogRoutes: a prefix in the og-simulation block and a prefix in this module's block BEGIN WITH "
	"one another (compared ignoring case, as FString::StartsWith compares), so one line can match a "
	"row in each block and the block order decides its route. The og-simulation block comes first only "
	"because no line can match both; give the new row a tag no prefix in the other block begins with.");

constexpr bool ogLogRoutesAreAllReachable()
{
	for (std::size_t later = 0; later < std::size(kOGLogRoutes); ++later)
	{
		for (std::size_t earlier = 0; earlier < later; ++earlier)
		{
			if (ogLogRouteStartsWith(kOGLogRoutes[later].prefix, kOGLogRoutes[earlier].prefix))
				return false;
		}
	}
	return true;
}
static_assert(ogLogRoutesAreAllReachable(),
	"RouteOGMessage: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier "
	"route always wins and the later one can never be taken. FString::StartsWith ignores "
	"case and so does this check. Was the ORDER IS LOAD-BEARING fence: [Resim.Input] must be "
	"matched ahead of the [Resim. catch-all, and widening that catch-all to [Resim would "
	"swallow every [ResimCheck. line.");

constexpr bool ogLogRoutePlainLevelsAreLogOrVerbose()
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (entry.plainLevel != ELogVerbosity::Log && entry.plainLevel != ELogVerbosity::Verbose)
			return false;
	}
	return true;
}

static_assert(ogLogRoutePlainLevelsAreLogOrVerbose(),
	"kOGLogRoutes: a row's plainLevel is the level of a line that carries no [Verbose] or "
	"[Warning] token, and it may only be Log (rare events) or Verbose (per tick, per packet, "
	"per relay). A line that must reach Warning says so with its own [Warning] token.");

constexpr const OGLogRouteEntry* ogLogRouteFor(const TCHAR* line)
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (ogLogRouteStartsWith(line, entry.prefix))
			return &entry;
	}
	return nullptr;
}

#define OG_LOG_ROUTED(Category, Format, ...) \
	do \
	{ \
		constexpr const OGLogRouteEntry* ogLogRouteEntryForSite = ogLogRouteFor(Format); \
		static_assert(ogLogRouteEntryForSite != nullptr, \
			"OG_LOG_ROUTED: the format's leading tag has no kOGLogRoutes row, so it has no level. " \
			"Add a row (prefix, route, plainLevel) or log this line with UE_LOG at a fixed level."); \
		if constexpr (ogLogRouteEntryForSite->plainLevel == ELogVerbosity::Verbose) \
		{ \
			UE_LOG(Category, Verbose, Format, ##__VA_ARGS__); \
		} \
		else \
		{ \
			UE_LOG(Category, Log, Format, ##__VA_ARGS__); \
		} \
	} while (0)

inline void RouteOGMessage(const char* msg)
{
	FString fmsg(msg);

	ELogVerbosity::Type severity = ELogVerbosity::Log;
	FString body = fmsg;
	if (fmsg.StartsWith(TEXT("[Verbose]")))
	{
		severity = ELogVerbosity::Verbose;
		body = fmsg.RightChop(9);
	}
	else if (fmsg.StartsWith(TEXT("[Warning]")))
	{
		severity = ELogVerbosity::Warning;
		body = fmsg.RightChop(9);
	}

#define EMIT_OG(cat) \
	do { \
		switch (severity) { \
			case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *fmsg); break; \
			case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *fmsg); break; \
			default:                     UE_LOG(cat, Log,     TEXT("%s"), *fmsg); break; \
		} \
	} while (0)

	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (!body.StartsWith(entry.prefix))
			continue;

		if (severity == ELogVerbosity::Log)
			severity = entry.plainLevel;

		switch (entry.route)
		{
		case OGLogRoute::SimTick:          EMIT_OG(LogOGSimTick); break;
		case OGLogRoute::ResimProbe:       EMIT_OG(LogOGResimProbe); break;
		case OGLogRoute::Sim:              EMIT_OG(LogOGSim); break;
		case OGLogRoute::Net:              EMIT_OG(LogOGNet); break;
		case OGLogRoute::RelayProbe:       EMIT_OG(LogOGRelayProbe); break;
		case OGLogRoute::DivergenceProbe:  EMIT_OG(LogOGDivergenceProbe); break;
		case OGLogRoute::Mgmt:             EMIT_OG(LogOGMgmt); break;
		}
		return;
	}

	EMIT_OG(LogOG);

#undef EMIT_OG
}
