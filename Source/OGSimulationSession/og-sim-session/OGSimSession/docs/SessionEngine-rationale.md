<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionEngine.h` — rationale

`SessionEngine.h` holds three of the session runtime's ports (og-session-runtime design §3.3): the engine (`SessionEngine`),
the authority's runner (`SessionRunner`) and the configuration source (`ConfigSource`), and the engine's log sink on its own
(`SessionLogSink`, since og-session-runtime task 23). Each is a concept with a reference
shape that models it and one negative per requirement, checked by `static_assert`s in the header, the way
`TickDriverConcepts.h` checks the tick driver's. It is the docs pointer target of `SessionEngine.h`. Every rule the header
states is one of those `static_assert`s, so there is no guards file.

**If this file and `SessionEngine.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Why concepts

The design rejected virtual ports (design §3.8: indirect calls on the step path, and og-tickdriver's rule that hooks are
template parameters). The runtime is a template over its engine; a type that does not model a port fails where the runtime
is instantiated, and the self-checks fail the build if a concept stops accepting its reference shape or starts accepting a
negative. Seen failing at og-session-runtime task 22: each concept's negative made to conform in a scratch copy fails with
its own `static_assert` message and no other (clang-cl; and one under MSVC in the tree, then restored).

## §2 `SessionRunner`

The authority's runner thread (design §3.3, the Runner row; today `FUEAuthorityRunner`). Requirements, each with its
negative in `sessionRunnerSelfCheck`:

| requirement | negative |
|---|---|
| a `Host` type that models og-simulation's `TickDriverRunnerHost` (`sleepUntil`), so the tick driver's authority loop can sleep on it | `HostIsNoRunnerHost` |
| `host()` returns that host by reference | `HostByValue` |
| `stopRequested()`, on a const runner, returns `const std::atomic<bool>&`: the flag itself, which the loop polls while the frame thread sets it; `SimulationTickDriver::runAuthorityLoop` takes it as that type | `StopRequestedByValue` |
| `join()` | `MissingJoin` |

## §3 `SessionEngine`

The engine port: everything `TickDriverEngine` requires (the clock, the frame number, the executor, `kHostDiagnostics`, the
host log), plus:

| requirement | negative in `sessionEngineSelfCheck` |
|---|---|
| it is a `TickDriverEngine` | `NotATickDriverEngine` (no `deferUntil`) |
| `logRouted(const char*)`: one sink for lines that carry their own `[Level][Tag]` prefix (design §3.3; Unreal: `RouteOGLevelLine`) | `MissingLogRouted` |
| `wants(route, level)`, const, returns `bool` (`SessionLogFilter`, §3.2; og-session-runtime task 36) | `MissingWants` |
| a `Runner` type that models `SessionRunner` | `RunnerIsNoSessionRunner` |
| `startRunner(body, highResolutionWait)` returns `std::unique_ptr<Runner>` | `StartRunnerReturnsRunner` |

The concept calls `startRunner` with a probe body, `sessionEngineConceptProbe::RunnerBody`, a callable that takes the
runner by reference, as the tick driver's concept probes its executor with a task. **`startRunner` returns an owning pointer,
not the runner by value**, a change from the design's `-> Runner`: a runner's thread holds its address (today's
`FUEAuthorityRunner` is neither copyable nor movable), so the runtime must be able to hold it without moving it.

### 3.1 `SessionLogSink` (og-session-runtime task 23)

The engine's `logRouted` requirement is its own concept, and `SessionEngine` requires it. A component that only writes lines
takes a `SessionLogSink`, not a whole engine: `SessionKnobs.h`'s functions do, and the Unreal session passes them a
`UESessionLogSink` (`UESimulationLogRoutes.h`) before it has a session engine.

| requirement | negative in `sessionLogSinkSelfCheck` |
|---|---|
| `logRouted(const char*)` | `MissingLogRouted` |
| it returns `void`: a line has no outcome the session acts on | `LogRoutedReturnsAStatus` |

`SessionEngine`'s own negative for the requirement, `sessionEngineSelfCheck::MissingLogRouted`, still fails it through the
refinement.

### 3.2 `SessionLogFilter` (og-session-runtime task 36)

`wants(route, level)` on a const engine returns whether a line of that `SimulationLogRoute` and `HostLogLevel` would be
printed, so that a line written on every step can skip its formatting when its level is off (`SessionNetHooks`'
`[ReleaseBatch]`; og-sim-session's `SessionLogRoutes-rationale.md` §5). It is its own concept, as `SessionLogSink` is, and
`SessionEngine` requires both. The knob intake takes a `SessionLogSink` only: its lines are start-up lines. Unreal's is
`ogLogWants` (`UESimulationLogRoutes.h`).

| requirement | negative in `sessionLogFilterSelfCheck` |
|---|---|
| `wants(SimulationLogRoute, HostLogLevel)` | `MissingWants` |
| callable on a const engine | `WantsNotConst` |
| it returns `bool` | `WantsReturnsALevel` |

## §4 `ConfigSource`

The configuration port (design §3.3; today the `[OGNetcode]` section, read through `GConfig` from the game ini, then the
engine ini). One source reads one section, by key:

| requirement | negative in `configSourceSelfCheck` |
|---|---|
| `readInt(key)` returns `std::optional<int32_t>`: an absent key is told apart from a zero | `ReadIntWithoutAbsence` |
| `readDouble(key)` returns `std::optional<double>` | `ReadDoubleAsFloat` |
| `readString(key)` returns `std::optional<std::string>` | `MissingReadString`; `ReadStringAsACString` (task 33b: a `const char*` converts to the optional, and a null one for an absent key would build a `std::string` from a null pointer) |
| every read is callable on a const source | `MutatingRead` |

`int32_t` and `double` are the types today's knob intake reads (`GetInt`, `GetDouble`).
