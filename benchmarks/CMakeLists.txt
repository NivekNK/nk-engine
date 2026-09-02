project(benchmarks)

add_executable(phase0_baseline EXCLUDE_FROM_ALL
    phase0_baseline.cpp
)

set_target_properties(phase0_baseline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}/benchmarks"
)

target_compile_definitions(phase0_baseline PRIVATE
    NK_PHASE0_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

target_link_libraries(phase0_baseline PRIVATE engine)

add_executable(collections_benchmark EXCLUDE_FROM_ALL
    collections_benchmark.cpp
)

set_target_properties(collections_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}/benchmarks"
)

target_compile_definitions(collections_benchmark PRIVATE
    NK_BENCHMARK_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

target_link_libraries(collections_benchmark PRIVATE engine)
