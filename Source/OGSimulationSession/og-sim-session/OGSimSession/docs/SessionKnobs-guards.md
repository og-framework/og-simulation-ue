<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: onRelayDelayFloorReplayed -- deleted by og-input-latency task P5-9 (the relay actors, their listeners and the session's two replay members); kept in G-01's history -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (G-03): the ini key that was built and removed on a ruling. It must NOT resolve -->
# `SessionKnobs.h` — guards

Prohibitions with a tagged site in og-sim-session's `OGSimSession/SessionKnobs.h`. Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionKnobs-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Every entry here moved from `UESimulationSession-guards.md` (og-simulation-unreal) with the code it marks, by og-session-runtime
task 23: the knob intake left the Unreal session for this engine-free header. Each entry names its old id; those ids are retired
there with this file's id as their successor. What the compiler checks is not here: the doors' bounds against
`ClockDilationConfig`'s compiled defaults and the render bracket (four `static_assert`s), the case-insensitive compare's
ASCII-only folding (a `static_assert`), the og-sim-net server's presence before the floor's publish (an `OG_CHECK`), and, in the
Unreal adapter, each line's category and level (a `static_assert` in the game module's `UESessionClockLineRouteTest.cpp` or
`UESessionKnobsLineRouteTest.cpp`, `UESimulationSession.h` until og-session-runtime task 31).

---

## G-01 — the relay-delay floor is read from the ini on the authority only

**Tag site:** `SessionKnobs.h`, directly above `if (worldIsAuthority)`, the floor intake in `readSessionKnobs`. The Unreal
session calls `readSessionKnobs` once in `beginPlay`, on both roles.

**The prohibition, as it stood in the source:**

> ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
> reading its own ini could disagree with the server it is meant to match.

**The consequence.** A client that takes its own ini floor predicts against a delay the server does not use.

**What breaks if the tag moves.** Nothing else distinguishes this gate from the rotation-width gate below it. That gate is
authority-only for a different and weaker reason (a client read has no reader).

*R0, task 26b:* verified. A client's floor arrives only through the timing relay: `onRelayDelayFloorReceived` and
`onRelayDelayFloorReplayed` clamp it and hand it to the manager (`applyReplicatedRelayDelayFloor`); `applySessionKnobs`
runs on the authority only (the owner calls it in its authority branch).

*R0, og-input-latency task P5-7:* the floor is og-sim-net's `RelayFloor` Control message since P5-7, published by
`applySessionKnobs` through the server's `publishFloor`; a client still learns it only from the server (the client host hands it
to `onRelayDelayFloorReceived`). Unchanged: the gate.

*R0, og-session-runtime task 23:* verified on the tree. A client learns the floor only from og-sim-net: its host calls the
Unreal session's `onRelayDelayFloorReceived` (`UESimNetHost.h`), which hands it to `applyReplicatedRelayDelayFloor` (the replay
twin, `onRelayDelayFloorReplayed`, was deleted by og-input-latency task P5-9). The Unreal session calls `applySessionKnobs` in
its authority branch only. The intake's old shape, `worldIsAuthority && GConfig != nullptr`, lost its second operand: the
config source answers "absent" when there is no `GConfig` (`UEConfigSource`).

*Was `UESimulationSession-guards.md` G-14 (moved by og-session-runtime task 23); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-57 (moved by og-ue-simulationcomponentandmanagersplit task 26b), with the intake it
marks.*

---

## G-02 — there is no relay-ring depth ini intake, deliberately

**Tag site:** `SessionKnobs.h`, the blank line between the relay-delay-floor intake and the rotation-width intake in
`readSessionKnobs`, where the retired intake used to be. An absence tag: the line under it is blank.

**The prohibition, as it stood in the source** (two fences, one at the intake and one at the apply block. They are one
tag, because the apply block has nothing left to re-add without the intake first):

> ⛔ RETIRED: there is deliberately no relay ring depth ini intake here any more. It read
> a session-configurable retention depth for the outbound ring's replace-latest write
> path; the flush-on-poll replacement takes its capacity from `relayedInputRing::kMaxDepth`,
> a compile-time constant with no ini key, so the intake, its clamp, its setter and its
> startup proof line were all removed together. §6

> ⛔ RETIRED: there is deliberately no relay ring depth clamp, setter or proof-line block
> here any more. It published a session-configurable retention depth that flush-on-poll
> had already made inert - the stage capacity is `relayedInputRing::kMaxDepth`, a
> compile-time constant - so the whole inert path went rather than keep publishing a
> number nothing on the live relay path reads. §6

