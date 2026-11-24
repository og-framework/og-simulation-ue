#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/SessionEngine-rationale.md

#include <atomic>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "OGSimulation/TickDriverConcepts.h"

namespace ogsession
{
namespace sessionEngineConceptProbe
{
struct RunnerBody
{
	template <typename RunnerT>
	void operator()(RunnerT&) const
	{
	}
};
} // namespace sessionEngineConceptProbe

template <typename R>
concept SessionRunner = requires(R& r, const R& cr)
{
	typename R::Host;
	requires TickDriverRunnerHost<typename R::Host>;
	{ r.host() }           -> std::same_as<typename R::Host&>;
	{ cr.stopRequested() } -> std::same_as<const std::atomic<bool>&>;
	{ r.join() }           -> std::same_as<void>;
};

namespace sessionRunnerSelfCheck
{
struct Conforming
{
	using Host = tickDriverRunnerHostSelfCheck::Conforming;

	Host& host();
	const std::atomic<bool>& stopRequested() const;
	void join();
};

struct HostIsNoRunnerHost : Conforming
{
	using Host = tickDriverRunnerHostSelfCheck::MissingSleepUntil;

	Host& host();
};

struct HostByValue : Conforming
{
	Host host();
};

struct StopRequestedByValue : Conforming
{
	bool stopRequested() const;
};

struct MissingJoin
{
	using Host = tickDriverRunnerHostSelfCheck::Conforming;

	Host& host();
	const std::atomic<bool>& stopRequested() const;
};

static_assert(SessionRunner<Conforming>,
	"SessionRunner self-check: the reference shape must satisfy the concept");
static_assert(!SessionRunner<HostIsNoRunnerHost>,
	"SessionRunner self-check: the runner's host is a TickDriverRunnerHost, so the authority loop can sleep on it");
static_assert(!SessionRunner<HostByValue>,
	"SessionRunner self-check: host() returns the runner's own host by reference, never a copy");
static_assert(!SessionRunner<StopRequestedByValue>,
	"SessionRunner self-check: stopRequested() returns the flag itself, which the authority loop polls while the frame "
	"thread sets it");
static_assert(!SessionRunner<MissingJoin>,
	"SessionRunner self-check: join is required; the frame thread joins the runner it started");
} // namespace sessionRunnerSelfCheck

template <typename E>
concept SessionEngine = TickDriverEngine<E>
	&& requires(E& e, const char* line, sessionEngineConceptProbe::RunnerBody body, bool highResolutionWait)
{
	{ e.logRouted(line) } -> std::same_as<void>;
	typename E::Runner;
	requires SessionRunner<typename E::Runner>;
	{ e.startRunner(std::move(body), highResolutionWait) } -> std::same_as<std::unique_ptr<typename E::Runner>>;
};

namespace sessionEngineSelfCheck
{
struct Conforming : tickDriverEngineSelfCheck::Conforming
{
	using Runner = sessionRunnerSelfCheck::Conforming;

	void logRouted(const char*);
	template <typename F> std::unique_ptr<Runner> startRunner(F&&, bool);
};

struct NotATickDriverEngine : tickDriverEngineSelfCheck::MissingDeferUntil
{
	using Runner = sessionRunnerSelfCheck::Conforming;

	void logRouted(const char*);
	template <typename F> std::unique_ptr<Runner> startRunner(F&&, bool);
};

struct MissingLogRouted : tickDriverEngineSelfCheck::Conforming
{
	using Runner = sessionRunnerSelfCheck::Conforming;

	template <typename F> std::unique_ptr<Runner> startRunner(F&&, bool);
};

struct RunnerIsNoSessionRunner : Conforming
{
	using Runner = sessionRunnerSelfCheck::MissingJoin;

	template <typename F> std::unique_ptr<Runner> startRunner(F&&, bool);
};

struct StartRunnerReturnsRunner : Conforming
{
	template <typename F> Runner startRunner(F&&, bool);
};

static_assert(SessionEngine<Conforming>,
	"SessionEngine self-check: the reference shape must satisfy the concept");
static_assert(!SessionEngine<NotATickDriverEngine>,
	"SessionEngine self-check: it refines TickDriverEngine, so logRouted and startRunner alone are not an engine");
static_assert(!SessionEngine<MissingLogRouted>,
	"SessionEngine self-check: logRouted (the one sink for lines that carry their own level and tag) is required");
static_assert(!SessionEngine<RunnerIsNoSessionRunner>,
	"SessionEngine self-check: the engine's Runner is a SessionRunner");
static_assert(!SessionEngine<StartRunnerReturnsRunner>,
	"SessionEngine self-check: startRunner returns the runner it started as a std::unique_ptr; the runner's thread holds "
	"its address, so it is never copied or moved");
} // namespace sessionEngineSelfCheck

template <typename C>
concept ConfigSource = requires(const C& c, const char* key)
{
	{ c.readInt(key) }    -> std::same_as<std::optional<int32_t>>;
	{ c.readDouble(key) } -> std::same_as<std::optional<double>>;
	{ c.readString(key) } -> std::same_as<std::optional<std::string>>;
};

namespace configSourceSelfCheck
{
struct Conforming
{
	std::optional<int32_t> readInt(const char*) const;
	std::optional<double> readDouble(const char*) const;
	std::optional<std::string> readString(const char*) const;
};

struct ReadIntWithoutAbsence : Conforming
{
	int32_t readInt(const char*) const;
};

struct ReadDoubleAsFloat : Conforming
{
	std::optional<float> readDouble(const char*) const;
};

struct MissingReadString
{
	std::optional<int32_t> readInt(const char*) const;
	std::optional<double> readDouble(const char*) const;
};

struct MutatingRead : Conforming
{
	std::optional<int32_t> readInt(const char*);
};

static_assert(ConfigSource<Conforming>,
	"ConfigSource self-check: the reference shape must satisfy the concept");
static_assert(!ConfigSource<ReadIntWithoutAbsence>,
	"ConfigSource self-check: readInt returns std::optional, so an absent key is told apart from a zero");
static_assert(!ConfigSource<ReadDoubleAsFloat>,
	"ConfigSource self-check: readDouble returns std::optional<double>; a float would round the ini's value");
static_assert(!ConfigSource<MissingReadString>,
	"ConfigSource self-check: readString (the knobs spelled as words) is required");
static_assert(!ConfigSource<MutatingRead>,
	"ConfigSource self-check: every read is callable on a const source; reading changes nothing");
} // namespace configSourceSelfCheck
} // namespace ogsession
