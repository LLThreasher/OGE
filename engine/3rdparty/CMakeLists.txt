function(fetch_file FILE_NAME URL)
if(NOT EXISTS ${FILE_NAME})
    message(STATUS "Downloading ${FILE_NAME}...")
    file(DOWNLOAD
        ${URL}
        ${FILE_NAME}
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()
endfunction()

if(BUILD_VULKAN)
add_subdirectory(vma)
endif()
add_subdirectory(stb)


include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.17.0
)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.16.0
)

FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    # GIT_TAG        release-3.4.12
    GIT_TAG        9c30a7cc82274ffff188fc8f61de2ba28c3681da
)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-14-3
)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/Lixue9jiu/enet.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(entt)
FetchContent_MakeAvailable(freetype)
FetchContent_MakeAvailable(enet)

set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

if(BUILD_WINDOW_SDL3)
set(SDL_SHARED ON CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)

# set(SDL_RENDER OFF CACHE BOOL "Disable SDL_RENDER support" FORCE)
# set(SDL_GPU OFF CACHE BOOL "Disable SDL_GPU support" FORCE)
# set(SDL_TEST OFF CACHE BOOL "Disable SDL_TEST support" FORCE)
# set(SDL_TESTS OFF CACHE BOOL "Disable SDL_TESTS support" FORCE)

# if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
# set(SDL_AUDIO OFF CACHE BOOL "Disable SDL_AUDIO support" FORCE)
# set(SDL_VULKAN OFF CACHE BOOL "Disable Vulkan support in SDL" FORCE)
# set(SDL_OPENGL OFF CACHE BOOL "Disable OpenGL support in SDL" FORCE)
# set(SDL_OPENGLES OFF CACHE BOOL "Disable OpenGL ES support in SDL" FORCE)
# set(SDL_DIRECTX OFF CACHE BOOL "Disable DIRECTX support in SDL" FORCE)
# set(SDL_POWER OFF CACHE BOOL "Disable SDL_POWER support" FORCE)
# set(SDL_CAMERA OFF CACHE BOOL "Disable SDL_CAMERA support" FORCE)
# set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "Disable SDL_VIRTUAL_JOYSTICK support" FORCE)
# endif()

FetchContent_MakeAvailable(SDL)
endif()
