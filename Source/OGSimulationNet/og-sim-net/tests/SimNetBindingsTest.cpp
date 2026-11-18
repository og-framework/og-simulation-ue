// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetRoster.h"

#include <cstdint>
#include <string>
#include <vector>

template <>
struct Catch::StringMaker<ogsimnet::BindVerdict>
{
	static std::string convert(const ogsimnet::BindVerdict& verdict)
	{
		static constexpr const char* kSteps[] = {"Wait", "Ready", "RoleMismatch"};
		return std::string("{") + kSteps[static_cast<int>(verdict.step)] + " simId=" + std::to_string(verdict.simId)
			+ " slot=" + std::to_string(verdict.physicsSlot) + " localIndex=" + std::to_string(verdict.localIndex)
			+ " isLocal=" + (verdict.isLocal ? "true" : "false") + "}";
	}
};

namespace
{
using ogsimnet::BindStep;
using ogsimnet::BindVerdict;
using ogsimnet::RosterAuthority;
using ogsimnet::RosterEntry;
using ogsimnet::RosterMessage;
using ogsimnet::RosterMirror;
using ogsimnet::RosterRecord;
using ogsimnet::RosterRemoveReason;
using ogsimnet::SimNetBindings;
using ogsimnet::kRosterNoSlot;
using ogsimnet::kRosterNotLocal;

constexpr SimConnectionId kMe = 11u;
constexpr SimConnectionId kOther = 22u;

void wire(RosterMirror& mirror, SimNetBindings& bindings, int& unregisters)
{
	mirror.setAddedCallback([&bindings](const RosterEntry& entry) { bindings.onRosterAdded(entry); });
	mirror.setRemovedCallback([&bindings, &unregisters](const RosterEntry& entry) {
		if (bindings.onRosterRemoved(entry.simId))
			++unregisters;
	});
}

bool sameVerdicts(const SimNetBindings& a, const SimNetBindings& b)
{
	for (unsigned simId = 0u; simId < SimNetBindings::kSimIdCount; ++simId)
	{
		const uint8_t id = static_cast<uint8_t>(simId);
		if (!(a.verdict(id, false) == b.verdict(id, false)) || !(a.verdict(id, true) == b.verdict(id, true))
			|| a.isRegistered(id) != b.isRegistered(id))
			return false;
	}
	return true;
}

std::string bindMismatchText(const BindVerdict& verdict, bool engineSaysLocal)
{
	return std::string(ogsimnet::formatRosterBindMismatch(verdict.simId, verdict.isLocal, engineSaysLocal).data());
}
} // namespace

TEST_CASE("SimNetBindings.WaitBeforeTheAddThenReadyWithTheSlotAndLocalIndex", "[NetTransport][SimNetBindings]")
{
	SimNetBindings bindings;
	CHECK(bindings.verdict(3u, true) == BindVerdict{BindStep::Wait, 3u, kRosterNoSlot, kRosterNotLocal, false});
	CHECK(bindings.verdict(4u, false) == BindVerdict{BindStep::Wait, 4u, kRosterNoSlot, kRosterNotLocal, false});
	CHECK(bindings.verdict(0u, false).step == BindStep::Wait);

	bindings.onRosterAdded(RosterEntry{3u, 2u, 1u});
	bindings.onRosterAdded(RosterEntry{4u, 5u, kRosterNotLocal});

	CHECK(bindings.verdict(3u, true) == BindVerdict{BindStep::Ready, 3u, 2u, 1u, true});
	CHECK(bindings.verdict(4u, false) == BindVerdict{BindStep::Ready, 4u, 5u, kRosterNotLocal, false});
	CHECK(bindings.verdict(5u, false).step == BindStep::Wait);
	CHECK_FALSE(bindings.isRegistered(3u));
	CHECK_FALSE(bindings.isRegistered(4u));
}

