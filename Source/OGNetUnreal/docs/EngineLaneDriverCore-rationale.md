<!-- SPDX-License-Identifier: MPL-2.0 -->
# `EngineLaneDriverCore.h` — rationale

`ognetunreal::EngineLaneDriverCore<HostT, BackendT>` is the engine-free half of the Unreal engine-lane driver: every policy the
driver needs between an og-net endpoint's engine lane (`ognet::EngineLane`) and the engine's own connections, with no engine
header. The Unreal driver (UOGNetDriver, task P4-4) holds one core and turns its callbacks into engine calls; the policies themselves are
tested here, over og-net's virtual network, without an engine session. This file is the docs pointer of `EngineLaneDriverCore.h`
and of its test, `EngineLaneDriverCoreTest.cpp`. Prohibitions are in `EngineLaneDriverCore-guards.md`.

The header holds the licence, the docs pointer, code, five guard tags and these compile-time checks:

- `kPreConnectSlotBytes` equals `ognet::EngineLane::maxPacketBytes()` (1,189 B): a pre-connect slot holds exactly one packet the
  lane would take (§4);
- `kBatchEventBytes` equals `ognet::kEngineEventBytes` (1,232 B) and fits a `uint16_t`: a batch slot holds any event a ring slot
  holds (§6);
- `kBatchEvents` (256) is at most `ognet::kEngineRingSlots`;
- `kDrainedCloseSeconds` (0.2 s) equals the Engine lane's `maxQueueAgeSeconds` in `ognet::lanes::kDefaultLaneTable` (§7).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Shape and threads

- **Header-only, std and og-net only,** namespace `ognetunreal`. `HostT` is an og-net host (`ognet::NetHostServices`) and `BackendT`
  a datagram backend (`ognet::DatagramBackend`); the core builds its own `ognet::NetEndpoint<HostT, BackendT>` in place, in a
  `std::optional`, at `start`. In the Unreal driver they are to be `FOGNetUnrealHost` and `SwitchableBackend` over
  `ognet::BsdDatagramBackend` (`SwitchableBackend-rationale.md`); in the tests, og-net's `TestHost` and `SwitchableBackend` over
  `VirtualBackend`.
- **One engine thread owns the core** (the game thread in the engine). Every member function, and every callback, runs on it.
  The engine lane fixes its poll thread at the first `poll` (`EngineLane-rationale.md` §3), so `tick` must always come from
  the same thread.
- **The one exception is the connection handler,** which og-net calls on its servicing thread: og-net's own thread in
  `IoMode::OwnThread`, or the thread that calls `serviceUntil` in `IoMode::HostServiced` (`EndpointCore.cpp`,
  `EndpointCore::raise`). It calls the sim lanes' forward target (§12), then touches nothing but the handler queue (§5), under
  `m_handlerMutex`.
