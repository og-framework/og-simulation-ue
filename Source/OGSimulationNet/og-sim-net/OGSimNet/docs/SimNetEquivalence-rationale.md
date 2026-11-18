<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetFrameLockedEquivalenceTest.cpp` and `SimNetTestRig.h` — rationale

These two test files hold og-sim-net's engine-free equivalence suite. It runs one server session (`SimNetServer.h`) and two
client sessions (`SimNetClient.h`) over og-net's endpoints and `VirtualNetwork.h`, frame-locked, and compares every frame
with a reference that has no transport. This file records what the suite proves, how its rig and its reference are built,
what it compares, and what it measured. The test files carry no comments; this is their only rationale, and there is no
guards file (they state no prohibition).

**If this file and the test files disagree, the test files are authoritative and this file is stale.**

---

## §1 What the suite proves, and what it does not

- **The lossless arm** (`SimNetEquivalence.TheLosslessFrameLockedRigEqualsTheNoTransportReferenceFrameByFrame`): with the
  frame hold on (`SimNetConfig`'s `immediate` false), 600 frames of 1 to 3 ticks each, three characters, correction rotation
  K = 1 and a redundancy depth of 3 give, frame by frame and bit for bit, the same observables (§5) over og-net as the
  reference (§4).
- **The loss arm** (`SimNetEquivalence.UnderB0LossTheShortfallPerStreamIsPrinted`): the same script with 1 % of the
  clients' inbound and 2 % of their outbound datagrams dropped. og-net does not re-send a lost unreliable message, so the
  transport rig delivers fewer corrections, rings, clock ticks and input bundles than the lossless reference. The arm prints
  that shortfall per stream (§6) and asserts only that no older correction is injected after a newer one and that every
  stream still delivers.
- **Not proved here:**
  - immediate mode. A send that leaves before `flushFrame` reaches its peer one frame earlier, so immediate mode is a
    deviation by design, not an equivalence; §3 explains why the rig can see it, and §7 shows that it does;
  - reordering and duplicates, which the stale filter handles (`SimNetClientTest.cpp` covers them);
  - the engine's own timing: when the engine's receive and flush points run, which is the host's binding of `dispatchFrame`
    and `flushFrame`.

## §2 The peers

- **One server and two clients** (A and B), each with its own `SimulationObjectStorage`, `SimulationReconciliation`,
  `SimulationInputResolution` and `SimulationNetSync`, all logging into one trace per peer (`SimStack` in the rig).
- **Three characters:** sim 1 (`a0`) and sim 2 (`a1`) are client A's local players 0 and 1; sim 3 (`b0`) is client B's
  local player 0. Each client predicts its own characters from a local input provider and the other client's from relay
  rings. The server's host reports a fixed RTT per connection, 30 ms for A and 90 ms for B, so the two connections sit in
  different delay tiers.
- **The simulatable is the rig's own `EqSim`** (a one-field state and a two-field input), not the mocks of
  `MockSimulatablesForNetworkTests.h`. Those mocks (`MockSimA`, `MockSimB`) declare only an `Input` alias, with no state type
  and no state accessors, so `SimulationNetSync` cannot register them. The client session also accepts a game with exactly
  one simulatable type (`kSimulatableCount`).
- **One owner-traits specialization serves both rigs.** `SimulatableOwnerTraits` for `EqSim` names one channel type,
  `SessionChannel<EqSim, Transport::ChannelTransport>` (`EqChannel`), as both the prediction and the authority owner. A
  `static_assert` checks that both sessions' `Channel` types are that type, so the reference's channels and the sessions'
  channels are one type in both roles.

## §3 One frame

The frame runs four phases. `k` is the frame's tick count (`ticksInFrame`, 1 to 3), and virtual time advances by `k` / 60 s
first.

1. **Each client:** service its endpoint, `dispatchFrame`, then `k` ticks (`allocateFrontierSlotsAll`, `collectInputAll`, the
   state step, `postPredictionAll`), then `sendLocalInputToAuthorityAll` once and `endInputSends`, then service.
2. **The server:** service, `dispatchFrame` (with the reception tick set to the next tick to step), then per tick
   `setAuthorityGuardContext`, `releaseForStep` for that tick, `collectInputAll` and the state step; then `afterBatch` with
   the next tick, `sendCorrectionAll` once, `publishClock` with the last tick stepped, then service.
3. **Each client:** `flushFrame`, then service.
4. **The server:** `flushFrame`, then service.

The order inside each peer is the Unreal host's: inputs are released per authority tick before it is stepped, the reception
coordinator is reaped after the batch, the post-simulation hook writes the corrections and sends the local input, and the
clock is published after it.

**Why the phases are staggered.** A client's hooks (phase 1) run before the server's dispatch of the same frame (phase 2),
and the server's hooks before the clients' flush (phase 3). So a send that leaves before its peer's `flushFrame` is drained
one frame early, and the comparison sees it. If every peer dispatched at the start of the frame, a send made mid-frame would
still be drained at the next frame's dispatch, and the hold would be invisible.

**Servicing.** A peer is serviced with `serviceUntil` at the current virtual time, repeated while a pass hands over at least
one datagram (at most 8 passes). A send made in a frame therefore reaches the other peer's inbox in that frame's service
passes and is drained at that peer's next `dispatchFrame`.

**Setup** runs before frame 0 in 1 ms passes of dispatch and flush with no hooks: the two clients connect and attach, the
server adds the three characters (`addCharacter`, then the server's NetSync registers each channel through
`registerSimulatable`'s authority overload, with the channel as both the prediction and the authority owner, as the Unreal
server registers its characters), and each client registers its NetSync prediction owner when the roster Add arrives. Every trace and line buffer is then
cleared, so frame 0 starts with no traffic in flight.

## §4 The reference

The reference (`ReferenceRig`) is the same script with no og-net, no session and no envelope. It models the Unreal path the
sessions replace: each payload a peer flushes is handed straight to its receivers' arrival doors at their next dispatch, as
replication sends at the engine's flush and applies at the next receive.

- **The server** calls a `ServerReceptionCoordinator` over `SimNetConnectionHandle` directly, as the engine binding does:
  per received bundle, `noteRttSample` and then `receiveInputBundle`, with the reference itself as the delivery and relay
  sink; `releaseDelayedInputs` per tick; `reapConnections` after the batch. It keeps one `RelayRingStager` per character.
  At its flush it hands every client but a character's owner that character's ring when the stager published one (with
  `relayResendEntries`), every client the correction of every channel whose buffer was written this frame (`takeDirty`),
  and every client the clock when its tick changed.
- **A client** stores each ring with `storeRing`, logs today's `[ReceiveCorrectionState] id= tick=` line and stores each
  correction with `storeCorrection`, and hands each clock tick to the host's `onTimingInfo`. Its input bundles are the bytes
  each channel built, taken from the channel's `setOnInputHandedCallback` callback. An empty bundle is dropped, as the client
  session drops it before the set. The bundles reach the server's next dispatch in client order, A then B, the order in
  which the transport rig's clients flush.
- **The reference's channels** are the sessions' channel type, each with a `SimNetBundleTransport` over the adapter of a
  `SimNetTransport` that is never installed and has no server connection, so nothing in them reaches a wire. The reference's
  server registers them as the transport rig's server does, through the authority overload with the channel as both owners.
- **The reference uses the transport rig's og-net connection ids**, so its handles and every line that prints a connection
  carry the same values.
- **The stagers' setup flushes are replayed.** Every `RelayRingStager` flush counts toward its report window
  (`kFlushReportRounds`, 120 flushes), whether or not it publishes, and the server session flushes its stagers once per
  setup pass after the characters are added. The reference replays that count (`stagerSetupFlushes`) before frame 0, so
  both rigs' `[RelayFlush]` window lines fall on the same frame. Measured without the replay: 10 frames differ, each by the
  three stagers' window lines.

## §5 What is compared, frame by frame

| field | what it holds |
|---|---|
| `server.decisions` | every line of the reception coordinator and the stagers, in order (`[ServerReceive]`, `[Park]`, `[Release]`, `[RelayFlush]` and the rest), without the session's own `[SimNet.Window]` and `[Roster.*]` lines |
| `server.trace` | the server's NetSync, reconciliation and resolution lines, in order: `[ReceiveLocalInput]` for each move NetSync queues, the queue reads, `[SendCorrectionStateToClients]` |
| `server.consumed` | per tick and character, the move `collectInputAll` took and the capture tick `getLastUsedCaptureTick` reports |
| `server.bundles` | the input bundles the server received, per character |
| `corrections`, `rings` | per client, each correction and ring that arrived: sim id and body bytes, in arrival order within the stream |
| `clock` | per client, the clock ticks the host was given |
| `correctionBuffers`, `ringStores` | per client, every channel's correction buffer and relay store after the frame |
| `lines` | per client, the host's log lines (`[ReceiveCorrectionState]`), without `[SimNet.Window]` |
| `trace` | per client, the NetSync, reconciliation and resolution lines, sorted (below) |
| `inputs` | per client, the input every character was predicted with, per tick |

- **The server's queued moves.** `SimulationInputResolution` has no reader for a character's remote move queue. The queue's
  contents over time are pinned by what goes in (each `[ReceiveLocalInput]` line, written when NetSync calls
  `queueRemoteMove`) and what comes out (`server.consumed`).
- **The order across streams inside one frame is not compared.** og-net's lane scheduler serves Control (priority 0), then
  SimClock, SimInput and SimRelay (priority 1), then SimState (priority 2) (`kDefaultLaneTable`), so a frame's sends arrive
  in that order whatever order a session made them in. The client trace is sorted for the same reason. A client gets at most
  one ring and one correction per character per frame, so sorting hides no order within a stream. Measured: swapping
  `flushRelays` and `flushCorrections` in the server's `flushFrame` leaves all 600 frames equal (§7).
- **Diagnostics.** A difference is counted per field and frame; the failure message gives the number of differing frames,
  the first difference, and each field's count and first frame.

## §6 The loss arm and its shortfall

- **The shape** is B0's: each client's backend is an `EmulatedBackend` that drops 2 % of its outbound (up) and 1 % of its
  inbound (down) datagrams, seeded `0xB0` plus the client's index. It is configured before the endpoint starts, because
  `EmulatedBackend` refuses `configure` once it is open. The server's backend and the reference are lossless.
- **One datagram per direction per frame.** og-net packs a frame's sends to one peer into one datagram here (each client's
  emulator was offered 603 datagrams each way, for 600 frames and the setup). So a lost down datagram loses that frame's
  clock tick, corrections and rings together, and a lost up datagram loses that client's whole input set.
- **Measured** (the seed above; the Unreal test build and a standalone MSVC build give the same counts):

| stream | per client and character | shortfall |
|---|---|---|
| clock | A, B | 6 of 599 each (1.00 %) |
| correction | A: sims 1, 2, 3 | 2, 1, 3 of 200, 200, 199 |
| correction | B: sims 1, 2, 3 | 1, 1, 4 of 200, 200, 199 |
| input | sims 1, 2 (A) | 15 of 599 each (2.50 %) |
| input | sim 3 (B) | 9 of 599 (1.50 %) |
| relay | to A: sim 3 | 15 of 598 (2.51 %) |
| relay | to B: sims 1, 2 | 21 of 598 each (3.51 %) |

  The emulators lost 15 (A) and 9 (B) up, and 6 down each. The clock and corrections lose exactly the down losses (6 per
  client). A ring is lost on a down loss and also when its owner's input set was lost upstream, since the server then
  stages nothing for it that frame: the rings to B of A's characters are short by A's 15 up losses plus B's 6 down losses.
  The same counts were measured again after the server's characters were registered through the authority overload with
  one channel type for both owners (og-input-latency task P5-1), and the lossless arm stayed equal for all 600 frames.
- **This is the known deviation** of og-net against replication: a lost correction is replaced at that character's next
  rotation turn (every 3 frames at N = 3, K = 1), and a lost ring's entries are gone unless `relayResendEntries` is above 0.
  The arm records its size; whether it is acceptable is decided on the engine captures.

## §7 What each comparison is known to catch

Each poison below was applied to a scratch copy of the named header (or of the test's `kHeldSends`), with one replacement
counted, and the lossless arm run:

| poison | first difference | fields that differ |
|---|---|---|
| the server's `flushFrame` sends corrections before relays, and the hold is dropped (`kHeldSends` immediate) | frame 0, `server.decisions`: the clients' first input sets are drained a frame early | all 600 frames: every server field, and every client field but `clock` and `lines` |
| the swap alone | none | none: all 600 frames equal (§5) |
| the hold dropped alone | frame 0, `server.decisions` | as the combined poison |
| relays also sent to the owner | frame 2, client A's `rings` (sims 1, 2, 3 against 3) | `rings`, `ringStores` of both clients |
| sim 2's corrections never sent | frame 2, client A's `corrections` | `corrections`, `correctionBuffers`, `lines`, `trace` |
| the clock published one tick late | frame 1, client A's `clock` (101 against 100) | `clock` of both clients |
| inputs released one tick early | frame 3, `server.decisions` | `server.decisions`, `server.trace`, `server.consumed`, then the corrections |
| the client does not store sim 2's corrections | frame 2, client A's `correctionBuffers` | `correctionBuffers`, `trace` |
| the client drops sim 3's rings | frame 2, client A's `ringStores` | `ringStores`, `trace`, `inputs` |
