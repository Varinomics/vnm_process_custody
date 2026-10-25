#include "helpers/test_check.h"
#include "native_receipt_capture.h"
#include "vnm_terminal/internal/posix_pty_backend.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>

namespace custody = vnm::process_custody;

namespace {

using vnm_terminal::test_helpers::check;
using vnm_terminal::test_helpers::make_native_receipt_hook;
using vnm_terminal::test_helpers::Native_receipt_capture;
using Kind = vnm::process_custody::Terminal_native_receipt_kind;
using Ack = vnm::process_custody::Terminal_native_receipt_ack;

vnm_terminal::internal::Terminal_backend_callbacks callbacks()
{
    vnm_terminal::internal::Terminal_backend_callbacks result;
    result.output_received = [](QByteArray) {};
    result.process_exited  = [](vnm_terminal::internal::Terminal_backend_exit) {};
    result.error_reported  = [](vnm_terminal::internal::Terminal_backend_error) {};
    return result;
}

vnm_terminal::internal::Terminal_launch_config shell_config(const QString& command)
{
    vnm_terminal::internal::Terminal_launch_config config;
    config.argv = {QStringLiteral("/bin/sh"), QStringLiteral("-c"), command};
    config.working_directory = QStringLiteral("/tmp");
    config.initial_grid_size = vnm_terminal::internal::terminal_grid_size_t{24, 80};
    return config;
}

bool no_ack_prevents_workload(bool missing_hook)
{
    QTemporaryDir directory;
    if (!check(directory.isValid(), "pre-START admission fixture directory exists")) {
        return false;
    }
    const auto marker = directory.filePath(QStringLiteral("workload-ran"));
    auto config = shell_config(QStringLiteral("printf started > '") + marker + QStringLiteral("'"));
    std::shared_ptr<Native_receipt_capture> receipts;
    custody::Terminal_native_receipt_hook receipt_hook;
    if (!missing_hook) {
        receipts = std::make_shared<Native_receipt_capture>();
        receipt_hook = [receipts](const custody::Terminal_native_receipt& receipt) {
            const auto retained = receipts->retain(receipt);
            return receipt.kind == Kind::HELPER_PRE_START ? Ack::REJECTED : retained;
        };
    }
    auto backend = vnm_terminal::test_helpers::make_custody_test_backend(std::move(receipt_hook));
    const auto result = backend->start(config, callbacks());
    bool ok = check(result.code == vnm_terminal::internal::Terminal_backend_result_code::REJECTED && result.error &&
        result.error->code == vnm_terminal::internal::Terminal_backend_error_code::START_FAILED,
        "missing or rejected pre-START acknowledgement reports native startup failure");
    backend.reset();
    if (receipts) {
        ok &= check(receipts->helper_exited(custody::Darwin_pty_owner_exit::NO_WORKLOAD),
            "withheld START is confirmed by the exact helper's typed no-workload status");
        const auto retained = receipts->receipts();
        ok &= check(retained.size() == 2 && retained[0].kind == Kind::HELPER_PRE_START &&
            retained[1].kind == Kind::NO_BIRTH && retained[1].root_pid == 0,
            "no acknowledgement yields helper then no-birth receipts without a root");
    }
    ok &= check(!QFileInfo::exists(marker), "unacknowledged startup executes no workload command");
    return ok;
}

bool root_receipts(bool failed_exec)
{
    QTemporaryDir directory;
    if (!check(directory.isValid(), "root receipt fixture directory exists")) {
        return false;
    }
    auto config = shell_config(QStringLiteral("exit 23"));
    if (failed_exec) {
        const auto executable_path = directory.filePath(QStringLiteral("unavailable-interpreter"));
        QFile executable(executable_path);
        const auto contents = QByteArrayLiteral("#!") +
            QFile::encodeName(directory.filePath(QStringLiteral("missing-interpreter"))) + QByteArrayLiteral("\n");
        if (!check(executable.open(QIODevice::WriteOnly) && executable.write(contents) == contents.size() &&
            executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner),
            "failed exec fixture reaches native birth with an unavailable interpreter"))
        {
            return false;
        }
        executable.close();
        config.argv = {executable_path};
    }
    const auto receipts = std::make_shared<Native_receipt_capture>();
    auto backend = vnm_terminal::test_helpers::make_custody_test_backend(make_native_receipt_hook(receipts));
    const auto result = backend->start(config, callbacks());
    bool ok = check(result.code == (failed_exec ? vnm_terminal::internal::Terminal_backend_result_code::REJECTED
                                               : vnm_terminal::internal::Terminal_backend_result_code::ACCEPTED),
        "rapid exit starts and native exec failure rejects through the production backend");
    if (failed_exec) {
        ok &= check(result.native_dispatch_occurred && !result.start_outcome_determinate,
            "enhanced owner preserves conservative post-dispatch failure determinacy");
    }
    ok &= check(receipts->wait_for_receipts(3), "exact root exit is retained even after rapid death");
    backend.reset();
    ok &= check(receipts->helper_exited(custody::Darwin_pty_owner_exit::CONFIRMED),
        "admitted root birth settles with typed confirmed status rather than no-workload");
    const auto retained = receipts->receipts();
    ok &= check(retained.size() == 3 && retained[0].kind == Kind::HELPER_PRE_START &&
        retained[1].kind == Kind::ROOT_BORN && retained[2].kind == Kind::ROOT_ENDED &&
        retained[1].root_pid > 1 && retained[1].root_birth_token != 0 &&
        retained[2].root_pid == retained[1].root_pid &&
        retained[2].root_birth_token == retained[1].root_birth_token &&
        retained[2].root_exit_code == (failed_exec ? 127 : 23) && retained[2].root_signal == 0,
        "birth precedes the original root's exact collected exit and never becomes no-birth");
    return ok;
}

