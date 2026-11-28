<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-simulation-ue

UE plugin shell wrapping [og-simulation](https://github.com/og-framework/og-simulation).

This repo contains the Unreal Engine integration layer: the `.uplugin`, the module rules, a GLM module stub, the three Unreal modules `OGNetUnreal`, `OGSimulationUnreal` and `OGSimulationJoltUnreal` (the last two in og-brawler-unreal's `Source/` until og-tickdriver task 16), and two engine-free libraries kept in this repo's own folders: og-sim-net (`Source/OGSimulationNet/og-sim-net/`) and og-sim-session (`Source/OGSimulationSession/og-sim-session/`). The other pure C++ sources live in submodules: `og-simulation` (with og-tickdriver in it), `og-net` and `og-simulation-jolt`. No source is duplicated — UBT compiles it from those paths.

## Position in the og-framework graph

```
og-simulation  (pure source submoduled at extern/og-simulation/)
    ↓ wrapped by
og-simulation-ue  (this repo — UE plugin shell)
    ↓ consumed as Plugins/OGSimulation/ by
og-brawler-unreal
```

## Related repos

| Repo | Role |
|---|---|
| [og-simulation](https://github.com/og-framework/og-simulation) | Pure C++ source submoduled here |
| [og-brawler-unreal](https://github.com/og-framework/og-brawler-unreal) | UE project that consumes this plugin |
| [og-brawler-ue](https://github.com/og-framework/og-brawler-ue) | Sibling plugin shell for the brawler layer |

## Quickstart

This plugin is consumed by [og-brawler-unreal](https://github.com/og-framework/og-brawler-unreal) as a submodule at `Plugins/OGSimulation/`. For the full project setup see that repo.

To use this plugin in your own UE project:

```bash
git submodule add https://github.com/og-framework/og-simulation-ue.git Plugins/OGSimulation
git submodule update --init --recursive Plugins/OGSimulation
```

Then open your `.uproject` — UBT will discover `OGSimulation.uplugin` and compile the plugin.

The plugin exposes ten UE modules, all `Runtime` with loading phase `Default`, in this order:

- `OGSimulation` — the deterministic simulation core, engine-free (source from the `og-simulation` submodule at `Source/OGSimulation/og-simulation/OGSimulation/`)
- `OGTickDriver` — og-tickdriver, the engine-free tick driver (source from the `og-simulation` submodule at `Source/OGSimulation/og-simulation/og-tickdriver/`; module rules at `Source/OGTickDriver/`; og-session-runtime task 27b)
- `OGNet` — og-net, the engine-free network transport (source from the `og-net` submodule at `Source/OGNet/og-net/`)
- `OGNetUnreal` — og-net's Unreal net driver and its sim lanes' endpoint
- `OGSimulationNet` — og-sim-net, the engine-free simulation traffic over og-net (source at `Source/OGSimulationNet/og-sim-net/`)
- `OGSimulationSession` — og-sim-session, the engine-free session runtime (source at `Source/OGSimulationSession/og-sim-session/`)
- `glm` — the vendored GLM math library (header-only, from `Source/OGSimulation/og-simulation/glm/`)
- `OGSimulationJolt` — the Jolt physics backend, engine-free (source from the `og-simulation-jolt` submodule at `Source/OGSimulationJolt/og-simulation-jolt/`, with Jolt vendored there)
- `OGSimulationUnreal` — the Unreal adapter, game-agnostic: the thin session adapter over og-sim-session's `SessionRuntime` (og-session-runtime task 31), the og-net driver's sim lanes and the hosts over og-sim-net's link, the authority's runner, the engine port (`SimulationEngineUImpl`) and the Unreal bodies, the latency diagnostics, the input translator, the static-geometry importer and the startup timer-resolution honour (until og-session-runtime tasks 3–6 also the Chaos adapters and engine traits; the replication relays went in og-input-latency task P5-9)
- `OGSimulationJoltUnreal` — the Unreal side of the Jolt engine: the Jolt engine traits

`OGSimulationUnreal` uses Enhanced Input, so `OGSimulation.uplugin` declares a dependency on the engine's `EnhancedInput` plugin; enabling this plugin enables it. Chaos and PhysicsCore are engine modules and need no entry.

## Layout

```
OGSimulation.uplugin                   Plugin descriptor
Source/OGSimulation/                   UE module rules + og-simulation/ (submodule — pure simulation source, og-tickdriver/ in it)
Source/OGTickDriver/                   UE module rules for og-tickdriver (source in og-simulation/og-tickdriver/)
Source/OGNet/                          UE module rules + og-net/ (submodule — engine-free network transport)
Source/OGNetUnreal/                    og-net's Unreal net driver and sim-lane endpoint (code and docs/)
Source/OGSimulationNet/                UE module rules + og-sim-net/ (engine-free sim traffic over og-net; this repo's own folder)
Source/OGSimulationSession/            UE module rules + og-sim-session/ (engine-free session runtime; this repo's own folder)
Source/glm/                            UE module stub for GLM (header-only)
Source/OGSimulationJolt/               UE module rules + og-simulation-jolt/ (submodule — Jolt backend source)
Source/OGSimulationUnreal/             Unreal adapter module (code and docs/)
Source/OGSimulationJoltUnreal/         Unreal + Jolt traits module (code and docs/)
standalone/                            CMake project: the engine-free libraries and their tests, without Unreal (README.md)
```

## Canonical workflow

See [`og-brawler-unreal/docs/cross-repo-dev-loop.md`](https://github.com/og-framework/og-brawler-unreal/blob/main/docs/cross-repo-dev-loop.md) for the multi-repo development workflow.

## License and contributing

[MPL-2.0](LICENSES/MPL-2.0.txt). Inbound = outbound.

See [CONTRIBUTING.md](https://github.com/og-framework/og-brawler-unreal/blob/main/CONTRIBUTING.md) for the decision tree on where to make your change.
