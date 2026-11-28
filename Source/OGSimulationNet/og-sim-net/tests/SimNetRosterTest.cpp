// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimNet/SimNetRoster.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using ogsimnet::DecodedRoster;
using ogsimnet::RosterAddRefusal;
using ogsimnet::RosterApplyResult;
using ogsimnet::RosterAuthority;
using ogsimnet::RosterEntry;
using ogsimnet::RosterInputVerdict;
using ogsimnet::RosterKind;
using ogsimnet::RosterMessage;
using ogsimnet::RosterMirror;
using ogsimnet::RosterRecord;
using ogsimnet::RosterRefusal;
using ogsimnet::RosterRemoveReason;
using ogsimnet::kRosterNoSlot;
using ogsimnet::kRosterNotLocal;
namespace rosterCodec = ogsimnet::rosterCodec;

constexpr SimConnectionId kConnA = 11u;
constexpr SimConnectionId kConnB = 22u;
constexpr SimConnectionId kConnC = 33u;

std::vector<std::byte> bytesOf(std::initializer_list<uint8_t> values)
{
	std::vector<std::byte> out;
	for (const uint8_t value : values)
	{
		out.push_back(static_cast<std::byte>(value));
	}
	return out;
}

std::vector<uint8_t> u8Of(std::span<const std::byte> bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(std::to_integer<uint8_t>(b));
	}
	return out;
}

uint32_t packed(const RosterEntry& entry)
{
	return (static_cast<uint32_t>(entry.simId) << 16u) | (static_cast<uint32_t>(entry.physicsSlot) << 8u) | entry.localIndex;
}

std::vector<uint32_t> packedAll(std::span<const RosterEntry> entries)
{
	std::vector<uint32_t> out;
	for (const RosterEntry& entry : entries)
	{
		out.push_back(packed(entry));
	}
	return out;
}

std::string refusalName(RosterRefusal refusal) { return ogsimnet::rosterRefusalName(refusal); }

template <size_t N>
constexpr bool messageEquals(const RosterMessage& message, const std::array<uint8_t, N>& expected)
{
	if (message.size != N)
	{
		return false;
	}
	for (size_t i = 0; i < N; ++i)
	{
		if (std::to_integer<uint8_t>(message.bytes[i]) != expected[i])
		{
			return false;
		}
	}
	return true;
}

constexpr std::array<uint8_t, 11> kGoldenSnapshot{0x01, 0x01, 0x05, 0x00, 0x02, 0x01, 0x00, 0xFF, 0x02, 0x01, 0x00};
constexpr std::array<uint8_t, 7> kGoldenAdd{0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x01};
constexpr std::array<uint8_t, 6> kGoldenRemove{0x01, 0x03, 0x07, 0x00, 0x02, 0x01};
constexpr std::array<RosterEntry, 2> kGoldenSnapshotEntries{RosterEntry{1u, 0u, kRosterNotLocal}, RosterEntry{2u, 1u, 0u}};

RosterApplyResult applyMessage(RosterMirror& mirror, const RosterMessage& message) { return mirror.apply(message.view()); }

RosterApplyResult applyBytes(RosterMirror& mirror, std::initializer_list<uint8_t> values)
{
	const std::vector<std::byte> bytes = bytesOf(values);
	return mirror.apply(bytes);
}

struct CallbackLog
{
	std::vector<uint32_t> added;
	std::vector<uint32_t> removed;

	void attach(RosterMirror& mirror)
	{
		mirror.setAddedCallback([this](const RosterEntry& entry) { added.push_back(packed(entry)); });
		mirror.setRemovedCallback([this](const RosterEntry& entry) { removed.push_back(packed(entry)); });
	}
};

void addThreeConnectionsTwoLocalPlayers(RosterAuthority& authority)
{
	const std::array<SimConnectionId, 3> conns{kConnA, kConnB, kConnC};
	uint8_t simId = 1u;
	for (size_t c = 0; c < conns.size(); ++c)
	{
		for (uint8_t local = 0u; local < 2u; ++local)
		{
			authority.add(RosterRecord{simId, static_cast<uint8_t>(simId - 1u), conns[c], local});
			++simId;
		}
	}
}
} // namespace

