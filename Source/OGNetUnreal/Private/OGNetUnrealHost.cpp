// SPDX-License-Identifier: MPL-2.0
// docs/OGNetUnrealHost-rationale.md

#include "OGNetUnrealHost.h"

#include "Misc/ScopeLock.h"
#include "OGNetUnreal/OGNetUnrealLog.h"

#include "OGNet/Crypto/OsRandom.h"
#include "OGNet/NetCheck.h"

namespace
{
class FOGNetHostRunnable final : public FRunnable
{
public:
	FOGNetHostRunnable(void (*fn)(void*), void* arg)
		: m_fn(fn)
		, m_arg(arg)
	{
	}

	virtual uint32 Run() override
	{
		m_fn(m_arg);
		return 0;
	}

private:
	void (*m_fn)(void*);
	void* m_arg;
};
} // namespace

FOGNetUnrealHost::~FOGNetUnrealHost()
{
	TArray<FStartedThread> threads;
	{
		const FScopeLock lock(&m_mutex);
		threads = MoveTemp(m_threads);
	}
	for (FStartedThread& started : threads)
	{
		started.thread->WaitForCompletion();
		started.thread.Reset();
		started.runnable.Reset();
	}
}

void FOGNetUnrealHost::startThread(const char* name, int priority, void (*fn)(void*), void* arg)
{
	OGNET_CHECK(fn != nullptr, "FOGNetUnrealHost::startThread: no thread function");
	TUniquePtr<FRunnable> runnable = MakeUnique<FOGNetHostRunnable>(fn, arg);
	FRunnableThread* thread =
		FRunnableThread::Create(runnable.Get(), UTF8_TO_TCHAR(name != nullptr ? name : ""), 0, toThreadPriority(priority));
	OGNET_CHECK(thread != nullptr, "FOGNetUnrealHost::startThread: Unreal could not create the thread");

	const FScopeLock lock(&m_mutex);
	m_threads.Add(FStartedThread{MoveTemp(runnable), TUniquePtr<FRunnableThread>(thread)});
}

void FOGNetUnrealHost::log(int level, const char* line)
{
	const char* utf8 = line != nullptr ? line : "";
	switch (static_cast<ognet::LogLevel>(level))
	{
	case ognet::LogLevel::Error:
		UE_LOG(LogOGNetTransport, Error, TEXT("%s"), UTF8_TO_TCHAR(utf8));
		return;
	case ognet::LogLevel::Warning:
		UE_LOG(LogOGNetTransport, Warning, TEXT("%s"), UTF8_TO_TCHAR(utf8));
		return;
	case ognet::LogLevel::Info:
		UE_LOG(LogOGNetTransport, Log, TEXT("%s"), UTF8_TO_TCHAR(utf8));
		return;
	case ognet::LogLevel::Verbose:
		UE_LOG(LogOGNetTransport, Verbose, TEXT("%s"), UTF8_TO_TCHAR(utf8));
		return;
	}
	UE_LOG(LogOGNetTransport, Log, TEXT("%s"), UTF8_TO_TCHAR(utf8));
}

void FOGNetUnrealHost::randomBytes(std::byte* out, std::size_t size)
{
	const bool drawn = ognet::osRandomBytes(out, size);
	OGNET_CHECK(drawn, "FOGNetUnrealHost::randomBytes: the OS random source failed");
}
