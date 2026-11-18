<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UELatencyBudgetDiagnostics.h` — guards

Prohibitions with a tagged site in `UELatencyBudgetDiagnostics.h`. Ids are opaque, stable, and retired rather than
reused. The reasoning lives in `UELatencyBudgetDiagnostics-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Two properties are checked by the compiler, not here: the const access to production state (the `static_assert` beside
`m_owner`, rationale §4) and provider presence (the `checkf` in `wrapInputProvider`, in both states of the gate,
rationale §3).

---

## G-01 — DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION

**Site:** `std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;`, the first member of the block of data members
that exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1.

**The prohibition.** ⛔ Every data member of this class sits in this one block, and nothing in production reads any of
them. Do not add a member here that a production decision, a production log line or a replicated value reads, do not
add an accessor that hands one of them to production code, and do not move a production value into this block. A value
that both feeds a decision and a latency line stays production, outside this class (`DiagnosticsConventions.md` §4:
the marker names the channel, never the fact).

**The consequence.** The block is compiled out of Shipping (`(!UE_BUILD_SHIPPING)`) and out of the fence build. A
production reader of one of these members either fails to compile there or, if it was itself gated to compile, makes
Shipping behave differently from Development, which is exactly what the fence test (rationale §5) exists to rule out.

**What breaks if the tag moves.** Nothing mechanical: the gate makes a Shipping or fence build fail to compile on an
ungated reader, but an edit inside this class can still hand a member out. The fence test's harness sessions are the
behavioural check.

*Moved from the game module's latency class (its G-01, og-tickdriver task 18) by task 19a, with the block.*

## G-02 — the input-sample source is resolved once, at wrap, outside the lambda

**Site:** `typename TraitsT::InputSampleSource sampleSource = TraitsT::Game::inputSampleSource(owner);` in `wrap`, the
statement before the lambda.

**The prohibition.** ⛔ Do not move this statement into the lambda. The source is resolved here, once per
registration, on the game thread; the lambda captures the result and only calls
`TraitsT::Game::inputSampledSeconds(sampleSource)` per provider call, on the step's thread.

**The consequence.** A resolution inside the lambda reads the component on the step's thread on every step: a new
game-to-step-thread read of a game object that the threads table (rationale §6) does not have, and the lambda would
have to capture the component, a lifetime edge from the core's container to it. The output would not change, so no
harness session would show it.

**What breaks if the tag moves.** Nothing mechanical; the two-phase shape is rationale §2. (Added by task 19a, from
the design review's finding that a single per-call hook would not be statement-exact.)

## G-04 — a resimulation's use stamps `Healed` only, never `Consumed`

**Site:** `post_Internal(latencyBudget::Event::stampHealedIfPending(id, read.appliedCaptureTick, stepStartSeconds));`
in `postRelayedReads_Internal`, the branch for an observation of an earlier tick that changed since the last step.

**The prohibition.** ⛔ Do not post a `Consumed` stamp from this branch, and do not replace this statement with a plain
`Event::stamp` of `Consumed` or of `Healed`. An observation of an earlier tick that changed since the last step was
written by a resimulation's read; it ends `H6L` and never `H6`.

**The consequence.** `H6` is the prediction-only hop: its end is a prediction step's scheduled read (rationale §6).
A `Consumed` stamp from a resimulation redefines `H6`, which ruling Q7 = A (og-input-latency, 2026-11-11) rejected,
because it breaks the comparison of every `H6` number with the archived ones. A plain stamp of `Healed` would
create slots for captures the probe never saw received (`noStart` on `H6L`) and count every re-served capture as a
duplicate in `[LatencyBudget.Window]`.

**What breaks if the tag moves.** Nothing mechanical. The probe's own pending semantics are
`LatencyBudgetProbe-rationale.md` §10; this site's place among the stamp points is rationale §11.

## G-05 — the read shadow is the step thread's; `forgetCharacter` never touches it

**Site:** the blank line after `m_lastHandedCorrectionTick.erase(id);` at the end of `forgetCharacter` (an absence
tag).

**The prohibition.** ⛔ Do not erase, clear or otherwise touch `m_seenReads_Physics` here, or anywhere else on the
game thread, however natural it looks beside the erase above it.

**The consequence.** `forgetCharacter` runs on the game thread. `m_seenReads_Physics` is read and written only by
`postRelayedReads_Internal`, on the step's thread, with no lock and no mailbox. An erase here races a client step
that is inserting into or iterating the same `std::unordered_map`: a crash on a rehash, not a wrong number. The
map's size is bounded on the step's thread instead (rationale §11).

**What breaks if the tag moves.** Nothing mechanical; the threads table is rationale §6.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-03 — RETIRED by og-input-latency task P5-5 (2026-11-17): moved to `UESimulationSession-guards.md` G-29

**Was:** *"the server loop skips a component whose binding is not bound"*. Its tag sat in `UELatencyBudgetDiagnostics.h`,
above `if (!component->isNetSyncBound()) continue;` in `tick_GameThread`'s server loop.

**Why it retired.** Its subject moved. Task P5-5 moved the loop's walk of the session's delayed-input map (the null check,
this skip, the correction-buffer read and the skip of tick 0) into the session's `forEachAuthorityCorrectionTick`, which
`tick_GameThread` now calls with the per-id `Handed` stamp. The entry moved with its text, re-worded only for the new site.
**Successor:** `UESimulationSession-guards.md` G-29.
