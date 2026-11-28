// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNet/Endpoint/EngineLane.h"

TEST_CASE("EngineLaneDriver.TheEngineLaneFitsUnrealsMaximumPacket", "[OGNet][EngineLaneDriver]")
{
	CHECK(ognet::EngineLane::maxPacketBytes() >= 1024);
}
