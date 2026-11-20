// SPDX-License-Identifier: MPL-2.0
// docs/OGNetDriver-rationale.md · docs/OGNetDriver-guards.md

#include "OGNetUnreal/OGNetDriver.h"

#include "AddressInfoTypes.h"
#include "Async/Async.h"
#include "Engine/PendingNetGame.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OGNetInternetAddr.h"
#include "OGNetUnreal/OGNetConnection.h"
#include "OGNetUnreal/OGNetUnrealLog.h"
#include "OGNetUnrealHost.h"
#include "SocketSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#include "OGNet/Backend/BsdDatagramBackend.h"
#include "OGNet/Backend/EmulatorProfile.h"
#include "OGNet/Endpoint/EngineLane.h"
#include "OGNet/Lanes/LaneConfig.h"
#include "OGNet/NetTypes.h"
#include "OGNetUnreal/Core/EngineLaneDriverCore.h"
#include "OGNetUnreal/Core/EngineLaneSettings.h"
#include "OGNetUnreal/Core/SimLaneEndpoint.h"
#include "OGNetUnreal/Core/SwitchableBackend.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{
constexpr std::size_t kEngineLaneMaxConnections = 16;
constexpr double kStopDrainSeconds = 0.2;
constexpr double kForeignTrafficWarningSeconds = 60.0;
constexpr uint64 kDefaultEmulatorSeed = 1;

FOGNetSimLanesFactory GSimLanesFactory = nullptr;
bool GSimLanesInitBaseSeen = false;

static_assert(kEngineLaneMaxConnections >= 8 && kEngineLaneMaxConnections <= ognet::kMaxEndpointConnections,
	"the engine lane's endpoint holds at least the session's 8 players and no more than og-net allows. Was design 4.3's "
	"'maxConnections 16 (>= the 8-player cap)'");
static_assert(kStopDrainSeconds == ognetunreal::kDrainedCloseSeconds,
	"the stop drain waits as long as a drained close: the Engine lane's age limit. Was P4-4's 'core.stop(now, 0.2)'");
static_assert(ognetunreal::IoMode{} == ognetunreal::IoMode::OwnThread,
	"IOGNetSimLanes::ioMode's default returns IoMode{}, which must be OwnThread: a lanes object that does not choose keeps og-net on "
	"its own thread. Was task P5-18's 'ioMode (default OwnThread)'");

const TCHAR* IoModeName(ognetunreal::IoMode Mode)
{
	return Mode == ognetunreal::IoMode::HostServiced ? TEXT("hostServiced") : TEXT("ownThread");
}

const char* DisconnectReasonName(ognet::DisconnectReason Reason)
{
	switch (Reason)
	{
	case ognet::DisconnectReason::Timeout:
		return "Timeout";
	case ognet::DisconnectReason::ClosedByPeer:
		return "ClosedByPeer";
	case ognet::DisconnectReason::LocalClose:
		return "LocalClose";
	case ognet::DisconnectReason::ResumeFailed:
		return "ResumeFailed";
	case ognet::DisconnectReason::ProtocolError:
		return "ProtocolError";
	case ognet::DisconnectReason::Rejected:
		return "Rejected";
	case ognet::DisconnectReason::ConnectFailed:
		return "ConnectFailed";
	case ognet::DisconnectReason::ServerKeyMismatch:
		return "ServerKeyMismatch";
	}
	return "Unknown";
}

const TCHAR* RoleName(ognet::Role Role)
{
	return Role == ognet::Role::Server ? TEXT("server") : TEXT("client");
}

double EngineTimeoutMultiplier(const UNetDriver& Driver)
{
	static const bool bLongTimeouts = FParse::Param(FCommandLine::Get(), TEXT("longtimeouts"));
	const bool bUnoptimized = bLongTimeouts || WITH_EDITOR || UE_BUILD_DEBUG;
	return (Driver.TimeoutMultiplierForUnoptimizedBuilds > 0.0f && bUnoptimized) ? Driver.TimeoutMultiplierForUnoptimizedBuilds : 1.0;
}

uint32 NonNegative(int32 Value)
{
	return Value > 0 ? static_cast<uint32>(Value) : 0u;
}

void SimLanesBeforeStart(void* Ctx, ognetunreal::SimLaneEndpoint& SimEndpoint)
{
	static_cast<IOGNetSimLanes*>(Ctx)->beforeStart(SimEndpoint);
}

