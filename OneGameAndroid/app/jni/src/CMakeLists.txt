if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG REQUIRED)
endif()

add_library(main SHARED
    main.cpp
)
target_link_libraries(main PRIVATE SDL3::SDL3 Engine)

#set_target_properties(main PROPERTIES
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/${ANDROID_ABI}"
#)

compile_shaders(main ../../../../shaders "${CMAKE_CURRENT_SOURCE_DIR}/../../src/main/assets")
copy_assets(main ../../../../assets "${CMAKE_CURRENT_SOURCE_DIR}/../../src/main/assets")
