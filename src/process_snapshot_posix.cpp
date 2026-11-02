#include "vnm_process_custody/process_snapshot.h"

#include "process_snapshot_common.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace vnm::process_custody {
namespace {

bool collect_members(int directory, std::set<std::int64_t>* members, bool covered_by_parent = false)
{
    const int descriptor = openat(directory, "cgroup.procs", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return errno == ENOENT || errno == ENODEV;
    }
    std::string text;
    char bytes[4096];
    for (;;) {
        const auto count = read(descriptor, bytes, sizeof(bytes));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            const int read_error = errno;
            close(descriptor);
            // A threaded subtree's processes were read from its domain ancestor.
            return read_error == EOPNOTSUPP && covered_by_parent;
        }
        if (count == 0) {
            break;
        }
        text.append(bytes, (std::size_t)count);
    }
    close(descriptor);
    std::istringstream identifiers(text);
    std::int64_t process_id = 0;
    while (identifiers >> process_id) {
        if (process_id <= 0) {
            return false;
        }
        members->insert(process_id);
    }
    if (!identifiers.eof()) {
        return false;
    }
    const int retained = openat(directory, ".", O_DIRECTORY | O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (retained < 0) {
        return errno == ENOENT || errno == ENODEV;
    }
    DIR* children = fdopendir(retained);
    if (!children) {
        close(retained);
        return false;
    }
    const auto close_directory = [](DIR* value) { closedir(value); };
    std::unique_ptr<DIR, decltype(close_directory)> owned(children, close_directory);
    for (;;) {
        errno = 0;
        const auto* entry = readdir(children);
        if (!entry) {
            return errno == 0;
        }
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
            continue;
        }
        const int child = openat(directory, entry->d_name,
            O_DIRECTORY | O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (child < 0) {
            if (errno == ENOENT || errno == ENOTDIR) {
                continue;
            }
            return false;
        }
        const bool complete = collect_members(child, members, true);
        close(child);
        if (!complete) {
            return false;
        }
    }
}

bool unsigned_number(std::string_view text, std::uint64_t* number)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), *number);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
}

Process_snapshot_entry read_process(std::int64_t process_id, std::int64_t* parent_id = nullptr)
{
    Process_snapshot_entry entry;
    entry.process_id = process_id;
    std::ifstream file("/proc/" + std::to_string(process_id) + "/stat");
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto opening = text.find('(');
    const auto closing = text.rfind(')');
    if (opening == std::string::npos || closing == std::string::npos || closing <= opening) {
        return entry;
    }
    std::istringstream fields(text.substr(closing + 1));
    std::vector<std::string> values{std::istream_iterator<std::string>(fields),
        std::istream_iterator<std::string>()};
    if (values.size() < 22 || !unsigned_number(values[19], &entry.birth_token)) {
        return entry;
    }
    if (parent_id) {
        const auto parsed = std::from_chars(values[1].data(), values[1].data() + values[1].size(), *parent_id);
        if (parsed.ec != std::errc() || parsed.ptr != values[1].data() + values[1].size()) {
            entry.birth_token = 0;
            return entry;
        }
    }
    entry.name = text.substr(opening + 1, closing - opening - 1);
    std::uint64_t user = 0, system = 0, resident_pages = 0;
    const long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second > 0 && unsigned_number(values[11], &user) &&
        unsigned_number(values[12], &system))
    {
        const auto microseconds = ((__uint128_t)user + system) * 1'000'000 / ticks_per_second;
        if (microseconds <= std::numeric_limits<std::uint64_t>::max()) {
            entry.cpu_time_usec = (std::uint64_t)microseconds;
        }
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size > 0 && unsigned_number(values[21], &resident_pages) &&
        resident_pages <= std::numeric_limits<std::uint64_t>::max() / (std::uint64_t)page_size)
    {
        entry.resident_memory_bytes = resident_pages * (std::uint64_t)page_size;
    }
    return entry;
}

} // namespace

std::optional<std::uint64_t> process_birth_token(std::int64_t process_id)
{
    const auto birth = read_process(process_id).birth_token;
    return birth != 0 ? std::optional(birth) : std::nullopt;
}

std::optional<Process_snapshot> observed_process_family_snapshot(
    std::int64_t root_process_id, std::uint64_t root_birth_token, bool include_root)
{
    if (root_process_id <= 0 || root_birth_token == 0) {
        return std::nullopt;
    }
    auto snapshot = internal::make_process_snapshot();
    DIR* directory = opendir("/proc");
    if (!snapshot || !directory) {
        if (directory) {
            closedir(directory);
        }
        return std::nullopt;
    }
    const auto close_directory = [](DIR* value) { closedir(value); };
    std::unique_ptr<DIR, decltype(close_directory)> retained(directory, close_directory);
    std::map<std::int64_t, Process_snapshot_entry> observations;
    std::map<std::int64_t, std::vector<std::int64_t>> children;
    for (;;) {
        errno = 0;
        const auto* name = readdir(directory);
        if (!name) {
            if (errno != 0) {
                return std::nullopt;
            }
            break;
        }
        const std::string_view text(name->d_name);
        std::int64_t process_id = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), process_id);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || process_id <= 0) {
            continue;
        }
        std::int64_t parent_id = 0;
        auto entry = read_process(process_id, &parent_id);
        if (entry.birth_token != 0) {
            children[parent_id].push_back(process_id);
            observations.emplace(process_id, std::move(entry));
        }
    }
    const auto root = observations.find(root_process_id);
    if (root == observations.end() || root->second.birth_token != root_birth_token) {
        return std::nullopt;
    }
    std::vector<std::int64_t> family{root_process_id};
    std::set<std::int64_t> selected{root_process_id};
    for (std::size_t index = 0; index < family.size(); ++index) {
        const auto process_id = family[index];
        const auto& entry = observations.at(process_id);
        if (read_process(process_id).birth_token != entry.birth_token) {
            continue;
        }
        if (include_root || process_id != root_process_id) {
            snapshot->processes.push_back(entry);
        }
        const auto descendants = children.find(process_id);
        if (descendants == children.end()) {
            continue;
        }
        for (const auto child : descendants->second) {
            const auto& child_entry = observations.at(child);
            if (child_entry.birth_token >= entry.birth_token && selected.insert(child).second) {
                family.push_back(child);
            }
        }
    }
    if (read_process(root_process_id).birth_token != root_birth_token) {
        return std::nullopt;
    }
    return snapshot;
}

std::optional<Process_snapshot> cgroup_process_snapshot(int directory)
{
    if (directory < 0) {
        return std::nullopt;
    }
    auto snapshot = internal::make_process_snapshot();
    std::set<std::int64_t> members;
    if (!snapshot || !collect_members(directory, &members)) {
        return std::nullopt;
    }
    for (const auto process_id : members) {
        snapshot->processes.push_back(read_process(process_id));
    }
    std::set<std::int64_t> remaining;
    if (!collect_members(directory, &remaining)) {
        return std::nullopt;
    }
    // Membership can change and PIDs can recycle while /proc is read. Admit
    // metrics only while the same occurrence remains in this workload scope.
    std::erase_if(snapshot->processes, [&remaining](const Process_snapshot_entry& entry) {
        if (!remaining.contains(entry.process_id)) {
            return true;
        }
        return entry.birth_token != 0 && read_process(entry.process_id).birth_token != entry.birth_token;
    });
    return snapshot;
}

} // namespace vnm::process_custody
