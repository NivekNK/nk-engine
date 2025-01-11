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
)

target_precompile_headers(${PROJECT_NAME} PUBLIC include/nkpch.h)

#============== Vendor Subdirectory ================

# add_subdirectory(vendor/library)
# find_package(library REQUIRED)

#================== Source files ===================

set(SOURCES
    src/core/os.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND SOURCES
        src/systems/memory_system.cpp
    )
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

#================== Include Files ==================

# target_link_libraries(${PROJECT_NAME}
# PUBLIC
#     library
# )

target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

    # Libraries
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
)
