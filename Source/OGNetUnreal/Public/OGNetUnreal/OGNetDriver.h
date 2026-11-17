#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGNetDriver-rationale.md · docs/OGNetDriver-guards.md

#include "CoreMinimal.h"
#include "Engine/NetDriver.h"
#include "Templates/UniquePtr.h"
// ⛔G-01  docs/OGNetDriver-guards.md

#include "OGNetDriver.generated.h"

class UOGNetConnection;
struct FOGNetEndpointHost;

UCLASS(transient, config = Engine)
class OGNETUNREAL_API UOGNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:
	UOGNetDriver();
	UOGNetDriver(FVTableHelper& Helper);
	virtual ~UOGNetDriver() override;

	UPROPERTY(Config)
	bool bUseEnginePacketSimulation = false;

	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool IsNetResourceValid() override;

	void sendEngine(uint32 Id, const uint8* Data, int32 Bytes);
	void engineClosed(UOGNetConnection& Connection);

private:
	friend struct FOGNetEndpointHost;

	TUniquePtr<FOGNetEndpointHost> Endpoint;
};
