cmake_minimum_required(VERSION 3.24)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(vsg_amd_perf)

find_package(vsg CONFIG REQUIRED)

add_executable(vsg_amd_offscreen_perf vsg_amd_offscreen_perf.cpp)
target_link_libraries(vsg_amd_offscreen_perf PRIVATE vsg::vsg)

add_executable(vsg_amd_window_perf vsg_amd_window_perf.cpp)
target_link_libraries(vsg_amd_window_perf PRIVATE vsg::vsg)
