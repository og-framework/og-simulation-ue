#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetRoster-rationale.md · docs/SimNetRoster-guards.md

#include "OGSimulation/BodySlotOccupancy.h"
#include "OGSimulation/Network/ConnectionSlotKey.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <span>
#include <utility>

namespace ogsimnet
{
inline constexpr uint8_t kRosterWireVersion = 1u;
inline constexpr uint8_t kRosterNoSlot = 0xFFu;
inline constexpr uint8_t kRosterNotLocal = 0xFFu;
inline constexpr uint8_t kRosterMaxLocalIndex = 7u;
inline constexpr size_t kRosterMaxEntries = kMaxSimulatableSlots;
inline constexpr size_t kRosterHeaderBytes = 4u;
inline constexpr size_t kRosterEntryBytes = 3u;
inline constexpr size_t kRosterSnapshotBaseBytes = kRosterHeaderBytes + 1u;
inline constexpr size_t kRosterAddBytes = kRosterHeaderBytes + kRosterEntryBytes;
inline constexpr size_t kRosterRemoveBytes = kRosterHeaderBytes + 2u;
inline constexpr size_t kRosterMaxMessageBytes = kRosterSnapshotBaseBytes + kRosterMaxEntries * kRosterEntryBytes;

enum class RosterKind : uint8_t { Snapshot = 1, Add = 2, Remove = 3 };

enum class RosterRemoveReason : uint8_t { Left = 1, LocalPlayerLeft = 2, Despawned = 3 };

enum class RosterRefusal : uint8_t
{
	None,
	Version,
	Kind,
	Truncated,
	Overlong,
	Count,
	SimIdZero,
	DuplicateSimId,
	SnapshotOrder,
	Slot,
	LocalIndex,
	DuplicateSlot,
	DuplicateLocalIndex,
	RemoveReason,
	NotAttached,
	RevisionGap,
	AddLive,
	RemoveNotLive
};

enum class RosterInputVerdict : uint8_t { Accepted, UnknownSim, NotOwner };

static_assert(kRosterWireVersion == simEnvelope::roster::kWireVersion
		&& kRosterHeaderBytes == simEnvelope::roster::kHeaderBytes
		&& kRosterHeaderBytes == simEnvelope::roster::kSnapshotCountOffset
		&& kRosterEntryBytes == simEnvelope::roster::kEntryBytes
		&& kRosterRemoveBytes == kRosterHeaderBytes + simEnvelope::roster::kRemoveBodyBytes
		&& static_cast<uint8_t>(RosterKind::Snapshot) == simEnvelope::roster::kSnapshot
		&& static_cast<uint8_t>(RosterKind::Add) == simEnvelope::roster::kAdd
		&& static_cast<uint8_t>(RosterKind::Remove) == simEnvelope::roster::kRemove,
	"SimNetRoster: the roster layout equals the framing simEnvelope::roster restates (version, header, kinds, entry and "
	"Remove sizes), so the envelope's length check and this decoder agree on every message. Was SimNetRoster-rationale.md §2");
static_assert(kRosterMaxMessageBytes == 29u && kRosterAddBytes == 7u && kRosterRemoveBytes == 6u,
	"SimNetRoster: a full Snapshot is 29 B, an Add 7 B and a Remove 6 B. Was SimNetRoster-rationale.md §2's sizes");
static_assert(simEnvelope::kHeaderBytes + kRosterMaxMessageBytes <= kMaxSimMessageBytes,
	"SimNetRoster: the largest roster message in its envelope fits one SimInbox record, so a Snapshot is never refused as "
	"tooLarge. Was SimNetRoster-rationale.md §2");
static_assert(kRosterNoSlot >= kMaxSimulatableSlots && kRosterNotLocal > kRosterMaxLocalIndex,
	"SimNetRoster: the 'no slot' and 'not local' sentinels lie outside the valid ranges, so neither reads as a real slot or "
	"local index. Was SimNetRoster-rationale.md §2");

namespace rosterDetail
{
	struct SlotKeyProbeAddress
	{
		bool isAlive() const { return false; }
		bool operator==(const SlotKeyProbeAddress&) const = default;
	};
} // namespace rosterDetail
} // namespace ogsimnet

