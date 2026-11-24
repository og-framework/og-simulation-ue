<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-sim-net

og-sim-net is the engine-free layer that carries og-simulation's network traffic over og-net. It depends on og-simulation
and og-net and includes no game-engine header.

Its version header (`OGSimNet/SimNetVersion.h`) holds the API version and og-sim-net's part of a build's wire token.

## Layout

```
og-sim-net/
  .ubtignore  CONTRIBUTING.md  README.md  LICENSES/
  OGSimNet/   the library
    docs/     <File>-rationale.md and <File>-guards.md for each source file
  tests/      Catch2 tests, built by the host project's low-level-test target
```

The library builds as the Unreal module `OGSimulationNet` from a `Build.cs` that lives outside this folder, one level above
it. `OGSimNet/docs/OGSimNet-rationale.md` describes the module and the test wiring. There is no standalone build yet.

## Licence

Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`).
