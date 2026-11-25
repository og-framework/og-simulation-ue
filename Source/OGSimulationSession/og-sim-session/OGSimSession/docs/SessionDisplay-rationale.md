<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: AuthorityPresentationEntry -- the Unreal session's nested struct until og-session-runtime task 25, now SessionDisplay::PresentationEntry; kept in section 1's history -->
<!-- lint-external-ref: AuthorityPresentation -- the Unreal session's nested struct until og-session-runtime task 25, now SessionDisplay::Presentation; kept in section 1's history -->
# `SessionDisplay.h` — rationale

`SessionDisplay.h` holds og-sim-session's `ogsession::SessionDisplay`: what a session's frame thread draws from. On a client
that is the display snapshot (`SimulationDisplaySnapshot.h`), published after each step batch and held by the frame from pass
to pass, with the frame's own copies of the visualization states; on an authority it is the game's authority-only presentation
(the brawler's ring-out score), published by the runner and applied on the frame. It is the docs pointer target of
`SessionDisplay.h`; the prohibitions are in `SessionDisplay-guards.md`.

**If this file and `SessionDisplay.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Where it came from (og-session-runtime task 25)

Design §3.4 names the component: "the display snapshot and the authority's presentation". Until task 25 it was four member
functions, two nested structs and four data members of og-simulation-unreal's `UESimulationSession`
(`publishDisplaySnapshot_Step`, `refreshDisplayView_GameThread`, `publishAuthorityPresentation_Runner`,
`presentAuthority_GameThread`; `AuthorityPresentationEntry`, `AuthorityPresentation`; the publisher, the viz-state array, the
held snapshot and the presentation's triple buffer). The task moved them in two steps (TeamDescription W6):

- **(a), the move** (OGSimulation `5d8ca34`): the four definitions and the two structs cut from `UESimulationSession.h` and
  pasted unchanged into this header, which the Unreal session then included in place of the code; 171 of 191 changed lines
  moved. In the same step `SimulationDisplaySnapshot.h`, its two docs and its test moved unchanged from og-simulation and
  og-simulation-tests into og-sim-session (og-simulation `1587f7d`, og-simulation-tests `dc328f8`): the session is the
  snapshot's only user.
- **(b), the adaptation:** the class `SessionDisplay<GameT, SnapshotT, ManagerT, StorageT, ResolutionT, ReconciliationT>` with the
  data members, doors named by operation, each first checked against the thread table, two ports as concepts, and the Unreal
  session's display accessors and hooks as forwarders.

## §2 The class, its ports and its doors

The class owns the display publisher (`SimulationDisplayPublisher<SnapshotT>`), the frame's array of visualization states
(one per entry, index-aligned with the held snapshot's entries), the held snapshot and the presentation's `TripleBuffer`. It
keeps references to what the client's fill reads: the manager (`SessionDisplayManager`: a const `getClientClock` and
`getNetworkEstimator`, for the snapshot's clock reading), the storage, the input resolution, the reconciliation, and the
clock's display-tier buffer (`SessionClock::displayTier()`, which the tier receipt writes, `SessionClock-rationale.md` §4).
The game is a template argument for its presentation slice (`SessionPresentationGame`: a type `AuthorityPresentationSlice`
and a static `authorityPresentationSlice_Step(systems, id)` returning it). Each concept has a reference shape and one negative
per requirement (`sessionDisplaySelfCheck`).

| door | thread (`SessionOp`) | what it does |
|---|---|---|
| `publishAfterBatch()` | step (`Publish`), a client only | fills a snapshot from the sources and publishes it (§3) |
| `acquireForFrame(overlay)` | frame (`AcquireDisplay`) | swaps the held snapshot for the newest, copies each entry's state and calls `overlay(id, state)` on the copy (§4) |
| `releaseHeld()` | frame (`AcquireDisplay`) | releases the held snapshot; the Unreal session calls it in `endPlay` |
| `snapshot()`, `findEntry(id)`, `findVizState(id)`, `forEachVizState(f)` | frame (`AcquireDisplay`) | answer from the held snapshot and the copies, or a null pointer (nothing) before the first acquire that found one |
| `publishPresentation(systems)` | runner (`Publish`), an authority only | one entry per stored simulatable, from the game's slice hook, then publish (§5) |
| `presentLatest(present)` | frame (`AcquireDisplay`) | `present(id, slice)` for every entry of the newest presentation, if one was published (§5) |

Every door's first statement is `SessionThreadChecks::checkOp` with its row; the readers check too, so a game reader on
another thread fails at once instead of racing the next acquire. The thread table had every row these doors need
(`SessionThreads-rationale.md` §3: `Publish` is "publish the display or the presentation", and the authority's
`AcquireDisplay` is where it applies the presentation), so the task added none. The two publishes each check their role with
an `OG_CHECK` after the thread: `Publish` is the step's on both roles, and a client must not publish a presentation nor an
authority a display snapshot.

## §3 The client's publish: after the step batch, on the thread that ran it

The tick game's `publishForDisplay()` calls `publishAfterBatch` (og-sim-session's `SessionTickGame` since og-session-runtime
task 26; the brawler binding called the Unreal session's `publishDisplaySnapshot_Step`, which forwarded, until then). og-simulation's driver calls it once after each client batch, on the thread that ran the batch, under the
world lock and inside a step scope (`SimulationTickDriver.h`, `publishAfterBatch`; `TickDriverConcepts-rationale.md` §11), and
never on an authority. Nothing steps there, so the fill (`fillSimulationDisplaySnapshot`) reads the storage, the input
resolution, the reconciliation and the clock as the batch left them. og-sim-net drains on the same thread, so the relay stores,
the arrival rings, the tier receipt and the registrations are written there too, and the fill reads every source on its
writer's thread. The tier comes from the clock's display-tier buffer: the newest published reading, or a default one before
the first.

The role check replaces the Unreal session's `checkf` on its predicting-session test (the manager exists and runs prediction).
The display exists only with a manager, and its role is the session's (both come from the same world net mode), so the two
tests are the same.

## §4 The frame's view: one snapshot held, its states copied and overlaid

`acquireForFrame`, in the Unreal session's post-physics pass (`refreshDisplayView_GameThread`), swaps the held snapshot for the
newest through the publisher's `refreshView_GameThread(held)`, which releases the held one first and then acquires the newest
(`SimulationDisplaySnapshot-rationale.md` §5). The order is the channel's read rule: while the reader holds a snapshot, a peek
of the newest returns the held one, never a later commit (`SessionDisplay-guards.md` G-01). Until og-input-latency task P6-1 the
Unreal session acquired first and released the old pointer only if it differed, so the peek returned the held snapshot,
nothing was released, and every client's view stayed on the first snapshot it acquired, published before any character
registered: the brawler found no entry and drew no aim indicator, attack or projectile visualization. Re-acquiring a slot just
released marks it held again (the channel's hold is one bit per slot), so the display holds one snapshot between passes and
none between the two calls. A publish may run in that instant, on the step's thread: the writer's open slot is not listed, so
the peek returns the newest committed snapshot. Every reader in the frame shares the one held snapshot until the next pass.

Then, for each entry, the display copies the entry's state into its own array and calls `overlay(id, state)` on the copy. The
Unreal session passes its backend's `overlayRenderPoses_GameThread`, which writes the presenter's rendered poses into every
non-root body state (`TickDriverBackendUImpl-rationale.md`). The snapshot itself stays as published: it is shared with the
writer's channel, and the entry is `const`, so the overlay cannot reach it.

## §5 The authority's presentation

The game's authority-only presentation (the brawler's ring-out score: its score system is authority-only and its replicated
score property is the scoreboard's source) travels in a `TripleBuffer` of `Presentation`s: `entryCount` and up to
`kMaxEntries` entries (the display snapshot's `kMaxSimulatables`), each a sim id and the game's slice. The runner fills one in
`publishPresentation(systems)`, which the Unreal session's `publishAuthorityPresentation_Runner` calls as the last statement of
its after-batch hook: one entry per stored simulatable, from `authorityPresentationSlice_Step(systems, id)`, then `publish()`.
The frame applies the newest in `presentLatest(present)`; the Unreal session's `presentAuthority_GameThread` passes the game's
`presentAuthoritySlice(session, id, slice)`, which needs the Unreal session, so its half of the hook pair stays a
`static_assert` there (`UESimulationSession-rationale.md` §21.3).

More stored simulatables than `kMaxEntries` fails an `OG_CHECK`, the Unreal session's `checkf` before. After the check the
fill returns instead of writing past the array, as `fillSimulationDisplaySnapshot` does: a build that compiles the check out
(Unreal's Shipping) showed only the first `kMaxEntries`, where it wrote out of bounds before (og-session-runtime task 25).

## §6 Heap and destruction

The publisher's channel holds four snapshots, each up to `kMaxSimulatables` entries with both relayed rings by value, so the
display holds it, the visualization copies and the presentation buffer through `std::unique_ptr`s and stays a few pointers
large. The Unreal session holds one `std::optional<SessionDisplay>`, built beside the clock in `beginPlay` on either role and
declared after the clock, so it is destroyed before the clock (whose display-tier buffer it reads) and before the storage, the
two peers and the manager; a `static_assert` in `UESimulationSession.h` holds that order. Before the first `beginPlay` the
Unreal accessors answer as before the first publish.

## §7 The tests

`SessionDisplayTest.cpp`, tagged `[Session][SessionDisplay]`, over a mock simulatable, og-simulation's storage, input
resolution and reconciliation, a fake manager with a real prediction clock and estimator, and a fake game. A client rig takes
the test's thread as the frame and runs the step on a worker inside a step scope; an authority rig configures its context on
the test's thread (its runner) and runs the frame on a worker. The cases: nothing is visible before the first publish (client
and authority); the frame sees the batch's state (entries, the overlaid copies, the clock and the tier); the newest publish
wins (several publishes before an acquire, an acquire with no publish, a release); a publish on one thread is acquired on
another while it runs (2,000 publishes against a reader checking that each snapshot is whole and that the sequence never goes
back); the presentation is the runner's newest, applied on the frame; it holds as many entries as the snapshot. Three hidden
cases, `[.][SessionDisplayDeath]`, are run by name: a presentation over its entry count, a client's publish on its frame thread,
and an authority's display publish, each trips its check.
