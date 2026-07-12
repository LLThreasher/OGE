cmake_minimum_required(VERSION 3.28)

option(ENABLE_ASAN "Enable AddressSanitizer")
option(ENABLE_PERF "Enable Perf Counters")
option(BUILD_ANDROID "Build Android APK")
option(ENABLE_VULKAN_VALIDATION "Enable Vulkan Validation Layer")

add_compile_definitions(SPDLOG_NO_EXCEPTIONS)

# Mark as cache variable explicitly
set(MAX_FRAMES_IN_FLIGHT
    1
    CACHE STRING "Frames in flight")

if (BUILD_ANDROID)
message(STATUS "building android")
project(OneGame LANGUAGES NONE)
else()
project(OneGame CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Find the Vulkan SDK
# This sets variables like Vulkan_INCLUDE_DIRS and Vulkan_LIBRARIES
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    unset(BUILD_ANDROID)
elseif(NOT BUILD_ANDROID)
    find_package(Vulkan REQUIRED)
endif()

function(fetch_file FILE_NAME URL)
if(NOT EXISTS ${FILE_NAME})
    message(STATUS "Downloading ${FILE_NAME}...")
    file(DOWNLOAD
        ${URL}
        ${FILE_NAME}
        SHOW_PROGRESS
        TLS_VERIFY ON
    )
endif()
endfunction()


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

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
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
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
endif()

if (BUILD_ANDROID)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	    set(GRADLEW_COMMAND gradlew.bat)
    else()
	    set(GRADLEW_COMMAND gradlew)
    endif()

    set(GRADLEW_ARGS "-DMAX_FRAMES_IN_FLIGHT=${MAX_FRAMES_IN_FLIGHT}")
    if (ENABLE_PERF)
        set(GRADLEW_ARGS "${GRADLEW_ARGS};-DENABLE_PERF=${ENABLE_PERF}")
    endif()

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(APK_NAME "debug/app-debug.apk")
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(APK_NAME "release/app-release.apk")
    endif()
    
    set(APK_BUILD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/OneGameAndroid/app/build")
    set_source_files_properties("${APK_SRC_PATH}" PROPERTIES GENERATED TRUE)

    add_custom_target(
	    run_gradle_assemble
	    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/OneGameAndroid/${GRADLEW_COMMAND}" "assemble${CMAKE_BUILD_TYPE}" "--console=plain" "--no-daemon" "--rerun-tasks" "--info" "-Pandroid.cmake.arguments=${GRADLEW_ARGS}"
	    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/OneGameAndroid"
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
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
add_subdirectory(3rdparty)
add_subdirectory(Engine)
add_subdirectory(OneGameLib)
add_subdirectory(OneGameAndroid)
else()
add_subdirectory(3rdparty)
add_subdirectory(Engine)
add_subdirectory(OneGameLib)
add_subdirectory(OneGame)
endif()


