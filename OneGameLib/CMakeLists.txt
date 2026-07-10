add_library(OneGameLib STATIC
	src/DebugClient.cpp
	src/DebugScene.cpp
	  )

target_link_libraries(OneGameLib PUBLIC EngineGraphics)
target_include_directories(OneGameLib
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Android"))
add_executable(OneGameLibHeadless
	src/DebugServer.cpp
	  )

target_include_directories(OneGameLibHeadless
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(OneGameLibHeadless PUBLIC EngineCore)
endif()
