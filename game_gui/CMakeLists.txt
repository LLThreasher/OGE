add_executable(game_gui)

target_sources(game_gui
PRIVATE
    main.cpp
 )

target_link_libraries(game_gui PRIVATE game_ctrl_ext oge::platform::sdl3 oge::graphics::vulkan)

compile_shaders(game_gui ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(game_gui ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
