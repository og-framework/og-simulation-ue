<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-session tests

Catch2 tests of og-sim-session. Unreal builds every `.cpp` here into its `OGSimulationTests` low-level-test target; the standalone
CMake build (`../CMakeLists.txt`, aggregated by `Plugins/OGSimulation/standalone`) builds them into two executables:

| Executable | Sources | Checks |
|---|---|---|
| `og_sim_session_tests` | every `.cpp` here but the headless test | `OG_CHECK` is `assert`, so a Release build compiles it out |
| `og_sim_session_headless_tests` | `HeadlessSessionIntegrationTest.cpp` | live: `OG_SIM_SESSION_LIVE_CHECKS` (ON by default) defines `OG_CHECKS_LIVE` for this executable alone |

Both register each `[@og]` case as its own CTest entry (label `og_sim_session`).

## The headless host

`HeadlessSessionIntegrationTest.cpp` runs one authority and two clients, each a `Peer` of `HeadlessSessionHost.h`, in one process,
engine-free: og-sim-session's `SessionRuntime` over `std::thread` and the steady clock, og-net's endpoints over its test network
(`VirtualNetwork.h`, in og-net's `tests/`), Jolt world hosts and a test simulatable of its own (no game). The authority runs 2 s,
client 1 joins, then client 2; 600 ticks of scripted input follow; client 2 leaves at tick 240 and a new client 2 joins at tick 300.
It takes about 12.5 s. Why each part is as it is: `../OGSimSession/docs/HeadlessSessionHost-rationale.md`,
`HeadlessSessionLink-rationale.md` and `HeadlessPlatform-rationale.md` beside it.

Two hidden cases run only by name, as their own CTest entries, and pass when their check fires:

| CTest entry | Case | It passes when the output shows |
|---|---|---|
| `og_sim_session_headless_thread_check_fires` | `HeadlessSession.StartFrameFromASecondThreadTripsTheThreadCheck` | `OG_CHECK failed: isAllowed(op)` from `SessionThreads.h` |
| `og_sim_session_headless_netio_check_fires` | `HeadlessSession.AServiceNetOpOffOgNetsIoThreadTripsTheExactRow` | the same |

Under Unreal they are `[.][HeadlessSessionDeath]` and not part of `[@og]`.

## Live checks

`OG_CHECKS_LIVE` makes og-simulation's `OG_CHECK` (`OGAssert.h`) print `OG_CHECK failed: <condition> (<file>:<line>): <message>`
to stderr and exit with code 3, whatever `NDEBUG` says. It is set on the headless executable only. Undefining `NDEBUG` instead would
be unsafe: Jolt's library is built with it, and Jolt's JPH_DEBUG (which follows `NDEBUG`) changes the layout of Jolt classes
such as its body manager. Inline functions that other libraries also compile (og-simulation's own `.cpp` files) may keep their checked or
unchecked copy, whichever the linker takes; the checks in og-sim-session's and og-tickdriver's templates, which only this
executable instantiates, are live.
