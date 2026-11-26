#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionLatencyDiagnostics-rationale.md · docs/SessionLatencyDiagnostics-guards.md

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimSession/SessionThreads.h"

namespace ogsession
{
template <typename P>
concept SessionLatencyPort = requires(const P& port, const char* line, SessionOp op)
{
	{ port.nowSeconds() }     -> std::same_as<double>;
	{ port.logLatency(line) } -> std::same_as<void>;
	{ port.checkOp(op) }      -> std::same_as<void>;
};

namespace sessionLatencyPortSelfCheck
{
struct ConformingPort
{
	double nowSeconds() const;
	void logLatency(const char* line) const;
	void checkOp(SessionOp op) const;
};

struct NowSecondsReturnsFloat
{
	float nowSeconds() const;
	void logLatency(const char* line) const;
	void checkOp(SessionOp op) const;
};

struct LogLatencyReturnsBool
{
	double nowSeconds() const;
	bool logLatency(const char* line) const;
	void checkOp(SessionOp op) const;
};

struct CheckOpReturnsBool
{
	double nowSeconds() const;
	void logLatency(const char* line) const;
	bool checkOp(SessionOp op) const;
};

struct NowSecondsOnAMutablePort
{
	double nowSeconds();
	void logLatency(const char* line) const;
	void checkOp(SessionOp op) const;
};

static_assert(SessionLatencyPort<ConformingPort>,
	"SessionLatencyPort self-check: the reference shape (the clock, the log sink, the thread checks) must satisfy the concept");
static_assert(!SessionLatencyPort<NowSecondsReturnsFloat>,
	"SessionLatencyPort self-check: nowSeconds() is required and returns double seconds; a float clock loses the "
	"sub-millisecond hops (H2, H5) a few minutes into a session");
static_assert(!SessionLatencyPort<LogLatencyReturnsBool>,
	"SessionLatencyPort self-check: logLatency(line) is required and returns void; the diagnostics hand each window line to "
	"the sink as a statement and discard any result");
static_assert(!SessionLatencyPort<CheckOpReturnsBool>,
	"SessionLatencyPort self-check: checkOp(op) is required and returns void; a thread row that is wrong stops the process, "
	"it does not return a verdict a door could ignore");
static_assert(!SessionLatencyPort<NowSecondsOnAMutablePort>,
	"SessionLatencyPort self-check: nowSeconds, logLatency and checkOp are callable on a const port; the doors read the "
	"clock from the step's thread and the frame thread alike, so reading it may not change the port");
} // namespace sessionLatencyPortSelfCheck

template <typename PlayerInputT>
using SessionLatencyInputProvider =
	std::function<PlayerInputT(const SimulationTimeStep&, const LocalInputCache<PlayerInputT>&)>;

struct SessionLatencyClockCounts
{
	uint32_t skips       = 0u;
	uint32_t stalls      = 0u;
	uint32_t hardResyncs = 0u;
	bool operator==(const SessionLatencyClockCounts&) const = default;
};

namespace sessionLatencyLines
{
inline constexpr std::size_t kLineBytes = 2048u;
inline constexpr char kClient[] = "Client";
inline constexpr char kServer[] = "Server";
inline constexpr char kHopLine[] =
	"[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms "
	"noStart=%u noEnd=%u outOfOrder=%u overflow=%u";
inline constexpr char kHealHopLine[] =
	"[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms "
	"noStart=%u noEnd=%u outOfOrder=%u overflow=%u healedByUse=%u healedByCorrection=%u";
inline constexpr char kWindowLine[] =
	"[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u "
	"mailboxDropped=%llu";

inline void formatHop(char (&line)[kLineBytes], const char* role, const latencyBudget::HopSummary& hop)
{
	constexpr double kMillis = 1000.0;
	if (hop.hop == latencyBudget::Hop::H6L)
	{
		std::snprintf(line, sizeof(line), kHealHopLine, role, latencyBudget::hopName(hop.hop),
			hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
			hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow, hop.healedByUse, hop.healedByCorrection);
		return;
	}
	std::snprintf(line, sizeof(line), kHopLine, role, latencyBudget::hopName(hop.hop),
		hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
		hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow);
}

inline void formatWindow(char (&line)[kLineBytes], const char* role, const latencyBudget::WindowReport& report,
	uint64_t mailboxDropped)
{
	std::snprintf(line, sizeof(line), kWindowLine,
		role, report.windowEndSeconds - report.windowStartSeconds, report.staleStamps,
		report.duplicateStamps, report.laneOverflow,
		static_cast<unsigned long long>(mailboxDropped));
}
} // namespace sessionLatencyLines

namespace sessionLatencyTest
{
struct Inspector;
} // namespace sessionLatencyTest

template <typename TraitsT, typename OwnerT, typename PortT, bool kEnabled>
class SessionLatencyDiagnostics;

template <typename TraitsT, typename OwnerT, typename PortT>
class SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>
{
	static_assert(SessionLatencyPort<PortT>,
		"SessionLatencyDiagnostics: PortT is the engine's clock, log sink and thread checks (SessionLatencyPort); on Unreal "
		"UELatencyBudgetPort");

public:
	using InputProvider = SessionLatencyInputProvider<typename TraitsT::PlayerInput>;
	using ClockCounts   = SessionLatencyClockCounts;

