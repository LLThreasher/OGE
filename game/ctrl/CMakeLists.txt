add_library(game_ctrl)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(game_ctrl
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_link_libraries(game_ctrl PUBLIC game_data game_sim stduuid oge::platform)

target_include_directories(game_ctrl
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(game::ctrl ALIAS game_ctrl)

add_library(game_ctrl_native INTERFACE)
target_link_libraries(game_ctrl_native INTERFACE game::ctrl oge::platform::native)
add_library(game::headless ALIAS game_ctrl_native)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

add_test_target(replication_events_test
    SOURCES test/replication_events_test.cpp
    LIBRARIES game::ctrl
)

add_test_target(registry_bug_recreate_test
    SOURCES test/registry_bug_recreate_test.cpp
    LIBRARIES game::ctrl oge::platform::native
)

add_test_target(scene_load_test
    SOURCES test/scene_load_test.cpp
    LIBRARIES game::ctrl oge::platform::native
)
