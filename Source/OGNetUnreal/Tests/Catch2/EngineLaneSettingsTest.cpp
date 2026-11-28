// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "OGNetUnreal/Core/EngineLaneSettings.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Backend/EmulatorProfile.h"
#include "OGNet/Connection/Connection.h"
#include "OGNet/Crypto/Crypto.h"
#include "OGNet/Endpoint/EngineLane.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetConcepts.h"
#include "OGNet/NetTypes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>

namespace
{
using ognet::Address;
using ognet::BindConfig;
using ognet::DirectionConfig;
using ognet::EmulatedBackend;
using ognet::EmulatorClockFn;
using ognet::EmulatorConfig;
using ognet::EmulatorProfile;
using ognet::RecvFn;
using ognet::Role;
using ognet::SendResult;
using ognet::connection::ConnectionTimings;
using ognet::connection::TimingsIssue;
using ognet::emulator::SplitMix64;
using ognetunreal::EmulatorIntake;
using ognetunreal::EnginePacketSimulation;
using ognetunreal::IgnoredSetting;

struct InertBackend
{
	bool open(const BindConfig&) { return true; }
	SendResult sendTo(const Address&, std::span<const std::byte>) { return SendResult::Ok; }
	size_t receive(uint32_t, RecvFn) { return 0; }
	bool rebind() { return true; }
	void wake() {}
};

static_assert(ognet::DatagramBackend<InertBackend>, "InertBackend must satisfy ognet::DatagramBackend");

double zeroClock(void*)
{
	return 0.0;
}

bool sameDirection(const DirectionConfig& a, const DirectionConfig& b)
{
	return a.lossPercent == b.lossPercent && a.lagMinMs == b.lagMinMs && a.lagMaxMs == b.lagMaxMs
		&& a.duplicatePercent == b.duplicatePercent && a.reorder == b.reorder && a.burst.enterBadPercent == b.burst.enterBadPercent
		&& a.burst.exitBadPercent == b.burst.exitBadPercent && a.burst.lossInBadPercent == b.burst.lossInBadPercent;
}

bool sameProfile(const EmulatorProfile& a, const EmulatorProfile& b)
{
	return a.seed == b.seed && sameDirection(a.up, b.up) && sameDirection(a.down, b.down);
}

DirectionConfig direction(double loss, double lagMin, double lagMax, double duplicate)
{
	DirectionConfig d;
	d.lossPercent = loss;
	d.lagMinMs = lagMin;
	d.lagMaxMs = lagMax;
	d.duplicatePercent = duplicate;
	return d;
}

const DirectionConfig kQuiet{};

struct Row
{
	const char* name;
	EnginePacketSimulation settings;
	DirectionConfig outbound;
	DirectionConfig inbound;
	bool active;
};

int32_t anySetting(SplitMix64& rng)
{
	static constexpr std::array<int32_t, 14> kEdges{
		INT32_MIN, -60001, -100, -1, 0, 1, 15, 50, 99, 100, 101, 60000, 60001, INT32_MAX};
	const uint64_t draw = rng.next();
	if ((draw & 1) == 0)
	{
		return kEdges[(draw >> 1) % kEdges.size()];
	}
	return static_cast<int32_t>(static_cast<uint32_t>(draw >> 32));
}

EnginePacketSimulation anySettings(SplitMix64& rng)
{
	EnginePacketSimulation s;
	for (int32_t* field : {&s.PktLoss, &s.PktLossMaxSize, &s.PktLossMinSize, &s.PktOrder, &s.PktDup, &s.PktLag, &s.PktLagVariance,
			 &s.PktLagMin, &s.PktLagMax, &s.PktIncomingLagMin, &s.PktIncomingLagMax, &s.PktIncomingLoss, &s.PktJitter, &s.PktFrameDelay,
			 &s.PktIncomingFrameDelay})
	{
		*field = (rng.next() % 3 == 0) ? anySetting(rng) : 0;
	}
	return s;
}
} // namespace

