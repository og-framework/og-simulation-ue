#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/EngineLaneSettings-rationale.md

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Backend/EmulatorProfile.h"
#include "OGNet/Connection/Connection.h"
#include "OGNet/Crypto/Crypto.h"
#include "OGNet/Endpoint/EngineLane.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetTypes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>

namespace ognetunreal
{
struct EnginePacketSimulation
{
	int32_t PktLoss = 0;
	int32_t PktLossMaxSize = 0;
	int32_t PktLossMinSize = 0;
	int32_t PktOrder = 0;
	int32_t PktDup = 0;
	int32_t PktLag = 0;
	int32_t PktLagVariance = 0;
	int32_t PktLagMin = 0;
	int32_t PktLagMax = 0;
	int32_t PktIncomingLagMin = 0;
	int32_t PktIncomingLagMax = 0;
	int32_t PktIncomingLoss = 0;
	int32_t PktJitter = 0;
	int32_t PktFrameDelay = 0;
	int32_t PktIncomingFrameDelay = 0;
};

static_assert(sizeof(EnginePacketSimulation) == 15 * sizeof(int32_t),
	"EnginePacketSimulation mirrors the 15 int32 fields of UE's FPacketSimulationSettings, in their order");

enum class IgnoredSetting : uint16_t
{
	PktOrder = 1,
	PktJitter = 2,
	PktLossMinSize = 4,
	PktLossMaxSize = 8,
	PktFrameDelay = 16,
	PktIncomingFrameDelay = 32,
};

inline constexpr std::array<IgnoredSetting, 6> kIgnoredSettings{IgnoredSetting::PktOrder, IgnoredSetting::PktJitter,
	IgnoredSetting::PktLossMinSize, IgnoredSetting::PktLossMaxSize, IgnoredSetting::PktFrameDelay,
	IgnoredSetting::PktIncomingFrameDelay};

constexpr const char* ignoredName(IgnoredSetting setting)
{
	switch (setting)
	{
	case IgnoredSetting::PktOrder:
		return "PktOrder";
	case IgnoredSetting::PktJitter:
		return "PktJitter";
	case IgnoredSetting::PktLossMinSize:
		return "PktLossMinSize";
	case IgnoredSetting::PktLossMaxSize:
		return "PktLossMaxSize";
	case IgnoredSetting::PktFrameDelay:
		return "PktFrameDelay";
	case IgnoredSetting::PktIncomingFrameDelay:
		return "PktIncomingFrameDelay";
	}
	return "Unknown";
}

struct EmulatorIntake
{
	ognet::EmulatorConfig config;
	bool active = false;
	uint16_t ignoredMask = 0;
};

namespace detail
{
constexpr bool ignoredSettingsAreDistinctBits()
{
	uint32_t seen = 0;
	for (const IgnoredSetting setting : kIgnoredSettings)
	{
		const uint32_t bit = static_cast<uint32_t>(setting);
		if (bit == 0 || (bit & (bit - 1)) != 0 || (seen & bit) != 0 || std::string_view{ignoredName(setting)} == "Unknown")
		{
			return false;
		}
		seen |= bit;
	}
	return seen == (1u << kIgnoredSettings.size()) - 1;
}

static_assert(ignoredSettingsAreDistinctBits(),
	"every IgnoredSetting is one distinct bit of ignoredMask, listed once in kIgnoredSettings and named by ignoredName");

constexpr double percentFrom(int32_t value)
{
	return static_cast<double>(std::clamp<int32_t>(value, 0, 100));
}

constexpr double lagMsFrom(int64_t value)
{
	return static_cast<double>(std::clamp<int64_t>(value, 0, static_cast<int64_t>(ognet::kMaxEmulatedLagMs)));
}

constexpr bool isZero(const ognet::DirectionConfig& direction)
{
	return direction.lossPercent == 0.0 && direction.lagMinMs == 0.0 && direction.lagMaxMs == 0.0
		&& direction.duplicatePercent == 0.0;
}

constexpr uint16_t bitIfSet(int32_t value, IgnoredSetting setting)
{
	return value != 0 ? static_cast<uint16_t>(setting) : uint16_t{0};
}

constexpr bool directionIs(const ognet::DirectionConfig& direction, double loss, double lagMin, double lagMax, double duplicate)
{
	return direction.lossPercent == loss && direction.lagMinMs == lagMin && direction.lagMaxMs == lagMax
		&& direction.duplicatePercent == duplicate && !direction.reorder && direction.burst.enterBadPercent == 0.0
		&& direction.burst.exitBadPercent == 0.0 && direction.burst.lossInBadPercent == 0.0;
}
} // namespace detail

constexpr EmulatorIntake mapPacketSimulation(const EnginePacketSimulation& settings, uint64_t seed)
{
	EmulatorIntake intake;
	intake.config.seed = seed;

	ognet::DirectionConfig& outbound = intake.config.outbound;
	outbound.lossPercent = detail::percentFrom(settings.PktLoss);
	if (settings.PktLag > 0)
	{
		const int64_t lag = settings.PktLag;
		const int64_t variance = settings.PktLagVariance < 0 ? -int64_t{settings.PktLagVariance} : int64_t{settings.PktLagVariance};
		outbound.lagMinMs = detail::lagMsFrom(lag - variance);
		outbound.lagMaxMs = detail::lagMsFrom(lag + variance);
	}
	else
	{
		outbound.lagMinMs = detail::lagMsFrom(settings.PktLagMin);
		outbound.lagMaxMs = std::max(outbound.lagMinMs, detail::lagMsFrom(settings.PktLagMax));
	}
	outbound.duplicatePercent = detail::percentFrom(settings.PktDup);
	outbound.reorder = false;

	ognet::DirectionConfig& inbound = intake.config.inbound;
	inbound.lossPercent = detail::percentFrom(settings.PktIncomingLoss);
	inbound.lagMinMs = detail::lagMsFrom(settings.PktIncomingLagMin);
	inbound.lagMaxMs = std::max(inbound.lagMinMs, detail::lagMsFrom(settings.PktIncomingLagMax));
	inbound.reorder = false;

	intake.active = !detail::isZero(outbound) || !detail::isZero(inbound);
	intake.ignoredMask = static_cast<uint16_t>(detail::bitIfSet(settings.PktOrder, IgnoredSetting::PktOrder)
		| detail::bitIfSet(settings.PktJitter, IgnoredSetting::PktJitter)
		| detail::bitIfSet(settings.PktLossMinSize, IgnoredSetting::PktLossMinSize)
		| detail::bitIfSet(settings.PktLossMaxSize, IgnoredSetting::PktLossMaxSize)
		| detail::bitIfSet(settings.PktFrameDelay, IgnoredSetting::PktFrameDelay)
		| detail::bitIfSet(settings.PktIncomingFrameDelay, IgnoredSetting::PktIncomingFrameDelay));
	return intake;
}

constexpr ognet::EmulatorProfile emulatorProfileFor(const EmulatorIntake& intake, ognet::Role role)
{
	ognet::EmulatorProfile profile;
	profile.seed = intake.config.seed;
	profile.up = role == ognet::Role::Client ? intake.config.outbound : intake.config.inbound;
	profile.down = role == ognet::Role::Client ? intake.config.inbound : intake.config.outbound;
	return profile;
}

inline constexpr EnginePacketSimulation kB0PacketSimulation{.PktLoss = 1, .PktLagMin = 15, .PktLagMax = 50,
	.PktIncomingLagMin = 15, .PktIncomingLagMax = 50, .PktIncomingLoss = 2};

static_assert(detail::directionIs(mapPacketSimulation(kB0PacketSimulation, 1).config.outbound, 1.0, 15.0, 50.0, 0.0)
		&& detail::directionIs(mapPacketSimulation(kB0PacketSimulation, 1).config.inbound, 2.0, 15.0, 50.0, 0.0)
		&& mapPacketSimulation(kB0PacketSimulation, 1).active && mapPacketSimulation(kB0PacketSimulation, 1).ignoredMask == 0,
	"B0's flags (PktLagMin=15 PktLagMax=50 PktLoss=1 PktIncomingLagMin=15 PktIncomingLagMax=50 PktIncomingLoss=2) map to "
	"out 15-50 ms 1 % and in 15-50 ms 2 %");
static_assert(detail::directionIs(
				  mapPacketSimulation(EnginePacketSimulation{.PktLagMin = 10, .PktLagMax = 20, .PktIncomingLagMin = 30, .PktIncomingLagMax = 40}, 1)
					  .config.outbound,
				  0.0, 10.0, 20.0, 0.0)
		&& detail::directionIs(
			mapPacketSimulation(EnginePacketSimulation{.PktLagMin = 10, .PktLagMax = 20, .PktIncomingLagMin = 30, .PktIncomingLagMax = 40}, 1)
				.config.inbound,
			0.0, 30.0, 40.0, 0.0),
	"PktLagMin/Max shape the outbound direction and PktIncomingLagMin/Max the inbound one");
static_assert(detail::directionIs(
				  mapPacketSimulation(EnginePacketSimulation{.PktLag = 40, .PktLagVariance = 10, .PktLagMin = 5, .PktLagMax = 6}, 1).config.outbound,
				  0.0, 30.0, 50.0, 0.0),
	"PktLag wins over PktLagMin/Max, as in UE's packet simulation: PktLag=40 PktLagVariance=10 PktLagMin=5 PktLagMax=6 gives "
	"out 30-50 ms");
static_assert(detail::directionIs(mapPacketSimulation(EnginePacketSimulation{.PktLag = 5, .PktLagVariance = 10}, 1).config.outbound,
				  0.0, 0.0, 15.0, 0.0),
	"PktLag minus PktLagVariance is clamped at 0: PktLag=5 PktLagVariance=10 gives out 0-15 ms");
static_assert(!mapPacketSimulation(EnginePacketSimulation{}, 1).active && mapPacketSimulation(EnginePacketSimulation{}, 1).ignoredMask == 0,
	"all-zero settings give an inactive intake with nothing ignored");
static_assert(!mapPacketSimulation(EnginePacketSimulation{.PktOrder = 1}, 1).active
		&& mapPacketSimulation(EnginePacketSimulation{.PktOrder = 1}, 1).ignoredMask == static_cast<uint16_t>(IgnoredSetting::PktOrder),
	"PktOrder alone maps to nothing: the intake is inactive and only the PktOrder bit is set");
static_assert(ognet::emulator::isValid(mapPacketSimulation(kB0PacketSimulation, 1).config.outbound)
		&& ognet::emulator::isValid(mapPacketSimulation(kB0PacketSimulation, 1).config.inbound)
		&& ognet::emulator::isValid(mapPacketSimulation(EnginePacketSimulation{.PktLag = 40, .PktLagVariance = 10, .PktLagMin = 5, .PktLagMax = 6}, 1).config.outbound)
		&& ognet::emulator::isValid(mapPacketSimulation(EnginePacketSimulation{.PktLag = 5, .PktLagVariance = 10}, 1).config.outbound)
		&& ognet::emulator::isValid(mapPacketSimulation(EnginePacketSimulation{.PktLoss = 500, .PktDup = -3, .PktLag = INT32_MAX,
			.PktLagVariance = INT32_MIN, .PktIncomingLagMin = INT32_MAX, .PktIncomingLagMax = -1, .PktIncomingLoss = INT32_MIN}, 1).config.outbound)
		&& ognet::emulator::isValid(mapPacketSimulation(EnginePacketSimulation{.PktLoss = 500, .PktDup = -3, .PktLag = INT32_MAX,
			.PktLagVariance = INT32_MIN, .PktIncomingLagMin = INT32_MAX, .PktIncomingLagMax = -1, .PktIncomingLoss = INT32_MIN}, 1).config.inbound),
	"every mapped direction passes ognet::emulator::isValid, the check EmulatedBackend::configure makes, extreme settings included");

struct EngineTimeouts
{
	double initialConnectTimeout = 0.0;
	double connectionTimeout = 0.0;
	double multiplier = 0.0;
};

inline constexpr double kTimeoutMarginSeconds = 5.0;

constexpr ognet::connection::ConnectionTimings timingsFor(const EngineTimeouts& timeouts)
{
	ognet::connection::ConnectionTimings timings;
	const double engineTimeout =
		std::max(timeouts.initialConnectTimeout, timeouts.connectionTimeout) * std::max(1.0, timeouts.multiplier);
	timings.idleTimeoutSeconds = std::max(ognet::connection::ConnectionTimings{}.idleTimeoutSeconds, engineTimeout + kTimeoutMarginSeconds);
	timings.resumeWindowSeconds = std::max(ognet::connection::ConnectionTimings{}.resumeWindowSeconds, timings.idleTimeoutSeconds);
	return timings;
}

static_assert(ognet::connection::validateTimings(timingsFor({60.0, 60.0, 1.0})) == ognet::connection::TimingsIssue::None,
	"UE's default timeouts (60 s, 60 s, multiplier 1) give timings og-net accepts");
static_assert(timingsFor({60.0, 10.0, 1.0}).idleTimeoutSeconds == 65.0,
	"og-net's idle timeout is UE's longest timeout times the multiplier plus kTimeoutMarginSeconds (60 s + 5 s), so UE "
	"always times a connection out first");

inline constexpr uint32_t kBudgetHeadroomPercent = 125;
inline constexpr uint32_t kEngineLaneBurstBytes = 65536;

constexpr ognet::lanes::BudgetConfig budgetFor(
	uint32_t maxClientRate, uint32_t maxInternetClientRate, uint32_t configuredInternetSpeed, uint32_t configuredLanSpeed)
{
	const uint64_t largest = std::max({maxClientRate, maxInternetClientRate, configuredInternetSpeed, configuredLanSpeed});
	const uint64_t withHeadroom = (largest * kBudgetHeadroomPercent + 99) / 100;
	ognet::lanes::BudgetConfig budget;
	budget.bytesPerSecond = static_cast<uint32_t>(
		std::clamp<uint64_t>(withHeadroom, ognet::lanes::kDefaultBudget.bytesPerSecond, uint64_t{UINT32_MAX}));
	budget.burstBytes = kEngineLaneBurstBytes;
	return budget;
}

static_assert(budgetFor(250000, 250000, 1000000, 1000000).bytesPerSecond == 1250000
		&& ognet::lanes::isValidBudget(budgetFor(250000, 250000, 1000000, 1000000)),
	"the project's rates (MaxClientRate 250,000, ConfiguredInternetSpeed 1,000,000) give og-net a valid budget of "
	"1,250,000 B/s, above every UE rate");

inline constexpr std::string_view kEngineLaneBuildLabel = "og-net:ue-engine-lane:ph2";

static_assert(kEngineLaneBuildLabel.size() <= std::tuple_size_v<ognet::crypto::Bytes32>,
	"the engine lane's build label must fit the 32-byte build identity og-net's Hello carries, zero-padded");

constexpr ognet::crypto::Bytes32 buildIdentityFromLabel(std::string_view label)
{
	ognet::crypto::Bytes32 identity{};
	for (std::size_t i = 0; i < label.size() && i < identity.size(); ++i)
	{
		identity[i] = static_cast<uint8_t>(label[i]);
	}
	return identity;
}

inline constexpr ognet::crypto::Bytes32 kEngineLaneBuildIdentity = buildIdentityFromLabel(kEngineLaneBuildLabel);

inline constexpr int32_t kEngineMaxPacketBytes = 1024;
static_assert(ognet::EngineLane::maxPacketBytes() >= static_cast<std::size_t>(kEngineMaxPacketBytes),
	"one full UE packet (UIpConnection's MaxPacket, 1,024 B) must fit one engine-lane message");

inline constexpr int32_t kEnginePacketOverheadBytes = 28;
static_assert(kEnginePacketOverheadBytes == 20 + 8,
	"UIpConnection's UDP_HEADER_SIZE (an IPv4 header and a UDP header): the OG connection reports the same overhead so UE's "
	"rate accounting matches IpNetDriver's; og-net's own header and tag are not counted");

inline constexpr std::size_t kMaxJoinHostChars = 64;

constexpr std::optional<ognet::Address> numericJoinAddress(std::string_view host, uint16_t port)
{
	if (host.size() > kMaxJoinHostChars)
	{
		return std::nullopt;
	}
	const bool bracket = host.find(':') != std::string_view::npos && host.find('[') == std::string_view::npos
		&& host.find(']') == std::string_view::npos;
	std::array<char, kMaxJoinHostChars + 8> text{};
	std::size_t length = 0;
	if (bracket)
	{
		text[length++] = '[';
	}
	for (const char c : host)
	{
		text[length++] = c;
	}
	if (bracket)
	{
		text[length++] = ']';
	}
	text[length++] = ':';
	std::array<char, 5> digits{};
	std::size_t digitCount = 0;
	uint32_t remaining = port;
	do
	{
		digits[digitCount++] = static_cast<char>('0' + remaining % 10);
		remaining /= 10;
	} while (remaining != 0);
	while (digitCount != 0)
	{
		text[length++] = digits[--digitCount];
	}
	return ognet::parseAddress(std::string_view{text.data(), length});
}

static_assert(numericJoinAddress("127.0.0.1", 7851).has_value() && numericJoinAddress("127.0.0.1", 7851)->family == 4
		&& numericJoinAddress("127.0.0.1", 7851)->port == 7851,
	"a numeric IPv4 host joins as an IPv4 address with the URL's port");
static_assert(numericJoinAddress("::1", 7777).has_value() && numericJoinAddress("::1", 7777)->family == 6
		&& numericJoinAddress("[::1]", 7777) == numericJoinAddress("::1", 7777),
	"an IPv6 host joins with or without brackets");
static_assert(!numericJoinAddress("localhost", 7777).has_value(), "a host name is not numeric: it resolves elsewhere");
} // namespace ognetunreal
