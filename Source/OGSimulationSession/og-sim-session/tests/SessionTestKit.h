#pragma once
// SPDX-License-Identifier: MPL-2.0
// ../OGSimSession/docs/SessionTestKit-rationale.md

#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "OGSimSession/SessionEngine.h"
#include "TickDriver/TickDriverFakes.h"

namespace ogsession::sessionTestKit
{
class FakeSessionRunner
{
public:
	using Host = tickDriverFakes::FakeRunnerHost;
	using Body = std::function<void(FakeSessionRunner&)>;

	FakeSessionRunner(tickDriverFakes::FakeEngine& engine, Body body, bool highResolutionWait)
		: m_host(engine)
		, m_body(std::move(body))
		, m_highResolutionWait(highResolutionWait)
	{
		const std::lock_guard<std::mutex> starting(m_startMutex);
		m_thread = std::thread(
			[this]
			{
				{
					const std::lock_guard<std::mutex> started(m_startMutex);
				}
				m_body(*this);
			});
		m_threadId = m_thread.get_id();
	}

	~FakeSessionRunner() { join(); }

	FakeSessionRunner(const FakeSessionRunner&) = delete;
	FakeSessionRunner& operator=(const FakeSessionRunner&) = delete;

	Host& host() { return m_host; }
	const std::atomic<bool>& stopRequested() const { return m_stopRequested; }

	void join()
	{
		m_stopRequested.store(true, std::memory_order_release);
		if (m_thread.joinable())
			m_thread.join();
	}

	void waitForStart()
	{
		std::unique_lock<std::mutex> lock(m_latchMutex);
		m_latch.wait(lock, [this]() { return m_started; });
	}

	void releaseStart()
	{
		{
			const std::lock_guard<std::mutex> lock(m_latchMutex);
			m_started = true;
		}
		m_latch.notify_all();
	}

	bool highResolutionWait() const { return m_highResolutionWait; }
	std::thread::id threadId() const { return m_threadId; }

private:
	std::mutex              m_latchMutex;
	std::condition_variable m_latch;
	bool                    m_started = false;
	Host              m_host;
	Body              m_body;
	bool              m_highResolutionWait;
	std::atomic<bool> m_stopRequested{ false };
	std::mutex        m_startMutex;
	std::thread       m_thread;
	std::thread::id   m_threadId;
};

class FakeSessionEngine : public tickDriverFakes::FakeEngine
{
public:
	using Runner = FakeSessionRunner;

	void logRouted(const char* line)
	{
		const std::lock_guard<std::mutex> guard(m_routedMutex);
		m_routedLines.emplace_back(line);
	}

	bool wants(SimulationLogRoute, HostLogLevel) const { return true; }

	template <typename F>
	std::unique_ptr<Runner> startRunner(F&& body, bool highResolutionWait)
	{
		runnersStarted.fetch_add(1u, std::memory_order_relaxed);
		return std::make_unique<Runner>(*this, Runner::Body(std::forward<F>(body)), highResolutionWait);
	}

	std::vector<std::string> routedLines() const
	{
		const std::lock_guard<std::mutex> guard(m_routedMutex);
		return m_routedLines;
	}

	std::atomic<uint32_t> runnersStarted{ 0u };

private:
	mutable std::mutex       m_routedMutex;
	std::vector<std::string> m_routedLines;
};

class MapConfigSource
{
public:
	MapConfigSource() = default;
	explicit MapConfigSource(std::map<std::string, std::string> values)
		: m_values(std::move(values))
	{
	}

	void set(std::string key, std::string value) { m_values[std::move(key)] = std::move(value); }

	std::optional<int32_t> readInt(const char* key) const
	{
		const std::string* text = find(key);
		if (text == nullptr)
			return std::nullopt;
		int32_t value = 0;
		const char* const end = text->data() + text->size();
		const std::from_chars_result parsed = std::from_chars(text->data(), end, value);
		if (parsed.ec != std::errc() || parsed.ptr != end)
			return std::nullopt;
		return value;
	}

	std::optional<double> readDouble(const char* key) const
	{
		const std::string* text = find(key);
		if (text == nullptr || text->empty())
			return std::nullopt;
		char* end = nullptr;
		const double value = std::strtod(text->c_str(), &end);
		if (end != text->c_str() + text->size())
			return std::nullopt;
		return value;
	}

	std::optional<std::string> readString(const char* key) const
	{
		const std::string* text = find(key);
		if (text == nullptr)
			return std::nullopt;
		return *text;
	}

private:
	const std::string* find(const char* key) const
	{
		const auto found = m_values.find(key);
		return found != m_values.end() ? &found->second : nullptr;
	}

	std::map<std::string, std::string> m_values;
};

static_assert(SessionEngine<FakeSessionEngine>, "FakeSessionEngine must model SessionEngine");
static_assert(SessionRunner<FakeSessionRunner>, "FakeSessionRunner must model SessionRunner");
static_assert(ConfigSource<MapConfigSource>, "MapConfigSource must model ConfigSource");
} // namespace ogsession::sessionTestKit
