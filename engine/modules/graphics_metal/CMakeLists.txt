add_library(oge_graphics_metal STATIC)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_graphics_metal
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_graphics_metal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(oge_graphics_metal
    PUBLIC
        OGE_DEBUG
    )
endif()

# Metal debug layer (set MTL_DEBUG_LAYER=1 / MTL_SHADER_VALIDATION=1
# in the environment at runtime).  This option adds a compile-time hint.
option(METAL_DEBUG "Enable Metal debug layer hints" OFF)
if(METAL_DEBUG)
    target_compile_definitions(oge_graphics_metal PUBLIC OGE_METAL_DEBUG)
endif()

target_link_libraries(oge_graphics_metal
PUBLIC
    oge::graphics
    metal-cpp
PRIVATE
    oge::platform
)

# SPIR-V → MSL cross-compilation (from Vulkan SDK).
find_library(SPIRV_CROSS_MSL spirv-cross-msl HINTS /usr/local/lib)
find_library(SPIRV_CROSS_GLSL spirv-cross-glsl HINTS /usr/local/lib)
find_library(SPIRV_CROSS_CORE spirv-cross-core HINTS /usr/local/lib)
find_path(SPIRV_CROSS_INC spirv_cross/spirv_msl.hpp HINTS /usr/local/include)
if(SPIRV_CROSS_MSL AND SPIRV_CROSS_CORE AND SPIRV_CROSS_INC)
    target_include_directories(oge_graphics_metal PRIVATE ${SPIRV_CROSS_INC})
    target_link_libraries(oge_graphics_metal PRIVATE
        ${SPIRV_CROSS_MSL} ${SPIRV_CROSS_GLSL} ${SPIRV_CROSS_CORE})
    target_compile_definitions(oge_graphics_metal PRIVATE OGE_HAS_SPIRV_CROSS)
    message(STATUS "Metal: SPIRV-Cross enabled")
endif()

add_library(oge::graphics::metal ALIAS oge_graphics_metal)
