// SPDX-License-Identifier: MPL-2.0
// docs/UELatencyBudgetDiagnostics-rationale.md · docs/UELatencyBudgetDiagnostics-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/LatencyBudgetProbe.h"

#include <functional>
#include <type_traits>

#ifndef OG_LATENCY_BUDGET_DIAGNOSTICS
#define OG_LATENCY_BUDGET_DIAGNOSTICS (!UE_BUILD_SHIPPING)
#endif

#if OG_LATENCY_BUDGET_DIAGNOSTICS
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "Engine/World.h"

#include <array>
#include <memory>
#include <unordered_map>
#include <utility>
#endif

class UWorld;

template <typename TraitsT> class UESimulationSession;

#if !UE_BUILD_SHIPPING
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGLatencyBudget, Log, All);
#endif

template <typename TraitsT>
class UELatencyBudgetDiagnostics
{
public:
	using InputProvider = std::function<typename TraitsT::PlayerInput(
		const SimulationTimeStep&,
		const LocalInputCache<typename TraitsT::PlayerInput>&)>;

	struct ClockCounts
	{
		uint32 skips       = 0u;
		uint32 stalls      = 0u;
		uint32 hardResyncs = 0u;
		bool operator==(const ClockCounts&) const = default;
	};

	UELatencyBudgetDiagnostics(const UELatencyBudgetDiagnostics&) = delete;
	UELatencyBudgetDiagnostics& operator=(const UELatencyBudgetDiagnostics&) = delete;

	InputProvider wrapInputProvider(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider)
	{
		[[maybe_unused]] const bool present = static_cast<bool>(inputProvider);
		InputProvider wrapped = wrap(id, owner, std::move(inputProvider));
		checkf(static_cast<bool>(wrapped) == present,
			TEXT("UELatencyBudgetDiagnostics::wrapInputProvider: a present provider must stay present and an empty ")
			TEXT("one empty. Provider presence is the core's local-vs-remote identity test, so the latency wrapper may ")
			TEXT("never turn an empty provider into a callable or a callable into an empty one, in either state of ")
			TEXT("OG_LATENCY_BUDGET_DIAGNOSTICS. Was the checkf of wrapInputProviderWithLatencyStamp."));
		return wrapped;
	}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	explicit UELatencyBudgetDiagnostics(const UESimulationSession<TraitsT>& session) : m_owner(session) {}

	void begin(UWorld& world, bool isAuthority);
	void end(UWorld& world);
	void forgetCharacter(unsigned int id);

	void noteWireSample(double roundTripSeconds);
	void post_Internal(const latencyBudget::Event& event);
	void stampClientReceived_Internal(latencyBudget::Stream stream, unsigned int lane, uint32 tick, double recvSeconds);
	void noteWireSample_Internal(double roundTripSeconds);
	void stampHandedAndLeft_Internal(unsigned int lane, uint32 captureTick);
	void stampAfterStep_Internal(double stepStartSeconds);
	void stampStepEnd_Internal();
	void stampServerReceived_Internal(unsigned int lane, uint32 captureTick, double recvSeconds);
	void stampCorrectionHanded_Internal(unsigned int lane, uint32 correctionTick, double handedSeconds);
	void onPostTickFlush();

	void tick_GameThread();

	ClockCounts clockCountsInWindow() const;
	void restartClockWindow();

	template <typename FunctionT>
	static void withInstance(const UWorld* world, bool isAuthority, FunctionT&& function)
	{
		if (UELatencyBudgetDiagnostics* diagnostics = instanceFor(world, isAuthority))
			function(*diagnostics);
	}

private:
	struct PostedEvent
	{
		latencyBudget::Event event;
		uint32               healDelayTicks   = 0u;
		bool                 healByCorrection = false;
	};

	static_assert(std::is_trivially_copyable_v<PostedEvent>,
		"UELatencyBudgetDiagnostics::PostedEvent crosses from the step thread to the game thread by value through "
		"SpscMailbox, like the latencyBudget::Event it carries; it must own no memory");

