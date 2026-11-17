<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetChannel.h` — rationale

The narrative behind `ogsimnet::SimNetChannel<SimT, TransportT>`, the per-character owner adapter. The header keeps
its licence, its docs pointer, its code and four tags; everything that explains it lives here. Prohibitions are in
`SimNetChannel-guards.md`.

**If this file and `SimNetChannel.h` disagree, the header is authoritative and this file is stale.** Fix this file; do
not soften the header to match it.

⚠ **og-sim-net ships under MPL-2.0, engine-free.** `SimNetChannel.h` includes og-simulation headers and the C++
standard library only, and names no og-net type. The Unreal owner it stands in for is named below as provenance;
nothing here needs it.

---

## §1 What it is

og-simulation's `SimulationNetSync` reaches a character's network traffic through two owner concepts in
`SimulationNetSync.h`: `PredictionSyncedBufferOwnerConcept` (the correction buffer and its arrival callback, the
relayed-input ring and its arrival callback, and the input send) and `AuthoritySyncedBufferOwnerConcept` (the
correction buffer the authority writes, and the remote-move callback). A game names the owner type for its simulatable
in a `SimulatableOwnerTraits` specialization, and NetSync's registration facades (`registerSimulatable`,
`unregisterSimulatable`) bind and unbind it.

`SimNetChannel` satisfies both concepts, so one channel object serves a character in both roles. It holds the
character's three payloads as og-simulation's engine-free buffers (`SimPayloadBuffers.h`), and it hands its input
bundles to a transport port. It is the og-sim-net counterpart of the Unreal owner, `UESimulatableOwnerBinding` with its
sibling component `USimulatableNetSyncComponent` (both in `OGSimulationUnreal`), whose members it mirrors with the same
semantics.

**NetSync is not edited.** The rotation gate, the applied-capture read, both arrival callbacks and their telemetry run
unchanged, because the channel plugs into the concepts they were written against.

**Nothing calls it yet.** A game switches to it by pointing its `SimulatableOwnerTraits` at a channel type. Until then
it is reached only by the tests (§9).

## §2 Template parameters, and the transport

```
template <class SimT, class TransportT> requires ClientSimTransport<TransportT> class SimNetChannel
```

- `SimT` is the simulatable. The channel takes `StateType` and `InputType` from it, as NetSync does.
- `SyncedCorrectionBufferType` is `CorrectionStateBuffer` and `RelayedInputRingType` is `RelayRingBuffer`, the two names
  the prediction concept requires.
- `TransportT` is the port the channel hands its bundles to, constrained by og-simulation's `ClientSimTransport`
  (`SimTransport.h`). **The channel holds it by value.** The intended argument is a small, copyable per-character
  handle onto the session's transport: it knows the character's sim id, and its `sendInputBundle` appends
  `(simId, bundle)` to the connection's pending bundle set. The handle's `endInputSends()` closes the set, but the channel
  never calls it: a send point covers every local character, so closing the set is the session's job.
- **The bytes the channel hands over are the input bundle codec's bytes, unchanged.** The port's `sendInputBundle`
  takes one bundle and no sim id, and the bundle set's framing (count, then sim id and bundle per entry) belongs to the
  layer that builds the set. So the sim id travels in the handle, not in the bytes.

## §3 The callbacks

There are four set/clear pairs, each holding a `std::function`. The first three types are the ones the concepts
require:

| pair | called by | type |
|---|---|---|
| `setOnCorrectionStateReceivedCallback` / `clear…` | `storeCorrection` (§6) | `void(const CorrectionStateBuffer&)` |
| `setOnRelayedInputReceivedCallback` / `clear…` | `storeRing` (§6) | `void(const RelayRingBuffer&)` |
| `setOnRemoteMoveReceivedCallback` / `clear…` | `deliverDelayedRemoteInput` | `void(std::uint32_t, const InputType&)` |
| `setOnInputHandedCallback` / `clear…` | `sendLocalInputToAuthority` (§5) | `void(const InputBundleBuffer&)` |

NetSync binds the first three at registration and clears them at unregistration. The fourth is not part of either
concept. It is for the host: the Unreal owner stamps every slot of a sent bundle as handed to the transport (its
latency budget's Handed point) right after the send, and the callback gives a host the same bundle at the same point.

Every caller tests the callback before calling it, so an unbound callback drops the delivery, as the Unreal owner's
callbacks do.

**Setting the relay callback does not replay the current ring (G-03).** The Unreal component does replay: when its
relay host is linked, setting the callback calls it at once with the host's ring. That replay covers a host that
replicated, and dropped its notifications, before the link existed. A channel owns its ring and has no link to wait
for. A ring stored before registration is read at registration by NetSync's own bind-time read, which stays out of the
relay arrival probe on purpose. A replay would add a probe sample that is not an arrival.

**What stays with the session or host.** The Unreal component does more around each arrival than the concepts need,
and those parts do not move into the channel:
- the correction's wire-format version latch (an Error line and a toast, then every later correction ignored);
- the `[ReceiveCorrectionState]` line and the correction's latency stamp;
- the relayed ring's latency stamps;
- the owner-skip check, which raises an error when a ring arrives for a character with a local input provider.

The session that decodes a message knows the envelope, the roster and the host, so it runs those before it calls the
channel's inbound door.

## §4 Dirty: written means send

`isDirty()` is true from a call of the mutable `getSyncedCorrectionStateBuffer()` until `takeDirty()`, which returns
the flag and clears it.

`SimulationNetSync::sendCorrectionAll` publishes a correction as `w.owner->getSyncedCorrectionStateBuffer().write(...)`
on the authority writer, so a character NetSync's rotation gate passes over is never touched and stays clean. A flush
that sends the dirty channels therefore sends exactly what NetSync wrote, as a replication system sends only dirty
properties. The mark is on the accessor, not on `write`, because the buffer type is og-simulation's and knows nothing
of channels (G-01).

- The const accessor does not mark. Readers, the tests included, use `std::as_const`.
- `storeCorrection` writes the member directly, so a received correction never marks the channel (G-02).
- The mark does not look at the bytes. A write of the same state at the same tick is still dirty: NetSync wrote it.

## §5 Sending input

`sendLocalInputToAuthority(queue, currentTick, redundancyDepth)` follows the Unreal owner's send step by step:
1. `InputBundleBuffer::reset()` empties the bundle the channel reuses, so no slot of an earlier send survives;
2. the depth is narrowed to `std::uint8_t`, then clamped to `InputBundleBuffer::kMaxSlots` (G-04);
3. `PendingInputQueue::forEachRecent(currentTick, depth, ...)` visits the most recent pending entries up to
   `currentTick`, oldest first, and each becomes one `appendSlot`;
4. the bundle's bytes go to the transport's `sendInputBundle`;
5. the handed callback, if set, gets the bundle.

The Unreal owner does steps 1 to 3 in `buildRedundancyBundle` (into its replicated bundle struct), step 4 as its
unreliable send, and step 5 as its Handed stamps.

- **NetSync's slot count decides the depth.** `sendLocalInputToAuthorityAll` passes `redundancySlots(...)`, then
  releases all but that many queue entries. The channel builds the bundle from the count it is given and changes
  nothing in the queue.
- **An empty bundle is still handed over.** With a depth of 0, or no pending entry at or below `currentTick`, the
  bundle holds no bytes, and the transport still gets a 0-byte span, as the Unreal owner still sends its empty bundle.
  (The Unreal server drops an empty bundle on receipt.) Whether an empty bundle goes on the wire is the transport's
  call.
- **A duplicate capture tick in one bundle** is the codec's business: `appendSlot` checks it. `forEachRecent` never
  yields one, because the queue holds distinct ticks.
- **The bundle is a member** (1,058 B of capacity, `InputBundleBuffer::kMaxWireBytes`), so a send allocates nothing.

## §6 The inbound doors

The sessions call two doors when a message for the character arrives:
- `storeRing(span)` copies a relayed-input ring into the channel's ring, then calls the relay callback with it;
- `storeCorrection(span)` copies a correction payload into the channel's correction buffer, then calls the correction
  callback with it.

The bytes are the payload codec's own: the ring codec's bytes, and the correction buffer's used prefix (tick, applied
capture tick, state composite), which is what `CorrectionStateBuffer::payload()` returns on the sending side. The
envelope's kind, sim id and correction version are the session's.

Each door returns `false`, copies nothing, and calls nothing when the bytes do not fit the buffer: above
`RelayRingBuffer::kMaxWireBytes` (1,066 B) or `CorrectionStateBuffer::kBufferBytes` (384 B). The channel's buffer then
still holds the last accepted payload. The return is `[[nodiscard]]`, so a session cannot ignore a refusal without
saying so.

A store with no callback bound keeps the bytes. That is how a ring that arrives before registration is still read at
registration (§3).

`deliverDelayedRemoteInput(captureTick, input)` is the authority's door: the server session calls it for each input
the reception coordinator releases, and it calls the remote-move callback, which NetSync bound to its by-id
remote-move queue. After unregistration the callback is cleared, so a late delivery is dropped.

## §7 Pinned in place

The channel has no copy or move: all four are deleted. NetSync's owner maps keep a raw pointer to a registered owner
(`AuthorityWriter` and `LocalInputSender` each hold one `owner` pointer), so a channel that moved while registered would
leave NetSync calling a dead object. A session keeps its channels where they cannot move, for example behind a
`std::unique_ptr`, as the tests do.

NetSync's rule that its writer structs stay one pointer in size (`SimulationNetSync.h`, asserted in the test files)
holds over a channel owner. `SimNetChannelTest.cpp` asserts it for a channel-owned simulatable.

## §8 Sim id and debug name

`simId()` is the character's sim id, the one-byte id the envelope carries (`SimEnvelope.h`). The constructor checks it
is not 0 with an `OG_CHECK`, since the envelope refuses sim id 0 (`DecodeRefusal::SimIdZero`). The channel itself never
puts the sim id on the wire (§2). It is there for the sessions, to route an inbound message and to frame an outbound
correction.

`debugName()` is a label for the sessions' log lines, set at construction and never read by the channel.

## §9 The tests

`tests/SimNetChannelTest.cpp` (og-simulation-tests target, tag `[NetTransport]`):
- `static_assert`s that a channel satisfies both owner concepts for a test simulatable, that its buffer types are the
  two og-simulation buffers, that it cannot be copied or moved, and that NetSync's writer structs stay one pointer over
  it. The test simulatable has a one-field state composite and a two-field input, the shape of og-simulation-tests'
  own network mocks plus the `StateType` the concepts need.
- **The NetSync sequence case** (`SimNetChannel.ANetSyncScriptGivesTheMockOwnersSequence`) runs one script twice
  through real `SimulationNetSync` instances (a server and a client), with the free `registerSimulatable` and
  `unregisterSimulatable` facades:
  - once with mock owners, a recording `MockPredictionOwner` and `MockAuthorityOwner` modelled on og-brawler-tests'
    NetSync mocks;
  - once with channels.

  The script has a local character (7) and a remote one (9). It stores a ring before registration and runs six ticks of
  sends, remote moves, rotated corrections (K = 1) and rings. Then it unregisters 9 on the client and delivers to it
  again, and it unregisters 7 on the server and delivers a late remote move. The trace is every NetSync, reconciliation
  and input-resolution log line of both peers, plus one line for each bundle sent (its slots) and each correction written
  (tick, applied capture tick, state). The case checks the two traces are equal, and checks the expected sends, writes,
  injects, remote moves and relay ingests are in the mock trace, so that two empty traces cannot pass.
- unit cases for the dirty flag, the inbound doors (refusal, cleared callbacks, no replay on set) and the send (slot
  window, clamp, empty bundle, the handed callback after the send).

`SimNetChannelBrawlerTest.cpp` (og-brawler-tests, tags `[WireFormat][SimulationNetSync]`) checks, for the brawler
simulatable, that the bytes the channel sends equal those of a line-for-line copy of `buildRedundancyBundle` over a test
buffer, across depths 0 to 259 and three send ticks. The Unreal builder itself cannot be called from that target.

Seen failing: with G-01's mark deleted, the sequence case fails at line 4 of the trace, where the mock owners record
the first correction (`wire write id=7 tick=100 ...`) and the channels record nothing. The traces are 40 and 64 lines
long. The dirty-flag case fails on `isDirty()` and `takeDirty()`. Each guard's own poison was also run, and each
failed where `SimNetChannel-guards.md` says: G-02 (the inbound correction written through the mutable accessor) fails
both dirty checks on a received correction; G-03 (a replay on set) puts `[RelayProbe.Ingest] id=9 newest=99 new=3` at
the head of the channel trace (65 lines against 64); G-04 (the clamp before the narrowing) fails the brawler byte pin
at depths 256 and 259, on both send ticks that have entries.

## Provenance

Written with the header on 2026-11-15. The Unreal members it mirrors were read in this tree:
`UESimulatableOwnerBinding.h` (the class, and its `sendLocalInputToAuthority` definition),
`InputRedundancyBundleBuilder.h` (`buildRedundancyBundle`), and `SimulatableNetSyncComponent.h` and
`SimulatableNetSyncComponent.cpp` (the callbacks, the ring replay on set and link, the correction version latch, the
owner-skip check and the empty-bundle drop).
