#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../OGSimSession/docs/SessionLogRoutes-rationale.md

#include <cstddef>

namespace sessionLogRoutesGolden
{
struct Row
{
	std::size_t index;
	const char* prefix;
	const char* route;
	const char* level;
};

inline constexpr Row kRows[] = {
	{  0, "[Resim.Input]",                  "SimTick",         "Verbose" },
	{  1, "[TimeResync.",                   "Sim",             "Log" },
	{  2, "[Resim.",                        "Sim",             "Log" },
	{  3, "[ResimCheck.Divergence]",        "Sim",             "Log" },
	{  4, "[ResimCheck.PrepareRestore]",    "Sim",             "Log" },
	{  5, "[ResimCheck.Check]",             "SimTick",         "Verbose" },
	{  6, "[ResimCheck.IsSimilar]",         "SimTick",         "Verbose" },
	{  7, "[AuthoritySimulation]",          "SimTick",         "Verbose" },
	{  8, "[PredictionSimulation]",         "SimTick",         "Verbose" },
	{  9, "[PostPrediction]",               "SimTick",         "Verbose" },
	{ 10, "[CollectInput]",                 "SimTick",         "Verbose" },
	{ 11, "[ReceiveLocalInput] DISCARD",    "Net",             "Log" },
	{ 12, "[ReceiveLocalInput]",            "Net",             "Verbose" },
	{ 13, "[SendCorrectionStateToClients]", "Net",             "Verbose" },
	{ 14, "[SendLocalInputToServer]",       "Net",             "Verbose" },
	{ 15, "[InjectCorrectionState]",        "Net",             "Verbose" },
	{ 16, "[InputGap]",                     "Net",             "Log" },
	{ 17, "[InputDrop]",                    "Net",             "Log" },
	{ 18, "[DelayShift]",                   "Net",             "Log" },
	{ 19, "[InputStats]",                   "Net",             "Log" },
	{ 20, "[InputArrival.Window]",          "Net",             "Log" },
	{ 21, "[InputArrival.BackDate]",        "Net",             "Log" },
	{ 22, "[Park]",                         "Net",             "Verbose" },
	{ 23, "[Release]",                      "Net",             "Verbose" },
	{ 24, "[InputDomain]",                  "Net",             "Log" },
	{ 25, "[RelaySkip]",                    "Net",             "Log" },
	{ 26, "[RelayFlush]",                   "Net",             "Log" },
	{ 27, "[SimNet.",                       "Net",             "Log" },
	{ 28, "[Roster.",                       "Net",             "Log" },
	{ 29, "[DivergenceProbe",               "DivergenceProbe", "Log" },
	{ 30, "[ResimProbe",                    "ResimProbe",      "Log" },
	{ 31, "[ClientPrediction]",             "SimTick",         "Verbose" },
	{ 32, "[ServerReceive]",                "Net",             "Verbose" },
	{ 33, "[SendRemoteInputToClients]",     "Net",             "Log" },
	{ 34, "[ReceiveCorrectionState]",       "Net",             "Verbose" },
	{ 35, "[ReceiveCorrectionInput]",       "Net",             "Log" },
	{ 36, "[InjectCorrectionInput]",        "Net",             "Log" },
	{ 37, "[DrainOutOfOrder]",              "Net",             "Log" },
	{ 38, "[ReleaseBatch]",                 "Net",             "Verbose" },
	{ 39, "[RelayProbe",                    "RelayProbe",      "Log" },
	{ 40, "SimulationManager:",             "Mgmt",            "Log" },
	{ 41, "tryRegister:",                   "Mgmt",            "Log" },
	{ 42, "NewFramework:",                  "Mgmt",            "Log" },
	{ 43, "[SimHost.",                      "SimHost",         "Log" },
	{ 44, "[RelayDelayFloor]",              "Net",             "Log" },
	{ 45, "[StateRotation]",                "Net",             "Log" },
	{ 46, "[ResimGate]",                    "Net",             "Log" },
	{ 47, "[LatencyKnobs]",                 "Net",             "Log" },
	{ 48, "[ClockRecentre.Config]",         "Net",             "Log" },
	{ 49, "[ClockSource.Config]",           "Net",             "Log" },
	{ 50, "[Dilation.Config]",              "Net",             "Log" },
	{ 51, "[ConnectionTier]",               "Net",             "Log" },
	{ 52, "[ClockSource.Window]",           "Net",             "Log" },
	{ 53, "[ClockSource.Held]",             "Net",             "Log" },
	{ 54, "[Dilation.Window]",              "Net",             "Log" },
	{ 55, "[Dilation.Authority]",           "Net",             "Log" }
};
} // namespace sessionLogRoutesGolden
