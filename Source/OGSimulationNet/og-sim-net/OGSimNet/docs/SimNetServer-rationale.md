<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: UESimulatableOwnerBinding::receiveRemoteMoveBundle -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
# `SimNetServer.h` — rationale

`SimNetServer<Game, HostT, TransportT, PolicyT>` is the authority's sim session over og-net. It drains the server's inbox,
checks each input bundle against the roster, hands it to the reception coordinator with the same arguments the engine's input
path uses, and at the frame point sends the relays, the corrections, the clock and the roster to the clients. The prohibitions
that govern its lines are in `SimNetServer-guards.md`; the host services it calls are described in `SimNetHost-rationale.md`.

**If this file and `SimNetServer.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What the session owns

- **The reception coordinator** (`ServerReceptionCoordinator<SimNetConnectionHandle, Sims...>`). The session constructs it
  from the `TimeConfig` it borrows and is its only owner; a host reaches it through `coordinator()`. Its logger is the host's
  `log`, so every coordinator line (`[ServerReceive]`, `[Park]`, `[Release]`, `[DelayShift]`, `[InputDrop]` and the rest)
  reaches the host unchanged.
- **The liveness table** (`LivenessTable`, `SimNetConnections.h`), fed only by the connection events the session drains. Every
  coordinator address is a `SimNetConnectionHandle` taken from it, so a connection id that og-net reuses after a disconnect
  reads dead under its old handle.
- **The roster authority** (`RosterAuthority`, `SimNetRoster.h`).
- **One channel per character** (`Channel<SimT>`, §2), kept per simulatable type and found by sim id, each holding the
  transport's per-character adapter (`channelTransport(simId)`) inside a `SimNetBundleTransport`.
- **One `RelayRingStager` per character**, named with the character's debug name and logging through the host.
- **The per-connection send state:** the connections that have their Snapshot, in attach order; the last tier each was sent;
  the tick of the last correction each was sent per character.

The session borrows the host, the transport and the `TimeConfig`; all three must outlive it. It hands `this` to the
coordinator's logger and to each stager, so it is neither copyable nor movable.

Nothing calls the session yet. P5's wiring binds `dispatchFrame` and `flushFrame` to the engine's receive and flush points,
`releaseForStep` and `afterBatch` to the tick driver's authority hooks, and `addCharacter` and `removeCharacter` to
registration.

## §2 The template

- **`Game`** is anything with a `Simulatables` type that is a `SimulatableList` (`SimNetGame`); a composition type such as the
  brawler's has one. The session is a partial specialization on that list, so `SimNetServer<Game, HostT, TransportT>` is all a
  host spells; the fifth parameter is the list itself and is not meant to be passed (a `static_assert` says so).
- **`HostT`** satisfies `SimNetServerHost` (`SimNetHost.h`).
- **`TransportT`** satisfies `SimNetServerTransport`: og-simulation's `ServerSimTransport` plus `inbox()` (with `drain` and
  `stats`), `flushFrame()` and `channelTransport(simId)`. `SimNetTransport` satisfies it; the tests use it over a recording
  endpoint and over og-net's `VirtualNetwork`.
- **`PolicyT`** is the correction policy (§10), `SendAllDirty` by default.
- **`Channel<SimT>` is `SessionChannel<SimT, ChannelTransport>`** (`SimNetChannel.h`), where `ChannelTransport` is the type
  the transport's `channelTransport(simId)` returns. `SimNetClient::Channel` is the same alias for the client's transport, so
  over one transport type the two sessions' channels are one type. A game's `SimulatableOwnerTraits` can then name that one
  type as both the prediction and the authority owner, and the host registers a server character the way the engine path does
  today: through `registerSimulatable`'s authority overload, with the channel as both owners. That registration also creates
  the remote-input store and binds the relay and correction callbacks on the authority, as the engine path does. A
  `static_assert` checks the alias for every simulatable; it folds over the variable template `kIsSessionChannel`, the form
  G-09 requires for the shape fold.
- **`HostT`'s optional `noteDelivered`** (§3): a `static_assert` refuses a host member of that name that the session could
  not call as `noteDelivered(SimLane, double)` (`SimNetDeliveryHookWellFormed`, `SimNetHost.h`).
- **One envelope shape for every simulatable.** An input set is walked with one bundle stride before any sim id in it is
  looked up (`simEnvelope::forEachInputBundle`), so every simulatable of the game must have the same input and state sizes. A
  `static_assert` checks it, through the variable template `kSimHasShape` (G-09 says why it is not written inline). A game
  that needs two shapes needs a set per shape, which is a wire change.
- **Two size checks:** a full relay ring of the game's input in its envelope fits one client inbox record
  (`kMaxSimMessageBytes`, 512 B), and so does a full `CorrectionStateBuffer` (384 B) in its envelope. A larger message would
  be refused as `tooLarge` by every client. The ring check is on the game's shape, because an input of up to 128 B is legal
  and its full ring is over 1 KiB. The correction check is on the constants, because the envelope's shape already caps a
  correction at the buffer's size.

## §3 The drain

`drain()` hands every record in the inbox to the session in FIFO order and returns how many it handled. `dispatchFrame()` is
the same drain, under the name of the frame point it is bound to in the frame-locked mode. The inbox's own single-consumer
guard holds; the session adds no thread check.

**The delivery hook.** When the host has `noteDelivered(SimLane, double recvSeconds)` (`SimNetDeliveryHook`), the session
calls it once for every `Message` record it drains, with the record's lane and og-net's receive time, before it decodes the
message. It is called for every message the inbox handed over, whatever the session then does with it: a malformed set, a
refused bundle and a message on the wrong lane are all delivered messages. It is never called for a `Connected` or
`Disconnected` record. A host without the member compiles unchanged and is never called; P5's Unreal host is to use it for
the sim lanes' rows of its `[OGNet.Window]` line.

- **One order, three rings.** The transport gives Control and the connection events rings of their own in the inbox
  (`SimNetTransport-rationale.md` §10, §11). The inbox hands records out in the order they were pushed across all three
  (og-simulation's `SimInbox-rationale.md` §3), so a connection's Control messages come before its messages on the other lanes
  exactly when og-net delivered them first, as with one ring, never before its `Connected`, and never after its
  `Disconnected`. A connection the event ring had no room to announce never reaches the session: neither its events nor its
  messages.
- **The quota is given back by the drain.** Each message handed out is subtracted from its connection's inbox quota by the
  inbox's own drain, after the session's callback returns. The session does nothing for it.
- **A connection closed for its Control quota is named.** The transport disconnects a connection whose undrained Control
  messages would pass its slot's quota, and pushes its `Disconnected` with the cause `ControlQuota`
  (`SimNetTransport-rationale.md` §10). The session logs that record before it detaches the connection:
  `[Warning][SimNet.Disconnect] role=server conn=<id> cause=controlQuota (more undrained Control messages than its slot's quota)`,
  and counts it (`quotaDisconnects`, in `stats()` and on the window line, §12).

| record | what the session does |
|---|---|
| `Connected` | the liveness table, then **attach**: the connection joins the attached list and is sent its roster Snapshot, then the relay floor if one was published (§11) |
| `Disconnected` | the liveness table, then **detach**: the connection leaves the attached list, and its last tier and last-sent correction ticks are forgotten |
| a message | decoded as client-to-server on its lane (`simEnvelope::decode`); anything but an `InputBundleSet` is counted as `badDecode` |

- The reception coordinator's claims for a disconnected wire are not dropped here. They go at the next reap at a dwell
  boundary (§7), when the handle reads dead, exactly as an engine connection's claims do today.
- **The set is all or nothing.** `decode` walks the set before it accepts it, and `forEachInputBundle` walks it again before
  it calls back, so a set with one bad bundle is counted once and none of its bundles reaches the roster check.
- Kinds that are legal client to server but not handled in Ph3 (the clock ping of Ph4) are refused by the decoder today, and
  are counted as `badDecode` with everything else.

## §4 One character bundle

For each `(simId, bundle)` of an accepted set, in the set's order:

1. **The roster check** (C-2; G-01). `checkInput(sender, simId)`: `UnknownSim` (no live entry, a removed character included)
   is counted as `unknownSim` and nothing is logged, because it is expected: a client's sends for a character cross that
   character's Remove on the wire. `NotOwner` is counted as `refusedNotOwner` and logged as `[Roster.InputRefused] conn= simId= reason=NotOwner`. A
   refused bundle reaches nothing else: no stamp, no RTT sample, no coordinator call.
2. **The bundle is copied** into one `InputBundleBuffer`, the codec buffer the coordinator walks. A bundle that does not fit
   it is counted as `badDecode`; the envelope's own length check already bounds it.
3. **One ServerReceived stamp per slot**, `stampServerReceived(simId, captureTick, recvSeconds)`, with the receive time the
   transport recorded for the whole message. The host turns that into its ServerReceived and NetReceived points.
4. **One RTT sample per bundle** (G-02): `noteRttSample(handle, simId, host.receptionTick(), host.rttMs(sender), *this)`.
5. **`receiveInputBundle<SimT>(simId, handle, localIndex, bundle, *this, *this)`**, with the local index from the roster
   and the session as both the delivery and the relay sink.

Steps 3 to 5 are the order of the engine's input path, `UESimulatableOwnerBinding::receiveRemoteMoveBundle`: the
ServerReceived stamps per slot, then the RTT sample, then `receiveInputBundle` with the session as both sinks. Three things
of that path are not here:

- **The fallback with no coordinator** (deliver every slot undelayed). The session always has its coordinator.
- **The diagnostics' wire sample** (`noteWireSample`). It is a host diagnostic; a host that wants it takes it in `rttMs`.
- **The engine's player slot.** The engine reads it from the child connection; the session uses the roster's local index,
  which the host set from the same value at `addCharacter`.

`SimT` is the simulatable type whose channel map holds the sim id. The session finds it by sim id, and the type then selects
the input type the bundle is decoded as.

## §5 The malformed-slot path is unreachable through the drain

The coordinator delivers a slot undelayed, through `deliverRemoteInput`, only when its player slot is out of range (over
`ConnectionSlotKey`'s `kMaxPlayerSlot`, 7). The session's player slot is the roster's local index, and a bundle reaches the
coordinator only when the roster accepted it, which needs an owner equal to the sender. `RosterAuthority::add` refuses an
owned character whose local index is not 0..7. So the drain never produces an out-of-range slot. The session still
implements `deliverRemoteInput`, because the coordinator's sink concept requires it, and it routes by sim id like the release
does. The decision-equivalence test drives that sink through the coordinator directly, with slot 8.

## §6 The sinks

- **`deliverRemoteInput(id, captureTick, input)`** and the **release callback** both deliver to the channel of that sim id
  (`deliverDelayedRemoteInput`). The release callback returns false when there is no such channel, and the coordinator then
  drops the claim, as it does for a gone engine component today.
- **`relayRemoteInput(id, captureTick, dA, input)`** stages into the character's `RelayRingStager`. Nothing is sent here; the
  stagers flush at `flushFrame` (§9).
- **`sendConnectionTierToOwningClient(id, tier)`** makes the session the coordinator's `ConnectionTierSink`. It looks up the
  character's owner in the roster and calls `publishTier(owner, tier)` (§8). An authority-local character has no owner, and
  nothing is sent.
- A sim id is a `uint8_t`; the coordinator's ids are `unsigned int`. An id of 0 or over 255 names no character and is ignored.

**`releaseForStep(firstTick, numSteps)`** drains the coordinator once per simulatable type of the game, in the list's order.
For a one-type game (the brawler) that is today's single drain. For more types, each drain walks every claim and dequeues only
its own type's queue; the drain-tick marker it sets is an assignment, and `[DelayShift]` logs only when a wire's delay
changes, so the repeated walk adds no line and changes no decision. The `[ReleaseBatch]` line stays the host's.

## §7 The reap

`afterBatch(serverTick)` is `reapConnections(serverTick)`, once per physics frame, with the frame's next tick. The coordinator
documents why it is per frame and not per step: the reap is the only feed of the receipt gate's server tick, and its dwell
gate is `serverTick % tierMinDwellTicks == 0`. At a boundary it drops every claim whose wire's handle is dead.

## §8 The tier

- `publishTier(conn, tier)` sends `ConnectionTier` on Control to that connection only, and only when the value differs from
  the last one sent to it. Two local players on one wire are two owners to the coordinator, and each publishes on its own
  tier change; the session sends the wire's value once. (Today two components write one per-wire relay property, which also
  replicates once.)
- **Tier 0 is never a first value.** A connection that was never sent a tier is at tier 0 on the client, which is the
  coordinator's baseline too ("a missing entry means never told, baseline 0"). Tier 0 after another tier is sent.
- A disconnect forgets the connection's last tier, so a reconnect under the same id starts again from "never sent".

## §9 `flushFrame()`

In this order, each step's sends held by the transport in call order (`immediate=false`):

1. **Relays.** Each character's stager flushes with `relayResendEntries`, clamped by `latencyKnobs::clampRelayResendEntries` and
   read from the borrowed `TimeConfig` on every call, so a live change takes effect at the next frame. A flush that publishes
   nothing sends nothing: the published ring is then byte-identical to the last one, as an unchanged replicated property is
   not sent again. A published ring goes as a `RelayRing` to every connection but the character's owner (`relay(owner, …)`;
   G-04); an authority-local character's owner is `kInvalidSimConnection`, which skips no one. Then one Handed stamp per entry
   of the published ring whose capture tick is not `kNoInputCaptureTick`, as the UE path's ring-flushed callback does.
