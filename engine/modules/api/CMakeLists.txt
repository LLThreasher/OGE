add_library(oge_c_api)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_include_directories(oge_c_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(oge_c_api
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_link_libraries(oge_c_api PRIVATE oge::platform::sdl3)
if(BUILD_VULKAN)
    target_link_libraries(oge_c_api PRIVATE oge::graphics::vulkan)
    target_compile_definitions(oge_c_api PRIVATE OGE_USE_VULKAN)
endif()
if(BUILD_METAL)
    target_link_libraries(oge_c_api PRIVATE oge::graphics::metal)
    target_compile_definitions(oge_c_api PRIVATE OGE_USE_METAL)
endif()
