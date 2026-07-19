add_library(oge_platform_ext INTERFACE)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# target_sources(oge_platform_ext
# PUBLIC
#     ${SOURCE_FILES}
# PRIVATE
#     ${SOURCE_FILES_CPP}
#  )

target_include_directories(oge_platform_ext
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(oge_platform_ext
    INTERFACE
        PLATFORM_WINDOWS
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(oge_platform_ext
    INTERFACE
        PLATFORM_DARWIN
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(oge_platform_ext
    INTERFACE
        PLATFORM_ANDROID
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(oge_platform_ext
    INTERFACE
        PLATFORM_LINUX
    )
endif()

add_library(oge::platform_ext ALIAS oge_platform_ext)

target_link_libraries(oge_platform_ext INTERFACE oge::core)
