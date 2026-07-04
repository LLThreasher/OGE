add_executable(OneGame
	"DebugScene.cpp"
	"main.cpp"
	  )

target_link_libraries(OneGame PRIVATE EngineGraphics)

add_executable(OneGameHeadless
	"main_headless.cpp"
	  )

target_link_libraries(OneGameHeadless PRIVATE EngineCore)

compile_shaders(OneGame ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(OneGame ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
