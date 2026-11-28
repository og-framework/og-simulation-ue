// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGSimSession/SessionKnobs.h"
#include "SessionTestKit.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
using ogsession::sessionTestKit::MapConfigSource;

template <typename ConfigT>
class RecordingReads
{
public:
	explicit RecordingReads(const ConfigT& config) : m_config(config) {}

	std::optional<int32_t> readInt(const char* key) const
	{
		m_keys.emplace_back(key);
		return m_config.readInt(key);
	}

	std::optional<double> readDouble(const char* key) const
	{
		m_keys.emplace_back(key);
		return m_config.readDouble(key);
	}

	std::optional<std::string> readString(const char* key) const
	{
		m_keys.emplace_back(key);
		return m_config.readString(key);
	}

	const std::vector<std::string>& keys() const { return m_keys; }

private:
	const ConfigT&                   m_config;
	mutable std::vector<std::string> m_keys;
};

static_assert(ogsession::ConfigSource<RecordingReads<MapConfigSource>>, "the test's recording reader models ConfigSource");

struct LineSink
{
	std::vector<std::string>* lines;

	void logRouted(const char* line) { lines->emplace_back(line); }
};

static_assert(ogsession::SessionLogSink<LineSink>, "the test's sink models SessionLogSink");

class RowManager
{
public:
	explicit RowManager(bool runsPrediction) : m_runsPrediction(runsPrediction) {}

	bool runsPrediction() const { return m_runsPrediction; }
	const TimeConfig& getTimeConfig() const { return m_timeConfig; }
	void setRecentreSettleSamples(uint32_t samples) { m_timeConfig.recentreSettleSamples = samples; }

private:
	bool       m_runsPrediction;
	TimeConfig m_timeConfig;
};

struct DummyOwner
{
	int32_t dummyKnobTicks = 3;
};

constexpr int32_t clampDummyKnobTicks(int32_t requested) { return requested < 0 ? 0 : (requested > 8 ? 8 : requested); }

inline constexpr knobTable::KnobRow<DummyOwner, int32_t> kDummyKnobRow{
	.intake = { .section = "OGNetcode", .key = "DummyKnobTicks" },
	.tag    = "DummyKnob.Config",
	.name   = "dummyKnobTicks",
	.clamp  = &clampDummyKnobTicks,
	.apply  = [](DummyOwner& owner, int32_t ticks) { owner.dummyKnobTicks = ticks; },
	.stored = [](const DummyOwner& owner) { return owner.dummyKnobTicks; },
};

inline constexpr knobTable::KnobRow<DummyOwner, int32_t> kOtherKnobRow{
	.intake = { .section = "OGNetcode", .key = "OtherKnobTicks" },
	.tag    = "DummyKnob.Config",
	.name   = "otherKnobTicks",
	.clamp  = &clampDummyKnobTicks,
	.apply  = [](DummyOwner&, int32_t) {},
	.stored = [](const DummyOwner&) { return int32_t{ 0 }; },
};

inline constexpr std::tuple kTableWithTheDummyRow{ kOtherKnobRow, kDummyKnobRow };
inline constexpr std::tuple kTableWithoutTheDummyRow{ kOtherKnobRow };

const MapConfigSource& dummyConfig()
{
	static const MapConfigSource config(std::map<std::string, std::string>{ { "DummyKnobTicks", "12" } });
	return config;
}

std::vector<std::string> recentreRowLines(std::optional<int32_t> requested, bool runsPrediction, uint32_t& stored)
{
	std::vector<std::string> lines;
	LineSink sink{ &lines };
	RowManager manager(runsPrediction);
	knobTable::applyKnob(ogsession::sessionKnobRows::kRecentreSettleSamples<RowManager>, requested, manager,
		ogsession::sessionKnobRows::worldOf(!runsPrediction), ogsession::sessionKnobsLines::knobRowSink(sink));
	stored = manager.getTimeConfig().recentreSettleSamples;
	return lines;
}
} // namespace

