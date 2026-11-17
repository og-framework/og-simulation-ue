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

**Tag site:** `OGNetDriver.h`, after the last engine include and before the generated header: an absence tag, with the blank
line under it where an og-net include would be typed.

**The prohibition.** Do not include an og-net header (`OGNet/...`) or one of this module's `OGNetUnreal/Core/...` headers in
`OGNetDriver.h`, and do not replace the forward-declared `FOGNetEndpointHost` with a member that needs one. og-net's types stay
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
