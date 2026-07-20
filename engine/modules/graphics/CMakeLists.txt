add_library(oge_graphics INTERFACE)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_graphics
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_graphics
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::graphics ALIAS oge_graphics)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(oge_graphics
    INTERFACE
        OGE_DEBUG
    )
endif()

target_compile_definitions(oge_graphics
    INTERFACE
    MAX_FRAMES_IN_FLIGHT=${MAX_FRAMES_IN_FLIGHT}
    FORCE_SINGLE_QUEUE=${FORCE_SINGLE_QUEUE}
)

target_link_libraries(oge_graphics
INTERFACE
    oge::core
    oge::platform_ext
)


