#pragma once

#include <cstring>
#include <string>

namespace vnm::process_custody::detail {

// "<system description>. | <failing call> (<code>)". A host shows its user the
// description before the " | " delimiter and keeps the rest for the developer, so
// the delimiter and this shape are a contract with those hosts.
inline std::string pty_setup_error(const char* operation, int code)
{
    // Operation labels and errno descriptions contain no caller-supplied paths or data.
    return std::string(std::strerror(code)) + ". | " + operation + " (" + std::to_string(code) + ")";
}

} // namespace vnm::process_custody::detail
