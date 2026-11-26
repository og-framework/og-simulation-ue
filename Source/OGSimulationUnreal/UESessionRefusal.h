// SPDX-License-Identifier: MPL-2.0
// docs/UESessionRefusal-rationale.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CoreMisc.h"
#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UEAuthorityRunner.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/UETickDriverHost.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <utility>

inline constexpr uint64 kSimNetDriverRefusedToastKey = 0x4F47534EF0000002ull;
inline constexpr float kSimNetDriverRefusedToastSeconds = 15.f;

struct UESessionNoSession
{
	bool        registerRefused = false;
	const char* reason = "no-session";
	bool        registerRaisesMessage = false;
};

namespace ueSessionRefusal
{
	inline constexpr char kRefusedLineFormat[] = "[SimHost.Refused] %s value=%s reason=%s";
	inline constexpr size_t kRefusedLineBytes = 160u;
	inline constexpr uint8 kServerExitCode = 3u;

	static_assert(kRefusedLineBytes >= sizeof(kRefusedLineFormat) + sizeof("role=authority netMode=DedicatedServer")
			+ sizeof("-2147483648") + sizeof("one-session-per-role-per-process"),
		"kRefusedLineBytes must hold the [SimHost.Refused] line with the longest subject (a second dedicated server's role), "
		"an int32 value and the longest reason (a second session of a role in one process)");

	inline void logRefusal(const char* subject, const char* value, const char* reason)
	{
		char line[kRefusedLineBytes];
		std::snprintf(line, sizeof(line), kRefusedLineFormat, subject, value, reason);
		UE_LOG(LogOGSimHost, Error, TEXT("%hs"), line);
	}

	inline void refuse(const char* subject, const char* value, const char* reason)
	{
		logRefusal(subject, value, reason);
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
				FString::Printf(TEXT("Simulation refused: %hs value=%hs reason=%hs. This world runs no simulation session"),
					subject, value, reason));
		}
	}

	inline void requestServerExit()
	{
		if (IsRunningDedicatedServer())
			FPlatformMisc::RequestExitWithStatus(/*Force=*/true, kServerExitCode);
	}

	inline bool refuseNetModeOrDriver(UWorld& world, ENetMode netMode, bool driverHasLanes, UESessionNoSession& noSession)
	{
		if (netMode == NM_Standalone)
		{
			UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Session] none netMode=Standalone"));
			noSession.reason = "dedicated-server-only";
			noSession.registerRaisesMessage = true;
			return true;
		}
		if (netMode == NM_ListenServer)
		{
			refuse("role=authority netMode=ListenServer", "-", "dedicated-server-only");
			noSession.reason = "dedicated-server-only";
			return true;
		}
		const bool isServer = netMode != NM_Client;
		if (driverHasLanes)
			return false;
		const UNetDriver* const driver = world.GetNetDriver();
		UE_LOG(LogOGNet, Error, TEXT("[SimNet.Host] refused role=%s driver=%s: simulation traffic needs the og-net driver"),
			isServer ? TEXT("server") : TEXT("client"), driver != nullptr ? *driver->GetClass()->GetName() : TEXT("none"));
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(kSimNetDriverRefusedToastKey, kSimNetDriverRefusedToastSeconds, FColor::Red,
				isServer ? TEXT("Simulation traffic needs the og-net driver: this server runs no simulation session")
				         : TEXT("Simulation traffic needs the og-net driver: this client simulates nothing"));
		}
		return true;
	}

	inline bool refuseRetiredKnobs(ENetMode netMode, bool simTrafficImmediateRefused)
	{
		const int32 runInline = UETickDriverHost::retiredRunInlineValue();
		if (runInline != 0)
		{
			char value[16];
			std::snprintf(value, sizeof(value), "%d", static_cast<int>(runInline));
			refuse("knob=og.Sim.RunInline", value, "retired");
			return true;
		}
		if (netMode == NM_Client)
		{
			if (!simTrafficImmediateRefused)
				return false;
			refuse("knob=SimTrafficImmediate", "0", "retired");
			return true;
		}
		if (retiredAuthorityLoopValue() != 0)
			return false;
		refuse("knob=og.Sim.AuthorityLoop", "0", "retired");
		return true;
	}

	inline bool refuseSecondSessionOfRole(UWorld& world, bool isAuthority, bool roleTaken, UESessionNoSession& noSession)
	{
		if (!roleTaken)
			return false;
		refuse(isAuthority ? "role=authority netMode=DedicatedServer" : "role=client netMode=Client",
			world.IsPlayInEditor() ? "pie" : "-", "one-session-per-role-per-process");
		noSession.reason = "one-session-per-role-per-process";
		return true;
	}

	inline void refuseRegistrationWithoutSession(unsigned int id, UESessionNoSession& noSession)
	{
		if (std::exchange(noSession.registerRefused, true))
			return;
		char subject[32];
		std::snprintf(subject, sizeof(subject), "register id=%u", id);
		if (noSession.registerRaisesMessage)
			refuse(subject, "-", noSession.reason);
		else
			logRefusal(subject, "-", noSession.reason);
	}
}

template <typename SessionT>
class UESessionRegistry
{
public:
	explicit UESessionRegistry(SessionT& owner) : m_owner(owner) {}

	UESessionRegistry(const UESessionRegistry&) = delete;
	UESessionRegistry& operator=(const UESessionRegistry&) = delete;

	static SessionT* instanceFor(const UWorld* world, bool isAuthority)
	{
		for (const std::atomic<UESessionRegistry*>& slot : s_instances)
		{
			UESessionRegistry* const entry = slot.load(std::memory_order_acquire);
			if (entry != nullptr && entry->m_world == world && entry->m_isAuthority == isAuthority)
				return &entry->m_owner;
		}
		return nullptr;
	}

	void note(const UWorld& world, bool isAuthority)
	{
		m_world = &world;
		m_isAuthority = isAuthority;
	}

	bool isAuthority() const { return m_isAuthority; }
	bool roleTaken() const { return s_started[m_isAuthority ? 0 : 1] != nullptr; }
	void markStarted() { s_started[m_isAuthority ? 0 : 1] = this; }

	void add()
	{
		for (std::atomic<UESessionRegistry*>& slot : s_instances)
		{
			UESessionRegistry* expected = nullptr;
			if (slot.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
				return;
		}
		checkf(false, TEXT("UESimulationSession::registerInstance: all %d registry slots are taken, one per world with a session ")
			TEXT("owner in this process"), static_cast<int32>(kSlots));
	}

	void remove()
	{
		for (std::atomic<UESessionRegistry*>& slot : s_instances)
		{
			UESessionRegistry* expected = this;
			slot.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
		}
		for (UESessionRegistry*& started : s_started)
		{
			if (started == this)
				started = nullptr;
		}
	}

private:
	SessionT&     m_owner;
	const UWorld* m_world = nullptr;
	bool          m_isAuthority = false;

	inline static constexpr size_t kSlots = 128u;
	inline static std::atomic<UESessionRegistry*> s_instances[kSlots] = {};
	inline static UESessionRegistry* s_started[2] = { nullptr, nullptr };
};
