cmake_minimum_required(VERSION 3.28)

option(BUILD_VULKAN "Enable Vulkan")
option(BUILD_WINDOW "Build with Window Support")
option(BUILD_WINDOW_SDL3 "Enable SDL3 with Window Support")
option(IO_USE_SDL3 "Use SDL3 for file IO")
option(IO_USE_NATIVE "Use native c++ for file IO")
option(ENABLE_VULKAN_VALIDATION "Enable Vulkan Validation Layer")
option(FORCE_SINGLE_QUEUE "Force to use a single submission queue")

set(MAX_FRAMES_IN_FLIGHT
    1
    CACHE STRING "Frames in flight")

set(MAX_CMD_BUFFER_PER_QUEUE
    4
    CACHE STRING "Command buffers per queue")

if(BUILD_WINDOW_SDL3)
message(STATUS "building window (SDL3)")
message(STATUS "MAX_FRAMES_IN_FLIGHT ${MAX_FRAMES_IN_FLIGHT}")
message(STATUS "MAX_CMD_BUFFER_PER_QUEUE ${MAX_CMD_BUFFER_PER_QUEUE}")
set(BUILD_WINDOW ON)
if(NOT IO_USE_NATIVE)
set(IO_USE_SDL3 ON)
endif()
endif()

if((NOT IO_USE_SDL3) AND (NOT IO_USE_NATIVE))
set(IO_USE_NATIVE ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(ENABLE_VULKAN_VALIDATION ON)
endif()

message(STATUS "BUILD_VULKAN ${BUILD_VULKAN}")
message(STATUS "BUILD_WINDOW ${BUILD_WINDOW}")
message(STATUS "BUILD_WINDOW_SDL3 ${BUILD_WINDOW_SDL3}")
message(STATUS "IO_USE_SDL3 ${IO_USE_SDL3}")
message(STATUS "IO_USE_NATIVE ${IO_USE_NATIVE}")
message(STATUS "ENABLE_VULKAN_VALIDATION ${ENABLE_VULKAN_VALIDATION}")
message(STATUS "FORCE_SINGLE_QUEUE ${FORCE_SINGLE_QUEUE}")

project(OGE CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(BUILD_VULKAN)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(oge_vulkan INTERFACE)
    target_link_libraries(oge_vulkan INTERFACE vulkan)
    add_library(oge::vulkan ALIAS oge_vulkan)
else()
    find_package(Vulkan REQUIRED)
    add_library(oge::vulkan ALIAS Vulkan::Vulkan)
endif()
endif()

add_subdirectory(3rdparty)
add_subdirectory(modules/core)
add_subdirectory(modules/platform)
add_subdirectory(modules/runtime)

if(BUILD_WINDOW)
add_subdirectory(modules/graphics)
add_subdirectory(modules/platform_ext)
add_subdirectory(modules/runtime_ext)
endif()
if(BUILD_WINDOW_SDL3)
add_subdirectory(modules/platform_ext_sdl3)
endif()
if(BUILD_VULKAN)
add_subdirectory(modules/graphics_vulkan)
endif()
