#include <vnm_process_custody/process_snapshot.h>
#include <vnm_process_custody/process_spawn.h>

#include <windows.h>
#include <aclapi.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace custody = vnm::process_custody;

namespace {

using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

bool check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

std::uint64_t ticks(const FILETIME& value)
{
    return ((std::uint64_t)value.dwHighDateTime << 32) | value.dwLowDateTime;
}

std::optional<std::uint64_t> process_cpu_ticks()
{
    FILETIME birth{}, exit{}, kernel{}, user{};
    return GetProcessTimes(GetCurrentProcess(), &birth, &exit, &kernel, &user)
        ? std::optional(ticks(kernel) + ticks(user)) : std::nullopt;
}

int child_main(const char* ready_text, const char* work_text)
{
    const auto ready = reinterpret_cast<HANDLE>((std::uintptr_t)std::strtoull(ready_text, nullptr, 0));
    const auto work = reinterpret_cast<HANDLE>((std::uintptr_t)std::strtoull(work_text, nullptr, 0));
    std::vector<unsigned char> resident(8 * 1024 * 1024, 1);
    volatile unsigned char* bytes = resident.data();
    do {
        const auto before = process_cpu_ticks();
        if (!before) {
            return 1;
        }
        auto current = before;
        do {
            current = process_cpu_ticks();
            if (!current) {
                return 1;
            }
            const auto index = (std::size_t)(*current % resident.size());
            bytes[index] = bytes[index] ^ 1;
        }
        while (*current - *before < 800'000);
        if (!SetEvent(ready)) {
            return 1;
        }
    }
    while (WaitForSingleObject(work, INFINITE) == WAIT_OBJECT_0);
    return 1;
}

std::string executable_path()
{
    std::vector<wchar_t> path(32768);
    const auto size = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    if (size == 0 || size >= path.size()) {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, path.data(), (int)size, nullptr, 0, nullptr, nullptr);
    std::string text((std::size_t)bytes, '\0');
    return WideCharToMultiByte(CP_UTF8, 0, path.data(), (int)size, text.data(), bytes, nullptr, nullptr) == bytes
        ? text : std::string{};
}

struct Child
{
    Handle process{nullptr, &CloseHandle};
    Handle ready{nullptr, &CloseHandle};
    Handle work{nullptr, &CloseHandle};
    DWORD process_id = 0;
};

class Fixture
{
public:
    ~Fixture()
    {
        for (auto& child : m_children) {
            if (child.process) {
                TerminateProcess(child.process.get(), 0);
                WaitForSingleObject(child.process.get(), 5000);
            }
        }
    }

    bool start(std::size_t index, HANDLE job)
    {
        auto& child = m_children[index];
        child.ready.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        child.work.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        if (!child.ready || !child.work) {
            return false;
        }
        custody::Spawn_request request;
        request.executable = executable_path();
        request.argv = {request.executable, "--child", custody::Inherited_reference{0}, custody::Inherited_reference{1}};
        request.inherited = {{child.ready.get(), 3}, {child.work.get(), 4}};
        request.job = job;
        auto started = custody::spawn(request);
        child.process.reset(started.process_handle);
        child.process_id = (DWORD)started.process_id;
        return started.ok && WaitForSingleObject(child.ready.get(), 5000) == WAIT_OBJECT_0;
    }

    Child& child(std::size_t index) { return m_children[index]; }

    bool work(std::size_t index)
    {
        auto& child = m_children[index];
        return ResetEvent(child.ready.get()) && SetEvent(child.work.get()) &&
            WaitForSingleObject(child.ready.get(), 5000) == WAIT_OBJECT_0;
    }

private:
    std::array<Child, 3> m_children;
};

const custody::Process_snapshot_entry* find(const custody::Process_snapshot& snapshot, DWORD process_id)
{
    const auto found = std::find_if(snapshot.processes.begin(), snapshot.processes.end(),
        [process_id](const auto& entry) { return entry.process_id == process_id; });
    return found == snapshot.processes.end() ? nullptr : &*found;
}

std::optional<std::set<std::int64_t>> native_members(HANDLE job)
{
    // The fixture starts only three commands. Query the OS as the membership
    // oracle because Windows can also admit its hidden console-host processes.
    std::array<ULONG_PTR, 130> buffer{};
    auto* native = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.data());
    if (!QueryInformationJobObject(job, JobObjectBasicProcessIdList, native, sizeof(buffer), nullptr) ||
        native->NumberOfProcessIdsInList != native->NumberOfAssignedProcesses)
    {
        return std::nullopt;
    }
    std::set<std::int64_t> members;
    for (DWORD index = 0; index < native->NumberOfProcessIdsInList; ++index) {
        members.insert((std::int64_t)native->ProcessIdList[index]);
    }
    return members;
}

std::set<std::int64_t> snapshot_members(const custody::Process_snapshot& snapshot)
{
    std::set<std::int64_t> members;
    for (const auto& entry : snapshot.processes) {
        members.insert(entry.process_id);
    }
    return members;
}

bool inaccessible_metrics(HANDLE job, Child& child)
{
    HANDLE process_token = nullptr;
    if (!check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
            &process_token), "retain fixture process privileges"))
    {
        return false;
    }
    Handle token(process_token, &CloseHandle);
    TOKEN_PRIVILEGES disabled{};
    disabled.PrivilegeCount = 1;
    if (!check(LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &disabled.Privileges[0].Luid),
            "identify the privilege that bypasses process ACLs"))
    {
        return false;
    }
    PACL original_acl = nullptr;
    PSECURITY_DESCRIPTOR original = nullptr;
    if (!check(GetSecurityInfo(child.process.get(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
            nullptr, nullptr, &original_acl, nullptr, &original) == ERROR_SUCCESS,
            "retain fixture process ACL"))
    {
        return false;
    }
    const auto free_descriptor = [](void* descriptor) { LocalFree(descriptor); };
    std::unique_ptr<void, decltype(free_descriptor)> retained(original, free_descriptor);
    ACL denied{};
    if (!check(InitializeAcl(&denied, sizeof(denied), ACL_REVISION) &&
            SetSecurityInfo(child.process.get(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                nullptr, nullptr, &denied, nullptr) == ERROR_SUCCESS,
            "refuse new query handles for the fixture process"))
    {
        return false;
    }
    // Hosted administrators may bypass the empty ACL through this privilege.
    TOKEN_PRIVILEGES previous{};
    DWORD previous_size = 0;
    bool ok = check(
        AdjustTokenPrivileges(token.get(), FALSE, &disabled, sizeof(previous), &previous, &previous_size),
        "disable the fixture's process ACL bypass privilege");
    if (previous.PrivilegeCount != 0) {
        std::puts("Disabled SeDebugPrivilege for the native access-denial fixture");
    }
    Handle query(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, child.process_id), &CloseHandle);
    ok &= check(!query && GetLastError() == ERROR_ACCESS_DENIED,
        "native process query is denied before testing unavailable metrics");
    const auto snapshot = custody::job_process_snapshot(job);
    ok &= check(AdjustTokenPrivileges(token.get(), FALSE, &previous, 0, nullptr, nullptr),
        "restore fixture process privileges");
    const bool restored = SetSecurityInfo(child.process.get(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, original_acl, nullptr) == ERROR_SUCCESS;
    ok &= check(restored, "restore fixture process ACL");
    const auto* entry = snapshot ? find(*snapshot, child.process_id) : nullptr;
    ok &= check(entry != nullptr, "unreadable member remains in its Job snapshot");
    if (entry) {
        ok &= check(entry->birth_token == 0 && entry->name.empty() &&
                !entry->cpu_time_usec && !entry->resident_memory_bytes,
            "denied metrics are unavailable rather than zero");
    }
    return ok;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 4 && std::string(argv[1]) == "--child") {
        return child_main(argv[2], argv[3]);
    }
    Handle job(CreateJobObjectW(nullptr, nullptr), &CloseHandle);
    Handle nested(CreateJobObjectW(nullptr, nullptr), &CloseHandle);
    Fixture fixture;
    if (!check(job && nested && fixture.start(0, job.get()) && fixture.start(1, job.get()) &&
            fixture.start(2, nullptr), "start retained Job members and outside control"))
    {
        return 1;
    }
    bool ok = check(AssignProcessToJobObject(nested.get(), fixture.child(1).process.get()), "admit nested Job member");
    const auto first = custody::job_process_snapshot(job.get());
    const auto native = native_members(job.get());
    ok &= check(first && native && first->processes.size() == native->size() && snapshot_members(*first) == *native,
        "parent snapshot includes all and only native Job members");
    if (!first) {
        return 1;
    }
    const auto* member = find(*first, fixture.child(0).process_id);
    ok &= check(member && member->birth_token != 0 && !member->name.empty() &&
            member->cpu_time_usec && member->resident_memory_bytes && *member->resident_memory_bytes > 0,
        "native member has name, original identity, CPU and resident bytes");
    PROCESS_MEMORY_COUNTERS memory{};
    ok &= check(GetProcessMemoryInfo(fixture.child(0).process.get(), &memory, sizeof(memory)) &&
            member && member->resident_memory_bytes && *member->resident_memory_bytes >= memory.WorkingSetSize / 2 &&
            *member->resident_memory_bytes <= (std::uint64_t)memory.WorkingSetSize * 2,
        "resident memory is measured in independently checked bytes");
    ok &= check(find(*first, fixture.child(1).process_id) && !find(*first, fixture.child(2).process_id),
        "nested member is included and unrelated process is excluded");
    const auto inner = custody::job_process_snapshot(nested.get());
    const auto native_inner = native_members(nested.get());
    ok &= check(inner && native_inner && inner->processes.size() == native_inner->size() && snapshot_members(*inner) == *native_inner &&
            find(*inner, fixture.child(1).process_id) && !find(*inner, fixture.child(0).process_id),
        "inner scope excludes its sibling");
    ok &= check(first->sampled_at_usec > 0 &&
            first->logical_processor_count == GetActiveProcessorCount(ALL_PROCESSOR_GROUPS),
        "sample clock and whole-machine CPU divisor are present");
    ok &= check(fixture.work(0), "request independently measured process CPU work");
    const auto second = custody::job_process_snapshot(job.get());
    const auto* later = second ? find(*second, fixture.child(0).process_id) : nullptr;
    ok &= check(member && later && later->birth_token == member->birth_token && member->cpu_time_usec &&
            later->cpu_time_usec && *later->cpu_time_usec >= *member->cpu_time_usec + 60'000 &&
            *later->cpu_time_usec <= *member->cpu_time_usec + 250'000 &&
            second->sampled_at_usec > first->sampled_at_usec,
        "same occurrence accumulates independently exercised CPU in microseconds");
    ok &= inaccessible_metrics(job.get(), fixture.child(0));
    ok &= check(!custody::job_process_snapshot(nullptr), "absent Job is unavailable");
    ok &= check(TerminateJobObject(job.get(), 0) &&
            WaitForSingleObject(fixture.child(0).process.get(), 5000) == WAIT_OBJECT_0 &&
            WaitForSingleObject(fixture.child(1).process.get(), 5000) == WAIT_OBJECT_0,
        "owned members leave the scope after native termination");
    auto ended = custody::job_process_snapshot(job.get());
    for (int attempt = 0; ended && !ended->processes.empty() && attempt < 500; ++attempt) {
        Sleep(10);
        ended = custody::job_process_snapshot(job.get());
    }
    ok &= check(ended && ended->processes.empty(), "retained settled Job yields an empty snapshot");
    std::puts(ok ? "PASS: native Job process snapshots" : "FAIL: native Job process snapshots");
    return ok ? 0 : 1;
}
