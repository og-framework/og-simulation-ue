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
#include "OGNetUnreal/OGNetDriver.h"
#include "OGSimNet/SimNetBindings.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetLink.h"
#include "OGSimNet/SimNetRoster.h"
#include "OGSimNet/SimNetServerHost.h"
#include "OGSimNet/SimNetTransport.h"
#include "OGSimulation/OGAssert.h"
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

template <typename TraitsT> class UESimulationSession;
namespace ogsession { template <typename TraitsT> class SessionRuntime; }
template <typename TraitsT> struct UESessionRuntimeTraits;
template <typename TraitsT> using UESessionRuntime = ogsession::SessionRuntime<UESessionRuntimeTraits<TraitsT>>;

inline constexpr uint64 kSimNetWireMismatchToastKey = 0x4F47574DF0000001ull;
inline constexpr float kSimNetWireMismatchToastSeconds = 15.f;

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGRtt, Log, All);

inline constexpr double kNoRttReadingSeconds = -1.0;
static_assert(kNoRttReadingSeconds < 0.0,
	"The client's no-reading RTT sentinel is negative: NetworkTimeEstimator::updateRTT rejects and counts a negative sample, "
	"while 0 is a real 0 ms ping. Was UEConnectionHandle.h's sentinel comment, moved here by og-input-latency task P5-9; its "
	"server half, kNoRttReadingMs, is og-sim-net's kSimNetServerNoRttMs since og-session-runtime task 39");

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
inline void routeLine(const char* line)
{
	ogsimnet::routeSimNetHostLine(line, [](const char* routed) { RouteOGMessage(routed); });
}
} // namespace ueSimNetHost

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
	UESessionRuntime<TraitsT>* session = nullptr;

	void onTimingInfo(uint32_t authorityTick, double roundTripSeconds) { session->onTimingInfoReceived(authorityTick, roundTripSeconds); }
	void onRelayFloor(uint8_t floorTicks) { session->onRelayDelayFloorReceived(floorTicks); }
	void onConnectionTier(uint8_t oldTier, uint8_t newTier) { session->onConnectionTierReceived(oldTier, newTier); }
	void onClockSample(const SimClockRttSample& sample) { session->onSimClockSample(sample); }
	void onLifecycleUnregistering(uint8_t simId) { session->onSimNetClientLifecycleUnregistering(simId); }

	void stampClientReceived(ogsimnet::SimNetStream stream, uint8_t simId, uint32_t tick, double recvSeconds)
	{
		const latencyBudget::Stream probeStream =
			stream == ogsimnet::SimNetStream::State ? latencyBudget::Stream::State : latencyBudget::Stream::Input;
		session->diagnostics().stampClientReceived_Internal(probeStream, simId, tick, recvSeconds);
	}
};

template <typename TraitsT>
class UESimNetServerHostPort
{
public:
	using Session = UESessionRuntime<TraitsT>;

	UESimNetServerHostPort(Session* worldSession) : m_session(worldSession) {}

	UESimNetServerHostPort(const UESimNetServerHostPort&) = delete;
	UESimNetServerHostPort& operator=(const UESimNetServerHostPort&) = delete;

	Session* session() const { return m_session; }

	Session& checkedSession() const
	{
		OG_CHECK(m_session != nullptr,
			"UESimNetServerHostPort: the server host calls its session hooks only with a session attached (the runtime builds "
			"it with its own address, and a host built with none, as the hosts concept test's, is given no session port). Was "
			"og-session-runtime task 31's check, review_SR5_31a item 3");
		return *m_session;
	}

	double nowSeconds() { return FPlatformTime::Seconds(); }
	void logHost(const char* line) { RouteOGMessage(line); }

	int32_t receptionTick() { return checkedSession().getServerReceptionTick(); }
	void stampHanded(uint8_t simId, uint32_t captureTick) { checkedSession().diagnostics().stampHandedAndLeft_Internal(simId, captureTick); }
	void onLifecycleUnregistering(uint8_t simId) { checkedSession().onSimNetLifecycleUnregistering(simId); }
	void noteWireSample(double roundTripSeconds) { checkedSession().diagnostics().noteServerWireSample_Internal(roundTripSeconds); }

	void stampServerReceived(uint8_t simId, uint32_t captureTick, double recvSeconds)
	{
		checkedSession().diagnostics().stampServerReceived_Internal(simId, captureTick, recvSeconds);
	}

private:
	Session* m_session;
};

template <typename TraitsT>
struct UESimNetServerHostParts
{
	UESimNetServerHostPort<TraitsT> m_port;
};

template <typename TraitsT>
using UESimNetServerHostCore =
	ogsimnet::SimNetServerHostCore<UESimNetServerHostPort<TraitsT>, UESimNetServerHostPort<TraitsT>, ognetunreal::SimLaneEndpoint>;

template <typename TraitsT>
class UESimNetLanesServerHost : private UESimNetServerHostParts<TraitsT>, public UESimNetServerHostCore<TraitsT>
{
	using Parts = UESimNetServerHostParts<TraitsT>;

public:
	UESimNetLanesServerHost(ognetunreal::SimLaneEndpoint& laneEndpoint, UESessionRuntime<TraitsT>* worldSession)
		: Parts{{worldSession}}
		, UESimNetServerHostCore<TraitsT>(this->m_port, laneEndpoint, worldSession != nullptr ? &this->m_port : nullptr)
	{
	}
};
