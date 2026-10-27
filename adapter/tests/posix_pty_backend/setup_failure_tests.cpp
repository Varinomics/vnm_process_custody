#include "helpers/test_check.h"
#include "native_receipt_capture.h"
#include "vnm_terminal/internal/posix_pty_backend.h"

#include <QCoreApplication>
#include <QString>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    using vnm_terminal::test_helpers::check;
    namespace backend = vnm_terminal::internal;
    if (!check(argc == 2, "interposition library path is supplied")) {
        return 1;
    }
    bool ok = true;
    std::set<std::string> messages;
    for (const char* operation : {"openpty", "fcntl(master)", "fcntl(slave)", "ioctl(TIOCSWINSZ)",
             "getsid", "setsid", "sigaction(SIGHUP)", "ioctl(TIOCSCTTY)", "sigaction(SIGTTOU)", "fstat"})
    {
        backend::Terminal_launch_config config;
        config.argv = {QStringLiteral("/bin/sh"), QStringLiteral("-c"), QStringLiteral("exit 0")};
        config.working_directory = QStringLiteral("/tmp");
        config.initial_grid_size = backend::terminal_grid_size_t{24, 80};
        config.environment_edits = {
            {backend::Terminal_environment_operation::SET, QStringLiteral("DYLD_INSERT_LIBRARIES"),
                QString::fromLocal8Bit(argv[1])},
            {backend::Terminal_environment_operation::SET, QStringLiteral("VNM_TEST_PTY_FAILURE"),
                QString::fromLatin1(operation)},
        };
        backend::Terminal_backend_callbacks callbacks;
        callbacks.output_received = [](QByteArray) {};
        callbacks.process_exited  = [](backend::Terminal_backend_exit) {};
        callbacks.error_reported  = [](backend::Terminal_backend_error) {};
        auto terminal = vnm_terminal::test_helpers::make_custody_test_backend();
        const auto result = terminal->start(config, callbacks);
        ok &= check(result.code == backend::Terminal_backend_result_code::REJECTED && result.error &&
            result.error->code == backend::Terminal_backend_error_code::START_FAILED,
            "PTY syscall failure reaches the terminal startup error");
        if (result.error) {
            const auto text = result.error->message.toStdString();
            std::fprintf(stderr, "%s -> %s\n", operation, text.c_str());
            ok &= check(text.find(operation) != std::string::npos,
                "terminal error preserves the failing operation");
            ok &= check(text.find(std::to_string(EACCES)) != std::string::npos &&
                text.find(std::strerror(EACCES)) != std::string::npos,
                "terminal error preserves the original native code and cause");
            ok &= check(messages.insert(text).second, "different failing operations remain distinguishable");
        }
    }
    return ok ? 0 : 1;
}
