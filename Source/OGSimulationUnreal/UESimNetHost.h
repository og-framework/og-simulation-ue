// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetHost-rationale.md · docs/UESimNetHost-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/ChildConnection.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Templates/Function.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

template <typename TraitsT> class UESimulationSession;

inline constexpr uint64 kSimNetWireMismatchToastKey = 0x4F47574DF0000001ull;
inline constexpr float kSimNetWireMismatchToastSeconds = 15.f;
inline constexpr char kSimNetRosterLinePrefix[] = "[Roster.";
inline constexpr char kSimNetWarningToken[] = "[Warning]";
inline constexpr size_t kSimNetWarnedRosterLineBytes = ogsimnet::kRosterLogLineBytes + sizeof(kSimNetWarningToken);

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGRtt, Log, All);

inline constexpr double kNoRttReadingMs = -1.0;
inline constexpr double kNoRttReadingSeconds = -1.0;
static_assert(kNoRttReadingMs < 0.0 && kNoRttReadingSeconds < 0.0,
	"The no-reading RTT sentinels are negative: ServerReceptionCoordinator::noteRttSample skips rttMs < 0, and "
	"NetworkTimeEstimator::updateRTT rejects and counts a negative sample, while 0 is a real 0 ms ping. Was "
	"UEConnectionHandle.h's sentinel comment, moved here by og-input-latency task P5-9");

inline UNetConnection* GetRootNetConnection(AActor* actor)
{
	if (actor == nullptr)
	{
		return nullptr;
	}

	UNetConnection* conn = actor->GetNetConnection();
	while (conn != nullptr)
	{
		UChildConnection* childConn = conn->GetUChildConnection();
		if (childConn == nullptr)
		{
			break;
		}

		UNetConnection* parent = childConn->GetParentConnection();
		if (parent == nullptr)
		{
			break;
		}

		conn = parent;
	}

	return conn;
}

inline uint8 GetPlayerSlotForActor(AActor* actor)
{
	if (actor == nullptr)
	{
		return 0;
	}

	// ⛔G-06  docs/UESimNetHost-guards.md
	const UNetConnection* conn = actor->GetNetConnection();
	if (conn == nullptr)
	{
		return 0;
	}

	// ⛔G-07  docs/UESimNetHost-guards.md
	const uint32 childId = conn->GetConnectionHandle().GetChildConnectionId();

	// ⛔G-08  docs/UESimNetHost-guards.md
	return static_cast<uint8>(childId > 255u ? 255u : childId);
}

enum class EServerRttUnavailable : uint8
{
	None,
	NoNetPing,
	RoundTripNotEnabled,
	AccumulatorEmpty,
	NoConnection,
	NoDriver,
	Count
};

inline const TCHAR* serverRttUnavailableText(EServerRttUnavailable cause)
{
	switch (cause)
	{
	case EServerRttUnavailable::NoNetPing:           return TEXT("no FNetPing on the connection - check net.NetPingEnabled");
	case EServerRttUnavailable::RoundTripNotEnabled: return TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes");
	case EServerRttUnavailable::AccumulatorEmpty:    return TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)");
	case EServerRttUnavailable::NoConnection:        return TEXT("no UE connection for og-net id");
	case EServerRttUnavailable::NoDriver:            return TEXT("no og-net driver; og-net id");
	default:                                         return TEXT("");
	}
}

template <typename CauseT>
class TRttCauseWarnings
{
	static_assert(static_cast<uint32>(CauseT::Count) <= 32u,
		"TRttCauseWarnings keeps one bit per RTT cause in a uint32. Was og-input-latency task P5-26's per-cause [RttSample] "
		"SERVER warning, which replaced readRoundTripMs's process-wide one-shot static, and task P5-17's per-cause CLIENT "
		"warning, which replaced UESimNetClientHost's single m_rttWarned flag");

public:
	bool first(CauseT cause)
	{
		const uint32 bit = 1u << static_cast<uint32>(cause);
		if ((m_warned & bit) != 0u)
			return false;
		m_warned |= bit;
		return true;
	}

private:
	uint32 m_warned = 0u;
};

