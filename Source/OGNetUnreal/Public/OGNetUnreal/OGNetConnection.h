#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGNetConnection-rationale.md · docs/OGNetConnection-guards.md

#include "CoreMinimal.h"
#include "Engine/NetConnection.h"

#include "OGNetConnection.generated.h"

UCLASS(transient, config = Engine)
class OGNETUNREAL_API UOGNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	virtual void InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0,
		int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, const FInternetAddr& InRemoteAddr,
		EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0,
		int32 InPacketOverhead = 0) override;
	virtual void InitHandler() override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	virtual void CleanUp() override;

	uint32 OGNetId = 0;
};