	SessionLatencyDiagnostics(const OwnerT& owner, PortT port) : m_owner(owner), m_port(std::move(port)) {}

	SessionLatencyDiagnostics(const SessionLatencyDiagnostics&) = delete;
	SessionLatencyDiagnostics& operator=(const SessionLatencyDiagnostics&) = delete;

	InputProvider wrapInputProvider(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider)
	{
		[[maybe_unused]] const bool present = static_cast<bool>(inputProvider);
		InputProvider wrapped = wrap(id, owner, std::move(inputProvider));
		OG_CHECK(static_cast<bool>(wrapped) == present,
			"SessionLatencyDiagnostics::wrapInputProvider: a present provider must stay present and an empty "
			"one empty. Provider presence is the core's local-vs-remote identity test, so the latency wrapper may "
			"never turn an empty provider into a callable or a callable into an empty one, in either arm "
			"(kEnabled). Was the checkf of wrapInputProviderWithLatencyStamp, and UELatencyBudgetDiagnostics' "
			"(og-session-runtime task 37)");
		return wrapped;
	}

	void begin(bool isAuthority);
	void forgetCharacter(unsigned int id);

	void noteServerWireSample_Internal(double roundTripSeconds);
	void post_Internal(const latencyBudget::Event& event);
	void stampClientReceived_Internal(latencyBudget::Stream stream, unsigned int lane, uint32_t tick, double recvSeconds);
	void noteWireSample_Internal(double roundTripSeconds);
	void stampHandedAndLeft_Internal(unsigned int lane, uint32_t captureTick);
	void stampAfterStep_Internal(double stepStartSeconds);
	void stampStepEnd_Internal();
	void stampServerReceived_Internal(unsigned int lane, uint32_t captureTick, double recvSeconds);
	void stampCorrectionHanded_Internal(unsigned int lane, uint32_t correctionTick, double handedSeconds);
	void onPostTickFlush();

	void tickWindow_Frame();

	ClockCounts clockCountsInWindow() const;
	void restartClockWindow();

protected:
	const OwnerT& diagnosticsOwner() const { return m_owner; }

private:
	friend struct sessionLatencyTest::Inspector;

	struct PostedEvent
	{
		latencyBudget::Event event;
		uint32_t             healDelayTicks   = 0u;
		bool                 healByCorrection = false;
	};

	static_assert(std::is_trivially_copyable_v<PostedEvent>,
		"SessionLatencyDiagnostics::PostedEvent crosses from the step thread to the frame thread by value through "
		"SpscMailbox, like the latencyBudget::Event it carries; it must own no memory");

	using Mailbox      = latencyBudget::SpscMailbox<PostedEvent, 4096>;
	using ClockMailbox = latencyBudget::SpscMailbox<ClockCounts, 64>;

	struct SeenRead
	{
		uint32_t                    simTick               = 0u;
		uint32_t                    appliedCaptureTick    = 0u;
		bool                        hasAppliedCaptureTick = false;
		ScheduledRelayedReadOutcome outcome               = ScheduledRelayedReadOutcome::NoProbe;
		bool operator==(const SeenRead&) const = default;
	};

	struct SeenReads
	{
		std::array<SeenRead, kRelayedReadObservationCapacityTicks> slots{};
		bool                                                       primed = false;
	};

	InputProvider wrap(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider);
	void postRelayedReads_Internal(unsigned int id, const RelayedReadObservationRing& ring, uint32_t tick,
		double stepStartSeconds);