TEST_CASE("SimNetRoster.GoldenVectorsEncodeByteForByte", "[NetTransport][SimNetRoster]")
{
	STATIC_CHECK(messageEquals(rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries), kGoldenSnapshot));
	STATIC_CHECK(messageEquals(rosterCodec::encodeAdd(6u, RosterEntry{3u, 2u, 1u}), kGoldenAdd));
	STATIC_CHECK(messageEquals(rosterCodec::encodeRemove(7u, 2u, RosterRemoveReason::Left), kGoldenRemove));

	CHECK(u8Of(rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries).view())
		== std::vector<uint8_t>(kGoldenSnapshot.begin(), kGoldenSnapshot.end()));
	CHECK(u8Of(rosterCodec::encodeAdd(6u, RosterEntry{3u, 2u, 1u}).view()) == std::vector<uint8_t>(kGoldenAdd.begin(), kGoldenAdd.end()));
	CHECK(u8Of(rosterCodec::encodeRemove(7u, 2u, RosterRemoveReason::Left).view())
		== std::vector<uint8_t>(kGoldenRemove.begin(), kGoldenRemove.end()));

	STATIC_CHECK(ogsimnet::kRosterWireVersion == 1u);
	STATIC_CHECK(rosterCodec::encodeSnapshot(0u, std::span<const RosterEntry>{}).size == 5u);
}

TEST_CASE("SimNetRoster.GoldenVectorsDecode", "[NetTransport][SimNetRoster]")
{
	const std::vector<std::byte> snapshotBytes = bytesOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x01, 0x00, 0xFF, 0x02, 0x01, 0x00});
	const DecodedRoster snapshot = rosterCodec::decode(snapshotBytes);
	CHECK(refusalName(snapshot.refusal) == "None");
	CHECK(snapshot.kind == RosterKind::Snapshot);
	CHECK(snapshot.revision == 5u);
	CHECK(packedAll(snapshot.view.span()) == packedAll(kGoldenSnapshotEntries));

	const std::vector<std::byte> addBytes = bytesOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x01});
	const DecodedRoster add = rosterCodec::decode(addBytes);
	CHECK(refusalName(add.refusal) == "None");
	CHECK(add.kind == RosterKind::Add);
	CHECK(add.revision == 6u);
	REQUIRE(add.view.count == 1u);
	CHECK(packed(add.view.entries[0]) == packed(RosterEntry{3u, 2u, 1u}));

	const std::vector<std::byte> removeBytes = bytesOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x01});
	const DecodedRoster remove = rosterCodec::decode(removeBytes);
	CHECK(refusalName(remove.refusal) == "None");
	CHECK(remove.kind == RosterKind::Remove);
	CHECK(remove.revision == 7u);
	CHECK(remove.removedSimId == 2u);
	CHECK(remove.removeReason == RosterRemoveReason::Left);
}

TEST_CASE("SimNetRoster.TheSpecsRefusedVectorsAreRefused", "[NetTransport][SimNetRoster]")
{
	const auto refusalOf = [](std::initializer_list<uint8_t> values) {
		const std::vector<std::byte> bytes = bytesOf(values);
		return refusalName(rosterCodec::decode(bytes).refusal);
	};
	CHECK(refusalOf({0x02, 0x01, 0x05, 0x00, 0x00}) == "Version");
	CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0xFF}) == "SimIdZero");
	CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x08, 0x01}) == "Slot");
	CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02}) == "Truncated");
}