TEST_CASE("SimNetBindings.RoleMismatchBothWays", "[NetTransport][SimNetBindings]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{3u, 2u, 0u});
	bindings.onRosterAdded(RosterEntry{4u, 5u, kRosterNotLocal});

	const BindVerdict localSeenRemote = bindings.verdict(3u, false);
	CHECK(localSeenRemote == BindVerdict{BindStep::RoleMismatch, 3u, 2u, 0u, true});
	CHECK(bindMismatchText(localSeenRemote, false) == "[Roster.BindMismatch] simId=3 roster=Local engine=Remote");

	const BindVerdict remoteSeenLocal = bindings.verdict(4u, true);
	CHECK(remoteSeenLocal == BindVerdict{BindStep::RoleMismatch, 4u, 5u, kRosterNotLocal, false});
	CHECK(bindMismatchText(remoteSeenLocal, true) == "[Roster.BindMismatch] simId=4 roster=Remote engine=Local");

	CHECK(bindings.verdict(3u, true).step == BindStep::Ready);
	CHECK(bindings.verdict(4u, false).step == BindStep::Ready);
}

TEST_CASE("SimNetBindings.RemoveThenEndPlayUnregistersExactlyOnce", "[NetTransport][SimNetBindings]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{6u, 1u, 0u});
	REQUIRE(bindings.verdict(6u, true).step == BindStep::Ready);
	bindings.markRegistered(6u);
	REQUIRE(bindings.isRegistered(6u));

	int unregisters = 0;
	if (bindings.onRosterRemoved(6u))
		++unregisters;
	CHECK_FALSE(bindings.isRegistered(6u));
	CHECK(bindings.verdict(6u, true).step == BindStep::Wait);
	if (bindings.markUnregistered(6u))
		++unregisters;
	if (bindings.markUnregistered(6u))
		++unregisters;
	CHECK(unregisters == 1);
	CHECK_FALSE(bindings.isRegistered(6u));
}

TEST_CASE("SimNetBindings.EndPlayThenRemoveUnregistersExactlyOnce", "[NetTransport][SimNetBindings]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{6u, 1u, kRosterNotLocal});
	REQUIRE(bindings.verdict(6u, false).step == BindStep::Ready);
	bindings.markRegistered(6u);

	int unregisters = 0;
	if (bindings.markUnregistered(6u))
		++unregisters;
	CHECK_FALSE(bindings.isRegistered(6u));
	CHECK(bindings.verdict(6u, false).step == BindStep::Ready);
	if (bindings.markUnregistered(6u))
		++unregisters;
	if (bindings.onRosterRemoved(6u))
		++unregisters;
	CHECK(unregisters == 1);
	CHECK_FALSE(bindings.isRegistered(6u));
	CHECK(bindings.verdict(6u, false).step == BindStep::Wait);
}

TEST_CASE("SimNetBindings.NeverRegisteredIdsUnregisterNothing", "[NetTransport][SimNetBindings]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{7u, 3u, kRosterNotLocal});
	CHECK_FALSE(bindings.markUnregistered(7u));
	CHECK_FALSE(bindings.markUnregistered(8u));
	CHECK_FALSE(bindings.onRosterRemoved(7u));
	CHECK_FALSE(bindings.onRosterRemoved(8u));
	CHECK(bindings.verdict(7u, false).step == BindStep::Wait);
}

TEST_CASE("SimNetBindings.AnIdAddedAgainAfterItsRemoveGetsTheNewSlot", "[NetTransport][SimNetBindings]")
{
	SECTION("a Remove, then an Add of the same id")
	{
		SimNetBindings bindings;
		bindings.onRosterAdded(RosterEntry{5u, 1u, kRosterNotLocal});
		bindings.markRegistered(5u);
		CHECK(bindings.onRosterRemoved(5u));
		bindings.onRosterAdded(RosterEntry{5u, 6u, 2u});
		CHECK(bindings.verdict(5u, true) == BindVerdict{BindStep::Ready, 5u, 6u, 2u, true});
		CHECK_FALSE(bindings.isRegistered(5u));
		bindings.markRegistered(5u);
		CHECK(bindings.isRegistered(5u));
	}
	SECTION("a second Snapshot that moves the id to another slot, through the mirror")
	{
		RosterMirror mirror;
		SimNetBindings bindings;
		int unregisters = 0;
		wire(mirror, bindings, unregisters);
		const RosterEntry before[] = {RosterEntry{5u, 1u, kRosterNotLocal}};
		REQUIRE(mirror.apply(ogsimnet::rosterCodec::encodeSnapshot(1u, before).view()).applied());
		bindings.markRegistered(5u);

		const RosterEntry after[] = {RosterEntry{5u, 6u, kRosterNotLocal}};
		REQUIRE(mirror.apply(ogsimnet::rosterCodec::encodeSnapshot(1u, after).view()).applied());
		CHECK(unregisters == 1);
		CHECK_FALSE(bindings.isRegistered(5u));
		CHECK(bindings.verdict(5u, false) == BindVerdict{BindStep::Ready, 5u, 6u, kRosterNotLocal, false});
	}
}