UPendingNetGame* PendingNetGameFor(const UNetDriver& Driver, const FNetworkNotify* Notify)
{
	for (TObjectIterator<UPendingNetGame> It; It; ++It)
	{
		if (It->NetDriver.Get() == &Driver && static_cast<const FNetworkNotify*>(*It) == Notify)
		{
			return *It;
		}
	}
	return nullptr;
}

#if DO_ENABLE_NET_TEST
static_assert(sizeof(FPacketSimulationSettings) == sizeof(ognetunreal::EnginePacketSimulation),
	"FPacketSimulationSettings holds exactly the 15 int32 fields the intake copies: an engine field added to it must be mapped or "
	"ignored here. Was P4-4's 'copy PacketSimulationSettings into an EnginePacketSimulation'");

ognetunreal::EnginePacketSimulation ToEnginePacketSimulation(const FPacketSimulationSettings& Settings)
{
	return ognetunreal::EnginePacketSimulation{.PktLoss = Settings.PktLoss,
		.PktLossMaxSize = Settings.PktLossMaxSize,
		.PktLossMinSize = Settings.PktLossMinSize,
		.PktOrder = Settings.PktOrder,
		.PktDup = Settings.PktDup,
		.PktLag = Settings.PktLag,
		.PktLagVariance = Settings.PktLagVariance,
		.PktLagMin = Settings.PktLagMin,
		.PktLagMax = Settings.PktLagMax,
		.PktIncomingLagMin = Settings.PktIncomingLagMin,
		.PktIncomingLagMax = Settings.PktIncomingLagMax,
		.PktIncomingLoss = Settings.PktIncomingLoss,
		.PktJitter = Settings.PktJitter,
		.PktFrameDelay = Settings.PktFrameDelay,
		.PktIncomingFrameDelay = Settings.PktIncomingFrameDelay};
}

int32 IgnoredValue(const ognetunreal::EnginePacketSimulation& Settings, ognetunreal::IgnoredSetting Setting)
{
	switch (Setting)
	{
	case ognetunreal::IgnoredSetting::PktOrder:
		return Settings.PktOrder;
	case ognetunreal::IgnoredSetting::PktJitter:
		return Settings.PktJitter;
	case ognetunreal::IgnoredSetting::PktLossMinSize:
		return Settings.PktLossMinSize;
	case ognetunreal::IgnoredSetting::PktLossMaxSize:
		return Settings.PktLossMaxSize;
	case ognetunreal::IgnoredSetting::PktFrameDelay:
		return Settings.PktFrameDelay;
	case ognetunreal::IgnoredSetting::PktIncomingFrameDelay:
		return Settings.PktIncomingFrameDelay;
	}
	return 0;
}
#endif
} // namespace

struct FOGNetEndpointHost
{
	using FBackend = ognetunreal::SwitchableBackend<ognet::BsdDatagramBackend>;
	using FCore = ognetunreal::EngineLaneDriverCore<FOGNetUnrealHost, FBackend>;

	FOGNetUnrealHost Host;
	ognet::BsdDatagramBackend Bsd;
	FBackend Backend{Bsd};
	FCore Core;

	ognet::Role Role = ognet::Role::Client;
	double TimeoutMultiplier = 1.0;
	TMap<uint32, TWeakObjectPtr<UOGNetConnection>> Connections;
	TMap<uint32, ognet::DisconnectReason> ClosedReasons;
	uint8 ReceiveBuffer[ognetunreal::kBatchEventBytes] = {};

	uint64 ReportedRetries = 0;
	uint64 ReportedMalformed = 0;
	double LastForeignWarningSeconds = 0.0;
	bool bForeignWarned = false;
	uint64 ConnectionlessSendsDropped = 0;
	uint64 DeliveriesWithoutConnection = 0;

	TWeakObjectPtr<UPendingNetGame> HeldJoin;
	uint64 HeldJoinFrame = 0;
	bool bJoinHeld = false;

	double Now() const
	{
		// ⛔G-02  docs/OGNetDriver-guards.md
		return Host.nowSeconds();
	}

	static UOGNetDriver& DriverOf(void* Ctx) { return *static_cast<UOGNetDriver*>(Ctx); }

	static double EmulatorClock(void* Ctx) { return static_cast<const FOGNetEndpointHost*>(Ctx)->Now(); }

