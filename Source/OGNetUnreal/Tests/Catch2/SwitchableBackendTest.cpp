// SPDX-License-Identifier: MPL-2.0
#include "catch_amalgamated.hpp"

#include "VirtualNetwork.h"

#include "OGNetUnreal/Core/SwitchableBackend.h"

#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetConcepts.h"
#include "OGNet/NetTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace
{
using ognet::test::VirtualBackend;
using ognet::test::VirtualNetwork;
using ognetunreal::SwitchableBackend;

struct ErrorCountingStats
{
	uint64_t receiveErrors = 0;
	uint64_t ignoredReceiveErrors = 0;
};

class ErrorCountingBackend
{
public:
	explicit ErrorCountingBackend(VirtualBackend& inner) : m_inner(inner) {}

	bool open(const ognet::BindConfig& config) { return m_inner.open(config); }
	ognet::SendResult sendTo(const ognet::Address& to, std::span<const std::byte> bytes) { return m_inner.sendTo(to, bytes); }
	std::size_t receive(uint32_t timeoutUs, ognet::RecvFn fn) { return m_inner.receive(timeoutUs, fn); }
	bool rebind() { return m_inner.rebind(); }
	void wake() { m_inner.wake(); }
	ErrorCountingStats stats() const { return ErrorCountingStats{}; }

private:
	VirtualBackend& m_inner;
};

static_assert(ognet::DatagramBackend<SwitchableBackend<VirtualBackend>>, "SwitchableBackend<VirtualBackend> is a DatagramBackend");
static_assert(ognet::ReportsLocalAddress<VirtualBackend> && ognet::ReportsLocalAddress<SwitchableBackend<VirtualBackend>>,
	"VirtualBackend reports its local address, so the switch over it does");
static_assert(!ognet::ReportsReceiveErrors<VirtualBackend> && !ognet::ReportsReceiveErrors<SwitchableBackend<VirtualBackend>>,
	"VirtualBackend reports no receive errors, so the switch over it does not");
static_assert(ognet::DatagramBackend<SwitchableBackend<ErrorCountingBackend>>, "SwitchableBackend<ErrorCountingBackend> is a DatagramBackend");
static_assert(ognet::ReportsReceiveErrors<ErrorCountingBackend> && ognet::ReportsReceiveErrors<SwitchableBackend<ErrorCountingBackend>>,
	"ErrorCountingBackend reports receive errors, so the switch over it does");
static_assert(!ognet::ReportsLocalAddress<ErrorCountingBackend> && !ognet::ReportsLocalAddress<SwitchableBackend<ErrorCountingBackend>>,
	"ErrorCountingBackend reports no local address, so the switch over it does not");

constexpr uint16_t kPeerPort = 7000;
constexpr uint16_t kSwitchPort = 7001;
constexpr int kDatagrams = 10;

struct Received
{
	std::vector<std::vector<std::byte>> datagrams;

	static void onDatagram(void* ctx, const ognet::Address&, std::span<const std::byte> bytes)
	{
		static_cast<Received*>(ctx)->datagrams.emplace_back(bytes.begin(), bytes.end());
	}

	ognet::RecvFn fn() { return ognet::RecvFn{&Received::onDatagram, this}; }
};

std::vector<std::byte> payload(int index)
{
	return std::vector<std::byte>{static_cast<std::byte>(index), std::byte{0x5A}, static_cast<std::byte>(index * 7)};
}

ognet::BindConfig bindTo(uint16_t port)
{
	ognet::BindConfig config;
	config.port = port;
	return config;
}

std::size_t receiveAll(VirtualBackend& backend, Received& received)
{
	std::size_t total = 0;
	for (;;)
	{
		const std::size_t got = backend.receive(0, received.fn());
		if (got == 0)
		{
			return total;
		}
		total += got;
	}
}
std::vector<uint8_t> u8(const std::vector<std::byte>& bytes)
{
	std::vector<uint8_t> out;
	for (const std::byte b : bytes)
	{
		out.push_back(static_cast<uint8_t>(b));
	}
	return out;
}
} // namespace

