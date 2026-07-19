add_library(oge_platform)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_platform
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::platform ALIAS oge_platform)

target_link_libraries(oge_platform
PUBLIC
    oge::core
    spdlog
PRIVATE
    stb
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(oge_platform
    PUBLIC
        PLATFORM_WINDOWS
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(oge_platform
    PUBLIC
        PLATFORM_DARWIN
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(oge_platform
    PUBLIC
        PLATFORM_ANDROID
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(oge_platform
    PUBLIC
        PLATFORM_LINUX
    )
endif()

if(IO_USE_NATIVE)
    target_compile_definitions(oge_platform
    PUBLIC
        IO_USE_NATIVE
    )
endif()