	static ognetunreal::AcceptVerdict OnAccept(void* Ctx, ognet::ConnectionId Id)
	{
		UOGNetDriver& Driver = DriverOf(Ctx);
		if (Driver.Notify == nullptr)
		{
			return ognetunreal::AcceptVerdict::Ignore;
		}
		const EAcceptConnection::Type Verdict = Driver.Notify->NotifyAcceptingConnection();
		if (Verdict == EAcceptConnection::Ignore)
		{
			return ognetunreal::AcceptVerdict::Ignore;
		}
		if (Verdict != EAcceptConnection::Accept)
		{
			return ognetunreal::AcceptVerdict::Reject;
		}
		UOGNetConnection* Connection = NewObject<UOGNetConnection>(GetTransientPackage(), Driver.NetConnectionClass);
		Connection->InitRemoteConnection(&Driver, nullptr, Driver.World ? Driver.World->URL : FURL(), FInternetAddrOGNet(Id), USOCK_Open);
		Driver.Notify->NotifyAcceptedConnection(Connection);
		Driver.AddClientConnection(Connection);
		Driver.Endpoint->Connections.Add(Id, Connection);
		UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] up id=%u handler=%s"), Id, Connection->Handler.IsValid() ? TEXT("on") : TEXT("off"));
		return ognetunreal::AcceptVerdict::Accept;
	}

	static void OnDeliver(void* Ctx, ognet::ConnectionId Id, std::span<const std::byte> Bytes)
	{
		FOGNetEndpointHost& Self = *DriverOf(Ctx).Endpoint;
		const TWeakObjectPtr<UOGNetConnection>* Found = Self.Connections.Find(Id);
		UOGNetConnection* Connection = Found != nullptr ? Found->Get() : nullptr;
		if (Connection == nullptr || Connection->GetConnectionState() == USOCK_Closed)
		{
			++Self.DeliveriesWithoutConnection;
			return;
		}
		FMemory::Memcpy(Self.ReceiveBuffer, Bytes.data(), Bytes.size());
		Connection->ReceivedRawPacket(Self.ReceiveBuffer, static_cast<int32>(Bytes.size()));
	}

	static void OnClosed(void* Ctx, ognet::ConnectionId Id, ognet::DisconnectReason Reason)
	{
		FOGNetEndpointHost& Self = *DriverOf(Ctx).Endpoint;
		Self.ClosedReasons.Add(Id, Reason);
		TWeakObjectPtr<UOGNetConnection> Weak;
		Self.Connections.RemoveAndCopyValue(Id, Weak);
		if (UOGNetConnection* Connection = Weak.Get())
		{
			Connection->SetConnectionState(USOCK_Closed);
		}
	}

	static void OnServerConnectionUp(void* Ctx, ognet::ConnectionId Id)
	{
		UOGNetDriver& Driver = DriverOf(Ctx);
		UOGNetConnection* Connection = Cast<UOGNetConnection>(Driver.ServerConnection);
		if (Connection == nullptr)
		{
			return;
		}
		Connection->OGNetId = Id;
		Connection->RemoteAddr = FInternetAddrOGNet(Id).Clone();
		Driver.Endpoint->Connections.Add(Id, Connection);
		UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] up id=%u handler=%s"), Id, Connection->Handler.IsValid() ? TEXT("on") : TEXT("off"));
	}

	static void OnConnectEnded(void* Ctx, ognet::DisconnectReason Reason)
	{
		UOGNetDriver& Driver = DriverOf(Ctx);
		UE_LOG(LogOGNetTransport, Warning, TEXT("[OGNet.Driver] connect ended reason=%hs"), DisconnectReasonName(Reason));
		if (Driver.ServerConnection != nullptr)
		{
			Driver.ServerConnection->SetConnectionState(USOCK_Closed);
		}
	}

	static void OnJoinResolved(UOGNetDriver& Driver, const FAddressInfoResult& Result, uint16 Port, double DeadlineSeconds)
	{
		if (!Driver.Endpoint.IsValid() || Driver.ServerConnection == nullptr)
		{
			return;
		}
		const FAddressInfoResultData* Chosen = nullptr;
		if (Result.ReturnCode == SE_NO_ERROR)
		{
			for (const FAddressInfoResultData& Candidate : Result.Results)
			{
				if (Candidate.AddressProtocol == ESocketProtocolFamily::IPv4)
				{
					Chosen = &Candidate;
					break;
				}
			}
			if (Chosen == nullptr && Result.Results.Num() > 0)
			{
				Chosen = &Result.Results[0];
			}
		}
		std::optional<ognet::Address> Address;
		if (Chosen != nullptr)
		{
			const FTCHARToUTF8 Text(*Chosen->Address->ToString(false));
			Address = ognetunreal::numericJoinAddress(std::string_view(Text.Get(), Text.Length()), Port);
		}
		if (!Address.has_value())
		{
			UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] could not resolve %s (code %d, %d results)"), *Result.QueryHostName,
				static_cast<int32>(Result.ReturnCode), Result.Results.Num());
			Driver.ServerConnection->SetConnectionState(USOCK_Closed);
			return;
		}
		UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] resolved %s to %hs"), *Result.QueryHostName, ognet::formatAddress(*Address).c_str());
		Driver.Endpoint->Core.connect(*Address, DeadlineSeconds);
	}

	static void ReleaseHeldJoin(UOGNetDriver& Driver)
	{
		FOGNetEndpointHost& Self = *Driver.Endpoint;
		UPendingNetGame* Join = Self.HeldJoin.Get();
		UNetConnection* Connection = Driver.ServerConnection;
		if (Join == nullptr || Join->NetDriver.Get() != &Driver || Connection == nullptr || Connection->GetConnectionState() != USOCK_Pending)
		{
			Self.bJoinHeld = false;
			Self.HeldJoin.Reset();
			return;
		}
		// ⛔G-03  docs/OGNetDriver-guards.md
		if (Self.Core.serverConnection() == ognet::kInvalidConnection || GFrameCounter <= Self.HeldJoinFrame)
		{
			Connection->LastSendTime = Driver.GetElapsedTime();
			return;
		}
		checkf(Connection->Channels[0] == nullptr && Connection->OutTotalPackets == 0,
			TEXT("OGNetDriver: the held join found the control channel open or %d packets sent before UE's hello: the engine's own hello "
				 "left inside the frame that opened the connection. Was task P4-13's 'the client's first send is UE's hello, on the first "
				 "TickDispatch after og-net is up and outside InitConnect's frame'"),
			Connection->OutTotalPackets);
		Self.bJoinHeld = false;
		Self.HeldJoin.Reset();
		Driver.CreateInitialClientChannels();
		UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] hello released frame=%llu heldFrame=%llu id=%u"), GFrameCounter, Self.HeldJoinFrame,
			Self.Core.serverConnection());
		Join->SendInitialJoin();
	}

	void ReportRetries()
	{
		const uint64 Retries = Core.stats().retries;
		if (Retries > ReportedRetries)
		{
			ReportedRetries = Retries;
			UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] connect retry n=%llu"), Retries);
		}
	}

	void ReportForeignTraffic(double NowSeconds)
	{
		const uint64 Malformed = Core.endpointStats().malformed;
		if (Malformed <= ReportedMalformed || (bForeignWarned && NowSeconds < LastForeignWarningSeconds + kForeignTrafficWarningSeconds))
		{
			return;
		}
		ReportedMalformed = Malformed;
		LastForeignWarningSeconds = NowSeconds;
		bForeignWarned = true;
		UE_LOG(LogOGNetTransport, Warning, TEXT("[OGNet.Driver] foreign traffic malformed=%llu (a peer on IpNetDriver?)"), Malformed);
	}
};

