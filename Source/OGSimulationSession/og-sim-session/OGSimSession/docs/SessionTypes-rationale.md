<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SessionTypes.h` — rationale

`SessionTypes.h` holds the session runtime's plain data: the role, what an engine adapter hands the runtime at start, what
it hands it per entity to register, and what comes back. It is the docs pointer target of `SessionTypes.h`. The header
states no prohibition (its two rules are `static_assert`s), so there is no guards file.

**If this file and `SessionTypes.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What it is for

og-session-runtime design §3.3 lists the ports an engine adapter provides, and §3.4 what the runtime owns. The types here
are the parts of both that are data rather than behaviour, so that the ports (`SessionEngine.h`, `SessionGame.h`) and the
thread table (`SessionThreads.h`) can name them. og-session-runtime task 22 added them with no caller: today's Unreal session
(`UESimulationSession.h`) keeps its own types until the tasks that move its responsibilities here (tasks 23 to 31).

## §2 `SessionRole`

`Authority` or `Prediction`: the two roles a world's session can have. The design calls them "authority" and "client";
og-session-runtime task 22 names the client by what it does, as og-simulation does in `ClientPredictionClock` and
`PredictionSyncedBufferOwnerConcept`.

## §3 `EntityKey`

The engine's handle of an entity whose bodies and registration the runtime tracks, opaque to the runtime: a `std::uintptr_t`
in a struct, compared with `==`. The Unreal adapter will use the pawn's address (design §3.6). It is a struct, not a bare
integer, so a key cannot be passed where a sim id is expected.

## §4 `SessionStart`

What the adapter hands the runtime's start (design §3.3, the SessionStart row), as plain values:

| member | today, in the Unreal session |
|---|---|
| `role` | the world's net mode, mapped to a role |
| `stepSeconds` | the physics settings' fixed step, `AsyncFixedTimeStepSize` |
| `requestedMaxCatchUpSteps` | `UETickDriverHost::requestedMaxCatchUpSteps`, the tick driver's `AuthorityLoopConfig` field of the same name |
| `authorityIdleSlackSeconds` | `readAuthorityIdleSlackSeconds` (`UEAuthorityRunner.h`) |
| `highResolutionWait` | og-net's endpoint, `hasHighResolutionWait` |
| `staticWorld`, `physicsOnlyShapes` | the static import's `description` and `physicsOnly` (`UEStaticGeometryImporter.h`), which the engine traits split into the world's two static sets |

The design also names the world host's options (gravity, contact counts, mapped categories, the backend's log function).
They are the engine traits' own types, so they join `SessionStart` with the composition (task 31), not before.

## §5 `RegisterRequest` and `RegisterStatus`

`RegisterRequest` carries the entity facts of design §3.3: the sim id, the entity key, a debug name, the owner's connection
(og-simulation's `SimConnectionId`, `kInvalidSimConnection` when none), the player's local index and whether the engine
says the entity is locally controlled (`engineSaysLocal`, which the client compares with og-sim-net's roster).
`RegisterStatus` is what `tryRegister` returns each frame: `Pending`, `Ready` or `Refused`, today's `TryRegisterStatus`.

## §6 `LifecycleOutcome`

What the owner thread posts and the frame thread collects, one alternative per kind (design §3.4, the lifecycle queue):
`Registered{simId, slot}`, `Refused{simId, reason}`, `Unregistered{simId}`, `RosterRemoved{simId}` and
`BodiesReleasable{entityKey}`. A `std::variant` rather than today's kind enum with shared fields: each kind carries exactly its
own fields, so a reader cannot take a slot from an outcome that has none.

- **`RegisterRefusal` has one reason, `SimNetRefused`:** the only refusal posted as an outcome is og-sim-net's drain
  refusing a queued registration, which it reports to the `applied` hook as a null channel for two reasons it does not
  pass on: no channel for the sim id, or the id already registered (`SimNetClient::applyRegister`; its
  `[SimNet.Lifecycle] … refused=noChannel|registered` line names which). The prediction lifecycle posts it
  (`SessionLifecycle::onRegisterApplied`, `SessionLifecycle-rationale.md` §4). The other refusals (no session, a roster
  disagreement) are decided on the frame thread and returned by `tryRegister` directly. A reason is added when a refusal
  of another kind moves here. Its name was NoChannel until og-session-runtime task 29, which found the second reason.
- **Two `static_assert`s.** The variant has five alternatives, one per kind the frame thread handles; adding a kind fails
  the build until this list and its collector are revisited. And an outcome is trivially copyable, so posting and
  collecting it copies bytes, with no allocation on the owner thread.
