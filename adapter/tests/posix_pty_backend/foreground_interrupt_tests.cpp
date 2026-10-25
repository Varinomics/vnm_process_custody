#include "native_receipt_capture.h"
#include "posix_pty_backend/foreground_interrupt_fixture.h"
#include "helpers/test_check.h"

int main(int argc, char** argv)
{
#if defined(__APPLE__)
    const auto receipts = std::make_shared<vnm_terminal::test_helpers::Native_receipt_capture>();
    const auto make_backend = [receipts] {
        return vnm_terminal::test_helpers::make_custody_test_backend(
            vnm_terminal::test_helpers::make_native_receipt_hook(receipts));
    };
    const int result = vnm_terminal::test_helpers::run_foreground_interrupt_tests(argc, argv, make_backend);
    if (argc != 1 || result != 0) {
        return result;
    }
    return vnm_terminal::test_helpers::check(
        receipts->helper_exited(vnm::process_custody::Darwin_pty_owner_exit::CONFIRMED),
        "exact retained helper confirms cleanup after foreground interruption") ? 0 : 1;
#else
    return 2;
#endif
}