using FServerRttWarnings = TRttCauseWarnings<EServerRttUnavailable>;

inline void logServerRttUnavailable(const TCHAR* causeText)
{
	UE_LOG(LogOGRtt, Warning,
		TEXT("[RttSample] SERVER tier RTT source unavailable (%s). No tier will be ")
		TEXT("derived or published for this connection, so it stays on the default ")
		TEXT("tier 0. One-shot warning."),
		causeText);
}

inline void logServerRttUnavailableForId(const TCHAR* causeText, SimConnectionId ogNetId)
{
	UE_LOG(LogOGRtt, Warning,
		TEXT("[RttSample] SERVER tier RTT source unavailable (%s %u). No tier will be derived or published for this ")
		TEXT("connection, so it stays on the default tier 0. One-shot warning."),
		causeText, static_cast<unsigned>(ogNetId));
}

struct FServerConnectionFound
{
	UOGNetConnection* connection = nullptr;
	EServerRttUnavailable missingCause = EServerRttUnavailable::None;
};

inline constexpr double kServerGoneConnectionForgetSeconds = 10.0;
static_assert(kServerGoneConnectionForgetSeconds > ognetunreal::kDrainedCloseSeconds,
	"FServerConnectionLookup forgets a closed og-net id only after og-net's drained close has delivered that connection's last "
	"sim records: forgotten sooner, the leaver's last input bundle warns NoConnection again. Was og-input-latency task P5-26's "
	"finding F-1, a graceful leave that spent the NoConnection warning");

class FServerConnectionLookup
{
public:
	FServerConnectionFound find(UOGNetDriver* driver, uint32 ogNetId, double nowSeconds)
	{
		if (driver == nullptr)
			return {nullptr, EServerRttUnavailable::NoDriver};
		UOGNetConnection* const connection = driver->FindConnection(ogNetId);
		if (connection != nullptr)
		{
			noteFound(*driver, ogNetId, nowSeconds);
			return {connection, EServerRttUnavailable::None};
		}
		double* const goneSinceSeconds = m_foundIds.Find(ogNetId);
		if (goneSinceSeconds == nullptr)
			return {nullptr, EServerRttUnavailable::NoConnection};
		if (*goneSinceSeconds < 0.0)
			*goneSinceSeconds = nowSeconds;
		return {nullptr, EServerRttUnavailable::None};
	}

	void noteFound(UOGNetDriver& driver, uint32 ogNetId, double nowSeconds)
	{
		if (m_foundIds.Contains(ogNetId))
			return;
		for (auto it = m_foundIds.CreateIterator(); it; ++it)
		{
			if (it.Value() < 0.0)
			{
				if (driver.FindConnection(it.Key()) == nullptr)
					it.Value() = nowSeconds;
			}
			else if (nowSeconds - it.Value() > kServerGoneConnectionForgetSeconds)
			{
				it.RemoveCurrent();
			}
		}
		m_foundIds.Add(ogNetId, kStillFound);
	}

private:
	static constexpr double kStillFound = -1.0;

	TMap<uint32, double> m_foundIds;
};

inline EServerRttUnavailable readRoundTripMs(UNetConnection& conn, double& outMs)
{
	outMs = kNoRttReadingMs;

	UE::Net::FNetPing* netPing = conn.GetNetPing();
	if (netPing == nullptr)
	{
		return EServerRttUnavailable::NoNetPing;
	}

	if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
	{
		return EServerRttUnavailable::RoundTripNotEnabled;
	}

	// ⛔G-09  docs/UESimNetHost-guards.md
	const UE::Net::FPingValues pingVals = netPing->GetPingValues(EPingType::RoundTrip);
	if (pingVals.Current < 0.0)
	{
		return EServerRttUnavailable::AccumulatorEmpty;
	}

	// ⛔G-10  docs/UESimNetHost-guards.md
	outMs = pingVals.Current * 1000.0;
	return EServerRttUnavailable::None;
}