TEST_CASE("SimNetRoster.EveryRefusalOfTheSpecIsRefused", "[NetTransport][SimNetRoster]")
{
	const auto refusalOf = [](std::initializer_list<uint8_t> values) {
		const std::vector<std::byte> bytes = bytesOf(values);
		return refusalName(rosterCodec::decode(bytes).refusal);
	};

	SECTION("a version other than 1")
	{
		CHECK(refusalOf({0x00, 0x02, 0x06, 0x00, 0x03, 0x02, 0x01}) == "Version");
		CHECK(refusalOf({0x02}) == "Version");
	}
	SECTION("an unknown kind")
	{
		CHECK(refusalOf({0x01, 0x00, 0x06, 0x00, 0x03, 0x02, 0x01}) == "Kind");
		CHECK(refusalOf({0x01, 0x04, 0x06, 0x00, 0x03, 0x02, 0x01}) == "Kind");
		CHECK(refusalOf({0x01, 0xFF, 0x06, 0x00}) == "Kind");
	}
	SECTION("a truncated message, each kind")
	{
		CHECK(refusalOf({}) == "Truncated");
		CHECK(refusalOf({0x01, 0x01, 0x05}) == "Truncated");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00}) == "Truncated");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x01, 0x00, 0xFF, 0x02, 0x01}) == "Truncated");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02}) == "Truncated");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00}) == "Truncated");
	}
	SECTION("an overlong message, each kind")
	{
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x00, 0x00}) == "Overlong");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x01, 0x01, 0x00, 0xFF, 0x00}) == "Overlong");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x01, 0x00}) == "Overlong");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x01, 0x00}) == "Overlong");
	}
	SECTION("sim id 0, in each kind")
	{
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x00, 0x02, 0x01}) == "SimIdZero");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x00, 0x01}) == "SimIdZero");
	}
	SECTION("a duplicate sim id in a Snapshot")
	{
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x04, 0x00, 0xFF, 0x04, 0x01, 0xFF}) == "DuplicateSimId");
	}
	SECTION("a slot of 8 or more that is not 0xFF; 0xFF is accepted")
	{
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x01, 0x01, 0x08, 0xFF}) == "Slot");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0xFE, 0x01}) == "Slot");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x07, 0x01}) == "None");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0xFF, 0x01}) == "None");
	}
	SECTION("a local index above 7 that is not 0xFF; 0xFF is accepted")
	{
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x08}) == "LocalIndex");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x01, 0x01, 0x00, 0xFE}) == "LocalIndex");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x07}) == "None");
		CHECK(refusalOf({0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0xFF}) == "None");
	}
	SECTION("a count above 8, even with the bytes for it")
	{
		std::vector<uint8_t> values{0x01, 0x01, 0x05, 0x00, 0x09};
		for (uint8_t simId = 1u; simId <= 9u; ++simId)
		{
			values.push_back(simId);
			values.push_back(0xFF);
			values.push_back(0xFF);
		}
		std::vector<std::byte> bytes;
		for (const uint8_t value : values)
		{
			bytes.push_back(static_cast<std::byte>(value));
		}
		CHECK(refusalName(rosterCodec::decode(bytes).refusal) == "Count");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0xFF}) == "Count");
	}
	SECTION("additions: a Snapshot out of ascending order, a shared slot or local index, an unknown Remove reason")
	{
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x04, 0x00, 0xFF, 0x02, 0x01, 0xFF}) == "SnapshotOrder");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x02, 0x03, 0xFF, 0x04, 0x03, 0xFF}) == "DuplicateSlot");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x02, 0xFF, 0xFF, 0x04, 0xFF, 0xFF}) == "None");
		CHECK(refusalOf({0x01, 0x01, 0x05, 0x00, 0x02, 0x02, 0x00, 0x01, 0x04, 0x01, 0x01}) == "DuplicateLocalIndex");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x00}) == "RemoveReason");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x04}) == "RemoveReason");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x02}) == "None");
		CHECK(refusalOf({0x01, 0x03, 0x07, 0x00, 0x02, 0x03}) == "None");
	}
}

TEST_CASE("SimNetRoster.TheMirrorRefusesStatefulCasesAndNeverAppliesPartially", "[NetTransport][SimNetRoster]")
{
	RosterMirror mirror;
	CallbackLog log;
	log.attach(mirror);

	SECTION("a delta before any Snapshot")
	{
		CHECK(refusalName(applyBytes(mirror, {0x01, 0x02, 0x06, 0x00, 0x03, 0x02, 0x01}).refusal) == "NotAttached");
		CHECK(refusalName(applyBytes(mirror, {0x01, 0x03, 0x07, 0x00, 0x02, 0x01}).refusal) == "NotAttached");
		CHECK_FALSE(mirror.attached());
		CHECK(mirror.entries().empty());
	}

	REQUIRE(applyMessage(mirror, rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries)).applied());
	const std::vector<uint32_t> attachedEntries = packedAll(mirror.entries());
	log.added.clear();

	SECTION("an Add whose sim id is live")
	{
		CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeAdd(6u, RosterEntry{2u, 5u, kRosterNotLocal})).refusal) == "AddLive");
	}
	SECTION("a Remove whose sim id is not live")
	{
		CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeRemove(6u, 9u, RosterRemoveReason::Despawned)).refusal)
			== "RemoveNotLive");
	}
	SECTION("an Add into a slot or a local index a live entry holds")
	{
		CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeAdd(6u, RosterEntry{3u, 1u, kRosterNotLocal})).refusal)
			== "DuplicateSlot");
		CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeAdd(6u, RosterEntry{3u, 4u, 0u})).refusal) == "DuplicateLocalIndex");
	}
	SECTION("a refused decode")
	{
		CHECK(refusalName(applyBytes(mirror, {0x01, 0x02, 0x06, 0x00, 0x03, 0x08, 0x01}).refusal) == "Slot");
	}
	SECTION("a refused Snapshot leaves the old one in place")
	{
		CHECK(refusalName(applyBytes(mirror, {0x01, 0x01, 0x09, 0x00, 0x01, 0x07, 0x00, 0xFF, 0x00}).refusal) == "Overlong");
		CHECK(mirror.revision() == 5u);
	}

	CHECK(packedAll(mirror.entries()) == attachedEntries);
	CHECK(mirror.revision() == 5u);
	CHECK(log.added.empty());
	CHECK(log.removed.empty());
}

