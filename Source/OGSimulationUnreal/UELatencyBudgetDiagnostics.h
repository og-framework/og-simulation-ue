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
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "Engine/World.h"

#include <memory>
#include <unordered_map>
#include <utility>
#endif

class UWorld;

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
	explicit UELatencyBudgetDiagnostics(const typename TraitsT::SessionOwner& owner) : m_owner(owner) {}

	void begin(UWorld& world, bool isAuthority);
	void end(UWorld& world);
	void forgetCharacter(unsigned int id);

	void stamp(latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point);
	void noteWireSample(double roundTripSeconds);
	void post_Internal(const latencyBudget::Event& event);
	void stampAfterStep_Internal(double stepStartSeconds);
	void stampStepEnd_Internal();
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
	using Mailbox      = latencyBudget::SpscMailbox<latencyBudget::Event, 4096>;
	using ClockMailbox = latencyBudget::SpscMailbox<ClockCounts, 64>;

	static UELatencyBudgetDiagnostics* instanceFor(bool isAuthority);

	InputProvider wrap(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider);

	const typename TraitsT::SessionOwner& m_owner;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_owner)>>,
		"UELatencyBudgetDiagnostics reads its session owner's production state (m_manager, m_storage, "
		"m_inputResolution, m_delayedInputComponentsById and the client clock) through a CONST reference only. It is "
		"the owner's friend, so a mutable reference here would be a write door from a diagnostic into every private "
		"member of the owner. Added by og-tickdriver task 18; moved to OGSimulationUnreal by task 19a.");

	// ⛔G-01  docs/UELatencyBudgetDiagnostics-guards.md
	std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;
	std::unique_ptr<Mailbox>                           m_mailbox;
	std::unique_ptr<ClockMailbox>                      m_clockMailbox;
	ClockCounts                                        m_clockPosted_Physics;
	ClockCounts                                        m_clockLatest;
	ClockCounts                                        m_clockAtWindowStart;
	uint64                                             m_mailboxDropped = 0u;
	std::unordered_map<unsigned int, uint32>           m_lastHandedCorrectionTick;
	FDelegateHandle                                    m_postTickFlushHandle;
#else
	explicit UELatencyBudgetDiagnostics(const typename TraitsT::SessionOwner&) {}

	void begin(UWorld&, bool) {}
	void end(UWorld&) {}
	void forgetCharacter(unsigned int) {}

	void stamp(latencyBudget::Stream, unsigned int, uint32, latencyBudget::Point) {}
	void noteWireSample(double) {}
	void post_Internal(const latencyBudget::Event&) {}
	void stampAfterStep_Internal(double) {}
	void stampStepEnd_Internal() {}
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
	m_probe = std::make_unique<latencyBudget::LatencyBudgetProbe>(
		isAuthority ? latencyBudget::kServerHops : latencyBudget::kClientHops);
	m_mailbox      = std::make_unique<Mailbox>();
	m_clockMailbox = std::make_unique<ClockMailbox>();
	m_postTickFlushHandle =
		world.OnPostTickFlush().AddWeakLambda(&m_owner, [this]() { onPostTickFlush(); });
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
		m_mailbox->tryPush(event);
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
			if (ring == nullptr)
				return;
			for (std::size_t index = 0; index < ring->size(); ++index)
			{
				const RelayedReadObservation* const observation = ring->at(index);
				if (observation == nullptr || observation->simTick != tick)
					continue;
				if (observation->hasAppliedCaptureTick)
				{
					post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, observation->appliedCaptureTick, latencyBudget::Point::Consumed,
						stepStartSeconds));
				}
				return;
			}
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

	m_mailbox->drain([this](const latencyBudget::Event& event) { m_probe->apply(event); });
	m_mailboxDropped += m_mailbox->takeDroppedCount();
	if (m_clockMailbox)
	{
		m_clockMailbox->drain(
			[this](const ClockCounts& counts) { m_clockLatest = counts; });
		m_mailboxDropped += m_clockMailbox->takeDroppedCount();
	}

	const double nowSeconds = FPlatformTime::Seconds();
	const bool isClient = m_owner.m_manager->runsPrediction();

	if (!isClient)
	{
		for (const auto& entry : m_owner.m_delayedInputComponentsById)
		{
			const typename TraitsT::Component* component = entry.second.Get();
			if (component == nullptr)
				continue;
			// ⛔G-03  docs/UELatencyBudgetDiagnostics-guards.md
			if (!component->isNetSyncBound())
				continue;
			const uint32 correctionTick =
				component->getSyncedCorrectionStateBuffer().template readFromBuffer<uint32>(
					correctionStateBuffer::kTickOffset);
			if (correctionTick == 0u)
				continue;
			uint32& lastHanded = m_lastHandedCorrectionTick[entry.first];
			if (lastHanded == correctionTick)
				continue;
			lastHanded = correctionTick;
			m_probe->stamp(latencyBudget::Stream::State, entry.first, correctionTick,
				latencyBudget::Point::Handed, nowSeconds);
		}
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
