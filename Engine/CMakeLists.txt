add_library(Engine STATIC)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

target_sources(Engine
PRIVATE
    "src/JobSystem.cpp"
    "src/Logger.cpp"
    "src/Graphics/RingStagingBuffer.cpp"
    "src/Graphics/UIPass.cpp"
    "src/Graphics/UniformArena.cpp"
    "src/Graphics/GraphicsBackends/GraphicsBackends.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/Vulkan.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanBindingGroups.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanBuffer.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanCommandBuffer.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanFence.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanFrameBuffer.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanPipeline.cpp"
    "src/Graphics/GraphicsBackends/Vulkan/VulkanTexture.cpp"
    "src/Graphics/Renderer.cpp"
    "src/Graphics/TerrainPass.cpp"
    "src/Graphics/MeshBuilder.cpp"
    "src/Terrain/TerrainMeshBuilder.cpp"
    "src/Platform/SDL3GameWindow.cpp"
    "src/Platform/SDL3AssetLoader.cpp"
    "src/Platform/Win32GameWindow.cpp"
    "src/Platform/Win32AssetLoader.cpp"
    "src/GameApp.cpp"
    "src/Graphics/DebugPass.cpp"
    "src/Platform/SDL3AssetLoader.cpp"
    "src/AssetManager.cpp"
    "src/StreamingManager.cpp"
    "src/Input/InputSystem.cpp"
    "src/Scenes/DebugScene.cpp"
    "src/Terrain/TerrainService.cpp"
    "src/ECS/SubsystemCamera.cpp"
    "src/ECS/SubsystemDebugInfo.cpp"
    "src/AssetBundle.cpp"
    "src/Terrain/TerrainUploader.cpp"
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(USE_SDL "Use SDL3 Game Window" True)

if(USE_SDL)
    target_compile_definitions(Engine
    PUBLIC
        USE_SDL3
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Engine
    PRIVATE
        VULKAN_VALIDATION
    )
endif()

target_compile_definitions(Engine
    PRIVATE MAX_FRAMES_IN_FLIGHT=${MAX_FRAMES_IN_FLIGHT}
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_WINDOWS
    PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
    )
    
    target_link_libraries(Engine
        PUBLIC
            glm::glm
            spdlog::spdlog
            EnTT::EnTT
        PRIVATE
            SDL3::SDL3-static
            Vulkan::Vulkan
            freetype
            stb
            vma
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(Engine
    PRIVATE
        "src/Platform/SDL3MetalExt.mm"
    )
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_DARWIN
    PRIVATE
        VK_USE_PLATFORM_METAL_EXT
    )

    target_link_libraries(Engine
        PUBLIC
            glm::glm
            spdlog::spdlog
            EnTT::EnTT
        PRIVATE
            SDL3::SDL3-static
            Vulkan::Vulkan
            freetype
            stb
            vma
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_ANDROID
    PRIVATE
        VK_USE_PLATFORM_ANDROID_KHR
    )
    
    target_link_libraries(Engine
        PUBLIC
            glm::glm
            spdlog::spdlog
            EnTT::EnTT
        PRIVATE
            SDL3::SDL3
            vulkan
            freetype
            stb
            vma
    )
endif()