TEST_CASE("SimNetRoster.ARevisionGapIsRefused", "[NetTransport][SimNetRoster]")
{
	RosterMirror mirror;
	REQUIRE(applyMessage(mirror, rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries)).applied());

	CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeAdd(7u, RosterEntry{3u, 2u, 1u})).refusal) == "RevisionGap");
	CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeAdd(5u, RosterEntry{3u, 2u, 1u})).refusal) == "RevisionGap");
	CHECK(refusalName(applyMessage(mirror, rosterCodec::encodeRemove(7u, 2u, RosterRemoveReason::Left)).refusal) == "RevisionGap");
	CHECK(mirror.entryFor(3u) == nullptr);
	CHECK(mirror.revision() == 5u);

	CHECK(applyMessage(mirror, rosterCodec::encodeAdd(6u, RosterEntry{3u, 2u, 1u})).applied());
	CHECK(applyMessage(mirror, rosterCodec::encodeRemove(7u, 2u, RosterRemoveReason::Left)).applied());
	CHECK(mirror.revision() == 7u);

	SECTION("the revision wraps from 65535 to 0")
	{
		RosterMirror wrapping;
		REQUIRE(applyMessage(wrapping, rosterCodec::encodeSnapshot(0xFFFFu, std::span<const RosterEntry>{})).applied());
		CHECK(refusalName(applyMessage(wrapping, rosterCodec::encodeAdd(1u, RosterEntry{4u, 0u, 0u})).refusal) == "RevisionGap");
		CHECK(applyMessage(wrapping, rosterCodec::encodeAdd(0u, RosterEntry{4u, 0u, 0u})).applied());
		CHECK(wrapping.revision() == 0u);
	}
}

TEST_CASE("SimNetRoster.EachRecipientGetsItsOwnView", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	addThreeConnectionsTwoLocalPlayers(authority);
	REQUIRE(authority.revision() == 6u);

	const auto expectedSnapshot = [](uint8_t firstOwnSimId) {
		std::vector<uint8_t> out{0x01, 0x01, 0x06, 0x00, 0x06};
		for (uint8_t simId = 1u; simId <= 6u; ++simId)
		{
			out.push_back(simId);
			out.push_back(static_cast<uint8_t>(simId - 1u));
			const bool own = simId == firstOwnSimId || simId == firstOwnSimId + 1u;
			out.push_back(own ? static_cast<uint8_t>(simId - firstOwnSimId) : 0xFFu);
		}
		return out;
	};

	CHECK(u8Of(authority.snapshotFor(kConnA).view()) == expectedSnapshot(1u));
	CHECK(u8Of(authority.snapshotFor(kConnB).view()) == expectedSnapshot(3u));
	CHECK(u8Of(authority.snapshotFor(kConnC).view()) == expectedSnapshot(5u));

	const std::vector<uint8_t> strangerSnapshot = u8Of(authority.snapshotFor(44u).view());
	for (size_t offset = 5u + 2u; offset < strangerSnapshot.size(); offset += 3u)
	{
		CHECK(strangerSnapshot[offset] == 0xFFu);
	}

	CHECK(u8Of(authority.addFor(kConnB, 4u).view()) == std::vector<uint8_t>{0x01, 0x02, 0x04, 0x00, 0x04, 0x03, 0x01});
	CHECK(u8Of(authority.addFor(kConnA, 4u).view()) == std::vector<uint8_t>{0x01, 0x02, 0x04, 0x00, 0x04, 0x03, 0xFF});
	CHECK(u8Of(authority.addFor(kConnC, 4u).view()) == std::vector<uint8_t>{0x01, 0x02, 0x04, 0x00, 0x04, 0x03, 0xFF});

	for (const SimConnectionId conn : {kConnA, kConnB, kConnC})
	{
		RosterMirror mirror;
		REQUIRE(applyMessage(mirror, authority.snapshotFor(conn)).applied());
		size_t localCount = 0u;
		for (const RosterEntry& entry : mirror.entries())
		{
			localCount += entry.isLocal() ? 1u : 0u;
			CHECK(entry.isLocal() == (authority.ownerOf(entry.simId) == conn));
		}
		CHECK(localCount == 2u);
	}
}

