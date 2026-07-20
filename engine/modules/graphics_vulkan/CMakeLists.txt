add_library(oge_graphics_vulkan STATIC)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries: ${Vulkan_LIBRARIES}")

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_graphics_vulkan
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_graphics_vulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(oge_graphics_vulkan
    PUBLIC
        OGE_DEBUG
    )
endif()

target_link_libraries(oge_graphics_vulkan
PUBLIC
    oge::graphics
PRIVATE
    vma
    oge::vulkan
    oge::platform
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # target_compile_definitions(oge_graphics_vulkan
    # PRIVATE
    #     VK_USE_PLATFORM_WIN32_KHR
    # )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(oge_graphics_vulkan
    PRIVATE
        VK_USE_PLATFORM_WAYLAND_KHR
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(oge_graphics_vulkan
    PRIVATE
        VK_USE_PLATFORM_METAL_EXT
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(oge_graphics_vulkan
    PRIVATE
        VK_USE_PLATFORM_ANDROID_KHR
    )
endif()

if(ENABLE_VULKAN_VALIDATION)
    target_compile_definitions(oge_graphics_vulkan
    PRIVATE
        VULKAN_VALIDATION
    )
endif()

add_library(oge::graphics::vulkan ALIAS oge_graphics_vulkan)
