<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetServer.h` — guards

Every prohibition that governs a line of `SimNetServer.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetServer-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetServer.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetServer-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

---

## G-01 — a bundle the roster does not accept goes no further

- **Site:** the `if (verdict != RosterInputVerdict::Accepted)` in `onBundle`.
- **Prohibition:** do not delete the check, and do not widen what passes it (for example, accept `UnknownSim`, or accept a
  bundle whose sender is not the character's owner).
- **Consequence:** any connection could drive any character. Its bundles would be stamped, sampled into the tier of the wire
  that sent them, parked under the victim's sim id and relayed to every other connection as that character's input. The
  park key is the sender's wire and the victim's local index, so it can also replace the claim of the sender's own character
  that has the same local index.
- **What breaks if it moves:** it must run before the stamps, the RTT sample and `receiveInputBundle`. Each of them changes
  state for the sim id it is given.

## G-02 — one RTT sample per character bundle, never per slot

- **Site:** the `noteRttSample` call in `receiveBundle`.
- **Prohibition:** do not move the call into the per-slot stamp walk above it, and do not call it once per slot anywhere else.
- **Consequence:** the tier table counts samples, not time: each sample adds to the smoothed RTT and to the dwell count. A
  bundle of three slots would count three times, so a wire's tier would change after a third of the dwell and its smoothing
  would depend on the redundancy depth. That is the coordinator's own "once per bundle, never per slot" rule; the reception
  path the session replaces samples once per bundle.

## G-03 — `remove` and `removeFor` are one statement

- **Site:** the `return` in `takeRemove`.
- **Prohibition:** do not split the two calls, and do not put another roster call, a send or a drain between them.
- **Consequence:** between `remove` and `removeFor` the roster holds a pending Remove. A `snapshotFor` in that window fails
  the roster's check (a new connection would get a Snapshot whose revision already counts a Remove it never receives), and an
  `addFor` sends an Add whose revision is newer than the Remove that follows it, which every client refuses as a revision
  gap and disconnects on.

## G-04 — a character's relay skips its owner

- **Site:** the `m_transport.relay(character.owner, …)` call in `flushRelays`.
- **Prohibition:** do not pass anything but the character's owner as the connection to skip.
- **Consequence:** with `kInvalidSimConnection` (or another id) the owner receives the relay of its own input every frame.
  The relay this replaces is never sent to its owner (the UE relay actor, until og-input-latency task P5-9 deleted it, replicated its ring with `COND_SkipOwner`), so no
  client path has ever received its own character's ring; the owner's SimRelay budget pays for it either way.

## G-05 — the flush reads a correction through the const accessor

- **Site:** the `std::as_const(channel).getSyncedCorrectionStateBuffer()` read in `flushCorrections`.
- **Prohibition:** do not read the buffer through the non-const accessor (drop the `std::as_const`).
- **Consequence:** the non-const accessor marks the channel dirty; that is how a NetSync write becomes a send. A flush that
  used it would re-mark every correction it just sent, so each one would be sent again on every later frame, to every
  connection, until the character leaves. Nothing fails: the clients drop the repeats as stale, and only the SimState budget
  shows it.

## G-06 — the return after the selection check stays

- **Site:** the `if (count >= indices.size() || …)` return in `CorrectionSelection::select`, after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** it runs only where `OG_CHECK` is compiled out: a Shipping build, and the standalone build with `NDEBUG`,
  where `OG_CHECK` is `assert`. There a policy that selects more than `kMaxCorrectionCandidates` times would write past the
  end of `indices`. Where the check is live it stops the program, or ends the case in a test runner that reports a failed
  check as a failed case, so no test can reach the return and its deletion passes every test.

## G-07 — the return after the removal check stays

- **Site:** the `if (found == m_characters.end())` return in `removeCharacter`, after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** as G-06: only a build without live checks reaches it, and there `removeCharacter` of a sim id with no
  character would dereference the end iterator of the character map. No test can reach it.

## G-08 — the skip after the selected-index check stays

- **Site:** the `if (index >= count)` `continue` in `flushCorrections`, after its `OG_CHECK`.
- **Prohibition:** do not delete the `continue`.
- **Consequence:** as G-06: only a build without live checks reaches it, and there a policy that selects an index past this
  frame's candidates would read a candidate slot that holds no correction. The envelope writer refuses its sim id 0 and
  returns size 0, so og-net would be handed an empty SimState message, and a send for sim id 0 would be recorded. No test
  can reach it.

## G-09 — the one-shape check folds over a variable template

- **Site:** the `static_assert((kSimHasShape<SimTs, Shape> && ...), …)` in the class.
- **Prohibition:** do not write the comparison inline in the fold (for example
  `(std::is_same_v<simEnvelope::ShapeFor<typename SimTs::InputType, typename SimTs::StateType>, Shape> && ...)`), and do not
  spell the alias out inside it either.
- **Consequence:** MSVC 19.38 (the UBT toolchain 14.38.33130) passes both inline forms for a pack whose shapes differ, so a
  game with two input sizes compiles and every input set from it is walked with the wrong stride for one of its types. The
  same fold over a variable template of one simulatable is evaluated, and fails. This was measured: the inline form was the
  first version of this assert, and its poison arm (a game with a 5 B and a 12 B input) compiled.

## G-10 — `addCharacter` refuses a live sim id or a live slot before it changes anything

- **Site:** the `if (refusal != AddCharacterRefusal::None)` in `addCharacter`.
- **Prohibition:** do not delete the refusal, do not move it after `m_roster.add`, the channel's creation or the Adds it sends,
  and do not replace it with an `OG_CHECK`.
- **Consequence:** the roster's own checks for a reused sim id and a held slot are `OG_CHECK`s. Where they are compiled out the
  roster adds a second record, `std::map::emplace` keeps the first channel and destroys the new one, and the pointer the host
  registers with NetSync dangles. Where they are live, a host's mistake stops the program instead of being refused. Moved after
  `m_roster.add`, the refusal comes too late: the roster has already stopped the program, or added the second record. Moved
  after the sends, every attached client would already hold an Add for a character the server then refuses.
- **What breaks if it moves:** `SimNetServer.AnAddWithALiveSimIdOrALiveSlotIsRefusedAndChangesNothing` aborts in
  `RosterAuthority::add`'s `OG_CHECK` (a sim id already used) at the first refused add.

## G-11 — `addCharacter` stops on the roster's refusal before it builds or sends anything

- **Site:** the `if (rosterRefusal != RosterAddRefusal::None)` right after `m_roster.add` in `addCharacter`.
- **Prohibition:** do not delete this refusal, do not discard `m_roster.add`'s result, and do not move the channel's creation,
  the stager or the Adds above it.
- **Consequence:** a refused add leaves the roster without the character. Past this point the session would still create its
  channel and stager and call `m_roster.addFor` for every attached connection, which finds no live entry: where `OG_CHECK` is
  live that stops the program, and where it is compiled out it reads through a null pointer. With no connection attached the
  host gets a channel for a character no client will ever be told about, and that the input check refuses as `UnknownSim`.
- **What breaks if it moves:** `SimNetServer.AnAddTheRosterRefusesAsFullOrDuplicateOwnerSlotReturnsNullAndChangesNothing`:
  the first refused add aborts in `RosterAuthority::addFor`'s `OG_CHECK` (no live entry has this sim id).

---

## G-12 — the drain applies the lifecycle queue before it reads any record

- **Site:** the `m_lifecycle.applyPending();` at the top of `drain()`.
- **Prohibition:** do not move the apply after the inbox's drain, and do not apply the queue from anywhere but `drain()`.
- **Consequence:** the records of one drain are then handled against the roster of the drain before: a bundle that arrives
  after the host unregistered its character is accepted for a character that is leaving the simulation, and a bundle for a
  character registered in the same drain is refused as an unknown sim id. Applied from another member, the facades run on that
  caller's thread, which is not the drain's once the authority loop drains on its runner (og-input-latency P2-19).
- **What breaks if it moves:** `SimNetLifecycleQueue.TheServerAppliesQueuedRegistrationsAtTheTopOfItsDrain` fails at
  `CHECK( rig.server->stats().unknownSim == 1u )` with `0 == 1`.

---

## G-13 — `removeCharacter` unregisters a queue-registered character before it destroys the channel

- **Site:** the `if (unregisterLifecycle(simId))` in `removeCharacter`.
- **Prohibition:** do not delete it, and do not move it below the channel's erase.
- **Consequence:** the channel is the character's prediction and authority owner in NetSync: the authority writer map holds a
  pointer to it and the remote-move callback is bound on it. Without the unregister the channel is destroyed while NetSync
  still holds it, so the next correction send walks a destroyed owner, and the storage and the resolver keep a character the
  roster no longer lists.
- **What breaks if it moves:** `SimNetLifecycleQueue.TheServerAppliesQueuedRegistrationsAtTheTopOfItsDrain`, its section
  "removeCharacter of a queue-registered character unregisters it before the channel goes", fails at
  `CHECK( rig.fullyGone(kRemote) )` and `removalUnregisters == 1u`.

---

## G-14 — the host hears of a queue-registered character's unregister before the facade runs

- **Site:** the `if constexpr (SimNetLifecycleUnregisteringHook<HostT>)` in `unregisterLifecycle`.
- **Prohibition:** do not delete the call, and do not move it below `unregister(*this, simId)`.
- **Consequence:** the host's game systems hold pointers into the character's storage, and the host's answer (the systems
  executor's unregister notify) needs the character still in storage. Below the facade the entry is destroyed; without the
  call a `removeCharacter` or a queued unregister of a queue-registered character leaves the systems pointing at freed storage.
  The authority loop registers every server character through the queue (og-input-latency P5-19), so this is every leave there.
- **What breaks if it moves:** `SimNetLifecycleUnregisteringHook.TheServerHookRunsBeforeTheFacadeWhileTheCharacterIsInStorage`,
  sections "removeCharacter of a queue-registered character" and "a queued unregister", fail at
  `REQUIRE( rig.host.calls.size() == 1u )` (deleted) or `CHECK( rig.host.calls[0].inStorage )` (moved below the facade).

---

## §R Retired ids

None.
