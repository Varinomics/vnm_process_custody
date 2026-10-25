#include <vnm_process_custody/terminal_process_owner.h>

#include <cstdio>

int main()
{
    const auto owner = vnm::process_custody::make_terminal_process_owner();
#if defined(_WIN32)
    if (owner) {
        std::fprintf(stderr, "FAIL: enhanced POSIX ownership must be unavailable on Windows\n");
        return 1;
    }
#else
    if (!owner || owner->process_cpu_time()) {
        std::fprintf(stderr, "FAIL: a new process owner must exist without a process CPU sample\n");
        return 1;
    }
#endif
    return 0;
}
