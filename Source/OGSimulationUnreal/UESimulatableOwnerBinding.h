// SPDX-License-Identifier: MPL-2.0
// docs/UESimulatableOwnerBinding-rationale.md · docs/UESimulatableOwnerBinding-guards.md

#pragma once

#include "CoreMinimal.h"

#include "InputRedundancyBundleBuilder.h"
#include "RelayedInputRing.h"
#include "SimulatableNetSyncComponent.h"
#include "SimulationInputRelay.h"
#include "SyncedSimulationStateBuffer.h"
#include "UEConnectionHandle.h"
#include "UELatencyBudgetDiagnostics.h"
#include "UESimulationLogRoutes.h"

#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"
#include "OGSimulation/SimulationQueues.h"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

struct FInputRedundancyBundle;
struct FRelayedInputRing;
class USimulatableNetSyncComponent;

class ISimulatableOwnerBinding
{
	friend class USimulatableNetSyncComponent;

protected:
	~ISimulatableOwnerBinding() = default;

private:
	virtual unsigned int simulationKey() const = 0;
	virtual void onCorrectionStateAccepted() = 0;
	virtual void receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle) = 0;
	virtual void noteRelayedRingArrived(const FRelayedInputRing& ring) = 0;
	virtual std::function<void(const FRelayedInputRing&)> makeRingFlushedCallback() = 0;
};

static_assert(!std::is_destructible_v<ISimulatableOwnerBinding>
		&& !std::has_virtual_destructor_v<ISimulatableOwnerBinding>,
	"ISimulatableOwnerBinding is never destroyed through a base pointer: the sibling only calls through it, and the "
	"owning game component is destroyed as itself. Its destructor stays protected and non-virtual.");

template <typename TraitsT>
concept UESimulatableOwnerBindingTraits = requires
{
	typename TraitsT::Simulatable;
	typename TraitsT::PlayerInput;
	typename TraitsT::State;
	typename TraitsT::SessionOwner;
};

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
class UESimulatableOwnerBinding : public ISimulatableOwnerBinding
{
public:
	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using RelayedInputRingType        = FRelayedInputRing;

	void bindNetSync(USimulatableNetSyncComponent& sibling);

	bool isNetSyncBound() const { return m_netSync != nullptr; }

	void setOnCorrectionStateReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_netSync->setOnCorrectionStateReceivedCallback(std::move(fn));
	}

	void clearOnCorrectionStateReceivedCallback()
	{
		m_netSync->clearOnCorrectionStateReceivedCallback();
	}

	void sendLocalInputToAuthority(
		const PendingInputQueue<typename TraitsT::PlayerInput>& queue,
		uint32 currentTick,
		uint32 redundancyDepth);

	void setOnRemoteMoveReceivedCallback(
		std::function<void(uint32, const typename TraitsT::PlayerInput&)> fn)
	{
		m_onRemoteMoveReceivedCallback = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback()
	{
		m_onRemoteMoveReceivedCallback = nullptr;
	}

	void deliverDelayedRemoteInput(uint32 captureTick,
	                               const typename TraitsT::PlayerInput& input)
	{
		if (m_onRemoteMoveReceivedCallback)
			m_onRemoteMoveReceivedCallback(captureTick, input);
	}

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_netSync->getSyncedCorrectionStateBuffer(); }
	const FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() const { return static_cast<const USimulatableNetSyncComponent&>(*m_netSync).getSyncedCorrectionStateBuffer(); }

	FRelayedInputRing&       getRelayedInputRing() { return m_netSync->getRelayedInputRing(); }
	const FRelayedInputRing& getRelayedInputRing() const { return static_cast<const USimulatableNetSyncComponent&>(*m_netSync).getRelayedInputRing(); }

	relayedInputRing::StageArrivalOutcome stageRelayedInput(
		uint32 captureTick, uint8 dA, const typename TraitsT::PlayerInput& input);

	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn)
	{
		m_netSync->setOnRelayedInputReceivedCallback(std::move(fn));
	}

	void clearOnRelayedInputReceivedCallback()
	{
		m_netSync->clearOnRelayedInputReceivedCallback();
	}

	void attachInputRelayHost(ASimulationInputRelay* host) { m_netSync->attachInputRelayHost(host); }

protected:
	~UESimulatableOwnerBinding() = default;

	USimulatableNetSyncComponent& netSync();
	void unbindNetSync();

