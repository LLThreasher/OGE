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

include(cmake/utils.cmake)

if(NOT BUILD_ANDROID)
add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(game_desktop)

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
  Compiler: ${CMAKE_CXX_COMPILER}
  Add: [-I/usr/local/include]

Index:
  Background: Build
")

    file(WRITE "${CMAKE_SOURCE_DIR}/.clangd" "${CLANGD_FILE_CONTENT}")
endif()
else()

include(FetchContent)

set(ANDROID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game_android")
FetchContent_Declare(
    vulkan_validation
    URL "https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/vulkan-sdk-1.4.350.1/android-binaries-1.4.350.1.tar.gz"
)
FetchContent_MakeAvailable(vulkan_validation)
file(COPY "${vulkan_validation_SOURCE_DIR}/" DESTINATION "${ANDROID_DIR}/app/libs/debug")

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(GRADLEW_COMMAND gradlew.bat)
else()
    set(GRADLEW_COMMAND gradlew)
endif()

set(GRADLEW_ARGS "-DMAX_CMD_BUFFER_PER_QUEUE=${MAX_CMD_BUFFER_PER_QUEUE};-DFORCE_SINGLE_QUEUE=${FORCE_SINGLE_QUEUE};-DMAX_FRAMES_IN_FLIGHT=${MAX_FRAMES_IN_FLIGHT};-DBUILD_WINDOW_SDL3=ON;-DBUILD_VULKAN=ON")
if (ENABLE_PERF)
    set(GRADLEW_ARGS "${GRADLEW_ARGS};-DENABLE_PERF=${ENABLE_PERF}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(APK_NAME "debug/app-debug.apk")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(APK_NAME "release/app-release.apk")
endif()

set(APK_BUILD_PATH "${ANDROID_DIR}/app/build")
set_source_files_properties("${APK_SRC_PATH}" PROPERTIES GENERATED TRUE)

add_custom_target(
    run_gradle_assemble
    COMMAND "${ANDROID_DIR}/${GRADLEW_COMMAND}" "assemble${CMAKE_BUILD_TYPE}" "--console=plain" "--no-daemon" "--rerun-tasks" "--info" "-PCMAKE_ARGS=${GRADLEW_ARGS}"
    WORKING_DIRECTORY "${ANDROID_DIR}"
    BYPRODUCTS "${APK_BUILD_PATH}/outputs/apk/${APK_NAME}"
    USES_TERMINAL
    VERBATIM
)
add_custom_command(
    OUTPUT FORCE_GRADLE_RUN_SYMBOLIC
    COMMAND ${CMAKE_COMMAND} -E echo "Running Gradle..."
)
set_target_properties(run_gradle_assemble PROPERTIES 
    OBJECT_DEPENDS FORCE_GRADLE_RUN_SYMBOLIC
)
add_custom_target(
    build_apk ALL
    COMMAND ${CMAKE_COMMAND} -E copy "${APK_BUILD_PATH}/outputs/apk/${APK_NAME}" "${CMAKE_BINARY_DIR}/Android-${CMAKE_BUILD_TYPE}.apk" 
    DEPENDS run_gradle_assemble "${APK_BUILD_PATH}/outputs/apk/${APK_NAME}"
)
install(CODE "
    message(installing \${CMAKE_BINARY_DIR}/Android-${CMAKE_BUILD_TYPE}.apk)
    execute_process(
        COMMAND \$ENV{ANDROID_HOME}/platform-tools/adb.exe install \${CMAKE_BINARY_DIR}/Android-${CMAKE_BUILD_TYPE}.apk
        WORKING_DIRECTORY \${CMAKE_BINARY_DIR}
        RESULT_VARIABLE res
    )
    if(NOT res EQUAL 0)
        message(FATAL_ERROR \"Custom post-install script failed!\")
    endif()
")

endif()