static_assert(sizeof(FOGNetEndpointHost::ReceiveBuffer) == ognet::kEngineEventBytes,
	"the receive buffer holds any engine event the lane delivers. Was P4-4's 'a 1,232 B member buffer'");

namespace
{
void DestroySimLanesAfterEndpoint(const TUniquePtr<FOGNetEndpointHost>& Endpoint, TUniquePtr<IOGNetSimLanes>& SimLanes)
{
	checkf(!Endpoint.IsValid(),
		TEXT("OGNetDriver: the sim lanes are destroyed after the endpoint, whose destruction joins og-net's thread: until then that "
			 "thread may call into the lanes' handlers. Was design P5 K1's 'lanes destroyed after og-net's thread joined'"));
	SimLanes.Reset();
}
} // namespace

UOGNetDriver::UOGNetDriver() = default;

UOGNetDriver::UOGNetDriver(FVTableHelper& Helper) : Super(Helper) {}

UOGNetDriver::~UOGNetDriver()
{
	if (Endpoint.IsValid() && Endpoint->Core.isStarted() && !Endpoint->Core.isStopped())
	{
		Endpoint->Core.stop(Endpoint->Now(), 0.0);
	}
	Endpoint.Reset();
	DestroySimLanesAfterEndpoint(Endpoint, SimLanes);
}

bool UOGNetDriver::IsAvailable() const
{
	return true;
}

