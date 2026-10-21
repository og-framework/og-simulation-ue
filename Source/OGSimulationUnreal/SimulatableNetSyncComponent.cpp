// SPDX-License-Identifier: MPL-2.0
// docs/SimulatableNetSyncComponent-rationale.md · docs/SimulatableNetSyncComponent-guards.md

#include "SimulatableNetSyncComponent.h"

#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "SimulationConnectionRelay.h"
#include "SimulationInputRelay.h"
#include "UEConnectionHandle.h"
#include "UESimulatableOwnerBinding.h"
#include "UESimulationLogRoutes.h"

#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

OGSIM_OPTIMIZE_OFF

USimulatableNetSyncComponent::USimulatableNetSyncComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void USimulatableNetSyncComponent::attachBinding(ISimulatableOwnerBinding& binding)
{
	m_binding = &binding;
}

void USimulatableNetSyncComponent::detachBinding()
{
	m_binding = nullptr;
}

void USimulatableNetSyncComponent::sendInputBundleToAuthority(const FInputRedundancyBundle& bundle)
{
	ServerReceiveRemoteMove(bundle);
}

void USimulatableNetSyncComponent::OnRep_CorrectionState()
{
	checkf(m_binding != nullptr,
		TEXT("USimulatableNetSyncComponent: OnRep_CorrectionState on '%s' with no binding attached"),
		*GetPathName());

	// ⛔G-03  docs/SimulatableNetSyncComponent-guards.md
	if (m_wireFormatMismatchDetected)
		return;

	const uint8 expectedVersion = FSimulationStateSyncBuffer::kWireFormatVersion;
	const uint8 wireVersion = m_simulationStateCorrectionSyncedBuffer.getReceivedWireFormatVersion();
	if (wireVersion != expectedVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on OnRep_CorrectionState: server version=%u, client expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)wireVersion, (unsigned int)expectedVersion);

		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(
				kWireFormatMismatchToastKey, /*TimeToDisplay*/ 15.f, FColor::Red,
				TEXT("Build mismatch — please get the latest archive (see PLAYTEST_PORTABLE_README.md)"));
		}

		m_wireFormatMismatchDetected = true;
		return;
	}

	m_binding->onCorrectionStateAccepted();
	if (m_onCorrectionStateReceivedCallback)
		m_onCorrectionStateReceivedCallback(m_simulationStateCorrectionSyncedBuffer);
}

// ⛔G-04  docs/SimulatableNetSyncComponent-guards.md

// ⛔G-05  docs/SimulatableNetSyncComponent-guards.md

void USimulatableNetSyncComponent::ServerReceiveRemoteMove_Implementation(const FInputRedundancyBundle& bundle)
{
	checkf(m_binding != nullptr,
		TEXT("USimulatableNetSyncComponent: ServerReceiveRemoteMove on '%s' with no binding attached"),
		*GetPathName());

	// ⛔G-06  docs/SimulatableNetSyncComponent-guards.md
	if (bundle.wireBytes.Num() == 0)
		return;

	const uint8 clientVersion = bundle.getWireFormatVersion();
	if (clientVersion != FInputRedundancyBundle::kWireFormatVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on ServerReceiveRemoteMove: client version=%u, server expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)clientVersion, (unsigned int)FInputRedundancyBundle::kWireFormatVersion);
		// ⛔G-07  docs/SimulatableNetSyncComponent-guards.md
		return;
	}

	m_binding->receiveRemoteMoveBundle(bundle);
}

void USimulatableNetSyncComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USimulatableNetSyncComponent, m_simulationStateCorrectionSyncedBuffer);

	// ⛔G-08  docs/SimulatableNetSyncComponent-guards.md

	// ⛔G-09  docs/SimulatableNetSyncComponent-guards.md

	// ⛔G-10  docs/SimulatableNetSyncComponent-guards.md

}

void USimulatableNetSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ⛔G-11  docs/SimulatableNetSyncComponent-guards.md
	if (m_binding == nullptr)
		detachInputRelayHost(GetNetMode() != NM_Client);

	UActorComponent::EndPlay(EndPlayReason);
}

void USimulatableNetSyncComponent::detachInputRelayHost(bool isAuthority)
{
	if (ASimulationInputRelay* host = m_inputRelayHost.Get())
	{
		// ⛔G-12  docs/SimulatableNetSyncComponent-guards.md
		if (isAuthority)
			host->detachFromParentAndDestroy();
		else
			host->clearOnRelayedInputReceivedCallback();
	}
	m_inputRelayHost = nullptr;
}

