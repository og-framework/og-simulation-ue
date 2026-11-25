<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-net's module and test wiring — rationale

og-sim-net is the engine-free layer between og-simulation and og-net: the part of the sim's network traffic that names og-net.
Its library sources under `OGSimNet/` build as the Unreal Engine module `OGSimulationNet`, and its tests under `tests/` build
into the og-simulation low-level-test target `OGSimulationTests`. This file records how both are wired and why. It is the docs
pointer of `OGSimulationNet.Build.cs`, `OGSimulationNetModule.cpp` and `SimNetVersion.h`. Those files state no prohibition, so
there is no guards file.

Engine source citations are to Unreal Engine 5.6's UnrealBuildTool (`Engine/Source/Programs/UnrealBuildTool/`), the engine this
project builds with. They are written without backticks because the engine is outside every tree this document's lint can
resolve against.

**If this file and the build files disagree, the build files are authoritative and this file is stale.**

<!-- lint-external-ref: catch_amalgamated.hpp -- Catch2 v3's single-file header, in the engine's ThirdParty/Catch2 directory, outside every scan root -->

---

## §1 Where the files live

- **og-sim-net is a plain folder for now**, at `Plugins/OGSimulation/Source/OGSimulationNet/og-sim-net/` in the OGSimulation
  plugin. It is laid out as a repository root (`README.md`, `CONTRIBUTING.md`, `LICENSES/MPL-2.0.txt`, `OGSimNet/`, `tests/`),
  so that it can become a submodule later at the same path with no code or path change.
- **Nothing inside og-sim-net names a path of the plugin's other code.** It reaches og-simulation and og-net only through their
  include roots, as `"OGSimulation/<...>.h"` and `"OGNet/<...>.h"`, and its tests reach og-net's test helper by its bare name,
  `"VirtualNetwork.h"`, on an include path the test target adds (§6).
- **og-sim-net holds no engine file.** The module rules (`OGSimulationNet.Build.cs`) and the module's one source file
  (`OGSimulationNetModule.cpp`, which declares the module to the engine) live one level above it, in
  `Plugins/OGSimulation/Source/OGSimulationNet/`. No source under `OGSimNet/` or `tests/` includes an engine header.

## §2 `.ubtignore`: the module compiles `OGSimNet/` only

UnrealBuildTool finds a module's sources by walking its module directory recursively. A directory that holds a file named
`.ubtignore` still contributes its own files: FindInputFilesFromDirectory lists every file in it, and only notes that the
ignore file is there (UEBuildModuleCPP.cs, lines 2665-2700). The walk then does not descend into that directory's
subdirectories (FindInputFilesFromDirectoryRecursive, lines 2622-2630).

- og-sim-net's root holds an empty `.ubtignore`. The walk from the module directory therefore takes
  `OGSimulationNetModule.cpp` and the files of og-sim-net's root, and neither `OGSimNet/` nor `tests/`.
- **og-sim-net's root holds no source file**, only Markdown and the licence folder, so the files the walk takes there are none
  that compile.
- `OGSimulationNet.Build.cs` adds `og-sim-net/OGSimNet` back with ConditionalAddModuleDirectory (ModuleRules.cs, lines
  1961-1969). Each added directory is walked from its own root, so the `.ubtignore` above it does not hide it. Without that
  file, `tests/` would compile into the runtime module, and the build would need Catch2's `catch_amalgamated.hpp`.
- The test target adds `og-sim-net/tests` the same way (§6).

## §3 Module settings

- **`PCHUsage = NoPCHs` and `bUseUnity = false`:** each `.cpp` sees exactly the headers it includes, with no engine header
  force-included and no unity neighbour, so a missing `#include` fails in the engine build too. og-sim-net's sources are meant
  to compile outside the engine. Measured on the Editor and `OGSimulationTests` builds: `OGSimulationNetModule.cpp`'s only
  force-included file is the module's generated definitions header.
- **`bEnableExceptions = false`, `bUseRTTI = false`:** og-sim-net uses neither. The module's exception setting can only turn
  exceptions on (UEBuildModuleCPP.cs, line 2316); the Editor target turns them on for every module (UEBuildTarget.cs, line
  5474). Measured: the Editor and `OGSimulationTests` builds compile `OGSimulationNetModule.cpp` with `/EHsc /GR-`.
