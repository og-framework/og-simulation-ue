// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetHost-rationale.md · docs/UESimNetHost-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/ChildConnection.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/Network/SimTransport.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Templates/Function.h"

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>

template <typename TraitsT> class UESimulationSession;

inline constexpr uint64 kSimNetWireMismatchToastKey = 0x4F47574DF0000001ull;
inline constexpr float kSimNetWireMismatchToastSeconds = 15.f;

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

namespace ueSimNetHost
{
inline void noteDelivered(ognetunreal::SimLaneEndpoint& endpoint, SimLane lane, double recvSeconds)
{
	ogsimnet::noteSimLaneDelivered(endpoint, lane, recvSeconds, FPlatformTime::Seconds());
}

inline void routeLine(const char* line)
{
	ogsimnet::routeSimNetHostLine(line, [](const char* routed) { RouteOGMessage(routed); });
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

class UESimNetLinkServices
{
public:
	explicit UESimNetLinkServices(UOGNetDriver* ognetDriver) : m_driver(ognetDriver) {}

	UESimNetLinkServices(const UESimNetLinkServices&) = delete;
	UESimNetLinkServices& operator=(const UESimNetLinkServices&) = delete;

	double nowSeconds() { return FPlatformTime::Seconds(); }
	void logRouted(const char* line) { RouteOGLevelLine(line); }
	void logHost(const char* line) { RouteOGMessage(line); }

	void publishServerRtt_GameThread()
	{
		checkf(IsInGameThread(),
			TEXT("UESimNetLinkServices::publishServerRtt_GameThread reads the driver's ServerConnection, which is game-thread "
				 "state; the step-thread drain reads only the published copy"));
		double roundTripSeconds = kNoRttReadingSeconds;
		const EClientRttUnavailable unavailable = readServerRoundTrip(roundTripSeconds);
		m_publishedRttUnavailable.store(unavailable, std::memory_order_relaxed);
		m_publishedRttSeconds.store(unavailable == EClientRttUnavailable::None ? roundTripSeconds : kNoRttReadingSeconds,
			std::memory_order_release);
	}

	double serverRttSeconds()
	{
		const double roundTripSeconds = m_publishedRttSeconds.load(std::memory_order_acquire);
		EClientRttUnavailable unavailable = EClientRttUnavailable::None;
		if (roundTripSeconds < 0.0)
			unavailable = m_publishedRttUnavailable.load(std::memory_order_relaxed);
		if (roundTripSeconds < 0.0 && unavailable == EClientRttUnavailable::None)
			unavailable = EClientRttUnavailable::NotPublished;
		if (unavailable == EClientRttUnavailable::None)
			return roundTripSeconds;
		warnClientRttUnavailableOnce(m_rttWarnings, unavailable);
		// ⛔G-02  docs/UESimNetHost-guards.md
		return kNoRttReadingSeconds;
	}

	void onWireMismatch(uint8_t received, uint8_t expected)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on CorrectionState: server version=%u, client expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			static_cast<unsigned>(received), static_cast<unsigned>(expected));
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetWireMismatchToastKey, kSimNetWireMismatchToastSeconds, FColor::Red,
				TEXT("Build mismatch — please get the latest archive (see PLAYTEST_PORTABLE_README.md)"));
		}
	}

	UOGNetDriver* driver() const { return m_driver; }

private:
	static_assert(std::atomic<double>::is_always_lock_free && std::atomic<EClientRttUnavailable>::is_always_lock_free,
		"UESimNetLinkServices publishes the server connection's round trip from the game thread to the client's "
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
	FClientRttWarnings m_rttWarnings;
	std::atomic<double> m_publishedRttSeconds{kNoRttReadingSeconds};
	std::atomic<EClientRttUnavailable> m_publishedRttUnavailable{EClientRttUnavailable::NotPublished};
};

static_assert(ogsimnet::SimNetLinkServices<UESimNetLinkServices> && ogsimnet::kSimNetLinkNoRttSeconds == kNoRttReadingSeconds,
	"UESimNetLinkServices is the og-sim-net link's engine services, and the link's no-reading round trip (an unattached "
	"client's) is this file's sentinel. Was UESimNetClientHost's own session check (og-session-runtime task 28)");

template <typename TraitsT>
struct UESimNetLinkSession
{
	UESimulationSession<TraitsT>* session = nullptr;

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds) { session->onTimingInfoReceived(authorityTick, roundTripSeconds); }
	void onRelayFloor(uint8_t floorTicks) { session->onRelayDelayFloorReceived(floorTicks); }
	void onConnectionTier(uint8_t oldTier, uint8_t newTier) { session->onConnectionTierReceived(oldTier, newTier); }
	void onClockSample(const SimClockRttSample& sample) { session->onSimClockSample(sample); }
	void onLifecycleUnregistering(uint8_t simId) { session->onSimNetClientLifecycleUnregistering(simId); }

	void stampClientReceived(ogsimnet::SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds)
	{
		const latencyBudget::Stream probeStream =
			stream == ogsimnet::SimNetStream::State ? latencyBudget::Stream::State : latencyBudget::Stream::Input;
		session->editLatencyBudgetDiagnostics().stampClientReceived_Internal(probeStream, simId, tick, recvSeconds);
	}
};

