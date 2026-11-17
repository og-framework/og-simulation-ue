// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "SimNetTestRig.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/SimulationQueues.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace simNetEquivalence
{
constexpr SimNetConfig kHeldSends{false};
constexpr double kB0DownLossPercent = 1.0;
constexpr double kB0UpLossPercent = 2.0;
constexpr std::uint64_t kB0LossSeed = 0xB0u;

struct StreamKey
{
	std::string stream;
	std::size_t client = 0u;
	std::uint8_t simId = 0u;

	friend bool operator<(const StreamKey& a, const StreamKey& b)
	{
		return std::tie(a.stream, a.client, a.simId) < std::tie(b.stream, b.client, b.simId);
	}
};

struct StreamCounts
{
	std::size_t reference = 0u;
	std::size_t transport = 0u;
	std::size_t injected = 0u;
};

std::size_t injectedFor(const std::vector<std::string>& trace, std::uint8_t simId)
{
	const std::string prefix = "[InjectCorrectionState] id=" + std::to_string(simId) + " ";
	std::size_t count = 0u;
	for (const std::string& line : trace)
		count += line.rfind(prefix, 0) == 0u ? 1u : 0u;
	return count;
}

void tally(const FrameRecord& transport, const FrameRecord& reference, std::map<StreamKey, StreamCounts>& counts)
{
	for (std::size_t c = 0u; c < kClientCount; ++c)
	{
		for (const StreamItem& item : reference.clients[c].corrections)
			++counts[StreamKey{"correction", c, item.simId}].reference;
		for (const StreamItem& item : transport.clients[c].corrections)
			++counts[StreamKey{"correction", c, item.simId}].transport;
		for (const CharacterSpec& spec : kCharacters)
			counts[StreamKey{"correction", c, spec.simId}].injected += injectedFor(transport.clients[c].trace, spec.simId);
		for (const StreamItem& item : reference.clients[c].rings)
			++counts[StreamKey{"relay", c, item.simId}].reference;
		for (const StreamItem& item : transport.clients[c].rings)
			++counts[StreamKey{"relay", c, item.simId}].transport;
		counts[StreamKey{"clock", c, 0u}].reference += reference.clients[c].clock.size();
		counts[StreamKey{"clock", c, 0u}].transport += transport.clients[c].clock.size();
	}
	for (const auto& [simId, bundles] : reference.server.bundles)
		counts[StreamKey{"input", specOf(simId)->client, simId}].reference += bundles;
	for (const auto& [simId, bundles] : transport.server.bundles)
		counts[StreamKey{"input", specOf(simId)->client, simId}].transport += bundles;
}

ognet::EmulatorConfig b0Loss()
{
	ognet::EmulatorConfig config;
	config.seed = kB0LossSeed;
	config.outbound.lossPercent = kB0UpLossPercent;
	config.inbound.lossPercent = kB0DownLossPercent;
	return config;
}

TEST_CASE("SimNetEquivalence.TheLosslessFrameLockedRigEqualsTheNoTransportReferenceFrameByFrame", "[NetTransport]")
{
	TransportRig transport(kHeldSends);
	ReferenceRig reference(transport.serverIds, transport.stagerSetupFlushes);
	FrameDiff diff;
	std::map<StreamKey, StreamCounts> counts;
	std::set<std::uint32_t> ticksPerFrame;
	std::size_t parks = 0u;
	std::size_t releases = 0u;
	std::size_t consumedRows = 0u;
	std::size_t consumedApplied = 0u;
	std::size_t injected = 0u;
	std::size_t queued = 0u;

	for (int frame = 0; frame < kFrames; ++frame)
	{
		const FrameRecord a = transport.frame(frame);
		const FrameRecord b = reference.frame(frame);
		compareFrames(frame, a, b, diff);
		queued += countWith(a.server.trace, "[ReceiveLocalInput]");
		tally(a, b, counts);
		ticksPerFrame.insert(a.ticks);
		parks += countWith(a.server.decisions, "[Park]");
		releases += countWith(a.server.decisions, "[Release]");
		for (const InputRow& row : a.server.consumed)
		{
			++consumedRows;
			consumedApplied += row.captureTick != kNoInputCaptureTick ? 1u : 0u;
		}
		for (const ClientFrame& client : a.clients)
			injected += countWith(client.trace, "[InjectCorrectionState]");
	}

	{
		INFO(diff.describe());
		CHECK(diff.empty());
	}

	CHECK(ticksPerFrame == std::set<std::uint32_t>{1u, 2u, 3u});
	CHECK(parks > 1000u);
	CHECK(releases > 1000u);
	CHECK(queued > 1000u);
	CHECK(consumedApplied * 10u > consumedRows * 9u);
	CHECK(injected >= kClientCount * static_cast<std::size_t>(kFrames - 1));
	for (const auto& [key, count] : counts)
	{
		INFO(key.stream << " client " << key.client << " sim " << static_cast<int>(key.simId));
		CHECK(count.transport == count.reference);
		CHECK(count.reference * 5u > static_cast<std::size_t>(kFrames) * (key.stream == "correction" ? 1u : 4u));
	}
	CHECK(counts.size() == kClientCount * kCharacters.size() + kCharacters.size() * (kClientCount - 1u) + kClientCount + kCharacters.size());
	std::printf("[SimNetEquivalence.Lossless] frames=%d equalFrames=%d parks=%zu releases=%zu queued=%zu consumed=%zu applied=%zu injected=%zu\n",
		kFrames, kFrames - static_cast<int>(diff.frames.size()), parks, releases, queued, consumedRows, consumedApplied, injected);
}

TEST_CASE("SimNetEquivalence.UnderB0LossTheShortfallPerStreamIsPrinted", "[NetTransport]")
{
	TransportRig transport(kHeldSends, b0Loss());
	ReferenceRig reference(transport.serverIds, transport.stagerSetupFlushes);
	std::map<StreamKey, StreamCounts> counts;
	std::map<std::pair<std::size_t, std::uint8_t>, std::uint32_t> newestInjected;
	std::size_t olderAfterNewer = 0u;

	for (int frame = 0; frame < kFrames; ++frame)
	{
		const FrameRecord a = transport.frame(frame);
		const FrameRecord b = reference.frame(frame);
		tally(a, b, counts);
		for (std::size_t c = 0u; c < kClientCount; ++c)
		{
			for (const StreamItem& item : a.clients[c].corrections)
			{
				const std::uint32_t tick = correctionTick(item);
				const auto key = std::make_pair(c, item.simId);
				const auto found = newestInjected.find(key);
				if (found != newestInjected.end() && tick <= found->second)
					++olderAfterNewer;
				newestInjected[key] = tick;
			}
		}
	}

	for (const auto& [key, count] : counts)
	{
		const long long shortfall = static_cast<long long>(count.reference) - static_cast<long long>(count.transport);
		const double percent = count.reference == 0u ? 0.0 : 100.0 * static_cast<double>(shortfall) / static_cast<double>(count.reference);
		std::printf("[SimNetEquivalence.Loss] stream=%s client=%zu simId=%u reference=%zu transport=%zu injected=%zu shortfall=%lld (%.2f%%)\n",
			key.stream.c_str(), key.client, static_cast<unsigned>(key.simId), count.reference, count.transport, count.injected, shortfall,
			percent);
	}
	for (std::size_t c = 0u; c < kClientCount; ++c)
	{
		const ognet::EmulatorStats stats = transport.clients[c]->peer->backend.emulatorStats();
		std::printf("[SimNetEquivalence.Loss] emulator client=%zu up offered=%llu lost=%llu down offered=%llu lost=%llu\n", c,
			static_cast<unsigned long long>(stats.outbound.offered), static_cast<unsigned long long>(stats.outbound.lost),
			static_cast<unsigned long long>(stats.inbound.offered), static_cast<unsigned long long>(stats.inbound.lost));
		CHECK(stats.outbound.lost > 0u);
		CHECK(stats.inbound.lost > 0u);
	}

	CHECK(olderAfterNewer == 0u);
	for (const auto& [key, count] : counts)
	{
		INFO(key.stream << " client " << key.client << " sim " << static_cast<int>(key.simId));
		CHECK(count.transport > 0u);
	}
}
} // namespace simNetEquivalence
