<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetConnection.h` and `.cpp` — rationale

`UOGNetConnection` is the engine's net connection over og-net's engine lane: one per og-net connection, created by
`UOGNetDriver` (`OGNetDriver-rationale.md`). This file is the docs pointer of `OGNetConnection.h` and `OGNetConnection.cpp`;
the one prohibition is in `OGNetConnection-guards.md`.

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 The packet size and overhead

`InitBase` always passes `ognetunreal::kEngineMaxPacketBytes` (1,024) and `ognetunreal::kEnginePacketOverheadBytes` (28) to
the base, whatever the caller asks for. Those are UIpConnection's values: MAX_PACKET_SIZE and UDP_HEADER_SIZE, an IPv4 and a
UDP header (IpConnection.cpp, lines 31-33 and 110-116). The engine budgets its rate and its bunches from these two numbers,
so with the same values the engine writes the same packets on both drivers. og-net's own header and tag are not counted:
og-net's budget is set above every engine rate (`EngineLaneSettings-rationale.md`).

A `static_assert` pins `kEngineMaxPacketBytes` to MAX_PACKET_SIZE (CoreNet.h): the base InitBase checks that the packet
size is at most MAX_PACKET_SIZE, and the engine lane carries up to 1,189 B, so a larger value would fail at run time and a
smaller one would make the engine write smaller packets than over IpNetDriver. Measured: comparing against 1023 fails the
build with C2338.

## §2 The two init paths

- **Server, `InitRemoteConnection`.** It takes the `FInternetAddrOGNet` of the new og-net connection (a check refuses any
  other address type), calls `InitBase`, stores the id in `OGNetId`, keeps a clone of the address as RemoteAddr and writes
  its text into the connection URL's host, then `InitSequence(4, 4)`, InitSendBuffer, and sets the login state to
  LoggingIn, expecting NMT_Hello. After InitBase that is UIpConnection's InitRemoteConnection (IpConnection.cpp, lines
  152-173), with the packet sequence added.
- **Client, `InitLocalConnection`.** `InitBase`, `OGNetId` 0 and an `FInternetAddrOGNet` of id 0 as RemoteAddr (og-net's
  connection is not up yet), `InitSequence(4, 4)`, InitSendBuffer. The driver sets the id and the address when og-net's
  connection comes up.
- **The packet sequence.** With a packet handler, the stateless connect component hands each side the other's starting
  sequence. There is no handler (§3), so both sides start at the same fixed sequence, 4 and 4, as Steam's sockets driver
  does when its handlers are disabled (SteamSocketsNetDriver.cpp, lines 191 and 518). Nothing depends on the value being
  secret: og-net's AEAD and replay window authenticate every packet.

## §3 No packet handler

`InitHandler` is overridden with an empty body, so the base InitBase's call to it (NetConnection.cpp, lines 518-520) leaves
`Handler` null. The base would create a PacketHandler holding the stateless connect component (NetConnection.cpp, lines
693-732). Its job, admission without state before a cookie round trip, is og-net's handshake cookie's; its connectionless
challenge would add a round trip and a second admission layer that only ever sees authenticated traffic. UE's encryption
component is not configured in this project (BaseEngine.ini's PacketHandlerComponents section, lines 3530-3532,
names it only in a comment, and the project's ini files have no such section), so nothing encrypted is lost; og-net encrypts the whole lane. The guard G-01
forbids putting the base call back.

## §4 Sending

`LowLevelSend` rounds the bit count up to bytes and hands them to `UOGNetDriver::sendEngine` with the connection's id. It
does not pass them through a packet handler (there is none). Before og-net's connection is up, the client's id is 0 and the
driver's core buffers the packets (`EngineLaneDriverCore-rationale.md`); a refused or dropped packet is lost exactly as a
lost UDP datagram is, and the engine's reliability resends what needs resending.

## §5 Describing the connection

`LowLevelGetRemoteAddress` is the remote address's text, `OGNet:` and the id in 8 hex digits, and `LowLevelDescribe` is the
same text with the engine's connection state. The connection never exposes the peer's IP (`OGNetInternetAddr-rationale.md`).

## §6 Clean-up and the close order

`CleanUp` remembers its OG driver, runs the base CleanUp, and only then calls `UOGNetDriver::engineClosed`.

- The base CleanUp closes an open connection first: Close sends the engine's close reason, marks the connection closed and,
  once the connection has received a packet from its peer, flushes the reason through `LowLevelSend` (NetConnection.cpp,
  lines 1073-1121 and 1365-1378). It ends by clearing the connection's driver (line 1457), which is why the driver is
  remembered first.
- `engineClosed` tells the core the engine is done with the connection. From then on the core refuses sends to it and
  disconnects og-net's connection once its engine queue has drained (`EngineLaneDriverCore-rationale.md`). Calling it before
  the base CleanUp would refuse the close reason the base is about to send, so the peer would see the connection drop with
  no reason.
- `engineClosed` checks that the engine has closed the connection, which holds only when it runs after the base CleanUp on a
  connection that was open. The check message names the order it enforces.
- The order runs opposite to the task text's ("`engineClosed` and then the base CleanUp"); the task text would have dropped
  the close reason.

## §7 The driver link

`OGNetId` is public so the driver can set it when the client's connection comes up. The connection reaches the driver
through a Cast of its Driver on every send and at clean-up: the connection class is only ever created by `UOGNetDriver`
(its InitBase refuses a NetConnectionClass that is not a `UOGNetConnection`), and a connection without an OG driver sends
nothing.
