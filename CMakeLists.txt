cmake_minimum_required(VERSION 3.25)
project(yapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

add_library(yapper INTERFACE)
add_compile_options("-Wno-interference-size")

target_include_directories(yapper INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

install(DIRECTORY include/yapper DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS yapper EXPORT yapperTargets)
install(EXPORT yapperTargets
    FILE yapperTargets.cmake
    NAMESPACE yapper::
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/yapper)

add_subdirectory(examples)

# testing
option(BUILD_TESTS "Build the test suite" OFF)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

