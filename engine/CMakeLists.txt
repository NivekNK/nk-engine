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

# find_package(Vulkan REQUIRED)
#
# add_subdirectory(vendor/tlsf)
# add_subdirectory(vendor/eventpp)

#================== Source files ===================
set(SOURCES
    src/system/memory_system.cpp
    src/system/logging_system.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
)

# if (WIN32)
#     list(APPEND SOURCES
#         src/platform/platform_win32.cpp
#     )
# elseif (UNIX AND NOT APPLE)
#     message(WARNING "Linux specific sources not yet implemented!")
# endif()

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
    # tlsf
    # eventpp::eventpp
    # Vulkan::Vulkan
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    # where the library itself will look for its internal headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # ${Vulkan_INCLUDE_DIR}
PUBLIC
    # where top-level project will look for the library's public headers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # Libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    # ${CMAKE_CURRENT_SOURCE_DIR}/vendor/internal
    # ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tlsf/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/vendor/eventpp/include
)