template <>
struct std::hash<ogsimnet::rosterDetail::SlotKeyProbeAddress>
{
	size_t operator()(const ogsimnet::rosterDetail::SlotKeyProbeAddress&) const noexcept { return 0u; }
};

namespace ogsimnet
{
static_assert(kRosterMaxLocalIndex == ConnectionSlotKey<rosterDetail::SlotKeyProbeAddress>::kMaxPlayerSlot,
	"SimNetRoster: a roster localIndex is the reception coordinator's player slot, so its bound is ConnectionSlotKey's "
	"kMaxPlayerSlot. Was SimNetRoster-rationale.md §2");

constexpr const char* rosterRefusalName(RosterRefusal refusal)
{
	switch (refusal)
	{
	case RosterRefusal::None: return "None";
	case RosterRefusal::Version: return "Version";
	case RosterRefusal::Kind: return "Kind";
	case RosterRefusal::Truncated: return "Truncated";
	case RosterRefusal::Overlong: return "Overlong";
	case RosterRefusal::Count: return "Count";
	case RosterRefusal::SimIdZero: return "SimIdZero";
	case RosterRefusal::DuplicateSimId: return "DuplicateSimId";
	case RosterRefusal::SnapshotOrder: return "SnapshotOrder";
	case RosterRefusal::Slot: return "Slot";
	case RosterRefusal::LocalIndex: return "LocalIndex";
	case RosterRefusal::DuplicateSlot: return "DuplicateSlot";
	case RosterRefusal::DuplicateLocalIndex: return "DuplicateLocalIndex";
	case RosterRefusal::RemoveReason: return "RemoveReason";
	case RosterRefusal::NotAttached: return "NotAttached";
	case RosterRefusal::RevisionGap: return "RevisionGap";
	case RosterRefusal::AddLive: return "AddLive";
	case RosterRefusal::RemoveNotLive: return "RemoveNotLive";
	}
	return "Unknown";
}

constexpr const char* rosterInputVerdictName(RosterInputVerdict verdict)
{
	switch (verdict)
	{
	case RosterInputVerdict::Accepted: return "Accepted";
	case RosterInputVerdict::UnknownSim: return "UnknownSim";
	case RosterInputVerdict::NotOwner: return "NotOwner";
	}
	return "Unknown";
}

struct RosterEntry
{
	uint8_t simId = 0u;
	uint8_t physicsSlot = kRosterNoSlot;
	uint8_t localIndex = kRosterNotLocal;

	constexpr bool isLocal() const { return localIndex != kRosterNotLocal; }
	constexpr bool operator==(const RosterEntry&) const = default;
};

struct RosterMessage
{
	std::array<std::byte, kRosterMaxMessageBytes> bytes{};
	uint8_t size = 0u;

	constexpr bool empty() const { return size == 0u; }
	constexpr std::span<const std::byte> view() const { return std::span<const std::byte>(bytes.data(), size); }
};

struct RosterView
{
	std::array<RosterEntry, kRosterMaxEntries> entries{};
	uint8_t count = 0u;

	constexpr std::span<const RosterEntry> span() const { return std::span<const RosterEntry>(entries.data(), count); }
};

struct DecodedRoster
{
	RosterRefusal refusal = RosterRefusal::None;
	RosterKind kind = RosterKind::Snapshot;
	uint16_t revision = 0u;
	RosterView view;
	uint8_t removedSimId = 0u;
	RosterRemoveReason removeReason = RosterRemoveReason::Left;
};

namespace rosterCodec
{
	namespace detail
	{
		constexpr uint8_t u8At(std::span<const std::byte> bytes, size_t offset)
		{
			return std::to_integer<uint8_t>(bytes[offset]);
		}

		constexpr void put(RosterMessage& message, uint8_t value)
		{
			message.bytes[message.size] = static_cast<std::byte>(value);
			++message.size;
		}

		constexpr void putHeader(RosterMessage& message, RosterKind kind, uint16_t revision)
		{
			put(message, kRosterWireVersion);
			put(message, static_cast<uint8_t>(kind));
			put(message, static_cast<uint8_t>(revision & 0xFFu));
			put(message, static_cast<uint8_t>((revision >> 8u) & 0xFFu));
		}

		constexpr void putEntry(RosterMessage& message, const RosterEntry& entry)
		{
			put(message, entry.simId);
			put(message, entry.physicsSlot);
			put(message, entry.localIndex);
		}

