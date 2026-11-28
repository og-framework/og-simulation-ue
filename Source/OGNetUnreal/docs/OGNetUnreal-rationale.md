<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: catch_amalgamated.hpp -- Catch2 v3's single-file header, in the engine's ThirdParty/Catch2 directory, outside every scan root -->
# The `OGNetUnreal` module — rationale

`OGNetUnreal` is og-net's Unreal Engine glue: the engine-lane network driver and the host services og-net needs from the
engine. It lives in the OGSimulation plugin (MPL-2.0), beside the `OGNet` module that compiles og-net itself, and holds no
game code. This file is the docs pointer of `OGNetUnreal.Build.cs`, `OGNetUnrealModule.cpp` and `OGNetUnrealLog.h`, and it
records how the module's test sources are wired into the `OGNetTests` low-level-test target. The host services have their
own file, `OGNetUnrealHost-rationale.md`. These files state no prohibition, so there is no guards file.

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the build files disagree, the build files are authoritative and this file is stale.**

---

## §1 Module rules (`OGNetUnreal.Build.cs`)

- **`PCHUsage = UseExplicitOrSharedPCHs`, `bUseUnity = false`**, as `OGSimulationUnreal` does. A plugin module gets a unity
  build whatever its file count: UnrealBuildTool applies its minimum-file rule for unity builds only to modules under the
  project's own `Source/` directory (UEBuildModuleCPP.cs, lines 769-773). With unity off, every `.cpp` compiles alone, so a
  missing include fails here too instead of being hidden by a neighbour in the same unity file.
- **Public dependencies: `Core`, `CoreUObject`, `Engine`.** The driver pair the module will declare derives from the
  engine's net driver and net connection classes, so a module that names them needs those three.
