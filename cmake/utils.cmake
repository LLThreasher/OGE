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

# ---------------------------------------------------------------------------
# add_test_target(name SOURCES file... [LIBRARIES lib...])
#
# Creates a test executable and registers it with CTest.  Links
# oge::test_support automatically.
#
# Usage:
#   add_test_target(my_test SOURCES test/my_test.cpp LIBRARIES game::ctrl)
# ---------------------------------------------------------------------------
function(add_test_target NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})

    # 1. Create the main executable target
    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE oge::test_support ${ARG_LIBRARIES})
    target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    # 2. Run the Python script to extract test names from the source files.
    #    Build absolute paths so the script can find files regardless of
    #    the working directory.
    set(_abs_sources "")
    foreach(_src ${ARG_SOURCES})
        list(APPEND _abs_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    execute_process(
        COMMAND python3 "${CMAKE_SOURCE_DIR}/cmake/extract_tests.py" ${_abs_sources}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE EXTRACTED_TESTS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _extract_ret
    )

    # 3. Register each extracted test case as an individual CTest entry.
    if(_extract_ret EQUAL 0 AND EXTRACTED_TESTS)
        foreach(TEST_NAME IN LISTS EXTRACTED_TESTS)
            add_test(NAME "${NAME}::${TEST_NAME}" COMMAND ${NAME} --run-test=${TEST_NAME})
        endforeach()
    else()
        # Fallback: register the executable itself as a single CTest test
        # so the build doesn't break.
        add_test(NAME ${NAME} COMMAND ${NAME})
    endif()
endfunction()
