<!-- SPDX-License-Identifier: MPL-2.0 -->
# `EngineLaneSettings.h` — rationale

`EngineLaneSettings.h` (`Public/OGNetUnreal/Core/`) holds the engine-free settings the OG engine-lane driver derives from
Unreal's own: the packet-simulation intake, the connection timings, the send budget, the build identity, the packet
constants and the numeric join address. It is header-only and includes only standard and og-net headers, so its Catch2 test
`EngineLaneSettingsTest.cpp` runs in `OGNetTests` without an engine. Nothing calls it yet; the driver will. Every function
is `constexpr`, and the header's `static_assert`s pin the mapping's rows, the timing and budget examples and the join-address
forms. The file states no prohibition, so there is no guards file.

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The packet-simulation intake

`EnginePacketSimulation` mirrors FPacketSimulationSettings: the same 15 int32 fields, with the same names, in the same order
(NetDriver.h, lines 467-573). A `static_assert` pins the size at 15 fields. The driver copies the engine's struct into it,
so the mapping can be tested without the engine.

`mapPacketSimulation(settings, seed)` returns an `EmulatorIntake`: an og-net `ognet::EmulatorConfig` for the endpoint's own
`outbound` and `inbound` directions, `active`, and `ignoredMask`.

| Unreal setting | og-net | Unreal's behaviour it follows |
|---|---|---|
| PktLoss | outbound `lossPercent` | |
| PktLag ± PktLagVariance (when PktLag > 0) | outbound lag [max(0, PktLag − \|PktLagVariance\|), PktLag + \|PktLagVariance\|] | PktLag wins over PktLagMin/Max: CheckOutgoingPacketEmulation tests PktLag before them (NetConnection.cpp, lines 2517 and 2529) |
| PktLagMin, PktLagMax | outbound lag [PktLagMin, max(PktLagMin, PktLagMax)] | |
| PktIncomingLagMin, PktIncomingLagMax | inbound lag [min, max(min, max)] | |
| PktIncomingLoss | inbound `lossPercent` | |
| PktDup | outbound `duplicatePercent` | Unreal duplicates only a packet it neither dropped nor delayed (NetConnection.cpp, lines 2374-2393) |
| PktOrder, PktJitter, PktLossMinSize, PktLossMaxSize, PktFrameDelay, PktIncomingFrameDelay | none | each sets its bit in `ignoredMask` |

- **Clamps.** Percentages are clamped to [0, 100] and lags to [0, `ognet::kMaxEmulatedLagMs`] (60,000 ms). Unreal clamps
  most of these in ValidateSettings (NetDriver.cpp, lines 4821-4837), but not PktLag or PktLagVariance, and it has no upper
  lag bound. The mapping clamps on its own, so its result does not depend on the driver having validated the settings,
  and every intake passes `ognet::emulator::isValid`, the check `EmulatedBackend::configure` makes. The upper bound is
  this header's addition to the design's table: without it a PktLag above 60 s would give a config the emulator refuses.
- **The variance's sign.** Unreal draws PktLag's extra lag as PktLag + 2 × (FRand − 0.5) × PktLagVariance
  (NetConnection.cpp, lines 2521-2523), which is symmetric, so a negative PktLagVariance spreads exactly as its magnitude.
  The mapping takes the magnitude. Without it a negative variance would give a lag range whose minimum is above its maximum.
- **What the range does not copy.** og-net draws a lag uniformly between the range's ends (`EmulatedBackend.h`,
  `DirectionModel`). When PktLagVariance exceeds PktLag, Unreal's draws below zero all send at once, so its distribution has
  a step at zero that the clamped uniform range does not. A negative PktLag also differs: Unreal takes the PktLag branch for
  any non-zero PktLag (`else if (PacketSimulationSettings.PktLag)`), and the mapping, as the design's table says, uses
  PktLagMin/Max unless PktLag is above zero.
- **No reordering.** `reorder` is always false: Unreal releases delayed packets in order unless PktOrder or PktJitter is
  set (PktJitter sets bSendDelayedPacketsOutofOrder, NetConnection.cpp, line 2495), and both of those are ignored.
