// SPDX-License-Identifier: MPL-2.0
// docs/UESimulationLogRoutes-rationale.md · docs/UESimulationLogRoutes-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimSession/SessionLogRoutes.h"
#include "OGSimulation/HostLog.h"
#include "OGSimulation/SimulationLogRoutes.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"

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
	SimTick, ResimProbe, Sim, Net, RelayProbe, DivergenceProbe, Mgmt, SimHost
};

struct OGLogRouteEntry
{
	const TCHAR*        prefix;
	OGLogRoute          route;
	ELogVerbosity::Type plainLevel;
};

inline constexpr OGLogRouteEntry kOGAdapterLogRoutes[] = {
	{ TEXT("[ResimProbe"),                       OGLogRoute::ResimProbe,      ELogVerbosity::Log },
	{ TEXT("[ClientPrediction]"),                OGLogRoute::SimTick,         ELogVerbosity::Verbose },
	{ TEXT("[ServerReceive]"),                   OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[SendRemoteInputToClients]"),        OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ReceiveCorrectionInput]"),          OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[InjectCorrectionInput]"),           OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[DrainOutOfOrder]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[RelayProbe"),                       OGLogRoute::RelayProbe,      ELogVerbosity::Log },
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
	case SimulationLogRoute::Mgmt:            return OGLogRoute::Mgmt;
	case SimulationLogRoute::SimHost:         return OGLogRoute::SimHost;
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

inline constexpr std::size_t kOGEngineFreeLogRouteCount = std::size(ogsession::kComposedLogRoutes);

constexpr std::size_t ogEngineFreeLogRoutePrefixLength(const char* prefix)
{
	std::size_t length = 0;
	while (prefix[length] != 0)
		++length;
	return length;
}

constexpr std::size_t ogLongestEngineFreeLogRoutePrefix()
{
	std::size_t longest = 0;
	for (const SimulationLogRouteEntry& entry : ogsession::kComposedLogRoutes)
	{
		if (ogEngineFreeLogRoutePrefixLength(entry.prefix) > longest)
			longest = ogEngineFreeLogRoutePrefixLength(entry.prefix);
	}
	return longest;
}

constexpr bool ogEngineFreeLogRoutePrefixesArePrintableAscii()
{
	for (const SimulationLogRouteEntry& entry : ogsession::kComposedLogRoutes)
	{
		for (const char* c = entry.prefix; *c != 0; ++c)
		{
			if (*c < 0x20 || *c > 0x7E)
				return false;
		}
	}
	return true;
}

static_assert(ogEngineFreeLogRoutePrefixesArePrintableAscii(),
	"ogsession::kComposedLogRoutes: a prefix holds a character outside printable ASCII. Each prefix is widened to TCHAR one "
	"char at a time, which is exact for ASCII only.");

struct OGEngineFreeLogRouteWidePrefix
{
	TCHAR text[ogLongestEngineFreeLogRoutePrefix() + 1];
};

constexpr std::array<OGEngineFreeLogRouteWidePrefix, kOGEngineFreeLogRouteCount> ogWidenEngineFreeLogRoutePrefixes()
{
	std::array<OGEngineFreeLogRouteWidePrefix, kOGEngineFreeLogRouteCount> wide{};
	for (std::size_t row = 0; row < kOGEngineFreeLogRouteCount; ++row)
	{
		const char* prefix = ogsession::kComposedLogRoutes[row].prefix;
		for (std::size_t i = 0; prefix[i] != 0; ++i)
			wide[row].text[i] = static_cast<TCHAR>(prefix[i]);
	}
	return wide;
}

inline constexpr std::array<OGEngineFreeLogRouteWidePrefix, kOGEngineFreeLogRouteCount> kOGEngineFreeLogRoutePrefixes =
	ogWidenEngineFreeLogRoutePrefixes();

constexpr std::array<OGLogRouteEntry, kOGEngineFreeLogRouteCount + std::size(kOGAdapterLogRoutes)> ogConcatenateLogRoutes()
{
	std::array<OGLogRouteEntry, kOGEngineFreeLogRouteCount + std::size(kOGAdapterLogRoutes)> routes{};
	for (std::size_t row = 0; row < kOGEngineFreeLogRouteCount; ++row)
	{
		routes[row] = { kOGEngineFreeLogRoutePrefixes[row].text,
			ogLogRouteOf(ogsession::kComposedLogRoutes[row].route),
			ogLogVerbosityOf(ogsession::kComposedLogRoutes[row].plainLevel) };
	}
	for (std::size_t row = 0; row < std::size(kOGAdapterLogRoutes); ++row)
		routes[kOGEngineFreeLogRouteCount + row] = kOGAdapterLogRoutes[row];
	return routes;
}

