#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGNetSimLanes-rationale.md

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class UOGNetDriver;

namespace ognetunreal
{
struct SimLaneEndpoint;
} // namespace ognetunreal

class IOGNetSimLanes
{
public:
	virtual ~IOGNetSimLanes() = default;

	virtual void beforeStart(ognetunreal::SimLaneEndpoint& Endpoint) = 0;
	virtual void afterStart() = 0;
	virtual void afterDispatch() = 0;
	virtual void onTickFlush() = 0;
	virtual void beforeStop() = 0;
};

using FOGNetSimLanesFactory = TUniquePtr<IOGNetSimLanes> (*)(UOGNetDriver& Driver, bool bIsServer);
