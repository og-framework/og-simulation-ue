<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: isNetSyncBound -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
# `UELatencyBudgetDiagnostics.h` — guards

Prohibitions with a tagged site in `UELatencyBudgetDiagnostics.h`. Ids are opaque, stable, and retired rather than
reused. The reasoning lives in `UELatencyBudgetDiagnostics-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

**Since og-session-runtime task 37 the header has no tagged site.** Its four guards moved with their sites into og-sim-session's
`SessionLatencyDiagnostics.h`, whose guards are `OGSimulationSession/og-sim-session/OGSimSession/docs/SessionLatencyDiagnostics-guards.md`;
each is retired below with its successor. What the compiler checks moved with them: the const access to production state (the
`static_assert` beside the engine-free class's `m_owner`) and provider presence (the `OG_CHECK` in each arm's
`wrapInputProvider`).

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-session-runtime task 37 (2026-11-26): moved to `SessionLatencyDiagnostics-guards.md` G-01

**Was:** *"DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION"*. Its tag sat on `m_probe`, the first member of the block of data
members that existed only while `OG_LATENCY_BUDGET_DIAGNOSTICS` was 1.

**Why it retired.** Its site moved: the block (`m_probe` … `m_seenReads_Physics`) is the engine-free class's feature-arm state
since task 37; the Unreal class keeps only `m_postTickFlushHandle`, which no production code reads either. The entry moved with
its text, re-worded for the arms. **Successor:** `SessionLatencyDiagnostics-guards.md` G-01 (og-sim-session).

### G-02 — RETIRED by og-session-runtime task 37 (2026-11-26): moved to `SessionLatencyDiagnostics-guards.md` G-02

**Was:** *"the input-sample source is resolved once, at wrap, outside the lambda"*. Its tag sat on the
`TraitsT::Game::inputSampleSource(owner)` statement in `wrap`.

**Why it retired.** Its site moved: `wrap` and its lambda are the engine-free class's since task 37 (the source is still resolved
through the traits; the class names no Unreal type). **Successor:** `SessionLatencyDiagnostics-guards.md` G-02 (og-sim-session).

### G-03 — RETIRED by og-input-latency task P5-5 (2026-11-17): moved to `UESimulationSession-guards.md` G-29

**Was:** *"the server loop skips a component whose binding is not bound"*. Its tag sat in `UELatencyBudgetDiagnostics.h`,
above `if (!component->isNetSyncBound()) continue;` in `tick_GameThread`'s server loop.

**Why it retired.** Its subject moved. Task P5-5 moved the loop's walk of the session's delayed-input map (the null check,
this skip, the correction-buffer read and the skip of tick 0) into the session's correction-tick walk (deleted by
og-session-runtime task 14), which
`tick_GameThread` now calls with the per-id `Handed` stamp. The entry moved with its text, re-worded only for the new site.
**Successor:** `UESimulationSession-guards.md` G-29.

### G-04 — RETIRED by og-session-runtime task 37 (2026-11-26): moved to `SessionLatencyDiagnostics-guards.md` G-03

**Was:** *"a resimulation's use stamps `Healed` only, never `Consumed`"*. Its tag sat on the `stampHealedIfPending` post in
`postRelayedReads_Internal`.

**Why it retired.** Its site moved with `postRelayedReads_Internal` (task 37 (a) and (b)). **Successor:**
`SessionLatencyDiagnostics-guards.md` G-03 (og-sim-session).

### G-05 — RETIRED by og-session-runtime task 37 (2026-11-26): moved to `SessionLatencyDiagnostics-guards.md` G-04

**Was:** *"the read shadow is the step thread's; `forgetCharacter` never touches it"*. Its tag sat on the blank line after the
lane forget in `forgetCharacter`.

**Why it retired.** Its site moved with `forgetCharacter` (task 37 (a) and (b)); `forgetCharacter`'s thread is its
`LatencyForget` row since. **Successor:** `SessionLatencyDiagnostics-guards.md` G-04 (og-sim-session).
