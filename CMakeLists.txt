cmake_minimum_required(VERSION 3.28)

if (BUILD_ANDROID)
message(STATUS "building android")
project(OneGame LANGUAGES NONE)
else()
project(OneGame CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