TEST_CASE("SimNetRoster.SnapshotThenAddThenRemoveLeavesTheMirrorEqualToTheAuthoritysView", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.add(RosterRecord{2u, 1u, kConnB, 0u});
	authority.add(RosterRecord{3u, kRosterNoSlot, std::nullopt, kRosterNotLocal});

	RosterMirror mirror;
	CallbackLog log;
	log.attach(mirror);

	REQUIRE(applyMessage(mirror, authority.snapshotFor(kConnA)).applied());
	CHECK(packedAll(mirror.entries()) == packedAll(authority.viewFor(kConnA).span()));
	CHECK(log.added == packedAll(authority.viewFor(kConnA).span()));
	CHECK(mirror.revision() == authority.revision());

	authority.add(RosterRecord{4u, 2u, kConnA, 1u});
	const RosterApplyResult added = applyMessage(mirror, authority.addFor(kConnA, 4u));
	REQUIRE(added.applied());
	CHECK(added.kind == RosterKind::Add);
	CHECK(packedAll(mirror.entries()) == packedAll(authority.viewFor(kConnA).span()));
	CHECK(log.added.back() == packed(RosterEntry{4u, 2u, 1u}));
	REQUIRE(mirror.entryFor(4u) != nullptr);
	CHECK(mirror.entryFor(4u)->isLocal());

	authority.remove(2u, RosterRemoveReason::Left);
	const RosterApplyResult removed = applyMessage(mirror, authority.removeFor(2u));
	REQUIRE(removed.applied());
	CHECK(removed.kind == RosterKind::Remove);
	CHECK(packedAll(mirror.entries()) == packedAll(authority.viewFor(kConnA).span()));
	CHECK(log.removed == std::vector<uint32_t>{packed(RosterEntry{2u, 1u, kRosterNotLocal})});
	CHECK(mirror.entryFor(2u) == nullptr);
	CHECK(mirror.revision() == authority.revision());
	CHECK(authority.revision() == 5u);
	CHECK(log.added.size() == 4u);
}

TEST_CASE("SimNetRoster.TheAuthorityKeepsOwnersForTheInputCheck", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.add(RosterRecord{2u, 1u, kConnA, 1u});
	authority.add(RosterRecord{3u, 2u, kConnB, 0u});
	authority.add(RosterRecord{4u, 3u, std::nullopt, 0u});

	CHECK((authority.ownerOf(2u) == kConnA));
	CHECK((authority.localIndexOf(2u) == uint8_t{1u}));
	CHECK((authority.ownerOf(3u) == kConnB));
	CHECK_FALSE(authority.ownerOf(4u).has_value());
	CHECK_FALSE(authority.localIndexOf(4u).has_value());
	CHECK_FALSE(authority.ownerOf(9u).has_value());

	CHECK(authority.checkInput(kConnA, 2u) == RosterInputVerdict::Accepted);
	CHECK(authority.checkInput(kConnB, 2u) == RosterInputVerdict::NotOwner);
	CHECK(authority.checkInput(kConnA, 4u) == RosterInputVerdict::NotOwner);
	CHECK(authority.checkInput(kConnA, 9u) == RosterInputVerdict::UnknownSim);

	authority.remove(1u, RosterRemoveReason::LocalPlayerLeft);
	CHECK(authority.checkInput(kConnA, 1u) == RosterInputVerdict::UnknownSim);
	CHECK_FALSE(authority.ownerOf(1u).has_value());
	CHECK(authority.liveCount() == 3u);
	CHECK(u8Of(authority.removeFor(1u).view()) == std::vector<uint8_t>{0x01, 0x03, 0x05, 0x00, 0x01, 0x02});

	for (const RosterEntry& entry : authority.viewFor(kConnC).span())
	{
		CHECK_FALSE(entry.isLocal());
	}
	REQUIRE(authority.recordFor(4u) != nullptr);
	CHECK_FALSE(authority.recordFor(4u)->owner.has_value());
}

