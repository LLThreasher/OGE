add_library(oge_core STATIC)

file(GLOB_RECURSE SOURCE_FILES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

file(GLOB_RECURSE SOURCE_FILES_CPP 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(oge_core
PUBLIC
    ${SOURCE_FILES}
PRIVATE
    ${SOURCE_FILES_CPP}
 )

target_include_directories(oge_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
target_compile_definitions(oge_core
PUBLIC
    OGE_DEBUG
)
endif()

target_link_libraries(oge_core PUBLIC glm::glm fmt::fmt)

# nlohmann/json backs the JSON parser/emitter in src/json.cpp.  PRIVATE:
# the public header oge/core/json.hpp stays self-contained (variant-based),
# only the .cpp includes nlohmann headers.
target_link_libraries(oge_core PRIVATE nlohmann_json::nlohmann_json)

add_library(oge::core ALIAS oge_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

add_test_target(datastruct_test
    SOURCES test/datastruct_test.cpp
    LIBRARIES oge::core
)