		constexpr RosterRefusal exactLength(size_t actual, size_t expected)
		{
			if (actual < expected)
				return RosterRefusal::Truncated;
			if (actual > expected)
				return RosterRefusal::Overlong;
			return RosterRefusal::None;
		}

		constexpr RosterRefusal checkEntry(const RosterEntry& entry)
		{
			if (entry.simId == 0u)
				return RosterRefusal::SimIdZero;
			if (entry.physicsSlot >= kMaxSimulatableSlots && entry.physicsSlot != kRosterNoSlot)
				return RosterRefusal::Slot;
			if (entry.localIndex > kRosterMaxLocalIndex && entry.localIndex != kRosterNotLocal)
				return RosterRefusal::LocalIndex;
			return RosterRefusal::None;
		}

		constexpr RosterEntry entryAt(std::span<const std::byte> bytes, size_t offset)
		{
			return RosterEntry{u8At(bytes, offset), u8At(bytes, offset + 1u), u8At(bytes, offset + 2u)};
		}

		constexpr RosterRefusal checkAgainst(const RosterEntry& candidate, std::span<const RosterEntry> others)
		{
			for (const RosterEntry& other : others)
			{
				if (other.simId == candidate.simId)
					return RosterRefusal::DuplicateSimId;
				if (candidate.physicsSlot != kRosterNoSlot && other.physicsSlot == candidate.physicsSlot)
					return RosterRefusal::DuplicateSlot;
				if (candidate.isLocal() && other.localIndex == candidate.localIndex)
					return RosterRefusal::DuplicateLocalIndex;
			}
			return RosterRefusal::None;
		}

		constexpr DecodedRoster refused(RosterRefusal refusal, RosterKind kind = RosterKind::Snapshot)
		{
			DecodedRoster decoded;
			decoded.refusal = refusal;
			decoded.kind = kind;
			return decoded;
		}
	} // namespace detail

	constexpr RosterMessage encodeSnapshot(uint16_t revision, std::span<const RosterEntry> entries)
	{
		RosterMessage message;
		if (entries.size() > kRosterMaxEntries)
			return message;
		detail::putHeader(message, RosterKind::Snapshot, revision);
		detail::put(message, static_cast<uint8_t>(entries.size()));
		for (const RosterEntry& entry : entries)
			detail::putEntry(message, entry);
		return message;
	}

	constexpr RosterMessage encodeAdd(uint16_t revision, const RosterEntry& entry)
	{
		RosterMessage message;
		detail::putHeader(message, RosterKind::Add, revision);
		detail::putEntry(message, entry);
		return message;
	}

	constexpr RosterMessage encodeRemove(uint16_t revision, uint8_t simId, RosterRemoveReason reason)
	{
		RosterMessage message;
		detail::putHeader(message, RosterKind::Remove, revision);
		detail::put(message, simId);
		detail::put(message, static_cast<uint8_t>(reason));
		return message;
	}

