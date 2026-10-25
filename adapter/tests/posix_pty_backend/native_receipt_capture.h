#pragma once

#include <vnm_process_custody/terminal_process_owner.h>
#include <vnm_terminal/internal/posix_pty_backend.h>

#include <memory>
#include <utility>

#if defined(__APPLE__)
#include <vnm_process_custody/owner.h>
#include <poll.h>
#include <sys/wait.h>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

namespace vnm_terminal::test_helpers {

namespace custody = vnm::process_custody;

// The test consumer keeps the exact native obligations independently of
// presentation callbacks, including backend destruction from a callback.
class Native_receipt_capture
{
public:
    custody::Terminal_native_receipt_ack retain(
        const custody::Terminal_native_receipt& receipt)
    {
        const std::lock_guard lock(m_mutex);
        std::string error;
        if (receipt.kind == custody::Terminal_native_receipt_kind::HELPER_PRE_START) {
            auto observation = std::make_unique<Observation>();
            observation->identity = receipt;
            if (!observation->helper.open(receipt.helper_pid, receipt.helper_birth_token, &error)) {
                return custody::Terminal_native_receipt_ack::REJECTED;
            }
            observation->receipts.push_back(receipt);
            m_observations.push_back(std::move(observation));
            m_changed.notify_all();
            return custody::Terminal_native_receipt_ack::RETAINED;
        }
        for (const auto& observation : m_observations) {
            if (observation->identity.helper_pid != receipt.helper_pid ||
                observation->identity.helper_birth_token != receipt.helper_birth_token)
            {
                continue;
            }
            observation->receipts.push_back(receipt);
            m_changed.notify_all();
            if (receipt.kind == custody::Terminal_native_receipt_kind::ROOT_BORN) {
                observation->identity.root_pid         = receipt.root_pid;
                observation->identity.root_birth_token = receipt.root_birth_token;
                observation->root = custody::open_process_observer(
                    receipt.root_pid, receipt.root_birth_token, &error);
                if (observation->root != custody::k_invalid_handle) {
                    return custody::Terminal_native_receipt_ack::RETAINED;
                }
                // Rapid exit can precede local registration. Only the exact
                // subsequent ROOT_ENDED receipt can discharge that obligation.
                return custody::Terminal_native_receipt_ack::REJECTED;
            }
            if (receipt.kind == custody::Terminal_native_receipt_kind::ROOT_ENDED &&
                receipt.root_pid > 1 && receipt.root_birth_token != 0 &&
                receipt.root_pid == observation->identity.root_pid &&
                receipt.root_birth_token == observation->identity.root_birth_token)
            {
                return custody::Terminal_native_receipt_ack::RETAINED;
            }
            if (receipt.kind == custody::Terminal_native_receipt_kind::NO_BIRTH &&
                observation->identity.root_pid == 0)
            {
                return custody::Terminal_native_receipt_ack::RETAINED;
            }
            return custody::Terminal_native_receipt_ack::REJECTED;
        }
        return custody::Terminal_native_receipt_ack::REJECTED;
    }

    std::vector<custody::Terminal_native_receipt> receipts()
    {
        const std::lock_guard lock(m_mutex);
        return m_observations.empty() ? std::vector<custody::Terminal_native_receipt>{}
                                     : m_observations.back()->receipts;
    }

    bool wait_for_receipts(std::size_t count)
    {
        std::unique_lock lock(m_mutex);
        return m_changed.wait_for(lock, std::chrono::seconds(5), [&] {
            return !m_observations.empty() && m_observations.back()->receipts.size() >= count;
        });
    }

    custody::Native_process_status wait_for_helper()
    {
        custody::Native_process_status_observer* observer;
        {
            const std::lock_guard lock(m_mutex);
            if (m_observations.empty()) {
                return {};
            }
            observer = &m_observations.back()->helper;
        }
        pollfd event{observer->native_handle(), POLLIN, 0};
        int ready;
        do {
            ready = ::poll(&event, 1, 5000);
        }
        while (ready < 0 && errno == EINTR);
        const std::lock_guard lock(m_mutex);
        return observer->observe();
    }

    bool helper_exited(custody::Darwin_pty_owner_exit expected)
    {
        const auto status = wait_for_helper();
        return status.state == custody::Native_process_observation::ENDED &&
            status.status_valid && WIFEXITED(status.wait_status) &&
            WEXITSTATUS(status.wait_status) == static_cast<int>(expected);
    }

private:
    struct Observation
    {
        ~Observation() { custody::close_native_handle(root); }

        custody::Terminal_native_receipt identity;
        custody::Native_process_status_observer helper;
        custody::Native_handle root = custody::k_invalid_handle;
        std::vector<custody::Terminal_native_receipt> receipts;
    };

    std::mutex m_mutex;
    std::condition_variable m_changed;
    std::vector<std::unique_ptr<Observation>> m_observations;
};

inline custody::Terminal_native_receipt_hook make_native_receipt_hook(
    std::shared_ptr<Native_receipt_capture> capture = {})
{
    if (!capture) {
        capture = std::make_shared<Native_receipt_capture>();
    }
    return [capture = std::move(capture)](const custody::Terminal_native_receipt& receipt) {
        return capture->retain(receipt);
    };
}

} // namespace vnm_terminal::test_helpers
#else
namespace vnm_terminal::test_helpers {

namespace custody = vnm::process_custody;

inline custody::Terminal_native_receipt_hook make_native_receipt_hook()
{
    return {};
}

} // namespace vnm_terminal::test_helpers
#endif

namespace vnm_terminal::test_helpers {

inline std::unique_ptr<internal::Terminal_backend> make_custody_test_backend(
    custody::Terminal_native_receipt_hook hook)
{
    return internal::make_posix_pty_backend(custody::make_terminal_process_owner(std::move(hook)));
}

inline std::unique_ptr<internal::Terminal_backend> make_custody_test_backend()
{
    return make_custody_test_backend(make_native_receipt_hook());
}

} // namespace vnm_terminal::test_helpers
