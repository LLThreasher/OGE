cmake_minimum_required(VERSION 3.28)

if (BUILD_ANDROID)
message(STATUS "building android")
project(OneGame LANGUAGES NONE)
else()
project(OneGame CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(game_gui)
