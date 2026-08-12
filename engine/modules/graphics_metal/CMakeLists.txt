add_library(oge_graphics_metal STATIC)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_graphics_metal
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_graphics_metal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(oge_graphics_metal
    PUBLIC
        OGE_DEBUG
    )
endif()

target_link_libraries(oge_graphics_metal
PUBLIC
    oge::graphics
    metal-cpp
PRIVATE
    oge::platform
)

add_library(oge::graphics::metal ALIAS oge_graphics_metal)
