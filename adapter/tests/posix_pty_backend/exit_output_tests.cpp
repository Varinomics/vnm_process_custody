#include "native_receipt_capture.h"
#include "posix_pty_backend/exit_output_fixture.h"
#include "helpers/test_check.h"

#if defined(__APPLE__)
#include <libproc.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    vnm_terminal::test_helpers::Exit_output_hooks hooks;
#if defined(__APPLE__)
    const auto receipts = std::make_shared<vnm_terminal::test_helpers::Native_receipt_capture>();
    hooks.make_backend = [receipts] {
        return vnm_terminal::test_helpers::make_custody_test_backend(
            vnm_terminal::test_helpers::make_native_receipt_hook(receipts));
    };
    hooks.observe_detached = [](pid_t pid) {
        proc_bsdinfo info{};
        std::string error;
        if (::proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) {
            return -1;
        }
        return vnm::process_custody::open_process_observer(pid,
            info.pbi_start_tvsec * 1'000'000 + info.pbi_start_tvusec, &error);
    };
    hooks.verify_cleanup = [receipts] {
        const auto helper = receipts->wait_for_helper();
        std::fprintf(stderr, "helper_observed state=%d status_valid=%d wait_status=%d\n",
            static_cast<int>(helper.state), helper.status_valid, helper.wait_status);
        return vnm_terminal::test_helpers::check(
            receipts->helper_exited(vnm::process_custody::Darwin_pty_owner_exit::CONFIRMED),
            "retired helper preserves its confirmed native outcome");
    };
#elif defined(__linux__)
    hooks.make_backend = [] { return vnm_terminal::test_helpers::make_custody_test_backend(); };
    hooks.observe_detached = [](pid_t pid) {
        return static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
    };
#endif
    return vnm_terminal::test_helpers::run_exit_output_tests(argc, argv, hooks);
}
