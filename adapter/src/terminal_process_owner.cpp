#include <vnm_process_custody/terminal_process_owner.h>

#if defined(__linux__) || defined(__APPLE__)
#include "posix_pty_owner.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <unistd.h>
#endif

#include <utility>

namespace vnm::process_custody {

#if defined(__linux__) || defined(__APPLE__)
namespace {

class Terminal_owner final : public vnm_terminal::Terminal_process_owner
{
public:
    explicit Terminal_owner(Terminal_native_receipt_hook receipt_hook)
    :
        m_receipt_hook(std::move(receipt_hook))
    {}

    vnm_terminal::terminal_process_launch_result_t launch(
        const vnm_terminal::terminal_process_launch_t& request) override
    {
        Owner_start_request native;
        native.mode                      = Owner_mode::PTY;
        native.process.executable        = request.executable;
        native.process.environment       = request.environment;
        native.process.working_directory = request.working_directory;
        native.process.argv.reserve(request.argv.size());
        for (const auto& argument : request.argv) {
            native.process.argv.emplace_back(argument);
        }
        for (const auto& descriptor : request.inherited) {
            native.process.inherited.push_back({descriptor.source, descriptor.target});
        }
        native.dimensions.rows         = request.rows;
        native.dimensions.columns      = request.columns;
        native.dimensions.pixel_width  = request.pixel_width;
        native.dimensions.pixel_height = request.pixel_height;

        vnm_terminal::terminal_process_launch_result_t result;
        pid_t root = -1;
        result.accepted = m_owner.launch(native, &result.master_descriptor, &root,
            &result.error, std::move(m_receipt_hook));
        result.native_dispatch_occurred = m_owner.native_dispatch_occurred();
        result.start_outcome_determinate = result.accepted || !result.native_dispatch_occurred;
        if (result.accepted) {
            result.root_process_id = root;
        }
        return result;
    }

    bool stop(
        std::chrono::milliseconds graceful_interval,
        std::chrono::milliseconds kill_interval,
        std::string*              error) noexcept override
    {
        (void)kill_interval;
        const auto grace = std::clamp<std::int64_t>(
            graceful_interval.count(), 0, std::numeric_limits<int>::max());
        return m_owner.stop(static_cast<int>(grace), error);
    }

    std::optional<vnm_terminal::terminal_process_exit_t> wait_root() override
    {
        const auto exit = m_owner.wait_root();
        if (!exit) {
            return std::nullopt;
        }
        return vnm_terminal::terminal_process_exit_t{exit->exit_code, exit->signal};
    }

    void finish() override { m_owner.finish(); }
    bool confirmed() const noexcept override { return m_owner.confirmed(); }

    std::optional<vnm_terminal::process_cpu_time_t> process_cpu_time() const override
    {
        const auto cumulative = m_owner.cpu_time_usec();
        const auto sampled_at = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const long processor_count = ::sysconf(_SC_NPROCESSORS_ONLN);
        if (!cumulative || sampled_at < 0 || processor_count <= 0 ||
            static_cast<unsigned long>(processor_count) > std::numeric_limits<std::uint32_t>::max())
        {
            return std::nullopt;
        }
#if defined(__APPLE__)
        constexpr auto scope = vnm_terminal::Process_cpu_scope::OBSERVED_FAMILY;
#else
        constexpr auto scope = vnm_terminal::Process_cpu_scope::WORKLOAD_FAMILY;
#endif
        return vnm_terminal::process_cpu_time_t{*cumulative,
            static_cast<std::uint64_t>(sampled_at), static_cast<std::uint32_t>(processor_count), scope};
    }

private:
    Terminal_native_receipt_hook m_receipt_hook;
    internal::Posix_pty_owner m_owner;
};

} // namespace
#endif

std::unique_ptr<vnm_terminal::Terminal_process_owner> make_terminal_process_owner(
    Terminal_native_receipt_hook receipt_hook)
{
#if defined(__linux__) || defined(__APPLE__)
    return std::make_unique<Terminal_owner>(std::move(receipt_hook));
#else
    (void)receipt_hook;
    return nullptr;
#endif
}

} // namespace vnm::process_custody
