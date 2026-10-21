<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGSimulationJoltUnreal.h` and `.cpp` — rationale

Companion to `Plugins/OGSimulation/Source/OGSimulationJoltUnreal/OGSimulationJoltUnreal.h` and `.cpp`, the module files of the
OGSimulationJoltUnreal module (og-tickdriver task 15e). Its guards are `OGSimulationJoltUnreal-guards.md` (none live).

> **Licence.** This tier is **MPL-2.0**, matching the code it describes (the user's ruling of 2026-10-18).

## §1 What the module is

The Unreal side of the Jolt engine that does not depend on a game: today the Jolt engine traits,
`JoltEngineTraitsUImpl` (`JoltEngineTraitsUImpl-rationale.md`). The user chose a module of its own for them
(2026-10-18) over adding Jolt to OGSimulationUnreal, so that OGSimulationUnreal stays free of Jolt: its rules name no
Jolt module and its code includes no og-simulation-jolt header. A future engine scheduled by og-simulation's tick
driver would get a module of the same shape.

| dependency | why |
|---|---|
| OGSimulationJolt | the Jolt adapters, the runtime, the fingerprint and the world-host template the traits name |
| OGSimulationUnreal | the Unreal world mutex (`UEFMutexLockableUImpl`), the engine port (`SimulationEngineUImpl`), the static importer's channel table type, and `LogOGSimHost`, the category of the log sink |
| OGSimulation, Core, CoreUObject, Engine | the og-simulation types and the Unreal types the included headers name |

A game that uses Jolt adds this module to its own dependencies and selects `JoltEngineTraitsUImpl` in its seam.

## §2 The module files

* The header includes Unreal's CoreMinimal header and declares nothing; the `.cpp` implements the module with Unreal's
  `FDefaultModuleImpl`. The module has no start-up work.
* The og-simulation-ue plugin lists the module in its descriptor, OGSimulation.uplugin (Runtime, loading phase
  Default, right after OGSimulationUnreal), since og-tickdriver task 16 (§4). Until then the project listed it in its
  `.uproject` and in its targets' extra module names, as it listed OGSimulationUnreal.
* No `#if` on a game's backend switch: the module compiles the same in every configuration of a game, and registers
  no console variable. The Jolt-only console variable `og.Jolt.ContactCounts` stays in the game module, where the
  backend switch is visible (`JoltEngineTraitsUImpl-rationale.md` §3).

## §3 One module for the thread context

og-simulation's thread context (`TickDriverThreadContext-rationale.md` §7 in og-simulation's docs) must compile into
one module, the game module. The traits' header names the world-host template, but an alias template is
instantiated where it is used, which is the game's instantiation of `TickDriverBackendUImpl`. The module's own
translation units (`OGSimulationJoltUnreal.cpp`, `JoltEngineTraitsUImpl.cpp`) instantiate no driver, step scope or
world host. og-tickdriver task 15e checked their objects with `dumpbin`, after a positive control on the game
module's object that holds the instantiation.

## §4 Moved to the og-simulation-ue plugin (og-tickdriver task 16)

og-tickdriver task 16 moved the module's folder, its code and these documents unchanged apart from path text and the
notes on the move (§2 and this section), from the game repository's Source folder into the og-simulation-ue plugin,
beside OGSimulationJolt, so that one submodule carries og-simulation, its Jolt backend and their Unreal modules. The
module keeps its name, so a game's module rules name it as before; the game enables the plugin instead of listing the
module. The module's rules name no project module, and its code includes nothing of a game. Task 16 re-ran the
one-module check of §3 over the module's objects at their new place, the plugin's intermediate folder.

The module's rules set `MinSourceFilesForUnityBuildOverride` to 32, as OGSimulationUnreal's did until
og-ue-simulationcomponentandmanagersplit task 19 (OGSimulationUnreal's set `bUseUnity = false` since;
`OGSimulationUnreal-rationale.md` §4): Unreal's build tool
builds a module with fewer than 32 source files one translation unit per file only when its rules lie under a
project's Source folder, so in the plugin the module would build as one unity translation unit. The override keeps
the compile mode it had as a project module; task 16 found the module's code identical to the pre-move build.
