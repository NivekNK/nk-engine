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
    src/core/engine.cpp
    src/core/input.cpp
    src/core/clock.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/memory/linear_allocator.cpp
    src/systems/logging_system.cpp
    src/systems/event_system.cpp
    src/systems/input_system.cpp
    src/platform/platform.cpp
    src/platform/file.cpp
    src/renderer/renderer.cpp
    src/renderer/vulkan/vulkan_renderer.cpp
    src/renderer/vulkan/utils.cpp
    src/renderer/vulkan/instance.cpp
    src/renderer/vulkan/device.cpp
    src/renderer/vulkan/swapchain.cpp
    src/renderer/vulkan/image.cpp
    src/renderer/vulkan/render_pass.cpp
    src/renderer/vulkan/framebuffer.cpp
    src/renderer/vulkan/command_buffer.cpp
    src/renderer/vulkan/fence.cpp
    src/renderer/vulkan/pipeline.cpp
    src/renderer/vulkan/buffer.cpp

    # Shaders
    src/renderer/vulkan/shaders/utils.cpp
    src/renderer/vulkan/shaders/object_shader.cpp
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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/vendor>
    
    # Libraries
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm>
)

#=======================================================================
# COMPILE SHADERS
#=======================================================================

if (WIN32)
    set(SLANGC "$ENV{VULKAN_SDK}/Bin/slangc.exe")
endif()

file(GLOB SLANG_SHADER_FILES
    "assets/shaders/*.slang"
)

foreach(SLANG ${SLANG_SHADER_FILES})
    get_filename_component(FILE_NAME ${SLANG} NAME_WLE)
    get_filename_component(SHADER_EXT ${FILE_NAME} LAST_EXT)
    string(REPLACE "." "" SHADER_STAGE ${SHADER_EXT})
    set(SPIRV "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/"
        COMMAND ${SLANGC} ${SLANG} -entry main -stage ${SHADER_STAGE} -target spirv -o ${SPIRV}
        DEPENDS ${SLANG}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SLANG)

add_custom_target(SHADERS DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(engine SHADERS)
