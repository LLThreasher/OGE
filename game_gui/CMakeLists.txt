add_executable(game_gui)

target_sources(game_gui
PRIVATE
    main.cpp
 )

target_link_libraries(game_gui PRIVATE game_ctrl_ext oge::platform::sdl3 oge::graphics::vulkan)

compile_shaders(game_gui ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(game_gui ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
set(APP_NAME PortableApp)

add_executable(${APP_NAME} MACOSX_BUNDLE main.cpp)
target_link_libraries(${APP_NAME} PRIVATE game_ctrl_ext oge::platform::sdl3 oge::graphics::vulkan)

compile_shaders(${APP_NAME} ../shaders "${CMAKE_CURRENT_BINARY_DIR}/assets")
copy_assets(${APP_NAME} ../assets "${CMAKE_CURRENT_BINARY_DIR}/assets")

# Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    )

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_BINARY_DIR}/assets
    $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/assets
)

# Note Mac specific extension .app
set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")

# Directories to look for dependencies
set(DIRS ${CMAKE_BINARY_DIR})

# install(CODE "include(BundleUtilities)
#     fixup_bundle(\"${APPS}\" \"\" \"${DIRS}\")")

# set(CPACK_GENERATOR "DRAGNDROP")
# include(CPack)

endif()