- **Private dependencies: `NetCore`, `Sockets`, `PacketHandler`, `OGNet`.** The first three are for the driver pair (the
  engine's net types, the socket-subsystem address type, and the packet-handler hook the connection overrides); no source in
  the module uses them yet. `OGNet` is private so that og-net's headers never reach a module that depends on this one: a
  dependent sees only Unreal types.

## §2 The log category `LogOGNetTransport`

`OGNetUnrealLog.h` declares the category with `DECLARE_LOG_CATEGORY_EXTERN`, exported (`OGNETUNREAL_API`), default
verbosity `Log`; `OGNetUnrealModule.cpp` defines it with `DEFINE_LOG_CATEGORY`. It carries og-net's own lines (through the
host services' `log`, see `OGNetUnrealHost-rationale.md` §4) and the driver's event lines. It is a category of its own, not
the simulation's `LogOGNet` (declared in `UESimulationLogRoutes.h`), so that a capture can tell transport lines from the
simulation's network lines.

`OGNetUnrealModule.cpp` also declares the module to the engine with the default module class; the module has no start-up
or shut-down work.

## §3 The plugin listing, and og-net's exported symbols

The module is listed in `OGSimulation.uplugin` as a Runtime module, loading phase Default, directly after `OGNet`.

In the modular Editor build every module is its own DLL, and a call from one module into a non-inline function of
another links only if that function is exported. og-net marks the declarations its hosts call with `OGNET_API`
(`OGNetExport.h`, `OGNetExport-rationale.md`), which UnrealBuildTool defines as an export in the `OGNet` module and as an
import in a module that depends on it: this module's generated definitions file reads `#define OGNET_API DLLIMPORT`.
Measured with dumpbin in the modular Editor build: `UnrealEditor-OGNet.dll` exports 297 names, and
`UnrealEditor-OGNetUnreal.dll` imports two of them, `ognet::osRandomBytes` and `ognet::detail::checkFailed` (behind
every `OGNET_CHECK`), both from `FOGNetUnrealHost::randomBytes`. Those two declarations each carry a guard in og-net
(`NetCheck-guards.md` G-03, `OsRandom-guards.md` G-03).

Before og-net had the marker, the `OGNet` DLL exported only the engine's ThisIsAnUnrealEngineModule marker, and with this
module listed the Editor build compiled all three of its sources and then failed to link `UnrealEditor-OGNetUnreal.dll`:

```
OGNetUnrealHost.cpp.obj : error LNK2019: unresolved external symbol "bool __cdecl ognet::osRandomBytes(enum std::byte *,unsigned __int64)" ... referenced in function "public: void __cdecl FOGNetUnrealHost::randomBytes(enum std::byte *,unsigned __int64)"
OGNetUnrealHost.cpp.obj : error LNK2019: unresolved external symbol "void __cdecl ognet::detail::checkFailed(char const *,int,char const *)" ... referenced in function "public: void __cdecl FOGNetUnrealHost::randomBytes(enum std::byte *,unsigned __int64)"
UnrealEditor-OGNetUnreal.dll : fatal error LNK1120: 2 unresolved externals
```

A call this module adds into an og-net function without the marker fails the same way. The monolithic targets (a cooked
game, the low-level-test executables) link every module into one image, define `OGNET_API` as nothing and need no
exports, which is why nothing failed while `OGNet` had no dependent.

## §4 Test sources

- **Automation tests** live in `Private/Tests/` and compile into the module, guarded by WITH_DEV_AUTOMATION_TESTS. Today
  that is `OGNet.Unreal.HostServices` (`OGNetUnrealHost-rationale.md` §6). Run them with
  UnrealEditor-Cmd and `-ExecCmds="Automation RunTests OGNet.Unreal;Quit" -unattended -nullrhi`.
- **Engine-free Catch2 tests** live in `Tests/Catch2/` and are compiled by the `OGNetTests` target, never by this module:
  - UnrealBuildTool walks a module directory recursively and stops at a directory holding a file named `.ubtignore`, but
    only for that directory's **subdirectories**: the files beside the `.ubtignore` are still collected
    (UEBuildModuleCPP.cs: FindInputFilesFromDirectory adds each `.cpp` it enumerates, lines 2665-2695, and
    FindInputFilesFromDirectoryRecursive returns early only after that, lines 2622-2630).
  - So `Tests/` holds the empty `.ubtignore` and no source, and the test sources sit one level down, in `Tests/Catch2/`.
    og-net uses the same shape: its `.ubtignore` sits in og-net's root, above `tests/` (`OGNet-rationale.md` §2).
  - Measured: with the `.ubtignore` deleted, the Editor build compiles `EngineLaneDriverWiringTest.cpp` into this module
    and fails on its include of `catch_amalgamated.hpp` (C1083). With the test file directly beside the `.ubtignore`, the
    module compiled it too.
  - UnrealBuildTool's cached makefile records only the directories its walk visited, so moving or deleting test files is
    noticed only when the makefile is rebuilt (`-NoUBTMakefiles`), as `OGNet-rationale.md` §2 records for og-net.
- **The `OGNetTests` wiring.** `OGNetTests.Build.cs` (whose docs pointer is og-net's `OGNet-rationale.md`) adds
  `Tests/Catch2/` as a module directory, so its sources compile into the test executable, and adds two private include
  paths: this module's `Public/` and og-net's `tests/` (for `VirtualNetwork.h`, which the engine-free driver tests use).
  The test target does not depend on this module: the Catch2 tests include only engine-free headers.
- **`EngineLaneDriver.TheEngineLaneFitsUnrealsMaximumPacket`** (`[OGNet][EngineLaneDriver]`) is the wiring's own case. It
  checks that og-net's engine lane carries a packet of Unreal's largest size: `ognet::EngineLane::maxPacketBytes()` (1,189)
  is at least 1,024, the MAX_PACKET_SIZE of CoreNet.h (line 795) that Unreal's IP connection clamps its packets to
  (IpConnection.cpp, lines 110-116). Its `[OGNet]` tag puts it under the `[@og]` alias, so
  `OGNetTests.exe "[@og]"` runs it with og-net's own tests.
