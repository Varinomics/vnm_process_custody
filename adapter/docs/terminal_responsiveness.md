# Unresolved terminal responsiveness work

The remaining work spans `vnm_terminal_surface`, `vnm_terminal` and this
repository's optional POSIX owner adapter. Keep these obligations until their
implementation and integration are verified; remove completed items. Current
custody behavior is documented in [process owner](process_owner.md).

## Native operation completion and stop progress

Ownership: `vnm_terminal_surface/src/terminal_session.cpp`, its backend contract,
`src/windows_conpty_backend.cpp`, surface startup/resize integration and
`vnm_terminal/src/main.cpp`. The enhanced POSIX integration lives here in
`adapter/src/terminal_process_owner.cpp`, `adapter/src/posix_pty_owner.h` and
the native custody implementation under `src/` and `include/vnm_process_custody/`.

Move blocking native startup and resize work behind retained ownership and
integrate authoritative completion receipts end to end. Admission is not
completion: pending startup must not publish readiness; the startup receipt
precedes buffered output/exit, and current resize receipts certify geometry.
Stale callbacks cannot revive stopped or replaced sessions. Migrate application
failure/timeout handling and surface/session consumers together.

Preserve the implemented stop-commit distinction: committed stop errors leave
the session unready, while a genuinely noncommitting rejection restores the
exact prior state. Native handles, callbacks, module code and cleanup obligations
remain owned until retirement. A held native operation must not become a GUI
destructor or process/module-shutdown join.

Windows stop progress still needs to be independent of close retirement:
`request_conpty_close()` waits for active resize calls before escalation.
Moving that call to another thread alone does not resolve the dependency.
Cancellation before startup dispatch means no birth; stop during native startup
remains intent until its native owner is available. An arbitrary blocked OS call
is not made cancellable by a worker or a reserved FIFO slot.

## Bounded GUI work with input ordering

Ownership: `vnm_terminal_surface/src/vnm_terminal_surface.cpp`, session callback
drains and their input/session integration tests.

Replace input-reachable unlimited drains with bounded consumption. Input must
observe earlier admitted output that changes terminal modes, while later output
cannot extend that prerequisite indefinitely. Preserve original Qt event
accepted/ignored behavior and normal unhandled-event propagation; accepting an
event and replaying a copy does not satisfy this contract.

## Asynchronous capture storage

Ownership: `vnm_terminal_surface/src/backend_output_capture.cpp`, its public
progress contract and session construction/output/exit/destruction integration.

Provide bounded, ordered asynchronous storage while preserving capture bytes
and format. The owner requires responsive UI with an explicit exit-pending
state until storage settles. Requested finalization is not completed storage;
failure or overflow cannot publish a successful completed artifact. Missing or
unavailable capture data remains capability-local, without a shared schema bump.
Do not introduce an unbounded static-registry destruction join.

## Integration and closure

Coordinate session, capture and GUI edits through one owner for shared files.
Installed and explicit external process-owner providers must support changed
transport contracts or fail configuration precisely. The Linux startup transport
already shares one deadline for request transmission and receipt, and failed or
partial control writes close the sending direction; remaining application-level
integration coverage must preserve those contracts.

Focused verification must cover held native startup/resize with responsive GUI
and destruction, authoritative readiness/geometry, stale receipts, committed
and noncommitting stop failures, output/input mode ordering and unhandled Qt
keys, mouse, wheel, IME and selection. Capture checks cover held writes,
finalization, overflow and shutdown. Retain accounted ConPTY close/observer
retirement and finite, generation-safe autonomous atlas recovery, whose retry
budget internal rollback must not replenish. Native helpers alone do not prove
application startup, resize, input and shutdown integration.

Use the exact candidate source and run the relevant native Windows and Linux
checks. Record any unavailable native coverage. Maintainers can start the
[Windows](../../.github/workflows/native-windows.yml) and
[Linux](../../.github/workflows/native-linux.yml) workflows manually. Focus
qualification and independent review on the changed lifecycle and integration
behavior.

The reported disappearance of idle windows on mouseover did not establish a GPU
reset or a termination cause. Completing these corrections does not itself prove
that incident resolved. Report any irreducible held-call/process-lifetime limit
explicitly rather than hiding it behind asynchronous admission.
