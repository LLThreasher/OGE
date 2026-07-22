add_library(game_ctrl_ext)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(game_ctrl_ext
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_link_libraries(game_ctrl_ext PUBLIC game_ctrl game_view oge::graphics::vulkan)

target_include_directories(game_ctrl_ext
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(game::ctrl_ext ALIAS game_ctrl_ext)
add_library(game::gui ALIAS game_ctrl_ext)
