#include <vnm_process_custody/process_tree.h>

#include <libproc.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <string_view>
#include <thread>
#include <vector>

namespace custody = vnm::process_custody;

namespace {

bool number(const char* text, std::uint64_t* value)
{
    const std::string_view input(text);
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), *value);
    return parsed.ec == std::errc() && parsed.ptr == input.data() + input.size();
}

std::uint64_t birth_token(const proc_bsdinfo& info)
{
    return info.pbi_start_tvsec * 1'000'000ULL + info.pbi_start_tvusec;
}

bool family(pid_t root, std::vector<pid_t>* members)
{
    members->assign(1, root);
    std::set<pid_t> selected{root};
    for (std::size_t index = 0; index < members->size(); ++index) {
        std::array<pid_t, 256> children{};
        errno = 0;
        const int count = proc_listchildpids((*members)[index], children.data(), sizeof(children));
        if (count < 0 || (count == 0 && errno != 0) || count >= (int)children.size()) {
            std::fprintf(stderr, "Family observation incomplete: parent=%d count=%d errno=%d\n",
                (*members)[index], count, errno);
            return false;
        }
        for (int child = 0; child < count; ++child) {
            if (children[child] > 1 && selected.insert(children[child]).second) {
                members->push_back(children[child]);
            }
        }
        if (members->size() > 1024) {
            std::fputs("Fixture family exceeds the 1024-process observation budget.\n", stderr);
            return false;
        }
    }
    return true;
}

void inspect(pid_t process_id)
{
    proc_bsdinfo identity{};
    errno = 0;
    const int identity_size = proc_pidinfo(process_id, PROC_PIDTBSDINFO, 0, &identity, sizeof(identity));
    const int identity_error = errno;

    proc_bsdshortinfo details{};
    errno = 0;
    const int detail_size = proc_pidinfo(process_id, PROC_PIDT_SHORTBSDINFO, 0, &details, sizeof(details));
    const int detail_error = errno;

    proc_taskallinfo task{};
    errno = 0;
    const int task_size = proc_pidinfo(process_id, PROC_PIDTASKALLINFO, 0, &task, sizeof(task));
    const int task_error = errno;

    std::printf("native pid=%d bsd_size=%d bsd_errno=%d birth=%llu "
        "short_size=%d short_errno=%d uid=%lu task_size=%d task_errno=%d\n",
        process_id, identity_size, identity_error,
        (unsigned long long)(identity_size == sizeof(identity) ? birth_token(identity) : 0),
        detail_size, detail_error,
        (unsigned long)(detail_size == sizeof(details) ? details.pbsi_uid : (uid_t)-1),
        task_size, task_error);
}

} // namespace

int main(int argc, char** argv)
{
    std::uint64_t root = 0, samples = 12, interval_ms = 250;
    if (argc < 2 || argc > 4 || !number(argv[1], &root) ||
        root <= 1 || root > (std::uint64_t)std::numeric_limits<pid_t>::max() ||
        (argc >= 3 && !number(argv[2], &samples)) ||
        (argc == 4 && !number(argv[3], &interval_ms)) ||
        samples == 0 || samples > 32 || interval_ms > 500)
    {
        std::fputs("Usage: vnm_process_custody_darwin_snapshot_probe ROOT_PID [SAMPLES<=32] "
            "[INTERVAL_MS<=500]\n", stderr);
        return 2;
    }

    proc_bsdinfo identity{};
    errno = 0;
    if (proc_pidinfo((pid_t)root, PROC_PIDTBSDINFO, 0, &identity, sizeof(identity)) != sizeof(identity) ||
        identity.pbi_pid != root || birth_token(identity) == 0)
    {
        std::fprintf(stderr, "Original root identity unavailable: pid=%llu errno=%d\n",
            (unsigned long long)root, errno);
        return 1;
    }
    const auto birth = birth_token(identity);
    std::printf("observer pid=%d uid=%lu euid=%lu root=%llu birth=%llu\n",
        getpid(), (unsigned long)getuid(), (unsigned long)geteuid(),
        (unsigned long long)root, (unsigned long long)birth);

    custody::Process_cpu_tracker tracker;
    tracker.set_root((std::int64_t)root, birth);
    for (std::uint64_t sample = 0; sample < samples; ++sample) {
        std::printf("sample=%llu\n", (unsigned long long)sample);
        std::vector<pid_t> members;
        if (!family((pid_t)root, &members)) {
            return 1;
        }
        for (const auto process_id : members) {
            inspect(process_id);
        }

        const auto started = std::chrono::steady_clock::now();
        const auto snapshot = tracker.process_snapshot();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::printf("snapshot available=%d rows=%zu elapsed_us=%lld\n",
            snapshot.has_value(), snapshot ? snapshot->processes.size() : 0, (long long)elapsed);
        if (snapshot) {
            for (const auto& entry : snapshot->processes) {
                std::printf("row pid=%lld birth=%llu cpu_available=%d rss_available=%d\n",
                    (long long)entry.process_id, (unsigned long long)entry.birth_token,
                    entry.cpu_time_usec.has_value(), entry.resident_memory_bytes.has_value());
            }
        }
        std::fflush(stdout);
        if (sample + 1 < samples && interval_ms != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
    // This is an observation tool, not a pass/fail oracle for the unexecuted
    // cross-user case. Rows and native errors above are the experiment result.
    return 0;
}