private:
	OwnerT&       derived()       { return static_cast<OwnerT&>(*this); }
	const OwnerT& derived() const { return static_cast<const OwnerT&>(*this); }

	unsigned int simulationKey() const final;
	void onCorrectionStateAccepted() final;
	void receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle) final;
	void noteRelayedRingArrived(const FRelayedInputRing& ring) final;
	std::function<void(const FRelayedInputRing&)> makeRingFlushedCallback() final;

	USimulatableNetSyncComponent* m_netSync = nullptr;

	std::function<void(uint32, const typename TraitsT::PlayerInput&)> m_onRemoteMoveReceivedCallback;
};

OGSIM_OPTIMIZE_OFF

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::bindNetSync(USimulatableNetSyncComponent& sibling)
{
	m_netSync = &sibling;
	sibling.attachBinding(*this);
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
USimulatableNetSyncComponent& UESimulatableOwnerBinding<OwnerT, TraitsT>::netSync()
{
	checkf(m_netSync != nullptr,
		TEXT("UESimulatableOwnerBinding::netSync: this component is not bound to a USimulatableNetSyncComponent. The ")
		TEXT("owning actor binds the two in its PostInitializeComponents (bindNetSync), and the component unbinds in its ")
		TEXT("EndPlay, so a read before the bind or after the unbind is a lifecycle error."));
	return *m_netSync;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::unbindNetSync()
{
	if (m_netSync != nullptr)
		m_netSync->detachBinding();
	m_netSync = nullptr;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
unsigned int UESimulatableOwnerBinding<OwnerT, TraitsT>::simulationKey() const
{
	static_assert(requires(const OwnerT& owner) { { owner.storageKeyOfOwner() } -> std::same_as<unsigned int>; },
		"UESimulatableOwnerBinding<OwnerT, TraitsT> needs the game component's key hook: a private (or public) "
		"'unsigned int storageKeyOfOwner() const' on OwnerT, with the binding befriended. The binding's final "
		"simulationKey() override returns it. The hook has its own name because a game member named simulationKey() "
		"would override the interface member instead (review B-2, ruling Q-F).");
	return derived().storageKeyOfOwner();
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::onCorrectionStateAccepted()
{
	typename TraitsT::State peeked;
	const uint32 tick = m_netSync->getSyncedCorrectionStateBuffer().readInto(peeked);
	OG_LOG_ROUTED(LogOGNet,
		TEXT("[ReceiveCorrectionState] id=%u tick=%u"),
		derived().storageKeyOfOwner(), tick);
	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, tick](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			diagnostics.stamp(latencyBudget::Stream::State, derived().storageKeyOfOwner(),
				tick, latencyBudget::Point::ClientReceived);
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
relayedInputRing::StageArrivalOutcome UESimulatableOwnerBinding<OwnerT, TraitsT>::stageRelayedInput(
	uint32 captureTick, uint8 dA, const typename TraitsT::PlayerInput& input)
{
	static_assert(std::is_same_v<decltype(&UESimulatableOwnerBinding::stageRelayedInput),
			relayedInputRing::StageArrivalOutcome (UESimulatableOwnerBinding::*)(
				uint32, uint8, const typename TraitsT::PlayerInput&)>,
		"UESimulatableOwnerBinding::stageRelayedInput takes NO depth parameter. The stage's capacity is "
		"relayedInputRing::kMaxDepth, fixed inside the codec; a depth passed down here would cap every "
		"round at one entry and silently restore the replace-latest behaviour the staging removed. "
		"Was the 'NO DEPTH PARAMETER, and that is the fence' prose fence of SimmableUpdateComponent.h "
		"and its .cpp one-liner (task 25 conversion); moved from that component by "
		"og-ue-simulationcomponentandmanagersplit task 21.");

	ASimulationInputRelay* host = m_netSync->getInputRelayHost();
	FRelayedInputRing& stage = m_netSync->relayStagingTarget();

	const relayedInputRing::StageArrivalOutcome outcome =
		stage.stageArrival<typename TraitsT::PlayerInput>(captureTick, dA, input);

	// ⛔G-01  docs/UESimulatableOwnerBinding-guards.md
	if (outcome.droppedOldest && host != nullptr)
		host->noteStageOverflowDrop();

	return outcome;
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
std::function<void(const FRelayedInputRing&)> UESimulatableOwnerBinding<OwnerT, TraitsT>::makeRingFlushedCallback()
{
	TWeakObjectPtr<OwnerT> weakSelf(&derived());
	return
			[weakSelf](const FRelayedInputRing& ring)
			{
				UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/true,
					[&weakSelf, &ring](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
					{
						OwnerT* self = weakSelf.Get();
						if (self == nullptr)
							return;
						const unsigned int id = self->storageKeyOfOwner();
						ring.forEachEntry<typename TraitsT::PlayerInput>(
							[&diagnostics, id](uint32 captureTick, uint8, const typename TraitsT::PlayerInput&)
							{
								if (captureTick != kNoInputCaptureTick)
								{
									diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
										latencyBudget::Point::Handed);
								}
							});
					});
			};
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::noteRelayedRingArrived(const FRelayedInputRing& ring)
{
	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, &ring](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			const unsigned int id = derived().storageKeyOfOwner();
			ring.forEachEntry<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, uint8, const typename TraitsT::PlayerInput&)
				{
					if (captureTick != kNoInputCaptureTick)
					{
						diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
							latencyBudget::Point::ClientReceived);
					}
				});
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::sendLocalInputToAuthority(
	const PendingInputQueue<typename TraitsT::PlayerInput>& queue,
	uint32 currentTick,
	uint32 redundancyDepth)
{
	FInputRedundancyBundle bundle;
	buildRedundancyBundle<typename TraitsT::PlayerInput>(
		queue, currentTick, static_cast<uint8>(redundancyDepth), bundle);
	m_netSync->sendInputBundleToAuthority(bundle);

	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/false,
		[this, &bundle](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			const unsigned int id = derived().storageKeyOfOwner();
			bundle.forEachSlot<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, const typename TraitsT::PlayerInput&)
				{
					diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
						latencyBudget::Point::Handed);
				});
		});
}

