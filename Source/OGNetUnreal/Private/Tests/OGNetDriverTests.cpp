// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/NetDriver.h"
#include "IPAddress.h"
#include "Misc/AutomationTest.h"
#include "OGNetInternetAddr.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "SocketSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
bool AddressesEqual(const FInternetAddr& Left, const FInternetAddr& Right)
{
	return Left == Right;
}
} // namespace

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
