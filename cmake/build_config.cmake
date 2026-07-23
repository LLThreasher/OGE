find_package(Git REQUIRED)

message(STATUS "Preset: ${CMAKE_PRESET_NAME}")

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if("${GIT_TAG}" STREQUAL "")
    set(MARKETING_VERSION "0.0.0")
else()
    string(REGEX REPLACE "^v" "" MARKETING_VERSION "${GIT_TAG}")
endif()

set(BUILD_NUMBER "0")
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    OUTPUT_VARIABLE BUILD_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND ${GIT_EXECUTABLE} diff --quiet
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_DIFF_RESULT
)

if(GIT_DIFF_RESULT EQUAL 0)
    # Clean working tree
    set(DIRTY_FLAG "")
else()
    # There are uncommitted changes
    set(DIRTY_FLAG "dirty")
endif()

message(STATUS "Marketing Version: ${MARKETING_VERSION}")
message(STATUS "Build Number: ${BUILD_NUMBER}")
message(STATUS "Commit Hash: ${COMMIT_HASH}")
message(STATUS "Dirty: ${DIRTY_FLAG}")

# ----------------------------
# Detect preset
# ----------------------------
if(DEFINED CMAKE_PRESET_NAME)
    set(BUILD_PRESET "${CMAKE_PRESET_NAME}")
elseif(DEFINED CMAKE_BUILD_PRESET_NAME)
    set(BUILD_PRESET "${CMAKE_BUILD_PRESET_NAME}")
else()
    set(BUILD_PRESET "manual-${CMAKE_BUILD_TYPE}")
endif()

# ----------------------------
# Build type (safe fallback)
# ----------------------------
if(CMAKE_BUILD_TYPE)
    set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(BUILD_TYPE "$<CONFIG>")  # Multi-config safe
endif()

# ----------------------------
# Combined build tag
# ----------------------------
if(NOT DIRTY_FLAG)
set(BUILD_TAG
    "${MARKETING_VERSION}.${BUILD_NUMBER}_${COMMIT_HASH}_${BUILD_PRESET}"
)
else()
set(BUILD_TAG
    "${MARKETING_VERSION}.${BUILD_NUMBER}_${COMMIT_HASH}_${DIRTY_FLAG}_${BUILD_PRESET}"
)
endif()

# ----------------------------
# Output directory for generated headers
# ----------------------------
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/build_config.h.in
    ${GENERATED_DIR}/build_config.h
    @ONLY
)

set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/.git/index
)

message(STATUS "Build: ${BUILD_TAG}")
