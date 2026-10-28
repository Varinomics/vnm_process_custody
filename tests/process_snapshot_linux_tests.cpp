#include <vnm_process_custody/process_snapshot.h>
#include <vnm_process_custody/process_tree.h>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace custody = vnm::process_custody;

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

std::uint64_t cpu_usec()
{
    timespec value{};
    return clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) == 0
        ? (std::uint64_t)value.tv_sec * 1'000'000 + value.tv_nsec / 1000 : 0;
}

struct response_t
{
    pid_t process_id = 0;
    pid_t intermediate_id = 0;
    char kind = 0;
};

int run_workload(int commands, int responses)
{
    if (setsid() < 0) {
        return 1;
    }
    std::vector<unsigned char> memory(8 * 1024 * 1024, 1);
    volatile unsigned char* bytes = memory.data();
    char command = 'W';
    do {
        const auto before = cpu_usec();
        auto current = before;
        do {
            current = cpu_usec();
            const auto index = (std::size_t)(current % memory.size());
            bytes[index] = bytes[index] ^ 1;
        }
        while (current - before < 80'000);
        const response_t response{getpid(), getppid(), 'W'};
        if (write(responses, &response, sizeof(response)) != sizeof(response)) {
            return 1;
        }
    }
    while (read(commands, &command, 1) == 1 && command == 'W');
    return 0;
}

class Fixture
{
public:
    ~Fixture()
    {
        if (m_commands >= 0) {
            close(m_commands);
        }
        if (m_control >= 0) {
            (void)write(m_control, "X", 1);
            close(m_control);
        }
        if (m_helper > 0) {
            int status = 0;
            while (waitpid(m_helper, &status, 0) < 0 && errno == EINTR) {}
        }
        if (m_responses >= 0) {
            close(m_responses);
        }
    }

    bool start()
    {
        int commands[2]{-1, -1}, responses[2]{-1, -1}, control[2]{-1, -1};
        if (pipe(commands) || pipe(responses) || pipe(control)) {
            for (int descriptor : {commands[0], commands[1], responses[0], responses[1], control[0], control[1]}) {
                if (descriptor >= 0) {
                    close(descriptor);
                }
            }
            return false;
        }
        m_helper = fork();
        if (m_helper == 0) {
            close(commands[1]);
            close(responses[0]);
            close(control[1]);
            if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
                _exit(1);
            }
            pid_t intermediate = fork();
            if (intermediate == 0) {
                close(control[0]);
                const pid_t workload = fork();
                if (workload == 0) {
                    _exit(run_workload(commands[0], responses[1]));
                }
                if (workload < 0) {
                    _exit(1);
                }
                close(commands[0]);
                close(responses[1]);
                for (;;) {
                    pause();
                }
            }
            close(commands[0]);
            if (intermediate < 0) {
                _exit(1);
            }
            char command = 0;
            while (read(control[0], &command, 1) == 1) {
                if (intermediate > 0) {
                    kill(intermediate, SIGTERM);
                    int status = 0;
                    while (waitpid(intermediate, &status, 0) < 0 && errno == EINTR) {}
                    intermediate = -1;
                }
                if (command == 'X') {
                    int status = 0;
                    while (waitpid(-1, &status, 0) > 0 || errno == EINTR) {}
                    _exit(0);
                }
                const response_t response{getpid(), 0, 'R'};
                if (write(responses[1], &response, sizeof(response)) != sizeof(response)) {
                    _exit(1);
                }
            }
            _exit(1);
        }
        close(commands[0]);
        close(responses[1]);
        close(control[0]);
        m_commands = commands[1];
        m_responses = responses[0];
        m_control = control[1];
        response_t ready;
        if (m_helper < 0 || !receive(&ready) || ready.kind != 'W') {
            return false;
        }
        m_workload = ready.process_id;
        m_intermediate = ready.intermediate_id;
        return true;
    }

    bool work()
    {
        response_t ready;
        return write(m_commands, "W", 1) == 1 && receive(&ready) && ready.kind == 'W';
    }

    bool reparent()
    {
        response_t ready;
        return write(m_control, "R", 1) == 1 && receive(&ready) && ready.kind == 'R';
    }

    pid_t helper() const { return m_helper; }
    pid_t workload() const { return m_workload; }
    pid_t intermediate() const { return m_intermediate; }

private:
    bool receive(response_t* response)
    {
        pollfd ready{m_responses, POLLIN, 0};
        return poll(&ready, 1, 5000) == 1 && read(m_responses, response, sizeof(*response)) == sizeof(*response);
    }

    pid_t m_helper = -1;
    pid_t m_workload = -1;
    pid_t m_intermediate = -1;
    int m_commands = -1;
    int m_responses = -1;
    int m_control = -1;
};

