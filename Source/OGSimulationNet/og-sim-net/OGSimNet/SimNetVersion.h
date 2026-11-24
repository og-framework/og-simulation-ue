#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGSimNet-rationale.md

#include "OGSimNet/SimNetClock.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimulation/Network/SimWireToken.h"

#include <cstdint>

namespace ogsimnet
{
inline constexpr uint32_t kSimNetApiVersion = 1u;

inline constexpr simWireToken::Token kWireTokenFragment =
	simWireToken::tokenOf({ { "ro", kRosterWireVersion }, { "ck", kClockWireVersion } });
} // namespace ogsimnet
