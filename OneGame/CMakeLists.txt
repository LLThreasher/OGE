add_executable(OneGame
	"main.cpp"
)

target_link_libraries(OneGame PRIVATE OneGameLib)
		
compile_shaders(OneGame ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(OneGame ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")

add_executable(OneGameHeadless
	"main_headless.cpp"
	  )

target_link_libraries(OneGameHeadless PRIVATE OneGameLibHeadless)

compile_shaders(OneGameHeadless ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(OneGameHeadless ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
