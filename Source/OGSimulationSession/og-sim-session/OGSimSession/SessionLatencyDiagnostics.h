#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../../../OGSimulationUnreal/docs/UELatencyBudgetDiagnostics-rationale.md · ../../../OGSimulationUnreal/docs/UELatencyBudgetDiagnostics-guards.md

template <typename TraitsT>
void UELatencyBudgetDiagnostics<TraitsT>::forgetCharacter(unsigned int id)
{
	if (m_probe)
		m_probe->forgetLane(id);
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
			m_owner.runtime().inputResolution().template findRemoteInputCache<typename TraitsT::Simulatable>(lane);
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
	if (!m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	const uint32 tick = m_owner.runtime().getManager()->currentIntegratedTick();

	if (!m_owner.runtime().getManager()->runsPrediction())
	{
		m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32 captureTick =
					m_owner.runtime().inputResolution().template getLastUsedCaptureTick<typename TraitsT::Simulatable>(id);
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

	m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_owner.runtime().inputResolution().getDiagnostics().template relayedReadObservations<typename TraitsT::Simulatable>(id);
			if (ring != nullptr)
				postRelayedReads_Internal(id, *ring, tick, stepStartSeconds);
		});

	if (m_clockMailbox)
	{
		const ClientPredictionClock& clock = m_owner.runtime().getManager()->getClientClock();
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
	if (!m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	const double stepEndSeconds = FPlatformTime::Seconds();
	const uint32 tick = m_owner.runtime().getManager()->currentIntegratedTick();
	m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}
