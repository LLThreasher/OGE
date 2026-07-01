add_subdirectory(spdlog)
add_subdirectory(glm)
add_subdirectory(stb)
add_subdirectory(vma)
add_subdirectory(entt)
add_subdirectory(freetype)

target_compile_definitions(spdlog PUBLIC SPDLOG_USE_STD_FORMAT)

set(SDL_SHARED ON CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
