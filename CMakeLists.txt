cmake_minimum_required(VERSION 3.28)

if (BUILD_ANDROID)
message(STATUS "building android")
project(OneGame LANGUAGES NONE)
else()
project(OneGame CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" OR MSVC)
    # Target only optimized configurations
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /OPT:REF /OPT:ICF")
    
    # Inject code generation optimizations
    add_compile_options("$<$<CONFIG:RELEASE>:/Gy;/Gw>")
    add_compile_options("$<$<CONFIG:MINSIZEREL>:/Gy;/Gw>")
else()
    add_compile_options(-ffunction-sections -fdata-sections)

    if(APPLE)
        add_link_options(-Wl,-dead_strip)
    else()
        add_link_options(-Wl,--gc-sections)
    endif()
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if (ipo_supported)
    message(STATUS "IPO/LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
endif()

function(compile_shaders TARGET SHADER_DIR ASSET_TARGET_DIR)

    # Collect all shader files
    file(GLOB_RECURSE SHADER_FILES
        CONFIGURE_DEPENDS
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
        "${SHADER_DIR}/*.comp"
        "${SHADER_DIR}/*.geom"
        "${SHADER_DIR}/*.tesc"
        "${SHADER_DIR}/*.tese"
    )

    set(SPIRV_FILES "")

    foreach(SHADER ${SHADER_FILES})

        get_filename_component(FILE_NAME ${SHADER} NAME)
        set(SPIRV "${ASSET_TARGET_DIR}/${FILE_NAME}.spv")
        set(SPIRV_OPT "${ASSET_TARGET_DIR}/${FILE_NAME}.opt.spv")

        add_custom_command(
            OUTPUT ${SPIRV_OPT}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${ASSET_TARGET_DIR}
            COMMAND glslc -O ${SHADER} -o ${SPIRV}
            COMMAND spirv-opt ${SPIRV} -o ${SPIRV_OPT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${FILE_NAME}"
            VERBATIM
        )

        list(APPEND SPIRV_FILES ${SPIRV_OPT})

    endforeach()

    target_sources(${TARGET} PRIVATE ${SPIRV_FILES})

endfunction()

function(copy_assets TARGET ASSET_DIR ASSET_TARGET_DIR)

add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_DIR}
        ${ASSET_TARGET_DIR})

endfunction()

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(game_gui)

# export compile commands
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    # Compute relative build directory
    file(RELATIVE_PATH CLANGD_BUILD_DIR
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}"
    )

    set(CLANGD_FILE_CONTENT
"CompileFlags:
  CompilationDatabase: ${CLANGD_BUILD_DIR}

Diagnostics:
  UnusedIncludes: Strict

Index:
  Background: Build
")

    file(WRITE "${CMAKE_SOURCE_DIR}/.clangd" "${CLANGD_FILE_CONTENT}")
endif()
