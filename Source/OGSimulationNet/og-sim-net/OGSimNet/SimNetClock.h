#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SimNetClock-rationale.md

#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNet/NetTypes.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/Network/SimEnvelope.h"
#include "OGSimulation/Network/SimInbox.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulation/OGAssert.h"

#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace ogsimnet
{
inline constexpr uint8_t kClockWireVersion = 2u;
inline constexpr double kClockPingIntervalSeconds = 1.0 / 20.0;
inline constexpr uint32_t kNoServerTick = std::numeric_limits<uint32_t>::max();

struct SimNetClockPong
{
	double t0 = 0.0;
	double t1 = 0.0;
	double t2 = 0.0;
	uint32_t serverTick = kNoServerTick;
	uint32_t serverTickAgeUs = 0;
};

struct SimNetClockSample
{
	double rttSeconds = 0.0;
	double thetaSeconds = 0.0;
	uint32_t serverTick = kNoServerTick;
	double serverTickAgeSeconds = 0.0;
	double t0 = 0.0;
	double t1 = 0.0;
	double t2 = 0.0;
	double t3 = 0.0;
};

struct SimNetClockStats
{
	uint64_t pingsSent = 0u;
	uint64_t pingsWithoutServer = 0u;
	uint64_t pingsAnswered = 0u;
	uint64_t pingsRefused = 0u;
	uint64_t pingsMalformed = 0u;
	uint64_t pongsReceived = 0u;
	uint64_t pongsMalformed = 0u;
	uint64_t forgedRecordsDropped = 0u;
	uint64_t passedThrough = 0u;
};

namespace simNetClock
{
	inline constexpr size_t kStampBytes = sizeof(uint64_t);
	inline constexpr size_t kPingBytes = simEnvelope::kHeaderBytes + kStampBytes;
	inline constexpr size_t kPongBytes = simEnvelope::kHeaderBytes + 3u * kStampBytes + 2u * sizeof(uint32_t);
	inline constexpr uint8_t kSampleRecordVersion = 2u;
	inline constexpr size_t kSampleRecordBytes = simEnvelope::kHeaderBytes + 6u * kStampBytes + 2u * sizeof(uint32_t);

	static_assert(sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559,
		"simNetClock: a stamp travels as the 64 bits of an IEEE 754 double, so both ends read the same seconds");
	static_assert(simEnvelope::isLegal(SimMessageKind::ClockPing, SimLane::SimClock, SimDirection::ClientToServer)
			&& simEnvelope::isLegal(SimMessageKind::ClockPong, SimLane::SimClock, SimDirection::ServerToClient),
		"simNetClock: a ping goes up and a pong comes down the SimClock lane, the one route the envelope gives each kind");
	static_assert(!simEnvelope::hasBodyLayout(SimMessageKind::ClockPing) && !simEnvelope::hasBodyLayout(SimMessageKind::ClockPong),
		"simNetClock: simEnvelope::decode refuses a ping and a pong, so the SimClock lane's own handler is their only reader and "
		"none reaches a session's drain as a clock message");
	static_assert(kPingBytes == 10u && kPongBytes == 34u && kPongBytes <= 40u,
		"simNetClock: a ping is the header and t0 (10 B), a pong the header, t0, t1, t2, the server tick and its age (34 B since "
		"og-time-dilation task 2, clock wire version 2; 30 B without the age), inside the 40 B the envelope table gives kinds 5 and 6");
	static_assert(kSampleRecordVersion != kSimEnvelopeVersion,
		"simNetClock: a clockSample record's version byte is one no envelope carries, so a message from the wire never reads as "
		"a record and a record never decodes as a message");
	static_assert(kSampleRecordBytes <= kMaxSimMessageBytes, "simNetClock: a clockSample record fits one inbox slot");

	namespace detail
	{
		constexpr uint8_t u8At(std::span<const std::byte> bytes, size_t offset) { return std::to_integer<uint8_t>(bytes[offset]); }

		constexpr uint64_t u64LeAt(std::span<const std::byte> bytes, size_t offset, size_t width)
		{
			uint64_t value = 0u;
			for (size_t i = 0u; i < width; ++i)
				value |= static_cast<uint64_t>(u8At(bytes, offset + i)) << (8u * i);
			return value;
		}

		constexpr void putLe(std::span<std::byte> out, size_t offset, uint64_t value, size_t width)
		{
			for (size_t i = 0u; i < width; ++i)
				out[offset + i] = static_cast<std::byte>((value >> (8u * i)) & 0xFFu);
		}

		constexpr double stampAt(std::span<const std::byte> bytes, size_t offset)
		{
			return std::bit_cast<double>(u64LeAt(bytes, offset, kStampBytes));
		}

		constexpr void putStamp(std::span<std::byte> out, size_t offset, double seconds)
		{
			putLe(out, offset, std::bit_cast<uint64_t>(seconds), kStampBytes);
		}

		constexpr uint32_t u32At(std::span<const std::byte> bytes, size_t offset)
		{
			return static_cast<uint32_t>(u64LeAt(bytes, offset, sizeof(uint32_t)));
		}

		constexpr void putU32(std::span<std::byte> out, size_t offset, uint32_t value) { putLe(out, offset, value, sizeof(uint32_t)); }

		constexpr double secondsOfMicroseconds(uint32_t microseconds) { return static_cast<double>(microseconds) * 1e-6; }

		constexpr uint32_t microsecondsOfSeconds(double seconds)
		{
			if (!(seconds > 0.0))
				return 0u;
			const double microseconds = seconds * 1e6;
			if (!(microseconds < static_cast<double>(std::numeric_limits<uint32_t>::max())))
				return std::numeric_limits<uint32_t>::max();
			return static_cast<uint32_t>(microseconds + 0.5);
		}

		constexpr bool hasHeader(std::span<const std::byte> bytes, uint8_t version, SimMessageKind kind)
		{
			return bytes.size() >= simEnvelope::kHeaderBytes && u8At(bytes, simEnvelope::kVersionOffset) == version
				&& u8At(bytes, simEnvelope::kKindOffset) == static_cast<uint8_t>(kind);
		}

		constexpr void putHeader(std::span<std::byte> out, uint8_t version, SimMessageKind kind)
		{
			out[simEnvelope::kVersionOffset] = static_cast<std::byte>(version);
			out[simEnvelope::kKindOffset] = static_cast<std::byte>(kind);
		}
	} // namespace detail

	constexpr bool isPing(std::span<const std::byte> bytes)
	{
		return detail::hasHeader(bytes, kSimEnvelopeVersion, SimMessageKind::ClockPing);
	}

	constexpr bool isPong(std::span<const std::byte> bytes)
	{
		return detail::hasHeader(bytes, kSimEnvelopeVersion, SimMessageKind::ClockPong);
	}

	constexpr bool isSampleRecord(std::span<const std::byte> bytes)
	{
		return detail::hasHeader(bytes, kSampleRecordVersion, SimMessageKind::ClockPong);
	}

	[[nodiscard]] constexpr size_t writePing(std::span<std::byte> out, double t0)
	{
		if (out.size() < kPingBytes)
			return 0u;
		detail::putHeader(out, kSimEnvelopeVersion, SimMessageKind::ClockPing);
		detail::putStamp(out, simEnvelope::kHeaderBytes, t0);
		return kPingBytes;
	}

	constexpr std::optional<double> readPing(std::span<const std::byte> bytes)
	{
		if (!isPing(bytes) || bytes.size() != kPingBytes)
			return std::nullopt;
		return detail::stampAt(bytes, simEnvelope::kHeaderBytes);
	}

	[[nodiscard]] constexpr size_t writePong(std::span<std::byte> out, const SimNetClockPong& pong)
	{
		if (out.size() < kPongBytes)
			return 0u;
		size_t offset = simEnvelope::kHeaderBytes;
		detail::putHeader(out, kSimEnvelopeVersion, SimMessageKind::ClockPong);
		detail::putStamp(out, offset, pong.t0);
		detail::putStamp(out, offset += kStampBytes, pong.t1);
		detail::putStamp(out, offset += kStampBytes, pong.t2);
		detail::putU32(out, offset += kStampBytes, pong.serverTick);
		detail::putU32(out, offset += sizeof(uint32_t), pong.serverTickAgeUs);
		return kPongBytes;
	}

	constexpr std::optional<SimNetClockPong> readPong(std::span<const std::byte> bytes)
	{
		if (!isPong(bytes) || bytes.size() != kPongBytes)
			return std::nullopt;
		size_t offset = simEnvelope::kHeaderBytes;
		SimNetClockPong pong;
		pong.t0 = detail::stampAt(bytes, offset);
		pong.t1 = detail::stampAt(bytes, offset += kStampBytes);
		pong.t2 = detail::stampAt(bytes, offset += kStampBytes);
		pong.serverTick = detail::u32At(bytes, offset += kStampBytes);
		pong.serverTickAgeUs = detail::u32At(bytes, offset += sizeof(uint32_t));
		return pong;
	}

	constexpr SimNetClockSample sampleOf(const SimNetClockPong& pong, double t3)
	{
		SimNetClockSample sample;
		sample.rttSeconds = (t3 - pong.t0) - (pong.t2 - pong.t1);
		sample.thetaSeconds = ((pong.t1 - pong.t0) + (pong.t2 - t3)) / 2.0;
		sample.serverTick = pong.serverTick;
		sample.serverTickAgeSeconds = detail::secondsOfMicroseconds(pong.serverTickAgeUs);
		sample.t0 = pong.t0;
		sample.t1 = pong.t1;
		sample.t2 = pong.t2;
		sample.t3 = t3;
		return sample;
	}

	[[nodiscard]] constexpr size_t writeSampleRecord(std::span<std::byte> out, const SimNetClockSample& sample)
	{
		if (out.size() < kSampleRecordBytes)
			return 0u;
		size_t offset = simEnvelope::kHeaderBytes;
		detail::putHeader(out, kSampleRecordVersion, SimMessageKind::ClockPong);
		detail::putStamp(out, offset, sample.rttSeconds);
		detail::putStamp(out, offset += kStampBytes, sample.thetaSeconds);
		detail::putStamp(out, offset += kStampBytes, sample.t0);
		detail::putStamp(out, offset += kStampBytes, sample.t1);
		detail::putStamp(out, offset += kStampBytes, sample.t2);
		detail::putStamp(out, offset += kStampBytes, sample.t3);
		detail::putU32(out, offset += kStampBytes, sample.serverTick);
		detail::putU32(out, offset += sizeof(uint32_t), detail::microsecondsOfSeconds(sample.serverTickAgeSeconds));
		return kSampleRecordBytes;
	}

	constexpr std::optional<SimNetClockSample> readSampleRecord(std::span<const std::byte> bytes)
	{
		if (!isSampleRecord(bytes) || bytes.size() != kSampleRecordBytes)
			return std::nullopt;
		size_t offset = simEnvelope::kHeaderBytes;
		SimNetClockSample sample;
		sample.rttSeconds = detail::stampAt(bytes, offset);
		sample.thetaSeconds = detail::stampAt(bytes, offset += kStampBytes);
		sample.t0 = detail::stampAt(bytes, offset += kStampBytes);
		sample.t1 = detail::stampAt(bytes, offset += kStampBytes);
		sample.t2 = detail::stampAt(bytes, offset += kStampBytes);
		sample.t3 = detail::stampAt(bytes, offset += kStampBytes);
		sample.serverTick = detail::u32At(bytes, offset += kStampBytes);
		sample.serverTickAgeSeconds = detail::secondsOfMicroseconds(detail::u32At(bytes, offset += sizeof(uint32_t)));
		return sample;
	}

	inline std::optional<SimNetClockSample> readSample(const InboundSimRecord& record)
	{
		if (record.kind != InboundKind::Message || record.lane != SimLane::SimClock)
			return std::nullopt;
		return readSampleRecord(record.payload());
	}

	namespace selfCheck
	{
		inline constexpr SimNetClockPong kPong{1.0, 1.75, 1.875, 4242u, 4321u};
		inline constexpr double kT3 = 1.25;

		constexpr bool pongRoundTrips()
		{
			std::array<std::byte, kPongBytes> wire{};
			if (writePong(wire, kPong) != kPongBytes)
				return false;
			const std::optional<SimNetClockPong> read = readPong(wire);
			return read && read->t0 == kPong.t0 && read->t1 == kPong.t1 && read->t2 == kPong.t2 && read->serverTick == kPong.serverTick
				&& read->serverTickAgeUs == kPong.serverTickAgeUs && !readPong(std::span<const std::byte>(wire).first(kPongBytes - 1u))
				&& !readPing(wire);
		}

		constexpr bool pingRoundTrips()
		{
			std::array<std::byte, kPingBytes + 1u> wire{};
			if (writePing(wire, 2.5) != kPingBytes)
				return false;
			const std::span<const std::byte> ping = std::span<const std::byte>(wire).first(kPingBytes);
			return readPing(ping) == 2.5 && !readPing(wire) && !readPong(ping);
		}

		constexpr bool sampleIsTheNtpPair()
		{
			const SimNetClockSample sample = sampleOf(kPong, kT3);
			return sample.rttSeconds == 0.125 && sample.thetaSeconds == 0.6875 && sample.serverTick == 4242u && sample.t3 == kT3
				&& sample.serverTickAgeSeconds > 0.0043209 && sample.serverTickAgeSeconds < 0.0043211;
		}

		constexpr bool sampleRecordRoundTripsAndNeverDecodes()
		{
			std::array<std::byte, kSampleRecordBytes> record{};
			const SimNetClockSample sample = sampleOf(kPong, kT3);
			if (writeSampleRecord(record, sample) != kSampleRecordBytes)
				return false;
			const std::optional<SimNetClockSample> read = readSampleRecord(record);
			const DecodedSimMessage decoded =
				simEnvelope::decode<simEnvelope::Shape<4u, 4u>>(SimLane::SimClock, SimDirection::ServerToClient, record);
			return read && read->rttSeconds == sample.rttSeconds && read->thetaSeconds == sample.thetaSeconds
				&& read->serverTick == sample.serverTick && read->serverTickAgeSeconds == sample.serverTickAgeSeconds
				&& read->t0 == sample.t0 && read->t1 == sample.t1 && read->t2 == sample.t2 && read->t3 == sample.t3
				&& !isPong(record) && decoded.refusal == DecodeRefusal::Version;
		}

		constexpr bool sampleRecordCarriesEveryAgeExactly()
		{
			for (const uint32_t ageUs : {0u, 1u, 4321u, 999999u, 1000000u, 4294967294u, 4294967295u})
			{
				SimNetClockPong pong = kPong;
				pong.serverTickAgeUs = ageUs;
				const SimNetClockSample sample = sampleOf(pong, kT3);
				std::array<std::byte, kSampleRecordBytes> record{};
				if (writeSampleRecord(record, sample) != kSampleRecordBytes)
					return false;
				const std::optional<SimNetClockSample> read = readSampleRecord(record);
				if (!read || read->serverTickAgeSeconds != sample.serverTickAgeSeconds
					|| detail::microsecondsOfSeconds(read->serverTickAgeSeconds) != ageUs)
					return false;
			}
			return detail::microsecondsOfSeconds(-1.0) == 0u && detail::microsecondsOfSeconds(std::numeric_limits<double>::quiet_NaN()) == 0u
				&& detail::microsecondsOfSeconds(1e10) == std::numeric_limits<uint32_t>::max();
		}

		static_assert(pongRoundTrips(), "simNetClock: a pong reads back as written, and a short pong or a pong read as a ping is refused");
		static_assert(pingRoundTrips(), "simNetClock: a ping reads back as written, and an overlong ping or a ping read as a pong is refused");
		static_assert(sampleIsTheNtpPair(),
			"simNetClock::sampleOf: RTT = (t3 - t0) - (t2 - t1) and theta = ((t1 - t0) + (t2 - t3)) / 2, the pair P3 section 7.1 names");
		static_assert(sampleRecordRoundTripsAndNeverDecodes(),
			"simNetClock: a clockSample record reads back as written, is not a pong, and simEnvelope::decode refuses it (Version)");
		static_assert(sampleRecordCarriesEveryAgeExactly(),
			"simNetClock: a clockSample record carries the pong's server tick age as the same uint32 microseconds, bit for bit over "
			"the whole range, and an age below 0, NaN or past the range writes 0 or the maximum");
	} // namespace selfCheck
} // namespace simNetClock

template <class C>
concept SimNetClockSource = requires(C& clock)
{
	{ clock.nowSeconds() } -> std::same_as<double>;
};

namespace simNetClockDetail
{
	inline void count(std::atomic<uint64_t>& counter) { counter.fetch_add(1u, std::memory_order_relaxed); }

	inline uint64_t read(const std::atomic<uint64_t>& counter) { return counter.load(std::memory_order_relaxed); }

	inline uint32_t microsecondsLow32(double seconds) { return static_cast<uint32_t>(static_cast<uint64_t>(std::llround(seconds * 1e6))); }
} // namespace simNetClockDetail

static_assert(std::atomic<uint64_t>::is_always_lock_free,
	"SimNetClockServer: the published tick and its step's start time (the low 32 bits of its microseconds) are one lock-free "
	"64-bit atomic, so a pong's tick and its age always come from the same publish (og-time-dilation tasks 2 and 2b)");

template <class TransportT, class ClockT>
class SimNetClockServer
{
	static_assert(SimNetClockSource<ClockT>,
		"SimNetClockServer: t2 is read from the clock og-net stamps receive times with (its host's nowSeconds), so t1 and t2 "
		"are on one clock");

public:
	SimNetClockServer(TransportT& transport, ClockT& clock) : m_transport(transport), m_clock(clock)
	{
		m_transport.setClockHandler(ognet::LaneHandler{&SimNetClockServer::onClockLane, this});
	}

	SimNetClockServer(const SimNetClockServer&) = delete;
	SimNetClockServer& operator=(const SimNetClockServer&) = delete;
	SimNetClockServer(SimNetClockServer&&) = delete;
	SimNetClockServer& operator=(SimNetClockServer&&) = delete;

	void publishServerTick(uint32_t tick, double tickStartSeconds)
	{
		m_published.store((static_cast<uint64_t>(tick) << 32u) | simNetClockDetail::microsecondsLow32(tickStartSeconds),
			std::memory_order_release);
	}

	uint32_t publishedServerTick() const { return static_cast<uint32_t>(m_published.load(std::memory_order_acquire) >> 32u); }

	SimNetClockStats stats() const
	{
		SimNetClockStats out;
		out.pingsAnswered = simNetClockDetail::read(m_pingsAnswered);
		out.pingsRefused = simNetClockDetail::read(m_pingsRefused);
		out.pingsMalformed = simNetClockDetail::read(m_pingsMalformed);
		out.passedThrough = simNetClockDetail::read(m_passedThrough);
		return out;
	}

private:
	static void onClockLane(void* ctx, ognet::ConnectionId id, ognet::Lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		static_cast<SimNetClockServer*>(ctx)->onMessage(id, recvSeconds, bytes);
	}

	void onMessage(SimConnectionId conn, double recvSeconds, std::span<const std::byte> bytes)
	{
		if (!simNetClock::isPing(bytes))
		{
			simNetClockDetail::count(m_passedThrough);
			m_transport.receiveClock(conn, recvSeconds, bytes);
			return;
		}
		const std::optional<double> t0 = simNetClock::readPing(bytes);
		if (!t0)
		{
			simNetClockDetail::count(m_pingsMalformed);
			return;
		}
		if (!m_transport.admitsClockFrom(conn))
		{
			simNetClockDetail::count(m_pingsRefused);
			return;
		}
		SimNetClockPong pong;
		pong.t0 = *t0;
		pong.t1 = recvSeconds;
		const uint64_t published = m_published.load(std::memory_order_acquire);
		pong.serverTick = static_cast<uint32_t>(published >> 32u);
		pong.t2 = m_clock.nowSeconds();
		if (pong.serverTick != kNoServerTick)
			pong.serverTickAgeUs = static_cast<uint32_t>(simNetClockDetail::microsecondsLow32(pong.t2) - static_cast<uint32_t>(published));
		std::array<std::byte, simNetClock::kPongBytes> out{};
		const size_t size = simNetClock::writePong(out, pong);
		m_transport.sendClockNow(conn, std::span<const std::byte>(out.data(), size));
		simNetClockDetail::count(m_pingsAnswered);
	}

	TransportT& m_transport;
	ClockT& m_clock;
	std::atomic<uint64_t> m_published{static_cast<uint64_t>(kNoServerTick) << 32u};
	std::atomic<uint64_t> m_pingsAnswered{0};
	std::atomic<uint64_t> m_pingsRefused{0};
	std::atomic<uint64_t> m_pingsMalformed{0};
	std::atomic<uint64_t> m_passedThrough{0};
};

static_assert(std::atomic<double>::is_always_lock_free,
	"SimNetClockClient: the next ping's due time is one lock-free atomic, so two threads that both ask for a ping send one");

template <class TransportT, class ClockT>
class SimNetClockClient
{
	static_assert(SimNetClockSource<ClockT>,
		"SimNetClockClient: t0 is read from the clock og-net stamps receive times with (its host's nowSeconds), so t0 and t3 "
		"are on one clock");

public:
	SimNetClockClient(TransportT& transport, ClockT& clock) : m_transport(transport), m_clock(clock)
	{
		m_transport.setClockHandler(ognet::LaneHandler{&SimNetClockClient::onClockLane, this});
	}

	SimNetClockClient(const SimNetClockClient&) = delete;
	SimNetClockClient& operator=(const SimNetClockClient&) = delete;
	SimNetClockClient(SimNetClockClient&&) = delete;
	SimNetClockClient& operator=(SimNetClockClient&&) = delete;

	bool servicePing()
	{
		const double now = m_clock.nowSeconds();
		double due = m_nextPingSeconds.load(std::memory_order_relaxed);
		if (now < due)
			return false;
		const SimConnectionId server = m_transport.serverConnection();
		if (server == kInvalidSimConnection)
		{
			simNetClockDetail::count(m_pingsWithoutServer);
			return false;
		}
		const double next = now - due < kClockPingIntervalSeconds ? due + kClockPingIntervalSeconds : now + kClockPingIntervalSeconds;
		if (!m_nextPingSeconds.compare_exchange_strong(due, next, std::memory_order_relaxed))
			return false;
		std::array<std::byte, simNetClock::kPingBytes> out{};
		const size_t size = simNetClock::writePing(out, m_clock.nowSeconds());
		m_transport.sendClockNow(server, std::span<const std::byte>(out.data(), size));
		simNetClockDetail::count(m_pingsSent);
		return true;
	}

	std::optional<SimNetClockSample> readSample(const InboundSimRecord& record) const { return simNetClock::readSample(record); }

	SimNetClockStats stats() const
	{
		SimNetClockStats out;
		out.pingsSent = simNetClockDetail::read(m_pingsSent);
		out.pingsWithoutServer = simNetClockDetail::read(m_pingsWithoutServer);
		out.pongsReceived = simNetClockDetail::read(m_pongsReceived);
		out.pongsMalformed = simNetClockDetail::read(m_pongsMalformed);
		out.forgedRecordsDropped = simNetClockDetail::read(m_forgedRecordsDropped);
		out.passedThrough = simNetClockDetail::read(m_passedThrough);
		return out;
	}

private:
	static void onClockLane(void* ctx, ognet::ConnectionId id, ognet::Lane, std::span<const std::byte> bytes, double recvSeconds)
	{
		static_cast<SimNetClockClient*>(ctx)->onMessage(id, recvSeconds, bytes);
	}

	void onMessage(SimConnectionId conn, double recvSeconds, std::span<const std::byte> bytes)
	{
		if (simNetClock::isSampleRecord(bytes))
		{
			simNetClockDetail::count(m_forgedRecordsDropped);
			return;
		}
		if (!simNetClock::isPong(bytes))
		{
			simNetClockDetail::count(m_passedThrough);
			m_transport.receiveClock(conn, recvSeconds, bytes);
			return;
		}
		const std::optional<SimNetClockPong> pong = simNetClock::readPong(bytes);
		if (!pong)
		{
			simNetClockDetail::count(m_pongsMalformed);
			return;
		}
		std::array<std::byte, simNetClock::kSampleRecordBytes> record{};
		const size_t size = simNetClock::writeSampleRecord(record, simNetClock::sampleOf(*pong, recvSeconds));
		m_transport.receiveClock(conn, recvSeconds, std::span<const std::byte>(record.data(), size));
		simNetClockDetail::count(m_pongsReceived);
	}

	TransportT& m_transport;
	ClockT& m_clock;
	std::atomic<double> m_nextPingSeconds{std::numeric_limits<double>::lowest()};
	std::atomic<uint64_t> m_pingsSent{0};
	std::atomic<uint64_t> m_pingsWithoutServer{0};
	std::atomic<uint64_t> m_pongsReceived{0};
	std::atomic<uint64_t> m_pongsMalformed{0};
	std::atomic<uint64_t> m_forgedRecordsDropped{0};
	std::atomic<uint64_t> m_passedThrough{0};
};
} // namespace ogsimnet