2. **Corrections.** Every channel whose correction was written since the last flush (`takeDirty`) is a candidate, in sim-id
   order. The buffer is read through the const accessor (G-05). For each attached connection the policy selects from the
   candidates (§10), and each selected one goes as a `CorrectionState` (`corrVersion` = `correctionStateBuffer::kWireFormatVersion`,
   the buffer's used prefix) by `sendState`. So a correction goes to every attached connection once per write.
3. **The clock**, as a `ClockPublish` to every connection (`broadcastClock`), when the tick last published differs from the
   tick last sent. Publishing the same tick twice sends it once; two publishes between flushes send the last.
4. **The transport's `flushFrame()`**, which releases this frame's held sends in call order. It is last, or the steps above
   would leave a frame late.
5. **The window line** (§12).

**Roster messages, the floor and the tier are not part of the flush.** They are sent when they happen (attach, `addCharacter`,
`removeCharacter`, `publishFloor`, `publishTier`); the transport holds them with the frame's other sends, in call order.

**Broadcasts can precede a Snapshot.** Relays and the clock go by `broadcastExcept`, to every connection og-net has. A
connection whose `Connected` is still in the inbox gets them before the session attaches it and sends its Snapshot. A client
drops a ring for a sim id it does not know (P2-11's `unknownSim`); a clock needs no roster. Corrections go to attached
connections only.

## §10 The correction seam

- **`CorrectionCandidate {simId, ageTicks, bytes, substituted}`.** `bytes` is the size of the message as sent (envelope
  included). `ageTicks` is per receiver: the correction's tick minus the tick of the last correction of that character sent to
  that receiver, or `kNeverSentAgeTicks` if none was. `substituted` is always false in Ph3; it is a field for P3's policy.
- **`CorrectionSelection`**: the indices of the candidates to send, in the order to send them, at most one per candidate.
- **`CorrectionPolicy`**: `policy.select(receiver, candidates, budgetBytes, selection)`. The selection is an out parameter of
  a concrete type because a requires-expression cannot declare an `auto` parameter.
- **`SendAllDirty`** selects every candidate in order and ignores the budget. The session counts a receiver's frame whose
  selected bytes exceed the budget as `overBudget` and sends it in full. og-net's SimState queue age is the backstop.
- **The budget** is a constructor argument, per receiver per flush. `correctionAllowanceBytes(lanes, budget, tickFrequency)`
  computes og-net's SimState share of the connection budget per tick: with the default lane table and budget at 60 Hz it is
  1,250 B, about 3.7 brawler corrections.

## §11 The roster

- **`addCharacter<SimT>(simId, physicsSlot, ownerConn, localIndex, debugName)`** adds the roster record (`ownerConn` =
  `kInvalidSimConnection` for an authority-local character), creates the channel and the stager, sends the Add to every
  attached connection in its own view, and returns a pointer to the channel for the host to register with NetSync.
- **It fails closed** (G-10). Before anything else it refuses an add whose sim id already has a character (`SimIdLive`), or
  whose physics slot is held by a live entry (`SlotLive`; `kRosterNoSlot` never collides). A refusal returns `nullptr`,
  leaves the roster, the channels, the stagers and the sends as they were, counts `addRefused` and logs
  `[Warning][Roster.Refused] role=server simId= slot= reason=SimIdLive|SlotLive` through the host. The roster checks the
  same two things with `OG_CHECK`s; where those are compiled out, the old path added a second roster record, `emplace` kept
  the first channel and destroyed the new one, and the reference it returned dangled.
- **The roster's own refusals** (G-11, from task P5-23). `RosterAuthority::add` returns `Full` when no roster slot is free
  (`kRosterMaxEntries` characters, a pending Remove included) and `DuplicateOwnerSlot` when the owner and local index are a live
  entry's (`SimNetRoster-rationale.md` §4). `addCharacter` turns either into the same `nullptr`, the same `addRefused` count and
  the same line, `[Warning][Roster.Refused] role=server simId= slot= reason=Full|DuplicateOwnerSlot`, before it creates the
  channel or the stager or sends anything. The roster changed nothing, so nothing is undone. The `slot=` field is the one the
  host passed, `255` for `kRosterNoSlot`.
- **What it does not refuse.** A sim id that was removed is not live, so the session passes it on; the roster then refuses it
  with its own `OG_CHECK`, because an authority never reuses a sim id (`SimNetRoster-rationale.md`). A removed character's
  physics slot is free again. The roster's other preconditions (a slot or local index out of range, an owner of
  `kInvalidSimConnection`, an owner with no local index) stay its `OG_CHECK`s; admission and the host keep them out.
- The client's own `[Roster.Refused] reason=` line (`SimNetRoster.h`'s formatter, a refused roster stream) shares the tag; the
  server's carries `role=server` and the leading `[Warning]`.
