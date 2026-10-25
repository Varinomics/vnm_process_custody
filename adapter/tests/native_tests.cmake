if(APPLE)
    add_executable(vnm_terminal_posix_pty_native_receipts
        posix_pty_backend/native_receipt_tests.cpp)
    target_link_libraries(vnm_terminal_posix_pty_native_receipts PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_terminal_posix_pty_native_receipts PROPERTIES AUTOMOC OFF)
    add_test(NAME vnm_terminal_posix_pty_native_receipts
        COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
            "$<TARGET_FILE:vnm_terminal_posix_pty_native_receipts>")
    set_tests_properties(vnm_terminal_posix_pty_native_receipts PROPERTIES
        LABELS "conformance;macos;pty;custody" TIMEOUT 30)

    add_executable(vnm_terminal_posix_pty_shell_jobs_close
        posix_pty_backend/shell_jobs_close_tests.cpp)
    target_link_libraries(vnm_terminal_posix_pty_shell_jobs_close PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_terminal_posix_pty_shell_jobs_close PROPERTIES AUTOMOC OFF)
    add_test(NAME vnm_terminal_posix_pty_shell_jobs_close
        COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
            "$<TARGET_FILE:vnm_terminal_posix_pty_shell_jobs_close>")
    set_tests_properties(vnm_terminal_posix_pty_shell_jobs_close PROPERTIES
        LABELS "conformance;macos;pty;custody" TIMEOUT 30)

    add_executable(vnm_process_custody_terminal_foreground_interrupt
        posix_pty_backend/foreground_interrupt_tests.cpp
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/tests/posix_pty_backend/foreground_interrupt_fixture.cpp")
    target_link_libraries(vnm_process_custody_terminal_foreground_interrupt PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_process_custody_terminal_foreground_interrupt PROPERTIES AUTOMOC OFF)
    add_test(NAME vnm_process_custody_terminal_foreground_interrupt
        COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
            "$<TARGET_FILE:vnm_process_custody_terminal_foreground_interrupt>")
    set_tests_properties(vnm_process_custody_terminal_foreground_interrupt PROPERTIES
        LABELS "conformance;macos;pty;custody" TIMEOUT 20)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
    add_executable(vnm_process_custody_terminal_exit_output
        posix_pty_backend/exit_output_tests.cpp
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/tests/posix_pty_backend/exit_output_fixture.cpp")
    target_link_libraries(vnm_process_custody_terminal_exit_output PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_process_custody_terminal_exit_output PROPERTIES AUTOMOC OFF)
    set(custody_exit_output_cases detached detached_tail)
    if(APPLE)
        list(APPEND custody_exit_output_cases finite small flow external silent reset cleanup_tail)
    endif()
    foreach(scenario IN LISTS custody_exit_output_cases)
        if(scenario MATCHES "^detached")
            set(test_name "vnm_terminal_posix_pty_exit_output_${scenario}")
        else()
            set(test_name "vnm_process_custody_terminal_exit_output_${scenario}")
        endif()
        add_test(NAME ${test_name}
            COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
                "$<TARGET_FILE:vnm_process_custody_terminal_exit_output>" "${scenario}")
        set_tests_properties(${test_name} PROPERTIES
            LABELS "conformance;posix;pty;custody" TIMEOUT 45)
    endforeach()

    add_executable(vnm_process_custody_terminal_family_cpu
        posix_pty_backend/family_cpu_tests.cpp)
    target_link_libraries(vnm_process_custody_terminal_family_cpu PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_process_custody_terminal_family_cpu PROPERTIES AUTOMOC OFF)
    add_dependencies(vnm_process_custody_terminal_family_cpu vnm_terminal_canvas_fixture)
    add_test(NAME vnm_process_custody_terminal_family_cpu
        COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
            "$<TARGET_FILE:vnm_process_custody_terminal_family_cpu>"
            "$<TARGET_FILE:vnm_terminal_canvas_fixture>")
    set_tests_properties(vnm_process_custody_terminal_family_cpu PROPERTIES
        LABELS "conformance;posix;pty;custody;cpu" TIMEOUT 30)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(vnm_process_custody_terminal_foreground_cleanup
        posix_pty_backend/foreground_cleanup_tests.cpp)
    target_link_libraries(vnm_process_custody_terminal_foreground_cleanup PRIVATE
        vnm_process_custody::terminal_owner
        vnm_terminal_surface::vnm_terminal_surface)
    set_target_properties(vnm_process_custody_terminal_foreground_cleanup PROPERTIES AUTOMOC OFF)
    foreach(scenario IN ITEMS adopted host-eof owner-loss)
        string(REPLACE "-" "_" test_suffix "${scenario}")
        add_test(NAME vnm_process_custody_terminal_foreground_${test_suffix}
            COMMAND "${CMAKE_COMMAND}" -E env "${vnm_terminal_qt_runtime_path}"
                "$<TARGET_FILE:vnm_process_custody_terminal_foreground_cleanup>"
                "--foreground-${scenario}")
        set_tests_properties(vnm_process_custody_terminal_foreground_${test_suffix} PROPERTIES
            LABELS "conformance;posix;pty;custody;shutdown" TIMEOUT 30)
    endforeach()
endif()

find_program(vnm_process_custody_test_ninja NAMES ninja)
if(vnm_process_custody_test_ninja)
    add_test(NAME vnm_terminal_process_custody_runtime_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/process_custody_runtime"
            "-DRUNTIME_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vnm_process_custody_runtime.cmake"
            "-DNINJA_EXECUTABLE=${vnm_process_custody_test_ninja}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/process_custody_runtime/check_runtime.cmake")
    set_tests_properties(vnm_terminal_process_custody_runtime_tests PROPERTIES TIMEOUT 120)
endif()
