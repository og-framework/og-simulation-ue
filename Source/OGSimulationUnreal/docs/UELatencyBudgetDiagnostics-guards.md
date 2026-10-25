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

## G-03 — the server loop skips a component whose binding is not bound

**Site:** `if (!component->isNetSyncBound()) continue;` in `tick_GameThread`'s server loop, the statement after the
null check on the weak pointer.

**The prohibition.** ⛔ Keep this skip, and keep it before the correction-buffer read. Do not fold it into a getter
that returns some other buffer, and do not replace it by erasing the map entry from this class: the map is the session's
own production state (the session owner's until og-ue-simulationcomponentandmanagersplit task 26a), which this class
reads through a const reference only (rationale §4).

**The consequence.** A game component's `const` `getSyncedCorrectionStateBuffer()` forwards to its
`USimulatableNetSyncComponent` through the binding's sibling pointer, which the component's teardown clears. Without
the skip, a component that is in the map, still valid as a weak pointer and already unbound would be dereferenced
through a null pointer on the game thread of a Development server.

**What breaks if the tag moves.** Nothing in any shape that runs today: the owner erases the map entry in its
unregistration, which the component's teardown runs before it unbinds, and a component reaches the map only after it
is bound. The skip keeps the read safe if a later change reorders those (rationale §4). It changes no log line: every
component the loop reads today is bound. (Added by og-ue-simulationcomponentandmanagersplit task 21, a lead ruling
of that task.)

---

## §R — Retired ids

None.
