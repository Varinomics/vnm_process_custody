#include <vnm_process_custody/process_spawn.h>

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace fs = std::filesystem;
namespace custody = vnm::process_custody;

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--child") {
        return std::string_view(argv[0]) == "logical-argv-zero" ? 17 : 1;
    }
    if (argc != 2) {
        return 1;
    }
    // The fixture supplies an ASCII relative name that includes slashes and
    // dot components, as callers can do in an absolute DOS path.
    const auto path = (fs::current_path() / argv[1]).u8string();
    custody::Spawn_request request;
    request.executable.assign(path.begin(), path.end());
    request.argv = {"logical-argv-zero", "--child"};
    const auto child = custody::spawn(request);
    if (!child.ok) {
        std::fprintf(stderr, "Long executable failed to start: %s\n", child.error.c_str());
        return 1;
    }
    const HANDLE process = child.process_handle;
    DWORD status = 0;
    const bool passed = WaitForSingleObject(process, 5000) == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process, &status) && status == 17;
    CloseHandle(process);
    if (!passed) {
        std::fprintf(stderr, "Long executable did not return its child marker.\n");
    }
    return passed ? 0 : 1;
}
