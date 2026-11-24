<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-session

og-sim-session is the engine-free session runtime: what one world needs to run og-simulation's simulation over og-sim-net,
whatever engine hosts it. It depends on og-simulation and og-sim-net and includes no game-engine header. An engine supplies
it through ports (concepts); the runtime itself is built up from them in later steps.

Today it holds the ports and the vocabulary: the session's role, start parameters, registration request and outcomes
(`OGSimSession/SessionTypes.h`), the engine and configuration ports (`SessionEngine.h`), the game's two halves
(`SessionGame.h`), and the per-role thread table with its checks (`SessionThreads.h`).

## Layout

```
og-sim-session/
  .ubtignore  CONTRIBUTING.md  README.md  LICENSES/
  OGSimSession/  the library
    docs/        <File>-rationale.md for each source file
  tests/         Catch2 tests and their test kit, built by the host project's low-level-test target
```

The library builds as the Unreal module `OGSimulationSession` from a `Build.cs` that lives outside this folder, one level
above it. `OGSimSession/docs/OGSimSession-rationale.md` describes the module and the test wiring. There is no standalone
build yet.

## Licence

Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`).