template <typename OwnerT, typename TraitsT>
	requires UESimulatableOwnerBindingTraits<TraitsT>
void UESimulatableOwnerBinding<OwnerT, TraitsT>::receiveRemoteMoveBundle(const FInputRedundancyBundle& bundle)
{
	// ⛔G-02  docs/UESimulatableOwnerBinding-guards.md
	typename TraitsT::SessionOwner* authorityManager =
		TraitsT::SessionOwner::instanceFor(/*isAuthority=*/true);
	auto* coordinator =
		(authorityManager != nullptr) ? authorityManager->getReceptionCoordinator() : nullptr;

	// ⛔G-03  docs/UESimulatableOwnerBinding-guards.md
	UNetConnection* rootConn =
		(coordinator != nullptr) ? GetRootNetConnection(derived().GetOwner()) : nullptr;

	const unsigned int id = derived().storageKeyOfOwner();

	UELatencyBudgetDiagnostics<TraitsT>::withInstance(/*isAuthority=*/true,
		[&bundle, id, rootConn](UELatencyBudgetDiagnostics<TraitsT>& diagnostics)
		{
			bundle.forEachSlot<typename TraitsT::PlayerInput>(
				[&diagnostics, id](uint32 captureTick, const typename TraitsT::PlayerInput&)
				{
					diagnostics.stamp(latencyBudget::Stream::Input, id, captureTick,
						latencyBudget::Point::ServerReceived);
				});
			if (rootConn != nullptr)
				diagnostics.noteWireSample(readRoundTripMs(rootConn) / 1000.0);
		});

	// ⛔G-04  docs/UESimulatableOwnerBinding-guards.md
	if (coordinator == nullptr || rootConn == nullptr)
	{
		bundle.forEachSlot<typename TraitsT::PlayerInput>(
			[this, id](uint32 captureTick, const typename TraitsT::PlayerInput& input)
			{
				OG_LOG_ROUTED(LogOGNet, TEXT("[ServerReceive] id=%u tick=%u"), id, captureTick);
				if (m_onRemoteMoveReceivedCallback)
					m_onRemoteMoveReceivedCallback(captureTick, input);
			});
		return;
	}

	const FUEConnectionHandle handle(rootConn);

	// ⛔G-05  docs/UESimulatableOwnerBinding-guards.md
	coordinator->noteRttSample(
		handle, id, authorityManager->getServerReceptionTick(),
		readRoundTripMs(rootConn), *m_netSync);

	const uint8 playerSlot = GetPlayerSlotForActor(derived().GetOwner());

	// ⛔G-06  docs/UESimulatableOwnerBinding-guards.md
	coordinator->template receiveInputBundle<typename TraitsT::Simulatable>(
		id, handle, playerSlot, bundle, *authorityManager, *authorityManager);
}

OGSIM_OPTIMIZE_ON
