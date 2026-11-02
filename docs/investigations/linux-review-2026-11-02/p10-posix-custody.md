# P10: POSIX custody and process snapshots

## Outcome and continuation

The Linux owner and adapter now wait on existing kernel/channel events instead
of waking every 25/100 ms. Focused owner and real-backend lifecycle checks pass,
and paired instrumentation confirms removal of the recurring idle wakeups and
64 KiB allocations. No extra worker thread or Darwin wait change was introduced.

The corrected Logonomic candidate keeps large replies fragmented through both
the 64 KiB host callback and Sintra's 262,144-byte frame boundary, assembling
only at the final consumer. The actual callback passes 21 QtTest results; the
real production RPC to an actual detached consumer in another native process
passes 13. The docked proxy translation unit compiles. These gates resolve the
two reproduced size bounds within their tested scope; full worker/runtime-owner,
docked execution and UI integration remain unexecuted.

Durable branches use `work/linux-review-20261102-p10-posix-custody`:

- [Custody PR #1](https://github.com/Varinomics/vnm_process_custody/pull/1)
  contains the Linux wait work, focused native fixtures and this investigation.
- [Logonomic PR #179](https://github.com/Varinomics/logonomic/pull/179)
  contains the snapshot transport work and records its current downstream gate.

Both are draft continuations. Canonical checkouts remain on `master`; task
worktrees preserve the candidates. The corrected transport refinement is
published and both final independent reviews found no blockers. Subsequent work
is the remaining application integration and the delegated Linux/native macOS
checks below.

## Disposition of every assigned finding

| Finding | Current evidence and result | Remaining boundary |
|---|---|---|
| **28 — oversized process events** | Both actual host-callback and downstream Sintra bounds reproduced. Published final-consumer fragmentation passes 21 callback and 13 native detached-delivery QtTest results; real docked proxy compiles. Both final independent reviews found no blockers. | Full worker/runtime-owner and docked execution; UI integration. |
| **32 — synchronous POSIX snapshot** | GUI-thread call chain and lock/lifetime requirements traced. Real adapter/snapshot/input measurements executed on a small no-cgroup workload; they do not establish a user-visible stall. No asynchronous production change made. | Dense family/host, admitted-cgroup and actual GUI latency measurements. |
| **51 — owner polling** | Baseline 25 ms runtime receive reproduced. Linux helper now waits on its retained SIGCHLD signalfd and control socket. Repeated idle activity disappeared in both paired runs; lifecycle checks pass. | Darwin behavior unchanged. No general CPU, latency or energy saving claimed. |
| **143 — adapter polling** | Baseline 100 ms runtime receive reproduced. Linux runtime event receive now blocks until input/EOF/error; startup retains its explicit deadline. Real stop, root exit, owner loss and EOF paths pass. | Darwin behavior unchanged; no new ownership or kill authority. |
| **52 — Darwin denied process details** | Whole-snapshot failure propagation remains source-confirmed. Primary XNU source narrows the blanket different-UID claim and shows that a full-BSD fallback also has a same-user requirement. Native probe supplied; production unchanged. | Apple Silicon compilation/execution and real consumer identity selection. |
| **145 — repeated Darwin observations** | Discovery/accounting and row construction both read task info. The second read also rechecks occurrence identity. No optimization enabled. | Native cost and safe reuse, with full affected consumer-path measurement. |
| **144 — threaded cgroup snapshot** | Linux kernel source corroborates read-time `EOPNOTSUPP`; collector only tolerates the corresponding open error. Mixed threaded/nested-domain reproducer compiled. Both delegated fixtures return skip77 on the actual read-only mount. | Execute on a writable delegated host before changing the production collector. |

## Sources, environment and evidence scope

Source selections captured for the gates are provenance, not forward dependency pins:

| Repository | Captured source |
|---|---|
| `vnm_process_custody` | `a779a8f9cc39c1a5cfaa69f746e988562a82a7e6` |
| `logonomic` | `958b4041ff64744656d5b8595e4c34150514ecaf` |
| `vnm_terminal_surface` | `02f80eddc41c0284357e21f4bb5413144da0e825` |
| `vnm_framework` | `11d1eadb` |
| `vnm_logger` | `924833ae` |
| `vnm_qt_dispatch` | `9244c66f` |
| `sintra` | `5edef79560f870ae51fe00a861c82c0688c02109` |
| `vnm_terminal` (docked header closure) | `c9bb0b7eb22bd169281bf90e284c11d0fc30cc24` |

The validated custody source/fixture publication is `8951c14679a250747f24b32036dbe762be8d0262`
(tree `8050471bc959f4fdc249c1bde958d33af53221c6`), following the first
report checkpoint `344aa457`. The initial Logonomic publication is `1d987f84`,
with the same source tree as the reviewed `beb275ad` candidate. The corrected
Logonomic publication is `50932d4aa0c8a5d869362c389f05d690b813ba3f`, with the
same tree (`07a43dfbfc9b7ee91cd125337abb5c1902e54842`) and parent as local
candidate `8673b4778f9138594e1e2502be248b2543020265`. All 11 candidate paths
match the SHA-256 freeze in `b33-candidate-source-hashes.json`; nine changed
relative to the initial publication. Both final independent reviewers verified
these hashes and all 184 inventory entries. The publication-only commit identity
change did not require repeating unaffected gates. No terminal-surface production
source or Sintra implementation was modified.

Publication-time Logonomic master is
`58f451b844e210eab083d6cc4bad45453ef15d1d`, 27 commits beyond the captured
baseline. The only affected-path overlap is an unrelated `CMakeLists.txt` hunk
moving an existing manager-test link; it does not overlap the new process-test
registrations or runtime/protocol/fixture sources. Newly added source-catalog
setup instructions were read. `logonomic-publication-upstream-assessment.json`
records this bounded assessment; the unrelated edits require no candidate change
or repeated test run.

The reused `logonomic-linux-development-sdk-2026.10.23.1` supplies Qt 6.12 and
external tools only. Its previously verified archive digest is
`56100ede7b00bd7faf06a7b64c57b94106f63e3ba1b77c0bbdf5d61d7ca220c2`;
the archive was not present to rehash here. All 24,138 immutable extracted
payload files were independently checked, with zero mismatches; 44 generated
Python bytecode files were excluded. `sdk-preflight` passed on Linux x86_64,
glibc 2.39 and GCC 13.3.0. Its optional SQL-driver/SpatialAudio dependency
warnings do not affect these gates. Preflight is not rendering or native
containment qualification.

Release builds used the actual source checkouts, Ninja and effective persistent
ccache. Configure held one queued build slot; compilation held two or four,
matching native parallelism and never exceeding four. The B35 source manifest
records 99 repository inputs. The corrected transport inventory records 184
source/header/generated inputs and 20 translation units, including MOC and the
docked compile-only unit; it separates executed targets from compilation alone.
The inventories and matching compiler commands identify what ran.

The Logonomic/Soter paired-path check passed over the 97-row shared inventory
for both the initial publication and the complete corrected candidate range
`origin/master..HEAD` at `8673b477`. The final result is
`paired-paths-logonomic-final.log`; the earlier log remains historical evidence.
No private hosted CI or live-user UI operation was performed.

## Linux owner and adapter correction

The helper already blocks SIGCHLD and owns a nonblocking signalfd before it
launches its workload. After reaping, its Linux live-root loop now polls that
retained descriptor together with the control socket. A pending SIGCHLD closes
the reap-to-wait race; child exit, a command, control EOF or poll failure takes
the existing handling/cleanup path. Only control readiness enters the protocol
receiver, using a zero timeout.

The adapter's single runtime event receiver uses an indefinite wait for
`wait_root()` and `finish()`. Startup still passes its remaining-deadline slice.
Stop uses the existing channel; send failure shuts down its write direction.
Owner death/channel close wakes receive; protocol failure retains the existing
unconfirmed outcome. Cleanup retry deadlines, root/descendant ownership,
occurrence checks and final settlement semantics remain unchanged.

This is confined to
[`src/owner_main_posix.cpp`](../../../src/owner_main_posix.cpp) and
[`adapter/src/posix_pty_owner.h`](../../../adapter/src/posix_pty_owner.h).
Darwin keeps its existing runtime wait path. Fresh independent review found no
source blocker in the Linux correction.

### What the measurements establish

The bounded probe uses the real private adapter, custody library and owner
helper, a disposable shell and one event-receiver thread. Each pair alternates
baseline/candidate order, warms up for 150 ms and measures 1.2 s idle. Six pairs
cycle twice through natural exit, stop and intentional owner loss. Each trial
also executes 20 input/echo and snapshot-before-input/echo pairs. One additional
six-pair run adds a root-receipt timestamp; no further timing search was used.

| Per 1.2 s idle interval | Baseline | Candidate |
|---|---:|---:|
| Helper voluntary context-switch delta | First run 45–47; receipt run 47–48 | 0 in every trial |
| Adapter receiver context-switch delta | 12 in every trial | 0 in every trial |
| Helper 64 KiB malloc requests | First run 45–47; receipt run 47 | 0 in every trial |
| Adapter receiver 64 KiB malloc requests | 12 in every trial | 0 in every trial |

These are direct evidence that recurring idle waits and their allocations were
removed. The same allocation auditor was used in both variants, but its writes
add baseline CPU work; scheduled-CPU deltas cannot be presented as absolute
production CPU savings. The blocked adapter may retain a buffer allocated before
the sample. Zero recurring allocations is not zero resident memory.

Timing samples support working command/lifecycle delivery, not an overall
speedup. The original candidate had a 28.237 ms natural-exit finish with no
intermediate timestamp; its cause remains unattributed. The later run localized
24.130 of one baseline's 24.817 ms finish before root receipt, consistent with
the 25 ms timer, but does not retroactively explain the original candidate
tail. Combined-ending medians are not an acceptance metric for performance.
No energy claim is made.

### Finding 32: measured scope and threading decision

Synchronous snapshot-to-input measurement covers the real adapter query,
followed by an input write and PTY echo. Every snapshot contained one session
process and reported cgroup accounting unavailable. A later read-only count
found 14 visible `/proc` process directories; it was not captured during each
trial.

| Median of per-trial medians | First baseline / candidate | Receipt-run baseline / candidate |
|---|---:|---:|
| Snapshot call | 182 / 183 us | 169.5 / 154.5 us |
| Snapshot then input/echo | 246.5 / 236.5 us | 276.5 / 206.5 us |
| Input/echo alone | 25 / 23 us | 32.5 / 23.5 us |

The snapshot implementation did not change. This small workload does not
establish a GUI stall or justify offloading. The source path asserts Qt object
affinity, holds the terminal-session recursive mutex, enters the backend's
public-call lifecycle guard and then the owner's CPU mutex. Moving a raw
pointer to QtConcurrent would not establish those lifetimes; keeping the same
lock while offloading could retain the contention. GUI scheduling, larger
families, framework transport and rendering remain outside this measurement.

## Snapshot transport across both bounded interfaces

The actual unchanged framework `Vnm_neutral_worker_host_services` callback
accepts 65,535/65,536-byte JSON and rejects 65,537 bytes before delivery. The
first production-serializer reproduction already exceeded the bound at 500
synthetic processes, producing 72,166 bytes; that reproduction modeled the
callback, while the later C ABI test executes it.

Current Sintra
[`5edef795`](https://github.com/imakris/sintra/blob/5edef79560f870ae51fe00a861c82c0688c02109/include/sintra/detail/messaging/message.h#L147-L185)
also rejects aligned complete frames at or above 262,144 bytes. The exact
generated `Terminal_window_control::process_snapshot_changed` message accepted
a 68,768-byte frame but rejected 274,161-byte and 685,161-byte JSON replies
with `std::length_error`. Those are frame-construction results. The separate
native delivery gate additionally confirms rejection of an unfragmented
5,000-process reply at the real RPC send boundary. Together they show why
reassembly before the final RPC was insufficient.

The corrected sender serializes the complete reply and fragments large bytes
into 45 KiB chunks in base64 JSON envelopes. Small replies retain their legacy
representation. The runtime owner validates the fragment structure and existing
worker source/stream identity, then forwards fragments without concatenating
them. Replies remain ephemeral and do not enter retained workspace/session
event projections. All four worker snapshot/kill emission sites use this
sender, including Windows completion paths; sending remains synchronous in the
existing request or completion continuation. Callback acceptance is not a new
end-consumer acknowledgment protocol.

| Route | Last bounded RPC | Final assembly |
|---|---|---|
| Docked | `Runtime_owner_event_sink::terminal_worker_event` | After sink provenance checks, the docked proxy checks terminal/generation and worker source/stream, then assembles before its existing complete `worker_event` signal. |
| Detached | `Terminal_window_control::process_snapshot_changed` | Exact method/sender/receiver and terminal/generation checks precede assembly. The existing complete-reply callback posts to the Processes window without another bounded IPC hop. |

Each final receiver owns its partial state. Docked stop/close or generation
replacement clears it; detached endpoint destruction drains its handlers.
A new first fragment replaces an interrupted reply, and a legacy complete
reply clears partial state. Only received bytes are allocated. Invalid encoding,
noncanonical/overflowing sizes, gaps, changed request/size, and invalid completed
JSON or reply schema cannot publish partial data. Wrong transported scope is
rejected before it can touch assembly state. Process entries, all kill results,
large Unicode names, exact counters, and empty/unavailable distinctions survive;
kill authority and CPU accounting semantics are unchanged.

The focused tests compile the real framework host callback and control support,
Logonomic sender/validator/assembler, owning logger, Qt dispatch, Sintra and
terminal-window protocol. Native delivery uses supported `spawn_swarm_process`
and the public production RPC into an actual `Terminal_window_control` in the
child. The child returns only callback count, byte length and SHA-256 through
a small observer RPC; a strict observer call on the same request ring confirms
dispatch without introducing another oversized return message. Child exit and
custody release are checked. No production provenance check is bypassed.

The real docked proxy translation unit also compiles, but was not linked or
executed. Full worker/runtime-owner service/client implementations and the
owner-to-desktop sink path were source-reviewed, not built or executed here.
Detached application callback wiring was inspected; no Processes UI was
instantiated. This establishes the callback and native detached-consumer paths,
with docked compilation, rather than a full application run.

The bounded full-root `linux_sdk_sources.py cache --offline` preflight stopped
at missing `vnm_qml_chrome`. Its original inventory lacked 18 of 24 direct
catalog entries and 17 of the runtime-owner target's 40 owned inputs; five
indirect declarations also needed resolution. Sintra, `vnm_terminal` and selected
owned headers were subsequently materialized for the focused gates. The original
counts are not a current full-tree inventory or evidence that Linux/the SDK
cannot run the complete gate.

The existing process-specific worker responsiveness method and its data rows
are entirely Windows-only. The cross-platform
`logonomic_session_process_snapshot_tests` additionally checks runtime-owner
request rejection and detached provenance, links the full runtime owner and was
not executed here. Full oversized worker delivery needs a maintained Linux case,
respecting P05 ownership of the shared worker test file.

## Focused checks actually executed

| Gate | Result and limit |
|---|---|
| Actual host callback and corrected sender/assembler | QtTest **21 passed, 0 failed, 0 skipped**, 160 ms, including init/cleanup. Exact/adjacent 64 KiB boundaries, 5,000 processes, long Unicode name, 2,000 kill entries, maximum request ID, integer limits, interrupted/corrupt fragments and callback failures covered. |
| Actual native detached RPC and final consumer | QtTest **13 passed, 0 failed, 0 skipped**, 429 ms, including init/cleanup; CTest **1/1**, 0.48 s. Large process/kill replies, huge Unicode name, empty/unavailable state, scope/provenance, malformed fragments and receiver replacement covered. |
| Actual docked proxy translation unit | Queued compile-only object target passes; not linked, instantiated or executed. |
| Ten selected custody owner/protocol/snapshot entries | Eight maintained entries passed on exact baseline and candidate. Candidate selection: **8 passed, 2 skipped, 0 failed**, 1.29 s. |
| Ordinary cgroup snapshot | **Skipped, exit77**, 0.12 s: `CPU accounting cgroup delegation unavailable: Read-only file system`. |
| New threaded cgroup snapshot | **Skipped, exit77**, 0.09 s, same admission failure. Fixture compiled; topology/error/completeness assertions did not execute. |
| Six maintained real-backend lifecycle cases | **All six passed** through current surface source, candidate adapter/library/helper. Three foreground cases used their required ordinary UID/EUID 65534. |
| Native Darwin probe | Source supplied; **not compiled or executed on macOS**. Three native recipe blocks passed `sh -n` only. |
| Paired application paths and diff integrity | Initial and complete corrected-candidate paired-path checks passed; scoped diff checks passed. |

The eight custody passes are ordinary snapshots, owner scenarios,
CPU-unavailable behavior, channel resources, started-descriptor ownership,
receive errors, late stop and start deadlines. They cover normal root exit,
stop/cancellation, owner loss, control EOF, PTY, START bounds, vacant standard
streams and interrupted/fragmented receives.

The six backend cases are foreground adoption, foreground host EOF,
foreground owner loss, detached exit output, detached exit-output tail and
family CPU observation. The tail case retains all 16,000 bytes. Owner loss
retains one unconfirmed record and a live foreground until cooperative fixture
cleanup; adoption/EOF confirm settlement and exact pidfd exit. The Linux family
CPU oracle accepts absent cgroup accounting, so its pass does not prove that
optional capability. The focused driver compiles real production backend
units; it is not a full canonical surface package/rendering build.

### Resolved setup failures and current limits

Initial sandbox execution failed eight selected native tests because that
context restricts socket/process inspection. The same unchanged assertions pass
in the authorized native execution context. Two early paired attempts stopped
before candidate execution on generated executable permissions; correcting the
generated binary modes resolved that setup failure.

The initial backend driver omitted QtGui while a real session header requires
QImage; adding the actual dependency fixed compilation. Three foreground cases
then rejected root as designed. Running the unchanged binaries at UID/EUID
65534 preserved their oracle; exact runtime libraries were copied to a readable
task directory and hash-checked after an initial SDK-directory loader failure.
All final cases pass. Earlier failure logs remain retained and are not relabeled
as candidate successes.

The first same-process delivery fixture had 3 passes and 10 failures: Sintra's
same-process shortcut lacked the transported provenance that the production
endpoint correctly requires. The failed run is preserved. Replacing only the
fixture topology with a supported separate native process resolved it; the final
13-result gate calls the public production RPC and does not manufacture message
provenance. Its preceding legacy-small smoke run passed three results including
init/cleanup, and is not added again to the full-target count.

Current gaps are the full-application source closure and integration execution,
docked consumer execution, read-only delegated cgroups and native Darwin
execution. The cgroup/OS limits do not imply unavailable ordinary Linux startup
or recovery.

## Linux threaded-cgroup disposition and native gate

The collector accepts `EOPNOTSUPP` from `openat()` only under a covering parent,
but fails every non-`EINTR` read error. Linux v6.18's
[`cgroup_procs_start()`](https://github.com/torvalds/linux/blob/v6.18/kernel/cgroup/cgroup.c#L5269-L5284)
returns that error through the sequence reader. The
[kernel cgroup-v2 contract](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html#threads)
places threaded-subtree processes in their threaded domain's membership file;
ordinary nested domains still require traversal. A threaded leaf alone lacks
complete membership authority.

On kernel 6.18.57, the actual native context has `/proc/self/cgroup = 0::/` and
`/sys/fs/cgroup` mounted `ro,nosuid,nodev,noexec,relatime`, type `cgroup2`, with
`nsdelegate`. `cgroup.type` is `domain`, subtree control is empty and write access
is false. The same result was checked outside the ordinary sandbox. No remount,
delegation alteration or unrelated process migration was attempted.

The new fixture creates `domain/nested` and `thread-root/leaf` inside its own
scope, places two retained workloads separately, proves open-success/read-error
at the real threaded file, and requires a complete combined result. Production
collection is unchanged. Its completeness assertion is expected to expose the
defect on a capable host; that is not an observed runtime failure here.

On a safely delegated native Linux host, run from the custody checkout with
that host's normal build queue and persistent compiler cache:

```sh
queued-build --slots 1 -- cmake -S . -B build/p10-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVNM_PROCESS_CUSTODY_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
queued-build --slots 4 -- cmake --build build/p10-linux --parallel 4 \
  --target vnm_process_custody_snapshot_tests
ctest --test-dir build/p10-linux --output-on-failure --no-tests=error \
  -R 'snapshot_tests$'
```

A skip77 remains unavailable evidence. Only after exact native reproduction,
repair the read-error handling at the covering-domain boundary and re-run the
ordinary nested-domain, threaded and top-level-leaf assertions together.

## Darwin identity, optional details and observation cost

In
[`process_tree_darwin.cpp`](../../../src/process_tree_darwin.cpp),
`cpu_time_usec()` uses full task info for identity and counters. A non-`ESRCH`
failure marks the observation unavailable, retaining known identity for retry.
`process_snapshot()` rejects that failed aggregate before reaching the later
row builder's optional-detail handling. That source failure propagation remains.

The inspected Apple XNU
[`proc_info.c`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/proc_info.c#L2195-L2213)
requires same-user policy for both `PROC_PIDTASKALLINFO` and `PROC_PIDTBSDINFO`.
Short BSD and unique-identity flavors are exempt but are not the existing
birth-time representation. Its
[`proc_security_policy()`](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/kern/proc_info.c#L3160-L3204)
also applies MAC policy and permits a privileged bypass. Therefore the blanket
different-UID claim and a simple full-BSD fallback are not established remedies.

The second task-info query obtains display/counter details and rechecks birth
identity. Reusing the first result would alter liveness/sampling behavior; no
cost or safe optimization is established by Linux. Observations still confer
no process-control/wait authority. Unknown birth identity cannot authorize a
kill or start a CPU-rate interval. The actual consumer's occurrence-safe
selection remains a native follow-up, distinct from missing optional CPU/RSS.

## Concrete isolated macOS fixture

`tests/process_snapshot_darwin_probe.cpp` is built as
`vnm_process_custody_darwin_snapshot_probe` when the normal test build is
configured on Darwin. It accepts `ROOT_PID [SAMPLES] [INTERVAL_MS]`, with
finite bounds of 32 samples and 500 ms between samples. It reads process
metadata and custody snapshots; it never signals or terminates a process and
never treats observed identity as kill authority. Exit 0 means the observations
were collected, **not** that unavailable snapshots were acceptable or that the
historical failure was refuted. It deliberately is not an automatic CTest for
an unexecuted privilege scenario.

Build on an Apple Silicon macOS host using that host's normal compiler cache
and build queue, then run the existing native observation gate:

```sh
queued-build --slots 1 -- cmake -S . -B build/p10-darwin -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVNM_PROCESS_CUSTODY_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
queued-build --slots 4 -- cmake --build build/p10-darwin --parallel 4 \
  --target vnm_process_custody_darwin_snapshot_probe vnm_process_custody_cpu_observation_tests
ctest --test-dir build/p10-darwin --output-on-failure \
  -R '^vnm_process_custody_cpu_observation_tests$'
probe="$(pwd)/build/p10-darwin/vnm_process_custody_darwin_snapshot_probe"
```

Run the following from a normal, unprivileged macOS account. All children
belong to a disposable shell and have finite sleep durations. `sudo -n` never
prompts; no sudoers or system configuration is changed. If the exact sleep
command is not permitted noninteractively, the run reports skip77 and does
not establish denied-detail coverage.

First capture a readable sibling and a member that starts readable, then
executes sudo:

```sh
/bin/sh -c '
  probe=$1
  /bin/sleep 6 & readable=$!
  (/bin/sleep 1; exec /usr/bin/sudo -n -- /bin/sleep 3) & changing=$!
  printf "fixture root=%s readable=%s changing=%s\n" "$$" "$readable" "$changing"
  "$probe" "$$" 20 250
  probe_status=$?
  wait "$changing"; elevated_status=$?
  wait "$readable"
  if [ "$elevated_status" -ne 0 ]; then
    printf "SKIP: sudo sleep was not established\n"
    exit 77
  fi
  exit "$probe_status"
' p10-observe "$probe"
```

Confirm in the output that `changing` had a readable birth-token row before
its UID transition. Scheduling that omits that phase does not establish the
known-member case. Compare later API errors and snapshot availability while
`readable` is still alive. Also repeat with the sudo sleep launched before
the observer, to exercise first discovery of a denied member:

```sh
/bin/sh -c '
  probe=$1
  /bin/sleep 5 & readable=$!
  /usr/bin/sudo -n -- /bin/sleep 4 & elevated=$!
  /bin/sleep 0.5
  printf "fixture root=%s readable=%s elevated=%s\n" "$$" "$readable" "$elevated"
  "$probe" "$$" 12 250
  probe_status=$?
  wait "$elevated"; elevated_status=$?
  wait "$readable"
  if [ "$elevated_status" -ne 0 ]; then
    printf "SKIP: sudo sleep was not established\n"
    exit 77
  fi
  exit "$probe_status"
' p10-observe "$probe"
```

The probe records raw BSD identity, short metadata, task-info failures,
per-row birth tokens, and bounded snapshot durations. It does not exercise
the actual Processes UI or process-selection handler; those remain a separate
native consumer gate. For performance work, pair identical workload runs
before and after any proposed change, preserve the same build and probe
configuration, and measure the consumer path as well as these native snapshot
times. Do not infer an optimization benefit merely from removing a syscall.

## Evidence and restart

The evidence bundle preserves the focused recipes, source manifests, raw logs
and bounded measurement JSON. Principal entries are:

- `snapshot-actual-callback-boundary.txt`, `snapshot-corrected-callback-case-log.txt`,
  `sintra-frame-before-correction.log`, `snapshot-native-delivery-tests.log`,
  `snapshot-native-delivery-case-log.txt`, `snapshot-native-delivery-results.xml`;
- `snapshot-delivery-local-fixture-failure.log`,
  `snapshot-docked-compile-build.log`, `snapshot-corrected-source-inventory.json`,
  `snapshot-corrected-compile-commands.json`,
  `snapshot-corrected-component-CMakeLists.txt`, `b33-candidate-source-hashes.json`;
- `b35/ctest-{baseline,candidate}-native.log`, `b35/paired-results.json`,
  `b35/paired-summary.json`, their `*-initial.json` counterparts and retained
  allocation records;
- `b35/backend-foreground-{adopted,host-eof,owner-loss}-ordinary.log`,
  `b35/backend-ctest-native.log`, `b35/source-manifest.json` and
  `b35/ordinary-runtime-manifest.json`;
- `sdk-preflight.log`, `sdk-integrity.json`, `paired-paths-logonomic-final.log`,
  `logonomic-publication-upstream-assessment.json` and
  `b33-source-closure-assessment.json`.

First inspect both PR branches and their working trees, preserving unique work.
Materialize the full current application source closure outside the SDK, run
the owning source-cache helper, configure through one queued slot, and build
`logonomic_session_process_snapshot_tests`,
`logonomic_session_process_transport_tests` and
`logonomic_session_process_delivery_tests` with at most four slots and matching
parallelism. Run their registered headless tests and the owning source-inventory
verification; add the missing docked/worker integration coverage within the
owning task boundaries. A full worker run uses its declared QRhi-capable OpenGL
environment; plain software Quick is rejected by worker initialization, and the
Windows-only process case is not Linux coverage. Do not repeat unaffected
completed gates or dispatch private hosted CI as an implicit substitute.
