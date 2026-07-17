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

add_subdirectory(spdlog)
add_subdirectory(glm)
add_subdirectory(stb)
add_subdirectory(entt)
add_subdirectory(freetype)
add_subdirectory(enet)

target_compile_definitions(spdlog PUBLIC SPDLOG_USE_STD_FORMAT)

if(BUILD_WINDOW_SDL3)
set(SDL_SHARED ON CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)

set(SDL_AUDIO OFF CACHE BOOL "Disable SDL_AUDIO support" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "Disable Vulkan support in SDL" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "Disable OpenGL support in SDL" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "Disable OpenGL ES support in SDL" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "Disable DIRECTX support in SDL" FORCE)
set(SDL_GPU OFF CACHE BOOL "Disable SDL_GPU support" FORCE)
set(SDL_POWER OFF CACHE BOOL "Disable SDL_POWER support" FORCE)
set(SDL_RENDER OFF CACHE BOOL "Disable SDL_RENDER support" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "Disable SDL_CAMERA support" FORCE)
set(SDL_TEST OFF CACHE BOOL "Disable SDL_TEST support" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL_TESTS support" FORCE)
set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "Disable SDL_VIRTUAL_JOYSTICK support" FORCE)

add_subdirectory(SDL EXCLUDE_FROM_ALL)
endif()
