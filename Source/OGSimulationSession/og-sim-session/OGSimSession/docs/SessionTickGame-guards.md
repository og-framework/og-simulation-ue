<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionTickGame.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionTickGame.h`. Ids are opaque, stable, and retired
rather than reused. The reasoning lives in `SessionTickGame-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

What the compiler or a check holds is not here: the hooks' shape (og-simulation's `TickDriverGame` and its four refining
concepts, asserted at the end of the header), the host's shape (`SessionTickHost`, checked where the class is named, and
`SessionTickComponents`, a `static_assert` in the doors' thread-check helper), each door's thread (its first statement,
`SessionThreadChecks::checkOp`, an `OG_CHECK` against `SessionThreads.h`'s table), and that the net hooks and the display exist
when a door reaches them (an `OG_CHECK` each). The order of the calls inside each door is pinned by
`SessionTickGameOrderTest.cpp`, which compares it with a transcription of the Unreal code it replaced; that test also fails on
G-01's count.

---

## G-01 — the authority's release passes a step count of 1

**Tag site:** `SessionTickGame.h`, directly above `netHooks().releaseForTick(static_cast<int32_t>(authorityTick), 1, stepSeconds);`
in `releaseInputsForAuthorityTick`.

**The prohibition.** ⛔ The tick game releases the delayed inputs for `authorityTick` with a step count of **1**. Do not pass a
larger count, a frame's step count, or a tick other than the one the hook was given.

**The consequence.** og-simulation's authority `TickDriverStepHooks::beforeTick` calls this hook once per step, immediately
before that tick is simulated, with the tick the step driver will simulate (`TickDriverStepHooks-guards.md` G-01). A count above 1
releases the inputs of later ticks into the queue early, which is the batch pattern the per-step drain replaced
(og-simulationscheduler-withjolt design D4); another tick releases for the wrong one. The count is also printed on the server's
`[ReleaseBatch] first=… numSteps=…` line (LogOGNet, Verbose); no check reads the count itself.

**What breaks if the tag moves.** `SessionTickGameOrderTest.cpp`'s authority wake: its expected entries are
`log [ReleaseBatch] first=201 numSteps=1` and `server.releaseForStep first=201 steps=1 …` (and `first=202` for the second
step), so a count other than 1 fails it (seen at the task 26 review: the count 1 → 2 fails entries 6, 7, 13 and 14). The concept
fixes only the hook's signature (one `SimTick`, no count), not the literal the tick game passes on.

*Moved from `BrawlerSessionHooksUImpl-guards.md` G-01 (retired there) by og-session-runtime task 26, with the release call: the
brawler's binding forwarded `releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1)` to the Unreal session until then.
R0 on the tree: `TickDriverStepHooks::beforeTick` calls `releaseInputsForAuthorityTick(*upcoming.authorityTick)` once per
authority step, after its owner-thread and tick checks; `SessionNetHooks::releaseForTick` passes the count to
`SimNetServer::releaseForStep(firstTick, numSteps, stepSeconds)` unchanged.*

---

## §R — Retired ids

None.