bool UOGNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	GSimLanesInitBaseSeen = true;
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}
	if (Endpoint.IsValid())
	{
		Error = TEXT("OGNetDriver: InitBase called twice");
		UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] %s"), *Error);
		return false;
	}
	if (NetConnectionClass == nullptr || !NetConnectionClass->IsChildOf<UOGNetConnection>())
	{
		Error = FString::Printf(TEXT("OGNetDriver: NetConnectionClass %s is not a UOGNetConnection"),
			NetConnectionClass != nullptr ? *NetConnectionClass->GetPathName() : TEXT("(none)"));
		UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] %s"), *Error);
		return false;
	}

	Endpoint = MakeUnique<FOGNetEndpointHost>();
	FOGNetEndpointHost& Self = *Endpoint;
	Self.Role = bInitAsClient ? ognet::Role::Client : ognet::Role::Server;
	Self.TimeoutMultiplier = EngineTimeoutMultiplier(*this);

	ognet::EndpointConfig Config;
	Config.role = Self.Role;
	Config.bind.port = bInitAsClient ? uint16{0} : static_cast<uint16>(FMath::Clamp(URL.Port, 0, 65535));
	Config.bind.dualStack = true;
	Config.buildIdentity = ognetunreal::kEngineLaneBuildIdentity;
	Config.maxConnections = kEngineLaneMaxConnections;
	const UPlayer* PlayerDefaults = GetDefault<UPlayer>();
	Config.budget = ognetunreal::budgetFor(NonNegative(MaxClientRate), NonNegative(MaxInternetClientRate),
		NonNegative(PlayerDefaults->ConfiguredInternetSpeed), NonNegative(PlayerDefaults->ConfiguredLanSpeed));
	Config.timings = ognetunreal::timingsFor(ognetunreal::EngineTimeouts{InitialConnectTimeout, ConnectionTimeout, Self.TimeoutMultiplier});

	const TCHAR* EmulatorName = TEXT("off");
#if DO_ENABLE_NET_TEST
	uint64 Seed = kDefaultEmulatorSeed;
	FParse::Value(FCommandLine::Get(), TEXT("OGNetEmulatorSeed="), Seed);
	const ognetunreal::EnginePacketSimulation EngineSettings = ToEnginePacketSimulation(PacketSimulationSettings);
	const ognetunreal::EmulatorIntake Intake = ognetunreal::mapPacketSimulation(EngineSettings, Seed);
	if (bUseEnginePacketSimulation)
	{
		if (Intake.active || Intake.ignoredMask != 0)
		{
			EmulatorName = TEXT("engine");
		}
	}
	else
	{
		if (Intake.active)
		{
			if (!Self.Backend.enableEmulator(Intake.config, ognet::EmulatorClockFn{&FOGNetEndpointHost::EmulatorClock, &Self}))
			{
				Error = TEXT("OGNetDriver: og-net's emulator refused the mapped packet simulation settings");
				UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] %s"), *Error);
				Endpoint.Reset();
				return false;
			}
			EmulatorName = TEXT("ognet");
		}
		FString IgnoredNames;
		for (const ognetunreal::IgnoredSetting Setting : ognetunreal::kIgnoredSettings)
		{
			if ((Intake.ignoredMask & static_cast<uint16>(Setting)) == 0)
			{
				continue;
			}
			UE_LOG(LogOGNetTransport, Warning, TEXT("[OGNet.Driver] ignored %hs=%d: og-net's emulator has no equivalent"),
				ognetunreal::ignoredName(Setting), IgnoredValue(EngineSettings, Setting));
			IgnoredNames += IgnoredNames.IsEmpty() ? TEXT("") : TEXT(",");
			IgnoredNames += ANSI_TO_TCHAR(ognetunreal::ignoredName(Setting));
		}
		if (Intake.active || Intake.ignoredMask != 0)
		{
			const ognet::DirectionConfig& Out = Intake.config.outbound;
			const ognet::DirectionConfig& In = Intake.config.inbound;
			UE_LOG(LogOGNetTransport, Log,
				TEXT("[OGNet.Driver] emulator out(loss=%g lag=%g..%g dup=%g) in(loss=%g lag=%g..%g) seed=%llu ignored=%s"), Out.lossPercent,
				Out.lagMinMs, Out.lagMaxMs, Out.duplicatePercent, In.lossPercent, In.lagMinMs, In.lagMaxMs, Seed,
				IgnoredNames.IsEmpty() ? TEXT("none") : *IgnoredNames);
		}
		if (Intake.active)
		{
			const std::string Profile = ognet::canonicalText(ognetunreal::emulatorProfileFor(Intake, Self.Role));
			UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Emulator] profile=%hs"), Profile.c_str());
		}
		PacketSimulationSettings.ResetSettings();
		bNeverApplyNetworkEmulationSettings = true;
		OnPacketSimulationSettingsChanged();
	}
