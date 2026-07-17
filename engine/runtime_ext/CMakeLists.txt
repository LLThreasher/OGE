add_library(oge_runtime_ext)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_runtime_ext
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_runtime_ext
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::runtime_ext ALIAS oge_runtime_ext)

target_link_libraries(oge_runtime_ext PUBLIC oge::core oge::platform oge::graphics oge::runtime)
