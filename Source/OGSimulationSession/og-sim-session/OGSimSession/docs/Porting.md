<!-- SPDX-License-Identifier: MPL-2.0 -->
# Porting the session to another engine

What an engine adapter writes to run og-sim-session's `SessionRuntime`, item by item, with the concept each item models, the
header that states it, the Unreal file that does it today and the thread it runs on. Written by og-session-runtime task 33
against the tree after task 31 (the thin Unreal adapter); the list is og-session-runtime design §3.6's ("what a Godot adapter
writes"). The Unreal adapter's own description is `OGSimulationUnreal/docs/UESimulationSession-rationale.md` §0.

**If this file and the headers disagree, the headers are authoritative.** Each concept has a reference shape and one negative per
requirement beside it, so the compiler states the contract exactly; this file says where to look.

Nothing on the list is netcode, lifecycle ordering, clock policy or threading: those are the runtime's (`SessionRuntime.h`,
`SessionRuntime-rationale.md`, beside this file).

---

## 1. The eight items

Threads: **frame** is the engine's main thread (Unreal's game thread); **step** is the thread running a client's step (a worker
task while it runs a batch); **runner** is an authority's own thread (the authority loop: it builds the world, pumps, steps and
services og-net); **og-net I/O** is og-net's own servicing thread on a client. The session's table of who calls what is
`SessionThreads-rationale.md`, and `SessionThreads.h` checks it.

