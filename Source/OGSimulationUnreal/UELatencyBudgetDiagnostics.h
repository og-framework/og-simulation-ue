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

	void stamp(latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point);
	void stampAt(latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point, double seconds);
	void stampCorrectionReceived(unsigned int lane, uint32 correctionTick);
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

	template <typename BackendDiagnosticsT>
	void tick_GameThread(BackendDiagnosticsT backendDiagnostics);

	ClockCounts clockCountsInWindow() const;
	void restartClockWindow();

	template <typename FunctionT>
	static void withInstance(bool isAuthority, FunctionT&& function)
	{
		if (UELatencyBudgetDiagnostics* diagnostics = instanceFor(isAuthority))
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

	static UELatencyBudgetDiagnostics* instanceFor(bool isAuthority);

	InputProvider wrap(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider);
	void postRelayedReads_Internal(unsigned int id, const RelayedReadObservationRing& ring, uint32 tick,
		double stepStartSeconds);

	const UESimulationSession<TraitsT>& m_owner;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_owner)>>,
		"UELatencyBudgetDiagnostics reads its session's production state (m_manager, m_storage, m_inputResolution and "
		"the client clock, and the delayed-input map through the session's const forEachAuthorityCorrectionTick) through "
		"a CONST reference only. It is the session's friend, so a mutable reference here would be a write door from a "
		"diagnostic into every private member of the session. Added by og-tickdriver task 18; moved to OGSimulationUnreal by "
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
	std::unordered_map<unsigned int, uint32>           m_lastHandedCorrectionTick;
	std::unordered_map<unsigned int, uint32>           m_lastHandedCorrectionTick_Runner;
	std::unordered_map<unsigned int, SeenReads>        m_seenReads_Physics;
	FDelegateHandle                                    m_postTickFlushHandle;
