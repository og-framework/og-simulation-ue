<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SwitchableBackend.h` — rationale

`ognetunreal::SwitchableBackend<Inner>` is an og-net datagram backend (`ognet::DatagramBackend`) that forwards to one of two paths,
chosen before the socket opens: the inner backend itself, or og-net's network emulator (`ognet::EmulatedBackend`) wrapped around
it. The Unreal driver uses it to keep the plain path when no emulation is asked for, and to put og-net's emulator in front of the
real socket when the engine's packet-simulation settings ask for it (task P4-4). This file is the docs pointer of
`SwitchableBackend.h` and of its test, `SwitchableBackendTest.cpp`. The header states no prohibition, so there is no guards file.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Why a switch, and not always the emulator

`EmulatedBackend` with a zero configuration still copies every outbound packet into its delay queue and releases it from there
(`EmulatedBackend::sendTo` admits, then releases what is due), and every inbound datagram passes through its queue the same way. So
the switch holds the emulator in a `std::optional`: without emulation, every call goes straight to the inner backend, and the
datagram path is the plain one. The tests' `SwitchableBackend.WithoutTheEmulatorEveryDatagramReachesThePeer` checks that path and
that `emulatorStats()` is empty.

## §2 The interface

- **`explicit SwitchableBackend(Inner&)`** holds a reference; the caller owns the inner backend. Not copyable or movable (the
  emulator inside it is neither).
- **`[[nodiscard]] bool enableEmulator(const ognet::EmulatorConfig&, ognet::EmulatorClockFn)`** builds the emulator around the inner
  backend and configures it. It returns false, and changes nothing, after `open` has been called (the emulator's configuration
  freezes at open, and the endpoint opens its backend in `start`), for a configuration og-net's `ognet::emulator::isValid` refuses
  in either direction, and for a clock with no function (the emulator would stop the process on it). Called again before `open`
  with a valid configuration, it replaces the emulator.
- **`emulatorEnabled()`** and **`emulatorStats()`**: the second returns `std::nullopt` without the emulator, else the emulator's
  per-direction counters.
- **The five concept members** (`open`, `sendTo`, `receive`, `rebind`, `wake`) forward to the emulator when it is enabled, else to
  the inner backend. `open` marks the switch opened first, even when the open fails.
- **`stats()` and `localAddress()`** exist exactly when `Inner` has them (`requires`-clauses, as in `EmulatedBackend`), and always
  forward to the inner backend: the emulator's own versions forward there too. So og-net's `NetEndpoint` still detects
  `ognet::ReportsLocalAddress` (its `[OGNet.Socket] open` and `[OGNet.Rebind]` lines print the local address from it) and
  `ognet::ReportsReceiveErrors` (a growing receive-error count requests a rebind, `EndpointCore::checkReceiveErrors`) through the
  switch, and the engine-lane core's `localAddress()` gives the driver its bound port.
- **`inner()`** gives the inner backend back, for its own statistics.

## §3 The compile-time checks

- **In the header,** for `ognet::BsdDatagramBackend`, the backend the Unreal driver uses: the switch is a `ognet::DatagramBackend`,
  `ognet::ReportsLocalAddress` and `ognet::ReportsReceiveErrors`. A change that drops a forward fails the compile of every file that
  includes the header.
- **In the test,** "exactly when" both ways: over og-net's `VirtualBackend` (a local address, no receive errors) the switch reports a
  local address and no receive errors; over the test's `ErrorCountingBackend` (receive errors, no local address) the reverse.

## §4 Tests

`SwitchableBackendTest.cpp` (`[OGNet][EngineLaneDriver]`, run by `OGNetTests`), over og-net's `VirtualNetwork` in virtual time:

- `SwitchableBackend.WithoutTheEmulatorEveryDatagramReachesThePeer`: 10 datagrams out and one back, byte-exact.
- `SwitchableBackend.WithFullOutboundLossNothingReachesThePeer`: with 100 % outbound loss the peer receives nothing and the inner
  backend sends nothing (all 10 offered, all 10 lost), while an inbound datagram still passes through the emulator.
- `SwitchableBackend.TheEmulatorIsChosenOnlyBeforeOpenAndOnlyWithAValidConfig`: an invalid configuration, a missing clock and any
  call after `open` return false and leave the switch as it was.

The engine-lane core's tests run every case over `SwitchableBackend` without the emulator, and one with a client whose emulator
drops every outbound datagram (`EngineLaneDriverCore-rationale.md` §10).
