add_executable(OneGame
	"main2.cpp"
	"TestRenderer.cpp"  )

target_link_libraries(OneGame PRIVATE Engine)

compile_shaders(OneGame ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(OneGame ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
