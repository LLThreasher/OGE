cmake_minimum_required(VERSION 3.28)

project(OneGameAndroid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL sources are in a subfolder named "SDL"
#add_subdirectory(SDL)

# Compilation of companion libraries
#add_subdirectory(SDL_image)
#add_subdirectory(SDL_mixer)
#add_subdirectory(SDL_ttf)

# Your game and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(engine)
add_subdirectory(src)

