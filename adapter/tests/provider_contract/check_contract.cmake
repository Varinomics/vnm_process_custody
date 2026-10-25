cmake_minimum_required(VERSION 3.21)

foreach(system_name IN ITEMS Linux Darwin Windows)
    foreach(omission IN ITEMS none
            VNM_PROCESS_CUSTODY_OWNER_START_DEADLINE
            VNM_PROCESS_CUSTODY_DARWIN_CPU_OBSERVATION
            VNM_PROCESS_CUSTODY_DARWIN_PTY_OUTPUT_RETIREMENT)
        set(expected_diagnostic "")
        if(NOT system_name STREQUAL "Windows")
            if(omission STREQUAL "VNM_PROCESS_CUSTODY_OWNER_START_DEADLINE")
                set(expected_diagnostic "lacks deadline-aware owner START")
            elseif(system_name STREQUAL "Darwin")
                if(omission STREQUAL "VNM_PROCESS_CUSTODY_DARWIN_CPU_OBSERVATION")
                    set(expected_diagnostic "lacks Darwin observed-family CPU accounting")
                elseif(omission STREQUAL "VNM_PROCESS_CUSTODY_DARWIN_PTY_OUTPUT_RETIREMENT")
                    set(expected_diagnostic "lacks Darwin PTY output retirement")
                endif()
            endif()
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}"
                -B "${TEST_ROOT}/${system_name}/${omission}"
                "-DTEST_SYSTEM_NAME=${system_name}" "-DOMIT_CAPABILITY=${omission}"
                "-DCONTRACT_MODULE=${CONTRACT_MODULE}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        string(REGEX REPLACE "[ \t\r\n]+" " " diagnostic "${output}${error}")
        if(expected_diagnostic)
            if(result EQUAL 0 OR NOT diagnostic MATCHES "${expected_diagnostic}")
                message(FATAL_ERROR
                    "${system_name} accepted a provider missing ${omission}: ${diagnostic}")
            endif()
        elseif(NOT result EQUAL 0)
            message(FATAL_ERROR "${system_name} rejected its provider contract: ${diagnostic}")
        endif()
    endforeach()
endforeach()
