cmake_minimum_required(VERSION 3.28)

project(Game CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(data)
add_subdirectory(sim)
add_subdirectory(view)
add_subdirectory(ctrl)
add_subdirectory(ctrl_ext)
