// SPDX-License-Identifier: MPL-2.0
// docs/UETickDriverHost-rationale.md · docs/UETickDriverHost-guards.md

#pragma once

#include "OGSimulationUnreal.h"
#include "Engine/EngineBaseTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "OGTickDriver/TickDriverConcepts.h"
#include "OGTickDriver/TickDriverLog.h"

#include <cstdint>
#include <utility>

class ULevel;
class UObject;

#include "OGSimulationUnreal/UESessionEngine.h"

struct OGSIMULATIONUNREAL_API FSimulationStartStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	void* target = nullptr;
	void (*startFrame)(void* target, double frameDeltaSeconds) = nullptr;
};

struct OGSIMULATIONUNREAL_API FSimulationEndStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	void* target = nullptr;
	void (*endFrame)(void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode) = nullptr;
};

class OGSIMULATIONUNREAL_API UETickDriverHost
{
public:
	using StartFrameFn = void (*)(void* target, double frameDeltaSeconds);
	using EndFrameFn   = void (*)(void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode);

	UETickDriverHost() = default;
	UETickDriverHost(const UETickDriverHost&) = delete;
	UETickDriverHost& operator=(const UETickDriverHost&) = delete;

	static int32 requestedMaxCatchUpSteps();
	static int32 retiredRunInlineValue();

	SimulationEngineUImpl& engine() { return m_engine; }

	template <typename DriverT>
	void begin(UObject& owner, ULevel& level, DriverT& driver)
	{
		registerTickFunctions(owner, level, &driver,
			[](void* target, double frameDeltaSeconds) { static_cast<DriverT*>(target)->startFrame(frameDeltaSeconds); },
			[](void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode)
			{
				static_cast<DriverT*>(target)->endFrame(completion, blockMode);
			});
	}

	void unregisterTickFunctions();

private:
	void registerTickFunctions(UObject& owner, ULevel& level, void* target, StartFrameFn startFrame, EndFrameFn endFrame);

	SimulationEngineUImpl                  m_engine;
	UObject*                               m_owner = nullptr;
	FSimulationStartStepsTickFunctionUImpl m_startTick;
	FSimulationEndStepsTickFunctionUImpl   m_endTick;
};
