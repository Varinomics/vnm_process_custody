#pragma once

#include <cstring>
#include <string>

namespace vnm::process_custody::detail {

inline std::string pty_setup_error(const char* operation, int code)
{
    // Operation labels and errno descriptions contain no caller-supplied paths or data.
    return std::string("Could not prepare terminal: ") + operation + " (" +
        std::to_string(code) + ": " + std::strerror(code) + ").";
}

} // namespace vnm::process_custody::detail