TEST_CASE("SimNetRoster.ASlotIsReusedAfterItsRemoveIsQueuedAndRevisionsCountEveryAddAndRemove", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.add(RosterRecord{2u, 1u, kConnB, 0u});
	CHECK(authority.revision() == 2u);
	CHECK(u8Of(authority.addFor(kConnA, 1u).view()) == std::vector<uint8_t>{0x01, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00});

	authority.remove(1u, RosterRemoveReason::Left);
	CHECK(authority.revision() == 3u);
	CHECK(u8Of(authority.removeFor(1u).view()) == std::vector<uint8_t>{0x01, 0x03, 0x03, 0x00, 0x01, 0x01});

	authority.add(RosterRecord{3u, 0u, kConnC, 0u});
	CHECK(authority.revision() == 4u);
	CHECK(u8Of(authority.addFor(kConnC, 3u).view()) == std::vector<uint8_t>{0x01, 0x02, 0x04, 0x00, 0x03, 0x00, 0x00});

	CHECK(u8Of(authority.snapshotFor(kConnB).view())
		== std::vector<uint8_t>{0x01, 0x01, 0x04, 0x00, 0x02, 0x02, 0x01, 0x00, 0x03, 0x00, 0xFF});
	CHECK(authority.revision() == 4u);
}

TEST_CASE("SimNetRoster.ASecondSnapshotReplacesTheMirrorAndReportsTheDifference", "[NetTransport][SimNetRoster]")
{
	RosterMirror mirror;
	CallbackLog log;
	log.attach(mirror);
	REQUIRE(applyMessage(mirror, rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries)).applied());
	log.added.clear();

	const std::array<RosterEntry, 2> next{RosterEntry{2u, 1u, 0u}, RosterEntry{5u, 4u, kRosterNotLocal}};
	REQUIRE(applyMessage(mirror, rosterCodec::encodeSnapshot(9u, next)).applied());
	CHECK(log.removed == std::vector<uint32_t>{packed(RosterEntry{1u, 0u, kRosterNotLocal})});
	CHECK(log.added == std::vector<uint32_t>{packed(RosterEntry{5u, 4u, kRosterNotLocal})});
	CHECK(packedAll(mirror.entries()) == packedAll(next));
	CHECK(mirror.revision() == 9u);
}

TEST_CASE("SimNetRoster.CallbacksSeeTheStateAfterTheMessage", "[NetTransport][SimNetRoster]")
{
	RosterMirror mirror;
	std::vector<bool> addedVisible;
	std::vector<bool> removedVisible;
	std::vector<uint16_t> revisions;
	mirror.setAddedCallback([&](const RosterEntry& entry) {
		addedVisible.push_back(mirror.entryFor(entry.simId) != nullptr);
		revisions.push_back(mirror.revision());
	});
	mirror.setRemovedCallback([&](const RosterEntry& entry) {
		removedVisible.push_back(mirror.entryFor(entry.simId) != nullptr);
		revisions.push_back(mirror.revision());
	});

	REQUIRE(applyMessage(mirror, rosterCodec::encodeSnapshot(5u, kGoldenSnapshotEntries)).applied());
	REQUIRE(applyMessage(mirror, rosterCodec::encodeAdd(6u, RosterEntry{3u, 2u, 1u})).applied());
	REQUIRE(applyMessage(mirror, rosterCodec::encodeRemove(7u, 2u, RosterRemoveReason::Left)).applied());

	CHECK(addedVisible == std::vector<bool>{true, true, true});
	CHECK(removedVisible == std::vector<bool>{false});
	CHECK(revisions == std::vector<uint16_t>{5u, 5u, 6u, 7u});

	mirror.clearAddedCallback();
	mirror.clearRemovedCallback();
	REQUIRE(applyMessage(mirror, rosterCodec::encodeAdd(8u, RosterEntry{4u, 3u, kRosterNotLocal})).applied());
	CHECK(addedVisible.size() == 3u);
}

