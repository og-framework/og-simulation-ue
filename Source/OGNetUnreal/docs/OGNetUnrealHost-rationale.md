<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetUnrealHost.h` and `.cpp` — rationale

`FOGNetUnrealHost` is og-net's host services (`ognet::NetHostServices`) for Unreal Engine: the clock, the thread factory,
the log sink and the random source an og-net endpoint asks its host for. This file is the docs pointer of
`OGNetUnrealHost.h`, `OGNetUnrealHost.cpp` and the automation test `OGNetUnrealHostTest.cpp`. The source holds the licence,
a docs pointer, code and four `static_assert`s; it states no prohibition, so there is no guards file.

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 Why a host of its own

og-net ships `ognet::StdHostServices` for engine-free hosts, and it does not fit an Unreal process: its `log` prints to
stdout, and its `startThread` records the priority it is given but starts a plain `std::thread` without applying it. Inside
the engine, og-net's thread should be an engine thread at the priority og-net asks for, and its lines should go to the
engine's log. The class is non-copyable because it owns the threads it starts.

The header ends with `static_assert(ognet::NetHostServices<FOGNetUnrealHost>)`, so a change that breaks the concept fails the
compile of every file that includes the header.

## §2 `nowSeconds`

FPlatformTime::Seconds. On Windows that is QueryPerformanceCounter scaled to seconds (WindowsPlatformTime.h, lines 23-30):
monotonic and high-resolution, as og-net's timers need. The engine adds a large constant to the value; og-net only ever
subtracts two readings.

## §3 `startThread` and the thread priorities

- Each call wraps `fn(arg)` in a small FRunnable, whose Run calls the function once and returns, and starts it with
  FRunnableThread::Create(runnable, name, 0, priority): the UTF-8 name converted with UTF8_TO_TCHAR, stack size 0 (the
  platform's default), and the priority from `FOGNetUnrealHost::toThreadPriority`.
- `toThreadPriority` maps og-net's three priorities, each pinned by a `static_assert` in the header:

  | og-net (`ognet::ThreadPriority`) | Unreal |
  |---|---|
  | Normal | TPri_Normal |
  | High | TPri_AboveNormal |
  | TimeCritical | TPri_TimeCritical |

  og-net's endpoint starts its own I/O thread as `"OGNet"` at High (`EndpointCore.cpp`), so in own-thread mode og-net runs
  above normal priority. A value outside the three maps to TPri_Normal, the engine's own default.
- **Ownership.** The host keeps every runnable and thread it creates. Its destructor takes the list under the lock, then for
  each one calls WaitForCompletion, deletes the thread and then the runnable. The thread goes first because the Windows
  thread's destructor calls Kill(true), which calls the runnable's Stop (MicrosoftRunnableThread.h, lines 51-57 and 79-88).
  So the host must be destroyed after its endpoint has stopped and its thread function has returned, as with
  `StdHostServices`, whose destructor joins its threads.
- **Any thread may call it.** A critical section guards the list.
- **Failure is fatal.** A null function, or a null thread from FRunnableThread::Create (which returns null when the platform
  has no threads and the runnable offers no single-thread interface, ThreadingBase.cpp, lines 851-881), stops the process
  through `OGNET_CHECK`. og-net has no way to run without the thread it asked for.

## §4 `log`

og-net's levels map to Unreal verbosities on `LogOGNetTransport` (`OGNetUnreal-rationale.md` §2):

| og-net (`ognet::LogLevel`) | Unreal |
|---|---|
| Error | Error |
| Warning | Warning |
| Info | Log |
| Verbose | Verbose |

A null line logs as an empty line, and a value outside the four logs at Log, as `StdHostServices` prints such a line as
`Log`. Each UE_LOG converts the UTF-8 text with UTF8_TO_TCHAR inside its own argument list: the macro yields a temporary
converter, so a converted pointer kept in a variable would point at freed storage by the next statement. Verbose lines are
written only when the category's verbosity is raised (its default is Log).

## §5 `randomBytes`

`ognet::osRandomBytes`, the operating system's random source, followed by `OGNET_CHECK` on its result, exactly as
`StdHostServices` does: og-net's handshake draws its key pairs from it (`Handshake.cpp`), and there is no safe fallback when it fails.

## §6 The automation test `OGNet.Unreal.HostServices`

The test (EditorContext, ProductFilter) checks each service through the class itself:

- **The thread.** `startThread("OGNet", High, ...)` runs a function that records, from
  FRunnableThread::GetRunnableThread, the thread's name and priority, then triggers an event. The test waits up to 5 s and
  then checks the name `OGNet` and the priority TPri_AboveNormal. The probe the function writes into is declared before the
  host, so a thread that outlives a failed wait still writes into live memory: the host's destructor waits for it first.
  Measured: mapping High to TPri_Normal (with its `static_assert`) fails the test with
  "Expected 'thread priority (og-net High)' to be 1, but it was 0."
- **The clock.** 1,000 consecutive `nowSeconds()` readings never decrease.
- **The log sink.** An FOutputDevice added to GLog records the lines of `LogOGNetTransport`. It reports
  CanBeUsedOnMultipleThreads, so GLog's redirector hands it each line synchronously on the logging thread instead of
  buffering it for the primary thread (OutputDeviceRedirector.cpp, lines 386-405 and 724-727). The test logs one Info line
  and one Warning line and checks that each arrives once, at Log and at Warning.
  - The Warning line is deliberately **not** declared with AddExpectedMessage. While a test runs, Warning and Error lines
    reach GLog through GWarn (LogMacros.cpp, lines 28-43), and the automation framework's filter on GWarn re-emits a line
    that matches an expected message at Verbose (AutomationTest.cpp, lines 279-295). A device on GLog then sees Verbose,
    never Warning: measured, the check failed with "Expected 'Warning line verbosity' to be 3, but it was 6." Undeclared,
    the line is reported as a test warning, and the test passes unless the project elevates log warnings to errors in its
    automation settings (this project does not).
- **The random source.** 32 bytes from `randomBytes` are not all zero.