#endif

	const ognetunreal::DriverCallbacks Callbacks{.ctx = this,
		.accept = &FOGNetEndpointHost::OnAccept,
		.deliver = &FOGNetEndpointHost::OnDeliver,
		.closed = &FOGNetEndpointHost::OnClosed,
		.serverConnectionUp = &FOGNetEndpointHost::OnServerConnectionUp,
		.connectEnded = &FOGNetEndpointHost::OnConnectEnded};
	TUniquePtr<IOGNetSimLanes> Lanes;
	if (GSimLanesFactory != nullptr)
	{
		Lanes = GSimLanesFactory(*this, !bInitAsClient);
	}
	const ognetunreal::IoMode Mode = Lanes.IsValid() ? Lanes->ioMode(!bInitAsClient) : ognetunreal::IoMode::OwnThread;
	const ognetunreal::SimLaneAttach Attach{.ctx = Lanes.Get(), .beforeStart = &SimLanesBeforeStart};
	if (!Self.Core.start(Self.Host, Self.Backend, Config, Mode, Callbacks, Lanes.IsValid() ? &Attach : nullptr))
	{
		Error = FString::Printf(TEXT("OGNetDriver: og-net could not start the %s endpoint on UDP port %d"), RoleName(Self.Role), Config.bind.port);
		UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] %s"), *Error);
		Endpoint.Reset();
		DestroySimLanesAfterEndpoint(Endpoint, Lanes);
		return false;
	}
	SimLanes = MoveTemp(Lanes);

	const std::optional<ognet::Address> Bound = Self.Core.localAddress();
	const int32 BoundPort = Bound.has_value() ? Bound->port : 0;
	LocalAddr = GetSocketSubsystem()->CreateInternetAddr();
	LocalAddr->SetAnyAddress();
	LocalAddr->SetPort(BoundPort);

	UE_LOG(LogOGNetTransport, Log,
		TEXT("[OGNet.Driver] start role=%s mode=%s handler=off port=%d maxPacket=%d overhead=%d budget=%u idle=%g emulator=%s"),
		RoleName(Self.Role), IoModeName(Self.Core.mode()), BoundPort, ognetunreal::kEngineMaxPacketBytes, ognetunreal::kEnginePacketOverheadBytes, Config.budget.bytesPerSecond,
		Config.timings.idleTimeoutSeconds, EmulatorName);
	if (SimLanes.IsValid())
	{
		SimLanes->afterStart();
	}
	return true;
}

bool UOGNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ConnectURL: %s: %s"), *ConnectURL.ToString(), *Error);
		return false;
	}

	ServerConnection = NewObject<UOGNetConnection>(GetTransientPackage(), NetConnectionClass);
	ServerConnection->InitLocalConnection(this, nullptr, ConnectURL, USOCK_Pending);
	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed);

	FOGNetEndpointHost& Self = *Endpoint;
	if (UPendingNetGame* Join = PendingNetGameFor(*this, InNotify))
	{
		Self.HeldJoin = Join;
		Self.HeldJoinFrame = GFrameCounter;
		Self.bJoinHeld = true;
		UE_LOG(LogOGNetTransport, Log, TEXT("[OGNet.Driver] hello held frame=%llu"), Self.HeldJoinFrame);
	}
	else
	{
		CreateInitialClientChannels();
	}
	const double DeadlineSeconds = Self.Now() + InitialConnectTimeout * Self.TimeoutMultiplier;
	const uint16 Port = static_cast<uint16>(FMath::Clamp(ConnectURL.Port, 0, 65535));
	const FTCHARToUTF8 Host(*ConnectURL.Host);
	if (const std::optional<ognet::Address> Numeric = ognetunreal::numericJoinAddress(std::string_view(Host.Get(), Host.Length()), Port))
	{
		Self.Core.connect(*Numeric, DeadlineSeconds);
		return true;
	}

	ISocketSubsystem* Sockets = GetSocketSubsystem();
	if (Sockets == nullptr)
	{
		Error = TEXT("OGNetDriver: no socket subsystem to resolve the host name");
		UE_LOG(LogOGNetTransport, Error, TEXT("[OGNet.Driver] %s"), *Error);
		return false;
	}
	TWeakObjectPtr<UOGNetDriver> WeakDriver(this);
	Sockets->GetAddressInfoAsync(
		[WeakDriver, Port, DeadlineSeconds](FAddressInfoResult Result)
		{
			AsyncTask(ENamedThreads::GameThread,
				[WeakDriver, Port, DeadlineSeconds, Result = MoveTemp(Result)]()
				{
					if (UOGNetDriver* Driver = WeakDriver.Get())
					{
						FOGNetEndpointHost::OnJoinResolved(*Driver, Result, Port, DeadlineSeconds);
					}
				});
		},
		*ConnectURL.Host, nullptr, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Datagram);
	return true;
}