inline constexpr std::array<OGLogRouteEntry, kOGEngineFreeLogRouteCount + std::size(kOGAdapterLogRoutes)> kOGLogRoutes =
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
	"kOGLogRoutes: an engine-free row's SimulationLogRoute has no OGLogRoute in ogLogRouteOf, so RouteOGLine would drop "
	"its lines. Map the new route to a LogOG* category.");

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
	for (std::size_t engineFreeRow = 0; engineFreeRow < kOGEngineFreeLogRouteCount; ++engineFreeRow)
	{
		for (std::size_t adapterRow = kOGEngineFreeLogRouteCount; adapterRow < std::size(kOGLogRoutes); ++adapterRow)
		{
			if (ogLogRouteStartsWith(kOGLogRoutes[engineFreeRow].prefix, kOGLogRoutes[adapterRow].prefix)
				|| ogLogRouteStartsWith(kOGLogRoutes[adapterRow].prefix, kOGLogRoutes[engineFreeRow].prefix))
				return false;
		}
	}
	return true;
}

static_assert(ogLogRouteBlocksAreDisjoint(),
	"kOGLogRoutes: a prefix in the engine-free block (ogsession::kComposedLogRoutes) and a prefix in this module's block "
	"BEGIN WITH one another (compared ignoring case, as FString::StartsWith compares), so one line can match a row in each "
	"block and the block order decides its route. The engine-free block comes first only because no line can match both; "
	"give the new row a tag no prefix in the other block begins with.");

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
	"RouteOGLine: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier "
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

#define OG_LOG_ACTIVE_AT(Category, Level) \
	((Level) == ELogVerbosity::Error     ? UE_LOG_ACTIVE(Category, Error) \
		: (Level) == ELogVerbosity::Warning ? UE_LOG_ACTIVE(Category, Warning) \
		: (Level) == ELogVerbosity::Verbose ? UE_LOG_ACTIVE(Category, Verbose) \
		: UE_LOG_ACTIVE(Category, Log))

inline bool ogLogWants(const OGLogRoute* route, ELogVerbosity::Type level)
{
	if (route == nullptr)
		return OG_LOG_ACTIVE_AT(LogOG, level);
	switch (*route)
	{
	case OGLogRoute::SimTick:          return OG_LOG_ACTIVE_AT(LogOGSimTick, level);
	case OGLogRoute::ResimProbe:       return OG_LOG_ACTIVE_AT(LogOGResimProbe, level);
	case OGLogRoute::Sim:              return OG_LOG_ACTIVE_AT(LogOGSim, level);
	case OGLogRoute::Net:              return OG_LOG_ACTIVE_AT(LogOGNet, level);
	case OGLogRoute::RelayProbe:       return OG_LOG_ACTIVE_AT(LogOGRelayProbe, level);
	case OGLogRoute::DivergenceProbe:  return OG_LOG_ACTIVE_AT(LogOGDivergenceProbe, level);
	case OGLogRoute::Mgmt:             return OG_LOG_ACTIVE_AT(LogOGMgmt, level);
	case OGLogRoute::SimHost:          return OG_LOG_ACTIVE_AT(LogOGSimHost, level);
	}
	return OG_LOG_ACTIVE_AT(LogOG, level);
}

#undef OG_LOG_ACTIVE_AT

inline bool ogLogWants(SimulationLogRoute route, HostLogLevel level)
{
	const OGLogRoute mapped = ogLogRouteOf(route);
	return ogLogWants(mapped == kOGLogRouteUnmapped ? nullptr : &mapped, ogLogVerbosityOf(level));
}

enum class OGLevelToken : uint8
{
	Printed,
	Read,
};

