// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetVersion.h"

#include <string>

static_assert(ogsimnet::kWireTokenFragment.view() == "ro1.ck2",
	"og-session-runtime task 21: og-sim-net's wire-token fragment is ro<roster>.ck<clock> from its own two version "
	"constants. A version bump changes this string and the game's whole-token pin.");

TEST_CASE("SimNetVersion.TheWireTokenFragmentIsTheRosterAndClockVersions", "[NetTransport][SimNetVersion]")
{
	CHECK(std::string(ogsimnet::kWireTokenFragment.view()) == "ro1.ck2");
}
