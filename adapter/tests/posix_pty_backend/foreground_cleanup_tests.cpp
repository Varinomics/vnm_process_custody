#include "native_receipt_capture.h"
#include "posix_pty_backend/foreground_cleanup_checks.h"

#include <QFileInfo>
#include <QString>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc == 4 && std::string_view(argv[1]) == "--foreground-cleanup-shell") {
        bool control_valid = false;
        bool report_valid = false;
        const int control = QString::fromLocal8Bit(argv[2]).toInt(&control_valid);
        const int report = QString::fromLocal8Bit(argv[3]).toInt(&report_valid);
        if (!control_valid || !report_valid || control < 3 || report < 3 || control == report) {
            return 2;
        }
        return vnm_terminal::test_helpers::run_foreground_cleanup_shell(control, report);
    }
    if (argc != 2) {
        return 2;
    }
    using Case = vnm_terminal::test_helpers::Foreground_cleanup_case;
    const std::string_view mode(argv[1]);
    if (mode != "--foreground-adopted" && mode != "--foreground-host-eof" &&
        mode != "--foreground-owner-loss")
    {
        return 2;
    }
    const auto scenario = mode == "--foreground-adopted" ? Case::ADOPTED
        : mode == "--foreground-host-eof" ? Case::HOST_EOF : Case::OWNER_LOSS;
    return vnm_terminal::test_helpers::check_foreground_cleanup(
        QFileInfo(QString::fromLocal8Bit(argv[0])).absoluteFilePath(),
        [] { return vnm_terminal::test_helpers::make_custody_test_backend(); }, scenario) ? 0 : 1;
}
