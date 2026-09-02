project(benchmarks)

add_executable(phase0_baseline EXCLUDE_FROM_ALL
    phase0_baseline.cpp
)

set_target_properties(phase0_baseline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}/benchmarks"
)

target_compile_definitions(phase0_baseline PRIVATE
    NK_DEBUG=1
    NK_RELEASE_DEBUG_INFO=2
    NK_RELEASE=3
    NK_DEV_MODE=1
    NK_ACTIVE_MEMORY_SYSTEM=0
    NK_PHASE0_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

target_link_libraries(phase0_baseline PRIVATE engine)
