#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimLaneEndpoint-rationale.md · docs/SimLaneEndpoint-guards.md

#include "OGNet/Connection/ConnectionStats.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetCheck.h"
#include "OGNet/NetTypes.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ognetunreal
{
struct SimLaneEndpointOps
{
	void (*send)(void* ctx, ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) = nullptr;
	void (*broadcastExcept)(void* ctx, ognet::ConnectionId skip, ognet::Lane lane, std::span<const std::byte> bytes) = nullptr;
	void (*setLaneHandler)(void* ctx, ognet::Lane lane, ognet::LaneHandler handler) = nullptr;
	void (*setConnectionHandler)(void* ctx, ognet::ConnectionHandler handler) = nullptr;
	void (*disconnect)(void* ctx, ognet::ConnectionId id) = nullptr;
	void (*noteDelivered)(void* ctx, ognet::Lane lane, double recvSeconds, double nowSeconds) = nullptr;
	const ognet::EndpointConfig& (*config)(const void* ctx) = nullptr;
	ognet::ServiceResult (*serviceUntil)(void* ctx, double deadlineSeconds) = nullptr;
	bool (*hasHighResolutionWait)(const void* ctx) = nullptr;
	ognet::ConnectionStats (*stats)(const void* ctx, ognet::ConnectionId id) = nullptr;
};

struct SimLaneEndpoint
{
public:
	SimLaneEndpoint(void* ctx, const SimLaneEndpointOps& ops) : m_ctx(ctx), m_ops(ops)
	{
		OGNET_CHECK(m_ops.send != nullptr && m_ops.broadcastExcept != nullptr && m_ops.setLaneHandler != nullptr
				&& m_ops.setConnectionHandler != nullptr && m_ops.disconnect != nullptr && m_ops.noteDelivered != nullptr
				&& m_ops.config != nullptr && m_ops.serviceUntil != nullptr && m_ops.hasHighResolutionWait != nullptr
				&& m_ops.stats != nullptr,
			"SimLaneEndpoint needs all ten operations: a missing one would be a null call on the first use. Was task P5-3's seven, "
			"plus task P5-18's serviceUntil and hasHighResolutionWait, plus og-session-runtime task 39's stats");
	}

	// ⛔G-01  docs/SimLaneEndpoint-guards.md
	SimLaneEndpoint(const SimLaneEndpoint&) = delete;
	SimLaneEndpoint& operator=(const SimLaneEndpoint&) = delete;
	SimLaneEndpoint(SimLaneEndpoint&&) = delete;
	SimLaneEndpoint& operator=(SimLaneEndpoint&&) = delete;

	void send(ognet::ConnectionId id, ognet::Lane lane, std::span<const std::byte> bytes) { m_ops.send(m_ctx, id, lane, bytes); }

	void broadcastExcept(ognet::ConnectionId skip, ognet::Lane lane, std::span<const std::byte> bytes)
	{
		m_ops.broadcastExcept(m_ctx, skip, lane, bytes);
	}

	void setLaneHandler(ognet::Lane lane, ognet::LaneHandler handler)
	{
		OGNET_CHECK(lane != ognet::Lane::Engine,
			"SimLaneEndpoint::setLaneHandler on the Engine lane: the engine lane is the driver core's, which polls it for the engine's "
			"packets; a sim lane handler goes on Control, SimClock, SimInput, SimRelay or SimState");
		m_ops.setLaneHandler(m_ctx, lane, handler);
	}

	void setConnectionHandler(ognet::ConnectionHandler handler) { m_ops.setConnectionHandler(m_ctx, handler); }
	void disconnect(ognet::ConnectionId id) { m_ops.disconnect(m_ctx, id); }

	void noteDelivered(ognet::Lane lane, double recvSeconds, double nowSeconds) { m_ops.noteDelivered(m_ctx, lane, recvSeconds, nowSeconds); }

	const ognet::EndpointConfig& config() const { return m_ops.config(m_ctx); }

	ognet::ServiceResult serviceUntil(double deadlineSeconds) { return m_ops.serviceUntil(m_ctx, deadlineSeconds); }
	bool hasHighResolutionWait() const { return m_ops.hasHighResolutionWait(m_ctx); }
	ognet::ConnectionStats stats(ognet::ConnectionId id) const { return m_ops.stats(m_ctx, id); }

private:
	void* m_ctx = nullptr;
	SimLaneEndpointOps m_ops;
};
} // namespace ognetunreal
