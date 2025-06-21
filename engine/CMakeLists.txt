#================ Default Config ===================
project(engine)

#===================== GLFW ========================
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
#===================================================

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
add_subdirectory(vendor/glfw)

find_package(Vulkan REQUIRED)

#================== Source files ===================

set(SOURCES
    src/core/entry_point.cpp
    src/core/os.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/systems/logging_system.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND SOURCES
        src/systems/memory_system.cpp
    )
endif()

# if (WIN32)
#     list(APPEND SOURCES
#         src/windows_specific.cpp
#     )
# elseif (UNIX AND NOT APPLE)
#     message(WARNING "Linux specific sources not yet implemented!")
# endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

#================== Include Files ==================

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm
    Vulkan::Vulkan
    glfw
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIR}

PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    
    # Libraries
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include>
)
