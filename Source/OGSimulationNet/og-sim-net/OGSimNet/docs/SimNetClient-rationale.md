<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetClient.h` — rationale

`SimNetClient<Game, HostT, TransportT>` is the predicting client's sim session over og-net. It drains the client's inbox,
keeps a mirror of the server's roster with one channel per character, hands relay rings and corrections to those channels in
arrival order, drops corrections and clock ticks older than ones already delivered, latches the relay floor and the
connection tier for a listener that binds late, and at the frame point closes this frame's input set and flushes the
transport. It hands SimClock samples to an optional host hook ahead of its decode (§4). In immediate mode it drains at step start (§16) and sends each tick's input set at capture, from the step (§17). A host can
enqueue a character's registration and unregistration for the drain to apply (§18). The prohibitions that govern its lines are in `SimNetClient-guards.md`; the host services it calls are described in
`SimNetHost-rationale.md`.

**If this file and `SimNetClient.h` disagree, the header is authoritative and this file is stale.**

---

## §1 What the session owns

- **The roster mirror** (`RosterMirror`, `SimNetRoster.h`), fed by the Roster messages the session drains.
- **One `SimNetChannel` per roster entry**, created when the entry is added and destroyed when it is removed. A channel holds
  the session's per-character transport adapter (§11).
- **The held rings:** the newest relay ring of each sim id that has no channel yet (§6).
- **The stale filter** (`CorrectionStaleFilter`), the newest correction tick per sim id, and a `NewestTickFilter` for the
  clock (§7, §8).
- **The latched floor and tier**, and whether the host's listener is bound (§9).
- **The refusal state:** whether the session has refused the stream, and the disconnect request it has not handed over yet
  (§10).
- **The lifecycle queue** (§18): the registrations and unregistrations the host enqueued and no drain has applied yet, the
  simulation they apply to, and which sim ids the queue registered.

The session borrows the host and the transport; both must outlive it. It hands `this` to the mirror's callbacks, so it is
neither copyable nor movable.

The Unreal host calls it since og-input-latency task P5-7: `UESimNetLanes` binds `dispatchFrame` to the og-net driver's
`PostTickDispatch` and `flushFrame` to its `TickFlush`, takes the disconnect request after each `dispatchFrame`, and binds the
host's listener when a world session binds (`bindClientSession`). Its host (`UESimNetClientHost`) hands each roster event to
the bindings that register the character's channel with `SimulationNetSync`. That host runs the session frame-locked
(`SimNetConfig::immediate` false); immediate mode (§16) is not bound yet.

## §2 The template

- **`Game`** is anything with a `Simulatables` list (`SimNetGame`, `SimNetServer.h`). **It must hold exactly one simulatable
  type**, and a `static_assert` says so. The client creates a character's channel from its roster entry, and the roster's v1
  entry carries a sim id, a physics slot and a local index, but no simulatable type, so the client could not tell a second
  type's characters apart. The server session has no such limit, because the server's `addCharacter<SimT>` names the type. A
  game with two types needs a type field in the roster, which is a wire change. The brawler has one (`SimulatableBrawler`).
- **`HostT`** satisfies `SimNetClientHost` (`SimNetHost.h`).
- **`TransportT`** satisfies `SimNetClientTransport` (§3).
- `SimT` is the game's simulatable, `Shape` its envelope shape, `ChannelTransport` the type the transport's
  `channelTransport(simId)` returns, and `Channel` the channel type the host registers with `SimulationNetSync` as the
  character's prediction owner: `SessionChannel<SimT, ChannelTransport>` (`SimNetChannel.h`). It is the type
  `SimNetServer::Channel<SimT>` is for the same transport, so a game's `SimulatableOwnerTraits` names one owner type for both
  roles; a `static_assert` pins the alias.
- **`HostT`'s optional `noteDelivered`** (§4): a `static_assert` refuses a host member of that name that the session could
  not call as `noteDelivered(SimLane, double)` (`SimNetDeliveryHookWellFormed`, `SimNetHost.h`).

## §3 The transport

`SimNetClientTransport` asks for `inbox()` (with `drain` and `stats`), `flushFrame()`, `endInputSends()`,
`channelTransport(simId)` (a `ClientSimTransport`), and `setServerConnection` / `serverConnection`. `SimNetTransport` satisfies
it; a `static_assert` on the transport's reference shape checks it. The send at capture (§17) also needs `beginInputSends()`
(`SimNetInputWindowTransport`), which only that member requires; `SimNetTransport` has it, and a `static_assert` checks that too.

The client has one connection, to the server. The session learns it from the `Connected` event and gives it to the transport
with `setServerConnection`, which is where the transport sends every input set. A `Disconnected` for that connection, or one
with id 0 (og-net's report of a failed connect), clears it in both. A `Connected` with id 0 is ignored. Messages are not
checked against the connection id: the inbox has one producer, the client endpoint, and its one connection.

## §4 The drain

`drain()` hands every record in the inbox to the session in FIFO order and returns how many it handled; `dispatchFrame()` is
the same drain under the name of its frame point, and an immediate session refuses it (§16). The inbox's single-consumer guard
holds; the session adds no thread check.
As on the server (`SimNetServer-rationale.md` §3), Control and the connection events have their own rings in the inbox and the
drain keeps the push order across all three, so the server's Roster, floor and tier messages reach the session where og-net
delivered them, after the
`Connected` that sets the server connection; the inbox's drain gives the connection's quota back itself. The client's one
connection has the server's Control quota too (`SimNetTransport-rationale.md` §10): a server that floods its client past it is
disconnected, and its `Disconnected`, with the cause `ControlQuota`, is logged as
`[Warning][SimNet.Disconnect] role=client conn=<id> cause=controlQuota (more undrained Control messages than its slot's quota)`
and counted (`quotaDisconnects`) before the session handles it.

Each message is decoded with the game's shape, for the server-to-client direction.
- A **Roster** message on the Control lane that the envelope refuses (truncated or overlong) is treated as a refused roster
  (§10), with the roster refusal `Truncated` or `Overlong`. P7's rule is that a refused roster decode disconnects, and the
  envelope's length check is that decode's first step.
- Any other refusal, and any kind a client never receives (an input set), counts `badDecode` and is dropped.
- **Roster** → the mirror (§5); **RelayRing** → §6; **CorrectionState** → §7; **ClockPublish** → §8; **RelayFloor** and
  **ConnectionTier** → §9.

**The delivery hook.** When the host has `noteDelivered(SimLane, double recvSeconds)`, the session calls it once for every
`Message` record it drains, with the record's lane and receive time, before the refusal check and before it decodes the
message: a message ignored while the stream is refused, and a malformed one, are delivered messages too. A `Connected` or
`Disconnected` record calls nothing. A host without the member compiles unchanged and is never called. The server session
does the same (`SimNetServer-rationale.md` §3).

**SimClock samples, ahead of the decode** (og-input-latency task P3-10). A host that builds a `SimNetClockClient` on the
session's transport gets one clockSample record in the inbox per pong (`SimNetClock-rationale.md` §6). After the refusal check
(a refused session ignores a sample like any message, G-04) and before the decode, `onMessage` asks
`simNetClock::readSample(record)`; a sample goes to the host's optional `onClockSample(sample)` (`SimNetHost-rationale.md`
§4b), in drain order, so a sample drained before a clock tick reaches the host before that drain's `onTimingInfo` (§8). It is
counted in `clockSamples`, and in `clockSamplesUnheard` too when the host has no hook. Without the branch the decode would
refuse the record (its version byte is one no envelope carries) and count a `badDecode`. The delivery hook sees a record like
any message.

