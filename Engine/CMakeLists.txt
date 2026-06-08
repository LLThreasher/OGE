add_library(Engine STATIC)

file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE ENGINE_HEADERS
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

target_sources(Engine PRIVATE
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(Engine PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
    PLATFORM_WINDOWS
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_WINDOWS
    PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_ANDROID
    PRIVATE
        VK_USE_PLATFORM_ANDROID_KHR
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Engine
    PRIVATE
        VULKAN_VALIDATION
    )
endif()

if (MSVC)
    target_compile_options(Engine PRIVATE
        $<$<CONFIG:Debug>:
            /permissive-
            /sdl
            /analyze
            /we4700
            /RTC1
        >
    )
endif()

target_link_libraries(Engine
    PUBLIC
        glm::glm
        spdlog::spdlog
    PRIVATE
        Vulkan::Vulkan
        stb
        vma
)