	using Mailbox      = latencyBudget::SpscMailbox<PostedEvent, 4096>;
	using ClockMailbox = latencyBudget::SpscMailbox<ClockCounts, 64>;

	struct SeenRead
	{
		uint32                      simTick               = 0u;
		uint32                      appliedCaptureTick    = 0u;
		bool                        hasAppliedCaptureTick = false;
		ScheduledRelayedReadOutcome outcome               = ScheduledRelayedReadOutcome::NoProbe;
		bool operator==(const SeenRead&) const = default;
	};

	struct SeenReads
	{
		std::array<SeenRead, kRelayedReadObservationCapacityTicks> slots{};
		bool                                                       primed = false;
	};

	static UELatencyBudgetDiagnostics* instanceFor(const UWorld* world, bool isAuthority);

	InputProvider wrap(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider);
	void postRelayedReads_Internal(unsigned int id, const RelayedReadObservationRing& ring, uint32 tick,
		double stepStartSeconds);

	const UESimulationSession<TraitsT>& m_owner;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_owner)>>,
		"UELatencyBudgetDiagnostics reads its session's production state (the runtime's manager, storage, input resolution "
		"and the client clock) through a CONST reference only, through the runtime's public const accessors (the session "
		"declares no friend since og-session-runtime task 31). Added by og-tickdriver task 18; moved to OGSimulationUnreal by "
		"task 19a; its owner became the session in og-ue-simulationcomponentandmanagersplit task 24a, the delayed-input "
		"map the session's own in task 26a, and its loop the session's in og-input-latency task P5-5.");

	// ⛔G-01  docs/UELatencyBudgetDiagnostics-guards.md
	std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;
	std::unique_ptr<Mailbox>                           m_mailbox;
	std::unique_ptr<ClockMailbox>                      m_clockMailbox;
	ClockCounts                                        m_clockPosted_Physics;
	ClockCounts                                        m_clockLatest;
	ClockCounts                                        m_clockAtWindowStart;
	uint64                                             m_mailboxDropped = 0u;
	std::unordered_map<unsigned int, uint32>           m_lastHandedCorrectionTick_Runner;
	std::unordered_map<unsigned int, SeenReads>        m_seenReads_Physics;
	FDelegateHandle                                    m_postTickFlushHandle;
#else
	explicit UELatencyBudgetDiagnostics(const UESimulationSession<TraitsT>&) {}

	void begin(UWorld&, bool) {}
	void end(UWorld&) {}
	void forgetCharacter(unsigned int) {}

	void noteWireSample(double) {}
	void post_Internal(const latencyBudget::Event&) {}
	void stampClientReceived_Internal(latencyBudget::Stream, unsigned int, uint32, double) {}
	void noteWireSample_Internal(double) {}
	void stampHandedAndLeft_Internal(unsigned int, uint32) {}
	void stampAfterStep_Internal(double) {}
	void stampStepEnd_Internal() {}
	void stampServerReceived_Internal(unsigned int, uint32, double) {}
	void stampCorrectionHanded_Internal(unsigned int, uint32, double) {}
	void onPostTickFlush() {}

	void tick_GameThread() {}

	ClockCounts clockCountsInWindow() const { return {}; }
	void restartClockWindow() {}

	template <typename FunctionT>
	static void withInstance(const UWorld*, bool, FunctionT&&) {}

private:
	InputProvider wrap(unsigned int, const typename TraitsT::Component&, InputProvider inputProvider)
	{
		return inputProvider;
	}
#endif
};

#if OG_LATENCY_BUDGET_DIAGNOSTICS

OGSIM_OPTIMIZE_OFF

