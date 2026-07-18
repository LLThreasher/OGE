cmake_minimum_required(VERSION 3.28)

option(BUILD_VULKAN "Enable Vulkan")
option(BUILD_WINDOW "Build with Window Support")
option(BUILD_WINDOW_SDL3 "Enable SDL3 with Window Support")
option(IO_USE_SDL3 "Use SDL3 for file IO")
option(IO_USE_NATIVE "Use native c++ for file IO")

set(MAX_FRAMES_IN_FLIGHT
    1
    CACHE STRING "Frames in flight")

if(BUILD_WINDOW_SDL3)
message(STATUS "building window (SDL3)")
message(STATUS "MAX_FRAMES_IN_FLIGHT ${MAX_FRAMES_IN_FLIGHT}")
set(BUILD_WINDOW ON)
set(IO_USE_SDL3 ON)
endif()

message(STATUS "BUILD_VULKAN ${BUILD_VULKAN}")
message(STATUS "BUILD_WINDOW ${BUILD_WINDOW}")
message(STATUS "BUILD_WINDOW_SDL3 ${BUILD_WINDOW_SDL3}")
message(STATUS "IO_USE_SDL3 ${IO_USE_SDL3}")
message(STATUS "IO_USE_NATIVE ${IO_USE_NATIVE}")

if((NOT IO_USE_SDL3) AND (NOT IO_USE_NATIVE))
message( FATAL_ERROR "At least one of IO_USE_SDL3 or IO_USE_NATIVE needs to be specified." )
endif()

project(OGE CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(BUILD_VULKAN)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(oge::vulkan INTERFACE)
    target_link_libraries(oge::vulkan INTERFACE vulkan)
else()
    find_package(Vulkan REQUIRED)
    add_library(oge::vulkan ALIAS Vulkan::Vulkan)
endif()
endif()

add_subdirectory(3rdparty)
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(runtime)

if(BUILD_WINDOW)
add_subdirectory(graphics)
add_subdirectory(platform_ext)
add_subdirectory(runtime_ext)
endif()
if(BUILD_WINDOW_SDL3)
add_subdirectory(platform_ext_sdl3)
endif()
if(BUILD_VULKAN)
add_subdirectory(graphics_vulkan)
endif()