	constexpr DecodedRoster decode(std::span<const std::byte> bytes)
	{
		if (bytes.empty())
			return detail::refused(RosterRefusal::Truncated);
		if (detail::u8At(bytes, 0u) != kRosterWireVersion)
			return detail::refused(RosterRefusal::Version);
		if (bytes.size() < kRosterHeaderBytes)
			return detail::refused(RosterRefusal::Truncated);
		const uint8_t kindByte = detail::u8At(bytes, 1u);
		if (kindByte < static_cast<uint8_t>(RosterKind::Snapshot) || kindByte > static_cast<uint8_t>(RosterKind::Remove))
			return detail::refused(RosterRefusal::Kind);

		DecodedRoster decoded;
		decoded.kind = static_cast<RosterKind>(kindByte);
		decoded.revision = static_cast<uint16_t>(detail::u8At(bytes, 2u) | (detail::u8At(bytes, 3u) << 8u));

		if (decoded.kind == RosterKind::Snapshot)
		{
			if (bytes.size() < kRosterSnapshotBaseBytes)
				return detail::refused(RosterRefusal::Truncated, decoded.kind);
			const uint8_t count = detail::u8At(bytes, kRosterHeaderBytes);
			if (count > kRosterMaxEntries)
				return detail::refused(RosterRefusal::Count, decoded.kind);
			const RosterRefusal lengthRefusal =
				detail::exactLength(bytes.size(), kRosterSnapshotBaseBytes + count * kRosterEntryBytes);
			if (lengthRefusal != RosterRefusal::None)
				return detail::refused(lengthRefusal, decoded.kind);
			for (uint8_t index = 0u; index < count; ++index)
			{
				const RosterEntry entry = detail::entryAt(bytes, kRosterSnapshotBaseBytes + index * kRosterEntryBytes);
				if (const RosterRefusal entryRefusal = detail::checkEntry(entry); entryRefusal != RosterRefusal::None)
					return detail::refused(entryRefusal, decoded.kind);
				if (const RosterRefusal clash = detail::checkAgainst(entry, decoded.view.span()); clash != RosterRefusal::None)
					return detail::refused(clash, decoded.kind);
				if (index > 0u && entry.simId < decoded.view.entries[index - 1u].simId)
					return detail::refused(RosterRefusal::SnapshotOrder, decoded.kind);
				decoded.view.entries[index] = entry;
				decoded.view.count = static_cast<uint8_t>(index + 1u);
			}
			return decoded;
		}

		if (decoded.kind == RosterKind::Add)
		{
			if (const RosterRefusal lengthRefusal = detail::exactLength(bytes.size(), kRosterAddBytes);
				lengthRefusal != RosterRefusal::None)
				return detail::refused(lengthRefusal, decoded.kind);
			const RosterEntry entry = detail::entryAt(bytes, kRosterHeaderBytes);
			if (const RosterRefusal entryRefusal = detail::checkEntry(entry); entryRefusal != RosterRefusal::None)
				return detail::refused(entryRefusal, decoded.kind);
			decoded.view.entries[0] = entry;
			decoded.view.count = 1u;
			return decoded;
		}

		if (const RosterRefusal lengthRefusal = detail::exactLength(bytes.size(), kRosterRemoveBytes);
			lengthRefusal != RosterRefusal::None)
			return detail::refused(lengthRefusal, decoded.kind);
		decoded.removedSimId = detail::u8At(bytes, kRosterHeaderBytes);
		if (decoded.removedSimId == 0u)
			return detail::refused(RosterRefusal::SimIdZero, decoded.kind);
		const uint8_t reasonByte = detail::u8At(bytes, kRosterHeaderBytes + 1u);
		if (reasonByte < static_cast<uint8_t>(RosterRemoveReason::Left)
			|| reasonByte > static_cast<uint8_t>(RosterRemoveReason::Despawned))
			return detail::refused(RosterRefusal::RemoveReason, decoded.kind);
		decoded.removeReason = static_cast<RosterRemoveReason>(reasonByte);
		return decoded;
	}
} // namespace rosterCodec

struct RosterRecord
{
	uint8_t simId = 0u;
	uint8_t physicsSlot = kRosterNoSlot;
	std::optional<SimConnectionId> owner;
	uint8_t localIndex = kRosterNotLocal;
};

class RosterAuthority
{
public:
	void add(const RosterRecord& record)
	{
		OG_CHECK(record.simId != 0u, "RosterAuthority::add: sim id 0 is None and is never sent");
		OG_CHECK(!m_everAdded.test(record.simId),
			"RosterAuthority::add: this sim id was already used in this authority's lifetime; a sim id is never reused, "
			"so a client cannot confuse a new character with a gone one");
		OG_CHECK(record.physicsSlot < kMaxSimulatableSlots || record.physicsSlot == kRosterNoSlot,
			"RosterAuthority::add: physicsSlot is neither a slot below kMaxSimulatableSlots nor kRosterNoSlot");
		OG_CHECK(record.localIndex <= kRosterMaxLocalIndex || record.localIndex == kRosterNotLocal,
			"RosterAuthority::add: localIndex is neither 0..7 nor kRosterNotLocal");
		OG_CHECK(!record.owner || *record.owner != kInvalidSimConnection,
			"RosterAuthority::add: an owner is a live connection id, never kInvalidSimConnection; use no owner for an "
			"authority-local character");
		OG_CHECK(!record.owner || record.localIndex <= kRosterMaxLocalIndex,
			"RosterAuthority::add: a character owned by a connection needs that connection's local index (0..7)");

		Slot* freeSlot = nullptr;
		for (Slot& slot : m_slots)
		{
			if (slot.state == SlotState::Free)
			{
				if (freeSlot == nullptr)
					freeSlot = &slot;
				continue;
			}
			[[maybe_unused]] const bool sameSlot =
				record.physicsSlot != kRosterNoSlot && slot.record.physicsSlot == record.physicsSlot;
			OG_CHECK(!(sameSlot && slot.state == SlotState::RemovePending),
				"RosterAuthority::add: this physics slot's previous Remove is not queued yet (removeFor); a slot is "
				"reusable only after its Remove was queued, or its Add could reach a client before that Remove");
			OG_CHECK(!sameSlot, "RosterAuthority::add: a live entry already holds this physics slot");
			OG_CHECK(!(slot.state == SlotState::Live && record.owner && slot.record.owner == record.owner
						 && slot.record.localIndex == record.localIndex),
				"RosterAuthority::add: (owner, localIndex) already names a live entry; each local player of a connection "
				"owns one sim id");
		}
		OG_CHECK(freeSlot != nullptr,
			"RosterAuthority::add: more characters than kMaxSimulatableSlots; admission must refuse the join before "
			"registration");

		m_everAdded.set(record.simId);
		++m_revision;
		*freeSlot = Slot{SlotState::Live, record, m_revision, 0u, RosterRemoveReason::Left};
	}

