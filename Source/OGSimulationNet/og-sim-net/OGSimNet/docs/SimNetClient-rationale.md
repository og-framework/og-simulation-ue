<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetClient.h` — rationale

`SimNetClient<Game, HostT, TransportT>` is the predicting client's sim session over og-net. It drains the client's inbox,
keeps a mirror of the server's roster with one channel per character, hands relay rings and corrections to those channels in
arrival order, drops corrections and clock ticks older than ones already delivered, latches the relay floor and the
connection tier for a listener that binds late, and at the frame point closes this frame's input set and flushes the
transport. The prohibitions that govern its lines are in `SimNetClient-guards.md`; the host services it calls are described in
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

The session borrows the host and the transport; both must outlive it. It hands `this` to the mirror's callbacks, so it is
neither copyable nor movable.

Nothing calls the session yet. P5's wiring binds `dispatchFrame` and `flushFrame` to the engine's receive and flush points,
registers each channel with `SimulationNetSync` from the host's roster events, and binds the host's listener.

## §2 The template

- **`Game`** is anything with a `Simulatables` list (`SimNetGame`, `SimNetServer.h`). **It must hold exactly one simulatable
  type**, and a `static_assert` says so. The client creates a character's channel from its roster entry, and the roster's v1
  entry carries a sim id, a physics slot and a local index, but no simulatable type, so the client could not tell a second
  type's characters apart. The server session has no such limit, because the server's `addCharacter<SimT>` names the type. A
  game with two types needs a type field in the roster, which is a wire change. The brawler has one (`SimulatableBrawler`).
- **`HostT`** satisfies `SimNetClientHost` (`SimNetHost.h`).
- **`TransportT`** satisfies `SimNetClientTransport` (§3).
- `SimT` is the game's simulatable, `Shape` its envelope shape, and `Channel` the channel type the host registers with
  `SimulationNetSync` as the character's prediction owner (`SimNetChannel<SimT, ChannelTransport>`).

## §3 The transport

`SimNetClientTransport` asks for `inbox()` (with `drain` and `stats`), `flushFrame()`, `endInputSends()`,
`channelTransport(simId)` (a `ClientSimTransport`), and `setServerConnection` / `serverConnection`. `SimNetTransport` satisfies
it; a `static_assert` on the transport's reference shape checks it.

The client has one connection, to the server. The session learns it from the `Connected` event and gives it to the transport
with `setServerConnection`, which is where the transport sends every input set. A `Disconnected` for that connection, or one
with id 0 (og-net's report of a failed connect), clears it in both. A `Connected` with id 0 is ignored. Messages are not
checked against the connection id: the inbox has one producer, the client endpoint, and its one connection.

## §4 The drain

`drain()` hands every record in the inbox to the session in FIFO order and returns how many it handled; `dispatchFrame()` is
the same drain under the name of its frame point. The inbox's single-consumer guard holds; the session adds no thread check.
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

## §5 The roster and the channels

The mirror applies a message all or nothing and then reports its difference: removed entries first, then added ones. The
session acts on each:

- **Added:** the channel is created, then the entry's held ring (if any) is stored in it, then the host is told
  (`onRosterAdded`). The host registers the channel with `SimulationNetSync` in that call, and registration reads the
  channel's ring once at bind. So a ring that arrived before the character's Add is what that read finds, as on today's
  path, where a ring replicated before registration is read at registration. The host's call comes last for that reason.
- **Removed:** the host is told first (`onRosterRemoved`), while the channel still exists, because unregistration clears the
  callbacks NetSync bound on the channel. Then the stale filter forgets the sim id, and the channel and any held ring are
  destroyed. A host that unregisters later than this call (Ph4's deferred lifecycle) must keep the channel alive itself; that
  is P2-17's change.
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

A clock tick goes through the `NewestTickFilter` (newest wins, as for corrections) and then to
`onTimingInfo(tick, serverRttSeconds())`. The clock needs no roster: it is delivered whether or not a Snapshot has arrived,
in both orders. Today's timing relay hands a tick to the client's listener whenever one exists and keeps none for later; the
session's host always exists, so nothing is dropped for lack of a listener.

## §9 The floor and the tier

Both are latched on arrival, then the host is called with `replayed = false` if its listener is bound. `bindListener()` marks
the listener bound and replays each latched value once with `replayed = true`; a second `bindListener()` does nothing until
`unbindListener()`. This is today's latch-and-replay: each relay latches its value first and the listener pulls it at bind.

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

- **The adapter.** Each channel holds a `SimNetClientChannelTransport`, which wraps the transport's per-character adapter
  (`channelTransport(simId)`). It drops an empty bundle and counts it (`emptyBundles`; G-05). A channel hands over a 0-byte
  bundle when it has no slot to send, as today's owner hands over an empty one. Today's empty bundle goes on the wire as an
  RPC and the server drops it on receipt; here it never goes on the wire, and an input set never holds a 0-byte entry.
- **One set per frame.** NetSync's send loop calls each local channel's `sendLocalInputToAuthority`; the transport appends
  each bundle to one input set. `endInputSends()` ends the set. `flushFrame()` ends it too, before it flushes the transport
  (G-06), so a host that does not call `endInputSends()` still sends one set per frame, and the next frame's bundles never join
  this frame's set.
- **Frame-locked or immediate.** With the transport's `immediate = false` (Ph3) the set is held with the frame's other sends and
  leaves at `flushFrame()`; with `immediate = true` it leaves when the set ends.
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
  it: corrections and rings reach the bound callbacks, and clock ticks the host, in arrival order; the
  `[ReceiveCorrectionState]` lines equal today's format string.
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
- **The SimInput rate:** four local players, one set per 144 Hz frame and one per 60 Hz tick, at 3 and 8 slots, against the
  cap in og-net's default lane table. A hidden `[.][SimNetRate]` case prints the table for 60, 120, 144 and 240 frames per
  second.

`SimNetIsolationTest.cpp` (`[NetTransport][SimNetIsolation]`) checks the window line's inbox fields on a real transport: the
quota refusals and the Control high-water mark, a quota disconnect, and, with a client's config over two windows, a fifth
reconnect in one drain gap refused and the event ring's high-water mark (the refusal counted in its own window only, the
high-water mark kept).
