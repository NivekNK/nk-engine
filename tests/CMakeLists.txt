#================ Default Config ===================
project(tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}/${PROJECT_NAME}")

enable_testing()

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

#============== Platform Specific ==================
target_compile_definitions(${PROJECT_NAME}
PRIVATE
    $<$<CONFIG:Debug>:NK_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NK_RELEASE_DEBUG_INFO>
    $<$<CONFIG:Release>:NK_RELEASE>
    NK_TESTING
)

if (WIN32)
    add_compile_definitions(NK_PLATFORM_WINDOWS)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
elseif (UNIX AND NOT APPLE)
    add_compile_definitions(NK_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Platform not supported!")
endif()

target_compile_definitions(engine PRIVATE NK_TESTING)

#============== Vendor Subdirectory ================
add_subdirectory(vendor/googletest)
add_subdirectory(vendor/benchmark)

#================== Source files ===================
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/tests/src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

if(NOT CMAKE_THREAD_PREFER_PTHREAD)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
endif()

if(NOT THREADS_PREFER_PTHREAD_FLAG)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
endif()

find_package(Threads REQUIRED)

#================== Include Files ==================
target_link_libraries(gtest PRIVATE Threads::Threads)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    engine
    GTest::gtest_main
    benchmark
)


set_target_properties(gtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
