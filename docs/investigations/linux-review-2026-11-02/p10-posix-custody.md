# P10: POSIX custody and process snapshots

## Outcome and continuation

This first draft checkpoint preserves the bounded Linux threaded-cgroup and
Darwin source investigations while the available Linux measurements, transport
fix, and fixtures continue. It does not assert that pending gates passed.

Task branch: `work/linux-review-20261102-p10-posix-custody`.
Owner repositories: `vnm_process_custody`, `logonomic`, and
`vnm_terminal_surface`. Canonical checkouts remain on `master`; task worktrees
preserve uncommitted refinements independently of this report checkpoint.

| Finding | Current disposition | Next action |
|---|---|---|
| 28 | Single-event snapshot overflow remains source-confirmed in current Logonomic and framework. | Reproduce the 64 KiB callback boundary and implement complete transport. |
| 32 | POSIX snapshot acquisition remains synchronous on the worker GUI thread; user-visible latency unmeasured. | Measure the available Linux snapshot-to-input path before any threading change. |
| 51 | Owner helper periodic receive and allocation remain source-confirmed; costs under measurement. | Compare bounded baseline/candidate wakeups, CPU, allocation, and command/exit latency. |
| 143 | Adapter periodic receive remains source-confirmed; root/channel exit wake paths under examination. | Complete lifecycle evidence before changing wait behavior. |
| 52 | Darwin failure propagation confirmed in source; blanket different-UID claim narrowed. | Isolated native observation fixture and Apple Silicon execution. |
| 145 | Repeated Darwin observation confirmed in source; cost and safe reuse unmeasured. | Preserve occurrence checks and collect native paired evidence. |
| 144 | Collector read-path mismatch corroborated by primary Linux source; native reproduction unavailable here. | Prepare threaded plus nested-domain fixture; execute on a safely delegated host. |

The immediate next steps are focused local Linux baseline tests, bounded paired
measurements, snapshot transport reproduction, and the two native follow-up
fixtures. No hosted CI or live-user UI action is part of this task.

Source inspected: `vnm_process_custody` master
`a779a8f9cc39c1a5cfaa69f746e988562a82a7e6`, captured at task start.
This is provenance, not a dependency pin or certification.

## Finding 144 — Linux threaded cgroups

The source asymmetry remains in `src/process_snapshot_posix.cpp:27-43`.
`collect_members()` accepts `EOPNOTSUPP` from `openat()` only when an ancestor
has already provided membership. A negative `read()` other than `EINTR`
always fails collection, which makes `cgroup_process_snapshot()` unavailable.