TEST_CASE("SimNetRoster.TagTexts", "[NetTransport][SimNetRoster]")
{
	CHECK(std::string(ogsimnet::formatRosterInputRefused(kConnB, 3u, RosterInputVerdict::NotOwner).data())
		== "[Roster.InputRefused] conn=22 simId=3 reason=NotOwner");
	CHECK(std::string(ogsimnet::formatRosterInputRefused(4294967295u, 255u, RosterInputVerdict::UnknownSim).data())
		== "[Roster.InputRefused] conn=4294967295 simId=255 reason=UnknownSim");
	CHECK(std::string(ogsimnet::formatRosterRefused(RosterRefusal::RevisionGap).data()) == "[Roster.Refused] reason=RevisionGap");
	CHECK(std::string(ogsimnet::formatRosterRefused(RosterRefusal::DuplicateLocalIndex).data())
		== "[Roster.Refused] reason=DuplicateLocalIndex");
	CHECK(std::string(ogsimnet::formatRosterBindMismatch(7u, true, false).data()) == "[Roster.BindMismatch] simId=7 roster=Local engine=Remote");
	STATIC_CHECK(std::string_view(ogsimnet::rosterAddRefusalName(RosterAddRefusal::None)) == "None");
	STATIC_CHECK(std::string_view(ogsimnet::rosterAddRefusalName(RosterAddRefusal::Full)) == "Full");
	STATIC_CHECK(std::string_view(ogsimnet::rosterAddRefusalName(RosterAddRefusal::DuplicateOwnerSlot)) == "DuplicateOwnerSlot");
}

TEST_CASE("SimNetRoster.AnAddPastCapacityIsRefusedAsFullAndChangesNothing", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	REQUIRE(authority.add(RosterRecord{1u, 0u, kConnA, 0u}) == RosterAddRefusal::None);
	for (uint8_t simId = 2u; simId <= 8u; ++simId)
	{
		REQUIRE(authority.add(RosterRecord{simId, static_cast<uint8_t>(simId - 1u), std::nullopt, kRosterNotLocal})
			== RosterAddRefusal::None);
	}
	REQUIRE(authority.liveCount() == 8u);
	REQUIRE(authority.revision() == 8u);
	const std::vector<uint8_t> snapshotBefore = u8Of(authority.snapshotFor(kConnA).view());
	RosterMirror mirror;
	REQUIRE(applyMessage(mirror, authority.snapshotFor(kConnA)).applied());

	CHECK(authority.add(RosterRecord{9u, kRosterNoSlot, std::nullopt, kRosterNotLocal}) == RosterAddRefusal::Full);
	CHECK(authority.add(RosterRecord{10u, kRosterNoSlot, kConnA, 1u}) == RosterAddRefusal::Full);
	CHECK(authority.add(RosterRecord{11u, kRosterNoSlot, kConnA, 0u}) == RosterAddRefusal::DuplicateOwnerSlot);
	CHECK(authority.liveCount() == 8u);
	CHECK(authority.revision() == 8u);
	CHECK(authority.recordFor(9u) == nullptr);
	CHECK(authority.recordFor(10u) == nullptr);
	CHECK(authority.recordFor(11u) == nullptr);
	CHECK(authority.checkInput(kConnA, 10u) == RosterInputVerdict::UnknownSim);
	CHECK((authority.ownerOf(1u) == kConnA));
	CHECK(u8Of(authority.snapshotFor(kConnA).view()) == snapshotBefore);

	authority.remove(8u, RosterRemoveReason::Despawned);
	CHECK(authority.add(RosterRecord{9u, kRosterNoSlot, std::nullopt, kRosterNotLocal}) == RosterAddRefusal::Full);
	CHECK(authority.revision() == 9u);
	REQUIRE(applyMessage(mirror, authority.removeFor(8u)).applied());
	CHECK(authority.add(RosterRecord{9u, kRosterNoSlot, std::nullopt, kRosterNotLocal}) == RosterAddRefusal::None);
	CHECK(authority.revision() == 10u);
	CHECK(applyMessage(mirror, authority.addFor(kConnA, 9u)).applied());
	CHECK(packedAll(mirror.entries()) == packedAll(authority.viewFor(kConnA).span()));
	CHECK(authority.liveCount() == 8u);
}

