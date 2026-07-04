add_library(EngineCore STATIC)
add_library(EngineGraphics STATIC)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

target_sources(EngineCore
PRIVATE
    "src/JobSystem.cpp"
    "src/Logger.cpp"
    "src/Terrain/TerrainService.cpp"
    "src/Terrain/BlockManager.cpp"
    "src/Terrain/TerrainGenerator.cpp"
    "src/GameApp.cpp"
    "src/AssetManager.cpp"
    "src/Input/InputSystem.cpp"
    "src/ECS/SubsystemPlayer.cpp"
    "src/Platform/PrintStackTrace.cpp"
    "src/Platform/CLIRunner.cpp"
    "src/TickScheduler.cpp"
 )

target_sources(EngineGraphics
PRIVATE
    "src/ECS/SubsystemUI.cpp"
    "src/ECS/SubsystemCamera.cpp"
    "src/ECS/SubsystemPlayerInput.cpp"
    "src/ECS/SubsystemDebugInfo.cpp"
    "src/WindowedGameApp.cpp"
    "src/AssetBundle.cpp"
    "src/StreamingManager.cpp"
    "src/ECS/Renderers.cpp"
    "src/Platform/SDL3GameWindow.cpp"
    "src/Platform/SDL3AssetLoader.cpp"
    "src/Platform/Win32GameWindow.cpp"
    "src/Platform/Win32AssetLoader.cpp"
    "src/Platform/SDL3AssetLoader.cpp"
    "src/Platform/SDL3Stopwatch.cpp"
    "src/Terrain/TerrainUploader.cpp"
    "src/Terrain/TerrainMeshBuilder.cpp"
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
    "src/Graphics/DebugPass.cpp"
    "src/Graphics/Renderer.cpp"
    "src/Graphics/TerrainPass.cpp"
    "src/Graphics/MeshBuilder.cpp"
    "src/Terrain/TerrainRenderer.cpp"
    "src/ECS/CameraRenderer.cpp"
)

target_include_directories(EngineCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Core
)

target_include_directories(EngineGraphics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Graphics
)

option(USE_SDL "Use SDL3 Game Window" True)

if(USE_SDL)
    target_compile_definitions(EngineGraphics
    PUBLIC
        USE_SDL3
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(EngineCore
    PRIVATE
        OGE_DEBUG
    )
    target_compile_definitions(EngineGraphics
    PRIVATE
        VULKAN_VALIDATION
    )
    if (WIN32)
        target_link_libraries(EngineCore PRIVATE dbghelp)
    endif()
endif()

target_compile_definitions(EngineGraphics
    PRIVATE MAX_FRAMES_IN_FLIGHT=${MAX_FRAMES_IN_FLIGHT}
)

target_link_libraries(EngineCore
    PUBLIC
        glm::glm
        spdlog::spdlog
        EnTT::EnTT
    PRIVATE
        enet
        stb
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(EngineCore
    PUBLIC
        PLATFORM_WINDOWS
    )
    target_compile_definitions(EngineGraphics
    PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
    )
    
    target_link_libraries(EngineGraphics
        PUBLIC
            EngineCore
        PRIVATE
            SDL3::SDL3-static
            Vulkan::Vulkan
            freetype
            vma
            stb
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
            enet
            stb
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
            enet
    )
endif()


