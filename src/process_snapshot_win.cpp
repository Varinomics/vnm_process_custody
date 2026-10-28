#include "vnm_process_custody/process_snapshot.h"

#include "process_snapshot_common.h"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace vnm::process_custody {
namespace {

std::uint64_t file_time_ticks(const FILETIME& time)
{
    return ((std::uint64_t)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

std::string process_name(HANDLE process)
{
    std::vector<wchar_t> path(1024);
    for (;;) {
        DWORD size = (DWORD)path.size();
        if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
            const auto begin = path.begin();
            const auto end = begin + size;
            const auto separator = std::find_if(std::make_reverse_iterator(end),
                std::make_reverse_iterator(begin), [](wchar_t value) {
                    return value == L'\\' || value == L'/';
                });
            const auto name = separator == std::make_reverse_iterator(begin)
                ? begin : separator.base();
            const int length = (int)(end - name);
            if (length <= 0) {
                return {};
            }
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, &*name, length,
                nullptr, 0, nullptr, nullptr);
            if (bytes <= 0) {
                return {};
            }
            std::string text((std::size_t)bytes, '\0');
            if (WideCharToMultiByte(CP_UTF8, 0, &*name, length,
                    text.data(), bytes, nullptr, nullptr) != bytes)
            {
                return {};
            }
            return text;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || path.size() >= 32768) {
            return {};
        }
        path.resize(path.size() * 2);
    }
}

} // namespace

std::optional<Process_snapshot> job_process_snapshot(Native_handle job)
{
    if (!job) {
        return std::nullopt;
    }
    auto snapshot = internal::make_process_snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    std::size_t capacity = 64;
    // A continuously forking workload must not hold a window refresh forever.
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (capacity > std::numeric_limits<DWORD>::max() / sizeof(ULONG_PTR) - 2) {
            return std::nullopt;
        }
        std::vector<ULONG_PTR> storage(capacity + 2);
        auto* list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
        const BOOL queried = QueryInformationJobObject(job, JobObjectBasicProcessIdList,
            list, (DWORD)(storage.size() * sizeof(ULONG_PTR)), nullptr);
        if (!queried || list->NumberOfProcessIdsInList < list->NumberOfAssignedProcesses) {
            if (!queried && GetLastError() != ERROR_MORE_DATA) {
                return std::nullopt;
            }
            capacity = std::max(capacity * 2, (std::size_t)list->NumberOfAssignedProcesses);
            continue;
        }
        for (DWORD index = 0; index < list->NumberOfProcessIdsInList; ++index) {
            const auto process_id = list->ProcessIdList[index];
            if (process_id == 0 || process_id > std::numeric_limits<DWORD>::max()) {
                continue;
            }
            Process_snapshot_entry entry;
            entry.process_id = (std::int64_t)process_id;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)process_id);
            if (!process) {
                if (GetLastError() == ERROR_INVALID_PARAMETER) {
                    continue;
                }
                snapshot->processes.push_back(std::move(entry));
                continue;
            }
            const auto close_process = [](HANDLE handle) { CloseHandle(handle); };
            std::unique_ptr<void, decltype(close_process)> retained(process, close_process);
            BOOL member = FALSE;
            if (!IsProcessInJob(process, job, &member)) {
                return std::nullopt;
            }
            if (!member) {
                continue;
            }
            FILETIME birth{}, exit{}, kernel{}, user{};
            if (GetProcessTimes(process, &birth, &exit, &kernel, &user)) {
                entry.birth_token = file_time_ticks(birth);
                const auto kernel_ticks = file_time_ticks(kernel);
                const auto user_ticks = file_time_ticks(user);
                if (kernel_ticks <= std::numeric_limits<std::uint64_t>::max() - user_ticks) {
                    entry.cpu_time_usec = (kernel_ticks + user_ticks) / 10;
                }
            }
            entry.name = process_name(process);
            PROCESS_MEMORY_COUNTERS memory{};
            if (GetProcessMemoryInfo(process, &memory, sizeof(memory))) {
                entry.resident_memory_bytes = (std::uint64_t)memory.WorkingSetSize;
            }
            snapshot->processes.push_back(std::move(entry));
        }
        return snapshot;
    }
    return std::nullopt;
}

} // namespace vnm::process_custody