TEST_CASE("SimNetRoster.ASecondLiveOwnerAndLocalIndexIsRefusedAndChangesNothing", "[NetTransport][SimNetRoster]")
{
	RosterAuthority authority;
	REQUIRE(authority.add(RosterRecord{1u, 0u, kConnA, 0u}) == RosterAddRefusal::None);
	REQUIRE(authority.add(RosterRecord{2u, 1u, kConnB, 0u}) == RosterAddRefusal::None);
	REQUIRE(authority.add(RosterRecord{3u, kRosterNoSlot, std::nullopt, 0u}) == RosterAddRefusal::None);
	const std::vector<uint8_t> snapshotBefore = u8Of(authority.snapshotFor(kConnA).view());
	const std::vector<uint8_t> strangerBefore = u8Of(authority.snapshotFor(kConnC).view());

	CHECK(authority.add(RosterRecord{4u, 2u, kConnA, 0u}) == RosterAddRefusal::DuplicateOwnerSlot);
	CHECK(authority.add(RosterRecord{5u, kRosterNoSlot, kConnB, 0u}) == RosterAddRefusal::DuplicateOwnerSlot);
	CHECK(authority.liveCount() == 3u);
	CHECK(authority.revision() == 3u);
	CHECK(authority.recordFor(4u) == nullptr);
	CHECK(authority.recordFor(5u) == nullptr);
	CHECK(authority.checkInput(kConnA, 4u) == RosterInputVerdict::UnknownSim);
	CHECK(authority.checkInput(kConnA, 1u) == RosterInputVerdict::Accepted);
	CHECK((authority.localIndexOf(1u) == uint8_t{0u}));
	CHECK(u8Of(authority.snapshotFor(kConnA).view()) == snapshotBefore);
	CHECK(u8Of(authority.snapshotFor(kConnC).view()) == strangerBefore);

	CHECK(authority.add(RosterRecord{4u, 2u, kConnA, 1u}) == RosterAddRefusal::None);
	CHECK(authority.add(RosterRecord{5u, 3u, kConnC, 0u}) == RosterAddRefusal::None);
	CHECK(authority.add(RosterRecord{6u, kRosterNoSlot, std::nullopt, 0u}) == RosterAddRefusal::None);
	CHECK(authority.revision() == 6u);

	authority.remove(1u, RosterRemoveReason::LocalPlayerLeft);
	(void)authority.removeFor(1u);
	CHECK(authority.add(RosterRecord{7u, kRosterNoSlot, kConnA, 0u}) == RosterAddRefusal::None);
	CHECK(authority.checkInput(kConnA, 7u) == RosterInputVerdict::Accepted);
	CHECK(authority.revision() == 8u);
	CHECK(authority.liveCount() == 6u);
}

TEST_CASE("SimNetRoster.Death.ASlotReusedBeforeItsRemoveIsQueuedFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.remove(1u, RosterRemoveReason::Left);
	authority.add(RosterRecord{2u, 0u, kConnB, 0u});
	FAIL("RosterAuthority::add into a slot whose Remove is not queued did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.ASnapshotWithARemovePendingFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.remove(1u, RosterRemoveReason::Left);
	(void)authority.snapshotFor(kConnB);
	FAIL("RosterAuthority::snapshotFor with a Remove pending did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AReusedSimIdFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.remove(1u, RosterRemoveReason::Left);
	(void)authority.removeFor(1u);
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	FAIL("RosterAuthority::add of a sim id used before did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddOfSimIdZeroFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{0u, 0u, kConnA, 0u});
	FAIL("RosterAuthority::add of sim id 0 did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddOfSlotEightFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 8u, kConnA, 0u});
	FAIL("RosterAuthority::add of slot 8 did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddOfLocalIndexEightFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 8u});
	FAIL("RosterAuthority::add of local index 8 did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddOwnedByTheInvalidConnectionFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kInvalidSimConnection, 0u});
	FAIL("RosterAuthority::add owned by kInvalidSimConnection did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddOwnedWithoutALocalIndexFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, kRosterNotLocal});
	FAIL("RosterAuthority::add owned with no local index did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddIntoALiveSlotFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	authority.add(RosterRecord{2u, 0u, kConnB, 0u});
	FAIL("RosterAuthority::add into a live entry's slot did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.RemoveOfAnUnknownSimIdFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.remove(3u, RosterRemoveReason::Left);
	FAIL("RosterAuthority::remove of an unknown sim id did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.AddForAnUnknownSimIdFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	(void)authority.addFor(kConnA, 3u);
	FAIL("RosterAuthority::addFor an unknown sim id did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.RemoveForWithNoRemovePendingFiresTheCheck", "[.][OGSimDeath]")
{
	RosterAuthority authority;
	authority.add(RosterRecord{1u, 0u, kConnA, 0u});
	(void)authority.removeFor(1u);
	FAIL("RosterAuthority::removeFor with no Remove pending did NOT fire its OG_CHECK");
}

TEST_CASE("SimNetRoster.Death.FormattingAnAcceptedVerdictFiresTheCheck", "[.][OGSimDeath]")
{
	(void)ogsimnet::formatRosterInputRefused(kConnA, 1u, RosterInputVerdict::Accepted);
	FAIL("formatRosterInputRefused with Accepted did NOT fire its OG_CHECK");
}
