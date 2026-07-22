add_library(oge_runtime)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_runtime
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::runtime ALIAS oge_runtime)

target_link_libraries(oge_runtime
PUBLIC oge::core oge::platform
EnTT::EnTT
PRIVATE
        enet)