TEST_CASE("SessionKnobRows.TheRecentreRowPrintsTheKeptLineFormatsByteForByte", "[Session][SessionKnobs]")
{
	namespace lines = ogsession::sessionKnobsLines;
	const std::vector<int32_t> values = { std::numeric_limits<int32_t>::min(), -30, -3, -1, 0, 1, 7, 30, 31,
		std::numeric_limits<int32_t>::max() };
	uint32_t comparedLines = 0u;
	for (const bool runsPrediction : { false, true })
	{
		INFO((runsPrediction ? "prediction" : "authority"));
		uint32_t stored = 0u;
		const std::vector<std::string> absent = recentreRowLines(std::nullopt, runsPrediction, stored);
		CHECK(absent == std::vector<std::string>{ lines::formatLine(lines::kRecentreSession, TimeConfig{}.recentreSettleSamples,
			"compiled") });
		CHECK(stored == TimeConfig{}.recentreSettleSamples);
		comparedLines += static_cast<uint32_t>(absent.size());

		for (const int32_t value : values)
		{
			INFO(value);
			const uint32_t clamped = value < 0 ? 0u : static_cast<uint32_t>(value);
			std::vector<std::string> expected;
			if (value < 0)
				expected.push_back(lines::formatLine(lines::kRecentreClamped, value, 0));
			expected.push_back(lines::formatLine(lines::kRecentreSession, clamped, "ini"));
			const std::vector<std::string> actual = recentreRowLines(value, runsPrediction, stored);
			CHECK(actual == expected);
			CHECK(stored == clamped);
			comparedLines += static_cast<uint32_t>(actual.size());
		}
	}
	CHECK(comparedLines == 2u * (1u + 10u + 4u));
}

TEST_CASE("SessionKnobRows.ADummyKeyIsReadThroughMapConfigSourceByItsRowAlone", "[Session][SessionKnobs]")
{
	const RecordingReads<MapConfigSource> config(dummyConfig());
	std::vector<std::string> lines;
	LineSink sink{ &lines };
	DummyOwner owner;
	knobTable::applyKnobs(kTableWithTheDummyRow, config, owner, knobTable::KnobWorld::Prediction,
		ogsession::sessionKnobsLines::knobRowSink(sink));

	CHECK(owner.dummyKnobTicks == 8);
	CHECK(config.keys() == std::vector<std::string>{ "OtherKnobTicks", "DummyKnobTicks" });
	CHECK(lines == std::vector<std::string>{ "[Warning][DummyKnob.Config] otherKnobTicks=0 (compiled)",
		"[Warning][DummyKnob.Config] ini [OGNetcode] DummyKnobTicks=12 out of range, clamped to 8",
		"[Warning][DummyKnob.Config] dummyKnobTicks=8 (ini)" });
}

TEST_CASE("SessionKnobRows.WithoutItsRowTheDummyKeyIsNotRead", "[Session][SessionKnobs]")
{
	const RecordingReads<MapConfigSource> config(dummyConfig());
	std::vector<std::string> lines;
	LineSink sink{ &lines };
	DummyOwner owner;
	knobTable::applyKnobs(kTableWithoutTheDummyRow, config, owner, knobTable::KnobWorld::Prediction,
		ogsession::sessionKnobsLines::knobRowSink(sink));
	const ogsession::SessionKnobs sessionKnobs = ogsession::readSessionKnobs(config, true);

	CHECK(owner.dummyKnobTicks == 3);
	CHECK(lines == std::vector<std::string>{ "[Warning][DummyKnob.Config] otherKnobTicks=0 (compiled)" });
	CHECK(config.keys().size() == 15u);
	CHECK(config.keys().front() == "OtherKnobTicks");
	for (const std::string& key : config.keys())
	{
		INFO(key);
		CHECK(key != "DummyKnobTicks");
	}
	CHECK_FALSE(sessionKnobs.hasIniRecentreSettleSamples);
}
