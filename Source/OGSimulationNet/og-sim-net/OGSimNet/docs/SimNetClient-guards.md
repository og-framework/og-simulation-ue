<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: SimNetClientChannelTransport -- a RETIRED token -- og-input-latency task P5-1 renamed it SimNetBundleTransport and moved it to SimNetChannel.h; retired G-05 names it as it stood -->
<!-- lint-external-ref: SimNetClientChannelTransport::sendInputBundle -- a RETIRED member -- the site of retired G-05, kept as it stood; the live site is SimNetBundleTransport::sendInputBundle in SimNetChannel.h -->
# `SimNetClient.h` — guards

Every prohibition that governs a line of `SimNetClient.h`. Each entry has an **opaque, stable id**. In the header a single
line `// ⛔G-nn  docs/SimNetClient-guards.md` sits exactly where the forbidden edit would be typed.

**If this file and `SimNetClient.h` disagree, the header is authoritative and this file is stale.** Fix this file; do not
soften the header to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, is moved to §R and its number is
spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn` **tag**.

⛔ **Nothing in this file is a rationale.** The narrative lives in `SimNetClient-rationale.md`. A guard is a prohibition plus
the consequence of ignoring it.

---

## G-01 — after a version mismatch, every correction is ignored

- **Site:** the `if (m_wireMismatch)` at the top of `onCorrection`.
- **Prohibition:** do not delete the check, do not move it below the version comparison or the channel lookup, and do not
  clear `m_wireMismatch` anywhere in the session.
- **Consequence:** a server whose correction layout differs from this build's would keep reaching the channels. A later
  correction that happens to carry this build's version byte would be injected with the other build's state bytes, and
  every mismatching one would call the host again, so the host's one Error line and toast would repeat on every correction.
- **What breaks if it moves:** `SimNetClient.AVersionMismatchFiresOneHostCallAndThenSilence` fails: a second
  `mismatch 16/15` host call, and corrections 22 and 24 delivered after the mismatch.

## G-02 — the stale filter runs before the log, the stamp and the store

- **Site:** the `if (!m_staleFilter.admit(message.simId, message.tick))` in `onCorrection`.
- **Prohibition:** do not delete the call, do not move it after the `[ReceiveCorrectionState]` line, the State stamp or
  `storeCorrection`, and do not replace it with a check of anything the channel or the correction cache holds.
- **Consequence:** og-net's SimState lane is plain unreliable and keeps no order between datagrams, so a correction older
  than one already injected would be injected after it, and reconciliation would rewind the character to the older state. A
  correction that reaches the session twice would be injected twice. Moved after the log or the stamp, a dropped correction would still print its line and
  stamp the latency budget as received.
- **What breaks if it moves:** `SimNetClient.NoCorrectionOlderThanOneAlreadyInjectedIsDeliveredUnderAScriptedReorder`
  fails (`{ 10, 12, 11, 12, 13, 9, 14 }` injected instead of `{ 10, 12, 13, 14 }`), and so does
  `SimNetClient.UnderTheEmulatorsDuplicateAndReorderNoOlderCorrectionIsInjected`. Moved after the log, the scripted case
  counts 8 `[ReceiveCorrectionState]` lines instead of 5.

## G-03 — a refused roster clears the transport's server connection

- **Site:** the `m_transport.setServerConnection(kInvalidSimConnection)` in `refuseRoster`.
- **Prohibition:** do not delete it, and do not make it wait for the host's disconnect.
- **Consequence:** the host disconnects after it reads `takeDisconnectRequest()`, which can be a frame later. Until then the
  client would keep sending input sets for characters whose roster it has refused, and the server would accept them as long
  as the connection is up.
- **What breaks if it moves:** `SimNetClient.ARevisionGapDisconnectsWithAReasonAndTheSessionFailsClosed` fails: the
  transport still names the server connection, and the frame's input set reaches the endpoint.

## G-04 — while refused, the session ignores every message

- **Site:** the `if (m_refused)` at the top of `onMessage`.
- **Prohibition:** do not delete the check, do not narrow it to the Control lane, and do not clear `m_refused` anywhere but on
  a new `Connected`.
- **Consequence:** after a refusal the mirror no longer matches the server's roster (P7's C-6: a roster is never partially
  applied, and the session stops trusting the stream). Rings and corrections that follow would still be routed by sim id into
  channels the server may already have removed or reassigned.
- **What breaks if it moves:** `SimNetClient.ARevisionGapDisconnectsWithAReasonAndTheSessionFailsClosed` fails: the ring and
  the correction sent after the gap are delivered (`{ r3@12, c3@12, c3@13 }`), and `ignoredWhileRefused` stays 0.

## G-06 — `flushFrame` ends the open input set before the transport flushes

- **Site:** the `m_transport.endInputSends()` in `flushFrame`'s frame-locked branch (`if (!m_immediate)`).
- **Prohibition:** do not delete it, and do not move it after `m_transport.flushFrame()`. (The branch around it is not this
  guard's: an immediate session's set is ended by `afterCaptureTick_Step` on the step's thread, and the transport's own
  `OG_CHECK` refuses a set end from `flushFrame` there, rationale §17.)
- **Consequence:** a host that sends its channels' bundles and does not call `endInputSends()` would leave the set open in the
  transport. The frame's input would not leave at this flush, and the next frame's bundles would be appended to the same set,
  so one set would carry two bundles of one character and every input would reach the server a frame late.
- **What breaks if it moves:** `SimNetClient.AFrameSendsOneHeldSetAndDropsEmptyBundles` fails (one SimInput message where
  two are expected), and so do the cases that never call `endInputSends()`: the revision-gap case (no set without a server
  is counted), the arrival-order case over og-net (4 relayed rings instead of at least 30) and the rate case (1.33 sets per
  send instead of 2).

## G-07 — a ring is held only for a sim id the roster has never listed

- **Site:** the `if (m_everListed.test(simId) || …)` in `holdRing`.
- **Prohibition:** do not delete the `m_everListed` test, and do not drop the cap on the number of held rings.
- **Consequence:** a held ring waits for its character's channel. A server never reuses a sim id, so a ring for a character
  the roster has removed would wait forever, and once `kRosterMaxEntries` such rings are held, the rings of characters that are
  still to be added are dropped. Without the cap, a server could make the client hold a ring for every sim id.
- **What breaks if it moves:** `SimNetClient.TheRosterCreatesChannelsBeforeTheHostEventAndDestroysThemAfterIt` fails: one
  held ring and one `unknownSim` where none and two are expected.

## G-08 — the return after the duplicate-channel check

- **Site:** the `if (m_channels.find(simId) != m_channels.end())` in `onRosterAdded`, right after its `OG_CHECK`.
- **Prohibition:** do not delete the return.
- **Consequence:** it runs only where `OG_CHECK` is compiled out. Without it, a second channel for the sim id would be built and
  destroyed at once by `emplace`, which keeps the existing entry, and the held ring would then be stored through a reference
  to the destroyed channel: a use after free.
- **What breaks if it moves:** nothing in a checked build, where the `OG_CHECK` stops first. The case that reaches it is the
  poison arm that keeps a removed channel (rationale §12).

## G-09 — `onClock` only latches the tick; the drain delivers the newest one

- **Site:** the `m_pendingClock = tick;` in `onClock`.
- **Prohibition:** do not call `onTimingInfo` from `onClock`, and do not deliver more than one clock tick per drain.
- **Consequence:** every clock message queued since the last drain becomes its own RTT sample and authority tick. At a join
  that is 10 to 17 samples in one frame, all with the same RTT reading (og-input-latency's Ph3 gate run, task P5-25),
  so one reading fills the estimator's sample count with that many samples (the clock's re-centre window,
  `TimeConfig::recentreSettleSamples`, counted them and filled that many samples early until og-input-latency P3-11; it
  counts the clock's advances since).
- **What breaks if it moves:** `SimNetClient.QueuedClockTicksReachTheHostAsTheNewestOnePerDrain` fails at
  `REQUIRE(s.host.timing.size() == 1u)` with `16 == 1`: one host call per queued tick where one is expected.

---

## G-10 — in immediate mode, `bindListener` and `unbindListener` only post a request

- **Site:** the `if (m_immediate)` in `bindListener`.
- **Prohibition:** do not apply the bind (the floor and tier replay) or the unbind in the caller's call when the session is
  immediate, and do not drop the request so that the next drain never applies it.
- **Consequence:** in immediate mode the drain runs on the step's thread and writes the latched floor and tier and reads the
  bound flag there. A bind applied in the caller's call runs the replay on the binding thread: in the Unreal host that is the
  game thread (`UESimNetLanes::bindClientSession`), so the host's tier-delay write (`UESimulationSession::onConnectionTierReceived`)
  and the floor write run on the game thread while a step task drains, which is the crossing immediate mode removes, and the
  plain latches race.
- **What breaks if it moves:** `SimNetClientImmediate.InImmediateModeTheNextDrainAppliesTheListenerBind` fails at its first
  `CHECK(host.calls.empty())` after `bindListener()`, and
  `SimNetClientImmediate.EveryRelayStoreCorrectionCacheAndEstimatorWriteRunsOnAStepTaskNeverOnTheGameThread` counts the floor
  and tier replays on the game thread.

---

## G-11 — the drain applies the lifecycle queue before it reads any record

- **Site:** the `m_lifecycle.applyPending();` at the top of `drain()`.
- **Prohibition:** do not move the apply after the inbox's drain, and do not apply the queue from anywhere but `drain()`.
- **Consequence:** a correction that arrives in the same drain as its character's registration reaches the channel before
  NetSync has bound its callbacks there: it is stored in the channel and never injected, so the first correction after a join is
  lost (a relay ring is read again at bind, a correction is not). Applied from another member, the facades run on that caller's
  thread: in the Unreal host the game thread, while a step reads the containers they write, which is the crossing the queue
  closes (og-sim `ThreadingCrossings.md` row 11).
- **What breaks if it moves:** `SimNetLifecycleQueue.ARegisterThenTheFirstArrivalInTheSameDrainReachesTheRegisteredCharacter`
  fails at `CHECK( rig.landings() == 1u )` with `0 == 1`.

---

## G-12 — a roster Remove unregisters a queue-registered character before anything else

- **Site:** the `if (unregisterLifecycle(simId))` in `onRosterRemoved`.
- **Prohibition:** do not delete it, and do not move it below the host's `onRosterRemoved` call or the channel's erase.
- **Consequence:** the channel is the character's owner in NetSync: the sender map holds a pointer to it and the arrival
  callbacks are bound on it. Without the unregister the channel is destroyed with the roster entry while NetSync still holds
  it, so the next send walks a destroyed owner, and the storage, the cache and the resolver's containers keep a character no
  roster lists. Below the host's call, the host is told of a removal while the character is still registered.
- **What breaks if it moves:** `SimNetLifecycleQueue.AnUnregisterThenALateArrivalIsACountedUnknownSimId`, its section "the
  roster removes the character before the unregister is applied", fails at `CHECK_FALSE( *storageHadItAtTheHostEvent )`,
  `CHECK( rig.fullyGone(kRemote) )`, `removalUnregisters == 1u` and `unregistersIgnored == 1u`.

---

## G-13 — the host hears of a queue-registered character's unregister before the facade runs

- **Site:** the `if constexpr (SimNetLifecycleUnregisteringHook<HostT>)` in `unregisterLifecycle`.
- **Prohibition:** do not delete the call, and do not move it below `m_unregisterLifecycle(*this, simId)`.
- **Consequence:** the host's game systems hold pointers into the character's storage (the brawler's hit routing keeps the
  character's root body and its `SimulatableBrawler`). The host's answer, the systems executor's unregister notify, needs the
  character still in storage. Below the facade the storage entry is already destroyed, and without the call nothing tells the
  systems at all for a roster Remove: they keep pointing at freed storage. The session's own unregistrations notify on their
  own, so only the Remove path shows the loss.
- **What breaks if it moves:** `SimNetLifecycleUnregisteringHook.TheClientHookRunsBeforeTheFacadeWhileTheCharacterIsInStorage`,
  sections "a roster Remove of a queue-registered character" and "a queued unregister", fail at
  `REQUIRE( rig.host.calls.size() == 1u )` (deleted) or `CHECK( rig.host.calls[0].inStorage )` (moved below the facade).

---

## §R Retired ids

### G-05 — RETIRED by og-input-latency task P5-1 (2026-11-17): moved to `SimNetChannel-guards.md` G-05

The empty-bundle drop left `SimNetClient.h` with its class: `SimNetClientChannelTransport` became the role-neutral
`SimNetBundleTransport` in `SimNetChannel.h`, which both sessions' channels hold. The prohibition is unchanged and lives in
`SimNetChannel-guards.md` as G-05. The entry as it stood here:

- **Site:** the `if (bundle.empty())` in `SimNetClientChannelTransport::sendInputBundle`.
- **Prohibition:** do not delete the check, and do not forward the empty span to the wrapped transport.