	void remove(uint8_t simId, RosterRemoveReason reason)
	{
		Slot* slot = find(simId, SlotState::Live);
		OG_CHECK(slot != nullptr, "RosterAuthority::remove: no live entry has this sim id");
		++m_revision;
		slot->state = SlotState::RemovePending;
		slot->removeRevision = m_revision;
		slot->removeReason = reason;
	}

	RosterView viewFor(SimConnectionId recipient) const
	{
		RosterView view;
		for (const Slot& slot : m_slots)
		{
			if (slot.state != SlotState::Live)
				continue;
			size_t at = view.count;
			while (at > 0u && view.entries[at - 1u].simId > slot.record.simId)
			{
				view.entries[at] = view.entries[at - 1u];
				--at;
			}
			view.entries[at] = entryFor(slot.record, recipient);
			++view.count;
		}
		return view;
	}

	RosterMessage snapshotFor(SimConnectionId recipient) const
	{
		OG_CHECK(!hasPendingRemove(),
			"RosterAuthority::snapshotFor: a Remove is pending; queue it (removeFor) first, or the new connection gets that "
			"Remove after a Snapshot whose revision already counts it, a revision gap");
		const RosterView view = viewFor(recipient);
		return rosterCodec::encodeSnapshot(m_revision, view.span());
	}

	RosterMessage addFor(SimConnectionId recipient, uint8_t simId) const
	{
		const Slot* slot = find(simId, SlotState::Live);
		OG_CHECK(slot != nullptr, "RosterAuthority::addFor: no live entry has this sim id");
		return rosterCodec::encodeAdd(slot->addRevision, entryFor(slot->record, recipient));
	}

	RosterMessage removeFor(uint8_t simId)
	{
		Slot* slot = find(simId, SlotState::RemovePending);
		OG_CHECK(slot != nullptr, "RosterAuthority::removeFor: no Remove is pending for this sim id (call remove first)");
		const RosterMessage message = rosterCodec::encodeRemove(slot->removeRevision, simId, slot->removeReason);
		*slot = Slot{};
		return message;
	}

	std::optional<SimConnectionId> ownerOf(uint8_t simId) const
	{
		const Slot* slot = find(simId, SlotState::Live);
		return slot != nullptr ? slot->record.owner : std::nullopt;
	}

	std::optional<uint8_t> localIndexOf(uint8_t simId) const
	{
		const Slot* slot = find(simId, SlotState::Live);
		if (slot == nullptr || !slot->record.owner)
			return std::nullopt;
		return slot->record.localIndex;
	}

	RosterInputVerdict checkInput(SimConnectionId sender, uint8_t simId) const
	{
		const Slot* slot = find(simId, SlotState::Live);
		if (slot == nullptr)
			return RosterInputVerdict::UnknownSim;
		if (!slot->record.owner || *slot->record.owner != sender)
			return RosterInputVerdict::NotOwner;
		return RosterInputVerdict::Accepted;
	}