bool UOGNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ListenURL: %s: %s"), *LocalURL.ToString(), *Error);
		return false;
	}
	LocalURL.Port = LocalAddr->GetPort();
	UE_LOG(LogNet, Log, TEXT("%s OGNetDriver listening on port %i"), *GetDescription(), LocalURL.Port);
	checkf(!ConnectionlessHandler.IsValid(),
		TEXT("OGNetDriver: the packet handler is off in every configuration; og-net's cookie admits connections. Was design D4's 'the driver "
			 "never calls InitConnectionlessHandler'"));
	return true;
}

void UOGNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);
	if (!Endpoint.IsValid())
	{
		return;
	}
	FOGNetEndpointHost& Self = *Endpoint;
	const double NowSeconds = Self.Now();
	const bool bClientConnectionPending = ServerConnection != nullptr && ServerConnection->GetConnectionState() == USOCK_Pending;
	Self.Core.tick(NowSeconds, bClientConnectionPending);
	if (Self.Role == ognet::Role::Client)
	{
		Self.ReportRetries();
		if (Self.bJoinHeld)
		{
			FOGNetEndpointHost::ReleaseHeldJoin(*this);
		}
	}
	Self.ReportForeignTraffic(NowSeconds);
}

void UOGNetDriver::PostTickDispatch()
{
	Super::PostTickDispatch();
	if (SimLanes.IsValid())
	{
		SimLanes->afterDispatch();
	}
}

void UOGNetDriver::TickFlush(float DeltaSeconds)
{
	Super::TickFlush(DeltaSeconds);
	if (SimLanes.IsValid())
	{
		SimLanes->onTickFlush();
	}
}

void UOGNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (Endpoint.IsValid())
	{
		++Endpoint->ConnectionlessSendsDropped;
	}
	UE_LOG(LogOGNetTransport, Verbose, TEXT("[OGNet.Driver] connectionless send dropped to=%s bits=%d"),
		Address.IsValid() ? *Address->ToString(true) : TEXT("(none)"), CountBits);
}

void UOGNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();
	if (HasAnyFlags(RF_ClassDefaultObject) || !Endpoint.IsValid())
	{
		return;
	}
	FOGNetEndpointHost& Self = *Endpoint;
	const ognet::Role Role = Self.Role;
	if (SimLanes.IsValid())
	{
		checkf(!Self.Core.isStopped(),
			TEXT("OGNetDriver: the sim lanes' beforeStop runs before the core stops, while og-net still delivers and sends. Was design "
				 "P5 3.1's 'lanes.beforeStop (unbind) -> core.stop'"));
		SimLanes->beforeStop();
	}
	Self.Core.stop(Self.Now(), kStopDrainSeconds);
	const ognetunreal::DriverStats Stats = Self.Core.stats();
	const uint64 ConnectionlessSendsDropped = Self.ConnectionlessSendsDropped;
	const uint64 DeliveriesWithoutConnection = Self.DeliveriesWithoutConnection;
	Endpoint.Reset();
	DestroySimLanesAfterEndpoint(Endpoint, SimLanes);
	UE_LOG(LogOGNetTransport, Log,
		TEXT("[OGNet.Driver] stop role=%s accepted=%llu rejected=%llu retries=%llu sendsRefused=%llu preConnectBuffered=%llu "
			 "preConnectDropped=%llu ignoredDatagrams=%llu unknownDatagrams=%llu closesDrained=%llu closesTimedOut=%llu handlerQueueDropped=%llu "
			 "batches=%llu maxBatch=%llu connectionlessDropped=%llu deliveriesWithoutConnection=%llu"),
		RoleName(Role), Stats.accepted, Stats.rejected, Stats.retries, Stats.sendsRefused, Stats.preConnectBuffered, Stats.preConnectDropped,
		Stats.ignoredDatagrams, Stats.unknownDatagrams, Stats.closesDrained, Stats.closesTimedOut, Stats.handlerQueueDropped, Stats.batches,
		Stats.maxBatch, ConnectionlessSendsDropped, DeliveriesWithoutConnection);
}

