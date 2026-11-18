// SPDX-License-Identifier: MPL-2.0
// docs/UESimNetHost-rationale.md · docs/UESimNetHost-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/ChildConnection.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
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

#include <cstdint>
#include <cstdio>
#include <cstring>

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

inline double readRoundTripMs(UNetConnection* conn)
{
	auto warnOnce = [](const TCHAR* cause)
	{
		static bool bWarned = false;
		if (bWarned)
		{
			return;
		}
		bWarned = true;
		UE_LOG(LogOGRtt, Warning,
			TEXT("[RttSample] SERVER tier RTT source unavailable (%s). No tier will be ")
			TEXT("derived or published for this connection, so it stays on the default ")
			TEXT("tier 0. One-shot warning."),
			cause);
	};

	if (conn == nullptr)
	{
		return kNoRttReadingMs;
	}

	UE::Net::FNetPing* netPing = conn->GetNetPing();
	if (netPing == nullptr)
	{
		warnOnce(TEXT("no FNetPing on the connection - check net.NetPingEnabled"));
		return kNoRttReadingMs;
	}

	if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
	{
		warnOnce(TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes"));
		return kNoRttReadingMs;
	}

	// ⛔G-09  docs/UESimNetHost-guards.md
	const UE::Net::FPingValues pingVals = netPing->GetPingValues(EPingType::RoundTrip);
	if (pingVals.Current < 0.0)
	{
		warnOnce(TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)"));
		return kNoRttReadingMs;
	}

	// ⛔G-10  docs/UESimNetHost-guards.md
	return pingVals.Current * 1000.0;
}

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
		UOGNetConnection* const connection = m_driver != nullptr ? m_driver->FindConnection(conn) : nullptr;
		if (connection == nullptr)
		{
			warnNoConnectionOnce(conn);
			// ⛔G-01  docs/UESimNetHost-guards.md
			return kNoRttReadingMs;
		}
		const double roundTripMs = readRoundTripMs(connection);
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
	void warnNoConnectionOnce(SimConnectionId conn)
	{
		if (m_noConnectionWarned)
			return;
		m_noConnectionWarned = true;
		UE_LOG(LogOGRtt, Warning,
			TEXT("[RttSample] SERVER tier RTT source unavailable (%s %u). No tier will be derived or published for this ")
			TEXT("connection, so it stays on the default tier 0. One-shot warning."),
			m_driver != nullptr ? TEXT("no UE connection for og-net id") : TEXT("no og-net driver; og-net id"),
			static_cast<unsigned>(conn));
	}

	UOGNetDriver* m_driver;
	ognetunreal::SimLaneEndpoint& m_endpoint;
	Session* m_session;
	bool m_noConnectionWarned = false;
};

template <typename TraitsT>
class UESimNetClientHost
{
public:
	using Session = UESimulationSession<TraitsT>;
	using FRegisteredRemoved = TFunction<void(uint8_t simId)>;

	UESimNetClientHost(UOGNetDriver* ognetDriver, ognetunreal::SimLaneEndpoint& laneEndpoint, ogsimnet::SimNetBindings& simBindings)
		: m_driver(ognetDriver), m_endpoint(laneEndpoint), m_bindings(simBindings)
	{
	}

	UESimNetClientHost(const UESimNetClientHost&) = delete;
	UESimNetClientHost& operator=(const UESimNetClientHost&) = delete;

	void bindSession(Session& worldSession) { m_session = &worldSession; }
	void unbindSession() { m_session = nullptr; }
	void setRoster(const ogsimnet::RosterMirror& roster) { m_roster = &roster; }
	void setOnRegisteredRemoved(FRegisteredRemoved onRegisteredRemoved) { m_onRegisteredRemoved = MoveTemp(onRegisteredRemoved); }

	double nowSeconds() { return FPlatformTime::Seconds(); }

	double serverRttSeconds()
	{
		if (m_session == nullptr)
			return kNoRttReadingSeconds;
		double roundTripSeconds = kNoRttReadingSeconds;
		const TCHAR* const unavailable = readServerRoundTrip(roundTripSeconds);
		if (unavailable == nullptr)
			return roundTripSeconds;
		warnRttSourceUnavailableOnce(unavailable);
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
			TEXT("Wire-format mismatch on OnRep_CorrectionState: server version=%u, client expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
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
		if (probeStream == latencyBudget::Stream::State)
			diagnostics.stampCorrectionReceived(simId, tick);
		else
			diagnostics.stamp(probeStream, simId, tick, latencyBudget::Point::ClientReceived);
		diagnostics.stampAt(probeStream, simId, tick, latencyBudget::Point::NetReceived, recvSeconds);
	}

	void noteDelivered(SimLane lane, double recvSeconds) { ueSimNetHost::noteDelivered(m_endpoint, lane, recvSeconds); }

	UOGNetDriver* driver() const { return m_driver; }
	Session* session() const { return m_session; }

private:
	const TCHAR* readServerRoundTrip(double& outSeconds) const
	{
		if (m_driver == nullptr)
			return TEXT("no UNetDriver");
		UNetConnection* const serverConnection = m_driver->ServerConnection;
		if (serverConnection == nullptr)
			return TEXT("no ServerConnection");
		UE::Net::FNetPing* const netPing = serverConnection->GetNetPing();
		if (netPing == nullptr)
			return TEXT("no FNetPing on the server connection - check net.NetPingEnabled");
		if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
			return TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes");
		const UE::Net::FPingValues pingValues = netPing->GetPingValues(EPingType::RoundTrip);
		if (pingValues.Current < 0.0)
			return TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)");
		outSeconds = pingValues.Current;
		return nullptr;
	}

	void warnRttSourceUnavailableOnce(const TCHAR* cause)
	{
		if (m_rttWarned)
			return;
		m_rttWarned = true;
		UE_LOG(LogOGRtt, Warning,
			TEXT("[RttSample] CLIENT prediction-offset RTT source unavailable (%s). ")
			TEXT("NetworkTimeEstimator will REJECT the sample, so the prediction offset ")
			TEXT("is running on predOffsetFloorTicks alone and is not tracking the network. ")
			TEXT("One-shot warning."),
			cause);
	}

	UOGNetDriver* m_driver;
	ognetunreal::SimLaneEndpoint& m_endpoint;
	ogsimnet::SimNetBindings& m_bindings;
	const ogsimnet::RosterMirror* m_roster = nullptr;
	Session* m_session = nullptr;
	FRegisteredRemoved m_onRegisteredRemoved;
	bool m_rttWarned = false;
};