	const RosterRecord* recordFor(uint8_t simId) const
	{
		const Slot* slot = find(simId, SlotState::Live);
		return slot != nullptr ? &slot->record : nullptr;
	}

	uint16_t revision() const { return m_revision; }

	size_t liveCount() const
	{
		size_t count = 0u;
		for (const Slot& slot : m_slots)
			count += slot.state == SlotState::Live ? 1u : 0u;
		return count;
	}

private:
	enum class SlotState : uint8_t { Free, Live, RemovePending };

	struct Slot
	{
		SlotState state = SlotState::Free;
		RosterRecord record{};
		uint16_t addRevision = 0u;
		uint16_t removeRevision = 0u;
		RosterRemoveReason removeReason = RosterRemoveReason::Left;
	};

	static RosterEntry entryFor(const RosterRecord& record, SimConnectionId recipient)
	{
		// ⛔G-01  docs/SimNetRoster-guards.md
		return RosterEntry{record.simId, record.physicsSlot,
			(record.owner && *record.owner == recipient) ? record.localIndex : kRosterNotLocal};
	}

	bool hasPendingRemove() const
	{
		for (const Slot& slot : m_slots)
		{
			if (slot.state == SlotState::RemovePending)
				return true;
		}
		return false;
	}

	Slot* find(uint8_t simId, SlotState state)
	{
		for (Slot& slot : m_slots)
		{
			if (slot.state == state && slot.record.simId == simId)
				return &slot;
		}
		return nullptr;
	}

	const Slot* find(uint8_t simId, SlotState state) const
	{
		for (const Slot& slot : m_slots)
		{
			if (slot.state == state && slot.record.simId == simId)
				return &slot;
		}
		return nullptr;
	}

	std::array<Slot, kRosterMaxEntries> m_slots{};
	std::bitset<256> m_everAdded;
	uint16_t m_revision = 0u;
};

struct RosterApplyResult
{
	RosterRefusal refusal = RosterRefusal::None;
	RosterKind kind = RosterKind::Snapshot;

	constexpr bool applied() const { return refusal == RosterRefusal::None; }
};

class RosterMirror
{
public:
	using AddedCallback = std::function<void(const RosterEntry&)>;
	using RemovedCallback = std::function<void(const RosterEntry&)>;

	RosterApplyResult apply(std::span<const std::byte> bytes)
	{
		const DecodedRoster decoded = rosterCodec::decode(bytes);
		if (decoded.refusal != RosterRefusal::None)
			return RosterApplyResult{decoded.refusal, decoded.kind};

		State next = m_state;
		if (const RosterRefusal refusal = stage(decoded, next); refusal != RosterRefusal::None)
			return RosterApplyResult{refusal, decoded.kind};

		const State before = m_state;
		m_state = next;
		notify(before, next);
		return RosterApplyResult{RosterRefusal::None, decoded.kind};
	}

	const RosterEntry* entryFor(uint8_t simId) const
	{
		for (const RosterEntry& entry : m_state.view.span())
		{
			if (entry.simId == simId)
				return &entry;
		}
		return nullptr;
	}

	std::span<const RosterEntry> entries() const { return m_state.view.span(); }
	bool attached() const { return m_state.attached; }
	uint16_t revision() const { return m_state.revision; }

	void setAddedCallback(AddedCallback callback) { m_added = std::move(callback); }
	void clearAddedCallback() { m_added = nullptr; }
	void setRemovedCallback(RemovedCallback callback) { m_removed = std::move(callback); }
	void clearRemovedCallback() { m_removed = nullptr; }

private:
	struct State
	{
		RosterView view;
		uint16_t revision = 0u;
		bool attached = false;
	};

	static bool contains(std::span<const RosterEntry> entries, const RosterEntry& entry)
	{
		for (const RosterEntry& other : entries)
		{
			if (other == entry)
				return true;
		}
		return false;
	}