template <typename TraitsT>
UELatencyBudgetDiagnostics<TraitsT>* UELatencyBudgetDiagnostics<TraitsT>::instanceFor(const UWorld* world, bool isAuthority)
{
	typename TraitsT::SessionOwner* manager = TraitsT::SessionOwner::instanceFor(world, isAuthority);
	return manager != nullptr ? &manager->editLatencyBudgetDiagnostics() : nullptr;
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::begin(UWorld& world, bool isAuthority)
{
	m_probe = std::make_unique<latencyBudget::LatencyBudgetProbe>(isAuthority
		? (latencyBudget::kServerHops | latencyBudget::kServerNetHops)
		: (latencyBudget::kClientHops | latencyBudget::kClientNetHops));
	m_mailbox      = std::make_unique<Mailbox>();
	m_clockMailbox = std::make_unique<ClockMailbox>();
	m_postTickFlushHandle =
		world.OnPostTickFlush().AddWeakLambda(&m_owner.owner(), [this]() { onPostTickFlush(); });
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::end(UWorld& world)
{
	if (m_postTickFlushHandle.IsValid())
	{
		world.OnPostTickFlush().Remove(m_postTickFlushHandle);
		m_postTickFlushHandle.Reset();
	}
}

#include "OGSimSession/SessionLatencyDiagnostics.h"

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::onPostTickFlush()
{
	if (!m_probe)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_probe->stampPending(latencyBudget::Stream::Input,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
	m_probe->stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::tick_GameThread()
{
	if (!m_probe || !m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	m_mailbox->drain([this](const PostedEvent& posted)
	{
		if (posted.healByCorrection)
			m_probe->stampHealedByCorrection(posted.event.lane, posted.event.tick, posted.healDelayTicks, posted.event.seconds);
		else
			m_probe->apply(posted.event);
	});
	m_mailboxDropped += m_mailbox->takeDroppedCount();
	if (m_clockMailbox)
	{
		m_clockMailbox->drain(
			[this](const ClockCounts& counts) { m_clockLatest = counts; });
		m_mailboxDropped += m_clockMailbox->takeDroppedCount();
	}

	const double nowSeconds = FPlatformTime::Seconds();
	const bool isClient = m_owner.runtime().getManager()->runsPrediction();

	m_probe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

	latencyBudget::WindowReport report;
	if (!m_probe->closeWindowIfDue(nowSeconds, report))
		return;

	const TCHAR* role = isClient ? TEXT("Client") : TEXT("Server");
	constexpr double kMillis = 1000.0;
	for (std::size_t index = 0; index < report.hopCount; ++index)
	{
		const latencyBudget::HopSummary& hop = report.hops[index];
		if (hop.hop == latencyBudget::Hop::H6L)
		{
			UE_LOG(LogOGLatencyBudget, Warning,
				TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
				TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u healedByUse=%u healedByCorrection=%u"),
				role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
				hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
				hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow, hop.healedByUse, hop.healedByCorrection);
			continue;
		}
		UE_LOG(LogOGLatencyBudget, Warning,
			TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
			TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u"),
			role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
			hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
			hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow);
	}
	UE_LOG(LogOGLatencyBudget, Warning,
		TEXT("[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u ")
		TEXT("mailboxDropped=%llu"),
		role, report.windowEndSeconds - report.windowStartSeconds, report.staleStamps,
		report.duplicateStamps, report.laneOverflow,
		static_cast<unsigned long long>(m_mailboxDropped));
	m_mailboxDropped = 0u;
}

template <typename TraitsT>
typename UELatencyBudgetDiagnostics<TraitsT>::ClockCounts UELatencyBudgetDiagnostics<TraitsT>::clockCountsInWindow() const
{
	ClockCounts window;
	window.skips       = m_clockLatest.skips - m_clockAtWindowStart.skips;
	window.stalls      = m_clockLatest.stalls - m_clockAtWindowStart.stalls;
	window.hardResyncs = m_clockLatest.hardResyncs - m_clockAtWindowStart.hardResyncs;
	return window;
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::restartClockWindow()
{
	m_clockAtWindowStart = m_clockLatest;
}

OGSIM_OPTIMIZE_ON

#endif
