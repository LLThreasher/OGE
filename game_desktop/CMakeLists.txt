add_executable(game_desktop)

target_sources(game_desktop
PRIVATE
    main.cpp
 )

target_link_libraries(game_desktop PRIVATE game::gui oge::platform::sdl3 oge::graphics::vulkan)

compile_shaders(game_desktop ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(game_desktop ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/macos/AppIcon.icns)

set_source_files_properties(${APP_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

add_executable(${APP_NAME} MACOSX_BUNDLE main.cpp ${APP_ICON})
target_link_libraries(${APP_NAME} PRIVATE game::gui oge::platform::sdl3 oge::graphics::vulkan)

set_target_properties(${APP_NAME} PROPERTIES
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MARKETING_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${BUILD_NUMBER}"
)

set_target_properties(${APP_NAME} PROPERTIES
    MACOSX_BUNDLE_ICON_FILE AppIcon.icns
)

compile_shaders(${APP_NAME} ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets2")
copy_assets(${APP_NAME} ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets2")

# Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    )

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_BINARY_DIR}/assets2
    $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/assets
)

# Note Mac specific extension .app
set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")

# Directories to look for dependencies
set(DIRS ${CMAKE_BINARY_DIR})

install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"${DIRS}\")")

set(CPACK_GENERATOR "DRAGNDROP")
include(CPack)

endif()
