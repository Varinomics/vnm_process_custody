# Terminal Process Owner Build And Integration

The optional adapter implements the public, Qt-free
`vnm_terminal::Terminal_process_owner` contract. Its target is
`vnm_process_custody::terminal_owner`; it links the custody core and the public
`vnm_terminal_surface::process_owner_contract` interface. The public surface
continues to provide native PTY/ConPTY I/O, deployment, and ordinary process
hosting independently of this repository.

## Source And Package Consumers

A host adds the surface before enabling the optional owner:

```cmake
add_subdirectory("../vnm_terminal_surface" "${CMAKE_BINARY_DIR}/surface")
set(VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER ON CACHE BOOL "" FORCE)
add_subdirectory("../vnm_process_custody" "${CMAKE_BINARY_DIR}/custody")
target_link_libraries(my_terminal PRIVATE
    vnm_terminal_surface::vnm_terminal_surface
    vnm_process_custody::terminal_owner)
vnm_process_custody_deploy_owner(my_terminal)
```

Installed consumers use
`find_package(vnm_process_custody_terminal_owner CONFIG REQUIRED)` and the same
owner target and deployment helper. Package producers enable
`VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER` and provide the surface package through
`CMAKE_PREFIX_PATH`. The option defaults to `OFF`; core-only consumers retain
the standalone core build.

The adapter package installs its BSD 2-Clause license and notice under
`share/licenses/vnm_process_custody_terminal_owner/` by default. The
terminal surface contract and the Qt Core library used on Linux and macOS
retain their own licenses. [Terminal surface license](https://github.com/Varinomics/vnm_terminal_surface/blob/master/LICENSE)

POSIX applications include `vnm_process_custody/terminal_process_owner.h`, create
an owner with `vnm::process_custody::make_terminal_process_owner(receipt_hook)`,
and pass it as the second argument to
`surface->start_terminal(request, std::move(owner))`. Ordinary standalone
terminal hosts call `start_terminal(request)` and need neither this adapter nor
its helper. Windows uses the surface's native process Job and rejects a supplied
POSIX owner before birth.

Darwin enhanced ownership requires retained native receipt observation. See
[Terminal process owner](process_owner.md) for admission, cleanup, output
retirement, and CPU coverage. These receipts are adapter configuration and are
not part of the public surface launch request.

## Tests

`VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER_TESTS=ON` enables the adapter's contract,
package, and enhanced-owner integration tests.
`VNM_PROCESS_CUSTODY_BUILD_TESTS=ON` separately enables the core's native tests.
The public surface owns the native backend, generic terminal, and ConPTY tests.

On Linux, the CPU accounting, owner-loss and client-loss tests need delegated
cgroup authority. The [Linux workflow](../../.github/workflows/native-linux.yml)
provides this delegation and rejects skipped accounting cases.
The [macOS workflow](../../.github/workflows/native-macos.yml) runs the core and
enhanced-owner tests in separate groups and records their inventory before
execution. Maintainers start both workflows manually.
