#================ Default Config ===================
project(engine)

add_library(${PROJECT_NAME} STATIC)

#============== Platform Specific ==================
target_compile_definitions(${PROJECT_NAME}
PRIVATE
    $<$<CONFIG:Debug>:NK_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NK_RELEASE_DEBUG_INFO>
    $<$<CONFIG:Release>:NK_RELEASE>
)

if (WIN32)
    add_compile_definitions(NK_PLATFORM_WINDOWS)
elseif (UNIX AND NOT APPLE)
    add_compile_definitions(NK_PLATFORM_LINUX)
endif()

target_precompile_headers(${PROJECT_NAME} PUBLIC include/nkpch.h)

#============== Vendor Subdirectory ================

add_subdirectory(vendor/wy)

find_package(Vulkan REQUIRED)

# add_subdirectory(vendor/tlsf)

#================== Source files ===================
set(SOURCES
    src/system/logging_system.cpp
    src/system/input_system.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/nk/entry_point.cpp
    src/nk/app.cpp
    src/nk/window.cpp
    src/nk/clock.cpp
    src/nk/input.cpp
    src/event/event.cpp
    src/renderer/renderer.cpp
    src/renderer/vulkan/utils.cpp
    src/renderer/vulkan/renderer_backend.cpp
    src/renderer/vulkan/instance.cpp
    src/renderer/vulkan/device.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND SOURCES
        src/system/memory_system.cpp
        src/memory/memory_type.cpp
    )
endif()

if (WIN32)
    list(APPEND SOURCES
        src/nk/window_win32.cpp
    )
elseif (UNIX AND NOT APPLE)
    message(WARNING "Linux specific sources not yet implemented!")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

if(NOT CMAKE_THREAD_PREFER_PTHREAD)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
endif()

if(NOT THREADS_PREFER_PTHREAD_FLAG)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
endif()

find_package(Threads REQUIRED)

#================== Include Files ==================
target_link_libraries(${PROJECT_NAME}
PRIVATE
    Threads::Threads
    wy
    Vulkan::Vulkan
    # tlsf
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    # where the library itself will look for its internal headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${Vulkan_INCLUDE_DIR}
PUBLIC
    # where top-level project will look for the library's public headers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # Libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    # ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tlsf/include
)