- **No optimization pin.** The module builds at its target's optimization level.
- **`FPSemantics = FPSemanticsMode.Precise`** (og-input-latency task 20): og-sim-net includes og-simulation headers, so it
  compiles with the floating-point mode of every module that compiles simulation code (`/fp:precise` on Win64;
  og-simulation's `FloatingPointMode-rationale.md`). The module uses no precompiled header, so it needs no private one.
- **C++20:** the module does not set CppStandard, and is not an engine module, so it keeps its target's standard
  (UEBuildModuleCPP.cs, lines 2385-2393). Measured: the Editor and `OGSimulationTests` builds compile `OGSimulationNetModule.cpp` and
  `SimNetSkeletonTest.cpp` with `/std:c++20`.

## §4 Dependencies, include paths and the plugin listing

- **`OGSimulation` and `OGNet` are public dependencies.** og-sim-net's headers include headers of both libraries, so a module
  that includes an og-sim-net header needs both include roots.
- **`Core` is a private dependency**, for the module macro in `OGSimulationNetModule.cpp`.
- **The public include path is og-sim-net's root**, so every include of an og-sim-net header is spelled `"OGSimNet/<File>.h"`.
- **`OGSimulationNetModule.cpp` includes `SimNetVersion.h`**, so every target that builds the module compiles an og-sim-net
  header through the public include path under that target's warning flags. Since og-session-runtime task 21 that header
  includes `SimNetClock.h` and `SimNetRoster.h` (§7), so the module's one source file compiles those two as well.
- **Nothing is exported yet.** `SimNetVersion.h` holds only inline constants, and the module defines no out-of-line function.
  Measured: the Editor build's module DLL exports one name, the engine's module marker ThisIsAnUnrealEngineModule.
  In the modular Editor every module is its own DLL, so the first out-of-line og-sim-net function that another module calls
  needs an export marker on its declaration, as og-net's `OGNET_API` does for og-net.
- **The plugin lists `OGSimulationNet`** as a Runtime module, after `OGNetUnreal`. Nothing depends on it yet, so the game's own
  modules are unchanged.

## §5 `kSimNetApiVersion`

`kSimNetApiVersion` (`SimNetVersion.h`, namespace `ogsimnet`) is og-sim-net's API version, 1. It is a placeholder: nothing reads
it yet, and later tasks keep it. It is not a wire version. The bytes on the wire carry og-simulation's envelope version,
`kSimEnvelopeVersion` (`SimEnvelope.h`), and each payload codec's own version byte.

## §6 The tests, in `OGSimulationTests`

- og-sim-net has no test target of its own. `OGSimulationTests.Build.cs` adds `og-sim-net/tests` as a source directory with
  ConditionalAddModuleDirectory, depends on `OGNet` and `OGSimulationNet`, and adds og-net's `tests/` directory as a private
  include path, for `VirtualNetwork.h`.
- `SimNetVersionTest.cpp` pins the wire-token fragment of §7 (tag `[NetTransport][SimNetVersion]`).
- `SimNetSkeletonTest.cpp` proves both include paths in one translation unit. It includes `SimTransport.h` (og-simulation),
  `SimNetVersion.h` (og-sim-net) and `VirtualNetwork.h` (og-net's tests), connects an og-net client and server endpoint over a
  `VirtualNetwork`, and checks that one message sent on the lane numbered by `SimLane::SimInput` arrives on og-net's SimInput
  lane with its bytes.
- Its tag `[NetTransport]` is one of the tags og-simulation-tests' `[@og]` alias selects, so `OGSimulationTests.exe "[@og]"`
  runs it. On its own: `OGSimulationTests.exe "SimNetSkeleton.*"`.
- Measured: with og-net's `tests/` removed from the include path, `SimNetSkeletonTest.cpp` fails to compile, on
  `VirtualNetwork.h`.
- **The tests also build without Unreal.** og-sim-net's `CMakeLists.txt` builds the library, compiles each header alone and
  builds these tests, one CTest entry per `[@og]` case (og-sim-net's README.md, "Standalone build"); the plugin's
  `standalone/` project builds it with og-simulation, og-net and og-sim-session from one working tree (its README.md).

## §7 `kWireTokenFragment`: og-sim-net's part of the wire token

A game presents one wire token at login, the versions of every layout its peers must read alike, and refuses a peer whose
token differs (og-simulation's `SimWireToken-rationale.md` §1). og-sim-net owns two of those versions, so it exports them as
one fragment, `ogsimnet::kWireTokenFragment` in `SimNetVersion.h`: `ro<roster>.ck<clock>`, written with og-simulation's
`simWireToken::tokenOf` (og-session-runtime task 21).

- **`ro`** is `kRosterWireVersion` (`SimNetRoster.h`), the first byte of every roster body (`SimNetRoster-rationale.md` §2).
- **`ck`** is `kClockWireVersion` (`SimNetClock.h`), the layout of the SimClock ping and pong, which carry no version byte
  of their own (`SimNetClock-rationale.md` §2).

Today they are 1 and 2, so the fragment is `ro1.ck2`; `SimNetVersionTest.cpp` pins that string in a `static_assert`. A game
joins it after og-simulation's `simWireToken::kSimFragment` and includes this header for it instead of the two headers that
own the versions, so a bump of either changes this fragment and the game's pin of its whole token, and no game code.
`kSimNetApiVersion` (§5) is not part of the token.