## §5 The roster and the channels

The mirror applies a message all or nothing and then reports its difference: removed entries first, then added ones. The
session acts on each:

- **Added:** the channel is created, then the entry's held ring (if any) is stored in it, then the host is told
  (`onRosterAdded`). The host registers the channel with `SimulationNetSync` in that call, and registration reads the
  channel's ring once at bind. So a ring that arrived before the character's Add is what that read finds, as on today's
  path, where a ring replicated before registration is read at registration. The host's call comes last for that reason.
- **Removed:** the host is told first (`onRosterRemoved`), while the channel still exists, because unregistration clears the
  callbacks NetSync bound on the channel. Then the stale filter forgets the sim id, and the channel and any held ring are
  destroyed. A character registered through the session's lifecycle queue (§18) is unregistered by the session itself,
  first, before the host is told (G-12).
- A second Snapshot (a re-attach) replaces the mirror and reports only the difference, so a character in both keeps its
  channel, and a character that left and came back gets a new one.

These two orders are pinned by `SimNetClient.TheRosterCreatesChannelsBeforeTheHostEventAndDestroysThemAfterIt` and
`SimNetClient.ARingStoredBeforeRegistrationIsReadAtTheBindTimeRead`, not by a tag: a tag on one statement of a pair cannot
see the other statement move.

A refused roster message is §10.

## §6 Relay rings

A ring for a sim id with a channel is stored in it (`storeRing`, which calls the bound relay callback: NetSync's arrival path
for a remote character), then each of its entries with a capture tick is stamped `ClientReceived` on the Input stream with
og-net's receive time. Today's Unreal component stamps before it calls the callback; the stamp's value does not depend on
the order, because it carries the message's receive time, not the time of the call.

**A ring for a sim id with no channel is held** (rule for relays that arrive before the Snapshot or before the character's
Add). og-net's Control lane, which carries the roster, and the relay lane are separate, and the server broadcasts relays to
every connection og-net has, including one it has not yet sent a Snapshot. So a ring can arrive before the client knows the
character. The session keeps the newest such ring per sim id (counted `ringsHeld`) and stores it in the channel when the
character is added (counted `heldRingsApplied`), before the host registers it (§5). This is today's latch: the Unreal relay
actor keeps the last ring it received while no consumer is bound, and the consumer reads it when it binds. Two limits:
- a ring for a sim id the roster has listed before (a character already removed) is dropped and counted `unknownSim`, since a
  server never reuses a sim id (G-07);
