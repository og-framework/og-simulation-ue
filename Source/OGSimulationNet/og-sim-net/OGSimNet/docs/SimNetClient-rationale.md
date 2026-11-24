<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetClient.h` — rationale

`SimNetClient<Game, HostT, TransportT>` is the predicting client's sim session over og-net. It drains the client's inbox,
keeps a mirror of the server's roster with one channel per character, hands relay rings and corrections to those channels in
arrival order, drops corrections and clock ticks older than ones already delivered, latches the relay floor and the
connection tier for a listener that binds late, and emits its window line from `flushFrame` (§13). It hands SimClock samples to an
optional host hook ahead of its decode (§4), and while its listener is unbound holds the newest one for the first clock
tick after the bind (§19). It drains at step start (§16) and sends each tick's input set at capture, from the
step (§17). A host can
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

**Its threads.** The session has one owner at a time, the thread that drains it: `drain()`, `flushFrame()`,
`takeDisconnectRequest()`, `roster()`, `channel()`, the accessors and the send at capture (`afterCaptureTick_Step`, §17) belong
to it, one consumer at a time (§16). Drained at the start of each step, the owner is the step's thread. `bindListener()`,
`unbindListener()`, `registerCharacter()` and `unregisterCharacter()` may be called from another thread: each only posts a
request that the next drain applies (§16, §18). Which host thread makes which call is the host's (og-sim's
`ThreadingCrossings.md` points to the table). The host hands each roster event to the bindings that register the character's
channel with `SimulationNetSync`. Until og-session-runtime task 13 the session also had a frame-locked mode, drained and
flushed once per frame and sending once per frame.

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

`SimNetClientTransport` asks for `inbox()` (with `drain` and `stats`), `channelTransport(simId)` (a
`ClientSimTransport`), and `setServerConnection` / `serverConnection`. `SimNetTransport` satisfies it; a `static_assert` on the
transport's reference shape checks it. The send at capture (§17) also needs `beginInputSends()` and `endInputSends()`
(`SimNetInputWindowTransport`), which only that member requires; `SimNetTransport` has them, and a `static_assert` checks that
too. og-session-runtime task 13 removed `endInputSends()` from `SimNetClientTransport` (its users were the session's own
`endInputSends()` and the frame-locked branch of `flushFrame()`) and the configured-transport concept (the session no longer reads
its transport's `config()`). og-session-runtime task 15 removed `flushFrame()` from it: the transport has no frame hold to
release (`SimNetTransport-rationale.md` §7), and the session's own `flushFrame()` only emits the window line (§13).

The client has one connection, to the server. The session learns it from the `Connected` event and gives it to the transport
with `setServerConnection`, which is where the transport sends every input set. A `Disconnected` for that connection, or one
with id 0 (og-net's report of a failed connect), clears it in both. A `Connected` with id 0 is ignored. Messages are not
checked against the connection id: the inbox has one producer, the client endpoint, and its one connection.

## §4 The drain

`drain()` hands every record in the inbox to the session in FIFO order and returns how many it handled. The inbox's
single-consumer guard holds; the session adds no thread check.
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
§4b), in drain order, so a sample drained before a clock tick reaches the host before that drain's `onTimingInfo` (§8). While the listener is
unbound the session holds the newest sample instead of handing it over (§19). Every sample is counted in `clockSamples`, and in
`clockSamplesUnheard` too when the host has no hook. Without the branch the decode would
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

**The held SimClock sample goes first** (og-session-runtime task 20). When the listener is bound and a sample is held (§19),
the drain that delivers a tick hands the held sample to the host's `onClockSample` just before the tick's `onTimingInfo`
(G-14). It reads `serverRttSeconds()` before both, as the Unreal host's latch it replaced did (the reading was the
`onTimingInfo` argument, evaluated before the latch fed the sample).

**Why the coalesce, and why here** (og-input-latency task 27, scope (i)). The server publishes its clock regularly, and while
a client does not drain (at a join) the messages queue: og-input-latency's Ph3 gate run (task P5-25) counted 10 to 17 in a client's first drain, as many as
the client's first step batch. Delivered one by one, each
was an `updateRTT` with the same `serverRttSeconds()` reading plus a `recordAuthorityTick`. The replicated timing relay this
replaced coalesced to its latest value, so its first frame carried one sample. The coalesce lives in the session, not in a
host, so every host gets it, and the scripted tests can count it.

⚠ **It is hygiene, not the Ph3 landing fix.** Read from that run's logs in its review: every first-frame sample carried a
smoothed RTT of 0 and the floor offset, so the newest sample gives the same target as the last of the sixteen. The landing above
target comes from the hard resync's frame running its remaining catch-up steps past the target, which the clock's one-shot
re-centre (`TimeConfig::recentreSettleSamples`) corrects. What the coalesce does change is the estimator's sample count, which
the re-centre's settle window counted until og-input-latency P3-11 (it counts the clock's advances since): sixteen identical
readings would have counted as sixteen samples.

## §9 The floor and the tier

Both are latched on arrival, then the host is called with `replayed = false` if its listener is bound. After
`bindListener()`, the next drain marks the listener bound and replays each latched value once with `replayed = true`; a second
`bindListener()` does nothing until `unbindListener()`, which the next drain applies too (§16, G-10). This is today's
latch-and-replay: each relay latches its value first and the listener pulls it at bind.

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
  host owns the endpoint, so the session asks rather than acts. The host takes the request after each drain.
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
- **One set per tick.** NetSync's send loop calls each local channel's `sendLocalInputToAuthority`; the transport appends
  each bundle to one input set, which `afterCaptureTick_Step` opens before the loop and ends after it (§17). The set leaves when
  it ends; `flushFrame()` ends no set. Until og-session-runtime task 13 a frame-locked session (over a transport built with the
  mode flag off) held the set with the frame's other sends until `flushFrame()`, which also ended a set the host left open
  (retired G-06).
- **The message rate.** A set is split when it would pass 512 B. With four local players, a set is two messages at the default
  redundancy of 3 slots and four messages at 8 slots, so the SimInput rate is the send rate times 2 or 4. og-net caps it per
  connection on the server side (`recvMaxMessagesPerSecond`); the rate case below measures where that cap binds.

## §12 The duplicate-channel check

`onRosterAdded` fails an `OG_CHECK` if the sim id already has a channel. The mirror refuses an Add of a live entry, and its
added and removed callbacks follow its own state, so this cannot happen unless the session's callbacks lose step with the
mirror, for example if a removal stopped destroying the channel. The return after it (G-08) runs only where `OG_CHECK` is
compiled out.

## §13 `[SimNet.Window]`

Every 10 s of the host's `nowSeconds`, checked in `flushFrame`, which since og-session-runtime task 15 does nothing else (the
transport it used to flush first has no frame hold):

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
  malformed roster; the roster's channels and a second Snapshot; the window line. Every client in these files is built over the
  one kind of transport there is (its mode flag went in og-session-runtime task 15); a scripted send opens and ends its set on
  the transport, as `afterCaptureTick_Step` does.
- **The delivery hook:** a host with `noteDelivered` sees one call per message record, with its lane and receive time (a
  malformed message and one ignored while refused included), and none for connection events.
- **SimClock samples** (the route test, `[NetTransport][SimNetClockRoute]`): a real `SimNetClockClient` on the session's
  transport, with the listener bound, pings through a capturing endpoint, and a pong delivered through the SimClock lane's handler reaches
  the host's `onClockSample` bit for bit as `simNetClock::sampleOf(pong, t3)`, before the drain's clock tick, with no
  `badDecode`; a session that refused the roster ignores a later sample; a host without the hook compiles and gets the sample
  counted (`clockSamplesUnheard`), not decoded.
- **The held sample** (§19; the hold test, `[NetTransport][SimNetClockHold]`, og-session-runtime task 20): the cases of the
  Unreal automation test it replaced (OGBrawler.SimNet.ClockSampleLatch, deleted by task 20; its successor is this test's
  `ClockSampleLatch.*` cases), under its name and with its check texts, through a real
  transport and session and a host that forwards as the Unreal client host does: two samples before the bind and a tick hold
  the newest (71 ms, not 87 ms) and reach nothing; the first tick after the bind gets the held sample first, then the tick, and
  the `[ClockSource.Held]` line with its exact text; the next tick comes alone; a sample drained after the bind supersedes the
  held one; with nothing held a tick arrives alone. A transcription of the Unreal latch and its host (`UESimNetLanes.h` at
  `3b8a211`) runs 300 seeded scripted joins (binds, unbinds, samples and ticks per drain) beside the session and gives the same
  session calls, bit for bit, the same `[ClockSource.Held]` lines and the same held state after every drain. A host without
  the hook holds nothing. Seen failing: the tick delivered before the held sample fails the seed case and the transcription;
  the held sample fed at the bind fails those two and the superseding case (G-14).
- **One channel type:** over one transport the client's `Channel` and the server's `Channel<SimT>` are the same type, and it
  satisfies both NetSync owner concepts.
- **The SimInput rate:** four local players, one set per 144 Hz frame and one per 60 Hz tick, at 3 and 8 slots, against the
  cap in og-net's default lane table. A hidden `[.][SimNetRate]` case prints the table for 60, 120, 144 and 240 frames per
  second.

`SimNetClientImmediateTest.cpp` (`[NetTransport]`) covers the step-start drain (§16):
- **Where the writes run:** two clients over og-net, each draining at the start of every step on a chain of tasks over two
  worker threads, while the test's main thread, a thread that never steps, only services the endpoints, publishes the RTT and
  binds the listener. The test simulatable's field setters record the thread of every relay-store write (the ring entries
  decoded into the store) and every correction-cache write (the correction decoded for the insert); the host records the
  estimator feed, the tier and the floor. None runs on the main thread. Its clients send through `afterCaptureTick_Step` (§17),
  once per frame after the frame's steps.
- The listener bind and unbind are applied by the next drain, the newest request winning.
- og-session-runtime task 13 deleted the cases whose subject was the frame-locked client: the P2-12 lossless script compared
  against a frame-locked run, the mode taken from the transport's `config()`, the frame-locked bind, and a hidden
  `[.][OGSimDeath]` case that called the session's deleted frame-point drain.

`SimNetClientSendAtCaptureTest.cpp` (`[NetTransport]`) covers the send at capture (§17). One client session over a transport
whose endpoint records each SimInput set with the frame and the phase (the step or the flush) it left in; the roster gives it
two local characters and a remote one, registered with a real `SimulationNetSync`. The client calls `afterCaptureTick_Step`
after each tick's step.
- **144 frames per second at a 60 Hz tick:** the client sends 60 bundles per character in each of 3 seconds;
  each newest capture tick is one more than the last; every bundle past the first two ticks carries `redundancyDepthTicks`
  (3) slots, and every send line says `slots=3`. With `redundancyFrames` 4, every bundle past the first three ticks carries 4
  (the ticks-per-send EMA at 1).
- **The knobs are read live:** `redundancyDepthTicks` changed from 3 to 5 between two frames moves the next send line to
  `depth=5 slots=5` and the bundles to 4, then 5 slots.
- **Where the send runs:** steps and sends on a chain of tasks over two worker threads, the test's main thread only posting
  them; the provider call (the queue's push end), the input reads of the bundle encode (its pop end), the
  `[SendLocalInputToServer]` line (the sender map walk) and the endpoint's SimInput send are recorded with their thread. None
  runs on the main thread.
- A hidden `[.][OGSimDeath]` case: NetSync's send from the main thread, outside the hook (the transport's check).
- og-session-runtime task 13 deleted the frame-locked arm: the one-tick-per-frame comparison of the two arms, the frame-locked
  rate and game-thread rows of the two cases above, and the hidden cases on the deleted `endInputSends()` and on
  `afterCaptureTick_Step`'s frame-locked check.

`SimNetIsolationTest.cpp` (`[NetTransport][SimNetIsolation]`) checks the window line's inbox fields on a real transport: the
quota refusals and the Control high-water mark, a quota disconnect, and, with a client's config over two windows, a fifth
reconnect in one drain gap refused and the event ring's high-water mark (the refusal counted in its own window only, the
high-water mark kept).

`SimNetLifecycleQueueTest.cpp` (`[Lifecycle]`) covers the lifecycle queue (§18) of both sessions, with a real
`SimulationNetSync` and its peers behind them:
- **Mid-batch:** a register enqueued between a drain and that step's collect is not collected, and is collected from the next
  drain on; with the steps on a thread of their own and the test's main thread enqueuing while they run, the
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

## §16 The step-start drain (Ph4)

Until og-session-runtime task 13 a session had two modes, read from its transport's mode flag at construction: frame-locked
(drained at the frame point, sending once per frame) and immediate (this section). Task 13 retired the frame-locked half, and
og-session-runtime task 15 removed the transport's flag.

- **The drain point.** The session is drained by `drain()`, at the start of each step, before `collectInputAll`, on the step's
  thread. The drain is
  owner-agnostic: the inbox's single-consumer guard is an in-drain flag, never a thread id, so a FrameBounded client whose steps
  run on a chain of worker tasks, each on any worker, drains from whichever worker runs the step.
- **What then runs on the step.** Every consumer the drain reaches: the correction inject (`storeCorrection`, then NetSync's
  `onCorrectionReceived`, `injectCorrectionState` and `tryInsertingCorrectState`), the relay ingest (`storeRing`, then
  `ingestRelayRing`), NetSync's arrival telemetry on both paths, the estimator feed (`onTimingInfo`, through which a host
  reaches `updateRTT` and `recordAuthorityTick`), the floor and the tier (`onRelayFloor`, `onConnectionTier`), and the roster
  with its host events. So og-sim's crossings rows 1, 2, 4, 5, 7, 9 and 10
  (`ThreadingCrossings.md`) are same-thread: the writer is the step thread that also reads them, between
  steps. The clock that reads the estimator and the stall debt (`ClientPredictionClock::advancePrediction`) runs inside the
  step too (`onGameSimulationPrediction`), after the drain.
- **The floor and the tier.** The host's `onRelayFloor` and `onConnectionTier` run inside the drain, on the step's thread, both
  for an arrival and for the replay at bind (G-10). The tier state a host writes there (a delay, a relay floor through
  `SimulationManager::setRelayDelayFloorTicks`, the clock's input-delay transition debt through `requestInputDelayTransition`)
  has its step-side readers (`collectInputAll`'s delay read, the clock's debt payment) later in the same step, on the same
  thread, so they need no synchronization. A reader on another thread reads a copy: og-sim's display snapshot carries the tier
  fields (`SimulationDisplaySnapshot.h`, `DisplayTierReading`).
- **The RTT.** The drain's clock delivery reads the host's `serverRttSeconds()` on the step's thread, so a host whose RTT is
  read elsewhere serves a value it publishes atomically.
- **The listener.** `bindListener()` and `unbindListener()` may be called from another thread than the drain's. They only
  store a request in one atomic; the next `drain()` applies it before it reads any
  record, so the replay of the latched floor and tier runs on the step's thread with the other host calls (G-10). The newest
  request wins: a bind and an unbind before one drain leave the listener unbound and replay nothing. A replay reaches the host
  one drain after the bind, not during it.
- **What the host still owns.** The rest of the session's surface (`takeDisconnectRequest`, `roster()`, `channel()`, the
  accessors, `flushFrame` and the sends) belongs to the drain's owner, called between steps from the same chain. A host that
  registers characters from its roster events on another thread uses the lifecycle queue (§18). The sends run at capture, from
  the step (§17).

## §17 Sends at capture (Ph4)

- **The hook.** `afterCaptureTick_Step(sender, tick, timeConfig)` is the host's call after a tick's capture, on the step's
  thread. It opens the transport's input set for the calling thread (`beginInputSends`), calls
  `sender.sendLocalInputToAuthorityAll(tick, redundancyDepthTicks, redundancyFrames)` and ends the set, which goes straight to
  the endpoint. `sender` is the client's `SimulationNetSync` (`SimNetInputSender`). Both knobs are read from
  `timeConfig` on every call, as `SimulationManager::onPostSimulationGameThread` reads them, so an ini change reaches the next
  send; the session keeps no copy.
- **One set per tick.** The host calls the hook once per tick, so a client sends at the tick rate, not the frame rate: at 144
  frames per second and a 60 Hz tick, 60 sets a second, where the retired frame-locked send made 144. Each send is one tick after the
  last, so NetSync's ticks-per-send EMA sits at 1, and its slot count (`SimulationNetSync::redundancySlots`) is
  `redundancyDepthTicks`, or `redundancyFrames` when that is larger: redundancy keeps its meaning (design D14).
- **A send from outside the hook is refused.** `flushFrame()` ends no set, and the transport refuses a bundle or a set end from
  any thread but the one that opened the set (`SimNetTransport-guards.md` G-07), so a send from outside the hook fails the check.
  Until og-input-latency P5 a client also sent once per frame from its host's post-simulation pass
  (`SimulationManager::onPostSimulationGameThread`, NetSync's `sendLocalInputToAuthorityAll`); og-session-runtime task 12
  removed that path, and task 13 the session's own `endInputSends()` and the hook's frame-locked `OG_CHECK`.
- **What becomes step-only.** NetSync's send walks `m_localInputSenders`, reads `m_redundancyCadences` with `operator[]` (a
  sender's first send inserts its entry) and takes each sender's `PendingInputQueue` from its pop end (`forEachRecent` in the
  channel's bundle build, then `releaseAllButRecent`). The queue's push end is `collectInputAll`, in the step. With the send at
  capture all of it runs on the step's thread, so the queue's two ends are on one thread and the two maps are walked only there
  (og-sim `ThreadingCrossings.md` row 11). Inserting and erasing their entries at registration and unregistration runs where
  the host registers a character: at the drain, on the step, for a host that enqueues them (§18).
- **A transport wrapper** must forward `beginInputSends()` and `endInputSends()`: the hook requires them
  (`SimNetInputWindowTransport`), so a wrapper that does not forward them fails to compile where the host calls the hook.

## §18 Registration through the lifecycle queue (Ph4)

- **The calls.** `bindLifecycle(storage, reconciliation, inputResolution, netSync)` names the simulation the queued jobs apply
  to, once, before the first registration; an `OG_CHECK` refuses a second bind and a registration before the bind.
  `registerCharacter(simId, simulatable, provider, applied)` and `unregisterCharacter(simId, applied)` only enqueue a job on
  the session's `DeferredLifecycleQueue` (og-simulation `DeferredLifecycleQueue.h`), so the host may call them from another
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
  (unpublish first). The channel is the owner and the session owns it, so the apply touches no object the host owns. A
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
  still in storage (G-13; `SimNetHost-rationale.md` §4a), so a host can tell its game's systems to forget the character.
- **Where it runs.** The apply runs where the drain runs: the step-start drain on the step's thread (§16), which with the send
  at capture (§17) leaves og-sim's row 11 one thread (`ThreadingCrossings.md`). A host that does not enqueue registers as
  before.
- **What stays the host's.** The work a host does around the facade (a callback on the channel, its bindings, a
  systems-roster notify, its per-id maps) runs where the host puts it; `applied` is the place to do it on the drain's
  thread. The session destroys its channels with itself, so a host
  unregisters its characters, or stops using the simulation, before it destroys the session, as with a direct registration.
  `lifecycleRegistered(simId)` and `lifecycleStats()` (`SimNetLifecycleStats`, `SimNetServer.h`) belong to the drain's owner.

## §19 The SimClock sample held while the listener is unbound (og-session-runtime task 20)

A client pings from its driver's first frames, long before its world exists, so pongs come back while no world session is
bound. Until og-input-latency P3-11 the Unreal host dropped those samples, as it drops a clock tick, and the estimator's first
sample came 4 to 43 ms after the first authority tick: the join's hard resync landed on the floor target and took a second one
when the reading arrived (` cause=firstRtt`, og-simulation `TimeConfig-rationale.md` §16). P3-11 added a latch to the Unreal
client host; og-session-runtime task 20 moved it here, beside the floor and tier replay (§9), so every host has it.

- **Held.** While the listener is unbound (§9, G-10), `onClockSample` keeps the newest sample (`heldClockSample()`) and does
  not call the host. A host without the hook holds nothing: its samples are counted unheard (§4).
- **Superseded.** A sample drained while the listener is bound goes to the host at once and drops the held one, so an older
  reading is never fed after a newer one.
- **Fed once, ahead of the first tick after the bind.** The drain that delivers a clock tick (§8) with the listener bound and
  a sample held hands the held sample to `onClockSample`, then the tick to `onTimingInfo` (G-14), and logs one line through the
  host's `log`: `[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick
  <t>: rttMs=<r> heldMs=<h>` (`kClockSourceHeldFormat`, `formatClockSourceHeld`). `heldMs` is the host's `nowSeconds()` minus
  the sample's receive stamp `t3Seconds`; a host whose `nowSeconds()` is the clock og-net stamps receive times with (the
  Unreal host's is) reads the time the sample waited. A drain that delivers no tick feeds nothing. A join with nothing held
  logs nothing, and so does one whose bind drain carries a fresh sample, which supersedes the held one. The line is the smoke's
  evidence that a join's first sample came before its world.
- **The line's text, category and level.** The text is the one the Unreal host logged with `UE_LOG` until task 20. The line
  carries no `[Warning]` token: the Unreal client host routes it by its tag (`kClockSourceHeldTag`, which a `static_assert`
  checks the format begins with) to the category and level it had, `LogOGNet` at `Warning` (`UESimNetLanes-rationale.md`
  §13); a UE-side test compares the bytes, the category and the level with the old `UE_LOG`'s.
- **Why at the first tick and not at the bind.** The Unreal session binds the lanes before it applies its latency knobs, and
  the estimator switches its RTT source to SimClock at the first valid sample it takes while its config says SimClock, and
  never back (`NetworkTimeEstimator::noteSimClockSample`). Fed at the bind, the sample would meet the compiled default, and an
  ini `ClockRttSource=Engine` arm would already have switched. The first tick is on the drain's thread, after the session is
  configured, and ahead of the advance that resyncs. So the bind replays the floor and the tier (§9) but not the sample.
- **An unbind holds again.** Samples drained after it are kept, and the next bind's first tick feeds the newest.
- **Threads.** The hold is written by the drains before the bind and read by the first drain after it, which is where every
  other piece of the session's state changes hands when a host's drain moves threads at the bind (the Unreal client drains
  on the game thread until its world session binds, then on the step's thread: `UESimNetLanes-guards.md` G-02).
- **Equivalent to the latch it replaced.** The latch held while the host had no world session; the session holds while its
  listener is unbound. The Unreal lanes bind the two together (`bindClientSession`: the host's session, then `bindListener`)
  and unbind them together, on the game thread between drains, so every drain sees the same state either way. The
  transcription case of the hold test (§15) checks it.
- **Measured.** og-input-latency P3-11's diagnostic run on shape (a), three joins: each held 4 or 5 samples before its world;
  two fed the newest, held 16 and 32 ms, and one was superseded. Since og-session-runtime task 9 (`bTickPhysicsAsync=False`)
  a client's world binds a frame or two later. Joins that fed a held sample (a `[ClockSource.Held]` line in the client's
  log) since then: in og-session-runtime's Editor smokes (`SR*_smoke*` under `Saved/Profiling`, three joins each, the
  async-physics arm aside), 5 of 6 in task 9's two smokes, 4 of 27 in SR2's nine and 1 of 12 in SR3's four (1 of 18 in the
  six before task 9); in its packaged gates, 1 to 4 of 6 clients per side on shape (a) (G-SR1c's candidate 4, G-SR2's
  reference 4 and candidate 1) and none on shapes (b) and (c) (0 of 4 and 0 of 3 per side). So a held sample is fed on
  some joins, not most.