enum class EClientRttUnavailable : uint8
{
	None,
	NoDriver,
	NoServerConnection,
	NoNetPing,
	RoundTripNotEnabled,
	AccumulatorEmpty,
	NotPublished,
	Count
};

inline const TCHAR* clientRttUnavailableText(EClientRttUnavailable cause)
{
	switch (cause)
	{
	case EClientRttUnavailable::NoDriver:            return TEXT("no UNetDriver");
	case EClientRttUnavailable::NoServerConnection:  return TEXT("no ServerConnection");
	case EClientRttUnavailable::NoNetPing:           return TEXT("no FNetPing on the server connection - check net.NetPingEnabled");
	case EClientRttUnavailable::RoundTripNotEnabled: return TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes");
	case EClientRttUnavailable::AccumulatorEmpty:    return TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)");
	case EClientRttUnavailable::NotPublished:        return TEXT("no game-thread reading published yet");
	default:                                         return TEXT("");
	}
}

using FClientRttWarnings = TRttCauseWarnings<EClientRttUnavailable>;

inline void logClientRttUnavailable(const TCHAR* causeText)
{
	UE_LOG(LogOGRtt, Warning,
		TEXT("[RttSample] CLIENT prediction-offset RTT source unavailable (%s). ")
		TEXT("NetworkTimeEstimator will REJECT the sample, so the prediction offset ")
		TEXT("is running on predOffsetFloorTicks alone and is not tracking the network. ")
		TEXT("One-shot warning."),
		causeText);
}

inline void warnClientRttUnavailableOnce(FClientRttWarnings& warnings, EClientRttUnavailable cause)
{
	if (warnings.first(cause))
		logClientRttUnavailable(clientRttUnavailableText(cause));
}

class FLockedSimNetBindings
{
public:
	void onRosterAdded(const ogsimnet::RosterEntry& entry)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.onRosterAdded(entry);
	}

	bool onRosterRemoved(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.onRosterRemoved(simId);
	}

	void seedFrom(const ogsimnet::RosterMirror& mirror)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.seedFrom(mirror);
	}

	ogsimnet::BindVerdict verdict(uint8_t simId, bool engineSaysLocal) const
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.verdict(simId, engineSaysLocal);
	}

	ogsimnet::BindVerdict markRegisteredIfReady(uint8_t simId, bool engineSaysLocal)
	{
		const std::scoped_lock lock(m_mutex);
		const ogsimnet::BindVerdict verdict = m_bindings.verdict(simId, engineSaysLocal);
		if (verdict.step == ogsimnet::BindStep::Ready)
			m_bindings.markRegistered(simId);
		return verdict;
	}

	void markRegistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		m_bindings.markRegistered(simId);
	}

	bool markUnregistered(uint8_t simId)
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.markUnregistered(simId);
	}

	bool isRegistered(uint8_t simId) const
	{
		const std::scoped_lock lock(m_mutex);
		return m_bindings.isRegistered(simId);
	}

private:
	mutable std::mutex m_mutex;
	ogsimnet::SimNetBindings m_bindings;
};

namespace ueSimNetHost
{
inline void noteDelivered(ognetunreal::SimLaneEndpoint& endpoint, SimLane lane, double recvSeconds)
{
	endpoint.noteDelivered(static_cast<ognet::Lane>(lane), recvSeconds, FPlatformTime::Seconds());
}

inline void routeLine(const char* line)
{
	if (std::strncmp(line, kSimNetRosterLinePrefix, sizeof(kSimNetRosterLinePrefix) - 1u) != 0)
	{
		RouteOGMessage(line);
		return;
	}
	char warned[kSimNetWarnedRosterLineBytes] = {};
	std::snprintf(warned, sizeof(warned), "%s%s", kSimNetWarningToken, line);
	RouteOGMessage(warned);
}
} // namespace ueSimNetHost

template <typename TraitsT>
class UESimNetServerHost
{
public:
	using Session = UESimulationSession<TraitsT>;