- at most `kRosterMaxEntries` rings are held; a ring for another sim id beyond that is dropped and counted `unknownSim`.

A held ring is not stamped, at arrival or when it is applied: it reached no character's arrival path, as today's unrouted ring
reaches none.

**No replay when the relay callback is bound.** Today's component calls the relay callback with the current ring when the
callback is set and the ring's host is linked, and again when a host links after the callback. That replay exists because
the component's ring lives on a separate replicated actor, whose arrivals before the link find no consumer. Here the channel
owns the ring, every arrival reaches it, and registration's own bind-time read ingests it. The replay's only other effect is
one call of the arrival path at registration, with a ring that did not arrive then, which adds a relay-probe line and sample
that is not an arrival; NetSync keeps its bind-time read out of that probe on purpose. The channel does not replay
(`SimNetChannel-rationale.md` §3), and neither does the session. The difference from today is at most one
`[RelayProbe.Ingest]` line per remote registration.

## §7 Corrections

In order:
1. **The version latch.** After a correction whose correction version is not this build's
   (`correctionStateBuffer::kWireFormatVersion`), the session calls `onWireMismatch(received, expected)` once and ignores every
   later correction (counted `mismatchIgnored`; G-01). The host shows today's Error line and toast. Today's latch is per
   character component, so a mismatching server produces one Error line per character; here it is one per session, and the
   toast was already one (it has a fixed key).
2. **The channel.** A correction for a sim id with no channel is dropped and counted `unknownSim`, before the stale filter, so
   a sim id the client does not know never advances the filter. It is not held: a correction is sent again at the
   character's next rotation turn.
3. **The stale filter** (R1; G-02). A tick at or below the newest one delivered for that sim id is dropped and counted
   (`staleDropped`, `duplicateDropped`). og-net's SimState lane keeps no order between datagrams, and today's replication
   delivered a property's values in order, so the filter is what keeps order. The filter reads nothing of the correction cache.
4. **`[ReceiveCorrectionState] id=<simId> tick=<tick>`** through `log`: today's text, without a severity prefix, so the host's
   route table gives it today's route and level.
5. **The State stamp**, `ClientReceived`, with og-net's receive time.
6. **`storeCorrection`**, which calls the bound correction callback (NetSync's inject). The envelope has already checked the
   payload's exact length for this correction version, so a refusal here counts `badDecode` and cannot happen with a decoded
   message.

## §8 The clock

A clock tick goes through the `NewestTickFilter` (newest wins, as for corrections). A tick the filter admits is only
latched (`onClock`, G-09); after the inbox is drained, `drain()` hands the newest latched tick to
`onTimingInfo(tick, serverRttSeconds())` once, so the host gets **at most one clock sample per drain** and a drain with no
admitted tick calls nothing. `clocksDelivered` counts the host calls, `clocksCoalesced` the admitted ticks a newer one in the
same drain replaced. The clock needs no roster: it is delivered whether or not a Snapshot has arrived, in both orders. Today's
timing relay handed a tick to the client's listener whenever one existed and kept none for later; the session's host always
exists, so nothing is dropped for lack of a listener. The latched tick is delivered after every other record of the drain,
so a clock admitted before a `Disconnected` record or a roster refusal in the same drain reaches the host after it, and the
host takes one sample from a connection that has closed (a clock message after a refusal is ignored, G-04, and never
latched).

**Why the coalesce, and why here** (og-input-latency task 27, scope (i)). The drain runs once per client frame, so this is
one sample per frame. The server publishes one clock per server frame, and while a client's frames do not drain (at a join)
the messages queue: og-input-latency's Ph3 gate run (task P5-25) counted 10 to 17 in a client's first drain, as many as
the client's first step batch. Delivered one by one, each
was an `updateRTT` with the same `serverRttSeconds()` reading plus a `recordAuthorityTick`. The replicated timing relay this
replaced coalesced to its latest value, so its first frame carried one sample. The coalesce lives in the session, not in a
host, so every host (the Unreal one, `UESimNetClientHost::onTimingInfo`, and a later Godot one) gets it, and the scripted
tests can count it.

⚠ **It is hygiene, not the Ph3 landing fix.** Read from that run's logs in its review: every first-frame sample carried a
smoothed RTT of 0 and the floor offset, so the newest sample gives the same target as the last of the sixteen. The landing above
target comes from the hard resync's frame running its remaining catch-up steps past the target, which the clock's one-shot
re-centre (`TimeConfig::recentreSettleSamples`) corrects. What the coalesce does change is the estimator's sample count, which
the re-centre's settle window counted until og-input-latency P3-11 (it counts the clock's advances since): sixteen identical
readings would have counted as sixteen samples.

## §9 The floor and the tier

Both are latched on arrival, then the host is called with `replayed = false` if its listener is bound. `bindListener()` marks
the listener bound and replays each latched value once with `replayed = true`; a second `bindListener()` does nothing until
`unbindListener()`. This is today's latch-and-replay: each relay latches its value first and the listener pulls it at bind. In
an immediate session the bind and the unbind take effect at the next drain instead (§16, G-10).

