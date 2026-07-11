add_library(EngineCore STATIC)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

target_sources(EngineCore
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    src/TickScheduler.cpp
    src/JobSystem.cpp
    src/Logger.cpp
    src/GameApp.cpp
    src/AssetManager.cpp

    src/Terrain/TerrainService.cpp
    src/Terrain/BlockManager.cpp
    src/Terrain/TerrainGenerator.cpp

    src/Input/InputSystem.cpp

    src/ECS/SubsystemPlayer.cpp
    src/ECS/GameNetSerializer.cpp
    src/ECS/SubsystemCamera.cpp
    src/ECS/SubsystemPhysics.cpp

    src/Platform/PrintStackTrace.cpp
    src/Platform/CLIRunner.cpp

    src/Net/NetServer.cpp
    src/Net/NetClient.cpp
    src/Terrain/TerrainView.cpp
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
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(EngineCore
    PUBLIC
        PLATFORM_LINUX
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(EngineCore
    PUBLIC
        PLATFORM_ANDROID
    )
endif()


