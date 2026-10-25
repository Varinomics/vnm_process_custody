cmake_minimum_required(VERSION 3.21)
project(vnm_process_custody_terminal_owner_consumer LANGUAGES CXX)

find_package(vnm_process_custody_terminal_owner CONFIG REQUIRED)
add_executable(vnm_process_custody_terminal_owner_consumer main.cpp)
target_link_libraries(vnm_process_custody_terminal_owner_consumer PRIVATE
    vnm_process_custody::terminal_owner)
if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
    vnm_process_custody_deploy_owner(vnm_process_custody_terminal_owner_consumer)
endif()
enable_testing()
add_test(NAME installed_terminal_owner_factory
    COMMAND vnm_process_custody_terminal_owner_consumer)
set_tests_properties(installed_terminal_owner_factory PROPERTIES TIMEOUT 15)