Primary kernel evidence narrows the uncertainty. Linux v6.18
[`cgroup_procs_start()`](https://github.com/torvalds/linux/blob/v6.18/kernel/cgroup/cgroup.c#L5269-L5284)
returns `ERR_PTR(-EOPNOTSUPP)` for a threaded cgroup, and the
[`cgroup.procs` registration](https://github.com/torvalds/linux/blob/v6.18/kernel/cgroup/cgroup.c#L5433-L5441)
uses that function as its sequence start callback. This places the error on
the read path, rather than the initial file open. The
[kernel cgroup-v2 contract](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#threads)
puts every threaded-subtree process in its threaded domain's `cgroup.procs`;
the files in threaded descendants cannot be read. Ordinary nested domains
remain distinct and must still be traversed. A top-level threaded descriptor
cannot claim complete membership without a retained covering domain.

The local host reports kernel `6.18.57`, `/proc/self/cgroup` = `0::/`, and
this cgroup mount:

```text
154 153 0:24 / /sys/fs/cgroup ro,nosuid,nodev,noexec,relatime - cgroup2 cgroup2 rw,nsdelegate
```

`cgroup.type` is `domain`; `cgroup.subtree_control` is empty;
`access(/sys/fs/cgroup, W_OK)` is false. No writable delegated subtree was
identified. This rules out executing a native threaded-subtree fixture here;
it says nothing about ordinary process startup, pidfd, subreaper, or `/proc`
snapshot availability. No remount, delegation change, or unrelated process
migration is attempted.

**Disposition:** source plus primary-kernel corroboration, with native
reproduction still unperformed. Do not claim a reproduced Linux product
failure or silently broaden completeness semantics. A focused fixture is
being prepared to combine a threaded subtree with an ordinary nested-domain
sibling, assert the exact open-success/read-`EOPNOTSUPP` behavior, and check
that both workloads remain present. The absent-scope path must return 77.
Run it on a delegated Linux host before changing the production collector.

## Finding 52 — Darwin identity and optional detail

The source mechanism remains in
[`Process_cpu_tracker::cpu_time_usec()` and `process_snapshot()`](https://github.com/Varinomics/vnm_process_custody/blob/a779a8f9cc39c1a5cfaa69f746e988562a82a7e6/src/process_tree_darwin.cpp#L151-L316).
The discovery/accounting pass uses `PROC_PIDTASKALLINFO` to obtain both birth
identity and CPU counters. Any non-`ESRCH` failure for a known member preserves
its identity for retry and marks the sample unavailable; a newly discovered
child's comparable failure also marks the sample unavailable. If members
remain, `process_snapshot()` returns `nullopt` before constructing rows.
The later row-building code's ability to retain a row with absent details
does not rescue that earlier failure.

The original categorical claim, "whenever any process has another euid," is
too broad. Apple XNU revision `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`
[`proc_info.c`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/proc_info.c#L2195-L2213)
requires the same-user policy for both `PROC_PIDTASKALLINFO` **and**
`PROC_PIDTBSDINFO`. `PROC_PIDT_SHORTBSDINFO` and
`PROC_PIDUNIQIDENTIFIERINFO` are among the exempt flavors, but are not the
existing birth-time contract. The
[`proc_security_policy()`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/proc_info.c#L3160-L3204)
also applies MAC policy and permits privileged callers to bypass a mismatched
credential UID. Thus a plain TBSD fallback is not proven to restore this
product's occurrence identity for a privileged descendant. Kernel-source
inspection is not a native macOS execution result.

The public custody snapshot contract says that observations confer no process
control or wait authority and that a zero birth token must not seed a CPU-rate
interval. Keep that boundary. Unknown identity must not authorize a selected
process kill; missing optional CPU/RSS is a different state from missing
occurrence identity. No production Darwin behavior or kill authority is
changed by this investigation.

**Disposition:** retain the source-confirmed failure propagation, narrow the
blanket UID claim, and prepare a bounded native observation fixture. Native
Apple Silicon follow-up must include a normal shell, a long-lived `sudo` or
setuid descendant, readable siblings, a denied member discovered before and
after privilege change, and process identity selection through the actual
consumer. It must record return size and errno separately for full BSD
identity, short BSD details, unique identity, and task counters, plus snapshot
availability and elapsed time. Fixture output does not itself authorize
changing the birth-token contract.

## Finding 145 — repeated Darwin observations

The discovery/accounting pass obtains `PROC_PIDTASKALLINFO` for each readable
tracked or newly admitted member, then `process_snapshot()` obtains it again
for each retained member to build name, resident-memory, and per-process CPU
fields. This repeated call path is source-confirmed. The second read also
rechecks birth identity and can exclude an exited/recycled occurrence, so
discarding it would change sampling and liveness behavior.

**Disposition:** no optimization enabled. Native cost, magnitude, and
user-visible latency are unmeasured. The same isolated macOS fixture should
record bounded paired snapshots of a stable family and one with exits or
reparenting. Any candidate reuse must preserve occurrence checks and establish
the full snapshot-to-consumer path before production adoption.

## Checks and remaining work at this checkpoint

- Completed: current source inspection and exact Git-blob-verified source
  acquisition; read-only Linux cgroup mount/membership probe; primary Linux
  and XNU API implementation inspection.
- Pending in this lane: focused Linux ordinary and cgroup CTests, compilation
  of the new threaded fixture, and completion of the native probe source.
- Unavailable on this runner: writable delegated cgroups and native Darwin
  execution. A returned skip77 is not a pass. macOS compile/runtime and the
  real consumer's identity-safe selection remain native follow-up.
