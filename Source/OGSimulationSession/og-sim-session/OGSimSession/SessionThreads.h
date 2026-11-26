#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionThreads-rationale.md

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>

#include "OGSimulation/OGAssert.h"
#include "OGTickDriver/TickDriverThreadContext.h"
#include "OGSimSession/SessionTypes.h"

namespace ogsession
{
enum class SessionThread : uint8_t
{
	Frame,
	Step,
	NetIo,
};

enum class SessionOp : uint8_t
{
	SampleInput,
	RateDoor,
	TryRegister,
	RequestUnregister,
	PollOutcomes,
	AcquireDisplay,
	Present,
	DrainInbox,
	ApplyLifecycleRequests,
	Capture,
	Step,
	Send,
	Publish,
	ServiceNet,
	LinkFrameService,
	DrainStopped,
	FrameProbe,
	Count,
};

inline constexpr std::size_t kSessionOpCount = static_cast<std::size_t>(SessionOp::Count);

struct SessionOpThread
{
	SessionOp                    op;
	std::optional<SessionThread> thread;
};

using SessionThreadTable = std::array<SessionOpThread, kSessionOpCount>;

inline constexpr SessionThreadTable kPredictionThreads{ {
	{ SessionOp::SampleInput,            SessionThread::Frame },
	{ SessionOp::RateDoor,               SessionThread::Frame },
	{ SessionOp::TryRegister,            SessionThread::Frame },
	{ SessionOp::RequestUnregister,      SessionThread::Frame },
	{ SessionOp::PollOutcomes,           SessionThread::Frame },
	{ SessionOp::AcquireDisplay,         SessionThread::Frame },
	{ SessionOp::Present,                SessionThread::Frame },
	{ SessionOp::DrainInbox,             SessionThread::Step },
	{ SessionOp::ApplyLifecycleRequests, SessionThread::Step },
	{ SessionOp::Capture,                SessionThread::Step },
	{ SessionOp::Step,                   SessionThread::Step },
	{ SessionOp::Send,                   SessionThread::Step },
	{ SessionOp::Publish,                SessionThread::Step },
	{ SessionOp::ServiceNet,             SessionThread::NetIo },
	{ SessionOp::LinkFrameService,       SessionThread::Frame },
	{ SessionOp::DrainStopped,           SessionThread::Frame },
	{ SessionOp::FrameProbe,             SessionThread::Frame },
} };

inline constexpr SessionThreadTable kAuthorityThreads{ {
	{ SessionOp::SampleInput,            std::nullopt },
	{ SessionOp::RateDoor,               SessionThread::Step },
	{ SessionOp::TryRegister,            SessionThread::Frame },
	{ SessionOp::RequestUnregister,      SessionThread::Frame },
	{ SessionOp::PollOutcomes,           SessionThread::Frame },
	{ SessionOp::AcquireDisplay,         SessionThread::Frame },
	{ SessionOp::Present,                SessionThread::Frame },
	{ SessionOp::DrainInbox,             SessionThread::Step },
	{ SessionOp::ApplyLifecycleRequests, SessionThread::Step },
	{ SessionOp::Capture,                SessionThread::Step },
	{ SessionOp::Step,                   SessionThread::Step },
	{ SessionOp::Send,                   SessionThread::Step },
	{ SessionOp::Publish,                SessionThread::Step },
	{ SessionOp::ServiceNet,             SessionThread::NetIo },
	{ SessionOp::LinkFrameService,       SessionThread::Frame },
	{ SessionOp::DrainStopped,           std::nullopt },
	{ SessionOp::FrameProbe,             SessionThread::Step },
} };

namespace sessionThreadTable
{
consteval bool isIndexedByOp(const SessionThreadTable& table)
{
	for (std::size_t index = 0u; index < table.size(); ++index)
	{
		if (static_cast<std::size_t>(table[index].op) != index)
			return false;
	}
	return true;
}

consteval bool stepWorkIsOffTheFrame(const SessionThreadTable& table)
{
	for (std::size_t index = static_cast<std::size_t>(SessionOp::DrainInbox);
		 index <= static_cast<std::size_t>(SessionOp::ServiceNet); ++index)
	{
		if (table[index].thread == SessionThread::Frame)
			return false;
	}
	return true;
}
} // namespace sessionThreadTable

static_assert(sessionThreadTable::isIndexedByOp(kPredictionThreads) && sessionThreadTable::isIndexedByOp(kAuthorityThreads),
	"kPredictionThreads, kAuthorityThreads: one row per SessionOp, in the enum's order, so a row's position is its op");
static_assert(sessionThreadTable::stepWorkIsOffTheFrame(kPredictionThreads)
		&& sessionThreadTable::stepWorkIsOffTheFrame(kAuthorityThreads),
	"kPredictionThreads, kAuthorityThreads: draining the inbox, applying the lifecycle requests, capture, the step, the send, "
	"the publish and the og-net service never run on the frame thread, on either role");
static_assert(!kAuthorityThreads[static_cast<std::size_t>(SessionOp::SampleInput)].thread.has_value(),
	"kAuthorityThreads: the authority samples no input; its characters' inputs arrive over the network");
static_assert(kPredictionThreads[static_cast<std::size_t>(SessionOp::DrainStopped)].thread == SessionThread::Frame
		&& !kAuthorityThreads[static_cast<std::size_t>(SessionOp::DrainStopped)].thread.has_value(),
	"kPredictionThreads, kAuthorityThreads: the stopped drain is a client's, once, on the frame thread at its teardown, after "
	"its steps have stopped; an authority has no such drain (og-session-runtime task 24)");
static_assert(kPredictionThreads[static_cast<std::size_t>(SessionOp::FrameProbe)].thread == SessionThread::Frame
		&& kAuthorityThreads[static_cast<std::size_t>(SessionOp::FrameProbe)].thread == SessionThread::Step,
	"kPredictionThreads, kAuthorityThreads: the frame probe runs where the pump does, after it: a client's frame thread "
	"(startFrame), an authority's runner (each loop wake); it is not the step's work (og-session-runtime task 26)");

class SessionThreadChecks
{
public:
	SessionThreadChecks(SessionRole role, const TickDriverThreadContext* context, std::thread::id frameThread)
		: m_role(role)
		, m_context(context)
		, m_frameThread(frameThread)
	{
	}

