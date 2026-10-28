#pragma once

#include "vnm_process_custody/process_pipe.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vnm::process_custody {

struct Process_snapshot_entry
{
    std::int64_t                 process_id = 0;
    std::uint64_t                birth_token = 0;
    std::string                  name;
    std::optional<std::uint64_t> cpu_time_usec;
    std::optional<std::uint64_t> resident_memory_bytes;
};

// Observations confer no process-control or wait authority. A zero birth token
// means the occurrence could not be read; it must not seed a CPU-rate interval.
struct Process_snapshot
{
    std::uint64_t                       sampled_at_usec = 0;
    std::uint32_t                       logical_processor_count = 0;
    std::vector<Process_snapshot_entry> processes;
};

#if defined(_WIN32)
// The caller retains the Job. Includes its nested Jobs, never the host family.
std::optional<Process_snapshot> job_process_snapshot(Native_handle job);
#elif defined(__linux__)
// The caller retains the cgroup directory. Includes descendant cgroups.
std::optional<Process_snapshot> cgroup_process_snapshot(int directory);
// Capture a birth token only while the caller retains the original process
// occurrence. Family observation is read-only and best effort, like /proc.
std::optional<std::uint64_t> process_birth_token(std::int64_t process_id);
std::optional<Process_snapshot> observed_process_family_snapshot(
    std::int64_t root_process_id, std::uint64_t root_birth_token, bool include_root = true);
#endif

} // namespace vnm::process_custody
