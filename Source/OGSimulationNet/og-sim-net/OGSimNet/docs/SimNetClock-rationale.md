<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetClock.h` — rationale

`SimNetClockServer<TransportT, ClockT>` and `SimNetClockClient<TransportT, ClockT>` are the SimClock ping/pong: a clock
source for the client's prediction clock that is measured on og-net's servicing threads, not through the engine. The client
sends `ClockPing {t0}` at 20 Hz; the server answers each one where it arrives with
`ClockPong {t0, t1, t2, serverTick, serverTickAgeUs}`; the client stamps `t3` where the pong arrives and pushes one
`clockSample {rtt, theta, serverTick, serverTickAgeSeconds, t0..t3}` record into its transport's inbox, for the drain to
feed the estimator's shadow. The header states no prohibition, so it has no guards
document: the rules it depends on are compile-time checks and `OG_CHECK`s.

**If this file and `SimNetClock.h` disagree, the header is authoritative and this file is stale.**

---

## §1 Why a clock of its own

Until og-input-latency task P3-10's flip the client's estimator took its RTT from the engine's ping (UE's `FNetPing`), read by
the host's `serverRttSeconds()` when a `ClockPublish` arrives (`SimNetClient-rationale.md`); since the flip it takes it from
these samples once the first one arrives (og-simulation `TimeConfig-rationale.md` §15). Since the engine rides og-net's engine lane, that ping is an ack
round trip through the engine, so it carries the server's frame turnaround: in og-input-latency's Ph3 measurements it rose
by about 4 ms on loopback (11.4 to 15.4 ms) while og-net's own round trip stayed at 0.17 ms. A larger RTT moves the client's
lead target up, which is input latency.

The SimClock pair removes the turnaround from the measurement: the server answers on the thread that received the ping, in the
same og-net service pass, and says how long it held it (`t2 − t1`). The sample is

- `RTT = (t3 − t0) − (t2 − t1)`
- `theta = ((t1 − t0) + (t2 − t3)) / 2`

(`simNetClock::sampleOf`; a `static_assert` pins both on exact binary values). `theta` is the server clock minus the client
clock under the usual NTP assumption that both directions take equally long. With unequal one-way delays it is off by half
their difference, `(up − down) / 2`; no two-way method can see that difference. The minimum-RTT sample of a window is the
least affected, which is why the consumer takes its offset from it.

## §2 The messages

| Message | Lane, direction | Layout | Bytes |
|---|---|---|---|
| `ClockPing` (kind 5) | SimClock ↑ | envelope header, `t0` | 10 |
| `ClockPong` (kind 6) | SimClock ↓ | envelope header, `t0`, `t1`, `t2`, `serverTick:u32`, `serverTickAgeUs:u32` | 34 |
| clockSample record | never on the wire | version byte 2, kind 6, `rtt`, `theta`, `t0`, `t1`, `t2`, `t3`, `serverTick:u32`, `serverTickAgeSeconds` as `u32` µs | 58 |

- **Stamps** are seconds on the sending host's og-net clock, written as the 64 bits of an IEEE 754 double, little-endian.
  `t0` and `t3` are the client's clock, `t1` and `t2` the server's; the two clocks need not agree, which is what `theta`
  measures. `t0` travels to the server and back unchanged, so the client keeps no table of pings in flight: a pong is a
  sample on its own, and a lost ping or pong costs one sample.
- **`serverTick`** is the last tick the authority published with `SimNetClockServer::publishServerTick`, read when the pong is
  written. Before the first publish it is `kNoServerTick` (`UINT32_MAX`), so a consumer can tell a pong from before the
  authority's first step.
- **`serverTickAgeUs`** (og-time-dilation task 2) is the server's time from the start of the step that produced `serverTick`
  to sending the pong (`t2`), in whole microseconds, little-endian right after the tick (§5; measured from the publication
  until task 2b). It is 0 when the tick is `kNoServerTick`. The sample
  carries it as `serverTickAgeSeconds` (`serverTickAgeUs · 10⁻⁶`); the record carries the same microseconds back, so a
  record reads back bit for bit (a `static_assert` checks it at seven ages from 0 to `UINT32_MAX`). Why a consumer needs it: the
  authority publishes once per step and the server answers a ping whenever it arrives, so without the age the tick's
  phase at `t2` is unknown in [0, 1) tick. On a jittery link that averages out; on loopback a client's pings can keep a
  fixed phase to the server's steps, and each client would then carry its own constant bias of up to a tick
  (og-time-dilation design §2.2). The age removes it for 4 bytes.
- **The clock's wire version** is `ogsimnet::kClockWireVersion`, 2 since the age; 1 was the 30-byte pong without it. The
  pong has no version byte of its own: `readPong` refuses any length but `kPongBytes`, so a 30-byte pong reaching a new
  client, or a 34-byte pong reaching an old one, is dropped and counted (`pongsMalformed`) and that client never gets a
  sample. The version therefore joins the brawler's build-identity wire token as `.ck2` (`OGBuildIdentity-rationale.md`
  §8 in the brawler), so a server refuses a client of the other clock layout at login. og-simulation's
  `kSimEnvelopeVersion` is unchanged: the pong's layout is og-sim-net's own.
- **The envelope refuses both kinds.** `simEnvelope::decode` has no body layout for kinds 5 and 6 (`hasBodyLayout`), so a
  session's drain that meets a raw ping or pong counts a bad decode and applies nothing. The clock's handler (§3) is their
  only reader, and the header's `static_assert` keeps it that way.
- **The record is not a message.** Its version byte, 2, is one no envelope carries (`kSimEnvelopeVersion` is 1), so the
  envelope refuses a record (`Version`) and a record cannot be mistaken for a pong. It was 0 before the age; the next
  value, 1, is the envelope's, which the header's `static_assert` refuses, so the age took 2. A message from the network that is shaped
  like a record is dropped by the client's handler and counted (`forgedRecordsDropped`): only the handler writes records.
  `simNetClock::readSample(record)`, or the client's `readSample`, turns an inbox record back into a `SimNetClockSample`, and
  returns nothing for any other record.
- **Malformed** pings and pongs (the right header, the wrong length) are dropped and counted (`pingsMalformed`,
  `pongsMalformed`).
- **Size and rate.** A pong is inside the 40 B the envelope table gives the clock kinds. og-net caps the SimClock lane at 100
  received messages a second per connection (`LaneConfig.h`'s default table). Up, a client sends 20 pings; down, a server sends
  20 pongs and its `ClockPublish`, which goes out only when the tick changed, so at most one per 60 Hz tick: 80 a second.

## §3 The handler: the SimClock lane's own

The transport's copy-only lane handler does nothing but copy into the inbox (`SimNetTransport-guards.md`, G-01). Answering a
ping on the servicing thread needs a handler that decodes, so the clock takes the SimClock lane with a handler of its own:

- **Construction installs it.** `SimNetClockServer` and `SimNetClockClient` call the transport's `setClockHandler` from their
  constructors, and `installHandlers` then gives the SimClock lane that handler in place of the copy-only one. The other four
  lanes keep the copy-only handler. The clock object is therefore built **before** `installHandlers` and `start`, and lives as
  long as the endpoint can call it. The transport `OG_CHECK`s both orders: a clock handler set after `installHandlers` (or
  after the endpoint started) and a second clock handler on one transport each fail a check
  (`SimNetClock.Death.AClockHandlerSetAfterInstallHandlersFiresTheCheck`, `SimNetClock.Death.ASecondClockHandlerFiresTheCheck`).
- **Only the clock can reach the transport's clock members.** `setClockHandler`, `admitsClockFrom`, `receiveClock` and
  `sendClockNow` are private to `SimNetTransport`, which befriends the two clock class templates. A session cannot send an
  unheld message or push into the inbox from the wrong thread through them.
- **Everything else still reaches the inbox.** A SimClock message the clock does not own (a `ClockPublish`, or anything the
  sessions refuse today) is handed to `receiveClock`, which is the copy-only path itself (`receive` with the SimClock lane):
  the same slot, closing and announcement checks, the same quota, the same receive time. Counted `passedThrough`.
- **One producer.** The handler runs on og-net's servicing thread, like every lane handler, so the inbox still has one
  producer thread (`SimNetTransport-rationale.md` §4).

## §4 The server: the pong at receipt

`SimNetClockServer::onMessage`, on the servicing thread:

1. A ping is admitted only from a connection the copy-only path would admit (`admitsClockFrom`: the connection has a slot, is
   not closing, and was announced). A refused ping is counted (`pingsRefused`, and the transport's own refusal counter) and
   not answered.
2. `t1` is og-net's receive time for the datagram (`recvSeconds`, `EndpointCore.cpp`'s `now()` when the datagram was read).
3. `serverTick` and its step's start time are read from the authority's atomic in one load (§5).
4. `t2` is read from the clock last, just before the pong is written, so `t2 − t1` is the time the handler held the ping.
   `serverTickAgeUs` is then the low 32 bits of `t2` in microseconds minus the start time's, an unsigned subtraction
   that wraps (§5), or 0 when the tick is `kNoServerTick`. It is measured to `t2`, not `t1`: the consumer adds the time
   from `t2` onwards itself, as half the RTT (og-simulation's `ServerTickEstimator`, og-time-dilation task 3), and the RTT
   excludes `t2 − t1` (§1), so half of it is the pong's flight from `t2`, not from `t1`.
5. `sendClockNow` passes the pong straight to the endpoint's `send` and counts it in `clockSends`, not in the session's
   `directSends` (`SimNetTransport-rationale.md` §7, §13). Until og-session-runtime task 15 a frame-locked transport held
   every session send until the frame flush, and the pong bypassed that hold, which would have added the frame turnaround the
   clock exists to remove. og-net flushes the commands a handler queues in the same service pass that delivered the
   datagram (og-net's `EndpointCore-guards.md`, G-09), so the pong leaves in that pass.

The clock needs no session and no host: it touches only the transport, its clock and its atomics.

## §5 The authority's tick

`publishServerTick(tick, tickStartSeconds)` stores the tick and the time its step started together in one
`std::atomic<uint64_t>` with release order: the tick in the high 32 bits, and in the low 32 bits the start time's
microseconds (`std::llround(tickStartSeconds · 10⁶)`) truncated to 32 bits. It does not read the clock. The handler loads
the word once with acquire order, so a pong's tick and its age always come from the same publish; the header
`static_assert`s the 64-bit atomic is lock free. Before the first publish the word holds `kNoServerTick` and 0.
`publishedServerTick()` returns the high half. The low half wraps every 2³² µs (about 71.6 minutes), and the age is the
wrapped difference, so it is exact while the age itself is under that: always, since the authority publishes every step.

**The age is measured from the tick's step start, not from its publication** (og-time-dilation task 2b). The authority
publishes a tick after the step that produced it, so a publish time sits a step's duration D after the start. The consumer
places tick n at `t2 − age` on the server's clock, and the server's own timeline (each step's start, the time the Unreal
host prints as `stepSeconds=` on that step's `[AuthoritySimulation]` line and on its `[Release]` lines)
places it at the start. With the publish time the estimate read D · f ticks low on that timeline. D is the step's cost and
grows with the server's load; on the Unreal brawler server's smokes of og-time-dilation tasks 8 and 2b it was typically
70–75 µs (the median 10-second window's p50; its p99 was 115–128 µs) and 237 µs at worst (about 0.0045 and 0.014 tick at
60 Hz). With the start time that part is 0 by construction. The caller passes the start time because only it knows when the step began.
It must be on the same clock as `t2` (the server clock's `nowSeconds`, `FPlatformTime::Seconds` in the Unreal host), and
no later than any `t2` that can read it: the caller takes it before the step and publishes after, so it always is.
Until task 2b the function took only the tick and read the clock at the publish; a test-file `static_assert` checks that
the one-argument form no longer compiles.

The authority publishes after each step, on its own thread; the handler on the servicing thread never waits for it. So the
server's clock is read on two threads, by the publisher for the start time and by the handler (`t2`) on the servicing
thread: a `ClockT` must be callable from both (a stateless clock is). Nothing in og-sim-net calls `publishServerTick`: its
host calls it after each authority step, with the tick it just stepped and that step's start time.

## §6 The client: pings, and the sample at receipt

- **`servicePing()`** sends a ping if one is due and the transport has a server connection (counted `pingsWithoutServer`
  otherwise). Pings are due every `kClockPingIntervalSeconds` (50 ms, 20 Hz) from the previous one; after a gap longer than an
  interval (a stalled caller) the next ping is due one interval after the late one, so a stall sends one ping, not a burst.
  The due time is one lock-free `std::atomic<double>` updated by compare-and-exchange (the header `static_assert`s it is lock
  free), so two threads that call `servicePing` at once send one ping. `t0` is read from the clock just before the ping is
  written. The ping goes through `sendClockNow`, unheld, like the pong. The caller can be any thread: og-net's `send` takes
  sends from any thread and wakes its servicing thread. og-sim-net itself never calls `servicePing`: the host does (the Unreal
  host once per client frame, from the driver's first frame; og-input-latency task P3-10).
- **The handler** (servicing thread) turns each pong into a sample with `t3 = recvSeconds`, og-net's receive time for the
  datagram, and pushes the record through `receiveClock`, with the same receive time. So the sample's RTT is independent of
  when the drain runs: a record drained a frame later reads the same as one drained at once
  (`SimNetClock.WithFixed20And30MsOneWayLagsTheRttIs50MsAndThetaIsTheOffsetPlusHalfTheAsymmetry` drains once a frame).

## §7 What the consumer does with a sample

The record is the transport half of og-input-latency's P3 §7.1 clock-source switch. The consumer (P3's task):

- logs the shadow window beside the engine's RTT, then switches the estimator's RTT source with a knob, without resetting it;
- takes `theta` from the window's minimum-RTT sample, and a one-way delay variation per direction (RFC 3550) from the raw
  stamps the record carries (`t1 − t0` and `t3 − t2` move with their own direction's delay plus a constant offset);
- reads `serverTick` with its own receive time (`t3`);
- since og-time-dilation task 2 the sample also carries `serverTickAgeSeconds`; since that initiative's task 8 the Unreal
  session copies `serverTick` and the age into og-simulation's `SimClockRttSample`, whose server-tick estimator reads them.

**The client session reads records ahead of its decode** (og-input-latency task P3-10). `SimNetClient`'s drain calls
`simNetClock::readSample(record)` on every message record before `simEnvelope::decode`, which would refuse a record
(`Version`) and count a bad decode, and hands a sample to the host's optional `onClockSample` (`SimNetHost-rationale.md` §4b,
`SimNetClient-rationale.md` §4). A host without the hook gets the samples counted and dropped. Before that branch existed a
host had to drain the records out of the inbox ahead of the session, which `SimNetClock.TheClockNeverTouchesTheEnginePingPathWhileTheSourceIsEngine`
still does through its tee (its session never meets a record either way). With no clock installed, no record exists and the
SimClock lane is the copy-only path, byte for byte.

## §8 Counters

`SimNetClockServer::stats()` and `SimNetClockClient::stats()` return `SimNetClockStats`, relaxed reads of atomics: the client's
`pingsSent`, `pingsWithoutServer`, `pongsReceived`, `pongsMalformed` and `forgedRecordsDropped`; the server's `pingsAnswered`,
`pingsRefused` and `pingsMalformed`; and each side's `passedThrough`. The fields the other side owns read 0. The transport counts
every clock send in `clockSends` (`SimNetTransportStats`); clock sends are not `directSends`, which counts the sessions'
unheld sends.

## §9 Tests

`tests/SimNetClockTest.cpp`, `[NetTransport]`, og-net's `VirtualNetwork` in virtual time with an `EmulatedBackend` on the
client giving fixed one-way lags, and the client's host clock offset from the server's. The rig steps the clock by 2⁻¹⁵ s, so
a delayed datagram is released at most one step late in each direction (under 0.061 ms on a round trip), and drains the inbox
once per 60 Hz frame:

- 20/30 ms lags, client clock 750 ms behind: every sample's RTT is 50 ms ± 0.1 and `theta` is 750 − 5 ms ± 0.1 (the
  asymmetry term of §1); `t2 − t1` is under 1 ms (0 in virtual time); `serverTick` is the tick published before `t1`;
- 25/25 ms lags: `theta` is the configured offset ± 0.1 (750 ms, −312.5 ms and 0);
- a server that never runs its session answers every ping at receipt, with the same samples, and counts each pong as a clock
  send, none as a session send (`directSends` 0);
- the client pings at 20 Hz (39 to 41 pings in 2 s), and after a 300 ms stall sends one ping, not a burst;
- before the first publish, every pong carries `kNoServerTick`;
- every sample's `serverTickAgeSeconds` is its pong's `t2` minus the start time its tick was published with, within 1 µs, at
  least 0 and under one tick plus two rig steps
  (`SimNetClock.EachSamplesServerTickAgeIsThePongsSendTimeMinusTheStartTimeItsTickWasPublishedWith`). The rig passes the
  server clock's time at the publish as the start time, as the one-argument publish read it before task 2b;
- a `ClockPublish` on the SimClock lane still reaches the inbox byte for byte; a record-shaped message and a short pong from the
  network are dropped and counted;
- with real `SimNetServer` and `SimNetClient` sessions and the engine's RTT read through the client host, the clock on and off
  give the same `onTimingInfo(tick, rtt)` sequence, the same number of engine RTT reads (`serverRttSeconds`, `rttMs`), the
  same session messages and no bad decode (the test's drain takes the records first, §7);
- hidden `[.][OGSimDeath]`: a clock handler set after `installHandlers`, and a second clock handler, each fail the transport's
  check.

`tests/SimNetClockSampleRouteTest.cpp`, `[NetTransport][SimNetClockSample]`, drives a real `SimNetClockServer` through a
capturing endpoint and a manual clock (og-time-dilation task 2):

- the pong codec writes the age little-endian at bytes 30 to 33, right after the tick, and reads all five fields back; a
  30-byte pong is refused;
- a tick whose step started at 10.000000 s, published at 10.001500 s, with a ping received at 10.004000 s and answered at
  10.004321 s, carries an age of 4321 µs. An age taken from `t1` instead would read 4000 (seen failing, task 2), and one
  taken from the publish call's time 2821 (seen failing, task 2b);
- the one-argument `publishServerTick(tick)` does not compile and the two-argument one does (a `static_assert`, task 2b);
- published at microsecond 2³² − 100 and answered 250 µs later, the age is 250 (the wrap);
- before the first publish the pong carries `kNoServerTick` and an age of 0;
- the sample record carries `serverTickAgeSeconds` bit for bit, standalone and through the client session to the host's
  `onClockSample`.
