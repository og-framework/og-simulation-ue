<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESimulationLogRoutes.h` and `.cpp` — guards

Every prohibition that governs a line of `UESimulationLogRoutes.h` or `UESimulationLogRoutes.cpp`. Each entry has an
**opaque, stable id**. In the source a single line `// ⛔G-nn  docs/UESimulationLogRoutes-guards.md` sits directly above
the statement where the forbidden edit would be typed.

**There is no live entry.** The rules the route table has — no prefix may begin with an earlier one, no prefix of one
block may begin with one of the other, every og-simulation route maps to a category, every prefix widens exactly, every
`plainLevel` is `Log` or `Verbose`, and a direct log site's tag has a row — are `static_assert`s, so a wrong edit does
not compile. Why the files are as they are lives in `UESimulationLogRoutes-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