	RosterRefusal stage(const DecodedRoster& decoded, State& next) const
	{
		if (decoded.kind == RosterKind::Snapshot)
		{
			next.view = decoded.view;
			next.revision = decoded.revision;
			next.attached = true;
			return RosterRefusal::None;
		}

		if (!m_state.attached)
			return RosterRefusal::NotAttached;
		if (decoded.revision != static_cast<uint16_t>(m_state.revision + 1u))
			return RosterRefusal::RevisionGap;

		if (decoded.kind == RosterKind::Add)
		{
			const RosterEntry& added = decoded.view.entries[0];
			if (entryFor(added.simId) != nullptr)
				return RosterRefusal::AddLive;
			if (const RosterRefusal clash = rosterCodec::detail::checkAgainst(added, m_state.view.span());
				clash != RosterRefusal::None)
				return clash;
			if (m_state.view.count >= kRosterMaxEntries)
				return RosterRefusal::Count;
			size_t at = next.view.count;
			while (at > 0u && next.view.entries[at - 1u].simId > added.simId)
			{
				next.view.entries[at] = next.view.entries[at - 1u];
				--at;
			}
			next.view.entries[at] = added;
			++next.view.count;
			next.revision = decoded.revision;
			return RosterRefusal::None;
		}

		if (entryFor(decoded.removedSimId) == nullptr)
			return RosterRefusal::RemoveNotLive;
		size_t write = 0u;
		for (size_t read = 0u; read < next.view.count; ++read)
		{
			if (next.view.entries[read].simId != decoded.removedSimId)
				next.view.entries[write++] = next.view.entries[read];
		}
		next.view.count = static_cast<uint8_t>(write);
		next.view.entries[write] = RosterEntry{};
		next.revision = decoded.revision;
		return RosterRefusal::None;
	}

	void notify(const State& before, const State& after) const
	{
		if (m_removed)
		{
			for (const RosterEntry& entry : before.view.span())
			{
				if (!contains(after.view.span(), entry))
					m_removed(entry);
			}
		}
		if (m_added)
		{
			for (const RosterEntry& entry : after.view.span())
			{
				if (!contains(before.view.span(), entry))
					m_added(entry);
			}
		}
	}

	State m_state;
	AddedCallback m_added;
	RemovedCallback m_removed;
};

inline constexpr size_t kRosterLogLineBytes = 128u;
using RosterLogLine = std::array<char, kRosterLogLineBytes>;

inline constexpr char kRosterInputRefusedFormat[] = "[Roster.InputRefused] conn=%u simId=%u reason=%s";
inline constexpr char kRosterRefusedFormat[] = "[Roster.Refused] reason=%s";
inline constexpr char kRosterBindMismatchFormat[] = "[Roster.BindMismatch] simId=%u roster=%s engine=%s";

static_assert(kRosterLogLineBytes >= sizeof(kRosterInputRefusedFormat) + 10u + 3u + sizeof("UnknownSim")
		&& kRosterLogLineBytes >= sizeof(kRosterRefusedFormat) + sizeof("DuplicateLocalIndex")
		&& kRosterLogLineBytes >= sizeof(kRosterBindMismatchFormat) + 3u + 2u * sizeof("Remote"),
	"kRosterLogLineBytes must hold the longest [Roster.*] line: each format with a 10-digit connection id, a 3-digit sim "
	"id and its longest name");

inline RosterLogLine formatRosterInputRefused(SimConnectionId conn, uint8_t simId, RosterInputVerdict verdict)
{
	OG_CHECK(verdict != RosterInputVerdict::Accepted, "formatRosterInputRefused: an accepted bundle is not refused");
	RosterLogLine line{};
	std::snprintf(line.data(), line.size(), kRosterInputRefusedFormat, static_cast<unsigned>(conn),
		static_cast<unsigned>(simId), rosterInputVerdictName(verdict));
	return line;
}

inline RosterLogLine formatRosterRefused(RosterRefusal refusal)
{
	RosterLogLine line{};
	std::snprintf(line.data(), line.size(), kRosterRefusedFormat, rosterRefusalName(refusal));
	return line;
}

inline RosterLogLine formatRosterBindMismatch(uint8_t simId, bool rosterLocal, bool engineLocal)
{
	RosterLogLine line{};
	std::snprintf(line.data(), line.size(), kRosterBindMismatchFormat, static_cast<unsigned>(simId),
		rosterLocal ? "Local" : "Remote", engineLocal ? "Local" : "Remote");
	return line;
}
} // namespace ogsimnet
