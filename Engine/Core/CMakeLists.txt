add_library(EngineCore STATIC)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

target_sources(EngineCore
PRIVATE
    src/JobSystem.cpp
    src/Logger.cpp
    src/Terrain/TerrainService.cpp
    src/Terrain/BlockManager.cpp
    src/Terrain/TerrainGenerator.cpp
    src/GameApp.cpp
    src/AssetManager.cpp
    src/Input/InputSystem.cpp
    src/ECS/SubsystemPlayer.cpp
    src/Platform/PrintStackTrace.cpp
    src/Platform/CLIRunner.cpp
    src/TickScheduler.cpp
    src/Net/NetServer.cpp
    src/Net/NetClient.cpp
 )

target_include_directories(EngineCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(EngineCore
    PUBLIC
        OGE_DEBUG
    )
    if (WIN32)
        target_link_libraries(EngineCore PRIVATE dbghelp)
    endif()
endif()

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
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(EngineCore
    PUBLIC
        PLATFORM_DARWIN
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(Engine
    PUBLIC
        PLATFORM_ANDROID
    PRIVATE
        VK_USE_PLATFORM_ANDROID_KHR
    )
endif()


