// SPDX-License-Identifier: MPL-2.0
// docs/OGNetConnection-rationale.md · docs/OGNetConnection-guards.md

#include "OGNetUnreal/OGNetConnection.h"

#include "Net/DataChannel.h"
#include "OGNetInternetAddr.h"
#include "OGNetUnreal/OGNetDriver.h"

#include "OGNetUnreal/Core/EngineLaneSettings.h"

namespace
{
constexpr int32 kInitialPacketSequence = 4;

static_assert(ognetunreal::kEngineMaxPacketBytes == MAX_PACKET_SIZE,
	"the OG connection reports UE's MAX_PACKET_SIZE as its MaxPacket, as UIpConnection does. Was P4-4's "
	"'static_assert(ognetunreal::kEngineMaxPacketBytes == MAX_PACKET_SIZE)'");
} // namespace

void UOGNetConnection::InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket,
	int32 InPacketOverhead)
{
	Super::InitBase(InDriver, InSocket, InURL, InState, ognetunreal::kEngineMaxPacketBytes, ognetunreal::kEnginePacketOverheadBytes);
}

void UOGNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, const FInternetAddr& InRemoteAddr,
	EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	checkf(InRemoteAddr.GetProtocolType() == FInternetAddrOGNet::ProtocolType(),
		TEXT("UOGNetConnection::InitRemoteConnection takes an FInternetAddrOGNet, got protocol %s"), *InRemoteAddr.GetProtocolType().ToString());
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	OGNetId = static_cast<const FInternetAddrOGNet&>(InRemoteAddr).Id;
	RemoteAddr = InRemoteAddr.Clone();
	URL.Host = RemoteAddr->ToString(false);
	InitSequence(kInitialPacketSequence, kInitialPacketSequence);
	InitSendBuffer();
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UOGNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket,
	int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);
	OGNetId = 0;
	RemoteAddr = FInternetAddrOGNet(OGNetId).Clone();
	InitSequence(kInitialPacketSequence, kInitialPacketSequence);
	InitSendBuffer();
}

void UOGNetConnection::InitHandler()
{
	// ⛔G-01  docs/OGNetConnection-guards.md

}

void UOGNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	UOGNetDriver* OGDriver = Cast<UOGNetDriver>(Driver);
	const int32 Bytes = FMath::DivideAndRoundUp(CountBits, 8);
	if (OGDriver == nullptr || Bytes <= 0)
	{
		return;
	}
	OGDriver->sendEngine(OGNetId, static_cast<const uint8*>(Data), Bytes);
}

FString UOGNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return RemoteAddr.IsValid() ? RemoteAddr->ToString(bAppendPort) : FString();
}

FString UOGNetConnection::LowLevelDescribe()
{
	return FString::Printf(TEXT("OGNet:%08x state=%s"), OGNetId, LexToString(GetConnectionState()));
}

void UOGNetConnection::CleanUp()
{
	UOGNetDriver* OGDriver = Cast<UOGNetDriver>(Driver);
	Super::CleanUp();
	if (OGDriver != nullptr)
	{
		OGDriver->engineClosed(*this);
	}
}
