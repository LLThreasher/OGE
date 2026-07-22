cmake_minimum_required(VERSION 3.28)

project(Game CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(data)
add_subdirectory(ctrl)
add_subdirectory(sim)

if(TARGET oge::graphics AND TARGET oge::platform_ext)
add_subdirectory(view)
add_subdirectory(ctrl_ext)
endif()