TEST_CASE("EngineLaneSettings.EveryRowOfTheIntakeTableMapsAsDesigned", "[OGNet][EngineLaneDriver]")
{
	const std::array<Row, 17> rows{{
		{"PktLoss", {.PktLoss = 7}, direction(7, 0, 0, 0), kQuiet, true},
		{"PktLag+-PktLagVariance", {.PktLag = 40, .PktLagVariance = 10}, direction(0, 30, 50, 0), kQuiet, true},
		{"PktLag alone", {.PktLag = 25}, direction(0, 25, 25, 0), kQuiet, true},
		{"PktLag-PktLagVariance clamps at 0", {.PktLag = 5, .PktLagVariance = 10}, direction(0, 0, 15, 0), kQuiet, true},
		{"a negative PktLagVariance spreads as its magnitude", {.PktLag = 40, .PktLagVariance = -10}, direction(0, 30, 50, 0), kQuiet, true},
		{"PktLag wins over PktLagMin/Max", {.PktLag = 40, .PktLagVariance = 10, .PktLagMin = 5, .PktLagMax = 6}, direction(0, 30, 50, 0),
			kQuiet, true},
		{"PktLagMin/Max", {.PktLagMin = 15, .PktLagMax = 50}, direction(0, 15, 50, 0), kQuiet, true},
		{"PktLagMax below PktLagMin", {.PktLagMin = 50, .PktLagMax = 15}, direction(0, 50, 50, 0), kQuiet, true},
		{"PktLagMax alone", {.PktLagMax = 20}, direction(0, 0, 20, 0), kQuiet, true},
		{"PktLagVariance without PktLag", {.PktLagVariance = 10, .PktLagMin = 15, .PktLagMax = 50}, direction(0, 15, 50, 0), kQuiet, true},
		{"PktIncomingLagMin/Max", {.PktIncomingLagMin = 30, .PktIncomingLagMax = 40}, kQuiet, direction(0, 30, 40, 0), true},
		{"PktIncomingLagMax below PktIncomingLagMin", {.PktIncomingLagMin = 40, .PktIncomingLagMax = 30}, kQuiet, direction(0, 40, 40, 0),
			true},
		{"PktIncomingLoss", {.PktIncomingLoss = 2}, kQuiet, direction(2, 0, 0, 0), true},
		{"PktDup", {.PktDup = 3}, direction(0, 0, 0, 3), kQuiet, true},
		{"percentages clamp to [0, 100]", {.PktLoss = 150, .PktDup = -5, .PktIncomingLoss = 101}, direction(100, 0, 0, 0),
			direction(100, 0, 0, 0), true},
		{"negative lags clamp to 0", {.PktLag = -40, .PktLagMin = -10, .PktLagMax = -5, .PktIncomingLagMin = -1, .PktIncomingLagMax = -1},
			kQuiet, kQuiet, false},
		{"lags clamp to the emulator's maximum", {.PktLag = 70000, .PktLagVariance = 20000, .PktIncomingLagMin = 59000,
			.PktIncomingLagMax = INT32_MAX}, direction(0, 50000, 60000, 0), direction(0, 59000, 60000, 0), true},
	}};
	for (const Row& row : rows)
	{
		INFO(row.name);
		const EmulatorIntake intake = ognetunreal::mapPacketSimulation(row.settings, 1);
		CHECK(sameDirection(intake.config.outbound, row.outbound));
		CHECK(sameDirection(intake.config.inbound, row.inbound));
		CHECK(intake.active == row.active);
		CHECK(intake.ignoredMask == 0);
		CHECK_FALSE(intake.config.outbound.reorder);
		CHECK_FALSE(intake.config.inbound.reorder);
	}
}

TEST_CASE("EngineLaneSettings.B0sFlagsMapToOut15To50Ms1PercentAndIn15To50Ms2Percent", "[OGNet][EngineLaneDriver]")
{
	const EmulatorIntake intake = ognetunreal::mapPacketSimulation(ognetunreal::kB0PacketSimulation, 9);
	CHECK(sameDirection(intake.config.outbound, direction(1, 15, 50, 0)));
	CHECK(sameDirection(intake.config.inbound, direction(2, 15, 50, 0)));
	CHECK(intake.active);
	CHECK(intake.ignoredMask == 0);
	CHECK(intake.config.seed == 9);
}

