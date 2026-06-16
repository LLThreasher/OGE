add_subdirectory(app/jni)

#file(GLOB_RECURSE JNI_SRC CONFIGURE_DEPENDS
#	"${CMAKE_CURRENT_SOURCE_DIR}/app/jni/src/include/*.h"
#	"${CMAKE_CURRENT_SOURCE_DIR}/app/jni/src/src/*.cpp"
#)
#
#if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
#	set(GRADLEW_COMMAND gradlew.bat)
#else()
#	set(GRADLEW_COMMAND gradlew)
#endif()
#
#add_custom_target(
#    app_icon ALL
#    COMMAND "magick" "${CMAKE_SOURCE_DIR}/assets/icon.bmp" "-resize" "192x192\!" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png"
#    COMMAND "magick" "${CMAKE_SOURCE_DIR}/assets/icon.bmp" "-resize" "144x144\!" "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/res/mipmap-xxhdpi/ic_launcher.png"
#    COMMAND "magick" "${CMAKE_SOURCE_DIR}/assets/icon.bmp" "-resize" "96x96\!"   "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/res/mipmap-xhdpi/ic_launcher.png"
#    COMMAND "magick" "${CMAKE_SOURCE_DIR}/assets/icon.bmp" "-resize" "72x72\!"   "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/res/mipmap-hdpi/ic_launcher.png"
#    COMMAND "magick" "${CMAKE_SOURCE_DIR}/assets/icon.bmp" "-resize" "48x48\!"   "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/res/mipmap-mdpi/ic_launcher.png"
#    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app/build/outputs/apk/release/app-release.apk"
#)
#
#add_custom_command(
#	PRE_BUILD
#	OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/app/build/outputs/apk/release/app-release.apk"
#	COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/${GRADLEW_COMMAND}" ARGS "assembleRelease"
#	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#	DEPENDS ${JNI_SRC} app_icon
#)
