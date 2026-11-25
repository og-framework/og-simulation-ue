<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UESessionTickHost.h` — rationale

Companion to `Source/OGSimulationUnreal/UESessionTickHost.h` (og-session-runtime task 26). Its rules are concepts and checks, so
there is no guards file.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is

`UESessionTickHost<BindingT>` is the Unreal session's host for og-sim-session's `SessionTickGame`
(`SessionTickGame-rationale.md`, og-sim-session): a session pointer and the accessors the tick game asks for on every hook. The
backend (`TickDriverBackendUImpl`) holds the tick game `UESessionTickGame<BindingT>`, i.e.
`ogsession::SessionTickGame<UESessionTickHost<BindingT>, UELatencyBudgetDiagnostics<TraitsT>>`, built from
`UESessionTickHost{ &session }`. `BindingT` is the game's binding (the brawler's `BrawlerSessionHooksUImpl<SessionT>`), and
`TraitsT` is read off the session type by `UESessionTraitsOf` (a partial specialisation over `UESimulationSession<TraitsT>`, which
needs no member of the incomplete session).

| accessor | returns | the session's |
|---|---|---|
| `manager()`, `storage()` | `BindingT::Manager&`, `BindingT::Storage&` | `*m_manager`, `m_storage` |
| `threads()` | `const SessionThreadChecks&` | `*m_threads`, built in `beginPlay` before the world (a `checkf`) |
| `clock()`, `display()`, `netHooks()` | a pointer, null until the configuration built it | `m_clock`, `m_display`, `m_netHooks` |
| `clockServer()` | `UESimNetClockServer*` | `m_clockServer` (null on a client) |
| `diagnostics()` | `UELatencyBudgetDiagnostics<TraitsT>&` | `m_latencyBudgetDiagnostics` |
| `sampleInput()` | — | the binding's `BindingT::sampleInput(session)` |
| `onFrameStepsDue(tick, steps)`, `onPostPhysics()` | — | `onFrameStepsDue_Step` (PROBE A), `onPostPhysicsStep` (the test hitch first, then the frame's post-physics pass) |

## §2 Why it is outside the session class, and its accessors are defined after it

The backend is the session's member `m_physicsBackend`, so its class, and the tick game it holds by value, is instantiated while
the session class is still being defined, and the brawler's `PhysicsBackendHostUImpl.h` names it before the game's traits struct
is complete. A host nested in the session could not be named there, and the session's component types (its `SessionClock`,
`SessionDisplay`, `SessionNetHooks` specialisations) cannot be either. So the host is a namespace-scope template whose three types
the driver needs early (`Manager`, `Storage`, `RenderSnapshot`) are the binding's (`BrawlerCompositionUImpl`'s, as the brawler
binding's were), and whose component accessors are declared with deduced return types and defined in `UESimulationSession.h`
after the session class, where its members are complete. The tick game checks them where its doors are instantiated
(`SessionTickComponents`); the session asserts that the binding's `Manager` and `Storage` are its own `ManagerType` and `Storage`.

**Access.** The accessors read the session's private members: the session befriends the host template
(`template <typename BindingT> friend struct UESessionTickHost;`). That friend line replaced the one for the game's binding
(`TraitsT::TickDriverGame`, og-session-runtime task 26): the game's code no longer reaches the session's members; what it needs
is passed to it (the static data for `gravity`, the slot queries for `callWithSlotRequest`, from the backend).
