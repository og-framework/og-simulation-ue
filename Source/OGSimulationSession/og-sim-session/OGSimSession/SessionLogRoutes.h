#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UESimulationLogRoutes-rationale.md · ../../../OGSimulationUnreal/docs/UESimulationLogRoutes-guards.md

	{ TEXT("[SimHost."),                         OGLogRoute::SimHost,         ELogVerbosity::Log },
	{ TEXT("[RelayDelayFloor]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[StateRotation]"),                   OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ResimGate]"),                       OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[LatencyKnobs]"),                    OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ClockRecentre.Config]"),            OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ClockSource.Config]"),              OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[Dilation.Config]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ConnectionTier]"),                  OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ClockSource.Window]"),              OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ClockSource.Held]"),                OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[Dilation.Window]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[Dilation.Authority]"),              OGLogRoute::Net,             ELogVerbosity::Log }
