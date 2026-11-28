<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimLaneEndpoint.h` — guards

Every prohibition that governs a line of `SimLaneEndpoint.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn` sits exactly where the forbidden edit would be typed.

**If this file and the sources disagree, the sources are authoritative and this file is stale.** Fix this file; do not soften the
sources to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to a "Retired" section and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `SimLaneEndpoint-rationale.md`.

---

## G-01 — the endpoint is never copied or moved

**Tag site:** `SimLaneEndpoint.h`, the line above the deleted copy constructor.

**The prohibition.** Do not delete these four `= delete` declarations or replace them with defaulted ones.

**The consequence.** The endpoint is kept by address: og-sim-net's `SimNetTransport::installHandlers` stores a pointer to the
endpoint it is given, and sends through it from then on. A transport handed a temporary copy sends through a destroyed object.
(Until og-session-runtime task 15 the detached endpoint's context was also its own address, so a copy's operations counted
into the original and dangled once it was gone; that endpoint is retired.)

**What breaks if it moves.** Nothing at compile time, and no test: a copy works until the original goes away.
