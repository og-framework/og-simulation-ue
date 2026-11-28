<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionTestKit.h` — rationale

`SessionTestKit.h` (in og-sim-session's `tests/`) holds the test doubles of the session runtime's ports, for the
engine-free tests of this module and the ones the later runtime tasks add. It is the docs pointer target of
`SessionTestKit.h`.

**If this file and `SessionTestKit.h` disagree, the header is authoritative and this file is stale.**

---

## §1 `FakeSessionEngine`

A `SessionEngine` built on og-simulation-tests' fake engine (`TickDriverFakes.h`, `tickDriverFakes::FakeEngine`): it
inherits its virtual clock, frame counter, host log and executor, and adds the two members `SessionEngine` adds.

- `logRouted` records each line, under its own mutex, in the order received; `routedLines()` returns a copy. The fake
  engine's host log (`lines`) is separate, so a test can tell a routed line from a host-log line.
- `startRunner(body, highResolutionWait)` counts the call (`runnersStarted`) and returns a `FakeSessionRunner` that runs
  `body` on a new thread.

## §2 `FakeSessionRunner`

A `SessionRunner`: a `std::thread` that runs the body once, with the runner as its argument.

- `host()` is a `tickDriverFakes::FakeRunnerHost` over the engine: `sleepUntil` records the deadline and moves the fake
  engine's virtual clock to it. That clock is a plain `double`, so a test must not read it while a runner body sleeps on it;
  the skeleton tests read it after `join`.
- `join()` sets `stopRequested()` and joins the thread; the destructor joins too. A body that loops polls
  `stopRequested()`, as the tick driver's authority loop does.
- `threadId()` is the thread's id. The constructor stores it while holding `m_startMutex`, and the thread takes that
  mutex before it runs the body, so the id is published before the body can run: a body may call `threadId()` on
  its first line, and a test may compare it after the join.

## §3 `MapConfigSource`

A `ConfigSource` over a `std::map` of keys to text. `readInt` takes the whole value as a base-10 `int32_t`
(`std::from_chars`): a value with trailing characters, or one out of range, reads as absent. `readDouble` takes the whole
value with `std::strtod`. `readString` returns the text. A missing key reads as absent for all three. These are the kit's
rules, chosen so a test can see absence; the Unreal adapter's `ConfigSource` follows the engine's own parsing.

## §4 Its include path

The kit includes `"TickDriver/TickDriverFakes.h"`: the test target adds og-simulation-tests' `Source/OGSimulationTests`
folder to its include path (`OGSimSession-rationale.md` §5), so og-sim-session names no path of the other repository.
