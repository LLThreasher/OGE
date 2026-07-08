add_subdirectory(Core)
add_subdirectory(Graphics)

if (ENABLE_PERF)
target_compile_definitions(EngineCore
    PRIVATE ENABLE_PERF=${ENABLE_PERF}
)
endif()

if (ENABLE_PERF)
target_compile_definitions(EngineGraphics
    PRIVATE ENABLE_PERF=${ENABLE_PERF}
)
endif()
