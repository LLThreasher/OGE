add_executable(OneGame
	"main.cpp"
	"TestRenderer.cpp")

target_link_libraries(OneGame PRIVATE Engine)
copy_assets(OneGame assets)
compile_shaders(OneGame shaders)