inline void RouteOGLine(const char* line, OGLevelToken token)
{
	const FString text = token == OGLevelToken::Read ? FString(UTF8_TO_TCHAR(line)) : FString(line);

	ELogVerbosity::Type severity = ELogVerbosity::Log;
	int32 tokenLength = 0;
	if (text.StartsWith(TEXT("[Verbose]")))
	{
		severity = ELogVerbosity::Verbose;
		tokenLength = 9;
	}
	else if (text.StartsWith(TEXT("[Warning]")))
	{
		severity = ELogVerbosity::Warning;
		tokenLength = 9;
	}
	else if (token == OGLevelToken::Read && text.StartsWith(TEXT("[Error]")))
	{
		severity = ELogVerbosity::Error;
		tokenLength = 7;
	}
	const FString body = text.RightChop(tokenLength);

	const OGLogRouteEntry* route = nullptr;
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (body.StartsWith(entry.prefix))
		{
			route = &entry;
			break;
		}
	}
	if (route != nullptr && severity == ELogVerbosity::Log)
		severity = route->plainLevel;

	const FString& printed = token == OGLevelToken::Printed ? text : body;

#define OG_EMIT_ROUTED_LINE(Category) \
	do \
	{ \
		switch (severity) \
		{ \
		case ELogVerbosity::Error:   UE_LOG(Category, Error,   TEXT("%s"), *printed); break; \
		case ELogVerbosity::Warning: UE_LOG(Category, Warning, TEXT("%s"), *printed); break; \
		case ELogVerbosity::Verbose: UE_LOG(Category, Verbose, TEXT("%s"), *printed); break; \
		default:                     UE_LOG(Category, Log,     TEXT("%s"), *printed); break; \
		} \
	} while (0)

	if (route == nullptr)
	{
		OG_EMIT_ROUTED_LINE(LogOG);
		return;
	}
	switch (route->route)
	{
	case OGLogRoute::SimTick:          OG_EMIT_ROUTED_LINE(LogOGSimTick); break;
	case OGLogRoute::ResimProbe:       OG_EMIT_ROUTED_LINE(LogOGResimProbe); break;
	case OGLogRoute::Sim:              OG_EMIT_ROUTED_LINE(LogOGSim); break;
	case OGLogRoute::Net:              OG_EMIT_ROUTED_LINE(LogOGNet); break;
	case OGLogRoute::RelayProbe:       OG_EMIT_ROUTED_LINE(LogOGRelayProbe); break;
	case OGLogRoute::DivergenceProbe:  OG_EMIT_ROUTED_LINE(LogOGDivergenceProbe); break;
	case OGLogRoute::Mgmt:             OG_EMIT_ROUTED_LINE(LogOGMgmt); break;
	case OGLogRoute::SimHost:          OG_EMIT_ROUTED_LINE(LogOGSimHost); break;
	}

#undef OG_EMIT_ROUTED_LINE
}

inline void RouteOGMessage(const char* msg)
{
	RouteOGLine(msg, OGLevelToken::Printed);
}

inline void RouteOGLevelLine(const char* line)
{
	RouteOGLine(line, OGLevelToken::Read);
}

constexpr bool ogLogRouteStartsWithNarrow(const char* text, const TCHAR* prefix)
{
	for (; *prefix != 0; ++text, ++prefix)
	{
		if (*text == 0
			|| ogLogRouteFoldCase(static_cast<TCHAR>(static_cast<unsigned char>(*text))) != ogLogRouteFoldCase(*prefix))
			return false;
	}
	return true;
}

constexpr const char* ogLevelLineBody(const char* line)
{
	if (ogLogRouteStartsWithNarrow(line, TEXT("[Verbose]")) || ogLogRouteStartsWithNarrow(line, TEXT("[Warning]")))
		return line + 9;
	if (ogLogRouteStartsWithNarrow(line, TEXT("[Error]")))
		return line + 7;
	return line;
}

constexpr const OGLogRouteEntry* ogLogRouteForLevelLine(const char* line)
{
	const char* body = ogLevelLineBody(line);
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (ogLogRouteStartsWithNarrow(body, entry.prefix))
			return &entry;
	}
	return nullptr;
}

constexpr ELogVerbosity::Type ogLogLevelOfLevelLine(const char* line)
{
	if (ogLogRouteStartsWithNarrow(line, TEXT("[Verbose]")))
		return ELogVerbosity::Verbose;
	if (ogLogRouteStartsWithNarrow(line, TEXT("[Warning]")))
		return ELogVerbosity::Warning;
	if (ogLogRouteStartsWithNarrow(line, TEXT("[Error]")))
		return ELogVerbosity::Error;
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr ? route->plainLevel : ELogVerbosity::Log;
}

struct UESessionLogSink
{
	void logRouted(const char* line) { RouteOGLevelLine(line); }
	bool wants(SimulationLogRoute route, HostLogLevel level) const { return ogLogWants(route, level); }
};
