<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: ueSimNetHost::noteDelivered -- Unreal's server host's delivery note, deleted by og-session-runtime task 31a (og-sim-net's SimNetServerHostCore::noteDelivered makes the note); named in this file's history -->
# `SimNetServerHost.h` — guards

Every prohibition that governs a line of `SimNetServerHost.h`. Each entry has an **opaque, stable id**. In the source a single
line `// ⛔G-nn  docs/SimNetServerHost-guards.md` sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not soften
the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R and its
number is spent forever.

⛔ **Nothing in this file is a rationale.** The reasons live in `SimNetServerHost-rationale.md`.

---

## G-01 — the server host's lines go through `routeSimNetHostLine`, never straight to `logHost`

**Tag site:** `SimNetServerHost.h`, `SimNetServerHostCore::log`, the line above
`routeSimNetHostLine(line, [this](const char* routed) { m_services.logHost(routed); });`.

**The prohibition.** Do not hand `line` to `m_services.logHost` directly, and do not route it through anything that drops
`routeSimNetHostLine`'s `[Roster.` rule.

**The consequence.** og-sim-net's server prints `[Roster.InputRefused]` (a bundle for a character the connection does not own,
`SimNetRoster.h`'s `kRosterInputRefusedFormat`) with no level token. `routeSimNetHostLine` prepends `[Warning]` to every line
that begins with `[Roster.`. Without it the line is routed at its row's plain level, Log, and the brawler's
`Config/DefaultEngine.ini` sets `LogOGNet=Warning`, so a refused input is filtered out at run time: exactly the line an
operator must see.

**What breaks if it moves.** `SimNetServerHostTest.cpp`'s "with no session the hooks are dropped and the lines still route" case
fails (its `[Roster.Refused]` line arrives without the token), and so does `OGBrawler.SimNet.Hosts` (its token-less
`[Roster.Refused]` probe must reach `LogOGNet` with `[Warning]`).

Was the rule of Unreal's `UESimNetServerHost::log`, which called `ueSimNetHost::routeLine` (og-session-runtime task 31a moved the
call here; `UESimNetHost-rationale.md` §9).

---

## G-02 — the delivery note's "now" is the services' clock, never the record's receive time

**Tag site:** `SimNetServerHost.h`, `SimNetServerHostCore::noteDelivered`, the line above
`noteSimLaneDelivered(m_endpoint, lane, recvSeconds, m_services.nowSeconds());`.

**The prohibition.** Do not pass `recvSeconds` (or any other value) as the fourth argument, and do not give the services a clock
other than the one og-net stamps `recvSeconds` with.

**The consequence.** The endpoint files each drained message's residence, `now − recv`, into its `[OGNet.Window]` sim-lane
histogram (og-net's `NetWindow::noteDelivered`). With `recvSeconds` as now every residence is 0 and the histogram reads "no
queueing" whatever the drain's lag; with another clock the residences are offset by the two clocks' difference. Either way
the rows that measure how long a received input waited for the server's drain lie.

**What breaks if it moves.** `SimNetServerHostTest.cpp`'s delivery-order case gives its services a clock 0.25 s ahead of the
network's and checks that every note's now is more than 0.125 s after its receive time, and that the last note's now is the
services' clock: a note made with the receive time as now fails both. In the engine, the server's `[OGNet.Window]`
residence rows collapse into their first bucket.

Was the rule of Unreal's `ueSimNetHost::noteDelivered`, which passed `FPlatformTime::Seconds()`, og-net's receive clock in
Unreal (og-session-runtime task 31a moved the call here; `UESimNetHost-rationale.md` §9).

---

## R. Retired ids

None.