	UESimNetServerHost(UOGNetDriver* ognetDriver, ognetunreal::SimLaneEndpoint& laneEndpoint, Session* worldSession)
		: m_driver(ognetDriver), m_endpoint(laneEndpoint), m_session(worldSession)
	{
	}

	UESimNetServerHost(const UESimNetServerHost&) = delete;
	UESimNetServerHost& operator=(const UESimNetServerHost&) = delete;

	double nowSeconds() { return FPlatformTime::Seconds(); }

	double rttMs(SimConnectionId conn)
	{
		// ⛔G-12  docs/UESimNetHost-guards.md
		const FServerConnectionFound found = m_connections.find(m_driver, conn, nowSeconds());
		if (found.connection == nullptr)
		{
			if (found.missingCause != EServerRttUnavailable::None)
				warnRttUnavailableOnce(found.missingCause, conn);
			// ⛔G-01  docs/UESimNetHost-guards.md
			return kNoRttReadingMs;
		}
		// ⛔G-11  docs/UESimNetHost-guards.md
		if (found.connection->IsClosingOrClosed())
			return kNoRttReadingMs;
		double roundTripMs = kNoRttReadingMs;
		const EServerRttUnavailable unavailable = readRoundTripMs(*found.connection, roundTripMs);
		if (unavailable != EServerRttUnavailable::None)
			warnRttUnavailableOnce(unavailable, conn);
		if (m_session != nullptr)
			m_session->editLatencyBudgetDiagnostics().noteWireSample(roundTripMs / 1000.0);
		return roundTripMs;
	}

	int32_t receptionTick()
	{
		checkf(m_session != nullptr,
			TEXT("UESimNetServerHost::receptionTick with no session: the server session that owns this host binds it at ")
			TEXT("construction, and SimNetServer asks for the tick only while it drains a received input"));
		return m_session != nullptr ? m_session->getServerReceptionTick() : 0;
	}

	void log(const char* line) { ueSimNetHost::routeLine(line); }

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		if (m_session == nullptr)
			return;
		UELatencyBudgetDiagnostics<TraitsT>& diagnostics = m_session->editLatencyBudgetDiagnostics();
		diagnostics.stamp(latencyBudget::Stream::Input, simId, captureTick, latencyBudget::Point::ServerReceived);
		diagnostics.stampAt(latencyBudget::Stream::Input, simId, captureTick, latencyBudget::Point::NetReceived, recvSeconds);
	}

	void stampHanded(uint8_t simId, uint32_t captureTick)
	{
		if (m_session == nullptr)
			return;
		m_session->editLatencyBudgetDiagnostics().stamp(latencyBudget::Stream::Input, simId, captureTick,
			latencyBudget::Point::Handed);
	}

	void noteDelivered(SimLane lane, double recvSeconds) { ueSimNetHost::noteDelivered(m_endpoint, lane, recvSeconds); }

	UOGNetDriver* driver() const { return m_driver; }
	ognetunreal::SimLaneEndpoint& endpoint() const { return m_endpoint; }
	Session* session() const { return m_session; }

private:
	void warnRttUnavailableOnce(EServerRttUnavailable cause, SimConnectionId conn)
	{
		if (!m_rttWarnings.first(cause))
			return;
		if (cause == EServerRttUnavailable::NoConnection || cause == EServerRttUnavailable::NoDriver)
		{
			logServerRttUnavailableForId(serverRttUnavailableText(cause), conn);
			return;
		}
		logServerRttUnavailable(serverRttUnavailableText(cause));
	}

	UOGNetDriver* m_driver;
	ognetunreal::SimLaneEndpoint& m_endpoint;
	Session* m_session;
	FServerConnectionLookup m_connections;
	FServerRttWarnings m_rttWarnings;
};

template <typename TraitsT>
class UESimNetClientHost
{
public:
	using Session = UESimulationSession<TraitsT>;
	using FRegisteredRemoved = TFunction<void(uint8_t simId)>;

