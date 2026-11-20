// SPDX-License-Identifier: MPL-2.0
// docs/UEAuthorityRunner-rationale.md

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Templates/Function.h"

#include <atomic>

class FEvent;
class FRunnableThread;

OGSIMULATIONUNREAL_API int32 readAuthorityLoopKnob();
OGSIMULATIONUNREAL_API double readAuthorityIdleSlackSeconds();

struct OGSIMULATIONUNREAL_API FUEAuthorityRunnerHost
{
	void sleepUntil(double deadlineSeconds);
};

class OGSIMULATIONUNREAL_API FUEAuthorityRunner final : public FRunnable
{
public:
	using FBody = TUniqueFunction<void(FUEAuthorityRunner& runner)>;

	FUEAuthorityRunner(FBody body, bool highResolutionWait);
	virtual ~FUEAuthorityRunner() override;

	FUEAuthorityRunner(const FUEAuthorityRunner&) = delete;
	FUEAuthorityRunner& operator=(const FUEAuthorityRunner&) = delete;

	void start_GameThread();
	void waitForStart_GameThread();
	void releaseStart();
	void join_GameThread();

	const std::atomic<bool>& stopRequested() const { return m_stopRequested; }
	FUEAuthorityRunnerHost& host() { return m_host; }
	bool joined() const { return m_joined; }

	virtual uint32 Run() override;

private:
	void logStartLine() const;

	FBody                  m_body;
	bool                   m_highResolutionWait;
	FUEAuthorityRunnerHost m_host;
	std::atomic<bool>      m_stopRequested{false};
	std::atomic<bool>      m_startReleased{false};
	FEvent*                m_startLatch = nullptr;
	FRunnableThread*       m_thread = nullptr;
	bool                   m_joined = false;
};
