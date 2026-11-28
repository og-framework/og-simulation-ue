<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetTestRig.h` — rationale

`SimNetTestRig.h` is og-sim-net's shared test fixture: a one-simulatable game, og-net peers over `VirtualNetwork.h`, recording
hosts for both sessions and the constants the rigs of three test files share. The header carries no comments; this is its only
rationale, and there is no guards file (it states no prohibition). Until og-session-runtime task 15 it also held the frame-locked
equivalence suite's rig and reference, which this file recorded; §3 keeps what that suite measured.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What the rig holds, and who uses it

- **The game:** `EqSim` (a one-field state, `EqPosition`, and a two-field input, `EqInput`) and `EqGame`, whose
  `Simulatables` list holds `EqSim` alone. It is the rig's own simulatable, not the mocks of
  `MockSimulatablesForNetworkTests.h`: those (`MockSimA`, `MockSimB`) declare only an `Input` alias, with no state type and no
  state accessors, so `SimulationNetSync` cannot register them. The client session also accepts a game with exactly one
  simulatable type (`kSimulatableCount`).
- **The peers:** `Transport` (`SimNetTransport` over og-net's `NetEndpoint` on an `EmulatedBackend` of a `VirtualBackend`,
  2,048 inbox slots) and `NetPeer`, which owns one endpoint, its transport and its host services, starts it in a role and
  services it in passes.
- **The hosts:** `ServerHost` and `ClientHost` record their log lines (the client's also its clock ticks, and it calls back on
  a roster add); a `static_assert` checks each against its session's host concept. `ServerTee` wraps a server transport so a test can count the input bundles a drain sees
  per character (`ServerFrame`).
- **The script's constants:** the three characters (`kCharacters`: client A's local players 0 and 1, client B's local player 0),
  the two clients' RTTs (30 and 90 ms, so the connections sit in different delay tiers), the tick, the start time, the setup
  step, the send's redundancy (`rigSendConfig`) and the frame's tick count (`ticksInFrame`, 1 to 3).
- **Its users:** `SimNetAuthorityLoopTest.cpp` (the game, the hosts, the characters and the constants, under its own
  `LoopRig`), `SimNetClientImmediateTest.cpp` (`NetPeer`, the hosts and the constants, under its `ThreadRig`) and
  `SimNetClockTest.cpp` (the transport, the endpoint, the game and `ServerTee`).

## §2 One owner-traits specialization serves both roles

`SimulatableOwnerTraits` for `EqSim` names one channel type, `SessionChannel<EqSim, Transport::ChannelTransport>` (`EqChannel`),
as both the prediction and the authority owner, so a server character is registered through `registerSimulatable`'s authority
overload with its channel as both owners. A `static_assert` checks that the
server session's `Channel` type is that type; the client's is the same alias over the same transport
(`SimNetClient-rationale.md` §2).

## §3 The retired equivalence suite (og-session-runtime task 15)

SimNetFrameLockedEquivalenceTest.cpp ran one server and two clients over og-net, frame-locked (the sessions drained at the
frame point and every send held until the frame flush), and compared every frame with a reference that had no transport: the
Unreal replication path the sessions replaced, payloads handed straight to the receivers' arrival doors at their next
dispatch. Its two cases:

- **The lossless arm** (SimNetEquivalence.TheLosslessFrameLockedRigEqualsTheNoTransportReferenceFrameByFrame): 600 frames of 1
  to 3 ticks, three characters, correction rotation K = 1 and a redundancy depth of 3 gave, frame by frame and bit for bit, the
  same observables over og-net as the reference: the server's coordinator decisions, trace and consumed moves, and each
  client's corrections, rings, clock ticks, stores, log lines, trace and predicted inputs. Eight of the nine poisons it recorded
  failed it (relays to the owner, a missing correction, a clock a tick late, inputs released a tick early, the hold dropped, and
  others); the ninth, the server's relays and corrections flushed in the other order, changed nothing, because og-net's lane
  scheduler orders a frame's lanes whatever order a session sends in.
- **The loss arm** (SimNetEquivalence.UnderB0LossTheShortfallPerStreamIsPrinted): the same script with 1 % of the clients'
  inbound and 2 % of their outbound datagrams dropped (B0's shape, seeded `0xB0` plus the client's index). og-net packed a
  frame's sends to one peer into one datagram, so a lost down datagram lost that frame's clock, corrections and rings together
  and a lost up datagram a client's whole input set. Measured: the clock and the corrections lost exactly the 6 down losses per
  client (1.00 % of 599 clock ticks); the inputs lost 15 of 599 for client A's characters (2.50 %) and 9 for client B's
  (1.50 %); a ring was lost on a down loss and also when its owner's set was lost upstream (to B: 21 of 598, 3.51 %). That
  was og-net's known deviation against replication: a lost correction is replaced at its character's next rotation turn, and a
  lost ring's entries are gone unless `relayResendEntries` is above 0.

**Why it went.** Its subject was the equivalence of the frame-locked sessions with Ph2's Unreal relays. Tasks 12 to 15 retired
the frame-locked client, the in-frame server and the transport's frame hold, so neither arm had a configuration left to run.
The sending-at-once sessions are covered by `SimNetAuthorityLoopTest.cpp` (`SimNetServer-rationale.md` §14) and
`SimNetClientImmediateTest.cpp` (`SimNetClient-rationale.md` §15). The suite's rig, its reference and their frame records went
with it; the full record of its build, its comparison and its poisons is in this file's history before task 15.
