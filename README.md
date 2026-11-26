# Native process custody

[![Windows](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-windows.yml/badge.svg)](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-windows.yml)
[![Linux](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-linux.yml/badge.svg)](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-linux.yml)
[![macOS](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-macos.yml/badge.svg)](https://github.com/Varinomics/vnm_process_custody/actions/workflows/native-macos.yml)

This standalone library supplies process spawning, descriptor transport and a
separate command owner executable. The caller waits only its direct helper;
the helper retains the workload's wait authority. Workload exit and cleanup
settlement are separate events.

The core supports Windows, Linux and macOS, requires C++20 and CMake 3.21 or
newer, and has no Qt dependency. Its public target is
`vnm_process_custody::vnm_process_custody`.

Qt applications can enable `VNM_PROCESS_CUSTODY_BUILD_QPROCESS` and link
`vnm_process_custody::qprocess`. Installed-package consumers request the
`qprocess` component. The adapter starts a configured `QProcess` and keeps
its channels, signals and wait ownership. On Windows it supports job admission
and restriction of inherited handles to the child's standard streams.

The library, helper and optional adapters in this repository use the
[BSD 2-Clause License](LICENSE). The core package installs its license
and [notice](NOTICE) under `share/licenses/vnm_process_custody/` by default.

The optional [terminal process owner](adapter/docs/building.md) implements the
public, Qt-free `vnm_terminal::Terminal_process_owner` extension for POSIX hosts.
The surface supplies native terminal I/O and an ordinary built-in owner without
this repository. Enable `VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER` when an
application needs retained receipts and the enhanced cleanup/accounting scope.

## Build and use

For a standalone Linux or macOS build with the native tests:

```sh
cmake -S . -B ../custody-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DVNM_PROCESS_CUSTODY_BUILD_TESTS=ON
cmake --build ../custody-build --parallel 4
ctest --test-dir ../custody-build --output-on-failure
cmake --install ../custody-build --prefix ../custody-install
```

On macOS, add `-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0` to configuration. Native
tests are enabled by default when this is the top-level project and disabled
when it is added as a subdirectory. Windows builds the native static library
and standalone Job-object snapshot tests.
MSVC consumers using AddressSanitizer must also set
`VNM_PROCESS_CUSTODY_SANITIZE_ADDRESS=ON` so both sides use matching container
annotations.

An installed-package consumer sets `CMAKE_PREFIX_PATH` to the install prefix:

```cmake
find_package(vnm_process_custody CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE vnm_process_custody::vnm_process_custody)
vnm_process_custody_deploy_owner(my_application)
```

Source consumers can instead use `add_subdirectory()` and the same target and
deployment helper. On Linux and macOS, the deployment helper places the owner
executable beside the application. Applications with a different bundle layout
can supply `DESTINATION`; installed application layouts use
`vnm_process_custody_install_owner(DESTINATION ... COMPONENT ...)`. Windows
does not need a separate owner executable.

## Cleanup and observation scope

`Cleanup_scope` qualifies completion receipts. `UNSPECIFIED` means that the
sender supplied no recognized scope capability and cannot establish a scoped
cleanup claim. Linux and Windows owners report `TRANSITIVE_TREE`. Darwin owners
report `OWNED_PROCESS_GROUPS`. Optional receipt extensions have their own tags,
versions and lengths; an absent or unknown extension does not upgrade a claim.

Darwin CPU accounting observes a root and discoverable descendants. A process
starts contributing after its first observation, and credited CPU remains after
it exits. Work before that first observation and children that start and exit
between observations can be missed. The caller chooses the sampling interval;
the tracker owns no timer or process-control authority. Query failure reports
an unavailable sample rather than idle CPU.

## Darwin cleanup contract and technical debt

Darwin command ownership covers the directly owned root and its original
process group. The owner starts the root in a new session, observes its exit
without consuming its wait status, and retains the root zombie while observing
group membership. This pins the original numeric group while signaling it.
A membership snapshot containing only that zombie permits a confirmed receipt
and the owner's sole root reap. If an authorized forced-termination attempt and
bounded observation exhaust without that snapshot, a fresh exact-root exit
observation instead permits the sole reap and a terminal `UNCONFIRMED` receipt.
For example, another
parent may retain a dead group member indefinitely. The owner reports root exit
before this receipt and exits; the caller retains the negative cleanup outcome.
A failed signal remains an error, including Darwin's `EPERM` for a group of
zombies; it cannot prove settlement or prevent an exhausted negative receipt.
A live root remains pending. Exhausted cleanup never becomes confirmed on a
later call. The owner never signals a saved group after reaping, opens signal
authority by a discovered PID, or acquires another parent's exclusive wait.

Independent generation containment and orphan adoption are unavailable on
Darwin. `Process_tree_containment::establish()` reports that limitation, and
`terminate_coordinates()` returns false. Losing the final owner leaves cleanup
unconfirmed; helper exit, observation failure, an empty pipe, and root exit are
not replacement settlement receipts. A caller must retain and display the
unconfirmed outcome where cleanup cannot be established.

Deliberately detached descendants, including descendants that create a new
session, may survive. This is accepted technical debt for the initial native
Mac scope, not transitive process-family containment. Ordinary foreground and
background terminal jobs are not classified as deliberately detached: their
enhanced controlling-PTY lifecycle belongs to the optional terminal process owner.
The command helper's group receipt does not establish cleanup of interactive jobs
moved into other groups. Applications use the optional terminal process owner for
that scope.
The terminal process owner launches the same helper with `--darwin-pty-outcome-v1`.
It retains the controlling PTY session while the shell runs in a separate
foreground group. During cleanup, discovered group numbers are inputs to
`TIOCSPGRP`, which validates membership in that retained session; `TIOCSIG`
then signals the kernel-retained tty group. A fixed two-PID tty snapshot naming
only the helper establishes scoped settlement after the original root's sole
reap. Truncated or failed observations never establish emptiness. Ordinary
foreground and background jobs remain in scope after shell exit.

After a terminal settlement receipt, an admitted PTY helper retains its terminal
until control-channel EOF so the client can finish delivering accepted output.
Clients retire that output by half-closing their sending direction, then collect
the helper's native exit. Late commands do not change settlement. A rejected
start without a transferred master does not wait for output retirement.
Once output is retired, the helper revokes its retained terminal before exiting;
post-retirement writes cannot keep the session leader's exit drain pending.
Failure to retire the original terminal supplies no successful native outcome.

Before sending START, the optional owner requires an accepted native receipt hook:
the receiver must retain a local exact-helper status observer and persist its
identity and `darwin-pty-owner-v1` contract. Root birth and exact root exit are
separate retained receipts. This admitted helper's normal exit codes mean
`90 = NO_WORKLOAD`, `91 = CONFIRMED`, and `92 = UNCONFIRMED`. Signals, generic
exit codes, and unrelated helper modes carry no positive cleanup meaning.
The receiver consumes and latches the status event once and persists the raw
status before interpreting it; loss before persistence remains unknown.

Darwin process references are observation-only local kqueues. Since macOS does
not transfer kqueues through `SCM_RIGHTS`, a STARTED receipt carries the original
public `PROC_PIDTBSDINFO` birth identity in microseconds. The receiver checks that
identity before and after registering its own `NOTE_EXIT` observer. An already
exited workload may have no receiver observer while its birth and root-exit
receipts remain valid. STARTED establishes native birth, not successful exec.
An admitted command or PTY birth that fails before exec still supplies its exact
ROOT_EXITED receipt before START_REJECTED and final settlement. A failure before
birth, or a positively reaped child whose execution was never admitted, needs
no native birth/exit receipts.

Never consume the kqueue event: `poll(POLLIN)` remains
latched after the original occurrence dies. A consumer that verifies live
executable identity must check that observer before and after verification.
Neither a birth token nor a local observer authorizes a numeric signal.

Darwin control transport uses bounded length-prefixed Unix stream records.
Transferred descriptors belong to the first byte of one record and are received
close-on-exec. Receive timeouts retain partial records and their descriptors.
A malformed or oversized frame closes the stream. A bounded send owns every
partial write; if a partial record cannot complete, it closes the stream rather
than permitting a replay from an ambiguous byte boundary. Callers serialize
writes and abandon failed START requests.

The native tests cover the stable ownership, framing, descriptor and deadline
contracts. They do not establish integrated terminal job-control behavior,
authenticated Soter recovery, or application edition limits.