TEST_CASE("EngineLaneSettings.EachIgnoredSettingAloneMapsToNothingAndSetsOnlyItsBit", "[OGNet][EngineLaneDriver]")
{
	struct Ignored
	{
		IgnoredSetting setting;
		const char* name;
		int32_t EnginePacketSimulation::* field;
	};
	const std::array<Ignored, 6> ignored{{
		{IgnoredSetting::PktOrder, "PktOrder", &EnginePacketSimulation::PktOrder},
		{IgnoredSetting::PktJitter, "PktJitter", &EnginePacketSimulation::PktJitter},
		{IgnoredSetting::PktLossMinSize, "PktLossMinSize", &EnginePacketSimulation::PktLossMinSize},
		{IgnoredSetting::PktLossMaxSize, "PktLossMaxSize", &EnginePacketSimulation::PktLossMaxSize},
		{IgnoredSetting::PktFrameDelay, "PktFrameDelay", &EnginePacketSimulation::PktFrameDelay},
		{IgnoredSetting::PktIncomingFrameDelay, "PktIncomingFrameDelay", &EnginePacketSimulation::PktIncomingFrameDelay},
	}};
	CHECK(ignored.size() == ognetunreal::kIgnoredSettings.size());
	for (const Ignored& entry : ignored)
	{
		INFO(entry.name);
		CHECK(std::string{ognetunreal::ignoredName(entry.setting)} == entry.name);
		CHECK(std::find(ognetunreal::kIgnoredSettings.begin(), ognetunreal::kIgnoredSettings.end(), entry.setting)
			!= ognetunreal::kIgnoredSettings.end());
		for (const int32_t value : {1, -1, 100})
		{
			EnginePacketSimulation settings;
			settings.*entry.field = value;
			const EmulatorIntake intake = ognetunreal::mapPacketSimulation(settings, 1);
			CHECK_FALSE(intake.active);
			CHECK(intake.ignoredMask == static_cast<uint16_t>(entry.setting));
			CHECK(sameDirection(intake.config.outbound, kQuiet));
			CHECK(sameDirection(intake.config.inbound, kQuiet));
		}
	}
	EnginePacketSimulation all;
	for (const Ignored& entry : ignored)
	{
		all.*entry.field = 1;
	}
	CHECK(ognetunreal::mapPacketSimulation(all, 1).ignoredMask == 63);
	CHECK(std::string{ognetunreal::ignoredName(static_cast<IgnoredSetting>(64))} == "Unknown");
}

TEST_CASE("EngineLaneSettings.AnIgnoredSettingBesideAMappedOneLeavesTheMappingAlone", "[OGNet][EngineLaneDriver]")
{
	const EmulatorIntake intake =
		ognetunreal::mapPacketSimulation(EnginePacketSimulation{.PktLoss = 4, .PktLag = 20, .PktJitter = 30, .PktFrameDelay = 2}, 1);
	CHECK(sameDirection(intake.config.outbound, direction(4, 20, 20, 0)));
	CHECK(sameDirection(intake.config.inbound, kQuiet));
	CHECK(intake.active);
	CHECK(intake.ignoredMask == (static_cast<uint16_t>(IgnoredSetting::PktJitter) | static_cast<uint16_t>(IgnoredSetting::PktFrameDelay)));
}

TEST_CASE("EngineLaneSettings.TheServersProfileOfB0sFlagsIsOgNetsB0Preset", "[OGNet][EngineLaneDriver]")
{
	const EmulatorIntake intake = ognetunreal::mapPacketSimulation(ognetunreal::kB0PacketSimulation, 7);
	const EmulatorProfile server = ognetunreal::emulatorProfileFor(intake, Role::Server);
	const ognet::ProfileParseResult b0 = ognet::parseEmulatorProfile("b0");
	REQUIRE(b0.ok);
	CHECK(sameProfile(server, b0.profile));
	CHECK(ognet::canonicalText(server)
		== "seed=7,up.loss=2,up.lag=15-50,up.dup=0,up.reorder=0,up.burst=0/0/0,down.loss=1,down.lag=15-50,down.dup=0,down.reorder=0,"
		   "down.burst=0/0/0");

	const EmulatorProfile client = ognetunreal::emulatorProfileFor(intake, Role::Client);
	CHECK(sameDirection(client.up, intake.config.outbound));
	CHECK(sameDirection(client.down, intake.config.inbound));
	CHECK(ognet::canonicalText(ognetunreal::emulatorProfileFor(ognetunreal::mapPacketSimulation(ognetunreal::kB0PacketSimulation, 1), Role::Client))
		== "seed=1,up.loss=1,up.lag=15-50,up.dup=0,up.reorder=0,up.burst=0/0/0,down.loss=2,down.lag=15-50,down.dup=0,down.reorder=0,"
		   "down.burst=0/0/0");
}

