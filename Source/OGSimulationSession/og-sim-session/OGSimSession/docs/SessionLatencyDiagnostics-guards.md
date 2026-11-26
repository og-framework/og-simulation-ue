<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionLatencyDiagnostics.h` — guards

Prohibitions with a tagged site in `SessionLatencyDiagnostics.h`. Ids are opaque, stable, and retired rather than reused. The
reasoning lives in `SessionLatencyDiagnostics-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Checked by the compiler or at run time, not here: the const access to production state (the `static_assert` beside `m_owner`,
rationale §6), provider presence (the `OG_CHECK` in each arm's `wrapInputProvider`, rationale §2), the port's shape (the
concept's negatives, rationale §3) and each door's thread (its `SessionOp` row, rationale §4).

All four entries moved here from `OGSimulationUnreal/docs/UELatencyBudgetDiagnostics-guards.md` with their sites
(og-session-runtime task 37); each names its old id, which is retired there.

---

## G-01 — DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION

**Site:** `std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;`, the first member of the block of the feature arm's
diagnostic state (`kEnabled` true).

**The prohibition.** ⛔ Nothing in production reads any member of this block. Do not add a member here that a production
decision, a production log line or a replicated value reads, do not add an accessor that hands one of them to production code,
and do not move a production value into this block. A value that both feeds a decision and a latency line stays production,
outside this class (og-simulation's `DiagnosticsConventions.md` §4: the marker names the channel, never the fact). The one
friend, `sessionLatencyTest::Inspector`, is the tests'.

**The consequence.** The block exists only in the feature arm; the off arm has no member (rationale §2), and Unreal picks the
off arm in Shipping and in the fence build. A production reader of one of these members either fails to compile there or, if it
was made to compile, makes Shipping behave differently from Development, which the fence test
(`UELatencyBudgetDiagnostics-rationale.md` §5) exists to rule out.

**What breaks if the tag moves.** Nothing mechanical: an edit inside this class can still hand a member out. The fence test's
sessions are the behavioural check.

*Was `UELatencyBudgetDiagnostics-guards.md` G-01 (og-tickdriver task 18; moved to OGSimulationUnreal by task 19a), retired there by
og-session-runtime task 37.*

## G-02 — the input-sample source is resolved once, at wrap, outside the lambda

**Site:** `typename TraitsT::InputSampleSource sampleSource = TraitsT::Game::inputSampleSource(owner);` in `wrap`, the statement
before the lambda.

**The prohibition.** ⛔ Do not move this statement into the lambda. The source is resolved here, once per registration, on the
frame thread (the `LatencyWrap` row); the lambda captures the result and only calls
`TraitsT::Game::inputSampledSeconds(sampleSource)` per provider call, on the step's thread (the `LatencyCapture` row).

**The consequence.** A resolution inside the lambda reads the registering component on the step's thread on every step: a new
frame-to-step read of an engine object that the session's thread table does not have, and the lambda would have to capture the
component, a lifetime edge from the core's container to it. The output would not change, so no session would show it.

**What breaks if the tag moves.** Nothing mechanical; the two-phase read is `UELatencyBudgetDiagnostics-rationale.md` §2.

*Was `UELatencyBudgetDiagnostics-guards.md` G-02 (og-ue-simulationcomponentandmanagersplit task 19a), retired there by
og-session-runtime task 37.*

## G-03 — a resimulation's use stamps `Healed` only, never `Consumed`

**Site:** `post_Internal(latencyBudget::Event::stampHealedIfPending(id, read.appliedCaptureTick, stepStartSeconds));` in
`postRelayedReads_Internal`, the branch for an observation of an earlier tick that changed since the last step.

**The prohibition.** ⛔ Do not post a `Consumed` stamp from this branch, and do not replace this statement with a plain
`Event::stamp` of `Consumed` or of `Healed`. An observation of an earlier tick that changed since the last step was written by a
resimulation's read; it ends `H6L` and never `H6`.

**The consequence.** `H6` is the prediction-only hop: its end is a prediction step's scheduled read. A `Consumed` stamp from a
resimulation redefines `H6`, which ruling Q7 = A (og-input-latency, 2026-11-11) rejected, because it breaks the comparison of
every `H6` number with the archived ones. A plain stamp of `Healed` would create slots for captures the probe never saw received
(`noStart` on `H6L`) and count every re-served capture as a duplicate in `[LatencyBudget.Window]`.

**What breaks if the tag moves.** Nothing mechanical. The probe's pending semantics are `LatencyBudgetProbe-rationale.md` §10;
this site's place among the stamp points is `UELatencyBudgetDiagnostics-rationale.md` §11.

*Was `UELatencyBudgetDiagnostics-guards.md` G-04 (og-input-latency task 6), retired there by og-session-runtime task 37.*

## G-04 — the read shadow is the step thread's; `forgetCharacter` never touches it

**Site:** the blank line after the lane forget (`m_probe->forgetLane(id);`) at the end of `forgetCharacter` (an absence tag).

**The prohibition.** ⛔ Do not erase, clear or otherwise touch `m_seenReads_Physics` here, or anywhere else on the frame thread,
however natural it looks beside the forget above it. The same holds for `m_lastHandedCorrectionTick_Runner` (og-input-latency task
P5-20): it is the authority runner's dedup map.

**The consequence.** `forgetCharacter` runs on the frame thread (its row, `LatencyForget`). `m_seenReads_Physics` is read and
written only by `postRelayedReads_Internal`, on the step's thread (the `LatencyAfterStep` row), with no lock and no mailbox. An
erase here races a client step that is inserting into or iterating the same `std::unordered_map`: a crash on a rehash, not a
wrong number. The map's size is bounded on the step's thread instead (`UELatencyBudgetDiagnostics-rationale.md` §11).
`m_lastHandedCorrectionTick_Runner` is read and written only by `stampCorrectionHanded_Internal`, on the authority runner (the
`LatencyCorrectionHanded` row), so an erase here races the runner the same way; it holds at most one entry per sim id (1..255),
and a stale entry costs at most one `Handed` stamp for a new character that reuses the id and whose first correction carries the
remembered tick (`UELatencyBudgetDiagnostics-rationale.md` §14).

**What breaks if the tag moves.** Nothing mechanical; the thread rows are rationale §4.

*Was `UELatencyBudgetDiagnostics-guards.md` G-05 (og-input-latency task 6; re-sited by og-session-runtime task 14), retired there by
og-session-runtime task 37.*

---

## §R — Retired ids

None yet. ⛔ **An id is never reused.**
