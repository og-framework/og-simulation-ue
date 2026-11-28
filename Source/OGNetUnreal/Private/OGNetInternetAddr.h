#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGNetInternetAddr-rationale.md

#include "CoreMinimal.h"
#include "IPAddress.h"

class FInternetAddrOGNet final : public FInternetAddr
{
public:
	FInternetAddrOGNet() = default;
	explicit FInternetAddrOGNet(uint32 InId) : Id(InId) {}

	static FName ProtocolType();

	virtual void SetIp(uint32 InAddr) override {}
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override { bIsValid = false; }
	virtual void GetIp(uint32& OutAddr) const override { OutAddr = Id; }
	virtual void SetPort(int32 InPort) override {}
	virtual int32 GetPort() const override { return 0; }
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override {}
	virtual TArray<uint8> GetRawIp() const override;
	virtual void SetAnyAddress() override {}
	virtual void SetBroadcastAddress() override {}
	virtual void SetLoopbackAddress() override {}
	virtual FString ToString(bool bAppendPort) const override;
	virtual bool operator==(const FInternetAddr& Other) const override;
	virtual uint32 GetTypeHash() const override { return Id; }
	virtual bool IsValid() const override { return Id != 0; }
	virtual TSharedRef<FInternetAddr> Clone() const override;
	virtual FName GetProtocolType() const override { return ProtocolType(); }

	uint32 Id = 0;
};