	UESimNetClientHost(UOGNetDriver* ognetDriver, ognetunreal::SimLaneEndpoint& laneEndpoint, FLockedSimNetBindings& simBindings,
		bool immediate)
		: m_driver(ognetDriver), m_endpoint(laneEndpoint), m_bindings(simBindings), m_immediate(immediate)
	{
	}

	UESimNetClientHost(const UESimNetClientHost&) = delete;
	UESimNetClientHost& operator=(const UESimNetClientHost&) = delete;

	void bindSession(Session& worldSession) { m_session = &worldSession; }
	void unbindSession() { m_session = nullptr; }
	void setRoster(const ogsimnet::RosterMirror& roster) { m_roster = &roster; }
	void setOnRegisteredRemoved(FRegisteredRemoved onRegisteredRemoved) { m_onRegisteredRemoved = MoveTemp(onRegisteredRemoved); }

	double nowSeconds() { return FPlatformTime::Seconds(); }

	void publishServerRtt_GameThread()
	{
		checkf(IsInGameThread(),
			TEXT("UESimNetClientHost::publishServerRtt_GameThread reads the driver's ServerConnection, which is game-thread "
				 "state; the step-thread drain reads only the published copy"));
		double roundTripSeconds = kNoRttReadingSeconds;
		const EClientRttUnavailable unavailable = readServerRoundTrip(roundTripSeconds);
		m_publishedRttUnavailable.store(unavailable, std::memory_order_relaxed);
		m_publishedRttSeconds.store(unavailable == EClientRttUnavailable::None ? roundTripSeconds : kNoRttReadingSeconds,
			std::memory_order_release);
	}

	double serverRttSeconds()
	{
		if (m_session == nullptr)
			return kNoRttReadingSeconds;
		double roundTripSeconds = kNoRttReadingSeconds;
		EClientRttUnavailable unavailable = EClientRttUnavailable::None;
		if (m_immediate)
		{
			roundTripSeconds = m_publishedRttSeconds.load(std::memory_order_acquire);
			if (roundTripSeconds < 0.0)
				unavailable = m_publishedRttUnavailable.load(std::memory_order_relaxed);
			if (roundTripSeconds < 0.0 && unavailable == EClientRttUnavailable::None)
				unavailable = EClientRttUnavailable::NotPublished;
		}
		else
		{
			unavailable = readServerRoundTrip(roundTripSeconds);
		}
		if (unavailable == EClientRttUnavailable::None)
			return roundTripSeconds;
		warnClientRttUnavailableOnce(m_rttWarnings, unavailable);
		// ⛔G-02  docs/UESimNetHost-guards.md
		return kNoRttReadingSeconds;
	}

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds)
	{
		if (m_session != nullptr)
			m_session->onTimingInfoReceived(authorityTick, roundTripSeconds);
	}

	void onRelayFloor(uint8_t floorTicks, [[maybe_unused]] bool replayed)
	{
		if (m_session == nullptr)
			return;
		// ⛔G-05  docs/UESimNetHost-guards.md
		m_session->onRelayDelayFloorReceived(floorTicks);
	}

	void onConnectionTier(uint8_t oldTier, uint8_t newTier, [[maybe_unused]] bool replayed)
	{
		if (m_session == nullptr)
			return;
		m_session->onConnectionTierReceived(oldTier, newTier);
	}

	void onRosterAdded(uint8_t simId)
	{
		const ogsimnet::RosterEntry* const entry = m_roster != nullptr ? m_roster->entryFor(simId) : nullptr;
		checkf(entry != nullptr,
			TEXT("UESimNetClientHost::onRosterAdded: sim id %u is not in the roster mirror. The mirror reports an Add after it ")
			TEXT("applied it, and the lanes set the mirror before the first drain"),
			static_cast<unsigned>(simId));
		if (entry != nullptr)
			m_bindings.onRosterAdded(*entry);
	}

	void onRosterRemoved(uint8_t simId)
	{
		const bool wasRegistered = m_bindings.onRosterRemoved(simId);
		checkf(!wasRegistered || static_cast<bool>(m_onRegisteredRemoved),
			TEXT("UESimNetClientHost::onRosterRemoved: sim id %u is registered and no unregister handler is set. The client ")
			TEXT("session sets one (setOnRegisteredRemoved) before it registers a character"),
			static_cast<unsigned>(simId));
		if (wasRegistered && m_onRegisteredRemoved)
		{
			// ⛔G-03  docs/UESimNetHost-guards.md
			m_onRegisteredRemoved(simId);
		}
	}

	void onWireMismatch(uint8_t received, uint8_t expected)
	{
		// ⛔G-04  docs/UESimNetHost-guards.md
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on CorrectionState: server version=%u, client expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			static_cast<unsigned>(received), static_cast<unsigned>(expected));
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetWireMismatchToastKey, kSimNetWireMismatchToastSeconds, FColor::Red,
				TEXT("Build mismatch — please get the latest archive (see PLAYTEST_PORTABLE_README.md)"));
		}
	}

	void log(const char* line) { ueSimNetHost::routeLine(line); }

	void stampClientReceived(ogsimnet::SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds)
	{
		if (m_session == nullptr)
			return;
		UELatencyBudgetDiagnostics<TraitsT>& diagnostics = m_session->editLatencyBudgetDiagnostics();
		const latencyBudget::Stream probeStream =
			stream == ogsimnet::SimNetStream::State ? latencyBudget::Stream::State : latencyBudget::Stream::Input;
		if (m_immediate)
		{
			diagnostics.stampClientReceived_Internal(probeStream, simId, tick, recvSeconds);
			return;
		}
		if (probeStream == latencyBudget::Stream::State)
			diagnostics.stampCorrectionReceived(simId, tick);
		else
			diagnostics.stamp(probeStream, simId, tick, latencyBudget::Point::ClientReceived);
		diagnostics.stampAt(probeStream, simId, tick, latencyBudget::Point::NetReceived, recvSeconds);
	}

	void noteDelivered(SimLane lane, double recvSeconds) { ueSimNetHost::noteDelivered(m_endpoint, lane, recvSeconds); }

	UOGNetDriver* driver() const { return m_driver; }
	Session* session() const { return m_session; }
	bool immediate() const { return m_immediate; }

