// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/AutomationTest.h"
#include "Net/DataChannel.h"
#include "OGNetInternetAddr.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "SocketSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
bool AddressesEqual(const FInternetAddr& Left, const FInternetAddr& Right)
{
	return Left == Right;
}

constexpr float kHelloPumpDeltaSeconds = 0.02f;
constexpr double kHelloUpWaitSeconds = 5.0;
constexpr int32 kHelloMinimumPumps = 30;

class FHelloCountingNotify final : public FNetworkNotify
{
public:
	int32 Accepted = 0;
	int32 Hellos = 0;
	int32 OtherMessages = 0;

	virtual EAcceptConnection::Type NotifyAcceptingConnection() override { return EAcceptConnection::Accept; }
	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override { ++Accepted; }
	virtual bool NotifyAcceptingChannel(UChannel* Channel) override { return true; }
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override
	{
		if (MessageType == NMT_Hello)
		{
			++Hellos;
			FNetControlMessage<NMT_Hello>::Discard(Bunch);
			return;
		}
		++OtherMessages;
		Bunch.SetAtEnd();
	}
};

struct FHelloTestState
{
	FAutomationTestBase* Test = nullptr;
	FHelloCountingNotify ServerNotify;
	UOGNetDriver* Server = nullptr;
	UOGNetDriver* Client = nullptr;
	UPendingNetGame* Join = nullptr;
	uint64 ConnectFrame = 0;
	bool bReleased = false;
	double HelloDeadlineSeconds = 0.0;
};

UOGNetConnection* ClientConnection(const FHelloTestState& State)
{
	return State.Client != nullptr ? Cast<UOGNetConnection>(State.Client->ServerConnection) : nullptr;
}

void PumpServer(FHelloTestState& State)
{
	State.Server->TickDispatch(kHelloPumpDeltaSeconds);
}

void PumpClient(FHelloTestState& State)
{
	State.Client->TickDispatch(kHelloPumpDeltaSeconds);
	State.Client->TickFlush(kHelloPumpDeltaSeconds);
}

void DestroyHelloTestState(FHelloTestState& State)
{
	if (State.Join != nullptr)
	{
		State.Join->NetDriver = nullptr;
		State.Join->RemoveFromRoot();
		State.Join = nullptr;
	}
	for (UOGNetDriver** Slot : {&State.Client, &State.Server})
	{
		if (UOGNetDriver* Driver = *Slot)
		{
			Driver->SetWorld(nullptr);
			Driver->Shutdown();
			Driver->LowLevelDestroy();
			Driver->RemoveFromRoot();
			*Slot = nullptr;
		}
	}
}
} // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOGNetHelloAfterTheConnectingFrameCommand, TSharedRef<FHelloTestState>, State);