	const OwnerT& m_owner;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_owner)>>,
		"SessionLatencyDiagnostics reads its owner's production state (the runtime's manager, storage, input resolution "
		"and the client clock) through a CONST reference only, through the runtime's public const accessors (the session "
		"declares no friend since og-session-runtime task 31). Added by og-tickdriver task 18 (UELatencyBudgetDiagnostics); "
		"engine-free since og-session-runtime task 37.");

	PortT m_port;

	// ⛔G-01  docs/SessionLatencyDiagnostics-guards.md
	std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;
	std::unique_ptr<Mailbox>                           m_mailbox;
	std::unique_ptr<ClockMailbox>                      m_clockMailbox;
	ClockCounts                                        m_clockPosted_Physics;
	ClockCounts                                        m_clockLatest;
	ClockCounts                                        m_clockAtWindowStart;
	uint64_t                                           m_mailboxDropped = 0u;
	std::unordered_map<unsigned int, uint32_t>         m_lastHandedCorrectionTick_Runner;
	std::unordered_map<unsigned int, SeenReads>        m_seenReads_Physics;
};

template <typename TraitsT, typename OwnerT, typename PortT>
class SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, false>
{
	static_assert(SessionLatencyPort<PortT>,
		"SessionLatencyDiagnostics: PortT is the engine's clock, log sink and thread checks (SessionLatencyPort); the "
		"diagnostics-off arm takes the same port and never calls it");

public:
	using InputProvider = SessionLatencyInputProvider<typename TraitsT::PlayerInput>;
	using ClockCounts   = SessionLatencyClockCounts;

	SessionLatencyDiagnostics(const OwnerT&, PortT) {}

	SessionLatencyDiagnostics(const SessionLatencyDiagnostics&) = delete;
	SessionLatencyDiagnostics& operator=(const SessionLatencyDiagnostics&) = delete;

	InputProvider wrapInputProvider(unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider)
	{
		[[maybe_unused]] const bool present = static_cast<bool>(inputProvider);
		InputProvider wrapped = wrap(id, owner, std::move(inputProvider));
		OG_CHECK(static_cast<bool>(wrapped) == present,
			"SessionLatencyDiagnostics::wrapInputProvider: a present provider must stay present and an empty "
			"one empty, in the diagnostics-off arm too (kEnabled false)");
		return wrapped;
	}

	void begin(bool) {}
	void forgetCharacter(unsigned int) {}

	void noteServerWireSample_Internal(double) {}
	void post_Internal(const latencyBudget::Event&) {}
	void stampClientReceived_Internal(latencyBudget::Stream, unsigned int, uint32_t, double) {}
	void noteWireSample_Internal(double) {}
	void stampHandedAndLeft_Internal(unsigned int, uint32_t) {}
	void stampAfterStep_Internal(double) {}
	void stampStepEnd_Internal() {}
	void stampServerReceived_Internal(unsigned int, uint32_t, double) {}
	void stampCorrectionHanded_Internal(unsigned int, uint32_t, double) {}
	void onPostTickFlush() {}

	void tickWindow_Frame() {}

	ClockCounts clockCountsInWindow() const { return {}; }
	void restartClockWindow() {}

private:
	InputProvider wrap(unsigned int, const typename TraitsT::Component&, InputProvider inputProvider)
	{
		return inputProvider;
	}
};