- The tier's old value is the previously latched tier, 0 before the first. A replay passes the latched pair (old, new), the
  same pair the last arrival reported. Today's replay call passes only the new tier.
- Values are not de-duplicated: the server sends a floor on each change and on attach, and a tier on each change, so each
  arrival is a value the host has not had on this connection.

## §10 A refused roster

A roster message the mirror refuses (a revision gap, a malformed message, an Add of a live entry, …) is P7's C-6 case: the
stream cannot be trusted, and the client disconnects with the reason.
- `[Roster.Refused] reason=<refusal>` is logged through the host (`formatRosterRefused`, `SimNetRoster.h`).
- The session records a `SimNetDisconnectRequest {conn, reason}`, which the host takes with `takeDisconnectRequest()` and
  carries out with og-net's `disconnect(conn)`. Neither `SimNetTransport` nor the client host has a disconnect service, and the
  host owns the endpoint, so the session asks rather than acts. P5's host takes the request after each `dispatchFrame`.
- The transport's server connection is cleared at once, so no input set leaves before the host disconnects (G-03); a set ended
  meanwhile is counted `inputSetsWithoutServer` by the transport.
- Every later message is ignored (counted `ignoredWhileRefused`; G-04) until a new `Connected`, after which the server's
  Snapshot starts a fresh stream.

## §11 Sends

- **The adapter.** Each channel holds a `SimNetBundleTransport` (`SimNetChannel.h`, role-neutral since the server's channels
  hold one too), which wraps the transport's per-character adapter (`channelTransport(simId)`). It drops an empty bundle and
  counts it in the session's `emptyBundles` (`SimNetChannel-guards.md` G-05). A channel hands over a 0-byte bundle when it has
  no slot to send, as the Unreal owner handed over an empty one. Until og-input-latency task P5-7 that empty bundle went on
  the wire as an RPC and the server dropped it on receipt; here it never goes on the wire, and an input set never holds a
  0-byte entry.
- **One set per frame.** NetSync's send loop calls each local channel's `sendLocalInputToAuthority`; the transport appends
  each bundle to one input set. `endInputSends()` ends the set. In a frame-locked session `flushFrame()` ends it too, before it
  flushes the transport (G-06), so a host that does not call `endInputSends()` still sends one set per frame, and the next
  frame's bundles never join this frame's set.
- **Frame-locked or immediate.** With the transport's `immediate = false` (Ph3) the set is held with the frame's other sends and
  leaves at `flushFrame()`; with `immediate = true` it leaves when the set ends. An immediate session sends one set per tick, at
  capture, through `afterCaptureTick_Step` (§17): `flushFrame()` then leaves the set alone, and `endInputSends()` is refused.
- **The message rate.** A set is split when it would pass 512 B. With four local players, a set is two messages at the default
  redundancy of 3 slots and four messages at 8 slots, so the SimInput rate is the send rate times 2 or 4. og-net caps it per
  connection on the server side (`recvMaxMessagesPerSecond`); the rate case below measures where that cap binds.

## §12 The duplicate-channel check

`onRosterAdded` fails an `OG_CHECK` if the sim id already has a channel. The mirror refuses an Add of a live entry, and its
added and removed callbacks follow its own state, so this cannot happen unless the session's callbacks lose step with the
mirror, for example if a removal stopped destroying the channel. The return after it (G-08) runs only where `OG_CHECK` is
compiled out.

## §13 `[SimNet.Window]`

Every 10 s of the host's `nowSeconds`, checked at the end of `flushFrame`:

`[Warning][SimNet.Window] role=client drains= msgs= maxDepth= drops= tooLarge= badDecode= refusedNotOwner= unknownSim= statesSent= relaysSent= overBudget= staleDropped= duplicateDropped= quotaRefused= controlHighWater= quotaDisconnects= eventsRefused= eventHighWater=`

- The fields are the server line's eleven, in its order, then two of the client's own, then the server line's last five, so
  a parser that reads fields by name reads both roles. `refusedNotOwner`, `statesSent`, `relaysSent` and `overBudget` are server
  counters and are always 0 on a client.
- `quotaRefused`, `controlHighWater`, `quotaDisconnects`, `eventsRefused` and `eventHighWater` mean what they mean on the
  server line. A client's transport has one connection slot, with a Control quota and no quota on the other lanes
  (`SimNetTransport-rationale.md` §10): a full inbox counts in `drops`, and `quotaRefused` counts only Control messages over the
  Control quota, each of which closes the server connection. Its event ring has 9 slots, room for four reconnects in one drain
  gap (`SimNetTransport-rationale.md` §11), so `eventsRefused` counts the `Connected` of a fifth.
- `staleDropped` and `duplicateDropped` add the stale filter's and the clock filter's counts.
- As on the server, every field but `controlHighWater` and `eventHighWater` counts the window only, the first window starts at
  the first `flushFrame`, and `maxDepth` is the most records one drain handled.
