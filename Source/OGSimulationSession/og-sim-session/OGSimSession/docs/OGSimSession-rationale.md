<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-session's module and test wiring — rationale

og-sim-session is the engine-free session runtime (og-session-runtime design §3.1). Its library sources under `OGSimSession/`
build as the Unreal Engine module `OGSimulationSession`, and its tests under `tests/` build into the og-simulation
low-level-test target `OGSimulationTests`. This file records how both are wired and why. It is the docs pointer of
`OGSimulationSession.Build.cs` and `OGSimulationSessionModule.cpp`. Those files state no prohibition, so there is no guards
file.

The module was made by og-session-runtime task 22, on the pattern of og-sim-net's (`OGSimNet-rationale.md`): the same
folder layout, the same module settings, one more library underneath. Engine build-tool behaviour that both modules rely on
is described once, in `OGSimNet-rationale.md` §2 and §3, and is not repeated here.

**If this file and the build files disagree, the build files are authoritative and this file is stale.**

---

## §1 Where the files live

- **og-sim-session is a plain folder**, at `Plugins/OGSimulation/Source/OGSimulationSession/og-sim-session/` in the
  OGSimulation plugin, beside og-sim-net. It is laid out as a repository root (`README.md`, `CONTRIBUTING.md`,
  `LICENSES/MPL-2.0.txt`, `OGSimSession/`, `tests/`), so that it can move whole, with og-sim-net, when the two leave the
  plugin repository (design_session_repo.md, the user's ruling of 2026-11-21).
- **Nothing inside og-sim-session names a path of the plugin's other code.** It reaches og-simulation and og-sim-net only
  through their include roots, as `"OGSimulation/<...>.h"` and `"OGSimNet/<...>.h"`, and its tests reach og-simulation-tests'
  tick-driver fakes as `"TickDriver/TickDriverFakes.h"`, on an include path the test target adds (§5).
- **og-sim-session holds no engine file.** The module rules (`OGSimulationSession.Build.cs`) and the module's one source file
  (`OGSimulationSessionModule.cpp`) live one level above it, in `Plugins/OGSimulation/Source/OGSimulationSession/`. No source
  under `OGSimSession/` or `tests/` includes an engine header.
- **The include root is og-sim-session's root**, so every include of a library header is spelled `"OGSimSession/<File>.h"`.
  The library's namespace is `ogsession`.

## §2 `.ubtignore`: the module compiles `OGSimSession/` only

og-sim-session's root holds an empty `.ubtignore`, as og-sim-net's does, and `OGSimulationSession.Build.cs` adds `og-sim-session/OGSimSession`
back as a module directory; `OGSimNet-rationale.md` §2 describes the engine behaviour. The root holds no source file, only
Markdown and the licence folder. Measured on the Editor build (og-session-runtime task 22): the module compiled
`OGSimulationSessionModule.cpp` and its generated inline file, and none of the files under `tests/`.

## §3 Module settings

The same as og-sim-net's, for the same reasons (`OGSimNet-rationale.md` §3): no precompiled header and no unity build, so each
`.cpp` sees exactly what it includes; no exceptions and no RTTI; no optimization pin; `FPSemantics = Precise`, because
og-sim-session includes og-simulation headers; C++20 from the target. Measured on task 22's builds, from the compiler
response files of `OGSimulationSessionModule.cpp`:

| target | flags |
|---|---|
| Editor, `OGSimulationTests` | `/std:c++20 /fp:precise /GR- /EHsc` |
| Game (`OGBrawlerUnreal`) | `/std:c++20 /fp:precise /GR-`, no `/EH` flag |

The Editor and test targets turn exceptions on for every module (`OGSimNet-rationale.md` §3). The only force-included file
is the module's generated definitions header.

## §4 Dependencies, include paths and the plugin listing

- **`OGSimulation` and `OGSimulationNet` are public dependencies.** og-sim-session's headers include og-simulation headers,
  and the runtime will include og-sim-net's (design §3.1), so a module that includes an og-sim-session header needs both
  include roots. og-net is reached only through og-sim-net: the tier lint forbids og-sim-session from including og-net,
  og-simulation-jolt or any engine module (`module_tiers.psd1`, the `OGSimSession` row).
- **`Core` is a private dependency**, for the module macro in `OGSimulationSessionModule.cpp`.
- **`OGSimulationSessionModule.cpp` includes the six library headers** (`SessionClock.h` since og-session-runtime task 24,
  `SessionEngine.h`, `SessionGame.h`, `SessionKnobs.h` since task 23, `SessionThreads.h`, `SessionTypes.h`), so every target that builds the module compiles each of them, with their
  self-check `static_assert`s, under that target's flags and after an engine header. That is where an identifier that
  collides with an engine macro would show first; it is why `SessionThreadChecks`' check is `checkOp`, not `check`
  (`SessionThreads-rationale.md` §4).
- **Nothing is exported.** The headers hold templates, inline functions and constants. Measured: the Editor build's module DLL
  exports one name, the engine's module marker ThisIsAnUnrealEngineModule.
- **The plugin lists `OGSimulationSession`** as a Runtime module, after `OGSimulationNet`.
- **`OGSimulationUnreal` and `OGBrawlerUnreal` list it as a public dependency.** Until og-session-runtime task 23 they included
  none of its headers and the dependency only added the include root (measured at task 22: the Editor build's
  `OGSimulationUnreal` response files carry it). Since task 23 `UESimulationSession.h` includes `SessionKnobs.h` (the knob
  intake) and `UEConfigSource.h` includes `SessionEngine.h` (the `ConfigSource` port); `OGBrawlerUnreal`'s route test includes
  `SessionKnobs.h`. Since task 24 `UESimulationSession.h` also includes `SessionClock.h` and `SessionThreads.h` (the clock
  routing and the schedule, and the clock's thread checks), and `OGBrawlerUnreal`'s clock route test includes `SessionClock.h`.
  `SessionClock.h` includes og-sim-net's `SimNetClock.h` (the SimClock client's counts on the window line) and `SimNetServer.h`
  (`kSimNetWindowSeconds`, the window's length, which only that header declares).

## §5 The tests, in `OGSimulationTests`

- og-sim-session has no test target of its own. `OGSimulationTests.Build.cs` adds `og-sim-session/tests` as a source
  directory, depends on `OGSimulationSession`, and adds og-simulation-tests' `Source/OGSimulationTests` folder as a private
  include path, for `TickDriver/TickDriverFakes.h`, which `SessionTestKit.h` builds on (`SessionTestKit-rationale.md`).
- Every case carries the tag `[Session]`, which og-simulation-tests' `[@og]` alias selects (`OgTagAliases.cpp`), so
  `OGSimulationTests.exe "[@og]"` runs them. On their own: `OGSimulationTests.exe "[Session]"`.
- `SessionSkeletonTest.cpp` proves the include roots in one translation unit (og-simulation, og-sim-net, og-sim-session and
  the test kit) and pins the test kit and the session types. `SessionThreadsTest.cpp` tests the thread table and its checks
  (`SessionThreads-rationale.md` §6), with one hidden death case, `[.][SessionThreadsDeath]`.
- Measured at task 22: `OGSimulationTests.exe "[Session]"` runs 12 cases, 288 assertions; `[@og]` grew from 1265 cases and
  994151 assertions to 1277 and 994439.
- **The tests also build without Unreal.** og-sim-session's `CMakeLists.txt` (og-session-runtime task 22b) builds the
  library, compiles each header alone and builds these tests, one CTest entry per `[@og]` case (og-sim-session's README.md,
  "Standalone build"); the plugin's `standalone/` project builds it with og-simulation, og-net and og-sim-net from one
  working tree (its README.md).