FRelayedInputRing& USimulatableNetSyncComponent::getRelayedInputRing()
{
	if (ASimulationInputRelay* host = m_inputRelayHost.Get())
		return host->editRelayedInputRing();
	return m_detachedRelayRing;
}

const FRelayedInputRing& USimulatableNetSyncComponent::getRelayedInputRing() const
{
	if (const ASimulationInputRelay* host = m_inputRelayHost.Get())
		return host->getRelayedInputRing();
	return m_detachedRelayRing;
}

FRelayedInputRing& USimulatableNetSyncComponent::relayStagingTarget()
{
	ASimulationInputRelay* host = m_inputRelayHost.Get();
	// ⛔G-13  docs/SimulatableNetSyncComponent-guards.md
	FRelayedInputRing& stage = (host != nullptr)
		? host->editRelayedInputStagingRing()
		: m_detachedRelayStagingRing;
	return stage;
}

ASimulationInputRelay* USimulatableNetSyncComponent::getInputRelayHost() const
{
	return m_inputRelayHost.Get();
}

void USimulatableNetSyncComponent::setOnRelayedInputReceivedCallback(
	std::function<void(const FRelayedInputRing&)> fn)
{
	m_onRelayedInputReceivedCallback = std::move(fn);

	if (const ASimulationInputRelay* host = m_inputRelayHost.Get())
	{
		if (m_onRelayedInputReceivedCallback)
			m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
	}
}

void USimulatableNetSyncComponent::clearOnRelayedInputReceivedCallback()
{
	m_onRelayedInputReceivedCallback = nullptr;
}

void USimulatableNetSyncComponent::attachInputRelayHost(ASimulationInputRelay* host)
{
	if (host == nullptr || m_inputRelayHost.Get() == host)
		return;

	m_inputRelayHost = host;

	TWeakObjectPtr<USimulatableNetSyncComponent> weakSelf(this);
	// ⛔G-14  docs/SimulatableNetSyncComponent-guards.md
	host->setOnRelayedInputReceivedCallback(
		[weakSelf](const FRelayedInputRing& ring)
		{
			if (USimulatableNetSyncComponent* self = weakSelf.Get())
				self->onRelayedInputRingArrived(ring);
		});

	if (GetNetMode() != NM_Client)
	{
		host->setOnRingFlushedCallback(m_binding->makeRingFlushedCallback());
	}

	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
}

void USimulatableNetSyncComponent::onRelayedInputRingArrived(const FRelayedInputRing& ring)
{
	// ⛔G-15  docs/SimulatableNetSyncComponent-guards.md
	if (m_hasLocalInputProvider && ring.num() > 0 && !m_loggedOwnerSkipDivergence)
	{
		m_loggedOwnerSkipDivergence = true;
		UE_LOG(LogOGNet, Error,
			TEXT("[InputRelay] OWNER-SKIP DIVERGENCE id=%u — a relay ring with %u entries "
			     "arrived for a character that HAS a local input provider. COND_SkipOwner and "
			     "provider-presence disagree; check the relay host's Owner."),
			m_binding->simulationKey(), (unsigned int)ring.num());
		// ⛔G-16  docs/SimulatableNetSyncComponent-guards.md
		ensureMsgf(false,
			TEXT("[InputRelay] relay ring arrived for a provider-present character (id=%u)"),
			m_binding->simulationKey());
	}

	m_binding->noteRelayedRingArrived(ring);

	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(ring);
}

void USimulatableNetSyncComponent::setHasLocalInputProvider(bool hasLocalInputProvider)
{
	m_hasLocalInputProvider = hasLocalInputProvider;
}

static_assert(ConnectionTierSink<USimulatableNetSyncComponent>,
	"USimulatableNetSyncComponent must satisfy ConnectionTierSink so ServerReceptionCoordinator "
	"can drive the tier send through it");

void USimulatableNetSyncComponent::sendConnectionTierToOwningClient(unsigned int id, uint8_t tier)
{
	check(id == m_binding->simulationKey());

	UNetConnection* rootConnection = GetRootNetConnection(GetOwner());
	if (rootConnection == nullptr)
	{
		return;
	}

	ASimulationConnectionRelay* relay =
		ASimulationConnectionRelay::findOrSpawnForConnection(GetWorld(), rootConnection);
	if (relay == nullptr)
		return;

	UE_LOG(LogOGNet, Log,
		TEXT("[ConnectionTier] id=%u server tier %u -> %u (relay %s)"),
		m_binding->simulationKey(),
		(unsigned int)relay->getConnectionTier(), (unsigned int)tier,
		*relay->GetName());

	relay->setConnectionTier(tier);
}

OGSIM_OPTIMIZE_ON