TEST_CASE("SimNetBindings.SeedFromEqualsTheIncrementalResult", "[NetTransport][SimNetBindings]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kMe, 0u});
	authority.add(RosterRecord{2u, 1u, kOther, 0u});
	authority.add(RosterRecord{3u, 2u, kMe, 1u});

	std::vector<RosterMessage> script;
	script.push_back(authority.snapshotFor(kMe));
	authority.add(RosterRecord{4u, 3u, kOther, 1u});
	script.push_back(authority.addFor(kMe, 4u));
	authority.add(RosterRecord{5u, 4u, kMe, 2u});
	script.push_back(authority.addFor(kMe, 5u));
	authority.remove(2u, RosterRemoveReason::Left);
	script.push_back(authority.removeFor(2u));
	authority.remove(3u, RosterRemoveReason::LocalPlayerLeft);
	script.push_back(authority.removeFor(3u));

	RosterMirror mirror;
	SimNetBindings incremental;
	int unregisters = 0;
	wire(mirror, incremental, unregisters);

	for (const RosterMessage& message : script)
	{
		REQUIRE(mirror.apply(message.view()).applied());
		SimNetBindings seeded;
		seeded.seedFrom(mirror);
		CHECK(seeded == incremental);
		CHECK(sameVerdicts(seeded, incremental));
	}

	CHECK(incremental.verdict(1u, true) == BindVerdict{BindStep::Ready, 1u, 0u, 0u, true});
	CHECK(incremental.verdict(4u, false) == BindVerdict{BindStep::Ready, 4u, 3u, kRosterNotLocal, false});
	CHECK(incremental.verdict(5u, true) == BindVerdict{BindStep::Ready, 5u, 4u, 2u, true});
	CHECK(incremental.verdict(2u, false).step == BindStep::Wait);
	CHECK(incremental.verdict(3u, true).step == BindStep::Wait);
	CHECK(unregisters == 0);

	SimNetBindings reseeded;
	reseeded.onRosterAdded(RosterEntry{9u, 7u, kRosterNotLocal});
	reseeded.seedFrom(mirror);
	CHECK(reseeded == incremental);
	CHECK(reseeded.verdict(9u, false).step == BindStep::Wait);
}

TEST_CASE("SimNetBindings.Death.AddOfSimIdZeroFiresTheCheck", "[.][OGSimDeath]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{0u, 0u, kRosterNotLocal});
	FAIL("SimNetBindings::onRosterAdded of sim id 0 did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetBindings.Death.AddOfALiveSimIdFiresTheCheck", "[.][OGSimDeath]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{3u, 0u, kRosterNotLocal});
	bindings.onRosterAdded(RosterEntry{3u, 1u, kRosterNotLocal});
	FAIL("SimNetBindings::onRosterAdded of a live sim id did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetBindings.Death.SeedingWithARegisteredIdFiresTheCheck", "[.][OGSimDeath]")
{
	RosterMirror mirror;
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{3u, 0u, kRosterNotLocal});
	bindings.markRegistered(3u);
	bindings.seedFrom(mirror);
	FAIL("SimNetBindings::seedFrom with a registered sim id did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetBindings.Death.RegisteringAnIdWithNoEntryFiresTheCheck", "[.][OGSimDeath]")
{
	SimNetBindings bindings;
	bindings.markRegistered(3u);
	FAIL("SimNetBindings::markRegistered of a sim id with no entry did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetBindings.Death.RegisteringTwiceFiresTheCheck", "[.][OGSimDeath]")
{
	SimNetBindings bindings;
	bindings.onRosterAdded(RosterEntry{3u, 0u, kRosterNotLocal});
	bindings.markRegistered(3u);
	bindings.markRegistered(3u);
	FAIL("SimNetBindings::markRegistered of a registered sim id did NOT fire its OG_CHECK");
}
