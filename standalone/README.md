<!-- SPDX-License-Identifier: MPL-2.0 -->
# The standalone build

A CMake project that builds the plugin's engine-free libraries from this working tree, without Unreal, and runs their tests
with CTest. It is the in-tree engine-free instrument for og-sim-net and og-sim-session: a library that reaches an Unreal
header does not compile here.

| Library | Its CMake | Target | Tests (CTest) |
|---|---|---|---|
| og-simulation | `../Source/OGSimulation/og-simulation/OGSimulation/` | `og_simulation`; `og_simulation_header_check` compiles each header alone | `og_simulation_tests`, from og-simulation-tests |
| og-tickdriver | `../Source/OGSimulation/og-simulation/og-tickdriver/` | `og_tickdriver`; `og_tickdriver_header_check` compiles each header alone | its cases in `og_simulation_tests` |
| og-simulation-jolt | `../Source/OGSimulationJolt/og-simulation-jolt/` | `og_simulation_jolt` | its cases in `og_simulation_tests` |
| og-net | `../Source/OGNet/og-net/` | `og_net` | `og_net_tests`, `og_net_check_aborts` |
| og-sim-net | `../Source/OGSimulationNet/og-sim-net/` | `og_sim_net` | one CTest entry per `[@og]` case, label `og_sim_net` |
| og-sim-session | `../Source/OGSimulationSession/og-sim-session/` | `og_sim_session` | one CTest entry per `[@og]` case, label `og_sim_session` |

## Build and test

From the repository that holds this plugin (`og-brawler-unreal`):

```
cmake -S Plugins/OGSimulation/standalone -B Intermediate/OGStandalone/full -DOGNET_CATCH2_DIR=<Catch2 directory>
cmake --build Intermediate/OGStandalone/full --config Release
ctest --test-dir Intermediate/OGStandalone/full -C Release
```

- **The build tree goes under `Intermediate/` or `%TEMP%`, never into a source folder.** `Intermediate/` is ignored by git.
  On Windows prefer `Intermediate/`: MSBuild warns (MSB8029) that a build tree under the temporary directory can miss
  incremental rebuilds after a header change.
- **Catch2:** `OGNET_CATCH2_DIR`, the variable og-net's CMake already reads, names a directory holding
  catch_amalgamated.hpp and catch_amalgamated.cpp (Catch2 v3). The engine's bundled copy
  (`Engine/Source/ThirdParty/Catch2/v3.4.0/extras`) is the one the Unreal low-level-test targets use, and it also holds
  `Catch.cmake`, which registers each og-sim-net and og-sim-session case as its own CTest entry. Without `Catch.cmake`
  each of the two test executables is one CTest entry.
- **og-simulation-tests:** `OG_SIMULATION_TESTS_DIR` defaults to this working tree's
  `Source/OGSimulationTests/extern/og-simulation-tests`. It provides og-simulation's tests, the `[@og]` tag alias
  (`OgTagAliases.cpp`, compiled into every test executable here so `[@og]` selects the same cases as the Unreal run) and
  the tick-driver fakes og-sim-session's test kit builds on.
- `ctest -L og_sim_net` or `ctest -L og_sim_session` runs one library's cases.

## The core alone

```
cmake -S Plugins/OGSimulation/standalone -B Intermediate/OGStandalone/core -DOGNET_CATCH2_DIR=<Catch2 directory> -DOG_STANDALONE_CORE_ONLY=ON
```

builds og-simulation, og-tickdriver, og-simulation-jolt and og-simulation-tests only: og-net, og-sim-net and og-sim-session are not added,
so the core is shown to build and pass its tests without the layers above it.

## Flags

Every target compiles as C++20 with `/Zc:preprocessor /utf-8 /bigobj` (MSVC) and defines `OG_STANDALONE_BUILD`. The
floating-point mode comes from each library's own target: `/fp:precise` on MSVC, `-ffp-contract=off` elsewhere, as the
`FPSemantics = Precise` of the Unreal modules. Jolt's determinism defines come only through og-simulation-jolt's own target.
`OG_CHECK` is `assert`, so a `Release` build compiles it out, except in og-sim-session's `og_sim_session_headless_tests`,
which defines `OG_CHECKS_LIVE` (the CMake option `OG_SIM_SESSION_LIVE_CHECKS`, ON by default; og-sim-session's `tests/README.md`).

## Licence

Mozilla Public License 2.0 (`../LICENSES/MPL-2.0.txt`).
