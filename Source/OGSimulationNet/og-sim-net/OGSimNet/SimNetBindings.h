#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetBindings-rationale.md · docs/SimNetBindings-guards.md

#include "OGSimNet/SimNetRoster.h"
#include "OGSimulation/OGAssert.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace ogsimnet
{
enum class BindStep : uint8_t { Wait, Ready, RoleMismatch };

struct BindVerdict
{
	BindStep step = BindStep::Wait;
	uint8_t simId = 0u;
	uint8_t physicsSlot = kRosterNoSlot;
	uint8_t localIndex = kRosterNotLocal;
	bool isLocal = false;

	constexpr bool operator==(const BindVerdict&) const = default;
};

class SimNetBindings
{
public:
	static constexpr size_t kSimIdCount = size_t{std::numeric_limits<uint8_t>::max()} + 1u;

	void onRosterAdded(const RosterEntry& entry)
	{
		OG_CHECK(entry.simId != 0u, "SimNetBindings::onRosterAdded: sim id 0 is None; the roster decoder refuses it (SimIdZero)");
		OG_CHECK(m_entries[entry.simId].simId == 0u,
			"SimNetBindings::onRosterAdded: this sim id already has an entry; the roster mirror reports a changed entry as "
			"removed before added, so its added and removed callbacks must have gone out of step");
		m_entries[entry.simId] = entry;
	}

	bool onRosterRemoved(uint8_t simId)
	{
		m_entries[simId] = RosterEntry{};
		return takeRegistration(simId);
	}

	void seedFrom(const RosterMirror& mirror)
	{
		OG_CHECK(std::find(m_registered.begin(), m_registered.end(), true) == m_registered.end(),
			"SimNetBindings::seedFrom: a sim id is registered; seeding replaces every entry, so a registered id the mirror no "
			"longer lists would never see its Remove. Seed before the first registration");
		m_entries = {};
		for (const RosterEntry& entry : mirror.entries())
			m_entries[entry.simId] = entry;
	}

	BindVerdict verdict(uint8_t simId, bool engineSaysLocal) const
	{
		const RosterEntry& entry = m_entries[simId];
		if (entry.simId == 0u)
			return BindVerdict{BindStep::Wait, simId, kRosterNoSlot, kRosterNotLocal, false};
		// ⛔G-02  docs/SimNetBindings-guards.md
		return BindVerdict{entry.isLocal() == engineSaysLocal ? BindStep::Ready : BindStep::RoleMismatch, simId,
			entry.physicsSlot, entry.localIndex, entry.isLocal()};
	}

	void markRegistered(uint8_t simId)
	{
		OG_CHECK(m_entries[simId].simId != 0u,
			"SimNetBindings::markRegistered: no roster entry has this sim id; register only after verdict returned Ready");
		OG_CHECK(!m_registered[simId],
			"SimNetBindings::markRegistered: this sim id is already registered; its Remove or its pawn's EndPlay "
			"unregisters it first");
		m_registered[simId] = true;
	}

	bool markUnregistered(uint8_t simId) { return takeRegistration(simId); }

	bool isRegistered(uint8_t simId) const { return m_registered[simId]; }

	bool operator==(const SimNetBindings&) const = default;

private:
	static_assert(kSimIdCount == 256u,
		"SimNetBindings: the tables are indexed by the whole uint8_t sim id range, so every sim id has a row and no lookup "
		"needs a bounds check. Was SimNetBindings-rationale.md §2");

	bool takeRegistration(uint8_t simId)
	{
		// ⛔G-01  docs/SimNetBindings-guards.md
		return std::exchange(m_registered[simId], false);
	}

	std::array<RosterEntry, kSimIdCount> m_entries{};
	std::array<bool, kSimIdCount> m_registered{};
};
} // namespace ogsimnet