bool output_survives_delayed_birth_receipt()
{
    auto config = shell_config(QStringLiteral("printf retained-terminal-output"));
    const auto receipts = std::make_shared<Native_receipt_capture>();
    custody::Terminal_native_receipt_hook receipt_hook =
        [receipts](const custody::Terminal_native_receipt& receipt) {
        const auto retained = receipts->retain(receipt);
        if (receipt.kind == Kind::ROOT_BORN) {
            // Hold startup until the helper exits or its bounded observation
            // wait expires. The backend reader cannot drain output yet.
            (void)receipts->wait_for_helper();
        }
        return retained;
    };

    std::mutex mutex;
    std::condition_variable changed;
    QByteArray output;
    bool exited = false;
    bool output_before_exit = false;
    vnm_terminal::internal::Terminal_backend_exit exit;
    auto events = callbacks();
    events.output_received = [&](QByteArray bytes) {
        const std::lock_guard lock(mutex);
        output += bytes;
    };
    events.process_exited = [&](vnm_terminal::internal::Terminal_backend_exit observed) {
        const std::lock_guard lock(mutex);
        exit = observed;
        output_before_exit = output == QByteArrayLiteral("retained-terminal-output");
        exited = true;
        changed.notify_all();
    };
    auto backend = vnm_terminal::test_helpers::make_custody_test_backend(std::move(receipt_hook));
    const auto result = backend->start(config, std::move(events));
    bool ok = check(result.code == vnm_terminal::internal::Terminal_backend_result_code::ACCEPTED,
        "delayed birth receipt preserves admitted terminal startup");
    {
        std::unique_lock lock(mutex);
        ok &= check(changed.wait_for(lock, std::chrono::seconds(5), [&] { return exited; }),
            "short-lived root delivers its terminal exit");
        ok &= check(output_before_exit && exit.reason == vnm_terminal::internal::Terminal_exit_reason::EXITED && exit.exit_code == 0,
            "all terminal output precedes normal exit despite delayed birth receipt");
    }
    backend.reset();
    ok &= check(receipts->helper_exited(custody::Darwin_pty_owner_exit::CONFIRMED),
        "delayed receipt retains exact confirmed native cleanup");
    return ok;
}

bool rejected_root_receipt_releases_unread_output()
{
    auto config = shell_config(QStringLiteral("printf abandoned-terminal-output"));
    const auto receipts = std::make_shared<Native_receipt_capture>();
    custody::Terminal_native_receipt_hook receipt_hook =
        [receipts](const custody::Terminal_native_receipt& receipt) {
        const auto retained = receipts->retain(receipt);
        if (receipt.kind == Kind::ROOT_BORN) {
            (void)receipts->wait_for_helper();
        }
        return receipt.kind == Kind::ROOT_BORN || receipt.kind == Kind::ROOT_ENDED
            ? Ack::REJECTED : retained;
    };
    auto backend = vnm_terminal::test_helpers::make_custody_test_backend(std::move(receipt_hook));
    const auto result = backend->start(config, callbacks());
    bool ok = check(result.code == vnm_terminal::internal::Terminal_backend_result_code::REJECTED && result.error &&
        result.error->code == vnm_terminal::internal::Terminal_backend_error_code::START_FAILED,
        "rejected root receipts abandon startup before any output reader begins");
    backend.reset();
    ok &= check(receipts->helper_exited(custody::Darwin_pty_owner_exit::CONFIRMED),
        "abandoning unread terminal output still releases the exact confirmed helper");
    return ok;
}

} // namespace

int main()
{
    bool ok = no_ack_prevents_workload(true);
    ok &= no_ack_prevents_workload(false);
    ok &= root_receipts(false);
    ok &= root_receipts(true);
    ok &= output_survives_delayed_birth_receipt();
    ok &= rejected_root_receipt_releases_unread_output();
    return ok ? 0 : 1;
}