- `SimNetClientStats` (`stats()`) holds the session's counters since construction.

## §14 What the session does not do

- **The owner-skip divergence check.** Today's component raises an Error and an ensure when a relay ring arrives for a
  character that has a local input provider. The session delivers such a ring like any other. The host sees every relayed
  entry through its Input stamp with the sim id, and the roster says which characters are local (`roster().entryFor(simId)`),
  so the check is the host's if it is kept.
- **A second server.** One session serves one server's stream. A reconnect to the same server is a new `Connected` and a new
  Snapshot; the filters and the version latch are kept. A different server would need a new session.

## §15 Tests

`SimNetClientTest.cpp` (`[NetTransport][SimNetClient]`), with a one-simulatable mock game (a second, with the brawler input's
39 B, for the rate case) and a recording host. The scripted cases push messages into the inbox of a `SimNetTransport` over a
recording endpoint; the others run P2-10's server session and the client sessions over og-net's `VirtualNetwork`.

- **Arrival order:** scripted, and over og-net with a tee on the client's inbox that records each record as the session drains
  it: corrections and rings reach the bound callbacks in arrival order, and the host gets the newest clock tick of each
  drain; the `[ReceiveCorrectionState]` lines equal today's format string.
- **The clock coalesce (G-09):** five clock messages queued before one drain reach the host as one call with the newest tick
  (`clocksDelivered` 1, `clocksCoalesced` 4); a drain with none calls nothing; the next drain delivers its own newest.
- **Stale corrections:** a scripted reorder and duplicate (ticks 10, 12, 11, 12, 13, 9, 14 deliver 10, 12, 13, 14), and over
  og-net with the emulator's 30 % duplicates and 0–40 ms reordering lag on the client's inbound side: every injected tick is
  newer than the last, the deliveries equal a newest-wins pass over the recorded arrivals, and some arrivals were stale.
  og-net's replay window drops the emulator's duplicate datagrams, so no duplicate reaches the session there.
- **The bind-time read:** a ring stored after the Add and before registration, and a ring held from before the Snapshot, are
  both what a real `SimulationNetSync` registration ingests.
- Relays, the clock and a correction before and after the Snapshot; the version latch; the floor and tier replay; a revision
  gap (scripted, and over og-net, where the host carries out the disconnect and the server detaches the connection); a
  malformed roster; the roster's channels and a second Snapshot; one held set per frame with empty bundles dropped, and the
  immediate mode; the window line.
- **The delivery hook:** a host with `noteDelivered` sees one call per message record, with its lane and receive time (a
  malformed message and one ignored while refused included), and none for connection events.
- **SimClock samples** (`SimNetClockSampleRouteTest.cpp`, `[NetTransport][SimNetClockSample]`): a real `SimNetClockClient` on
  the session's transport pings through a capturing endpoint, and a pong delivered through the SimClock lane's handler reaches
  the host's `onClockSample` bit for bit as `simNetClock::sampleOf(pong, t3)`, before the drain's clock tick, with no
  `badDecode`; a session that refused the roster ignores a later sample; a host without the hook compiles and gets the sample
  counted (`clockSamplesUnheard`), not decoded.
- **One channel type:** over one transport the client's `Channel` and the server's `Channel<SimT>` are the same type, and it
  satisfies both NetSync owner concepts.
- **The SimInput rate:** four local players, one set per 144 Hz frame and one per 60 Hz tick, at 3 and 8 slots, against the
  cap in og-net's default lane table. A hidden `[.][SimNetRate]` case prints the table for 60, 120, 144 and 240 frames per
  second.

`SimNetClientImmediateTest.cpp` (`[NetTransport]`) covers immediate mode (§16):
- **Where the writes run:** two clients over og-net, each draining at the start of every step on a chain of tasks over two
  worker threads, while the test's main thread (the game thread) only services the endpoints, publishes the RTT and binds the
  listener. The test simulatable's field setters record the thread of every relay-store write (the ring entries decoded into
  the store) and every correction-cache write (the correction decoded for the insert); the host records the estimator feed,
  the tier and the floor. None runs on the game thread.
- **The P2-12 lossless script in immediate mode:** the clients drain at step start after the server's frame, and the server and
  every delivered value (each client's corrections, rings, clock ticks and `[ReceiveCorrectionState]` lines, in order) equal
  the frame-locked run's, each delivered up to one frame earlier.
- The mode comes from the transport's `config()`, and a transport without one (the P2-12 rig's tee) runs frame-locked; the
  listener bind and unbind are applied by the next drain, the newest request winning; frame-locked binds as before. A hidden
  `[.][OGSimDeath]` case calls `dispatchFrame()` on an immediate session. Its clients send through `afterCaptureTick_Step`
  (§17), once per frame after the frame's steps, as P2-12's script sends, so the server sees the same input as the frame-locked run.

