set(VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR "" CACHE PATH
    "Public surface sources used by the terminal integration test fixtures")
if(NOT TARGET vnm_terminal_surface::vnm_terminal_surface)
    message(FATAL_ERROR "Terminal owner integration tests require the full public surface source target")
endif()
get_target_property(surface_imported vnm_terminal_surface::vnm_terminal_surface IMPORTED)
if(surface_imported)
    message(FATAL_ERROR "Terminal owner integration tests require the public surface source target")
endif()
get_target_property(surface_target vnm_terminal_surface::vnm_terminal_surface ALIASED_TARGET)
if(NOT surface_target)
    set(surface_target vnm_terminal_surface::vnm_terminal_surface)
endif()
target_compile_definitions(${surface_target} PRIVATE VNM_TERMINAL_CAPTURE_TEST_HOOKS=1)
if(NOT VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR)
    get_target_property(VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR
        ${surface_target} SOURCE_DIR)
endif()
if(NOT EXISTS "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/tests/helpers/test_check.h")
    message(FATAL_ERROR
        "Terminal owner integration tests require VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR")
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
if(WIN32)
    set(vnm_terminal_qt_runtime_path "PATH=$<TARGET_FILE_DIR:Qt6::Core>$<SEMICOLON>$ENV{PATH}")
elseif(APPLE)
    set(vnm_terminal_qt_runtime_path "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:Qt6::Core>:$ENV{DYLD_LIBRARY_PATH}")
else()
    set(vnm_terminal_qt_runtime_path "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:Qt6::Core>:$ENV{LD_LIBRARY_PATH}")
endif()

if(NOT TARGET vnm_terminal_canvas_fixture)
    add_executable(vnm_terminal_canvas_fixture
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/tools/terminal_canvas_fixture/terminal_canvas_fixture.cpp")
    target_include_directories(vnm_terminal_canvas_fixture PRIVATE
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/include")
    set_target_properties(vnm_terminal_canvas_fixture PROPERTIES AUTOMOC OFF)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/native_tests.cmake")

add_test(NAME vnm_process_custody_terminal_owner_contract
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/provider_contract"
        "-DCONTRACT_MODULE=${CMAKE_CURRENT_SOURCE_DIR}/../cmake/vnm_terminal_process_custody_contract.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/provider_contract/check_contract.cmake")
set_tests_properties(vnm_process_custody_terminal_owner_contract PROPERTIES
    LABELS "package;native" TIMEOUT 30)

get_property(adapter_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(test_target IN LISTS adapter_test_targets)
    target_compile_features(${test_target} PRIVATE cxx_std_20)
    target_include_directories(${test_target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/include"
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/src"
        "${VNM_PROCESS_CUSTODY_TERMINAL_SURFACE_SOURCE_DIR}/tests")
    if(test_target STREQUAL "vnm_terminal_error_mode_reporter")
        continue()
    endif()
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /FIvnm_terminal_test_process.h /FS)
    endif()
    if(VNM_PROCESS_CUSTODY_SANITIZE_ADDRESS OR VNM_TERMINAL_SANITIZE_ADDRESS)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /fsanitize=address)
            target_compile_definitions(${test_target} PRIVATE
                _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        else()
            target_compile_options(${test_target} PRIVATE -fsanitize=address)
            target_link_options(${test_target} PRIVATE -fsanitize=address)
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
        vnm_process_custody_deploy_owner(${test_target})
    endif()
endforeach()
