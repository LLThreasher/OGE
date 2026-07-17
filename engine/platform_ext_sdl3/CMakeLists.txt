add_library(oge_platform_ext_sdl3)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_platform_ext_sdl3
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_platform_ext_sdl3
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::platform::sdl3 ALIAS oge_platform_ext_sdl3)

target_link_libraries(oge_platform_ext_sdl3 PUBLIC oge::core oge::platform_ext)

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
target_link_libraries(oge_platform_ext_sdl3 PRIVATE SDL3::SDL3)
else()
target_link_libraries(oge_platform_ext_sdl3 PRIVATE SDL3::SDL3-static)
endif()