`SimNetClientSendAtCaptureTest.cpp` (`[NetTransport]`) covers the send at capture (§17). One client session over a transport
whose endpoint records each SimInput set with the frame and the phase (step, game thread, flush) it left in; the roster gives it
two local characters and a remote one, registered with a real `SimulationNetSync`. The frame-locked arm sends as the Unreal host
does today (NetSync's send on the game thread after the frame's steps, then `flushFrame`); the immediate arm calls
`afterCaptureTick_Step` after each tick's step.
- **One tick per frame:** over 300 frames both arms send 300 sets, byte for byte the same, in the same frame, the frame-locked
  ones at the flush and the immediate ones at the step; the `[SendLocalInputToServer]` lines are the same.
- **144 frames per second at a 60 Hz tick:** immediate sends 60 bundles per character in each of 3 seconds (frame-locked 144);
  each newest capture tick is one more than the last; every bundle past the first two ticks carries `redundancyDepthTicks`
  (3) slots, and every send line says `slots=3`. With `redundancyFrames` 4, every bundle past the first three ticks carries 4
  (the ticks-per-send EMA at 1).
- **The knobs are read live:** `redundancyDepthTicks` changed from 3 to 5 between two frames moves the next send line to
  `depth=5 slots=5` and the bundles to 4, then 5 slots.
- **Where the send runs:** steps and sends on a chain of tasks over two worker threads, the test's main thread (the game thread)
  only posting them; the provider call (the queue's push end), the input reads of the bundle encode (its pop end), the
  `[SendLocalInputToServer]` line (the sender map walk) and the endpoint's SimInput send are recorded with their thread. In
  immediate mode none runs on the game thread. The same rig frame-locked, with NetSync's send on the main thread, records the
  last three there, so the recording sees a game-thread send when there is one.
- Hidden `[.][OGSimDeath]` cases: NetSync's send on the game thread of an immediate session (the transport's check),
  `endInputSends()` on an immediate session, and `afterCaptureTick_Step` on a frame-locked one.

`SimNetIsolationTest.cpp` (`[NetTransport][SimNetIsolation]`) checks the window line's inbox fields on a real transport: the
quota refusals and the Control high-water mark, a quota disconnect, and, with a client's config over two windows, a fifth
reconnect in one drain gap refused and the event ring's high-water mark (the refusal counted in its own window only, the
high-water mark kept).

`SimNetLifecycleQueueTest.cpp` (`[Lifecycle]`) covers the lifecycle queue (§18) of both sessions, with a real
`SimulationNetSync` and its peers behind them:
- **Mid-batch:** a register enqueued between a drain and that step's collect is not collected, and is collected from the next
  drain on; with the steps on a thread of their own and the test's main thread (the game thread) enqueuing while they run, the
  character is collected from some step after the enqueue and on every step after it, `applied` runs on the step's thread, and
  no `NOCACHE` line is logged.
- **A register and the first arrival in the same drain:** the correction reaches NetSync (one landing) and the ring the remote
  store.
- **An unregister, then a late arrival:** with the roster still listing the character, the late correction and ring are two
  `unknownSim` counts and reach nothing, and a register after them works again; with a roster Remove before the queued
  unregister, the session unregisters the character before the host's event, the queued unregister does nothing, and a late
  correction is one `unknownSim`.
- **Publish last and unpublish first through the queue:** at the one move of the simulatable into storage, its provider, its
  delay line and its correction cache already exist; at its destruction in storage, all three still exist.
- A refused register (no channel, already registered) is counted and logged; the server's queue applies at the top of its
  drain, and a bundle that arrives after the unregister in the same drain is an `unknownSim`; `removeCharacter` of a
  queue-registered character unregisters it first; a refused add reaches `applied` as `nullptr`. Hidden `[.][OGSimDeath]`
  cases: a register before the bind, and a second bind.

## §16 Immediate mode (Ph4)

`SimNetConfig::immediate` is fixed when the transport is built, and the session reads it from the transport's `config()` once,
at construction (`immediate()`). A transport type with no `config()` runs a frame-locked session
(`SimNetConfiguredTransport`, `simNetTransportIsImmediate`): a wrapper around a real transport must forward `config()` or its
session drains frame-locked over an immediate transport. `SimNetTransport` has one, and a `static_assert` pins it.

- **The drain point.** An immediate session is drained by `drain()`, which the host calls at the start of each step, before
  `collectInputAll`, on the step's thread (og-input-latency P5 binds it). `dispatchFrame()` is refused with an `OG_CHECK`, so a
  host that still binds the frame point fails at once instead of running the drain's consumers on the game thread. The drain is
  owner-agnostic: the inbox's single-consumer guard is an in-drain flag, never a thread id, so a FrameBounded client whose steps
  run on a chain of worker tasks, each on any worker, drains from whichever worker runs the step.
