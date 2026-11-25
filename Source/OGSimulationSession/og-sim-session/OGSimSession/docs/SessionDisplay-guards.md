<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionDisplay.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionDisplay.h`. Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionDisplay-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

What the compiler or a check holds is not here: each door's thread (its first statement, `SessionThreadChecks::checkOp`, an
`OG_CHECK` against `SessionThreads.h`'s table), the role of the two publishes (an `OG_CHECK` each), the presentation's entry count
(an `OG_CHECK`), that the overlay writes the display's own copy and never the published snapshot (the entry is `const`), the
ports (two concepts with their negatives), and, in the Unreal adapter, the display's destruction before the members it reads (a
`static_assert` in `UESimulationSession.h`).

---

## G-01 — `acquireForFrame` swaps the held snapshot through `refreshView_GameThread`: release first, then peek

**Tag site:** `SessionDisplay.h`, directly above `m_held = m_publisher->refreshView_GameThread(m_held);` in `acquireForFrame`.

**The prohibition.** ⛔ Do not replace the call with a peek of the newest snapshot (`acquire_GameThread`) followed by a release of
the old pointer when the two differ, nor with a peek before the release in any other form. The held snapshot is released first,
and only then is the newest acquired.

**What breaks if the edit is made.** While the reader holds a snapshot, the channel's peek of the newest returns the held one
(`SnapshotChannel::peekNewest` anchors its view at the held slot, `Mailbox-rationale.md`). A peek-then-release therefore
receives the pointer it already holds, sees the two are equal, releases nothing, and keeps that snapshot for good: the frame's
view never moves again while the step keeps publishing into the other slots. That was the Unreal session's refresh from
og-input-latency task P5-16 to P6-1, and every client drew no character's visualization (`SessionDisplay-rationale.md` §4).
The display's test `SessionDisplay.TheNewestPublishWins` fails on that order, at the first publish after its first acquire (`301 == 401`: the view stays put; the publisher's own
`SimulationDisplaySnapshot.ARefreshedViewFollowsEveryPublish` pins `refreshView_GameThread`, not this call).

*R0, og-session-runtime task 25:* verified on the tree: `SnapshotChannel::viewPosition` (`Mailbox.h`) anchors a peek at the held
slot with the smallest `back`, so `peekNewest(0)` while one snapshot is held returns that snapshot;
`SimulationDisplayPublisher::refreshView_GameThread` (`SimulationDisplaySnapshot.h`) releases before it peeks.