- **`removeCharacter(simId, reason)`**: the stager's `finalReport` (`[RelayFlush] teardown`), then the roster's `remove` and
  `removeFor` in one statement (G-03), then the Remove to every attached connection, then the coordinator's `forgetOwner(simId)`
  (on the engine path the Unreal session's own reception forget did the same until og-input-latency task P5-7, which
  routed the game's unregistration here), then the channel, the
  stager and the per-receiver correction ticks are erased.
- The ordering duties the roster leaves to the session (`SimNetRoster-rationale.md` §6) hold: a connection gets its Snapshot
  when it attaches, before any Add or Remove to it; every attached connection gets every Add and Remove when it happens, so in
  revision order; `removeFor` is in the same statement as `remove`. The transport's hold keeps that order on the wire.
- **The floor.** `publishFloor(f)` sends `RelayFloor` to every attached connection when the value changes, the first value
  included (0 too). A connection that attaches later gets the current value right after its Snapshot.

## §12 `[SimNet.Window]`

Every 10 s of the host's `nowSeconds`, checked at the end of `flushFrame`:

`[Warning][SimNet.Window] role=server drains= msgs= maxDepth= drops= tooLarge= badDecode= refusedNotOwner= unknownSim= statesSent= relaysSent= overBudget= quotaRefused= controlHighWater= quotaDisconnects= eventsRefused= eventHighWater=`

- Every field but `controlHighWater` and `eventHighWater` counts the window only. `maxDepth` is the most records one drain
  handled; `drops` (all lanes), `tooLarge`, `quotaRefused` (all lanes: messages refused because their connection held its
  inbox quota, on Control its Control quota) and `eventsRefused` are the inbox's counters' growth over the window.
  `quotaDisconnects` is the `Disconnected` records with the cause `ControlQuota` that the window's drains handled (§3).
- `eventsRefused` counts the connection events the inbox's event ring refused for want of room. A refused `Connected` leaves
  its connection unannounced, and the transport closes it (`SimNetTransport-rationale.md` §11, the coalescing rule); a
  correct peer meets it only when more connections open in one drain gap than the ring is sized for (400 on a default server).
- `controlHighWater` and `eventHighWater` are the most records the inbox's Control ring and event ring have ever held, since
  the transport was made, not since the window began; they show how close each ring came to its size
  (`SimNetTransport-rationale.md` §10 and §11).
- The window starts at the first `flushFrame`, and the first line also counts what happened before it.
- The leading `[Warning]` is the og-simulation logger convention for the line's severity, as the `[RelayFlush]` line has:
  a dedicated server runs its net categories at Warning, so a lower line would not exist where it is needed.
- `SimNetServerStats` (`stats()`) holds the session's own counters since construction: the line's `drains`, `msgs`
  (`messages`), `badDecode`, `refusedNotOwner`, `unknownSim`, `statesSent`, `relaysSent`, `overBudget` and `quotaDisconnects`,
  plus the clocks, tiers, floors and roster messages sent, the refused adds (`addRefused`, §11) and the empty bundles its
  channels dropped (`emptyBundles`, `SimNetChannel-rationale.md` §10). The last two are not on the line. `maxDepth` is kept per window only, and the inbox's counters are
  the transport's (`inbox().stats()`).

## §13 Returns after checks

Three checks have a return (or `continue`) right after them: a policy selecting too many candidates, `removeCharacter` of an
unknown sim id, and a selected index past the candidates. They run only where `OG_CHECK` is compiled out (Shipping, and the
standalone build with `NDEBUG`), and keep those builds from writing or reading out of bounds (G-06, G-07, G-08).

## §14 Tests

`SimNetServerTest.cpp` (`[NetTransport][SimNetServer]`), with a two-simulatable mock game and a fake host:

- **Decision equivalence:** twelve frames of scripted arrivals over two connections and three characters (redundant slots,
  so duplicates; one out-of-domain capture; one late capture; one truncated set; then a slot-8 bundle through the coordinator)
  give the same coordinator log, the same deliveries, the same relay rings and the same tier per frame as a coordinator fed
  directly with the engine path's arguments.
- The roster check; the tier once per connection change, and again after a reconnect; the floor on every change, a first 0
  included; the reap at the next dwell boundary; removal; the window line; the over-budget count; each receiver's candidate
  age, with only attached connections asked; and, over `VirtualNetwork`, the Snapshot and floor before deltas, the relay to
  all but the owner, corrections once per write to every connection, and the clock once per changed tick.
- `addCharacter`'s refusal: a live sim id (of either simulatable type) and a live slot are refused, counted and logged, and
  the first channel, its stager, the roster's revision and a new connection's Snapshot are unchanged; two characters with no
  slot are both added; a removed character's slot is free again.
- The roster's refusals through `addCharacter`: a second live owner and local index, then a ninth character, each return
  `nullptr`, count, log its line (`reason=DuplicateOwnerSlot`, `reason=Full` with `slot=255`), create no channel or stager,
  send nothing and leave the roster's revision and a new connection's Snapshot unchanged; after a removal the refused sim id is
  added.
- The channel is the session channel type, and an empty bundle is counted in `emptyBundles`; a host with `noteDelivered`
  sees one call per message record (a malformed set, a refused bundle and a message on the wrong lane included) with its lane
  and receive time, and none for connection events.
- Four hidden `[.][OGSimDeath]` cases: one per `OG_CHECK` (three), and one that re-adds a removed sim id and reaches the
  roster's never-reused check.

`SimNetIsolationTest.cpp` (`[NetTransport][SimNetIsolation]`) checks the window line's inbox fields on a real transport: the
quota refusals and the Control high-water mark, a quota disconnect, and, over two windows, a refused `Connected` and the event
ring's high-water mark (the refusal counted in its own window only, the high-water mark kept).