- **What then runs on the step.** Every consumer the drain reaches: the correction inject (`storeCorrection`, then NetSync's
  `onCorrectionReceived`, `injectCorrectionState` and `tryInsertingCorrectState`), the relay ingest (`storeRing`, then
  `ingestRelayRing`), NetSync's arrival telemetry on both paths, the estimator feed (`onTimingInfo`, which the Unreal host
  passes to `UESimulationSession::onTimingInfoReceived`, then `updateRTT` and `recordAuthorityTick`; that function also notes a
  wire sample in the session's latency-budget diagnostics, a host-side write P5 accounts for), the floor and the tier
  (`onRelayFloor`, `onConnectionTier`), and the roster with its host events. So og-sim's crossings rows 1, 2, 4, 5, 7, 9 and 10
  (`ThreadingCrossings.md`) are same-thread in immediate mode: the writer is the step thread that also reads them, between
  steps. The clock that reads the estimator and the stall debt (`ClientPredictionClock::advancePrediction`) runs inside the
  step too (`onGameSimulationPrediction`), after the drain.
- **The tier receipt in the Unreal host** (`UESimulationSession::onConnectionTierReceived`, then `applyReplicatedConnectionTier`
  and `recomputeAndPublishEffectiveInputDelay`; the floor's `applyReplicatedRelayDelayFloor`) is a writer of tier state: the tier
  consumer, the last published delay, the runtime relay floor (`SimulationManager::setRelayDelayFloorTicks`), the published
  delay atomic and the clock's stall debt (`requestInputDelayIncreaseStall`). In immediate mode it runs inside the drain at step
  start, on the step's thread, both for an arrival and for the replay at bind (G-10). Its step-side readers (`collectInputAll`'s
  delay read, the clock's stall) run later in the same step on the same thread, so they need no synchronization. Its
  game-thread readers are display reads (the HUD's `getReplicatedTierConsumer`, the input-history delay decomposition); they
  must read the display snapshot's tier fields (og-sim `SimulationDisplaySnapshot.h`, `DisplayTierReading`) before immediate
  mode is switched on, which is og-input-latency P5's re-pointing. Frame-locked, all of it stays on the game thread as today.
- **The RTT.** The drain's clock delivery reads `serverRttSeconds()` on the step's thread. In immediate mode the host serves it
  from a value its game thread publishes atomically each frame (og-input-latency R-P5-9); the Unreal host's current body reads
  the driver's server connection directly, which is game-thread state.
- **The listener.** `bindListener()` and `unbindListener()` may be called from another thread than the drain's (the Unreal host
  binds from the game thread). In an immediate session they only store a request in one atomic; the next `drain()` applies it
  before it reads any record, so the replay of the latched floor and tier runs on the step's thread with the other host calls
  (G-10). The newest request wins: a bind and an unbind before one drain leave the listener unbound and replay nothing. A replay
  then reaches the host one drain after the bind instead of during it. Frame-locked, both apply at once, as before.
- **What the host still owns.** The rest of the session's surface (`takeDisconnectRequest`, `roster()`, `channel()`, the
  accessors, `flushFrame` and the sends) belongs to the drain's owner, called between steps from the same chain. A host that
  reads the roster or a channel from the game thread (the Unreal host's `bindClientSession` seeds its bindings from `roster()`),
  or that registers characters from its roster events on the game thread, needs the lifecycle queue (§18) and P5's re-pointing
  first. The sends run at capture, from the step (§17).

## §17 Sends at capture (Ph4)

- **The hook.** `afterCaptureTick_Step(sender, tick, timeConfig)` is the host's call after a tick's capture, on the step's
  thread. It opens the transport's input set for the calling thread (`beginInputSends`), calls
  `sender.sendLocalInputToAuthorityAll(tick, redundancyDepthTicks, redundancyFrames)` and ends the set, which in immediate mode
  goes straight to the endpoint. `sender` is the client's `SimulationNetSync` (`SimNetInputSender`). Both knobs are read from
  `timeConfig` on every call, as `SimulationManager::onPostSimulationGameThread` reads them, so an ini change reaches the next
  send; the session keeps no copy.
- **One set per tick.** The host calls the hook once per tick, so a client sends at the tick rate, not the frame rate: at 144
  frames per second and a 60 Hz tick, 60 sets a second, where the frame-locked send makes 144. Each send is one tick after the
  last, so NetSync's ticks-per-send EMA sits at 1, and its slot count (`SimulationNetSync::redundancySlots`) is
  `redundancyDepthTicks`, or `redundancyFrames` when that is larger: redundancy keeps its meaning (design D14).
- **Only in immediate mode.** A frame-locked session refuses the hook with an `OG_CHECK`: it sends once per frame, from the
  host's game-thread send and `flushFrame()`, as before.
- **The game thread's send is refused.** In immediate mode `endInputSends()` is refused with an `OG_CHECK`, `flushFrame()` does
  not end the set (G-06 is the frame-locked branch), and the transport refuses a bundle or a set end from any thread but the one
  that opened the set (`SimNetTransport-guards.md` G-07). The Unreal host's client sends from the game thread today:
  `UESimulationSession::onPostSimulationGameThread` calls `SimulationManager::onPostSimulationGameThread`, which calls NetSync's
  `sendCorrectionAll` (an empty walk on a client, which has no authority writers) and `sendLocalInputToAuthorityAll`. With
  immediate on, that send reaches the transport outside an open set and fails the check, so og-input-latency P5 stops it on an
  immediate client and calls the hook from the step instead.
- **What becomes step-only.** NetSync's send walks `m_localInputSenders`, reads `m_redundancyCadences` with `operator[]` (a
  sender's first send inserts its entry) and takes each sender's `PendingInputQueue` from its pop end (`forEachRecent` in the
  channel's bundle build, then `releaseAllButRecent`). The queue's push end is `collectInputAll`, in the step. With the send at
  capture all of it runs on the step's thread, so the queue's two ends are on one thread and the two maps are walked only there
  (og-sim `ThreadingCrossings.md` row 11). Inserting and erasing their entries at registration and unregistration runs where
  the host registers a character: at the drain, on the step, for a host that enqueues them (§18); on the game thread in the
  Unreal host until og-input-latency P5-17 wires the queue.
- **A transport wrapper** must forward `beginInputSends()` as well as `config()`: the hook requires it
  (`SimNetInputWindowTransport`), so a wrapper that does not forward it fails to compile where the host calls the hook.

## §18 Registration through the lifecycle queue (Ph4)

- **The calls.** `bindLifecycle(storage, reconciliation, inputResolution, netSync)` names the simulation the queued jobs apply
  to, once, before the first registration; an `OG_CHECK` refuses a second bind and a registration before the bind.
  `registerCharacter(simId, simulatable, provider, applied)` and `unregisterCharacter(simId, applied)` only enqueue a job on
  the session's `DeferredLifecycleQueue` (og-simulation `DeferredLifecycleQueue.h`), so the host may call them from its game
  thread while a step drains. `provider` is a local character's input provider and empty for a remote one, as
  `registerSimulatable` takes it. `applied` is optional: the drain calls it right after it applied the job, on the drain's
  thread, with the channel, or with `nullptr` when the job did nothing.
- **The apply point.** `drain()` applies every queued job at its top, after the listener request and before it reads any
  record (G-11). A registration enqueued while a step batch runs is invisible to that step's collect and whole from the next
  drain, and a correction or a ring that arrives in the same drain as the registration reaches the registered character. The
  queue is FIFO, so a register and the unregister after it apply in that order.
- **Through the facades, in their published order.** A register job calls `registerSimulatable` with the session's channel as
  the character's prediction owner, so the id reaches storage after its correction cache, its provider and its delay line
  (publish last). An unregister job calls `unregisterSimulatable` with the channel, so the id leaves storage before the rest
  (unpublish first). The channel is the owner and the session owns it, so the apply touches no game-thread object. A
  `static_assert` checks that the game's `SimulatableOwnerTraits` names `Channel` as the prediction owner.
- **What the drain refuses.** A register for a sim id with no channel (the roster has not listed it, or removed it before the
  apply), or for one the queue already registered, is refused: counted (`lifecycleStats().registersRefused`) and logged as
  `[Warning][SimNet.Lifecycle] role=client simId=<id> refused=noChannel|registered`, and the simulatable is destroyed with the
  job. An unregister of a sim id the queue has not registered (a roster Remove unregistered it first) is counted
  (`unregistersIgnored`) and does nothing.
- **After an unregister.** The roster may still list the character, so its channel stays (channels follow the roster, §5),
  but the session treats the sim id as unknown until it is registered again: a correction or a ring for it is counted as
  `unknownSim` and dropped, so nothing is stored in a channel whose callbacks were cleared. A later register on the same
  channel ends that.
- **A roster Remove of a queue-registered character** unregisters it inside the drain, through the same facade, before the
  host's `onRosterRemoved` and before the channel is destroyed (G-12), so the session never destroys a channel NetSync still
  holds (§5). The host's own queued unregister for it then does nothing.
- **The host hears of it before the facade.** Whenever the queue unregisters a character (a roster Remove or a queued
  unregister), `unregisterLifecycle` calls the host's optional `onLifecycleUnregistering(simId)` first, while the character is
  still in storage (G-13; `SimNetHost-rationale.md` §4a), so a host can tell its game's systems to forget the character. The
  Unreal host answers with the systems executor's unregister notify (og-input-latency task P5-19).
- **Both modes.** The apply runs where the drain runs. Immediate, that is the step-start drain on the step's thread (§16),
  which with the send at capture (§17) leaves og-sim's row 11 one thread (`ThreadingCrossings.md`). Frame-locked, it is the
  game thread's frame point, the thread a direct registration runs on today. A host that does not enqueue registers as before.
- **What stays the host's.** The work a host does around the facade (the Unreal host's handed callback on the channel, its
  bindings, the systems-roster notify, its per-id maps) runs where the host puts it; `applied` is the place to do it on the
  drain's thread (og-input-latency P5-17 wires the Unreal host). The session destroys its channels with itself, so a host
  unregisters its characters, or stops using the simulation, before it destroys the session, as with a direct registration.
  `lifecycleRegistered(simId)` and `lifecycleStats()` (`SimNetLifecycleStats`, `SimNetServer.h`) belong to the drain's owner.
