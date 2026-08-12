add_library(oge_platform_native)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_include_directories(oge_platform_native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(oge_platform_native
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_compile_definitions(oge_platform_native
PUBLIC
    IO_USE_NATIVE
)

target_link_libraries(oge_platform_native PUBLIC oge::platform)

add_library(oge::platform::native ALIAS oge_platform_native)
