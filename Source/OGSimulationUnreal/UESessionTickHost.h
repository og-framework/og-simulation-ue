// SPDX-License-Identifier: MPL-2.0
// docs/UESessionTickHost-rationale.md

#pragma once

#include "OGSimSession/SessionThreads.h"
#include "OGSimSession/SessionTickGame.h"

#include <cstdint>

template <typename TraitsT> class UESimulationSession;
template <typename TraitsT> class UELatencyBudgetDiagnostics;

template <typename SessionT>
struct UESessionTraitsOf;

template <typename TraitsT>
struct UESessionTraitsOf<UESimulationSession<TraitsT>>
{
	using Type = TraitsT;
};

template <typename BindingT>
struct UESessionTickHost
{
	using Session        = typename BindingT::Session;
	using Manager        = typename BindingT::Manager;
	using Storage        = typename BindingT::Storage;
	using RenderSnapshot = typename BindingT::RenderSnapshot;
	using Diagnostics    = UELatencyBudgetDiagnostics<typename UESessionTraitsOf<Session>::Type>;

	Session* session = nullptr;

	Manager&                              manager();
	Storage&                              storage();
	const ogsession::SessionThreadChecks& threads() const;
	auto*                                 clock();
	auto*                                 display();
	auto*                                 netHooks();
	auto*                                 clockServer();
	Diagnostics&                          diagnostics();
	void                                  sampleInput();
	void                                  onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps);
	void                                  onPostPhysics();
};

template <typename BindingT>
using UESessionTickGame = ogsession::SessionTickGame<UESessionTickHost<BindingT>, typename UESessionTickHost<BindingT>::Diagnostics>;