| # | What the adapter writes | Concept it models | The concept's header | The Unreal file that does it today | Thread |
|---|---|---|---|---|---|
| 1 | The og-net peer for the engine lane, and its host services | `ognet::NetHostServices` (the host's clock, thread start, log and random bytes) and `ognet::DatagramBackend` (the socket) | og-net's `OGNet/NetConcepts.h` | OGNetUnreal: `Private/OGNetUnrealHost.h` (`FOGNetUnrealHost`, asserted there) and `Public/OGNetUnreal/Core/SwitchableBackend.h` (the backend, asserted there), under the driver `OGNetDriver.h` (`UOGNetDriver`) | frame (the driver's calls); og-net I/O on a client (`IoMode::OwnThread`), the runner on a dedicated server (`IoMode::HostServiced`) |
| 2 | A `SessionEngine`: the clock, the frame count, a logger, an executor for the step tasks, and a runner | `ogsession::SessionEngine` (refines og-tickdriver's `TickDriverEngine` with `SessionLogSink` and the runner factory); the runner is `ogsession::SessionRunner` | `OGSimSession/SessionEngine.h`; `OGTickDriver/TickDriverConcepts.h` | OGSimulationUnreal: `UESessionEngine.h` / `.cpp` (`SimulationEngineUImpl`; `UESessionRunner` over `UEAuthorityRunner.h`'s `FUEAuthorityRunner`), held by `UETickDriverHost.h` | frame (clock, frame count, log, dispatch); the step tasks on the executor's workers; the runner thread it creates |
| 3 | Two nodes that call the tick driver's `startFrame` and `endFrame` | the runtime's port hooks `beginFrames(driver)` and `endFrames()` (`ogsession::SessionRuntimeHost`); the driver is og-tickdriver's `SimulationTickDriver` | `OGSimSession/SessionRuntime.h`; `OGTickDriver/SimulationTickDriver.h` | `UETickDriverHost.h` / `.cpp` (the two tick functions), begun by `UESimulationSession.h`'s `beginFrames` | frame |
| 4 | A `ConfigSource` over the engine's settings, and a `SessionStart` (the step length, the caps, the static geometry through the axis map) | `ogsession::ConfigSource`; `ogsession::SessionStart` is a struct | `OGSimSession/SessionEngine.h`; `OGSimSession/SessionTypes.h` | `UEConfigSource.h` (the game ini, then the engine ini); `UESimulationSession.h`'s `beginPlay` fills the start (with `ueSessionEngine::importStatics`, `UESessionEngine.h`, for the static geometry) | frame (once, at start) |
| 5 | A render sink over the engine's scene nodes, and the present controls | og-tickdriver's `TickDriverRenderSink` (the presenter's template parameter); the controls are og-tickdriver's `PresentControls` | `OGTickDriver/TickDriverConcepts.h`; `OGTickDriver/TickDriverPresenter.h` | `UERenderTargetTable.h` (the sink), `UEPresentControls.h` (the controls from console variables), together in `UESessionEngine.h`'s `UESessionPresentation` | frame |
| 6 | The link adapter: create a `SimNetLink` when the peer starts, service it each frame, stop it with the peer | og-sim-net's `SimNetLinkServices`, `SimNetLinkSession`, `SimNetLinkEndpoint` and `SimNetLinkLock` (the link's template parameters); on an authority the server host's ports (`SimNetServerHost.h`); on the session's side `SessionNetLanes` and `SessionClockLink` | og-sim-net's `OGSimNet/SimNetLink.h` and `OGSimNet/SimNetServerHost.h`; `OGSimSession/SessionNetHooks.h`, `OGSimSession/SessionClock.h` | `UESimNetLanes.h` (the driver's `IOGNetSimLanes`, over the link) and `UESimNetHost.h` (`UESimNetLinkServices`, `UESimNetLinkSession`, the server host's Unreal port) | frame (the link's frame doors); the step (`drainClient_Step`); og-net I/O (the transport's handlers) on a client; the runner on an authority |
| 7 | Node ↔ `EntityKey` bookkeeping; `tryRegister` / `requestUnregister` and what each outcome does to the scene | `ogsession::SessionRuntimeHost` (teardown, forget, `forEachRegisteredId`, the body requests) and `ogsession::SessionBodies` (the bodies the lifecycle creates and binds); the registration hooks are `SessionAuthorityRegisterHooks` | `OGSimSession/SessionRuntime.h`; `OGSimSession/SessionLifecycle.h` | `UESimulationSession.h` (`tryRegister`, `unregisterFromNewFramework`, the outcome handlers, the component maps) and `TickDriverBackendUImpl.h` (the bodies, the keys: the component's and the pawn's addresses) | frame; an authority's bodies are bound on the runner (the lifecycle's requests) |
| 8 | The game's engine-side hooks (input sampling, presentation) | `ogsession::SessionGameHooks` as designed; today the runtime calls the game's statics through the traits and the adapter calls its instance hooks (`ueSimulationSessionHookNames::GameInstanceHooks`) | `OGSimSession/SessionGame.h`; `OGSimulationUnreal/TickDriverBackendUImpl.h` | the game module: the brawler's `BrawlerSessionHooksUImpl.h` (sampling, the slot request) and its session owner's instance hooks | frame (sampling, `afterBodiesCreated`, `presentAuthoritySlice`, `afterVisualization`); the runner for the authority's presentation slice (`authorityPresentationSlice_Step`, read after each step) |

Notes on the columns:

- **Item 8's concept.** `SessionGameHooks` is the port as design §3.3 wrote it, with its reference shape and negatives in
  `SessionGame.h`, but no Unreal type is asserted against it at this tree: the Unreal game's hooks are split between the traits'
  statics (which the runtime calls) and the instance hooks `UESimulationSession` calls, checked by `GameInstanceHooks`. A second
  adapter should model `SessionGameHooks`; an assertion on the Unreal game is a code change for a later task.
- **Item 3** is a port hook, not a concept of its own: the runtime hands its driver to `beginFrames` and the adapter registers
  whatever its engine calls once per frame before and after physics.
- **Items 1 and 6** are the network half. og-net's own Unreal binding (OGNetUnreal) is not part of the session; a second engine
  needs its own og-net peer before item 6 has anything to attach to.

---

## 2. The core without our session

og-simulation runs without og-sim-net and without this session; its own tests do (og-session-runtime's design of the
session's repository, its §3.1, re-pointed here at today's tree). What a session must supply to the core, and the smallest one
in the tree:

| Seam | What a session supplies | The smallest one in the tree |
|---|---|---|
| Composition | storage, static data, reconciliation, input resolution, net sync, systems, integration, then `SimulationManager(role, dt, Params)` | `JoltRollbackSuiteRig.h`'s `DriverRig` constructor (lines 759–780): the members in that order and the manager's `Params` |
| A physics world | a `PhysicsWorldAdapter` with its body and query adapters | og-simulation-jolt; the rig's Jolt scene; og-simulation-tests' `MockPhysicsWorld.h` |
| **Per character: an owner** | a type named by `SimulatableOwnerTraits`, modelling `PredictionSyncedBufferOwnerConcept` and `AuthoritySyncedBufferOwnerConcept` (og-simulation's `SimulationNetSync.h`). This is the core's real network seam | `JoltRollbackSuiteRig.h`'s `ProbeOwner` (lines 400–421), named for both roles by its `SimulatableOwnerTraits` specialisation (lines 423–428) and asserted at line 433. Ours is og-sim-net's `SessionChannel` (`SimNetChannel.h`) |
| Lifecycle | `registerSimulatable` / `unregisterSimulatable`, and the manager's two notifications | the rig's `join` and `leave` (lines 782–800): register, `notifyCharacterRegistered`, the slot occupancy; and the reverse |
| Stepping | `SimulationStepDriver::runTick` with the four `StepHooks`; or og-tickdriver's driver with a `TickDriverGame`, an engine, a world host and a presenter | the rig's `ProbeHooks` (lines 735–743), its `ProbeDriver` (line 755) and the `driver.runTick(++physicsStep)` loop (line 822); og-simulation-tests' tick-driver fakes for the other shape |
| The clock, on a predicting peer | `recordAuthorityTick` and `noteSimClockSample` (or `noteEngineRttSample`) on the network time estimator | not needed by the rig; ours is `SessionClock` |
| The manager's API | the knob setters, the clocks' and the estimator's accessors, the two notifications, the post-simulation send | the rig calls a handful (`setResimTriggerPolicy`, the notifications) |

**The worked example is `JoltRollbackSuiteRig.h`** (og-simulation-tests, `Source/OGSimulationTests/Jolt/`): a session over the
core in one header, with no og-sim-net and no og-net. Read its owner (`ProbeOwner`: callbacks it stores and a no-op
`sendLocalInputToAuthority`), its composition (`DriverRig`), its lifecycle (`join`, `leave`) and its loop (`runTick`), in that
order. What it cannot change without replacing core parts is the netcode model itself: `SimulationManager` takes net sync, input
resolution and reconciliation as template parameters and has a role flag (that design's §3.1, "The model").

---

## 3. Build

The libraries build without Unreal through CMake (og-session-runtime tasks 22b and 27b). Each folder's `CMakeLists.txt` works
inside a parent build or on its own (top level: C++20, `OG_STANDALONE_BUILD`, testing on); `Plugins/OGSimulation/standalone/`
builds them all from one working tree (`standalone/README.md`).

| Target | Kind | Links (public) | Public flags |
|---|---|---|---|
| `og_simulation` | static library (`OGSimulation/CMakeLists.txt`) | `glm::glm-header-only` | `/fp:precise` (MSVC), `/fp:precise /clang:-ffp-contract=off` (clang-cl), `-ffp-contract=off` (others); include root: the folder above `OGSimulation/` |
| `og_tickdriver` | static library (`og-tickdriver/CMakeLists.txt`) | `og_simulation` | the same floating-point flags; include root: the `og-tickdriver/` folder |
| `og_sim_net` | interface library (header-only; `og-sim-net/CMakeLists.txt`) | `og_simulation`, `og_net` | the same floating-point flags; C++20 |
| `og_sim_session` | interface library (header-only; `og-sim-session/CMakeLists.txt`) | `og_simulation`, `og_tickdriver`, `og_sim_net` | the same floating-point flags; C++20 |

- **Finding the others.** A folder built on its own is pointed at its dependencies by cache paths: og-sim-net by
  `OG_SIMULATION_DIR` and `OG_NET_DIR`, og-sim-session by `OG_SIM_NET_DIR` (which takes og-simulation and og-net the same way)
  and optionally `OG_SIMULATION_JOLT_DIR`; og-tickdriver is found beside og-simulation. Inside a parent build an existing target
  is used instead.
- **Core only.** `standalone/` with `-DOG_STANDALONE_CORE_ONLY=ON` builds og-simulation, og-tickdriver, og-simulation-jolt and
  their tests, and adds neither og-net, og-sim-net nor og-sim-session.
- **The Jolt defines come from og-simulation-jolt's `OGSimulationJolt/CMakeLists.txt`,** on its `og_jolt_physics` target as
  PUBLIC definitions, so every target that links Jolt gets them: `JPH_OBJECT_LAYER_BITS=16` and `JPH_CROSS_PLATFORM_DETERMINISTIC`
  always; `JPH_USE_SSE4_1` and `JPH_USE_SSE4_2` on x86-64 (with `-msse4.1 -msse4.2 -mpopcnt` outside MSVC); `JPH_ENABLE_ASSERTS`
  only with the option `OGJOLT_ENABLE_ASSERTS` (it changes `JPH_VERSION_ID`). The Unreal build sets the same first two, and the
  SSE pair, in `OGSimulationJolt.Build.cs`. `og_simulation_jolt` links `og_simulation`, `og_tickdriver` and `og_jolt_physics`.
  og-sim-session links no Jolt target itself; a host that runs Jolt worlds (the tests, a headless host) links `og_simulation_jolt`.
- **The header checks.** Each of the four folders adds an object library, `<target>_header_check`, with one translation unit per
  header that includes only that header, compiled without RTTI and exceptions (`/GR- /EHs-c-` with `_HAS_EXCEPTIONS=0`, or
  `-fno-rtti -fno-exceptions`), as the Unreal modules compile. A header that needs an include it does not name fails there.
- **The tests.** With `OGNET_CATCH2_DIR` (Catch2 v3's amalgamated files) and `OG_SIMULATION_TESTS_DIR` set, og-sim-net and
  og-sim-session each build a test executable (`og_sim_net_tests`, `og_sim_session_tests`) and register its `[@og]` cases
  with CTest: one entry per case when the Catch2 directory holds `Catch.cmake` (`catch_discover_tests`, labelled with the
  library's name), else one entry that runs them all. In a standalone build `OG_CHECK` (og-simulation's `OGAssert.h`) is the C `assert`, so a Release
  configuration (`NDEBUG`) compiles it out, unless the target defines `OG_CHECKS_LIVE`: then a failed check prints and exits
  with code 3 whatever `NDEBUG` says (og-sim-session's `og_sim_session_headless_tests` does, through the CMake option
  `OG_SIM_SESSION_LIVE_CHECKS`, ON by default); the Unreal build's is `checkf`.
- **The Unreal modules** are the same folders under UBT: `OGSimulation`, `OGTickDriver` (task 27b), `OGSimulationNet` and
  `OGSimulationSession` (each module's Build.cs file outside its folder), with `FPSemantics` set to precise for the simulation modules.
