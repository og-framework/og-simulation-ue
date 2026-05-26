<!-- SPDX-License-Identifier: MPL-2.0 -->
# og-simulation-ue

UE plugin shell wrapping [og-simulation](https://github.com/og-framework/og-simulation).

This repo contains only the Unreal Engine integration layer (`.uplugin`, `Build.cs`, a GLM module stub). The pure C++ simulation source lives in `extern/og-simulation/` via submodule. No source is duplicated — UBT compiles it from the extern path via `ConditionalAddModuleDirectory`.

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

The plugin exposes two UE modules:

- `OGSimulation` — the deterministic simulation core (source from `extern/og-simulation/OGSimulation/`)
- `glm` — the vendored GLM math library (header-only, from `extern/og-simulation/Source/glm/`)

## Layout

```
OGSimulation.uplugin       Plugin descriptor
Source/OGSimulation/       UE module stub + Build.cs
Source/glm/                UE module stub for GLM (header-only)
extern/og-simulation/      Submodule — pure simulation source
```

## Canonical workflow

See [`og-brawler-unreal/docs/cross-repo-dev-loop.md`](https://github.com/og-framework/og-brawler-unreal/blob/main/docs/cross-repo-dev-loop.md) for the multi-repo development workflow.

## License and contributing

[MPL-2.0](LICENSES/MPL-2.0.txt). Inbound = outbound.

See [CONTRIBUTING.md](https://github.com/og-framework/og-brawler-unreal/blob/main/CONTRIBUTING.md) for the decision tree on where to make your change.
