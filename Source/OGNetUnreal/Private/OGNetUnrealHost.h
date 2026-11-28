#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/OGNetUnrealHost-rationale.md

#include "Containers/Array.h"
#include "GenericPlatform/GenericPlatformAffinity.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Templates/UniquePtr.h"

#include "OGNet/NetConcepts.h"
#include "OGNet/NetTypes.h"

#include <cstddef>

class FOGNetUnrealHost
{
public:
	FOGNetUnrealHost() = default;
	FOGNetUnrealHost(const FOGNetUnrealHost&) = delete;
	FOGNetUnrealHost& operator=(const FOGNetUnrealHost&) = delete;

	~FOGNetUnrealHost();

	double nowSeconds() const { return FPlatformTime::Seconds(); }

	void startThread(const char* name, int priority, void (*fn)(void*), void* arg);

	void log(int level, const char* line);

	void randomBytes(std::byte* out, std::size_t size);

	static constexpr EThreadPriority toThreadPriority(int priority)
	{
		switch (static_cast<ognet::ThreadPriority>(priority))
		{
		case ognet::ThreadPriority::Normal:
			return TPri_Normal;
		case ognet::ThreadPriority::High:
			return TPri_AboveNormal;
		case ognet::ThreadPriority::TimeCritical:
			return TPri_TimeCritical;
		}
		return TPri_Normal;
	}

private:
	struct FStartedThread
	{
		TUniquePtr<FRunnable> runnable;
		TUniquePtr<FRunnableThread> thread;
	};

	FCriticalSection m_mutex;
	TArray<FStartedThread> m_threads;
};

static_assert(ognet::NetHostServices<FOGNetUnrealHost>, "FOGNetUnrealHost must satisfy ognet::NetHostServices");
static_assert(FOGNetUnrealHost::toThreadPriority(static_cast<int>(ognet::ThreadPriority::Normal)) == TPri_Normal,
	"og-net's Normal thread priority runs at Unreal's TPri_Normal");
static_assert(FOGNetUnrealHost::toThreadPriority(static_cast<int>(ognet::ThreadPriority::High)) == TPri_AboveNormal,
	"og-net's High thread priority, which its own I/O thread asks for, runs at Unreal's TPri_AboveNormal");
static_assert(FOGNetUnrealHost::toThreadPriority(static_cast<int>(ognet::ThreadPriority::TimeCritical)) == TPri_TimeCritical,
	"og-net's TimeCritical thread priority runs at Unreal's TPri_TimeCritical");
