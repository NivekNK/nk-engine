#================ Default Config ===================
project(tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}/${PROJECT_NAME}")

enable_testing()

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} "src/main.cpp")

#============== Platform Specific ==================

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    $<$<CONFIG:Debug>:NK_DEV_MODE=1>
    $<$<CONFIG:RelWithDebInfo>:NK_DEV_MODE=2>
    $<$<CONFIG:Release>:NK_DEV_MODE=3>
    NK_DEBUG=1
    NK_RELEASE_DEBUG_INFO=2
    NK_RELEASE=3
    # $<$<CONFIG:Debug>:NK_ACTIVE_MEMORY_SYSTEM=1>
)

if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

#============== Vendor Subdirectory ================

add_subdirectory(vendor/googletest)

#================== Source files ===================
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/tests/src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

#================== Include Files ==================
target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    engine
    GTest::gtest_main
)

set_target_properties(gtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