- **The ignored settings.** og-net's emulator has nothing that matches them:
  - PktOrder: Unreal holds every packet and, on each send, releases each held packet with probability one half, newest
    first (lines 2473-2491).
  - PktJitter: Unreal gives even packets a low lag and odd ones a high lag, and lets a later packet overtake an earlier
    one (lines 2493-2515, and the release loop at line 4909).
  - PktLossMinSize, PktLossMaxSize: Unreal drops only packets within a size window (ShouldDropPacketOfSize). og-net's loss
    ignores size.
  - PktFrameDelay, PktIncomingFrameDelay: Unreal delays by engine frames (lines 2540 and 3017). og-net delays by time.

  `IgnoredSetting` gives each one a bit. `kIgnoredSettings` lists them, and `ignoredName` returns the Unreal field name, so
  the driver can warn once per ignored setting. A `static_assert` checks that every entry is a distinct single bit and has
  a name. A value outside the enum is named `Unknown`.
- **`active`** is true when any mapped value is non-zero. An inactive intake leaves the emulator off, so the endpoint uses
  its plain backend: the emulator decorator queues every packet even with an all-zero config.
- **The seed** is the caller's. The design's driver reads it from `-OGNetEmulatorSeed=` (default 1).

## §2 The profile: the one emulator format

`emulatorProfileFor(intake, role)` turns the intake into og-net's `ognet::EmulatorProfile`, the format every shaping host
shares. It is the inverse of `ognet::toEmulatorConfig`: a client's `up` is its `outbound` and its `down` its `inbound`; a
server's are the reverse. The driver logs the profile's `ognet::canonicalText` on its `[OGNet.Emulator]` line, so a run shaped
by `Pkt*` flags reads in the same text as one shaped by the out-of-process shaper.

B0's flags (`kB0PacketSimulation`: PktLagMin/Max 15/50, PktLoss 1, PktIncomingLagMin/Max 15/50, PktIncomingLoss 2) shaped
the server only. The server's profile of those flags is og-net's `b0` preset exactly; the test compares it with the parsed
preset and with its canonical text:
`seed=7,up.loss=2,up.lag=15-50,up.dup=0,up.reorder=0,up.burst=0/0/0,down.loss=1,down.lag=15-50,down.dup=0,down.reorder=0,down.burst=0/0/0`.

## §3 The connection timings

`timingsFor(EngineTimeouts)` keeps Unreal's own timeouts authoritative: og-net must never drop a connection that Unreal
still considers alive.

- Unreal's timeout (GetTimeoutValue, NetConnection.cpp, lines 4477-4511) is InitialConnectTimeout, or ConnectionTimeout
  once the connection is past pending and its owner asks for the short timeout, or 2 s while it is pending destroy. It is
  multiplied by TimeoutMultiplierForUnoptimizedBuilds when that is above zero, in editor and debug builds or with
  `-longtimeouts`. IpNetDriver's defaults are 60 s, 60 s and a multiplier of 1 (BaseEngine.ini, lines 1763-1766).
- og-net's idle timeout becomes `max(default, max(initial, connection) × max(1, multiplier) + kTimeoutMarginSeconds)`, with
  a 5 s margin. It takes the longer of the two timeouts and applies the multiplier in every build, so it is never shorter
  than Unreal's, only longer.
- The resume window becomes `max(default, idle)`, because `ognet::connection::validateTimings` requires the idle timeout to
  be no longer than the resume window. Every other field keeps og-net's default (keepalive 1 s, idle 5 s, resume 30 s:
  `Connection.h`).
- `static_assert`s: Unreal's defaults give valid timings, and 60 s with 10 s gives 65 s. The test checks the grid
  initial ∈ {0.5, 10, 60, 120} × connection ∈ {0.5, 10, 60} × multiplier ∈ {0, 1, 2}: every result is valid and outlasts
  Unreal's timeout.

## §4 The send budget

