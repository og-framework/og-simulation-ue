// SPDX-License-Identifier: MPL-2.0
// docs/SimulatableNetSyncComponent-rationale.md · docs/SimulatableNetSyncComponent-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "SyncedSimulationStateBuffer.h"
#include "RelayedInputRing.h"

#include <cstdint>
#include <functional>

#include "SimulatableNetSyncComponent.generated.h"

class ISimulatableOwnerBinding;
class ASimulationInputRelay;

UCLASS(ClassGroup = OGSimulation)
class OGSIMULATIONUNREAL_API USimulatableNetSyncComponent final : public UActorComponent
{
	GENERATED_BODY()
public:

	USimulatableNetSyncComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void attachBinding(ISimulatableOwnerBinding& binding);
	void detachBinding();

	void setOnCorrectionStateReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_onCorrectionStateReceivedCallback = std::move(fn);
	}

	void clearOnCorrectionStateReceivedCallback()
	{
		m_onCorrectionStateReceivedCallback = nullptr;
	}

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_simulationStateCorrectionSyncedBuffer; }
	const FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() const { return m_simulationStateCorrectionSyncedBuffer; }

	void sendInputBundleToAuthority(const FInputRedundancyBundle& bundle);

	void sendConnectionTierToOwningClient(unsigned int id, uint8_t tier);

	FRelayedInputRing&       getRelayedInputRing();
	const FRelayedInputRing& getRelayedInputRing() const;

	FRelayedInputRing& relayStagingTarget();

	ASimulationInputRelay* getInputRelayHost() const;

	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn);

	void clearOnRelayedInputReceivedCallback();

	void attachInputRelayHost(ASimulationInputRelay* host);

	void detachInputRelayHost(bool isAuthority);

	void onRelayedInputRingArrived(const FRelayedInputRing& ring);

	void setHasLocalInputProvider(bool hasLocalInputProvider);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CorrectionState)
	FSimulationStateSyncBuffer m_simulationStateCorrectionSyncedBuffer;
	UFUNCTION()
	void OnRep_CorrectionState();

	bool m_wireFormatMismatchDetected = false;
	static constexpr uint64 kWireFormatMismatchToastKey = 0x4F47574DF0000001ull;

	UFUNCTION(Server, Unreliable)
	void ServerReceiveRemoteMove(const FInputRedundancyBundle& bundle);

	// ⛔G-01  docs/SimulatableNetSyncComponent-guards.md

	// ⛔G-02  docs/SimulatableNetSyncComponent-guards.md

	ISimulatableOwnerBinding* m_binding = nullptr;

	TWeakObjectPtr<ASimulationInputRelay> m_inputRelayHost;

	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayRing;

	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayStagingRing;

	bool m_hasLocalInputProvider = false;
	bool m_loggedOwnerSkipDivergence = false;

	std::function<void(const FSimulationStateSyncBuffer&)> m_onCorrectionStateReceivedCallback;
	std::function<void(const FRelayedInputRing&)> m_onRelayedInputReceivedCallback;
};
