cmake_minimum_required(VERSION 3.21)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT TARGET vnm_process_custody::vnm_process_custody)
    find_package(vnm_process_custody CONFIG REQUIRED)
endif()
if(NOT TARGET vnm_terminal_surface::process_owner_contract)
    find_package(vnm_terminal_surface CONFIG REQUIRED)
endif()
find_package(Threads REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/vnm_terminal_process_custody_contract.cmake")
vnm_terminal_require_process_custody()

add_library(vnm_process_custody_terminal_owner STATIC
    src/terminal_process_owner.cpp
    src/posix_pty_owner.h)
add_library(vnm_process_custody::terminal_owner ALIAS vnm_process_custody_terminal_owner)
set_target_properties(vnm_process_custody_terminal_owner PROPERTIES
    EXPORT_NAME terminal_owner
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
    POSITION_INDEPENDENT_CODE ON)
target_compile_features(vnm_process_custody_terminal_owner PUBLIC cxx_std_20)
target_include_directories(vnm_process_custody_terminal_owner PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(vnm_process_custody_terminal_owner PUBLIC
    vnm_process_custody::vnm_process_custody
    vnm_terminal_surface::process_owner_contract
    PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
    target_link_libraries(vnm_process_custody_terminal_owner PRIVATE Qt6::Core)
endif()

if(VNM_PROCESS_CUSTODY_SANITIZE_ADDRESS OR VNM_TERMINAL_SANITIZE_ADDRESS)
    if(MSVC)
        target_compile_options(vnm_process_custody_terminal_owner PRIVATE /fsanitize=address /FS)
        target_compile_definitions(vnm_process_custody_terminal_owner PRIVATE
            _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    else()
        target_compile_options(vnm_process_custody_terminal_owner PRIVATE -fsanitize=address)
        target_link_options(vnm_process_custody_terminal_owner PUBLIC -fsanitize=address)
    endif()
endif()

set(adapter_package_directory "${CMAKE_INSTALL_LIBDIR}/cmake/vnm_process_custody_terminal_owner")
configure_package_config_file(
    cmake/vnm_process_custody_terminal_owner-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/vnm_process_custody_terminal_owner-config.cmake"
    INSTALL_DESTINATION "${adapter_package_directory}")
install(TARGETS vnm_process_custody_terminal_owner
    EXPORT vnm_process_custody_terminal_owner_targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES include/vnm_process_custody/terminal_process_owner.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/vnm_process_custody")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/../NOTICE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/vnm_process_custody_terminal_owner")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/vnm_process_custody_terminal_owner-config.cmake"
    cmake/vnm_terminal_process_custody_contract.cmake
    DESTINATION "${adapter_package_directory}")
install(EXPORT vnm_process_custody_terminal_owner_targets
    NAMESPACE vnm_process_custody::
    DESTINATION "${adapter_package_directory}")

option(VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER_TESTS
    "Build explicit enhanced terminal ownership integration tests" OFF)
if(VNM_PROCESS_CUSTODY_BUILD_TERMINAL_OWNER_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
