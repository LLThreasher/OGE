add_executable(game_server)

target_sources(game_server
PRIVATE
    main.cpp
 )

target_link_libraries(game_server PRIVATE game::headless)

compile_shaders(game_server ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(game_server ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
