execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

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
set(BUILD_TAG
    "${GIT_HASH}_${BUILD_PRESET}"
)

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