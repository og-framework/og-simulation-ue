<!-- SPDX-License-Identifier: MPL-2.0 -->
# `HeadlessSessionLink.h` — rationale

`HeadlessSessionLink.h` (in og-sim-session's `tests/`) is the headless host's link adapter and the server host's port (og-session-runtime task 32):
what `UESimNetLanes.h` and the server half of `UESimNetHost.h` are on Unreal. It is design §3.6's item 6, "create a `SimNetLink`
when the peer starts, service it each frame, stop it with the peer", plus the runtime's `Link` doors.

## §1 `Lanes`: a `SimNetLink` with the runtime's doors

`headlessSession::Lanes` derives from og-sim-net's `SimNetLink` and adds what `SessionRuntime` calls on its `Link` that the link
itself does not have:

- **The engine's driver doors**, named as Unreal's `IOGNetSimLanes` callbacks: `beforeStart` (start the link on the endpoint, before
  og-net starts), `afterDispatch` (a client's frame service; a server's runner round-trip publish), `beforeStop` (join the runner,
  run the stop handler, unbind, stop the link). `markStarted` and `flushFrame` are the link's own.
- **The runtime's doors:** `hostServiced` (true on a server: the runner services og-net through `SimNetIdle`), `driver` (none),
  `ackSrttSeconds`, the step and stopped drains, the stop handler, the authority runner's two callbacks and the client session's
  bind and unbind.

The link's services and the attached session's hooks live in a private base, `LinkParts`, built before the link: the link holds
both by reference from its constructor on (the order `UESimNetLanes` keeps with its member order and a `static_assert`; bases
are built in the order they are listed, and `LinkParts` is listed before the link, so no assert is needed). The base's members are called
`m_linkServices` and `m_linkSessionHooks` because `SimNetLink` has a private `m_services` of its own, and a second member of that
name in a base made every use ambiguous.

`SessionHooks` forwards the link's session calls to the runtime's doors (`SessionRuntime::onTimingInfoReceived` and the others),
as `UESimNetLinkSession` does; the client receive stamp goes nowhere (this host keeps no latency diagnostics).

## §2 `ServerHost`: og-sim-net's server host over a port

The runtime builds its `Link::ServerHost` with the driver, the endpoint and its own address (`SessionRuntime::attachServerLink`).
`ServerHost` is og-sim-net's `SimNetServerHostCore` over `ServerPort`, which is both the core's services and its session (as
`UESimNetServerHostPort` is on Unreal). The port lives in a private base, `ServerPortPart`, so it is built before the core that holds
it by reference: the split task 31a's `UESimNetLanesServerHost` uses.

- `rttMs` reads og-net's statistics for the connection (`HeadlessPlatform-rationale.md` §5). The core runs every door on the
  thread that built it, the authority runner, and og-net's statistics are safe to read there.
- `receptionTick` and `onLifecycleUnregistering` go to the runtime; the two server stamps go nowhere.
- `publishRunnerRtt_GameThread` does nothing: Unreal copies the driver's round trips into a table the runner reads, because its
  connections are game-thread objects. og-net's statistics need no copy.
