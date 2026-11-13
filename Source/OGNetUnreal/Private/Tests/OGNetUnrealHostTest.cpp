// SPDX-License-Identifier: MPL-2.0

#if WITH_DEV_AUTOMATION_TESTS

#include "OGNetUnrealHost.h"

#include "HAL/Event.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "OGNetUnreal/OGNetUnrealLog.h"

#include "OGNet/NetTypes.h"

#include <cstddef>

namespace
{
struct FOGNetHostThreadProbe
{
	FString name;
	EThreadPriority priority = TPri_Lowest;
	bool ranOnARunnableThread = false;
	FEventRef done;
};

class FOGNetTransportLineCapture final : public FOutputDevice
{
public:
	struct FLine
	{
		FString text;
		ELogVerbosity::Type verbosity;
	};

	virtual void Serialize(const TCHAR* text, ELogVerbosity::Type verbosity, const FName& category) override
	{
		if (category != LogOGNetTransport.GetCategoryName())
		{
			return;
		}
		const FScopeLock lock(&m_mutex);
		m_lines.Add(FLine{FString(text), static_cast<ELogVerbosity::Type>(verbosity & ELogVerbosity::VerbosityMask)});
	}

	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	TArray<FLine> linesContaining(const FString& marker)
	{
		const FScopeLock lock(&m_mutex);
		return m_lines.FilterByPredicate([&marker](const FLine& line) { return line.text.Contains(marker); });
	}

private:
	FCriticalSection m_mutex;
	TArray<FLine> m_lines;
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOGNetUnrealHostServicesTest,
	"OGNet.Unreal.HostServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGNetUnrealHostServicesTest::RunTest(const FString& Parameters)
{
	FOGNetHostThreadProbe probe;
	FOGNetUnrealHost host;

	host.startThread("OGNet", static_cast<int>(ognet::ThreadPriority::High),
		[](void* arg)
		{
			FOGNetHostThreadProbe& seen = *static_cast<FOGNetHostThreadProbe*>(arg);
			if (const FRunnableThread* thread = FRunnableThread::GetRunnableThread())
			{
				seen.ranOnARunnableThread = true;
				seen.name = thread->GetThreadName();
				seen.priority = thread->GetThreadPriority();
			}
			seen.done->Trigger();
		},
		&probe);
	const bool signalled = probe.done->Wait(FTimespan::FromSeconds(5.0));
	if (!TestTrue(TEXT("the OGNet thread ran and signalled within 5 s"), signalled))
	{
		return false;
	}
	TestTrue(TEXT("the thread function ran on an Unreal runnable thread"), probe.ranOnARunnableThread);
	TestEqual(TEXT("thread name"), probe.name, FString(TEXT("OGNet")));
	TestEqual(TEXT("thread priority (og-net High)"), static_cast<int32>(probe.priority), static_cast<int32>(TPri_AboveNormal));

	double previous = host.nowSeconds();
	int32 decreases = 0;
	for (int32 read = 0; read < 1000; ++read)
	{
		const double now = host.nowSeconds();
		if (now < previous)
		{
			++decreases;
		}
		previous = now;
	}
	TestEqual(TEXT("nowSeconds() decreases over 1,000 reads"), decreases, 0);

	FOGNetTransportLineCapture capture;
	GLog->AddOutputDevice(&capture);
	host.log(static_cast<int>(ognet::LogLevel::Info), "OGNet.Unreal.HostServices info line");
	host.log(static_cast<int>(ognet::LogLevel::Warning), "OGNet.Unreal.HostServices warning line");
	GLog->RemoveOutputDevice(&capture);

	const TArray<FOGNetTransportLineCapture::FLine> infoLines = capture.linesContaining(TEXT("OGNet.Unreal.HostServices info line"));
	if (TestEqual(TEXT("LogOGNetTransport lines for the Info line"), infoLines.Num(), 1))
	{
		TestEqual(TEXT("Info line verbosity"), static_cast<int32>(infoLines[0].verbosity), static_cast<int32>(ELogVerbosity::Log));
	}
	const TArray<FOGNetTransportLineCapture::FLine> warningLines =
		capture.linesContaining(TEXT("OGNet.Unreal.HostServices warning line"));
	if (TestEqual(TEXT("LogOGNetTransport lines for the Warning line"), warningLines.Num(), 1))
	{
		TestEqual(TEXT("Warning line verbosity"), static_cast<int32>(warningLines[0].verbosity),
			static_cast<int32>(ELogVerbosity::Warning));
	}

	std::byte bytes[32] = {};
	host.randomBytes(bytes, sizeof(bytes));
	bool anyNonZero = false;
	for (const std::byte value : bytes)
	{
		anyNonZero = anyNonZero || value != std::byte{0};
	}
	TestTrue(TEXT("32 random bytes are not all zero"), anyNonZero);

	return true;
}

#endif