TEST_CASE("EngineLaneSettings.EveryIntakeIsValidRoundTripsThroughItsProfileAndConfiguresAnEmulator", "[OGNet][EngineLaneDriver]")
{
	SplitMix64 rng(0x5EED4002ull);
	InertBackend inner;
	int activeCount = 0;
	for (int i = 0; i < 10000; ++i)
	{
		const EnginePacketSimulation settings = anySettings(rng);
		const EmulatorIntake intake = ognetunreal::mapPacketSimulation(settings, rng.next());
		INFO("iteration " << i);
		REQUIRE(ognet::emulator::isValid(intake.config.outbound));
		REQUIRE(ognet::emulator::isValid(intake.config.inbound));
		activeCount += intake.active ? 1 : 0;
		for (const Role role : {Role::Client, Role::Server})
		{
			const EmulatorProfile profile = ognetunreal::emulatorProfileFor(intake, role);
			const std::optional<EmulatorConfig> config = ognet::toEmulatorConfig(profile, role);
			REQUIRE(config.has_value());
			CHECK(config->seed == intake.config.seed);
			CHECK(sameDirection(config->outbound, intake.config.outbound));
			CHECK(sameDirection(config->inbound, intake.config.inbound));
			const std::string text = ognet::canonicalText(profile);
			const ognet::ProfileParseResult parsed = ognet::parseEmulatorProfile(text);
			INFO(text);
			REQUIRE(parsed.ok);
			CHECK(sameProfile(parsed.profile, profile));
		}
		if (i % 100 == 0)
		{
			EmulatedBackend<InertBackend> emulated(inner, EmulatorClockFn{&zeroClock, nullptr});
			CHECK(emulated.configure(intake.config));
		}
	}
	CHECK(activeCount > 1000);
	CHECK(activeCount < 10000);
}

TEST_CASE("EngineLaneSettings.TimingsForEveryEngineTimeoutOnTheGridAreValidAndOutlastUnreal", "[OGNet][EngineLaneDriver]")
{
	const ConnectionTimings defaults;
	for (const double initial : {0.5, 10.0, 60.0, 120.0})
	{
		for (const double connection : {0.5, 10.0, 60.0})
		{
			for (const double multiplier : {0.0, 1.0, 2.0})
			{
				INFO("initial " << initial << " connection " << connection << " multiplier " << multiplier);
				const ConnectionTimings timings = ognetunreal::timingsFor({initial, connection, multiplier});
				CHECK(ognet::connection::validateTimings(timings) == TimingsIssue::None);
				const double engine = std::max(initial, connection) * std::max(1.0, multiplier);
				CHECK(timings.idleTimeoutSeconds == std::max(defaults.idleTimeoutSeconds, engine + ognetunreal::kTimeoutMarginSeconds));
				CHECK(timings.idleTimeoutSeconds > engine);
				CHECK(timings.resumeWindowSeconds == std::max(defaults.resumeWindowSeconds, timings.idleTimeoutSeconds));
				CHECK(timings.keepaliveSeconds == defaults.keepaliveSeconds);
				CHECK(timings.maxAckDelaySeconds == defaults.maxAckDelaySeconds);
				CHECK(timings.silenceProbeSeconds == defaults.silenceProbeSeconds);
				CHECK(timings.probeIntervalSeconds == defaults.probeIntervalSeconds);
				CHECK(timings.silenceRebindSeconds == defaults.silenceRebindSeconds);
				CHECK(timings.minRebindIntervalSeconds == defaults.minRebindIntervalSeconds);
				CHECK(timings.suspensionGapSeconds == defaults.suspensionGapSeconds);
			}
		}
	}
	CHECK(ognetunreal::timingsFor({60.0, 10.0, 1.0}).idleTimeoutSeconds == 65.0);
	CHECK(ognetunreal::timingsFor({60.0, 60.0, 1.0}).resumeWindowSeconds == 65.0);
	CHECK(ognetunreal::timingsFor({120.0, 60.0, 2.0}).idleTimeoutSeconds == 245.0);
	CHECK(ognetunreal::timingsFor({0.5, 0.5, 0.0}).idleTimeoutSeconds == 5.5);
	CHECK(ognetunreal::timingsFor({0.0, 0.0, 0.0}).idleTimeoutSeconds == defaults.idleTimeoutSeconds);
	CHECK(ognetunreal::timingsFor({0.0, 0.0, 0.0}).resumeWindowSeconds == defaults.resumeWindowSeconds);
}