- **The sim endpoint (§12)** is the other door. Its `send`, `broadcastExcept` and `disconnect` are og-net's own, which take any
  thread; its `noteDelivered` adds to the window's atomic residence counters (`NetWindow.cpp`, `NetWindow::noteDelivered`); its two
  handler setters run only inside `start`, from the attach. In `HostServiced` mode its `serviceUntil` (the core's) runs on the
  host's servicing thread, never the tick thread, and `hasHighResolutionWait` reads only what `start` wrote (§13).
- **No allocation after `start`.** The record table, the pre-connect buffer and the batch are allocated by `start`; `tick` and
  `send` only index into them.
- **Teardown.** The destructor resets the endpoint first. `ognet::EndpointCore`'s destructor calls `stop`, which in own-thread mode
  waits for og-net's thread to exit (`EndpointCore.cpp`, `EndpointCore::stop`), so the thread that calls the connection handler is
  gone before the handler queue's mutex is destroyed. The endpoint is also the last member, so the order holds without the
  explicit reset too.

## §2 The interface

| member | what it does |
|---|---|
| `start(host, backend, config, mode, callbacks, attach = nullptr)` | builds the endpoint, sets the connection handler, enables the engine lane (`engineLane()` before `start`, as og-net requires), runs the attach's `beforeStart` with the sim endpoint (§12), starts it, and in `OwnThread` mode calls `startOwnThread()`. When og-net's `start` fails it returns false and nothing is left started: the endpoint is destroyed and the forward target cleared. A second `start`, a missing callback and an attach without `beforeStart` are `OGNET_CHECK`s. |
| `connect(address, engineDeadlineSeconds)` | client only: remembers both and issues og-net's connect. The deadline is the engine's own: the time its pending connection gives up (§5). |
| `send(id, bytes)` | §3's verdicts. |
| `tick(nowSeconds, clientConnectionPending)` | in order: the handler queue (§5), the ring (§6), the drained closes (§7), the `[OGNet.Window]` close (§11). `clientConnectionPending` is whether the engine still waits for its server connection. |
| `engineClosed(id, nowSeconds)` | the engine has cleaned up its connection: §7. |
| `stop(nowSeconds, drainSeconds)` | §8. |
| `serviceUntil(deadline)` | `HostServiced` mode only (`OGNET_CHECK`): the caller services the endpoint, as og-net's mode B (§13). |
| `hasHighResolutionWait()` | the backend's `hasHighResolutionWait()` when `BackendT` has one (`if constexpr` on a `requires`), else false; false before `start` (§13). |
| `stats()` | `DriverStats`, the core's counters (§9). |
| `connectionStats(id)`, `endpointStats()`, `engineLaneStats()`, `localAddress()` | og-net's own figures, passed through; `localAddress` exists when the backend reports one (`ognet::ReportsLocalAddress`). |
| `isStarted()`, `isStopped()`, `mode()`, `serverConnection()` | state; `serverConnection()` is the client's og-net id once it is up, else 0. |

The callbacks are plain function pointers with one context (`DriverCallbacks`): `accept`, `deliver`, `closed`,
`serverConnectionUp` and `connectEnded`. All five must be set, whatever the role, because one driver class serves both roles.

## §3 `send` and its verdicts

| case | verdict | counted in |
|---|---|---|
| the core is not started, or has stopped | `Dead` | `sendsDead` |
| client, server connection not up, not ended: room in the buffer | `Buffered` | `preConnectBuffered` |
| the same, buffer full | `BufferFull` (the new packet is dropped, the oldest kept) | `preConnectDropped` |
| the same, packet over 1,189 B | `Refused` | `sendsRefused` |
| client after its connection ended (§5, §7) | `Dead` | `sendsDead` |
| an id that is not a live connection of the core | `Dead` | `sendsDead` |
| a live id, the engine lane did not take the packet, and og-net reports the connection not connected | `Dead` | `sendsDead` |
| a live id, the engine lane did not take the packet, and og-net still reports the connection connected | `Refused` | `sendsRefused` |
| a live id, and the lane took it | `Sent` | — |

- **`Dead` covers every id that is not live** (closed by the peer, passed to `engineClosed`, rejected, still pending, or never
  known), not only the ids the core has closed. The engine sends only on connections it has: its connections are created from
  `accept`, so an id it never knew does not reach `send`. A send after a close is normal engine behaviour, and it must not count as
  a refusal: `sendsRefused` means "the lane dropped a packet", and the Ph2 gate's transport-health check expects it to stay 0.
- **A connection og-net has closed before the core knows it.** The core's record of a connection stays live until the ring's
  `Disconnected` for it is polled (§6), but og-net closes the connection on its own servicing thread first. On a graceful peer
  close the engine, handling the peer's close packet from `deliver`, closes its own connection and sends its close reason
  (P4-5's server log: UNetConnection::Close, then Sending CloseBunch) before the `Disconnected` behind that packet is dispatched;
  og-net has already dropped the connection, and the lane does not take the packet. That send is a send after a close, so it is `Dead` (P4-5's finding F1: it was counted as `refused=1` on the server's
  `closed` line after every graceful leave). When the lane does not take a packet for a live id, the core asks og-net whether the
  connection is still connected, two ways, and the send is `Dead` if either says no:
  - **the lane's own count.** og-net's reap drops a connection from the engine lane as soon as its state leaves `Connected`
    (`Closing` or `Disconnected`; `EndpointCore.cpp`, `EndpointCore::reap`, `EngineLane::onDisconnected`), and the lane counts a
    send to an id it no longer has in `EngineLaneStats::sendsToUnknownConnection` (`EngineLane::send`). The core keeps the count it
    last read: a failed send that raised it went to a connection og-net no longer has. Only the core sends on the engine lane, so
    the count changes only inside the core's own `send`;
  - **the endpoint's connection state.** `ConnectionStats::state` from og-net's `stats(id)` (an unknown id reads
    `Disconnected`). og-net publishes it every 50 ms (`kStatsPublishSeconds`), so right after the reap it can still read
    `Connected` (`EngineLaneDriver.ASendAfterAGracefulPeerCloseThatTheCoreHasNotPolledYetIsDeadNotRefused` sends in that gap); it
    covers a packet the lane turns down for another reason (too large, or the command queue full) on a connection og-net has
    already closed (`EngineLaneDriver.AnOversizePacketIsRefusedOnALiveConnectionAndDeadOnOneTheEndpointReportsClosed`).

  Both are read only after the lane has turned a packet down, so a sent packet costs nothing more.
- **A refused send is dropped.** The engine's own reliability resends what matters, as after a UDP loss.

## §4 The client's pre-connect buffer

- **Why.** Without a packet handler, the engine sends its first join packet as soon as its pending connection begins the
  handshake (UPendingNetGame::BeginHandshake calls SendInitialJoin at once when the connection has no handler,
  PendingNetGame.cpp, lines 112-125), which is before og-net's handshake has ended; and the engine lane refuses an id it does not
  know (`EngineLane-rationale.md` §1). So the core keeps such packets: 32 slots (`kPreConnectSlots`) of up to 1,189 B, in arrival
  order.
- **On a join, `UOGNetDriver` now sends nothing before up.** It holds its pending game's hello until og-net is up and the frame
  that opened the connection has ended (`OGNetDriver-rationale.md` §14), so on that path the buffer stays empty (P4-13's
  automation test logs `preConnectBuffered=0` on the client's `stop` line). The buffer is the core's contract for any engine send
  before up, and it still serves a connection opened for a notify that is not a pending net game, where the driver keeps the
  engine's own order.
- **When the server connection comes up** (the client's `Connected` from the ring), the core records the id, sends the whole buffer
  through the lane oldest first, and only then calls `serverConnectionUp(id)`. A packet sent from inside that callback is then
  already behind the buffered ones on the wire.
  `EngineLaneDriver.PacketsSentBeforeTheServerConnectionIsUpArriveFirstInOrderAndByteExact` checks both, with one packet sent from
  inside the callback.
- **Full:** the new packet is dropped and counted (`BufferFull`); the oldest are kept, because the engine's handshake packets come
  first (`EngineLaneDriver.AFullPreConnectBufferKeepsTheOldestAndCountsTheDrop`: of 33, the server gets the first 32).
- **Cleared** when the client's connection ends before it was up (`connectEnded`, or `engineClosed` before up): nothing would ever
  send it.

## §5 The handler queue and the connect retries

- **Where a failed connect is reported.** og-net reports a client handshake that ended without a connection (rejected, failed,
  timed out) only to the connection handler, as a `Disconnected` with id 0 (`EndpointCore.cpp`, `EndpointCore::handleClientStep`);
  the engine ring never sees it, because there was never a connection. The handler pushes the reason of each such event into a
  mutex-guarded queue of 64 (`kHandlerQueueSlots`); a full queue drops the event and counts it (`handlerQueueDropped`). Every other
  connection event is ignored there: the ring carries `Connected` and `Disconnected` for real connections, in order with their
  datagrams.
- **`tick` drains the queue first.** While the client is connecting:
  - `ConnectFailed` while `clientConnectionPending` and before the engine's deadline: og-net's connect again (`retries`). og-net's
    handshake gives up after 10 s (`ognet::handshake::kConnectTimeoutSeconds`), while the engine's pending connection lives for its
    own initial-connect timeout (60 s in the engine's base configuration, BaseEngine.ini, line 1764): without the retry, a server that starts listening a few seconds late is never
    reached, though the engine is still waiting.
  - anything else (`Rejected`, `ServerKeyMismatch`, a `ConnectFailed` after the deadline or when the engine no longer waits):
    `connectEnded(reason)`, once. The client is then ended: later queue events are ignored and `send` gives `Dead`.
- `EngineLaneDriver.AClientWhoseConnectFailsBeforeTheDeadlineReconnectsAndALaterServerAcceptsIt` (failure at 10 s, one retry, a
  server started at 15 s accepts) and `EngineLaneDriver.AClientWhoseConnectFailsAfterTheDeadlineEndsOnceWithoutARetry` (deadline
  5 s: one `connectEnded(ConnectFailed)`, no retry, nothing more by 25 s).
- **Beyond the specification:** a client `Connected` that arrives after the client has ended is disconnected at once, so og-net does
  not keep a connection the engine no longer has.

## §6 The ring, in batches

- `tick` polls the engine lane for at most 256 events at a time (`kBatchEvents`) into a preallocated batch of slots
  `{kind, id, reason, recvSeconds, size, bytes[1232]}`. The poll callback only copies. After `poll` has returned, the whole batch
  is noted in the window (§11), then the callbacks run, slot by slot, and the core polls again while a batch came back full.
- **Why not dispatch inside the callback.** og-net forbids `poll` from its own callback (an `OGNET_CHECK`), and the event's bytes are
  valid only inside it (the ring slot is wiped after the callback returns, `EngineLane-rationale.md` §1, §3). Engine code reached
  from a callback can do anything, including stopping the driver. With dispatch inside the callback, a `deliver` that stopped the
  core did not stop the poll: the four remaining datagrams of the batch and the stop's own `Disconnected` were still delivered
  (5 callbacks after the stop, P4-3's arm D). Guard G-01.
- **Per event:**

  | event | server | client |
  |---|---|---|
  | `Connected` | `accept(id)`: **Accept** → a live record, `accepted`; **Reject** → og-net's `disconnect(id)`, `rejected`; **Ignore** → a pending record | the server connection is up: §4 |
  | `Datagram`, live id | `deliver` | `deliver` |
  | `Datagram`, pending id | `accept` again: Accept → live, `accepted`, then `deliver`; Reject → `disconnect`, `rejected`, the datagram dropped; Ignore → dropped. Both drops count in `ignoredDatagrams` | — |
  | `Datagram`, any other id (unknown, or draining after `engineClosed`) | dropped, `unknownDatagrams` | same |
  | `Disconnected`, live id | the record goes, then `closed(id, reason)` | same, and the client is ended |
  | `Disconnected`, pending or draining id | the record goes, no callback: the engine never had, or no longer has, the connection | same |

- **Ignore** is the engine's answer while the server switches levels (UWorld::NotifyAcceptingConnection, World.cpp, lines
  6788-6801). The core asks again on every later datagram (`EngineLaneDriver.AnIgnoredConnectionDropsItsDatagramsUntilItIsAccepted`: three Ignores,
  two datagrams dropped, the rest delivered in order). **Reject** closes the connection from the server, so the client sees
  `closed(ClosedByPeer)` (`EngineLaneDriver.ARejectedConnectionIsClosedByThePeerAndDeliversNothing`).
- `EngineLaneDriver.SixHundredDatagramsQueuedBetweenTwoTicksArriveInOrderInBatchesOf256`: 600 datagrams in one tick, in order, over
  3 batches, `maxBatch == 256`.
- **The record table** holds one entry per og-net connection the core knows (pending, live, or draining), sized to the endpoint's
  connection slots (`maxConnections` on a server, 1 on a client). og-net delivers a slot's `Disconnected` before the next
  `Connected` of that slot (`EngineLane-rationale.md` §4), so the table cannot overflow; `insert` checks it (`OGNET_CHECK`).

## §7 The drained close

- `engineClosed(id, now)` marks the record draining and stamps the time; it does not disconnect. Each `tick` then calls og-net's
  `disconnect(id)` once `queuedBytes(id) == 0` (`closesDrained`), or once `now` is 0.2 s past the `engineClosed` stamp (`closesTimedOut`).
- **Why.** When the engine closes a connection it sends a last packet first: UNetConnection::Close sends the close reason and
  flushes (NetConnection.cpp, lines 1106-1119). A closing og-net connection seals its
  Close frame first in every datagram it still sends (`Connection.cpp`, `Connection::sealNext`). Disconnecting in the same tick as
  that last send lost it: with an immediate disconnect the client received no packet before its `closed` (P4-3's arm B,
  `EngineLaneDriver.ALastPacketSentWithEngineClosedReachesThePeerBeforeItsClose`). Guard G-02.
- **0.2 s** is the Engine lane's queue age limit: a packet still queued after it has been dropped by the lane anyway, so waiting
  longer gains nothing (the `static_assert`).
- After `engineClosed`, `send` to the id gives `Dead`, its datagrams count as unknown, and its og-net `Disconnected` brings no
  `closed` callback: the engine has already cleaned it up.
- **Beyond the specification:** `engineClosed` on a client whose server connection is not up yet (the engine dropped its pending
  connection, with id 0) ends the client: the buffer is cleared, no retry follows, and a late `Connected` is disconnected (§5).

## §8 Stop and re-entrancy

- `stop(now, drainSeconds)` marks the core stopped, then in `OwnThread` mode polls `queuedBytes` of every live and draining
  connection every 1 ms (`kStopDrainPollInterval`) until all are 0 or `drainSeconds` have passed on the host's clock, then calls
  og-net's `stop` (which in own-thread mode waits for og-net's thread to exit). In `HostServiced` mode there is no drain: nothing
  services the endpoint while the caller waits in `stop`. A second `stop` does nothing.
- **A callback may call `send`, `engineClosed` or `stop`.** `send` from `serverConnectionUp` is §4's case. After a `stop`, no
  further callback of that `tick` runs: the batch loop checks after every event (guard G-04), the handler queue and the drained
  closes stop at it. Without the check, a stop from `deliver` was followed by four more `deliver`s from the same batch (P4-3's arm E,
  `EngineLaneDriver.ADeliverThatStopsTheCoreRunsNoFurtherCallback`).
- `EngineLaneDriver.OnItsOwnThreadEachSideSendsFiveHundredInOrderAndStopDrainsTheLastPacket`: real threads, a 10 ms tick, 500
  packets each way in order; the client sends its last packet and calls `stop(…, 0.2)` in the same frame, and the server delivers
  that packet before its `closed(ClosedByPeer)`.

## §9 `DriverStats`

`accepted`, `rejected`, `ignoredDatagrams`, `unknownDatagrams` (§6); `preConnectBuffered`, `preConnectDropped`, `sendsRefused`,
`sendsDead` (§3, §4); `retries` (§5); `closesDrained`, `closesTimedOut` (§7); `handlerQueueDropped` (§5, read under the handler mutex); `batches`
(non-empty polls) and `maxBatch` (§6). The driver's `closed` line and Ph2's transport-health check read them; `sendsDead` is on
no log line.

## §10 Tests

`EngineLaneDriverCoreTest.cpp` (`[OGNet][EngineLaneDriver]`, run by `OGNetTests`). The rig is og-net's `VirtualNetwork` in virtual
time with `TestHost`, both cores in `HostServiced` mode over `SwitchableBackend<VirtualBackend>` without an emulator: each 1 ms step
calls `serviceUntil(now)` on every started core, then `tick(now, …)` on every core that ticks. The steady case runs real threads in
`OwnThread` mode on the network's steady clock, with a 30 s give-up. One more case runs a client whose emulator drops every
outbound datagram: the server never sees a datagram (`EngineLaneDriver.AClientWhoseEmulatorDropsEveryOutboundDatagramNeverReachesTheServer`). The window cases are in §11.


## §11 The `[OGNet.Window]` lines

The core owns one `ognet::NetWindow` (og-net's `NetWindow-rationale.md`): every og-net run of the driver, server and client,
writes og-net's window lines, so a capture can attribute a transport difference to a connection, a lane or the endpoint. Ph2's
gate requires a `[OGNet.Window]` line from every og-net process (task P4-9).

- **Built** at a successful `start`, with the endpoint's role and og-net's default window (`NetWindowConfig`, 10 s). Its
  connection table is allocated there, so `tick` still allocates nothing (§1).
- **Fed from the engine poll,** on the tick thread. After each `poll` returns and before any callback runs, every event of the
  batch is noted:
  - a `Datagram` as `noteDelivered(Engine, recvSeconds, polled)`, where `polled` is the host's clock read once after the
    `poll`. og-net stamps `recvSeconds` with the same host clock when its datagram arrives (`EndpointCore.cpp`,
    `EndpointCore::onDatagram`), so the residence is the time the datagram waited in the ring for the engine's poll;
  - a `Connected` or `Disconnected` as a `ConnectionEvent` with its id (and reason), and no address.

  Every polled event is noted, whatever the engine's verdict (pending, rejected, draining or unknown): the window reports og-net's
  connections, not the engine's. Noting the batch before dispatching it means a callback that stops the core cannot hide the
  rest of the batch from the window.
- **What it does not see.** The ring carries no address, so a connection line prints the address og-net publishes for the
  connection (`NetWindow-rationale.md` §4), which follows a roam without an `AddressChanged` event. A connection that came and
  went before the ring could hold its `Connected` is elided by the lane (`EngineLane-rationale.md` §4) and has no connection
  line; the Endpoint line still counts its traffic. The handler queue's events (§5) are not noted in the window: they all carry
  id 0, which the window ignores.
- **The sim lanes' residence** is noted from outside: the sim endpoint's `noteDelivered` (§12) adds one delivered sim message to the
  same window, so a capture reads the sim lanes' `[OGNet.Window.Rx]` lines beside the Engine lane's. The core does not see sim
  messages itself: og-net hands them to the sim lanes' handlers on its own thread.
- **Closed** at the end of every `tick` while the core runs: `closeIfDue(now, endpoint, sink, {engine})`. The first `tick`
  starts the first window. `engine` is a copy of `engineLaneStats()`, read on every tick (a lock and a copy), because the extras
  are passed before the window knows whether it is due. After `stop` no window is closed, so the last partial window is not
  written.
- **The sink** is the host's `log` at `Info`. In the Unreal driver that is `LogOGNetTransport` at `Log`
  (`OGNetUnrealHost-rationale.md`), the category of the driver's own `[OGNet.Driver]` lines.
- **No emulator lines.** og-net's emulator keeps its counters as plain fields written on og-net's own thread, and
  `emulatorStats()` copies them without a lock, so reading them on the tick thread in `OwnThread` mode would be a data race. The
  window is given no emulator extras, and `[OGNet.Emulator.Window]` lines do not appear.
- **One thread.** The window fixes its host thread at its first `noteConnection` or `closeIfDue` and checks it on every later
  call (`OGNET_CHECK`): the tick thread of §1.

Tests (§10's rig):

| Case | What |
|---|---|
| `EngineLaneDriver.EachSideWritesOneNetWindowBlockPerWindowThroughItsHostLogFedByTheEnginePoll` | 25 s, Engine datagrams both ways for 15 s: each side writes 2 windows, each with its one `[OGNet.Window]` line (`seconds=10.00`, `Connected`; the client's `peer=` is the server's address), its Engine lane line and one Endpoint line with the `engine.` fields; the Rx residence samples equal the datagrams delivered; every window line is at `Info` |
| `EngineLaneDriver.TheWindowsEngineResidenceIsHowLongADatagramWaitedForThePollAndAClosedConnectionIsPrintedOnce` | the server stops ticking while the client sends 10 datagrams, then ticks 24 ms later: all 10 residences fall in the 16–32 ms bucket (`residenceP99Us=32000`); the client's `engineClosed` closes the connection, and each side prints it once, `state=Disconnected`, then two Endpoint lines and no further connection line |

## §12 The sim lanes: the attach, the sim endpoint and the forward

og-net takes lane handlers and its one connection handler before the endpoint starts only (`EndpointCore.cpp`,
`EndpointCore::setLaneHandler` and `EndpointCore::setConnectionHandler`, each an `OGNET_CHECK`), and the core starts the endpoint
inside `start`. So the sim side (og-sim-net's `SimNetTransport`, in a host such as the Unreal driver's sim lanes,
`OGNetSimLanes-rationale.md`) can only install its handlers from inside `start`. Task P5-3, design P5 K1.

- **The attach.** `start` takes an optional `const SimLaneAttach*`: a context and `beforeStart(ctx, SimLaneEndpoint&)`. In order:
  the core sets its own connection handler, takes the engine lane, calls `beforeStart` with its sim endpoint, then starts the
  endpoint. Without an attach, `start` is exactly P4-3's.
- **The sim endpoint** (`SimLaneEndpoint-rationale.md`) is a member, built in the constructor over the core's endpoint:
  - `send`, `broadcastExcept`, `disconnect`: og-net's, and nothing once the endpoint is gone (a failed start);
  - `setLaneHandler`: og-net's, and only inside `beforeStart` (an `OGNET_CHECK`); the Engine lane is refused by the endpoint itself;
  - `setConnectionHandler`: stores the **forward target**, only inside `beforeStart` (an `OGNET_CHECK`). og-net has one connection
    handler, and the core's must stay installed for §5;
  - `noteDelivered`: into the window (§11), once it exists;
  - `config()`: the config `start` was given, set before the attach runs;
  - `serviceUntil` and `hasHighResolutionWait`: the core's own (§13), so a host's runner holding only the sim endpoint can service
    a host-serviced core and read its backend's wait resolution (task P5-18).
- **The forward.** The core's connection handler first passes **every** event, any kind and any id, to the forward target, and only
  then applies its own filter (id-0 `Disconnected` into the handler queue, §5). The sim transport needs the real connections' events
  (its per-connection slots and its inbox's `Connected`/`Disconnected`), which the core's own filter drops; and it needs them in
  order with the lane messages, which og-net raises on the same servicing thread: a connection's `Connected` before its first
  message. Guard G-05. The forward runs on og-net's servicing thread (§1), so the target must take that thread (P2-7 F3:
  `SimNetTransport::onConnectionEvent` only pushes into its inbox).
- **Lifetime.** The forward target and the lane handlers' contexts must outlive og-net's servicing thread. In `OwnThread` mode the
  thread is joined when the endpoint is destroyed (§1), so the host destroys whatever owns them after the core
  (`OGNetDriver-rationale.md` §13). In `HostServiced` mode the servicing thread is the host's own, which joins it before the
  core's `stop` (§13).

Tests (`SimLaneSeamTest.cpp`, `[OGNet][EngineLaneDriver]`, §10's rig with the attach added):

| Case | What |
|---|---|
| `EngineLaneDriver.ARecordingAttachInstallsTheFiveSimLaneHandlersBeforeStartAndEachLaneDeliversWithItsReceiveTime` | both sides install handlers on Control, SimClock, SimInput, SimRelay and SimState in `beforeStart`; the client sends on Control, SimClock and SimInput, the server on Control, SimClock, SimRelay and SimState (each lane in its og-net direction); each arrives once, byte-exact, with a `recvSeconds` after the send; no engine `deliver` |
| `EngineLaneDriver.EveryConnectionEventIsForwardedAndConnectedPrecedesItsConnectionsFirstMessage` | a message sent at the client's up: each side's handler sees exactly `Connected`, the message, `Disconnected` for its connection |
| `EngineLaneDriver.AFailedConnectsIdZeroDisconnectedIsForwardedAndStillEndsTheEngineConnectOnce` | the id-0 `Disconnected(ConnectFailed)` is forwarded, and §5 still ends the connect once |
| `EngineLaneDriver.OnItsOwnThreadTheForwardRunsOnOGNetsThreadBeforeTheConnectionsFirstMessage` | real threads: the forward's thread is not the test's, and is the thread that delivers the lane message |
| `EngineLaneDriver.AnAttachedSimNetTransportReceivesEachSimLaneIntoItsInboxWithConnectedFirst` | a `SimNetTransport<SimLaneEndpoint, 128>` sized from `config()` and installed through the attach: each side's inbox drains its `Connected` first, then one message per lane; the server's sends leave at once (the transport's frame hold went in og-session-runtime task 15) |
| `EngineLaneDriver.NoteDeliveredOnTheSimEndpointWritesTheSimLaneRxLineOfTheCoresWindow` | seven `noteDelivered(SimInput, now - 20 ms, now)` give one `[OGNet.Window.Rx] role=Server lane=SimInput` line with 7 samples, `residenceP99Us=32000` |
| the nine `EngineLaneDriver.WithAndWithoutAnAttach.*` cases | P4-3's nine scenarios (§4 to §8 and §10's emulator and steady cases), each run without an attach and with a recording one; with it, the forward also sees the scenario's connection events |

## §13 `HostServiced` mode: a host's thread services the endpoint (task P5-18)

Ph4's server runs its simulation on a runner thread of its own and services og-net from it, so the runner, not the tick thread's
frame, decides when datagrams are read (design P5 K14, og-net's mode B). The core already took the mode at `start`; task P5-18 lets a host that holds only
the sim endpoint (§12) do the servicing.

- **What `start` does.** In `HostServiced` mode `start` does not call `startOwnThread`: no og-net thread exists, and nothing
  services the endpoint until the host calls `serviceUntil`. Everything else is the same as in `OwnThread` mode: the tick thread's
  `tick` still polls the engine lane (§6), and every other member still runs on the tick thread (§1).
- **One servicing thread.** og-net fixes the servicing thread at the first `serviceUntil` and aborts on a call from any other
  thread afterwards (`EndpointCore.cpp`, `EndpointCore::fixServicingThread`, an `OGNET_CHECK`). So one host thread services the
  endpoint for its whole life, and the tick thread never does. That thread is also where the connection handler, the forward
  (§12) and the lane handlers run.
- **The sim endpoint's two operations.** `SimLaneEndpoint::serviceUntil` is the core's `serviceUntil` (the mode check included);
  `SimLaneEndpoint::hasHighResolutionWait` is the core's `hasHighResolutionWait`, which reads the backend the core was started with.
  Over the Unreal driver's `SwitchableBackend<BsdDatagramBackend>` that is the BSD backend's answer, forwarded by the switch
  (`SwitchableBackend-rationale.md` §2); a backend without the member reports false. The backend pointer is written by `start`,
  before any host thread can hold the sim endpoint.
- **Stop.** `stop` does not drain in this mode (§8), and og-net's `stop`, called on the tick thread, aborts if `serviceUntil` is
  running on another thread at that moment (`EndpointCore.cpp`, `EndpointCore::stop`). The host therefore joins its servicing
  thread before calling `stop`. og-net's `stop` then closes each live connection on the calling thread, so the forward target sees
  their `Disconnected(LocalClose)` on the tick thread, after every earlier event on the servicing thread.

Tests (`SimLaneSeamTest.cpp`, `[OGNet][EngineLaneDriver]` unless stated):

| Case | What |
|---|---|
| `EngineLaneDriver.AHostServicedServerServicedOnlyByARunnerThreadDeliversEngineAndSimLaneTraffic` | real threads on the steady network: a `HostServiced` server whose only servicing is a runner thread calling `SimLaneEndpoint::serviceUntil` in 2 ms slices, and an `OwnThread` client. One engine packet and one sim message cross each way (engine both ways, SimInput up, SimState down); the server's `Connected` and SimInput arrive on the runner thread; the runner never saw `stopped`. After the runner has joined, the server's `stop` on the test thread delivers `Disconnected(LocalClose)` on the test thread. The backend (`VirtualBackend`) has no high-resolution wait. |
| `EngineLaneDriver.Death.ASecondThreadsServiceUntilOnAHostServicedEndpointAborts` (`[.][OGSimDeath]`, hidden) | a runner thread services once and joins; the test thread's `serviceUntil` then aborts on og-net's "called from a second thread" check |
| `EngineLaneDriver.TheSimEndpointReportsTheBackendsHighResolutionWaitThroughTheSwitch` | over a test backend that reports `true` (then `false`) behind `SwitchableBackend`, the core and the sim endpoint both report it |