private:
	static_assert(std::atomic<double>::is_always_lock_free && std::atomic<EClientRttUnavailable>::is_always_lock_free,
		"UESimNetClientHost publishes the server connection's round trip from the game thread to an immediate client's "
		"step-thread drain through two lock-free atomics. Was the drain's own game-thread read of the ServerConnection "
		"(og-input-latency task P5-17)");

	EClientRttUnavailable readServerRoundTrip(double& outSeconds) const
	{
		if (m_driver == nullptr)
			return EClientRttUnavailable::NoDriver;
		UNetConnection* const serverConnection = m_driver->ServerConnection;
		if (serverConnection == nullptr)
			return EClientRttUnavailable::NoServerConnection;
		UE::Net::FNetPing* const netPing = serverConnection->GetNetPing();
		if (netPing == nullptr)
			return EClientRttUnavailable::NoNetPing;
		if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
			return EClientRttUnavailable::RoundTripNotEnabled;
		const UE::Net::FPingValues pingValues = netPing->GetPingValues(EPingType::RoundTrip);
		if (pingValues.Current < 0.0)
			return EClientRttUnavailable::AccumulatorEmpty;
		outSeconds = pingValues.Current;
		return EClientRttUnavailable::None;
	}

	UOGNetDriver* m_driver;
	ognetunreal::SimLaneEndpoint& m_endpoint;
	FLockedSimNetBindings& m_bindings;
	const bool m_immediate;
	const ogsimnet::RosterMirror* m_roster = nullptr;
	Session* m_session = nullptr;
	FRegisteredRemoved m_onRegisteredRemoved;
	FClientRttWarnings m_rttWarnings;
	std::atomic<double> m_publishedRttSeconds{kNoRttReadingSeconds};
	std::atomic<EClientRttUnavailable> m_publishedRttUnavailable{EClientRttUnavailable::NotPublished};
};
