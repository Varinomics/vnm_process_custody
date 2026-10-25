cmake_minimum_required(VERSION 3.21)
project(terminal_owner_provider_contract LANGUAGES NONE)

set(CMAKE_SYSTEM_NAME "${TEST_SYSTEM_NAME}")
set(APPLE FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(APPLE TRUE)
endif()
add_library(vnm_process_custody::vnm_process_custody INTERFACE IMPORTED)
foreach(capability IN ITEMS
        VNM_PROCESS_CUSTODY_OWNER_START_DEADLINE
        VNM_PROCESS_CUSTODY_DARWIN_CPU_OBSERVATION
        VNM_PROCESS_CUSTODY_DARWIN_PTY_OUTPUT_RETIREMENT)
    if(NOT capability STREQUAL OMIT_CAPABILITY)
        set_property(TARGET vnm_process_custody::vnm_process_custody PROPERTY ${capability} 1)
    endif()
endforeach()
include("${CONTRACT_MODULE}")
vnm_terminal_require_process_custody()
