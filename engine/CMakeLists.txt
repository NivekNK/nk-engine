#================ Default Config ===================
project(engine)

add_library(${PROJECT_NAME} STATIC)

#============== Platform Specific ==================

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    $<$<CONFIG:Debug>:NK_DEV_MODE=1>
    $<$<CONFIG:Debug>:NK_ACTIVE_MEMORY_SYSTEM=1>
    $<$<CONFIG:RelWithDebInfo>:NK_DEV_MODE=2>
    $<$<CONFIG:RelWithDebInfo>:NK_ACTIVE_MEMORY_SYSTEM=1>
    $<$<CONFIG:Release>:NK_DEV_MODE=3>
    $<$<CONFIG:Release>:NK_ACTIVE_MEMORY_SYSTEM=0>
    NK_DEBUG=1
    NK_RELEASE_DEBUG_INFO=2
    NK_RELEASE=3
    GLM_FORCE_SWIZZLE
)

target_precompile_headers(${PROJECT_NAME} PUBLIC include/nkpch.h)

#============== Vendor Subdirectory ================

add_subdirectory(vendor/glm)

find_package(Vulkan REQUIRED)

#================== Source files ===================

set(SOURCES
    src/core/entry_point.cpp
    src/core/os.cpp
    src/core/app.cpp
    src/core/engine.cpp
    src/core/input.cpp
    src/core/clock.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/systems/logging_system.cpp
    src/systems/event_system.cpp
    src/systems/input_system.cpp
    src/platform/platform.cpp
    src/renderer/renderer.cpp
    src/renderer/vulkan/vulkan_renderer.cpp
    src/renderer/vulkan/utils.cpp
    src/renderer/vulkan/instance.cpp
    src/renderer/vulkan/device.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND SOURCES
        src/systems/memory_system.cpp
    )
endif()

if (WIN32)
    list(APPEND SOURCES
        src/platform/platform_win32.cpp
    )
elseif (UNIX AND NOT APPLE)
    message(WARNING "Linux specific sources not yet implemented!")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

#================== Include Files ==================

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm
    Vulkan::Vulkan
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${Vulkan_INCLUDE_DIR}

PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    
    # Libraries
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm>
)
