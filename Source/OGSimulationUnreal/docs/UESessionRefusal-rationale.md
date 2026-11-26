<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESessionRefusal.h` — rationale

## 1. What it is

The one shape of a refused simulation session (og-session-runtime design §2.2), shared by every refusal the session prints:

- `ueSessionRefusal::refuse(subject, value, reason)` logs `[SimHost.Refused] <subject> value=<value> reason=<reason>` at
  Error on `LogOGSimHost`, and raises the on-screen message with the key and the duration of the driver refusal
  (`kSimNetDriverRefusedToastKey`, `kSimNetDriverRefusedToastSeconds`, 15 s). One key means a later refusal replaces the
  message on screen instead of stacking a second one.
- `ueSessionRefusal::logRefusal(subject, value, reason)` is the line alone, for a consequence of a refusal whose message is
  already on screen: the session's one registration line in a world without a session (`UESimulationSession-rationale.md`
  §23). Through `refuse` it would replace the refusal's message with its own; a standalone world, which raised none, uses
  `refuse` for it (og-session-runtime task 14).
- `ueSessionRefusal::requestServerExit()` ends a dedicated server with exit code 3 (`kServerExitCode`) and does nothing on any
  other net mode (§3). It tests `IsRunningDedicatedServer()`, the process: a dedicated-server world inside the editor (Play
  In Editor with "Run Under One Process") is refused without ending the editor.

The header is inline only: it holds no state, and its callers are the session's templates.

## 2. The line

`kRefusedLineFormat` is formatted with `snprintf` into a `kRefusedLineBytes` (160) buffer and logged with `TEXT("%hs")`,
the pattern of the `[SimNet.Host]` proof line's `kSimNetHost` (`SessionRuntime.h` since og-session-runtime task 31). The `static_assert`
sizes the buffer for the longest subject the session prints (`role=authority netMode=DedicatedServer`,
og-session-runtime task 16b), an int32 value and the longest reason (`one-session-per-role-per-process`, task 16b). A longer subject is cut by
`snprintf`, never written past the buffer.

The refusals printed today:

| subject | value | reason | where | message | a dedicated server |
|---|---|---|---|---|---|
| `knob=og.Sim.RunInline` | the console variable's value | `retired` | `UESimulationSession::beginPlay`, before anything is built (`refuseRetiredKnobs`) | raised | exits with code 3 |
| `knob=SimTrafficImmediate` | `0` | `retired` | the same, on a client whose og-net lanes read `[OGNetcode] SimTrafficImmediate=0` at driver start (og-session-runtime task 12) | raised | — (a client only) |
| `knob=og.Sim.AuthorityLoop` | `0` | `retired` | the same, on a dedicated server (og-session-runtime task 14; any other value is accepted) | raised | exits with code 3 |
| `role=authority netMode=ListenServer` | `-` | `dedicated-server-only` | the same, first: a listen server starts no session (task 14, design Q2) | raised | — (a listen server only) |
| `role=client netMode=Client` or `role=authority netMode=DedicatedServer` | `pie` in a Play-In-Editor world, else `-` | `one-session-per-role-per-process` | the same, last: a second world of a role in one process, while the first has a session (`refuseSecondSessionOfRole`, task 16b; `UESimulationSession-rationale.md` §23.1) | raised | exits with code 3 only as a dedicated-server process, which never holds two server worlds; in the editor nothing exits |
| `register id=<n>` | `-` | `no-session` after a knob or driver refusal; `dedicated-server-only` in a standalone or listen-server world | the first `tryRegister` or `clientBindStep` of a world without a session, once per world | raised in a standalone world only, where nothing raised one before; elsewhere not (the refusal's stays) | — (it has exited) |

Two more refusals of the start rule (`UESimulationSession-rationale.md` §23) do not use this line: a standalone world logs one
Warning, `[SimHost.Session] none netMode=Standalone`, and starts no session (it is not an error: the packaged client's front
end is one), and a world on a driver other than og-net keeps today's `[SimNet.Host] refused role=<server|client> driver=<class>`
Error on `LogOGNet` with the message under this file's key, and on a dedicated server exits with code 3 too
(`requestServerExit`).

## 3. Why a dedicated server exits, and why it is forced

A refused knob is refused, not ignored: the og-input-latency runbooks and package scripts still pass the retired knobs for
their attribution arms, and an ignored knob would run the default and be read as the arm (design §2.2). A dedicated server
exits with a non-zero code, so a capture's 10-second check fails at once instead of finding a server that runs without
simulating. A client stays up: it shows the message, and its world plays nothing (`UESimulationSession-rationale.md` §23).

The exit is forced (`RequestExitWithStatus` with Force true). Measured in og-session-runtime task 11 on Windows: the graceful
request (Force false), the backlog's wording, exited with code 0 (run `SR2_t11_srv_knob_e1`: the server logged the line and
exited after 5.2 s with code 0). The engine's graceful Windows path requests the engine's exit and posts the code in the quit
message, which nothing reads; the launcher returns the main loop's own error level, 0. Its Unix path keeps the code as an
override and exits with it. The forced path flushes the log and ends the process with the code on both (run
`SR2_t11_srv_knob`: one line, code 3 after 4.1 s). Nothing is lost by skipping the graceful shutdown: the refusal comes at
the simulation manager's `BeginPlay`, before the session built anything, on a server with no client yet.

## 4. Provenance

Written by og-session-runtime task 11 (2026-11-24), when `og.Sim.RunInline` was retired (`UETickDriverHost-rationale.md`
§4). Task 14 added the `og.Sim.AuthorityLoop` and listen-server rows, the standalone message and the driver refusal's exit, when
the in-frame server was retired. Task 16b added the second-session row. The two message constants moved here from `UESimulationSession.h` with their values, so the og-net driver refusals
(`[SimNet.Host] refused role=…`, `UESimulationSession-rationale.md` §16) and this file's refusals share one key.
