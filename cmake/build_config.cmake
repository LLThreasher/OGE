find_package(Git REQUIRED)

message(STATUS "Preset: ${CMAKE_PRESET_NAME}")

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Default values
set(MARKETING_VERSION "0.0.0")
set(BUILD_NUMBER "0")
set(COMMIT_HASH "")
set(DIRTY_FLAG "")

# Split by "-"
string(REPLACE "-" ";" GIT_PARTS "${GIT_HASH}")
list(LENGTH GIT_PARTS PART_COUNT)

if(PART_COUNT EQUAL 1)
    # Exactly on a tag: v1.4.2
    string(REGEX REPLACE "^v" "" MARKETING_VERSION "${GIT_HASH}")

elseif(PART_COUNT GREATER_EQUAL 3)
    # v1.4.2-15-gabc1234[-dirty]

    list(GET GIT_PARTS 0 TAG_PART)
    list(GET GIT_PARTS 1 BUILD_NUMBER)
    list(GET GIT_PARTS 2 HASH_PART)

    string(REGEX REPLACE "^v" "" MARKETING_VERSION "${TAG_PART}")
    string(REGEX REPLACE "^g" "" COMMIT_HASH "${HASH_PART}")

    if(PART_COUNT GREATER 3)
        list(GET GIT_PARTS 3 DIRTY_FLAG)
    endif()
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    OUTPUT_VARIABLE BUILD_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

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