OGSIM_OPTIMIZE_OFF

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::begin(bool isAuthority)
{
	m_probe = std::make_unique<latencyBudget::LatencyBudgetProbe>(isAuthority
		? (latencyBudget::kServerHops | latencyBudget::kServerNetHops)
		: (latencyBudget::kClientHops | latencyBudget::kClientNetHops));
	m_mailbox      = std::make_unique<Mailbox>();
	m_clockMailbox = std::make_unique<ClockMailbox>();
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::forgetCharacter(unsigned int id)
{
	m_port.checkOp(SessionOp::LatencyForget);
	if (m_probe)
		m_probe->forgetLane(id);
	// ⛔G-04  docs/SessionLatencyDiagnostics-guards.md

}

template <typename TraitsT, typename OwnerT, typename PortT>
typename SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::InputProvider
SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::wrap(
	unsigned int id, const typename TraitsT::Component& owner, InputProvider inputProvider)
{
	m_port.checkOp(SessionOp::LatencyWrap);
	if (!inputProvider)
		return inputProvider;

	// ⛔G-02  docs/SessionLatencyDiagnostics-guards.md
	typename TraitsT::InputSampleSource sampleSource = TraitsT::Game::inputSampleSource(owner);
	return
		[this, id, sampleSource, inner = std::move(inputProvider)](
			const SimulationTimeStep& step,
			const LocalInputCache<typename TraitsT::PlayerInput>& localInputCache)
		{
			m_port.checkOp(SessionOp::LatencyCapture);
			typename TraitsT::PlayerInput input = inner(step, localInputCache);
			const double capturedSeconds = m_port.nowSeconds();
			const uint32_t tick = step.getTick();
			const double sampledSeconds = TraitsT::Game::inputSampledSeconds(sampleSource);
			if (sampledSeconds > 0.0)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, tick, latencyBudget::Point::InputSampled, sampledSeconds));
			}
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
				id, tick, latencyBudget::Point::Captured, capturedSeconds));
			return input;
		};
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::noteServerWireSample_Internal(double roundTripSeconds)
{
	m_port.checkOp(SessionOp::LatencyServerWireSample);
	post_Internal(latencyBudget::Event::sample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5));
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::post_Internal(const latencyBudget::Event& event)
{
	if (m_mailbox)
		m_mailbox->tryPush(PostedEvent{ event });
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampClientReceived_Internal(
	latencyBudget::Stream stream, unsigned int lane, uint32_t tick, double recvSeconds)
{
	m_port.checkOp(SessionOp::LatencyClientReceived);
	if (!m_mailbox)
		return;
	const double nowSeconds = m_port.nowSeconds();
	m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(stream, lane, tick, latencyBudget::Point::ClientReceived,
		nowSeconds) });
	if (stream == latencyBudget::Stream::State)
	{
		const auto* store =
			m_owner.runtime().inputResolution().template findRemoteInputCache<typename TraitsT::Simulatable>(lane);
		if (store != nullptr)
		{
			const auto latest = store->findLatest();
			if (latest.valid)
			{
				m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, tick,
					latencyBudget::Point::Healed, nowSeconds), static_cast<uint32_t>(latest.dA), true });
			}
		}
	}
	m_mailbox->tryPush(PostedEvent{ latencyBudget::Event::stamp(stream, lane, tick, latencyBudget::Point::NetReceived,
		recvSeconds) });
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::noteWireSample_Internal(double roundTripSeconds)
{
	m_port.checkOp(SessionOp::LatencyClientWireSample);
	post_Internal(latencyBudget::Event::sample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5));
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampHandedAndLeft_Internal(unsigned int lane,
	uint32_t captureTick)
{
	m_port.checkOp(SessionOp::LatencyHanded);
	const double nowSeconds = m_port.nowSeconds();
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick, latencyBudget::Point::Handed,
		nowSeconds));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::LeftProcess, nowSeconds));
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampServerReceived_Internal(unsigned int lane,
	uint32_t captureTick, double recvSeconds)
{
	m_port.checkOp(SessionOp::LatencyServerReceived);
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::ServerReceived, m_port.nowSeconds()));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input, lane, captureTick,
		latencyBudget::Point::NetReceived, recvSeconds));
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampCorrectionHanded_Internal(unsigned int lane,
	uint32_t correctionTick, double handedSeconds)
{
	m_port.checkOp(SessionOp::LatencyCorrectionHanded);
	uint32_t& lastHanded = m_lastHandedCorrectionTick_Runner[lane];
	if (lastHanded == correctionTick)
		return;
	lastHanded = correctionTick;
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State, lane, correctionTick,
		latencyBudget::Point::Handed, handedSeconds));
	post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State, lane, correctionTick,
		latencyBudget::Point::LeftProcess, handedSeconds));
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampAfterStep_Internal(double stepStartSeconds)
{
	m_port.checkOp(SessionOp::LatencyAfterStep);
	if (!m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	const uint32_t tick = m_owner.runtime().getManager()->currentIntegratedTick();

	if (!m_owner.runtime().getManager()->runsPrediction())
	{
		m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32_t captureTick =
					m_owner.runtime().inputResolution().template getLastUsedCaptureTick<typename TraitsT::Simulatable>(id);
				if (captureTick != kNoInputCaptureTick)
				{
					post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, captureTick, latencyBudget::Point::Consumed, stepStartSeconds));
				}
			});
		return;
	}

	post_Internal(latencyBudget::Event::stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::ClientReceived, latencyBudget::Point::Consumed, stepStartSeconds));

	m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_owner.runtime().inputResolution().getDiagnostics().template relayedReadObservations<typename TraitsT::Simulatable>(id);
			if (ring != nullptr)
				postRelayedReads_Internal(id, *ring, tick, stepStartSeconds);
		});

	if (m_clockMailbox)
	{
		const auto& clock = m_owner.runtime().getManager()->getClientClock();
		ClockCounts counts;
		counts.skips       = clock.getDiagnostics().skipCount();
		counts.stalls      = clock.getDiagnostics().stallCount();
		counts.hardResyncs = clock.getDiagnostics().hardResyncCount();
		if (!(counts == m_clockPosted_Physics) && m_clockMailbox->tryPush(counts))
			m_clockPosted_Physics = counts;
	}
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::postRelayedReads_Internal(
	unsigned int id, const RelayedReadObservationRing& ring, uint32_t tick, double stepStartSeconds)
{
	auto seenIt = m_seenReads_Physics.find(id);
	if (seenIt == m_seenReads_Physics.end())
	{
		if (m_seenReads_Physics.size() >= latencyBudget::ProbeConfig{}.maxLanes)
			m_seenReads_Physics.clear();
		seenIt = m_seenReads_Physics.emplace(id, SeenReads{}).first;
	}
	SeenReads& seen = seenIt->second;

	for (std::size_t index = 0; index < ring.size(); ++index)
	{
		const RelayedReadObservation* const observation = ring.at(index);
		if (observation == nullptr)
			continue;
		const SeenRead read{ observation->simTick, observation->appliedCaptureTick,
			observation->hasAppliedCaptureTick, observation->outcome };
		const bool servedAtSchedule =
			read.hasAppliedCaptureTick && read.outcome == ScheduledRelayedReadOutcome::Hit;
		if (read.simTick == tick)
		{
			if (read.hasAppliedCaptureTick)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, read.appliedCaptureTick, latencyBudget::Point::Consumed, stepStartSeconds));
			}
			if (servedAtSchedule)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, read.appliedCaptureTick, latencyBudget::Point::Healed, stepStartSeconds));
			}
		}
		else if (seen.primed && seen.slots[index] != read && servedAtSchedule)
		{
			// ⛔G-03  docs/SessionLatencyDiagnostics-guards.md
			post_Internal(latencyBudget::Event::stampHealedIfPending(id, read.appliedCaptureTick, stepStartSeconds));
		}
		seen.slots[index] = read;
	}
	seen.primed = true;
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::stampStepEnd_Internal()
{
	m_port.checkOp(SessionOp::LatencyStepEnd);
	if (!m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	const double stepEndSeconds = m_port.nowSeconds();
	const uint32_t tick = m_owner.runtime().getManager()->currentIntegratedTick();
	m_owner.runtime().storage().template forEachSimulatable<typename TraitsT::Simulatable>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::onPostTickFlush()
{
	m_port.checkOp(SessionOp::LatencyPostTickFlush);
	if (!m_probe)
		return;
	const double nowSeconds = m_port.nowSeconds();
	m_probe->stampPending(latencyBudget::Stream::Input,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
	m_probe->stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::tickWindow_Frame()
{
	m_port.checkOp(SessionOp::LatencyWindow);
	if (!m_probe || !m_mailbox || !m_owner.runtime().getManager().has_value())
		return;

	m_mailbox->drain([this](const PostedEvent& posted)
	{
		if (posted.healByCorrection)
			m_probe->stampHealedByCorrection(posted.event.lane, posted.event.tick, posted.healDelayTicks, posted.event.seconds);
		else
			m_probe->apply(posted.event);
	});
	m_mailboxDropped += m_mailbox->takeDroppedCount();
	if (m_clockMailbox)
	{
		m_clockMailbox->drain(
			[this](const ClockCounts& counts) { m_clockLatest = counts; });
		m_mailboxDropped += m_clockMailbox->takeDroppedCount();
	}

	const double nowSeconds = m_port.nowSeconds();
	const bool isClient = m_owner.runtime().getManager()->runsPrediction();

	m_probe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

	latencyBudget::WindowReport report;
	if (!m_probe->closeWindowIfDue(nowSeconds, report))
		return;

	const char* role = isClient ? sessionLatencyLines::kClient : sessionLatencyLines::kServer;
	char line[sessionLatencyLines::kLineBytes];
	for (std::size_t index = 0; index < report.hopCount; ++index)
	{
		sessionLatencyLines::formatHop(line, role, report.hops[index]);
		m_port.logLatency(line);
	}
	sessionLatencyLines::formatWindow(line, role, report, m_mailboxDropped);
	m_port.logLatency(line);
	m_mailboxDropped = 0u;
}

template <typename TraitsT, typename OwnerT, typename PortT>
typename SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::ClockCounts
SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::clockCountsInWindow() const
{
	ClockCounts window;
	window.skips       = m_clockLatest.skips - m_clockAtWindowStart.skips;
	window.stalls      = m_clockLatest.stalls - m_clockAtWindowStart.stalls;
	window.hardResyncs = m_clockLatest.hardResyncs - m_clockAtWindowStart.hardResyncs;
	return window;
}

template <typename TraitsT, typename OwnerT, typename PortT>
void SessionLatencyDiagnostics<TraitsT, OwnerT, PortT, true>::restartClockWindow()
{
	m_clockAtWindowStart = m_clockLatest;
}

OGSIM_OPTIMIZE_ON
} // namespace ogsession