#else
	explicit UELatencyBudgetDiagnostics(const UESimulationSession<TraitsT>&) {}

	void begin(UWorld&, bool) {}
	void end(UWorld&) {}
	void forgetCharacter(unsigned int) {}

	void stamp(latencyBudget::Stream, unsigned int, uint32, latencyBudget::Point) {}
	void stampAt(latencyBudget::Stream, unsigned int, uint32, latencyBudget::Point, double) {}
	void stampCorrectionReceived(unsigned int, uint32) {}
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

	template <typename BackendDiagnosticsT>
	void tick_GameThread(BackendDiagnosticsT) {}

	ClockCounts clockCountsInWindow() const { return {}; }
	void restartClockWindow() {}

	template <typename FunctionT>
	static void withInstance(bool, FunctionT&&) {}

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
UELatencyBudgetDiagnostics<TraitsT>* UELatencyBudgetDiagnostics<TraitsT>::instanceFor(bool isAuthority)
{
	typename TraitsT::SessionOwner* manager = TraitsT::SessionOwner::instanceFor(isAuthority);
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

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::forgetCharacter(unsigned int id)
{
	if (m_probe)
		m_probe->forgetLane(id);
	m_lastHandedCorrectionTick.erase(id);
	// ⛔G-05  docs/UELatencyBudgetDiagnostics-guards.md

}

template <typename TraitsT>
typename UELatencyBudgetDiagnostics<TraitsT>::InputProvider UELatencyBudgetDiagnostics<TraitsT>::wrap(
	unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider)
{
	if (!inputProvider)
		return inputProvider;

	// ⛔G-02  docs/UELatencyBudgetDiagnostics-guards.md
	typename TraitsT::InputSampleSource sampleSource = TraitsT::Game::inputSampleSource(owner);
	return
		[this, id, sampleSource, inner = std::move(inputProvider)](
			const SimulationTimeStep& step,
			const LocalInputCache<typename TraitsT::PlayerInput>& localInputCache)
		{
			typename TraitsT::PlayerInput input = inner(step, localInputCache);
			const double capturedSeconds = FPlatformTime::Seconds();
			const uint32 tick = step.getTick();
			const double sampledSeconds = TraitsT::Game::inputSampledSeconds(sampleSource);
			if (sampledSeconds > 0.0)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, tick, latencyBudget::Point::InputSampled, sampledSeconds));
			}
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
				id, tick, latencyBudget::Point::Captured, capturedSeconds));
			return input;
		};
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stamp(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point)
{
	checkf(IsInGameThread(),
		TEXT("UELatencyBudgetDiagnostics::stamp is the GAME-THREAD door to the latency probe; ")
		TEXT("the physics thread posts through post_Internal's mailbox instead."));
	if (m_probe)
		m_probe->stamp(stream, lane, tick, point, FPlatformTime::Seconds());
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampAt(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point, double seconds)
{
	checkf(IsInGameThread(),
		TEXT("UELatencyBudgetDiagnostics::stampAt is the GAME-THREAD door to the latency probe for a stamp at a time ")
		TEXT("the caller measured; the physics thread posts through post_Internal's mailbox instead."));
	if (m_probe)
		m_probe->stamp(stream, lane, tick, point, seconds);
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampCorrectionReceived(unsigned int lane, uint32 correctionTick)
{
	checkf(IsInGameThread(),
		TEXT("UELatencyBudgetDiagnostics::stampCorrectionReceived is game-thread only: the probe has one owner, and the ")
		TEXT("relay store it reads the delay stamp from is written on the game thread."));
	if (!m_probe)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_probe->stamp(latencyBudget::Stream::State, lane, correctionTick, latencyBudget::Point::ClientReceived, nowSeconds);

	const auto* store =
		m_owner.m_inputResolution.template findRemoteInputCache<typename TraitsT::Simulatable>(lane);
	if (store == nullptr)
		return;
	const auto latest = store->findLatest();
	if (latest.valid)
		m_probe->stampHealedByCorrection(lane, correctionTick, latest.dA, nowSeconds);
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::noteWireSample(double roundTripSeconds)
{
	checkf(IsInGameThread(),
		TEXT("UELatencyBudgetDiagnostics::noteWireSample is game-thread only: the probe has one owner."));
	if (m_probe)
		m_probe->addSample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5);
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::post_Internal(const latencyBudget::Event& event)
{
	if (m_mailbox)
		m_mailbox->tryPush(PostedEvent{ event });
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampClientReceived_Internal(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, double recvSeconds)
{
	if (!m_mailbox)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(stream, lane, tick, latencyBudget::Point::ClientReceived,
		nowSeconds) });
	if (stream == latencyBudget::Stream::State)
	{
		const auto* store =
			m_owner.m_inputResolution.template findRemoteInputCache<typename TraitsT::Simulatable>(lane);
		if (store != nullptr)
		{
			const auto latest = store->findLatest();
			if (latest.valid)
			{
				m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, tick,
					latencyBudget::Point::Healed, nowSeconds), static_cast<uint32>(latest.dA), true });
			}
		}
	}
	m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(stream, lane, tick, latencyBudget::Point::NetReceived,
		recvSeconds) });
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::noteWireSample_Internal(double roundTripSeconds)
{
	post_Internal(latencyBudget::Event::sample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5));
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampHandedAndLeft_Internal(unsigned int lane, uint32 captureTick)
{
	const double nowSeconds = FPlatformTime::Seconds();
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick, latencyBudget::Point::Handed,
		nowSeconds));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::LeftProcess, nowSeconds));
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampServerReceived_Internal(unsigned int lane, uint32 captureTick,
	double recvSeconds)
{
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::ServerReceived, FPlatformTime::Seconds()));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::NetReceived, recvSeconds));
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampCorrectionHanded_Internal(unsigned int lane, uint32 correctionTick,
	double handedSeconds)
{
	uint32& lastHanded = m_lastHandedCorrectionTick_Runner[lane];
	if (lastHanded == correctionTick)
		return;
	lastHanded = correctionTick;
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State, lane, correctionTick,
		latencyBudget::Point::Handed, handedSeconds));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State, lane, correctionTick,
		latencyBudget::Point::LeftProcess, handedSeconds));
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampAfterStep_Internal(double stepStartSeconds)
{
	if (!m_mailbox || !m_owner.m_manager.has_value())
		return;

	const uint32 tick = m_owner.m_manager->currentIntegratedTick();

	if (!m_owner.m_manager->runsPrediction())
	{
		m_owner.m_storage.template forEachSimulatable<typename TraitsT::Simulatable>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32 captureTick =
					m_owner.m_inputResolution.template getLastUsedCaptureTick<typename TraitsT::Simulatable>(id);
				if (captureTick != kNoInputCaptureTick)
				{
					post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, captureTick, latencyBudget::Point::Consumed, stepStartSeconds));
				}
			});
		return;
	}

	post_Internal(latencyBudget::Event::stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::ClientReceived, latencyBudget::Point::Consumed, stepStartSeconds));

	m_owner.m_storage.template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_owner.m_inputResolution.getDiagnostics().template relayedReadObservations<typename TraitsT::Simulatable>(id);
			if (ring != nullptr)
				postRelayedReads_Internal(id, *ring, tick, stepStartSeconds);
		});

	if (m_clockMailbox)
	{
		const ClientPredictionClock& clock = m_owner.m_manager->getClientClock();
		ClockCounts counts;
		counts.skips       = clock.getDiagnostics().skipCount();
		counts.stalls      = clock.getDiagnostics().stallCount();
		counts.hardResyncs = clock.getDiagnostics().hardResyncCount();
		if (!(counts == m_clockPosted_Physics) && m_clockMailbox->tryPush(counts))
			m_clockPosted_Physics = counts;
	}
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::postRelayedReads_Internal(
	unsigned int id, const RelayedReadObservationRing& ring, uint32 tick, double stepStartSeconds)
{
	auto seenIt = m_seenReads_Physics.find(id);
	if (seenIt == m_seenReads_Physics.end())
	{
		if (m_seenReads_Physics.size() >= latencyBudget::ProbeConfig{}.maxLanes)
			m_seenReads_Physics.clear();
		seenIt = m_seenReads_Physics.emplace(id, SeenReads{}).first;
	}
	SeenReads& seen = seenIt->second;

	for (std::size_t index = 0; index < ring.size(); ++index)
	{
		const RelayedReadObservation* const observation = ring.at(index);
		if (observation == nullptr)
			continue;
		const SeenRead read{ observation->simTick, observation->appliedCaptureTick,
			observation->hasAppliedCaptureTick, observation->outcome };
		const bool servedAtSchedule =
			read.hasAppliedCaptureTick && read.outcome == ScheduledRelayedReadOutcome::Hit;
		if (read.simTick == tick)
		{
			if (read.hasAppliedCaptureTick)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, read.appliedCaptureTick, latencyBudget::Point::Consumed, stepStartSeconds));
			}
			if (servedAtSchedule)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, read.appliedCaptureTick, latencyBudget::Point::Healed, stepStartSeconds));
			}
		}
		else if (seen.primed && seen.slots[index] != read && servedAtSchedule)
		{
			// ⛔G-04  docs/UELatencyBudgetDiagnostics-guards.md
			post_Internal(latencyBudget::Event::stampHealedIfPending(id, read.appliedCaptureTick, stepStartSeconds));
		}
		seen.slots[index] = read;
	}
	seen.primed = true;
}

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::stampStepEnd_Internal()
{
	if (!m_mailbox || !m_owner.m_manager.has_value())
		return;

	const double stepEndSeconds = FPlatformTime::Seconds();
	const uint32 tick = m_owner.m_manager->currentIntegratedTick();
	m_owner.m_storage.template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}

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
template <typename BackendDiagnosticsT>
void UELatencyBudgetDiagnostics<TraitsT>::tick_GameThread(BackendDiagnosticsT backendDiagnostics)
{
	if (!m_probe || !m_mailbox || !m_owner.m_manager.has_value())
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
	const bool isClient = m_owner.m_manager->runsPrediction();

	if (!isClient && !m_owner.m_authorityLoop)
	{
		m_owner.forEachAuthorityCorrectionTick(
			[this, nowSeconds](unsigned int id, uint32 correctionTick)
			{
				uint32& lastHanded = m_lastHandedCorrectionTick[id];
				if (lastHanded == correctionTick)
					return;
				lastHanded = correctionTick;
				m_probe->stamp(latencyBudget::Stream::State, id, correctionTick,
					latencyBudget::Point::Handed, nowSeconds);
			});
	}

	m_probe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

	backendDiagnostics.sampleLatencyWindow_GameThread(isClient);

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

	backendDiagnostics.logLatencyWindow_GameThread(isClient);
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
