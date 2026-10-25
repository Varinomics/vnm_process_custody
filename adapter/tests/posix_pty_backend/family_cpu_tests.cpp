#include "native_receipt_capture.h"
#include "helpers/test_check.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <chrono>
#include <condition_variable>
#include <mutex>

int main(int argc, char** argv)
{
    if (argc != 2) {
        return 2;
    }
    std::mutex mutex;
    std::condition_variable changed;
    QByteArray output;
    bool exited = false;
    bool error = false;
    vnm_terminal::internal::Terminal_backend_callbacks callbacks;
    callbacks.output_received = [&](QByteArray bytes) {
        const std::lock_guard lock(mutex);
        output += bytes;
        changed.notify_all();
    };
    callbacks.process_exited = [&](auto) {
        const std::lock_guard lock(mutex);
        exited = true;
        changed.notify_all();
    };
    callbacks.error_reported = [&](auto) {
        const std::lock_guard lock(mutex);
        error = true;
        changed.notify_all();
    };
    auto backend = vnm_terminal::test_helpers::make_custody_test_backend();
    vnm_terminal::internal::Terminal_launch_config config;
    config.argv = {QString::fromLocal8Bit(argv[1]), QStringLiteral("--hold-open")};
    config.working_directory = QFileInfo(config.argv.front()).absolutePath();
    config.initial_grid_size = vnm_terminal::internal::terminal_grid_size_t{24, 80};
    if (!vnm_terminal::test_helpers::check(backend->start(config, std::move(callbacks)).code ==
        vnm_terminal::internal::Terminal_backend_result_code::ACCEPTED,
        "enhanced owner CPU fixture starts"))
    {
        return 1;
    }
    {
        std::unique_lock lock(mutex);
        if (!vnm_terminal::test_helpers::check(changed.wait_for(lock, std::chrono::seconds(5), [&] {
            return output.contains("hold-open") || exited || error;
        }) && output.contains("hold-open") && !error, "enhanced owner CPU fixture reaches readiness"))
        {
            return 1;
        }
    }
    const auto cpu = backend->process_cpu_time();
#if defined(__APPLE__)
    const bool ok = vnm_terminal::test_helpers::check(
        cpu && cpu->scope == vnm_terminal::Process_cpu_scope::OBSERVED_FAMILY,
        "live Darwin PTY supplies observed-family CPU accounting");
#else
    const bool ok = vnm_terminal::test_helpers::check(
        !cpu || cpu->scope == vnm_terminal::Process_cpu_scope::WORKLOAD_FAMILY,
        "available Linux cgroup CPU declares native family accounting");
#endif
    backend.reset();
    return ok ? 0 : 1;
}
