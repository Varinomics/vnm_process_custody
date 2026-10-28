#pragma once

#include "vnm_process_custody/process_snapshot.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <chrono>
#include <limits>

namespace vnm::process_custody::internal {

inline std::optional<Process_snapshot> make_process_snapshot()
{
#if defined(_WIN32)
    const auto processor_count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else
    const auto processor_count = ::sysconf(_SC_NPROCESSORS_ONLN);
#endif
    const auto sampled_at = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (sampled_at <= 0 || processor_count <= 0 ||
        (std::uint64_t)processor_count > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    Process_snapshot snapshot;
    snapshot.sampled_at_usec = (std::uint64_t)sampled_at;
    snapshot.logical_processor_count = (std::uint32_t)processor_count;
    return snapshot;
}

} // namespace vnm::process_custody::internal