`budgetFor(maxClientRate, maxInternetClientRate, configuredInternetSpeed, configuredLanSpeed)` sets og-net's lane budget a
quarter above the largest of Unreal's four rates, so Unreal's own rate limit stays the one that binds and og-net never
shapes engine traffic. The project sets MaxClientRate and MaxInternetClientRate to 250,000 (DefaultEngine.ini, the
IpNetDriver section) and ConfiguredInternetSpeed and ConfiguredLanSpeed to 1,000,000 (the Engine.Player section), which
gives 1,250,000 B/s.

- The result is `ceil(largest × kBudgetHeadroomPercent / 100)` (125 %), computed in integers so that no floating-point
  mode can change it, never below og-net's default budget (`ognet::lanes::kDefaultBudget`, 250,000 B/s) and saturating at
  the largest `uint32_t`.
- `burstBytes` is `kEngineLaneBurstBytes`, 64 KiB, which holds many maximum datagrams (`ognet::lanes::isValidBudget`
  requires at least one).

## §5 The build identity

`kEngineLaneBuildIdentity` is the ASCII label `kEngineLaneBuildLabel` (`og-net:ue-engine-lane:ph2`, 25 bytes),
zero-padded to the 32 bytes of og-net's Hello (`kBuildIdentityBytes` in `WireFormat.h`). A `static_assert` checks that the label fits.

With no admission callback, og-net's server refuses a Hello whose identity differs from its own with
`ognet::handshake::RejectReason::BuildMismatch` (`ServerHandshake::decideAdmission`, `Handshake.cpp`). Every build of this
driver carries the same label, so that check always passes between them: the version checks that matter in this phase are
Unreal's own, carried on the engine lane. Changing the label makes this build refuse, and be refused by, every build that
still carries the old one, at og-net's handshake and before Unreal's checks run.

## §6 The packet constants

- `kEngineMaxPacketBytes` = 1,024 is UIpConnection's MaxPacket: InitBase uses MAX_PACKET_SIZE (IpConnection.cpp,
  line 115), which is 1,024 (CoreNet.h, line 795). A `static_assert` checks that og-net's engine lane carries a packet that
  size (`ognet::EngineLane::maxPacketBytes`, 1,189 B). og-net also has a constant named `kEngineMaxPacketBytes`, in
  namespace `ognet`: that one is the lane's capacity, this one is what the Unreal connection reports.
- `kEnginePacketOverheadBytes` = 28 is UIpConnection's UDP_HEADER_SIZE, an IPv4 header and a UDP header
  (IpConnection.cpp, lines 32-33). The OG connection reports the same overhead, so Unreal's rate accounting matches
  IpNetDriver's; og-net's own header and tag are not counted. A `static_assert` spells it as 20 + 8.

## §7 The numeric join address

`numericJoinAddress(host, port)` writes `host:port`, or `[host]:port` when the host contains a `:` and no bracket, and
parses the text with `ognet::parseAddress`. So a dotted IPv4 address, a bracketed or bare IPv6 address, and an IPv4-mapped
IPv6 address (which `ognet::parseAddress` returns as IPv4) all parse; anything else gives `nullopt`. A host name gives
`nullopt`, and the driver resolves it through Unreal instead. A host that already carries a port (`127.0.0.1:7777`) is
bracketed and fails. A host longer than `kMaxJoinHostChars` (64) gives `nullopt` without parsing; the longest textual IPv6
address is 45 characters. The function is `constexpr`, and `static_assert`s pin the IPv4, IPv6 and name cases.

## §8 The tests

`EngineLaneSettingsTest.cpp` (`[OGNet][EngineLaneDriver]`) checks:

- every row of §1's table, including both lag branches, the clamps and the precedence of PktLag;
- B0's flags, and each of the six ignored settings alone (with 1, −1 and 100): inactive, all directions zero, only its bit
  set; an ignored setting next to mapped ones leaves the mapping alone;
- §2's profile for both roles, and B0's server profile against the `b0` preset;
- 10,000 generated settings (edge values and random 32-bit values, a fixed seed): every intake is valid, its profile maps
  back to the same config for both roles, its canonical text parses back to the same profile, and every hundredth one
  configures a real `ognet::EmulatedBackend`;
- §3's grid, §4's budget rows (including the floor and the saturation), §5's bytes, §6's constants and §7's join forms.
