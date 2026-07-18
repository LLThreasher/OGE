add_library(game_sim)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(game_sim
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_link_libraries(game_sim PUBLIC game_data)

target_include_directories(game_sim
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
