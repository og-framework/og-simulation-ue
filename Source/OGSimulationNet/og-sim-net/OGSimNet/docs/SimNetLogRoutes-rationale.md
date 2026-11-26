<!-- SPDX-License-Identifier: MPL-2.0 -->
# `SimNetLogRoutes.h` — rationale

`SimNetLogRoutes.h` holds og-sim-net's log-route rows, `ogsimnet::kSimNetLogRoutes`: one row per tag family whose lines
og-sim-net's sessions, roster and link hand to a log sink. A host routes each line by the first row whose prefix begins it;
og-sim-session's `SessionLogRoutes.h` composes these rows after og-simulation's and before its own
(`SessionLogRoutes-rationale.md` in og-sim-session). The header states no prohibition (its two rules are `static_assert`s), so
there is no guards file.

**If this file and `SimNetLogRoutes.h` disagree, the header is authoritative and this file is stale.**

---

## §1 The rows

| row | route | `plainLevel` | emitters in og-sim-net |
|---|---|---|---|
| `[SimNet.` | `Net` | `Log` | `SimNetServer.h` and `SimNetClient.h` (`[SimNet.Window]`, `[SimNet.Lifecycle]`), `SimNetServer.h` (`[SimNet.Disconnect]`), `SimNetLink.h` (`[SimNet.Host]`, the client's disconnect after a refused roster, with an `[Error]` token) |
| `[Roster.` | `Net` | `Log` | `SimNetRoster.h` holds the formats (`[Roster.InputRefused]`, `[Roster.Refused]`, `[Roster.BindMismatch]`); `SimNetServer.h` prints `[Roster.Refused]` with a `[Warning]` token; `SimNetLink.h`'s `routeSimNetHostLine` puts `[Warning]` in front of a token-less `[Roster.` line |
| `[ReceiveCorrectionState]` | `Net` | `Verbose` | `SimNetClient.h` (one line per correction received) |
| `[ClockSource.Held]` | `Net` | `Log` | `SimNetClient.h` (the held clock sample's line; `SimNetLink.h` gives it a `[Warning]` token) |

Each is a row of og-simulation's type, `SimulationLogRouteEntry`, on og-simulation's `Net` route. `[SimNet.` and `[Roster.`
are family catch-alls, with the dot. A host and og-sim-session emit lines of both families too (the Unreal adapter's
`[SimNet.Host]` refusal lines, the session's `[SimNet.Host]` start-up line and `[Roster.BindMismatch]` check); they route through
these rows.

`allReachable` and `plainLevelsAreLogOrVerbose` (og-simulation's `SimulationLogRoutes.h`) are `static_assert`ed over the
block: no row shadows a later one, and no row is at `Warning` (a line that must reach `Warning` carries the token).

## §2 Provenance

og-session-runtime task 36 (2026-11-26) gave og-sim-net its own block:
- step (a) moved the `[SimNet.` and `[Roster.` rows, unchanged, out of og-simulation's `kSimulationLogRoutes`, where they had
  been since og-input-latency P5-4 (2026-11-18) by decision, because og-sim-net then had no table of its own;
- step (b) made the rows an array of their own and added `[ReceiveCorrectionState]` and `[ClockSource.Held]`, which until then
  sat in the Unreal adapter's block (`[ClockSource.Held]` since og-session-runtime task 28). No row's prefix, route or level
  changed.
