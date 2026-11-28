<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetDriver.h` and `.cpp` — guards

Every prohibition that governs a line of `OGNetDriver.h` or `OGNetDriver.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn` sits exactly where the forbidden edit would be typed.

**If this file and the sources disagree, the sources are authoritative and this file is stale.** Fix this file; do not soften the
sources to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to a "Retired" section and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `OGNetDriver-rationale.md`.

---

## G-01 — the reflected header includes no og-net header

**Tag site:** `OGNetDriver.h`, after the last include and before the generated header: an absence tag, with the blank line under
it where an og-net include would be typed.

**The prohibition.** Do not include an og-net header (`OGNet/...`) or one of this module's `OGNetUnreal/Core/...` headers in
`OGNetDriver.h`, and do not replace the forward-declared `FOGNetEndpointHost` with a member that needs one. The same holds for
`OGNetSimLanes.h`, which this header includes: it forward-declares `ognetunreal::SimLaneEndpoint` and includes no og-net header. og-net's types stay
in `OGNetDriver.cpp`, behind the pimpl.

**The consequence.** `OGNetDriver.h` is a public header of a module that depends on og-net privately. Any module that
includes it (the game module will, to name the driver) would then need og-net's include paths and its `OGNET_API` definition,
and the engine-lane fence would read the class as declared in a file that includes `OGNet/`, a sim-side file.

**What breaks if it moves.** Nothing in this module: it compiles either way. The first includer outside the module fails to
compile.

## G-02 — every time the core sees comes from the host's clock

**Tag site:** `OGNetDriver.cpp`, the line above `return Host.nowSeconds();` in `FOGNetEndpointHost::Now`.

**The prohibition.** Do not return any other clock here: not the engine's frame or app time, not the world's time, not the
driver's elapsed time.

**The consequence.** Two readers depend on this being the host's clock. The core's `stop` drains
while its host's clock is below the given time plus the drain (`EngineLaneDriverCore.h`, `stop`), so a time from another
clock makes the shut-down drain wait far too long or not at all. And the emulator's delay queues read `Now` through
`EmulatorClock` on og-net's own thread, which keeps running between engine frames, while an engine frame clock does not advance.

**What breaks if it moves.** No test in this module. The shut-down drain loses its bound, and emulated packets are released
by a clock that stands still between frames.

## G-03 — a held join is released only outside the frame that opened the connection

**Tag site:** `OGNetDriver.cpp`, the line above the `if` in `FOGNetEndpointHost::ReleaseHeldJoin` that keeps holding while og-net
is not up or the engine's frame counter is not past the held frame.

**The prohibition.** Do not drop or weaken the frame condition (`GFrameCounter <= HeldJoinFrame`): do not release UE's hello as
soon as og-net is up, and do not compare with `<` or with a frame other than the one InitConnect ran in.

**The consequence.** At start-up og-net is up during the client's first engine tick, which is still frame 0. A hello released
there is acked at the start of frame 1, so UE's first round-trip sample still holds the end of the engine-init frame (68 ms in
P4-12's `Ph2_c_B1` client1 log; 60-120 ms on rendered clients). That sample feeds `RoundTrip=PlayerStateAvg`'s first bucket,
and through it the prediction clock's target offset: the F10 lead error this hold removes (`OGNetDriver-rationale.md` §14).

**What breaks if it moves.** `OGNet.Unreal.Driver.HelloAfterTheConnectingFrame` fails: og-net comes up inside the test's frame,
so the hello leaves before the frame ends and the "UE packets sent in the frame that opened the connection" check reads 1. No
runtime check fires.
