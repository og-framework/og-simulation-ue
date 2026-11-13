#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SwitchableBackend-rationale.md

#include "OGNet/Backend/BsdDatagramBackend.h"
#include "OGNet/Backend/EmulatedBackend.h"
#include "OGNet/Endpoint/NetEndpoint.h"
#include "OGNet/NetConcepts.h"
#include "OGNet/NetTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ognetunreal
{
template <ognet::DatagramBackend Inner>
class SwitchableBackend
{
public:
	explicit SwitchableBackend(Inner& inner) : m_inner(inner) {}

	SwitchableBackend(const SwitchableBackend&) = delete;
	SwitchableBackend& operator=(const SwitchableBackend&) = delete;
	SwitchableBackend(SwitchableBackend&&) = delete;
	SwitchableBackend& operator=(SwitchableBackend&&) = delete;

	[[nodiscard]] bool enableEmulator(const ognet::EmulatorConfig& config, ognet::EmulatorClockFn clock)
	{
		if (m_opened || clock.fn == nullptr || !ognet::emulator::isValid(config.outbound) || !ognet::emulator::isValid(config.inbound))
		{
			return false;
		}
		m_emulator.reset();
		m_emulator.emplace(m_inner, clock);
		if (!m_emulator->configure(config))
		{
			m_emulator.reset();
			return false;
		}
		return true;
	}

	bool emulatorEnabled() const { return m_emulator.has_value(); }

	std::optional<ognet::EmulatorStats> emulatorStats() const
	{
		if (!m_emulator.has_value())
		{
			return std::nullopt;
		}
		return m_emulator->emulatorStats();
	}

	bool open(const ognet::BindConfig& config)
	{
		m_opened = true;
		return m_emulator.has_value() ? m_emulator->open(config) : m_inner.open(config);
	}

	ognet::SendResult sendTo(const ognet::Address& to, std::span<const std::byte> bytes)
	{
		return m_emulator.has_value() ? m_emulator->sendTo(to, bytes) : m_inner.sendTo(to, bytes);
	}

	std::size_t receive(uint32_t timeoutUs, ognet::RecvFn fn)
	{
		return m_emulator.has_value() ? m_emulator->receive(timeoutUs, fn) : m_inner.receive(timeoutUs, fn);
	}

	bool rebind() { return m_emulator.has_value() ? m_emulator->rebind() : m_inner.rebind(); }

	void wake()
	{
		if (m_emulator.has_value())
		{
			m_emulator->wake();
			return;
		}
		m_inner.wake();
	}

	decltype(auto) stats() const
		requires requires(const Inner& i) { i.stats(); }
	{
		return m_inner.stats();
	}

	decltype(auto) localAddress() const
		requires requires(const Inner& i) { i.localAddress(); }
	{
		return m_inner.localAddress();
	}

	Inner& inner() { return m_inner; }
	const Inner& inner() const { return m_inner; }

private:
	Inner& m_inner;
	std::optional<ognet::EmulatedBackend<Inner>> m_emulator;
	bool m_opened = false;
};

static_assert(ognet::DatagramBackend<SwitchableBackend<ognet::BsdDatagramBackend>>,
	"SwitchableBackend over the BSD backend must stay an og-net DatagramBackend. Was P4-3's 'the five concept members forward'");
static_assert(ognet::ReportsLocalAddress<SwitchableBackend<ognet::BsdDatagramBackend>>,
	"the switch must forward localAddress, or og-net's open and rebind lines and the driver lose the bound address. Was P4-3's "
	"'localAddress exists exactly when Inner has it'");
static_assert(ognet::ReportsReceiveErrors<SwitchableBackend<ognet::BsdDatagramBackend>>,
	"the switch must forward stats, or og-net stops seeing receive errors and never rebinds on them. Was P4-3's 'stats exists "
	"exactly when Inner has it'");
} // namespace ognetunreal
