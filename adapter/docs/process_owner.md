# Terminal Process Owner

The optional adapter supplies `vnm_terminal::Terminal_process_owner` to the
public surface's built-in POSIX backend. The surface continues to own PTY I/O,
input and output queues, resize, parser/model state, and rendering. Standalone
terminal hosts can use the built-in owner without this repository.

Applications create an enhanced owner through
`vnm::process_custody::make_terminal_process_owner(receipt_hook)` and pass it as
the second argument to `surface->start_terminal(request, std::move(owner))`.
Owner selection happens before birth; an enhanced owner's failed admission
never falls back to an unmanaged launch.

## Birth And Receipt Admission

The enhanced POSIX owner launches `vnm_process_custody_owner` beside the hosting
executable. That helper owns the workload's sole wait and native cleanup; the
adapter owns its helper's sole wait. Linux starts the shell in a new session and
uses the helper's subreaper for tree cleanup. On macOS, the persistent helper
retains the controlling PTY session and places the shell in its own foreground
process group within that session.

Darwin workload START requires `Terminal_native_receipt_hook` to acknowledge
retention and persistence of the original helper's identity and status observer.
The hook separately receives exact root birth, root exit, or no-birth evidence;
revoking presentation callbacks does not revoke these native obligations.

The helper withholds execution until it retains the child's native identity and
confirms that START remains admitted. A rejected gate permits no-workload
evidence only when no child was born or the still-gated child was positively
reaped. An admitted or uncertain birth retains its exact identity and root-exit
evidence separately from startup rejection. Startup failure never fabricates
group settlement. Native cleanup continues independently of the bounded startup
return.

## Cleanup Scope

STOP carries the requested grace interval to the persistent helper, which
attempts graceful termination and then forced cleanup. On macOS it signals
ordinary foreground and background groups through the retained tty and keeps
the original root's sole wait. After collecting that wait, a tty membership
snapshot naming only the retained helper confirms scoped cleanup. Bounded
exhaustion while inert members remain produces terminal `UNCONFIRMED`; failure
or timeout never proves emptiness. A live root remains pending. A failed STOP
send closes the sending direction so control EOF requests the same cleanup.

The helper starts cleanup on root exit independently of presentation output
drain and the exit callback. Root exit does not establish job emptiness, and
callback delivery does not release a native cleanup obligation. Deliberately
detached descendants can survive on macOS: its guarantee is owned-process-group
cleanup, not transitive containment. Helper death without an admitted terminal
outcome leaves native cleanup unconfirmed.

After macOS cleanup settlement, the helper retains its controlling terminal
until the client retires output through control-channel EOF. `finish()`
half-closes the client's sending direction before closing and collecting the
helper, so a duplicate endpoint cannot delay normal retirement. An endpoint
leaked to another live process can delay EOF after client abandonment. The
helper then revokes its retained terminal so later producer output cannot block
the session leader's native exit. Drain-control failures do not establish
complete output delivery.

Retaining the controlling session lets the backend drain output after the shell
exits, including output from an external writer, output following a terminal
reset, and output produced during natural cleanup. Delivery remains bounded by
the final drain and output retirement. Those guarantees depend on this owner.
With the public direct-child owner, the session leader revokes its controlling terminal
on exit; ordinary hangup/reset and explicit graceful-stop tail handling do not
establish the same scope after root exit.

## CPU Accounting

The owner returns cumulative CPU time with its actual coverage scope. Linux
uses native workload accounting when the required delegated containment is
available. Darwin observes the root and discoverable descendants; CPU credited
before exit remains in the total. Work before first observation and children
that start and exit between observations can be missed. Detached descendants
may escape observation. A failed query remains unavailable, not idle CPU.

The tracker owns no sampling timer. Applications choose the polling interval
and must treat samples from different launches as unrelated. Logonomic schedules
its CPU polling in the application, independently of PTY exit and cleanup
checks. See the [core custody contract](../../README.md) for receipt scope,
observed-family limits, and the distinction between root exit and cleanup settlement.