(The `§6` the quotes end with was the brawler's rationale section on the relay ring; the depth's argument is
`SessionKnobs-rationale.md` §2.7, *The retired fourth knob*, and the Unreal session's rationale §9.7, *The relay tap*.)

**The consequence.** Restoring the key restores a knob that nothing on the live path reads. That is the "silently inert
setting" class the brawler's session file shipped three times.

**What breaks if the tag moves.** This is an absence fence: no identifier exists to grep for, and the tag is the only
record.

*R0, task 26b:* verified: the stage capacity is `relayedInputRing::kMaxDepth` (og-simulation's `RelayedInputRingCodec.h`),
read inside the codec and fed to PROBE 5 from the same constant (G-12 of the Unreal session's guards, retired by
og-input-latency task P5-7 with PROBE 5); the manager has no depth setter (`SimulationManager.h` carries its own absence fence
for one).

*R0, og-session-runtime task 23:* verified: `relayedInputRing::kMaxDepth` is 8 in `RelayedInputRingCodec.h`, and the manager's
absence fence still stands above its setters.

*Was `UESimulationSession-guards.md` G-15 (moved by og-session-runtime task 23); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-56 (moved by og-ue-simulationcomponentandmanagersplit task 26b), with the intakes it
sits between.*

---

## G-03 — the resim-trigger-policy intake is not authority-gated, is read once, and has no cooldown key

**Tag site:** `SessionKnobs.h`, directly above `if (std::optional<std::string> iniPolicy = config.readString("ResimTriggerPolicy"))`,
the resim-trigger-policy intake in `readSessionKnobs`.

**The prohibition, as it stood in the source:**

> ⛔ THERE IS DELIBERATELY NO `ResimCooldownTicks` KEY. A trigger-rate ceiling was built
> here and REMOVED on a user ruling: it defers acting on a correction already known to
> disagree, which is the defect this mechanism repairs. If a design document names that
> key, the document predates the ruling. The throttle is structural instead.
>
> ⚠ NOT AUTHORITY-GATED, unlike the two above: the gate exists ONLY on a predicting client.
>
> ⛔ ONE-SHOT, and here that is THREAD SAFETY: the policy is read unsynchronized at every
> correction landing, which is sound ONLY because it is written once before any land.

and, from the brawler's file banner:

> ⛔ NO KNOB HERE MAY BECOME A CVAR. Each is read ONCE at composition: the
> rotation width because a cadence that moves mid-run makes a probe window
> unattributable, and the resim policy because it is pushed into every
> correction cache and read unsynchronized on the landing path - which is sound
> only because it is written before any correction can land. §3

**The consequence.** Copying the neighbours' `worldIsAuthority` guard onto this intake reads the ini on the one role that
cannot use it and skips it on the only role that can. Turning the policy into a cvar makes it a value written mid-session
that `policyEnforcesDepthCeiling` reads on the step thread with no synchronization.

**What breaks if the tag moves.** The rotation-width intake above has the same no-cvar rule for the weaker reason
(attribution). This tag sits on the intake where the rule is a correctness matter.

*R0, task 26b:* verified: the key is no identifier anywhere in the tree and no ini sets it; the name survives only inside
the removed setter's name, in two comments (og-simulation's own absence fence on the manager, `SimulationManager.h`, and
og-simulation-tests' `ResimGatePolicyTest.cpp`). The manager's `onCheckIsSimilar`
reads the policy through `resimGate::policyEnforcesDepthCeiling` with no synchronization, and `setResimTriggerPolicy` also
pushes it into the reconciliation peer's correction caches (og-simulation's `SimulationManager.h`).

*R0, og-session-runtime task 23:* re-verified on the tree: the same two comments are the key's only occurrences, and
`SimulationManager::onCheckIsSimilar` reads `resimGate::policyEnforcesDepthCeiling(m_timeConfig.resimTriggerPolicy)`. The
consequence said "on the physics thread"; on a client that is the step's thread, which runs the steps since og-input-latency
P5-17, so it says so. (og-simulation's `ResimGatePolicy.h` still names `SimulationReconciliation::checkDivergenceAll` as the
predicate's reader; the reader is the manager's `onCheckIsSimilar`. That comment is og-simulation's, outside this task, and is
reported to the lead.)

*Was `UESimulationSession-guards.md` G-16 (moved by og-session-runtime task 23); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-58 (moved by og-ue-simulationcomponentandmanagersplit task 26b), with the intake it
marks.*

---

## G-04 — the latency-knob intake is not authority-gated

**Tag site:** `SessionKnobs.h`, directly above `if (const std::optional<int32_t> iniFrames = config.readInt("RedundancyFrames"))`,
the latency-knob intake in `readSessionKnobs`. The `RelayResendEntries` read is the statement after it; the tag covers the
first (the comment rule's clause B), and the edit this entry forbids is the same on both.

**The prohibition.** ⛔ Do not give this intake its neighbours' `worldIsAuthority` guard. `RedundancyFrames` is read only by
a predicting client's send, so an authority-gated read skips it on the one role that uses it. `RelayResendEntries` is read
only by the authority's flush, and reading it on both roles keeps one `TimeConfig` shape, as the resim-trigger-policy intake
does (`⛔G-03`).

**The consequence.** With the guard, a client never reads `RedundancyFrames`. The client's `[LatencyKnobs]` proof line
still prints, reporting `compiled`, so the knob looks applied in the server's log and is silently inert where it acts.

**What breaks if the tag moves.** The floor and rotation-width intakes above are authority-gated for their own reasons
(`⛔G-01`), so copying their shape is the natural edit; this tag is what marks the difference at the line where it would
be typed.

*R0, og-input-latency task 4:* written with the code. `applyLatencyKnobs` runs after both role branches in `beginPlay`,
and on every peer it prints one `[LatencyKnobs]` line with the values read back from the manager's `TimeConfig`. The
fields have no reader yet (og-input-latency tasks 7 and 8 add them); the prohibition is about the intake's shape, which
those tasks rely on.

*R0, og-session-runtime task 23:* the readers exist now: `redundancyFrames` is read by og-sim-net's client send
(`SimNetClient.h`, the redundant window) and by the manager's send path (`SimulationManager.h`), `relayResendEntries` by
og-sim-net's server flush (`SimNetServer.h`, through `latencyKnobs::clampRelayResendEntries`). The two reads were one
`if (GConfig != nullptr)` block before this task; they are two statements now, the tag on the first.

*Was `UESimulationSession-guards.md` G-27 (moved by og-session-runtime task 23), written by og-input-latency task 4.*

---

## G-05 — the resim-trigger policy is applied and proved once, after both role branches

**Tag site:** `SessionKnobs.h`, directly above `if (knobs.hasIniResimTriggerPolicy)`, the first statement of
`applyResimTriggerPolicy`. The runtime's `start` calls it once, after both role branches (the Unreal session's `beginPlay` until
og-session-runtime task 31); the "after both role branches" half of the first quote below is typed at that call, so it has
its own tag there (`SessionRuntime-guards.md` G-04; `UESimulationSession-guards.md` G-24 until task 31). This entry
keeps the body's three.

**The prohibition, as it stood in the source:**

> ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3

> STEP 2 - PARSE + VALIDATE. ⛔ An unrecognised string is REPORTED and the default kept:
> a typo silently selecting the other value changes the gate on a build nobody touched. §3

> STEP 4 - THE PROOF LINE. Unconditional, at Warning; see the banner. §3
>
> ⛔ THIS LINE IS THE BEHAVIOUR-NEUTRALITY RECEIPT: a later claim names WHICH POLICY WAS LIVE.
>
> ⛔ Values are read back from TimeConfig, so it cannot claim a setting nothing stored.
>
> ⛔ `depthPolicy` and `rateLimit` state inertness rather than falling silent. §3

**The consequence.** If apply-plus-proof is duplicated into the two branches, the roles drift. If an unrecognised string
falls through to a value, the gate changes on a build nobody touched. If the proof line becomes conditional, a run's log
can no longer say which policy was live.

*R0, task 26b:* the brawler's entry carried a correction (its rationale §13.10, C13-1): the banner's reason for
`Warning`, that the project ini sets `LogOGNet=Warning`, was false at `b9f6d81`, where that ini set `Verbose`. Re-checked
on today's tree: the brawler project's `Config/DefaultEngine.ini` sets `LogOGNet=Warning` again, so the banner's reason
holds today. The line's other reason, that it is unconditional, holds: the `[ResimGate] session` line is printed on every
path through the function, with the values read back from the manager's `TimeConfig`.

*R0, og-session-runtime task 23:* re-verified: `Config/DefaultEngine.ini` sets `LogOGNet=Warning`; the `[ResimGate] session`
line is the function's last statement, printed on every path, its values read back from `manager.getTimeConfig()`. Its level
is now the line's `[Warning]` token, which the Unreal adapter's router reads (`SessionKnobs-rationale.md` §6).

*Was `UESimulationSession-guards.md` G-17 (moved by og-session-runtime task 23); before that the brawler's
`OGBrawlerSimulationManager-guards.md` G-63 (moved by og-ue-simulationcomponentandmanagersplit task 26b), whose site was the
bare scope block after the role branches in the owner's `BeginPlay`.*

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

None yet.
