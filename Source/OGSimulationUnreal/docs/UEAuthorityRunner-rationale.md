<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEAuthorityRunner.h` — rationale

`UEAuthorityRunner.h` and `UEAuthorityRunner.cpp` are the Unreal host's half of og-simulation's authority loop (the tick driver's
`FreeRunning` mode, og-input-latency P11): the two console variables that select and tune it, the dedicated runner thread
`OGSimAuthority` with its start latch, and the runner host whose `sleepUntil` the driver falls back to. Added by og-input-latency
task P5-19. The files state no prohibition, so they have no guards doc; the start and stop order is enforced at run time
(below) and by the callers' guards (`UESimNetLanes-guards.md` G-03).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The two console variables

| variable | default | read | meaning |
|---|---|---|---|
| `og.Sim.AuthorityLoop` | 1 | once, by the server's og-net sim lanes in `ioMode` (`UOGNetDriver::InitBase`) | 1: a Jolt dedicated server steps its authority on `OGSimAuthority` and services og-net there; 0: it steps inline on the game thread |
| `og.Sim.AuthorityIdleSlackMs` | 0.5 on Win64, 0 elsewhere | once, by the world session when it starts the runner | the driver's `idleSlackSeconds`: it idles to `deadline − slack`, then polls og-net to the deadline |

- **The defaults** are Ph4's, set by og-input-latency task P5-22 after Ph4's measured gate passed (task P5-34,
  its rework 2) and before the final gate, whose PIE may veto them. Until then both were 0. The loop is refused, with one Warning, where
  it cannot run (`UESimNetLanes-rationale.md` §12): Chaos, a listen server, and a world with no og-net driver. An in-editor PIE
  server is refused as a listen server is (`RefusedNotDedicated`: `IsRunningDedicatedServer()` is false in an editor process
  started without `-server`), so only a dedicated server process runs the loop. 0 keeps Ph3's inline server, the attribution arm.
- **Read once.** The loop's choice is latched by the lanes before the endpoint starts (og-net fixes its servicing thread at the
  first service; `UESimNetLanes-rationale.md` §12), so setting the variable later has no effect on a running driver. Set it in
  `[ConsoleVariables]` or on the command line (`-ini:Engine:[ConsoleVariables]:og.Sim.AuthorityLoop=0`).
- **The slack** is clamped at 0 from below here; the driver's constructor checks that it is finite and below one step
  (`SimulationTickDriver`'s `OG_CHECK`). The Win64 default, 0.5 ms, is Ph4's gate's (P11 Q1; lead ruling R7, from the gate's R4):
  on shaped (a) runs the mean `lateUs` p99 fell from 722 µs (slack 0, two runs) to 186 µs (slack 0.5, three runs), with `approachUs` p99
  465–469 µs and no `earlyWakes` or `idleFallbacks`. Other platforms keep 0, so the runner never polls there: the measurement is
  Win64's alone (a Linux dedicated server with the og-net driver is og-input-latency P9's, not yet built or run), and the poll is a busy loop of
  non-blocking og-net passes (og-simulation `SimulationTickDriver-rationale.md` §11), about 3 % of a core at 60 Hz, which is not spent unmeasured.
  The default is the console variable's constructor value (`PLATFORM_WINDOWS`), so `[ConsoleVariables]` or the command line still
  override it on any platform.
- Both readers use `GetValueOnGameThread`: the lanes' `ioMode` and the session's `beginPlay` run on the game thread.

## §2 The runner thread (`FUEAuthorityRunner`)

- **A dedicated `FRunnable`, one per session,** created by `start_GameThread` with `FRunnableThread::Create(this, "OGSimAuthority",
  0, TPri_AboveNormal)` (P11 Q5). Dedicated, not pooled: the thread that builds the world host is its owner thread for the rest of
  the world's life (`TickDriverThreadContext`'s owner mark), and the thread exits when the loop returns.
- **`Run()`** logs the start line (§3), runs the body, and then releases the start latch if the body has not. The body is the
  tick backend's `runAuthorityLoop` (`TickDriverBackendUImpl-rationale.md` §15); it releases the latch itself after the driver is
  built (`afterStart`). The release after the body means a body that returns early (the stop was requested before the start)
  still frees the game thread.
- **The start latch** is a manual-reset `FEvent` from the pool. `waitForStart_GameThread` blocks the game thread on it and then
  logs the wait, `[SimHost.Runner] startLatch waitMs=<ms>` at Warning on `LogOGSimHost` (P11 Q6: the BeginPlay latch wait is
  logged). The wait is the world build, the game's session configuration (`afterBuild`) and the driver's construction, all on the
  runner while the game thread is parked, so nothing the runner writes there races the game thread. `releaseStart` triggers the
  event once (an atomic exchange), so the latch cannot be released twice.
- **Stop and join** (`join_GameThread`, P11 D7's S1–S3): set the stop flag (release), `WaitForCompletion`, delete the thread. The
  loop returns at its next wake, at most one step plus the slack away. It is idempotent: the lanes' `beforeStop` and the session's
  `endPlay` may both call it. `stopRequested()` is the flag the tick driver's loop polls.
- **Run-time checks instead of comments.** The destructor `checkf`s that a started thread was joined (a runner destroyed while
  its loop runs would step a destroyed session); `start_GameThread` and `join_GameThread` `checkf` the game thread; the tick
  driver's `stop()` `OG_CHECK`s that the loop returned, so a host that stops the backend before the join aborts at once.

## §3 The start line

`[SimHost.Runner] priority=AboveNormal nice=<n|-> highResWait=<0|1>` at Warning on `LogOGSimHost`, printed by the runner itself
before the body (P11 §8):
- `priority` is the requested `TPri_AboveNormal`;
- `nice` is the Linux read-back of the runner's own nice value (`getpriority(PRIO_PROCESS, tid)`): UE maps AboveNormal to a nice
  of −10 relative to the process, which needs CAP_SYS_NICE or RLIMIT_NICE, and without them the runner stays at 0 (P11 §8.2,
  Q4). Other platforms print `-`;
- `highResWait` is og-net's `hasHighResolutionWait()` for the server endpoint, read by the session from the lanes' endpoint
  (og-input-latency task P5-18 forwards it through the driver's backend). Ph4's gate requires 1 on Win64: without the
  high-resolution waitable timer an og-net wait is late by up to a 15.6 ms timer period (P11-8 F1).

## §4 The runner host (`FUEAuthorityRunnerHost`)

The tick driver's `TickDriverRunnerHost`: one member, `sleepUntil(deadlineSeconds)`, which sleeps the remaining time on the engine
clock (`FPlatformTime::Seconds()`) with `FPlatformProcess::Sleep`. The driver calls it only when the idle hook is `Unavailable` or
after three early returns in a row (P11 D6), so its millisecond precision does not matter: it is the degraded path. The normal
idle is og-net's high-resolution wait through og-sim-net's `SimNetIdle`.

## §5 Tests

No automated test reaches the runner: it needs a world, a driver and an og-net endpoint. The smokes of og-input-latency task P5-19
are its evidence (the start lines, `lostSteps=0`, and the 200 ms game-thread hitch that leaves the runner's `maxStepsPerFrame`
at 1).
