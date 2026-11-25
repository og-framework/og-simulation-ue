<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-session

og-sim-session is the engine-free session runtime: what one world needs to run og-simulation's simulation over og-sim-net,
whatever engine hosts it. It depends on og-simulation and og-sim-net and includes no game-engine header. An engine supplies
it through ports (concepts); the runtime itself is built up from them in later steps.

Today it holds the ports and the vocabulary: the session's role, start parameters, registration request and outcomes
(`OGSimSession/SessionTypes.h`), the engine and configuration ports (`SessionEngine.h`), the game's two halves
(`SessionGame.h`), the per-role thread table with its checks (`SessionThreads.h`), and the first part of the runtime: the
`[OGNetcode]` knob intake (`SessionKnobs.h`).

## Layout

```
og-sim-session/
  .ubtignore  CMakeLists.txt  CONTRIBUTING.md  README.md  LICENSES/
  OGSimSession/  the library
    docs/        <File>-rationale.md for each source file
  tests/         Catch2 tests and their test kit, built by the host project's low-level-test target or by CMakeLists.txt
```

The library builds as the Unreal module `OGSimulationSession` from a `Build.cs` that lives outside this folder, one level
above it. `OGSimSession/docs/OGSimSession-rationale.md` describes the module and the test wiring.

## Standalone build

`CMakeLists.txt` defines three targets:

- `og_sim_session`: the header-only library (C++20; `/fp:precise` on MSVC, `-ffp-contract=off` elsewhere, as the
  Build.cs).
- `og_sim_session_header_check`: compiles each header in a translation unit of its own, with exceptions and RTTI off, as
  the Build.cs. A header that reaches an Unreal header fails here.
- `og_sim_session_tests`: the tests, with one CTest entry per `[@og]` case (label `og_sim_session`), built when
  `OGNET_CATCH2_DIR` and `OG_SIMULATION_TESTS_DIR` are set. The test kit reaches og-simulation-tests'
  `TickDriver/TickDriverFakes.h` through a private include path to its `Source/OGSimulationTests`, as the Unreal test
  target does.

og-sim-net comes from the parent project when its target `og_sim_net` exists, or else from `OG_SIM_NET_DIR`, which takes
og-simulation and og-net in turn (`OG_SIMULATION_DIR`, `OG_NET_DIR`). og-simulation-jolt comes from the parent or
`OG_SIMULATION_JOLT_DIR` when either is given, and only the tests link it: the library does not depend on it.

```
cmake -S og-sim-session -B <out> -DOG_SIM_NET_DIR=<og-sim-net> -DOG_SIMULATION_DIR=<og-simulation> -DOG_NET_DIR=<og-net> -DOGNET_CATCH2_DIR=<Catch2 v3 amalgamated> -DOG_SIMULATION_TESTS_DIR=<og-simulation-tests>
cmake --build <out> --config Release
ctest --test-dir <out> -C Release
```

`<out>` is never inside a source folder. The plugin's standalone project (its `standalone/` folder) builds this folder
together with og-simulation, og-simulation-jolt, og-net and og-sim-net from one working tree.

## Licence

Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`).