	static const SessionThreadTable& tableFor(SessionRole role)
	{
		return role == SessionRole::Authority ? kAuthorityThreads : kPredictionThreads;
	}

	std::optional<SessionThread> threadOf(SessionOp op) const
	{
		const std::size_t index = static_cast<std::size_t>(op);
		if (index >= kSessionOpCount)
			return std::nullopt;
		return tableFor(m_role)[index].thread;
	}

	bool isOn(SessionThread thread) const
	{
		const bool onFrame = std::this_thread::get_id() == m_frameThread;
		if (thread == SessionThread::Frame)
			return onFrame;
		if (thread == SessionThread::NetIo && m_role == SessionRole::Prediction && m_netIoThread != std::thread::id{})
			return std::this_thread::get_id() == m_netIoThread;
		if (m_context == nullptr || m_context->roleBit() != roleBit())
			return false;
		if (m_role == SessionRole::Authority)
			return m_context->isOwnerThreadForRole();
		const bool inStep = m_context->isStepRunningOnThisThread();
		return thread == SessionThread::Step ? inStep : (!onFrame && !inStep);
	}

	bool isAllowed(SessionOp op) const
	{
		const std::optional<SessionThread> thread = threadOf(op);
		return thread.has_value() && isOn(*thread);
	}

	void checkOp(SessionOp op) const
	{
		OG_CHECK(isAllowed(op),
			"SessionThreadChecks::checkOp: this operation ran on a thread its role's row of the session thread table does not "
			"name (kPredictionThreads, kAuthorityThreads)");
	}

	SessionRole role() const { return m_role; }

	void setNetIoThread(std::thread::id netIoThread) { m_netIoThread = netIoThread; }

private:
	uint8_t roleBit() const
	{
		return m_role == SessionRole::Authority ? tickDriverThread::kAuthorityRoleBit : tickDriverThread::kClientRoleBit;
	}

	SessionRole                    m_role;
	const TickDriverThreadContext* m_context;
	std::thread::id                m_frameThread;
	std::thread::id                m_netIoThread{};
};

class StoppedDrainScope
{
public:
	StoppedDrainScope(std::atomic<bool>& inStoppedDrain, const SessionThreadChecks& threads)
		: m_inStoppedDrain(inStoppedDrain)
		, m_threads(threads)
	{
		m_threads.checkOp(SessionOp::DrainStopped);
		m_inStoppedDrain.store(true, std::memory_order_relaxed);
	}

	~StoppedDrainScope()
	{
		m_threads.checkOp(SessionOp::DrainStopped);
		m_inStoppedDrain.store(false, std::memory_order_relaxed);
	}

	StoppedDrainScope(const StoppedDrainScope&) = delete;
	StoppedDrainScope& operator=(const StoppedDrainScope&) = delete;

private:
	std::atomic<bool>&         m_inStoppedDrain;
	const SessionThreadChecks& m_threads;
};
} // namespace ogsession
