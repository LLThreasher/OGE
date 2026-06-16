fetch_file(${CMAKE_CURRENT_SOURCE_DIR}/include/stb_image.h https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image.h)
fetch_file(${CMAKE_CURRENT_SOURCE_DIR}/include/stb_easy_font.h https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_easy_font.h)

add_library(stb STATIC
    stb.cpp
)

target_include_directories(stb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
