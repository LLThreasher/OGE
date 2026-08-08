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

add_library(game::sim ALIAS game_sim)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

add_executable(sim_physics_test
    test/subsystem_physics_test.cpp
)

target_link_libraries(sim_physics_test PRIVATE game_sim)

target_include_directories(sim_physics_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME sim_physics_test COMMAND sim_physics_test)
