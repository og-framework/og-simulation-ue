<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-simulation-ue

UE plugin shell wrapping [og-simulation](https://github.com/og-framework/og-simulation).

This repo contains the Unreal Engine integration layer: the `.uplugin`, the module rules, a GLM module stub, and the two Unreal modules `OGSimulationUnreal` and `OGSimulationJoltUnreal` (below; in og-brawler-unreal's `Source/` until og-tickdriver task 16). The pure C++ simulation source lives in the `og-simulation` and `og-simulation-jolt` submodules. No source is duplicated — UBT compiles it from the submodule paths.

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

The plugin exposes five UE modules, all `Runtime` with loading phase `Default`, in this order:

- `OGSimulation` — the deterministic simulation core, engine-free (source from the `og-simulation` submodule at `Source/OGSimulation/og-simulation/`)
- `glm` — the vendored GLM math library (header-only, from `Source/OGSimulation/og-simulation/glm/`)
- `OGSimulationJolt` — the Jolt physics backend, engine-free (source from the `og-simulation-jolt` submodule at `Source/OGSimulationJolt/og-simulation-jolt/`, with Jolt vendored there)
- `OGSimulationUnreal` — the Unreal adapter, game-agnostic: the tick driver's engine port and backend template, the Chaos adapters and engine traits, the replication relays, the input translator, the static-geometry importer and the startup timer-resolution honour
- `OGSimulationJoltUnreal` — the Unreal side of the Jolt engine: the Jolt engine traits

`OGSimulationUnreal` uses Enhanced Input, so `OGSimulation.uplugin` declares a dependency on the engine's `EnhancedInput` plugin; enabling this plugin enables it. Chaos and PhysicsCore are engine modules and need no entry.

## Layout

```
OGSimulation.uplugin                   Plugin descriptor
Source/OGSimulation/                   UE module rules + og-simulation/ (submodule — pure simulation source)
Source/glm/                            UE module stub for GLM (header-only)
Source/OGSimulationJolt/               UE module rules + og-simulation-jolt/ (submodule — Jolt backend source)
Source/OGSimulationUnreal/             Unreal adapter module (code and docs/)
Source/OGSimulationJoltUnreal/         Unreal + Jolt traits module (code and docs/)
```

## Canonical workflow

See [`og-brawler-unreal/docs/cross-repo-dev-loop.md`](https://github.com/og-framework/og-brawler-unreal/blob/main/docs/cross-repo-dev-loop.md) for the multi-repo development workflow.

## License and contributing

[MPL-2.0](LICENSES/MPL-2.0.txt). Inbound = outbound.

See [CONTRIBUTING.md](https://github.com/og-framework/og-brawler-unreal/blob/main/CONTRIBUTING.md) for the decision tree on where to make your change.