ISocketSubsystem* UOGNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
}

bool UOGNetDriver::IsNetResourceValid()
{
	return Endpoint.IsValid() && Endpoint->Core.isStarted() && !Endpoint->Core.isStopped();
}

void UOGNetDriver::sendEngine(uint32 Id, const uint8* Data, int32 Bytes)
{
	if (!Endpoint.IsValid() || Bytes <= 0)
	{
		return;
	}
	Endpoint->Core.send(Id, std::span<const std::byte>(reinterpret_cast<const std::byte*>(Data), static_cast<std::size_t>(Bytes)));
}

void UOGNetDriver::engineClosed(UOGNetConnection& Connection)
{
	checkf(Connection.GetConnectionState() == USOCK_Closed,
		TEXT("OGNetDriver::engineClosed before the engine closed the connection: UOGNetConnection::CleanUp calls it after Super::CleanUp, so "
			 "the engine's close reason is queued before og-net's drained close. Was P4-4's 'CleanUp calls engineClosed and then "
			 "Super::CleanUp', reversed"));
	if (!Endpoint.IsValid())
	{
		return;
	}
	FOGNetEndpointHost& Self = *Endpoint;
	const uint32 Id = Connection.OGNetId;
	const ognet::ConnectionStats Stats = Self.Core.connectionStats(Id);
	const bool bLive = Id != ognet::kInvalidConnection && Stats.id == Id;
	const ognet::LaneConnectionStats& EngineLane = Stats.lanes[ognet::lanes::laneIndex(ognet::Lane::Engine)];
	const ognetunreal::DriverStats CoreStats = Self.Core.stats();
	ognet::DisconnectReason Reason = ognet::DisconnectReason::LocalClose;
	const bool bClosedByOGNet = Self.ClosedReasons.RemoveAndCopyValue(Id, Reason);
	Self.Connections.Remove(Id);
	UE_LOG(LogOGNetTransport, Log,
		TEXT("[OGNet.Driver] closed id=%u reason=%hs stats=%s engineSent=%llu engineRecv=%llu refused=%llu preConnectDropped=%llu "
			 "queueDropsAge=%llu queueDelayP99Us=%u"),
		Id, bClosedByOGNet ? DisconnectReasonName(Reason) : "engine", bLive ? TEXT("live") : TEXT("gone"), EngineLane.messagesSent,
		EngineLane.messagesReceived, CoreStats.sendsRefused, CoreStats.preConnectDropped, EngineLane.queueDropsAge, EngineLane.queueDelayP99Micros);
	Self.Core.engineClosed(Id, Self.Now());
}

void UOGNetDriver::SetSimLanesFactory(FOGNetSimLanesFactory Factory)
{
	checkf(IsInGameThread(), TEXT("UOGNetDriver::SetSimLanesFactory off the game thread: InitBase reads the factory on the game thread"));
	checkf(GSimLanesFactory == nullptr,
		TEXT("UOGNetDriver::SetSimLanesFactory called twice: the game registers one factory, once. Was task P5-3's 'a second set is a checkf'"));
	checkf(!GSimLanesInitBaseSeen,
		TEXT("UOGNetDriver::SetSimLanesFactory after a driver's InitBase: that driver runs without sim lanes. Was task P5-3's 'before the "
			 "first InitBase'"));
	GSimLanesFactory = Factory;
}

IOGNetSimLanes* UOGNetDriver::GetSimLanes() const
{
	return SimLanes.Get();
}

UOGNetConnection* UOGNetDriver::FindConnection(uint32 OGNetId) const
{
	checkf(IsInGameThread(), TEXT("UOGNetDriver::FindConnection off the game thread: the og-net id to connection map is the game thread's"));
	if (!Endpoint.IsValid())
	{
		return nullptr;
	}
	const TWeakObjectPtr<UOGNetConnection>* Found = Endpoint->Connections.Find(OGNetId);
	return Found != nullptr ? Found->Get() : nullptr;
}

double UOGNetDriver::GetSmoothedRttSeconds(uint32 OGNetId) const
{
	checkf(IsInGameThread(),
		TEXT("UOGNetDriver::GetSmoothedRttSeconds off the game thread: the endpoint behind the pimpl is created and destroyed on the "
			 "game thread"));
	if (!Endpoint.IsValid())
	{
		return -1.0;
	}
	const ognet::ConnectionStats Stats = Endpoint->Core.connectionStats(OGNetId);
	return Stats.id == OGNetId && Stats.hasRttSample ? Stats.smoothedRttSeconds : -1.0;
}
