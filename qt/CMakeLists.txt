cmake_minimum_required(VERSION 3.21)
if(TARGET vnm_process_custody::qprocess)
    return()
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(vnm_process_custody_qprocess LANGUAGES CXX)
endif()

include(GNUInstallDirs)
find_package(Qt6 REQUIRED COMPONENTS Core)
add_library(vnm_process_custody_qprocess STATIC qprocess_start.cpp)
add_library(vnm_process_custody::qprocess ALIAS vnm_process_custody_qprocess)
set_target_properties(vnm_process_custody_qprocess PROPERTIES
    EXPORT_NAME qprocess POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_features(vnm_process_custody_qprocess PUBLIC cxx_std_20)
target_link_libraries(vnm_process_custody_qprocess PUBLIC Qt6::Core)
target_include_directories(vnm_process_custody_qprocess
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
if(WIN32)
    target_compile_definitions(vnm_process_custody_qprocess PRIVATE NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

if(WIN32 AND VNM_PROCESS_CUSTODY_BUILD_TESTS)
    enable_testing()
    add_executable(vnm_process_custody_qprocess_tests tests/qprocess_start_win_tests.cpp)
    target_link_libraries(vnm_process_custody_qprocess_tests PRIVATE vnm_process_custody::qprocess)
    add_test(NAME vnm_process_custody_qprocess_tests COMMAND vnm_process_custody_qprocess_tests)
    set_tests_properties(vnm_process_custody_qprocess_tests PROPERTIES TIMEOUT 25)
endif()

install(TARGETS vnm_process_custody_qprocess
    EXPORT vnm_process_custody_qprocess_targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT vnm_process_custody)
install(FILES include/vnm_process_custody/qprocess_start.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/vnm_process_custody" COMPONENT vnm_process_custody)
install(EXPORT vnm_process_custody_qprocess_targets
    NAMESPACE vnm_process_custody::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/vnm_process_custody" COMPONENT vnm_process_custody)
