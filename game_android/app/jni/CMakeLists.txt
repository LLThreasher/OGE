cmake_minimum_required(VERSION 3.28)

project(OneGameAndroid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if (CMAKE_BUILD_TYPE STREQUAL "Release")
if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" OR MSVC)
    # Target only optimized configurations
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /OPT:REF /OPT:ICF")
    
    # Inject code generation optimizations
    add_compile_options("$<$<CONFIG:RELEASE>:/Gy;/Gw>")
    add_compile_options("$<$<CONFIG:MINSIZEREL>:/Gy;/Gw>")
else()
    add_compile_options(-ffunction-sections -fdata-sections)
    add_compile_options(-fno-exceptions)

    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    else()
        add_link_options(-Wl,--gc-sections)
    endif()
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if (ipo_supported)
    message(STATUS "IPO/LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
endif()


# SDL sources are in a subfolder named "SDL"
#add_subdirectory(SDL)

# Compilation of companion libraries
#add_subdirectory(SDL_image)
#add_subdirectory(SDL_mixer)
#add_subdirectory(SDL_ttf)

include(cmake/utils.cmake)

# Your game and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(src)

include(cmake/build_config.cmake)
target_include_directories(
    game_data
    PUBLIC
    ${GENERATED_DIR}
)