TEST_CASE("SwitchableBackend.WithoutTheEmulatorEveryDatagramReachesThePeer", "[OGNet][EngineLaneDriver]")
{
	VirtualNetwork network;
	VirtualBackend& peer = network.addHost(1);
	VirtualBackend& inner = network.addHost(2);
	SwitchableBackend<VirtualBackend> backend(inner);
	REQUIRE(peer.open(bindTo(kPeerPort)));
	REQUIRE(backend.open(bindTo(kSwitchPort)));
	CHECK_FALSE(backend.emulatorEnabled());
	CHECK_FALSE(backend.emulatorStats().has_value());
	REQUIRE(backend.localAddress().has_value());
	CHECK(*backend.localAddress() == ognet::test::virtualHostAddress(2, kSwitchPort));

	for (int i = 0; i < kDatagrams; ++i)
	{
		const std::vector<std::byte> bytes = payload(i);
		CHECK(backend.sendTo(ognet::test::virtualHostAddress(1, kPeerPort), bytes) == ognet::SendResult::Ok);
	}
	Received atPeer;
	CHECK(receiveAll(peer, atPeer) == static_cast<std::size_t>(kDatagrams));
	REQUIRE(atPeer.datagrams.size() == static_cast<std::size_t>(kDatagrams));
	for (int i = 0; i < kDatagrams; ++i)
	{
		CHECK(u8(atPeer.datagrams[static_cast<std::size_t>(i)]) == u8(payload(i)));
	}

	const std::vector<std::byte> back = payload(99);
	CHECK(peer.sendTo(ognet::test::virtualHostAddress(2, kSwitchPort), back) == ognet::SendResult::Ok);
	Received atSwitch;
	CHECK(backend.receive(0, atSwitch.fn()) == 1);
	REQUIRE(atSwitch.datagrams.size() == 1);
	CHECK(u8(atSwitch.datagrams[0]) == u8(back));
	CHECK(inner.stats().sent == static_cast<uint64_t>(kDatagrams));
}

TEST_CASE("SwitchableBackend.WithFullOutboundLossNothingReachesThePeer", "[OGNet][EngineLaneDriver]")
{
	VirtualNetwork network;
	VirtualBackend& peer = network.addHost(1);
	VirtualBackend& inner = network.addHost(2);
	SwitchableBackend<VirtualBackend> backend(inner);
	ognet::EmulatorConfig config;
	config.outbound.lossPercent = 100.0;
	REQUIRE(backend.enableEmulator(config, network.clock()));
	CHECK(backend.emulatorEnabled());
	REQUIRE(peer.open(bindTo(kPeerPort)));
	REQUIRE(backend.open(bindTo(kSwitchPort)));

	for (int i = 0; i < kDatagrams; ++i)
	{
		const std::vector<std::byte> bytes = payload(i);
		backend.sendTo(ognet::test::virtualHostAddress(1, kPeerPort), bytes);
	}
	network.advance(1.0);
	Received sink;
	backend.receive(0, sink.fn());
	Received atPeer;
	CHECK(receiveAll(peer, atPeer) == 0);
	CHECK(inner.stats().sent == 0);
	REQUIRE(backend.emulatorStats().has_value());
	CHECK(backend.emulatorStats()->outbound.offered == static_cast<uint64_t>(kDatagrams));
	CHECK(backend.emulatorStats()->outbound.lost == static_cast<uint64_t>(kDatagrams));

	const std::vector<std::byte> back = payload(42);
	CHECK(peer.sendTo(ognet::test::virtualHostAddress(2, kSwitchPort), back) == ognet::SendResult::Ok);
	Received atSwitch;
	CHECK(backend.receive(0, atSwitch.fn()) == 1);
	REQUIRE(atSwitch.datagrams.size() == 1);
	CHECK(u8(atSwitch.datagrams[0]) == u8(back));
	CHECK(backend.emulatorStats()->inbound.released == 1);
}

TEST_CASE("SwitchableBackend.TheEmulatorIsChosenOnlyBeforeOpenAndOnlyWithAValidConfig", "[OGNet][EngineLaneDriver]")
{
	VirtualNetwork network;
	VirtualBackend& inner = network.addHost(2);
	SwitchableBackend<VirtualBackend> backend(inner);

	ognet::EmulatorConfig invalid;
	invalid.inbound.lossPercent = 101.0;
	CHECK_FALSE(backend.enableEmulator(invalid, network.clock()));
	CHECK_FALSE(backend.emulatorEnabled());
	CHECK_FALSE(backend.enableEmulator(ognet::EmulatorConfig{}, ognet::EmulatorClockFn{}));
	CHECK_FALSE(backend.emulatorEnabled());

	ognet::EmulatorConfig valid;
	valid.seed = 7;
	valid.outbound.lagMinMs = 10.0;
	valid.outbound.lagMaxMs = 20.0;
	REQUIRE(backend.enableEmulator(valid, network.clock()));
	CHECK_FALSE(backend.enableEmulator(invalid, network.clock()));
	CHECK(backend.emulatorEnabled());

	REQUIRE(backend.open(bindTo(kSwitchPort)));
	CHECK_FALSE(backend.enableEmulator(valid, network.clock()));
	CHECK(backend.emulatorEnabled());

	SwitchableBackend<VirtualBackend> plain(network.addHost(3));
	REQUIRE(plain.open(bindTo(kSwitchPort)));
	CHECK_FALSE(plain.enableEmulator(valid, network.clock()));
	CHECK_FALSE(plain.emulatorEnabled());
}
