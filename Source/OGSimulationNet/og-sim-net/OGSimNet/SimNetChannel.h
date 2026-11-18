#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetChannel-rationale.md · docs/SimNetChannel-guards.md

#include "OGSimulation/Network/SimPayloadBuffers.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/SimulationQueues.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>

namespace ogsimnet
{
template <class SimT, class TransportT>
	requires ClientSimTransport<TransportT>
class SimNetChannel
{
public:
	using StateType                  = typename SimT::StateType;
	using InputType                  = typename SimT::InputType;
	using SyncedCorrectionBufferType = CorrectionStateBuffer;
	using RelayedInputRingType       = RelayRingBuffer;

	SimNetChannel(TransportT transport, std::uint8_t simId, std::string debugName)
		: m_transport(std::move(transport))
		, m_simId(simId)
		, m_debugName(std::move(debugName))
	{
		OG_CHECK(m_simId != 0u,
			"SimNetChannel: sim id 0 is never a character's; the envelope refuses it (DecodeRefusal::SimIdZero), so a "
			"channel with it could never be addressed");
	}

	SimNetChannel(const SimNetChannel&)            = delete;
	SimNetChannel& operator=(const SimNetChannel&) = delete;
	SimNetChannel(SimNetChannel&&)                 = delete;
	SimNetChannel& operator=(SimNetChannel&&)      = delete;

	void setOnCorrectionStateReceivedCallback(std::function<void(const CorrectionStateBuffer&)> fn)
	{
		m_onCorrectionStateReceived = std::move(fn);
	}

	void clearOnCorrectionStateReceivedCallback() { m_onCorrectionStateReceived = nullptr; }

	void setOnRelayedInputReceivedCallback(std::function<void(const RelayRingBuffer&)> fn)
	{
		m_onRelayedInputReceived = std::move(fn);
		// ⛔G-03  docs/SimNetChannel-guards.md

	}

	void clearOnRelayedInputReceivedCallback() { m_onRelayedInputReceived = nullptr; }

	void setOnRemoteMoveReceivedCallback(std::function<void(std::uint32_t, const InputType&)> fn)
	{
		m_onRemoteMoveReceived = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback() { m_onRemoteMoveReceived = nullptr; }

	void setOnInputHandedCallback(std::function<void(const InputBundleBuffer&)> fn) { m_onInputHanded = std::move(fn); }

	void clearOnInputHandedCallback() { m_onInputHanded = nullptr; }

	const RelayRingBuffer& getRelayedInputRing() const { return m_relayedInputRing; }

	CorrectionStateBuffer& getSyncedCorrectionStateBuffer()
	{
		// ⛔G-01  docs/SimNetChannel-guards.md
		m_dirty = true;
		return m_correctionState;
	}

	const CorrectionStateBuffer& getSyncedCorrectionStateBuffer() const { return m_correctionState; }

	void sendLocalInputToAuthority(const PendingInputQueue<InputType>& queue, std::uint32_t currentTick,
		std::uint32_t redundancyDepth)
	{
		m_bundle.reset();
		// ⛔G-04  docs/SimNetChannel-guards.md
		const std::uint8_t requested = static_cast<std::uint8_t>(redundancyDepth);
		const std::uint8_t depth     = requested < InputBundleBuffer::kMaxSlots ? requested : InputBundleBuffer::kMaxSlots;
		queue.forEachRecent(currentTick, static_cast<std::size_t>(depth),
			[this](std::uint32_t captureTick, const InputType& input)
			{
				m_bundle.template appendSlot<InputType>(captureTick, input);
			});
		m_transport.sendInputBundle(m_bundle.payload());
		if (m_onInputHanded)
			m_onInputHanded(m_bundle);
	}

	void deliverDelayedRemoteInput(std::uint32_t captureTick, const InputType& input)
	{
		if (m_onRemoteMoveReceived)
			m_onRemoteMoveReceived(captureTick, input);
	}

	[[nodiscard]] bool storeRing(std::span<const std::byte> wire)
	{
		if (!m_relayedInputRing.assignFromWire(wire))
			return false;
		if (m_onRelayedInputReceived)
			m_onRelayedInputReceived(m_relayedInputRing);
		return true;
	}

	[[nodiscard]] bool storeCorrection(std::span<const std::byte> wire)
	{
		// ⛔G-02  docs/SimNetChannel-guards.md
		if (!m_correctionState.assignFromWire(wire))
			return false;
		if (m_onCorrectionStateReceived)
			m_onCorrectionStateReceived(m_correctionState);
		return true;
	}

	std::uint8_t       simId() const { return m_simId; }
	const std::string& debugName() const { return m_debugName; }
	bool               isDirty() const { return m_dirty; }

	bool takeDirty()
	{
		const bool wasDirty = m_dirty;
		m_dirty             = false;
		return wasDirty;
	}

private:
	TransportT            m_transport;
	std::uint8_t          m_simId;
	std::string           m_debugName;
	CorrectionStateBuffer m_correctionState;
	RelayRingBuffer       m_relayedInputRing;
	InputBundleBuffer     m_bundle;
	bool                  m_dirty = false;

	std::function<void(const CorrectionStateBuffer&)>       m_onCorrectionStateReceived;
	std::function<void(const RelayRingBuffer&)>             m_onRelayedInputReceived;
	std::function<void(std::uint32_t, const InputType&)>    m_onRemoteMoveReceived;
	std::function<void(const InputBundleBuffer&)>           m_onInputHanded;
};

template <ClientSimTransport InnerT>
class SimNetBundleTransport
{
public:
	SimNetBundleTransport(InnerT inner, std::uint64_t& emptyBundles) : m_inner(std::move(inner)), m_emptyBundles(&emptyBundles) {}

	void sendInputBundle(std::span<const std::byte> bundle)
	{
		// ⛔G-05  docs/SimNetChannel-guards.md
		if (bundle.empty())
		{
			++*m_emptyBundles;
			return;
		}
		m_inner.sendInputBundle(bundle);
	}

	void endInputSends() { m_inner.endInputSends(); }

private:
	InnerT         m_inner;
	std::uint64_t* m_emptyBundles;
};

template <class SimT, class InnerT>
using SessionChannel = SimNetChannel<SimT, SimNetBundleTransport<InnerT>>;
} // namespace ogsimnet
