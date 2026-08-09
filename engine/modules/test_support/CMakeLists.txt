add_library(oge_test_support INTERFACE)

target_include_directories(oge_test_support INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(oge::test_support ALIAS oge_test_support)
