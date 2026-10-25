#pragma once

#include <vnm_process_custody/terminal_native_receipt.h>
#include <vnm_terminal/terminal_process_owner.h>

#include <memory>

namespace vnm::process_custody {

// Windows terminals already retain native Job ownership in the built-in backend.
// POSIX callers retain their recovery observations through this private owner.
std::unique_ptr<vnm_terminal::Terminal_process_owner> make_terminal_process_owner(
    Terminal_native_receipt_hook receipt_hook = {});

} // namespace vnm::process_custody