inline constexpr size_t kRunnerRttSlots = 64u;

class FRunnerRttTable
{
public:
	static_assert(ognet::kInvalidConnection == 0u,
		"FRunnerRttTable: a word of 0 is an empty slot, and a connection's word holds its og-net id in the high half; og-net "
		"never assigns id 0 (kInvalidConnection), so no connection's word reads as empty. A connection still without an "
		"og-net id is not published, and id 0 is never read. Was review_P5-19 N2 (an id 0 with a round trip of 0 read as "
		"absent)");
	static_assert(std::atomic<uint64_t>::is_always_lock_free,
		"FRunnerRttTable: one connection's reading is one lock-free 64-bit word (og-net id, float milliseconds), so the runner "
		"never reads an id with another connection's round trip. Was the runner's call into the driver's connection map, "
		"which is game-thread state");

	void publish_GameThread(size_t slot, uint32 ogNetId, double roundTripMs)
	{
		m_slots[slot].store((static_cast<uint64_t>(ogNetId) << 32u) | std::bit_cast<uint32_t>(static_cast<float>(roundTripMs)),
			std::memory_order_release);
	}

	void clearFrom_GameThread(size_t firstSlot)
	{
		for (size_t slot = firstSlot; slot < kRunnerRttSlots; ++slot)
			m_slots[slot].store(0u, std::memory_order_release);
	}

	double read(uint32 ogNetId) const
	{
		if (ogNetId == ognet::kInvalidConnection)
			return kNoRttReadingMs;
		for (const std::atomic<uint64_t>& slot : m_slots)
		{
			const uint64_t word = slot.load(std::memory_order_acquire);
			if (word != 0u && static_cast<uint32>(word >> 32u) == ogNetId)
				return static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(word & 0xFFFFFFFFu)));
		}
		return kNoRttReadingMs;
	}

private:
	std::array<std::atomic<uint64_t>, kRunnerRttSlots> m_slots{};
};

template <typename TraitsT>
class UESimNetLanesServerHost : public UESimNetServerHost<TraitsT>
{
	using Base = UESimNetServerHost<TraitsT>;

public:
	using Base::Base;

	double rttMs(SimConnectionId conn) { return m_runnerRtt.read(conn); }

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		if (this->session() == nullptr)
			return;
		this->session()->editLatencyBudgetDiagnostics().stampServerReceived_Internal(simId, captureTick, recvSeconds);
	}

	void stampHanded(uint8_t simId, uint32_t captureTick)
	{
		if (this->session() != nullptr)
			this->session()->editLatencyBudgetDiagnostics().stampHandedAndLeft_Internal(simId, captureTick);
	}

	void onLifecycleUnregistering(uint8_t simId)
	{
		if (this->session() != nullptr)
			this->session()->onSimNetLifecycleUnregistering(simId);
	}

	void publishRunnerRtt_GameThread()
	{
		checkf(IsInGameThread(),
			TEXT("UESimNetLanesServerHost::publishRunnerRtt_GameThread: the game thread reads the driver's connections and "
				 "publishes each one's round trip for the authority runner, which reads only the published copy"));
		size_t slot = 0u;
		if (UOGNetDriver* const driver = this->driver())
		{
			for (UNetConnection* const connection : driver->ClientConnections)
			{
				UOGNetConnection* const ognetConnection = Cast<UOGNetConnection>(connection);
				if (ognetConnection == nullptr || ognetConnection->OGNetId == ognet::kInvalidConnection || slot == kRunnerRttSlots)
					continue;
				m_runnerRtt.publish_GameThread(slot++, ognetConnection->OGNetId, publishedRttMs_GameThread(*ognetConnection));
			}
		}
		m_runnerRtt.clearFrom_GameThread(slot);
	}

private:
	double publishedRttMs_GameThread(UOGNetConnection& connection)
	{
		// ⛔G-13  docs/UESimNetHost-guards.md
		if (connection.ClientLoginState == EClientLoginState::ReceivedJoin)
			return Base::rttMs(connection.OGNetId);
		double roundTripMs = kNoRttReadingMs;
		if (!connection.IsClosingOrClosed())
			readRoundTripMs(connection, roundTripMs);
		return roundTripMs;
	}

	FRunnerRttTable m_runnerRtt;
};
