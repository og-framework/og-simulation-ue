<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-net

og-sim-net is the engine-free layer that carries og-simulation's network traffic over og-net. It depends on og-simulation
and og-net and includes no game-engine header.

Its version header (`OGSimNet/SimNetVersion.h`) holds the API version and og-sim-net's part of a build's wire token.

## Layout

```
og-sim-net/
  .ubtignore  CMakeLists.txt  CONTRIBUTING.md  README.md  LICENSES/
  OGSimNet/   the library
    docs/     <File>-rationale.md and <File>-guards.md for each source file
  tests/      Catch2 tests, built by the host project's low-level-test target or by CMakeLists.txt
```

The library builds as the Unreal module `OGSimulationNet` from a `Build.cs` that lives outside this folder, one level above
it. `OGSimNet/docs/OGSimNet-rationale.md` describes the module and the test wiring.

## Standalone build

`CMakeLists.txt` defines three targets:

- `og_sim_net`: the header-only library (C++20; `/fp:precise` on MSVC, `-ffp-contract=off` elsewhere, as the Build.cs).
- `og_sim_net_header_check`: compiles each header in a translation unit of its own, with exceptions and RTTI off, as the
  Build.cs. A header that reaches an Unreal header fails here.
- `og_sim_net_tests`: the tests, with one CTest entry per `[@og]` case (label `og_sim_net`), built when `OGNET_CATCH2_DIR`
  and `OG_SIMULATION_TESTS_DIR` are set. They also reach og-net's `tests/` folder (found from the `og_net` target) for
  `VirtualNetwork.h`, and og-simulation-tests for the `[@og]` alias. `SimNetAuthorityLoopTest.cpp` includes the
  tick-driver fakes by a path relative to this folder, so the tests build only where og-simulation-tests sits where
  og-brawler-unreal has it.

og-simulation and og-net come from the parent project when its targets `og_simulation` and `og_net` exist, or else from
`OG_SIMULATION_DIR` and `OG_NET_DIR`, the way og-simulation-jolt takes og-simulation. The tests also link og-tickdriver:
the parent's `og_tickdriver` target, or else the `og-tickdriver/` folder of that og-simulation checkout.

```
cmake -S og-sim-net -B <out> -DOG_SIMULATION_DIR=<og-simulation> -DOG_NET_DIR=<og-net> -DOGNET_CATCH2_DIR=<Catch2 v3 amalgamated> -DOG_SIMULATION_TESTS_DIR=<og-simulation-tests>
cmake --build <out> --config Release
ctest --test-dir <out> -C Release
```

`<out>` is never inside a source folder. The plugin's standalone project (its `standalone/` folder) builds this folder
together with og-simulation, og-simulation-jolt, og-net and og-sim-session from one working tree.

## Licence

Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`).