bool FOGNetHelloAfterTheConnectingFrameCommand::Update()
{
	FHelloTestState& S = *State;
	if (S.Client == nullptr || S.Server == nullptr)
	{
		return true;
	}
	if (GFrameCounter <= S.ConnectFrame)
	{
		return false;
	}
	UOGNetConnection* Connection = ClientConnection(S);
	if (!S.bReleased)
	{
		S.Test->TestEqual(TEXT("the server saw no hello before the frame that opened the connection ended"), S.ServerNotify.Hellos, 0);
		PumpClient(S);
		S.bReleased = true;
		S.HelloDeadlineSeconds = FPlatformTime::Seconds() + kHelloUpWaitSeconds;
		S.Test->TestTrue(TEXT("the first TickDispatch of the next frame sends UE's hello"), Connection != nullptr && Connection->OutTotalPackets >= 1);
		S.Test->TestTrue(TEXT("the control channel opens with the hello"), Connection != nullptr && Connection->Channels[0] != nullptr);
	}
	while (S.ServerNotify.Hellos == 0 && FPlatformTime::Seconds() < S.HelloDeadlineSeconds)
	{
		PumpServer(S);
		FPlatformProcess::Sleep(0.005f);
	}
	S.Test->TestEqual(TEXT("the server received exactly one hello"), S.ServerNotify.Hellos, 1);
	S.Test->TestEqual(TEXT("the server received no other control message"), S.ServerNotify.OtherMessages, 0);
	DestroyHelloTestState(S);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGNetDriverHelloAfterTheConnectingFrameTest, "OGNet.Unreal.Driver.HelloAfterTheConnectingFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGNetDriverHelloAfterTheConnectingFrameTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FHelloTestState> State = MakeShared<FHelloTestState>();
	FHelloTestState& S = *State;
	S.Test = this;

	S.Server = NewObject<UOGNetDriver>(GetTransientPackage());
	S.Server->AddToRoot();
	S.Server->NetDriverName = FName(TEXT("OGNetHelloTestServer"));
	FURL ListenURL;
	ListenURL.Port = 0;
	FString Error;
	if (!TestTrue(TEXT("the server listens"), S.Server->InitListen(&S.ServerNotify, ListenURL, false, Error)))
	{
		AddError(Error);
		DestroyHelloTestState(S);
		return false;
	}

	FURL ConnectURL;
	ConnectURL.Host = TEXT("127.0.0.1");
	ConnectURL.Port = ListenURL.Port;
	S.Join = NewObject<UPendingNetGame>(GetTransientPackage());
	S.Join->AddToRoot();
	S.Join->Initialize(ConnectURL);
	S.Client = NewObject<UOGNetDriver>(GetTransientPackage());
	S.Client->AddToRoot();
	S.Client->NetDriverName = NAME_PendingNetDriver;
	S.ConnectFrame = GFrameCounter;
	S.Join->InitNetDriver(S.Client);

	UOGNetConnection* Connection = ClientConnection(S);
	if (!TestTrue(TEXT("the pending game connects through the client driver"), S.Join->NetDriver.Get() == S.Client && Connection != nullptr))
	{
		AddError(S.Join->ConnectionError);
		DestroyHelloTestState(S);
		return false;
	}
	TestTrue(TEXT("no connection error"), S.Join->ConnectionError.IsEmpty());
	TestEqual(TEXT("UE packets sent inside InitConnect and BeginHandshake"), Connection->OutTotalPackets, 0);
	TestTrue(TEXT("no control channel before the hello"), Connection->Channels[0] == nullptr);

	const double UpDeadlineSeconds = FPlatformTime::Seconds() + kHelloUpWaitSeconds;
	int32 Pumps = 0;
	while ((Pumps < kHelloMinimumPumps || Connection->OGNetId == 0 || S.ServerNotify.Accepted == 0) && FPlatformTime::Seconds() < UpDeadlineSeconds)
	{
		PumpServer(S);
		PumpClient(S);
		++Pumps;
		FPlatformProcess::Sleep(0.005f);
	}
	TestTrue(TEXT("og-net is up inside the frame that opened the connection"), Connection->OGNetId != 0 && S.ServerNotify.Accepted == 1);
	TestEqual(TEXT("still the frame that opened the connection"), GFrameCounter, S.ConnectFrame);
	TestTrue(TEXT("the client's elapsed time passed the keepalive interval"), S.Client->GetElapsedTime() > S.Client->KeepAliveTime);
	TestEqual(TEXT("UE packets sent in the frame that opened the connection, og-net up"), Connection->OutTotalPackets, 0);
	TestEqual(TEXT("hellos the server received in that frame"), S.ServerNotify.Hellos, 0);

	ADD_LATENT_AUTOMATION_COMMAND(FOGNetHelloAfterTheConnectingFrameCommand(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGNetDriverConfigMatchesIpNetDriverTest, "OGNet.Unreal.Driver.ConfigMatchesIpNetDriver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGNetDriverConfigMatchesIpNetDriverTest::RunTest(const FString& Parameters)
{
	UClass* IpNetDriverClass = LoadClass<UNetDriver>(nullptr, TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
	if (!TestNotNull(TEXT("IpNetDriver class"), IpNetDriverClass))
	{
		return false;
	}
	const UNetDriver* IpDefaults = IpNetDriverClass->GetDefaultObject<UNetDriver>();
	const UNetDriver* OGDefaults = GetDefault<UOGNetDriver>();

	int32 Compared = 0;
	int32 Differing = 0;
	for (TFieldIterator<FProperty> It(UNetDriver::StaticClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Config) || Property->GetFName() == FName(TEXT("NetConnectionClassName")))
		{
			continue;
		}
		++Compared;
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			const void* IpValue = Property->ContainerPtrToValuePtr<void>(IpDefaults, Index);
			const void* OGValue = Property->ContainerPtrToValuePtr<void>(OGDefaults, Index);
			if (Property->Identical(IpValue, OGValue, PPF_None))
			{
				continue;
			}
			++Differing;
			FString IpText;
			FString OGText;
			Property->ExportTextItem_Direct(IpText, IpValue, nullptr, nullptr, PPF_None);
			Property->ExportTextItem_Direct(OGText, OGValue, nullptr, nullptr, PPF_None);
			AddError(FString::Printf(TEXT("UNetDriver config property %s differs: IpNetDriver=%s OGNetDriver=%s"), *Property->GetName(), *IpText,
				*OGText));
		}
	}

	TestTrue(TEXT("UNetDriver has config properties to compare"), Compared > 10);
	TestEqual(TEXT("UNetDriver config properties that differ between IpNetDriver and OGNetDriver"), Differing, 0);
	AddInfo(FString::Printf(TEXT("compared %d UNetDriver config properties"), Compared));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGNetDriverInternetAddrTest, "OGNet.Unreal.Driver.InternetAddr",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGNetDriverInternetAddrTest::RunTest(const FString& Parameters)
{
	const FInternetAddrOGNet A(0x1234abcdu);
	const FInternetAddrOGNet SameAsA(0x1234abcdu);
	const FInternetAddrOGNet B(7u);
	const FInternetAddrOGNet Unset;

	TestTrue(TEXT("equal ids are equal"), AddressesEqual(A, SameAsA));
	TestFalse(TEXT("different ids differ"), AddressesEqual(A, B));
	TestEqual(TEXT("hash is the id"), A.GetTypeHash(), 0x1234abcdu);
	TestEqual(TEXT("GetTypeHash(addr) is the id"), GetTypeHash(static_cast<const FInternetAddr&>(B)), 7u);
	TestEqual(TEXT("ToString without port"), A.ToString(false), FString(TEXT("OGNet:1234abcd")));
	TestEqual(TEXT("ToString with port"), A.ToString(true), FString(TEXT("OGNet:1234abcd")));
	TestEqual(TEXT("ToString pads to 8 hex digits"), B.ToString(false), FString(TEXT("OGNet:00000007")));
	TestEqual(TEXT("protocol type"), A.GetProtocolType(), FName(TEXT("OGNet")));
	TestTrue(TEXT("a non-zero id is valid"), A.IsValid());
	TestFalse(TEXT("id 0 is not valid"), Unset.IsValid());
	TestEqual(TEXT("no port"), A.GetPort(), 0);

	const TSharedRef<FInternetAddr> Clone = A.Clone();
	TestEqual(TEXT("clone keeps the protocol"), Clone->GetProtocolType(), FName(TEXT("OGNet")));
	TestTrue(TEXT("clone equals the original"), AddressesEqual(*Clone, A));
	TestTrue(TEXT("original equals the clone"), AddressesEqual(A, *Clone));
	TestEqual(TEXT("clone keeps the id"), static_cast<const FInternetAddrOGNet&>(*Clone).Id, 0x1234abcdu);

	const TArray<uint8> Raw = A.GetRawIp();
	TestEqual(TEXT("raw ip is 4 bytes"), Raw.Num(), 4);
	if (Raw.Num() == 4)
	{
		TestEqual(TEXT("raw ip byte 0"), Raw[0], uint8{0x12});
		TestEqual(TEXT("raw ip byte 1"), Raw[1], uint8{0x34});
		TestEqual(TEXT("raw ip byte 2"), Raw[2], uint8{0xab});
		TestEqual(TEXT("raw ip byte 3"), Raw[3], uint8{0xcd});
	}
	uint32 Ip = 0;
	A.GetIp(Ip);
	TestEqual(TEXT("GetIp is the id"), Ip, 0x1234abcdu);

	FInternetAddrOGNet Mutable(42u);
	Mutable.SetPort(7777);
	Mutable.SetIp(0x7f000001u);
	bool bValid = true;
	Mutable.SetIp(TEXT("127.0.0.1"), bValid);
	Mutable.SetRawIp(TArray<uint8>{1, 2, 3, 4});
	Mutable.SetAnyAddress();
	Mutable.SetLoopbackAddress();
	Mutable.SetBroadcastAddress();
	TestEqual(TEXT("the setters leave the id"), Mutable.Id, 42u);
	TestEqual(TEXT("the port stays 0"), Mutable.GetPort(), 0);
	TestFalse(TEXT("an IP text is never valid"), bValid);

	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		const TSharedRef<FInternetAddr> Platform = Sockets->CreateInternetAddr();
		Platform->SetRawIp(A.GetRawIp());
		Platform->SetPort(0);
		TestFalse(TEXT("an OGNet address never equals a platform address with the same bytes"), AddressesEqual(A, *Platform));
	}
	return true;
}

#endif