TEST_CASE("EngineLaneSettings.TheBudgetIsAQuarterAboveTheLargestUnrealRateAndNeverBelowOgNetsDefault", "[OGNet][EngineLaneDriver]")
{
	struct BudgetRow
	{
		std::array<uint32_t, 4> rates;
		uint32_t bytesPerSecond;
	};
	const std::array<BudgetRow, 8> rows{{
		{{250000, 250000, 1000000, 1000000}, 1250000},
		{{1000000, 250000, 250000, 250000}, 1250000},
		{{250000, 1000001, 0, 0}, 1250002},
		{{0, 0, 0, 3000000}, 3750000},
		{{100000, 100000, 100000, 100000}, 250000},
		{{0, 0, 0, 0}, 250000},
		{{200001, 0, 0, 0}, 250002},
		{{UINT32_MAX, 0, 0, 0}, UINT32_MAX},
	}};
	for (const BudgetRow& row : rows)
	{
		INFO(row.rates[0] << " " << row.rates[1] << " " << row.rates[2] << " " << row.rates[3]);
		const ognet::lanes::BudgetConfig budget = ognetunreal::budgetFor(row.rates[0], row.rates[1], row.rates[2], row.rates[3]);
		CHECK(budget.bytesPerSecond == row.bytesPerSecond);
		CHECK(budget.burstBytes == 65536);
		CHECK(ognet::lanes::isValidBudget(budget));
	}
}

TEST_CASE("EngineLaneSettings.TheBuildIdentityIsTheEngineLaneLabelZeroPadded", "[OGNet][EngineLaneDriver]")
{
	const std::string label = "og-net:ue-engine-lane:ph2";
	CHECK(std::string{ognetunreal::kEngineLaneBuildLabel} == label);
	const ognet::crypto::Bytes32& identity = ognetunreal::kEngineLaneBuildIdentity;
	for (std::size_t i = 0; i < identity.size(); ++i)
	{
		INFO("byte " << i);
		CHECK(identity[i] == (i < label.size() ? static_cast<uint8_t>(label[i]) : uint8_t{0}));
	}
}

TEST_CASE("EngineLaneSettings.TheEnginePacketConstantsAreUIpConnectionsAndFitTheEngineLane", "[OGNet][EngineLaneDriver]")
{
	CHECK(ognetunreal::kEngineMaxPacketBytes == 1024);
	CHECK(ognetunreal::kEnginePacketOverheadBytes == 28);
	CHECK(ognet::EngineLane::maxPacketBytes() >= static_cast<std::size_t>(ognetunreal::kEngineMaxPacketBytes));
}

TEST_CASE("EngineLaneSettings.NumericJoinHostsParseAndNamesDoNot", "[OGNet][EngineLaneDriver]")
{
	const std::optional<Address> loopback = ognetunreal::numericJoinAddress("127.0.0.1", 7851);
	REQUIRE(loopback.has_value());
	CHECK(loopback->family == 4);
	CHECK(loopback->port == 7851);
	CHECK(ognet::formatAddress(*loopback) == "127.0.0.1:7851");

	const std::optional<Address> lan = ognetunreal::numericJoinAddress("192.168.1.42", 7777);
	REQUIRE(lan.has_value());
	CHECK(lan->family == 4);
	CHECK(lan->port == 7777);
	CHECK(ognet::formatAddress(*lan) == "192.168.1.42:7777");

	const std::optional<Address> bare = ognetunreal::numericJoinAddress("::1", 7777);
	const std::optional<Address> bracketed = ognetunreal::numericJoinAddress("[::1]", 7777);
	REQUIRE(bare.has_value());
	REQUIRE(bracketed.has_value());
	CHECK(bare->family == 6);
	CHECK(bare->port == 7777);
	CHECK(ognet::formatAddress(*bare) == "[::1]:7777");
	CHECK(*bare == *bracketed);

	CHECK_FALSE(ognetunreal::numericJoinAddress("localhost", 7777).has_value());
	CHECK_FALSE(ognetunreal::numericJoinAddress("999.1.1.1", 7777).has_value());
	CHECK_FALSE(ognetunreal::numericJoinAddress("", 7777).has_value());
	CHECK_FALSE(ognetunreal::numericJoinAddress("127.0.0.1:7777", 7777).has_value());
	CHECK_FALSE(ognetunreal::numericJoinAddress(std::string(65, '1'), 7777).has_value());

	const std::optional<Address> highPort = ognetunreal::numericJoinAddress("10.0.0.1", 65535);
	REQUIRE(highPort.has_value());
	CHECK(highPort->port == 65535);
	const std::optional<Address> portZero = ognetunreal::numericJoinAddress("10.0.0.1", 0);
	REQUIRE(portZero.has_value());
	CHECK(portZero->port == 0);
}
