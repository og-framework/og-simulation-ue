#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetConnections-rationale.md

#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace ogsimnet
{
class LivenessTable;

inline constexpr uint32_t kNoConnectionEpoch = 0u;
inline constexpr size_t kLivenessPruneEntries = 2048u;

struct SimNetConnectionHandle
{
	SimConnectionId id = kInvalidSimConnection;
	uint32_t epoch = kNoConnectionEpoch;
	const LivenessTable* table = nullptr;

	bool isAlive() const;

	friend bool operator==(const SimNetConnectionHandle&, const SimNetConnectionHandle&) = default;
};

class LivenessTable
{
public:
	LivenessTable() { m_entries.reserve(kLivenessPruneEntries); }
	LivenessTable(const LivenessTable&) = delete;
	LivenessTable& operator=(const LivenessTable&) = delete;
	LivenessTable(LivenessTable&&) = delete;
	LivenessTable& operator=(LivenessTable&&) = delete;

	bool apply(const InboundSimRecord& record)
	{
		switch (record.kind)
		{
		case InboundKind::Message:
			return false;
		case InboundKind::Connected:
			onConnected(record.conn);
			return true;
		case InboundKind::Disconnected:
			onDisconnected(record.conn);
			return true;
		}
		return false;
	}

	bool isAlive(SimConnectionId id, uint32_t epoch) const
	{
		const auto found = m_entries.find(id);
		return epoch != kNoConnectionEpoch && found != m_entries.end() && found->second.up && found->second.epoch == epoch;
	}

	bool isUp(SimConnectionId id) const
	{
		const auto found = m_entries.find(id);
		return found != m_entries.end() && found->second.up;
	}

	SimNetConnectionHandle handleFor(SimConnectionId id) const
	{
		const auto found = m_entries.find(id);
		if (found == m_entries.end() || !found->second.up)
		{
			return SimNetConnectionHandle{id, kNoConnectionEpoch, this};
		}
		return SimNetConnectionHandle{id, found->second.epoch, this};
	}

	size_t upCount() const { return m_upCount; }
	size_t entryCount() const { return m_entries.size(); }
	uint32_t lastEpoch() const { return m_lastEpoch; }

private:
	struct Entry
	{
		uint32_t epoch = kNoConnectionEpoch;
		bool up = false;
	};

	void onConnected(SimConnectionId id)
	{
		if (id == kInvalidSimConnection)
		{
			return;
		}
		if (m_entries.size() >= kLivenessPruneEntries)
		{
			std::erase_if(m_entries, [](const auto& entry) { return !entry.second.up; });
		}
		Entry& entry = m_entries[id];
		m_upCount += entry.up ? 0u : 1u;
		entry.up = true;
		entry.epoch = nextEpoch();
	}

	void onDisconnected(SimConnectionId id)
	{
		const auto found = m_entries.find(id);
		if (found == m_entries.end() || !found->second.up)
		{
			return;
		}
		found->second.up = false;
		--m_upCount;
	}

	uint32_t nextEpoch()
	{
		++m_lastEpoch;
		if (m_lastEpoch == kNoConnectionEpoch)
		{
			++m_lastEpoch;
		}
		return m_lastEpoch;
	}

	std::unordered_map<SimConnectionId, Entry> m_entries;
	uint32_t m_lastEpoch = kNoConnectionEpoch;
	size_t m_upCount = 0u;
};

inline bool SimNetConnectionHandle::isAlive() const
{
	return table != nullptr && table->isAlive(id, epoch);
}
} // namespace ogsimnet

template <>
struct std::hash<ogsimnet::SimNetConnectionHandle>
{
	size_t operator()(const ogsimnet::SimNetConnectionHandle& handle) const noexcept
	{
		return std::hash<SimConnectionId>{}(handle.id);
	}
};

static_assert(ConnectionAddress<ogsimnet::SimNetConnectionHandle>,
	"SimNetConnectionHandle: the server's reception coordinator and its tier and delay tables take it as their connection address, "
	"so it must stay a regular, default-constructible, hashable value with isAlive(). Was SimNetConnections-rationale.md §2");
