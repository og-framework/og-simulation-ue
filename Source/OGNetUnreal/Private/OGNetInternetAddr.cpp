// SPDX-License-Identifier: MPL-2.0
// docs/OGNetInternetAddr-rationale.md

#include "OGNetInternetAddr.h"

FName FInternetAddrOGNet::ProtocolType()
{
	static const FName Name(TEXT("OGNet"));
	return Name;
}

TArray<uint8> FInternetAddrOGNet::GetRawIp() const
{
	return TArray<uint8>{static_cast<uint8>(Id >> 24), static_cast<uint8>(Id >> 16), static_cast<uint8>(Id >> 8), static_cast<uint8>(Id)};
}

FString FInternetAddrOGNet::ToString(bool bAppendPort) const
{
	return FString::Printf(TEXT("OGNet:%08x"), Id);
}

bool FInternetAddrOGNet::operator==(const FInternetAddr& Other) const
{
	return Other.GetProtocolType() == ProtocolType() && static_cast<const FInternetAddrOGNet&>(Other).Id == Id;
}

TSharedRef<FInternetAddr> FInternetAddrOGNet::Clone() const
{
	return MakeShared<FInternetAddrOGNet>(Id);
}