const custody::Process_snapshot_entry* find(const custody::Process_snapshot& snapshot, pid_t process_id)
{
    const auto found = std::find_if(snapshot.processes.begin(), snapshot.processes.end(),
        [process_id](const auto& entry) { return entry.process_id == process_id; });
    return found == snapshot.processes.end() ? nullptr : &*found;
}

} // namespace

int main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);
    Fixture fixture;
    if (!check(fixture.start(), "start dedicated subreaper, intermediate, and detached workload")) {
        return 1;
    }
    if (argc == 2 && std::string(argv[1]) == "--cgroup") {
        custody::Process_cpu_scope scope;
        std::string error;
        if (!scope.establish("snapshot-" + std::to_string(getpid()), &error)) {
            std::printf("SKIP: native cgroup snapshot: %s\n", error.c_str());
            return 77;
        }
        custody::Spawn_request request;
        scope.configure_spawn(request);
        const int directory = request.cpu_accounting_cgroup;
        if (!check(mkdirat(directory, "snapshot-child", 0700) == 0, "create a real descendant cgroup")) {
            return 1;
        }
        const int membership = openat(directory, "snapshot-child/cgroup.procs", O_WRONLY | O_CLOEXEC);
        const auto identifier = std::to_string(fixture.workload());
        const bool moved = membership >= 0 && write(membership, identifier.data(), identifier.size()) == (ssize_t)identifier.size();
        if (membership >= 0) {
            close(membership);
        }
        if (!check(moved, "move the retained fixture workload into the descendant cgroup")) {
            return 1;
        }
        const auto snapshot = scope.process_snapshot();
        const bool ok = check(snapshot && snapshot->processes.size() == 1 && find(*snapshot, fixture.workload()) &&
                !find(*snapshot, fixture.helper()) && !find(*snapshot, getpid()),
            "cgroup snapshot includes nested member and excludes owner and caller");
        std::puts(ok ? "PASS: native cgroup process snapshots" : "FAIL: native cgroup process snapshots");
        return ok ? 0 : 1;
    }
    const auto birth = custody::process_birth_token(fixture.helper());
    if (!check(birth.has_value(), "capture the original retained helper identity")) {
        return 1;
    }
    bool ok = check(!custody::observed_process_family_snapshot(fixture.helper(), *birth + 1),
        "recycled or mismatched root identity cannot select a family");
    const auto first = custody::observed_process_family_snapshot(fixture.helper(), *birth, false);
    ok &= check(first && first->processes.size() == 2 && find(*first, fixture.intermediate()) &&
            find(*first, fixture.workload()) && !find(*first, fixture.helper()) && !find(*first, getpid()),
        "private helper scope includes descendants and excludes helper and unrelated caller");
    const auto* workload = first ? find(*first, fixture.workload()) : nullptr;
    ok &= check(workload && workload->birth_token && !workload->name.empty() && workload->cpu_time_usec &&
            workload->resident_memory_bytes && *workload->resident_memory_bytes >= 4 * 1024 * 1024,
        "workload exposes original identity, name, CPU microseconds, and resident bytes");
    ok &= check(fixture.work(), "request independent CPU-clock work");
    const auto later = custody::observed_process_family_snapshot(fixture.helper(), *birth, false);
    const auto* active = later ? find(*later, fixture.workload()) : nullptr;
    ok &= check(workload && workload->cpu_time_usec && active && active->birth_token == workload->birth_token && active->cpu_time_usec &&
            *active->cpu_time_usec >= *workload->cpu_time_usec + 60'000 &&
            *active->cpu_time_usec <= *workload->cpu_time_usec + 250'000,
        "same process counter uses independently checked microseconds");
    ok &= check(fixture.reparent(), "retire intermediate while workload survives in its custody helper");
    const auto orphan = custody::observed_process_family_snapshot(fixture.helper(), *birth, false);
    ok &= check(orphan && orphan->processes.size() == 1 && find(*orphan, fixture.workload()),
        "subreaper adoption preserves the surviving detached workload");
    ok &= check(!custody::cgroup_process_snapshot(-1), "missing cgroup is unavailable without inventing empty rows");
    std::puts(ok ? "PASS: Linux observed process snapshots" : "FAIL: Linux observed process snapshots");
    return ok ? 0 : 1;
}
