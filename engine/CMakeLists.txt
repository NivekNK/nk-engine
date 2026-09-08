#================ Default Config ===================
project(engine)

add_library(${PROJECT_NAME} STATIC)

#============== Platform Specific ==================

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    GLM_FORCE_SWIZZLE
)

target_precompile_headers(${PROJECT_NAME} PUBLIC include/nkpch.h)

#============== Vendor Subdirectory ================

add_subdirectory(vendor/glm)

# Keep image decoding self-contained and reproducible across platforms. Only
# the static variants are needed because these libraries are implementation
# details of the engine.
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/zlib EXCLUDE_FROM_ALL)

add_library(nk_spng STATIC vendor/libspng/spng/spng.c)
target_compile_definitions(nk_spng PUBLIC SPNG_STATIC)
target_include_directories(nk_spng PUBLIC vendor/libspng/spng)
target_link_libraries(nk_spng PRIVATE ZLIB::ZLIBSTATIC)
set_source_files_properties(
    vendor/libspng/spng/spng.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE
)

if (MSVC)
    target_compile_options(nk_spng PRIVATE /W0)
    target_compile_options(zlibstatic PRIVATE /W0)
else()
    target_compile_options(nk_spng PRIVATE -w)
    target_compile_options(zlibstatic PRIVATE -w)
endif()

find_package(Vulkan REQUIRED)

set(PLATFORM_LIBRARIES)
set(PLATFORM_INCLUDE_DIRS)

#================== Source files ===================

set(SOURCES
    src/core/entry_point.cpp
    src/core/os.cpp
    src/core/engine.cpp
    src/core/input.cpp
    src/core/clock.cpp
    src/core/thread.cpp
    src/core/hash.cpp
    src/core/str.cpp
    src/core/transform.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/memory/linear_allocator.cpp
    src/memory/free_list.cpp
    src/memory/free_list_allocator.cpp
    src/systems/logging_system.cpp
    src/systems/event_system.cpp
    src/systems/input_system.cpp
    src/systems/shader_system.cpp
    src/systems/texture_system.cpp
    src/systems/material_system.cpp
    src/systems/geometry_system.cpp
    src/systems/resource_system.cpp
    src/systems/camera_system.cpp
    src/platform/platform.cpp
    src/platform/file.cpp
    src/resources/image_loader.cpp
    src/resources/loaders.cpp
    src/resources/static_mesh_loader.cpp
    src/resources/static_mesh_binary.cpp
    src/renderer/vulkan/samplers.cpp
    src/resources/tinyobjloader.cpp
    src/resources/mesh.cpp
    src/resources/shader_loader.cpp
    src/renderer/renderer.cpp
    src/renderer/render_target.cpp
    src/renderer/render_view.cpp
    src/renderer/buffer_suballocator.cpp
    src/renderer/shader.cpp
    src/renderer/shader_config.cpp
    src/renderer/vulkan/vulkan_renderer.cpp
    src/renderer/vulkan/utils.cpp
    src/renderer/vulkan/instance.cpp
    src/renderer/vulkan/device.cpp
    src/renderer/vulkan/swapchain.cpp
    src/renderer/vulkan/image.cpp
    src/renderer/vulkan/render_pass.cpp
    src/renderer/vulkan/framebuffer.cpp
    src/renderer/vulkan/command_buffer.cpp
    src/renderer/vulkan/graphics_commands.cpp
    src/renderer/vulkan/fence.cpp
    src/renderer/vulkan/pipeline.cpp
    src/renderer/vulkan/buffer.cpp

    # Shaders
    src/renderer/vulkan/shaders/utils.cpp
    src/renderer/vulkan/shaders/vulkan_shader.cpp

    src/core/camera.cpp

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
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-keysyms)
    pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
    find_path(X11_KEYSYM_INCLUDE_DIR X11/keysym.h REQUIRED)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
    pkg_get_variable(WAYLAND_PROTOCOLS_DATA_DIR wayland-protocols pkgdatadir)

    set(WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
    set(XDG_SHELL_PROTOCOL_XML "${WAYLAND_PROTOCOLS_DATA_DIR}/stable/xdg-shell/xdg-shell.xml")
    set(XDG_SHELL_PROTOCOL_HEADER "${WAYLAND_GENERATED_DIR}/xdg-shell-client-protocol.h")
    set(XDG_SHELL_PROTOCOL_SOURCE "${WAYLAND_GENERATED_DIR}/xdg-shell-protocol.c")

    add_custom_command(
        OUTPUT
            "${XDG_SHELL_PROTOCOL_HEADER}"
            "${XDG_SHELL_PROTOCOL_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WAYLAND_GENERATED_DIR}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
            client-header "${XDG_SHELL_PROTOCOL_XML}" "${XDG_SHELL_PROTOCOL_HEADER}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
            private-code "${XDG_SHELL_PROTOCOL_XML}" "${XDG_SHELL_PROTOCOL_SOURCE}"
        DEPENDS "${XDG_SHELL_PROTOCOL_XML}"
        COMMENT "Generating the Wayland xdg-shell client protocol"
        VERBATIM
    )
    set_source_files_properties(
        "${XDG_SHELL_PROTOCOL_HEADER}"
        "${XDG_SHELL_PROTOCOL_SOURCE}"
        PROPERTIES GENERATED TRUE
    )
    set_source_files_properties(
        "${XDG_SHELL_PROTOCOL_SOURCE}"
        PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE
    )

    list(APPEND SOURCES
        src/core/thread_linux.cpp
        src/platform/platform_linux.cpp
        src/platform/platform_wayland.cpp
        "${XDG_SHELL_PROTOCOL_HEADER}"
        "${XDG_SHELL_PROTOCOL_SOURCE}"
    )
    list(APPEND PLATFORM_LIBRARIES
        Threads::Threads
        PkgConfig::XCB
        PkgConfig::WAYLAND
        PkgConfig::XKBCOMMON
    )
    list(APPEND PLATFORM_INCLUDE_DIRS
        ${X11_KEYSYM_INCLUDE_DIR}
        "${WAYLAND_GENERATED_DIR}"
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_XCB_KHR
        VK_USE_PLATFORM_WAYLAND_KHR
    )
endif()

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

set_source_files_properties(
    src/resources/tinyobjloader.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE
)

if (MSVC)
    set_source_files_properties(
        src/resources/tinyobjloader.cpp
        PROPERTIES COMPILE_OPTIONS "/W0"
    )
else()
    set_source_files_properties(
        src/resources/tinyobjloader.cpp
        PROPERTIES COMPILE_OPTIONS "-w"
    )
endif()

#================== Include Files ==================

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm
    nk_spng
    Vulkan::Vulkan
    ${PLATFORM_LIBRARIES}
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${Vulkan_INCLUDE_DIR}
    ${PLATFORM_INCLUDE_DIRS}

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

if (NOT SLANGC)
    find_program(SLANGC
        NAMES slangc slangc.exe
        HINTS "$ENV{VULKAN_SDK}/Bin"
        DOC "Path to the Slang shader compiler"
    )
endif()

if (NOT SLANGC)
    message(FATAL_ERROR "slangc was not found. Add it to PATH or configure -DSLANGC=/path/to/slangc")
endif()

execute_process(
    COMMAND "${SLANGC}" -version
    RESULT_VARIABLE SLANG_VERSION_RESULT
    OUTPUT_VARIABLE SLANG_VERSION
    ERROR_VARIABLE SLANG_VERSION_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if (NOT SLANG_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Unable to execute slangc at '${SLANGC}': ${SLANG_VERSION_ERROR}")
endif()
if (SLANG_VERSION STREQUAL "")
    set(SLANG_VERSION "${SLANG_VERSION_ERROR}")
endif()
message(STATUS "Slang compiler: ${SLANG_VERSION} (${SLANGC})")

file(GLOB SLANG_SHADER_FILES CONFIGURE_DEPENDS
    "assets/shaders/*.slang"
)

foreach(SLANG ${SLANG_SHADER_FILES})
    get_filename_component(FILE_NAME ${SLANG} NAME_WLE)
    get_filename_component(SHADER_EXT ${FILE_NAME} LAST_EXT)
    string(REPLACE "." "" SHADER_STAGE ${SHADER_EXT})
    if (NOT SHADER_STAGE MATCHES "^(vertex|fragment|geometry)$")
        message(FATAL_ERROR
            "Unsupported Slang stage suffix '${SHADER_STAGE}' in '${SLANG}'. "
            "Use .vertex.slang, .fragment.slang or .geometry.slang.")
    endif()
    set(SPIRV "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/"
        COMMAND "${SLANGC}" "${SLANG}" -entry main -stage ${SHADER_STAGE}
            -target spirv -o "${SPIRV}"
        DEPENDS "${SLANG}"
        COMMENT "Compiling ${FILE_NAME} with Slang"
        VERBATIM
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SLANG)

add_custom_target(SHADERS DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(engine SHADERS)

file(GLOB SHADER_CONFIG_FILES CONFIGURE_DEPENDS
    "assets/shaders/*.shadercfg")

foreach(SHADER_CONFIG ${SHADER_CONFIG_FILES})
    get_filename_component(SHADER_CONFIG_NAME ${SHADER_CONFIG} NAME)
    set(SHADER_CONFIG_OUTPUT
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/${SHADER_CONFIG_NAME}")
    add_custom_command(
        OUTPUT ${SHADER_CONFIG_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_CONFIG} ${SHADER_CONFIG_OUTPUT}
        DEPENDS ${SHADER_CONFIG}
        VERBATIM
    )
    list(APPEND SHADER_CONFIG_BINARY_FILES ${SHADER_CONFIG_OUTPUT})
endforeach(SHADER_CONFIG)

add_custom_target(SHADER_CONFIGS DEPENDS ${SHADER_CONFIG_BINARY_FILES})
add_dependencies(engine SHADER_CONFIGS)

add_custom_target(shader_assets
    DEPENDS ${SPIRV_BINARY_FILES} ${SHADER_CONFIG_BINARY_FILES})
add_custom_target(verify_shader_assets
    COMMAND ${CMAKE_COMMAND}
        -DSHADER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders
        -DSHADER_OUTPUT_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders
        -P ${CMAKE_HOME_DIRECTORY}/.scripts/verify-shader-assets.cmake
    DEPENDS shader_assets
    COMMENT "Verifying compiled Slang shader assets"
    VERBATIM
)

file(GLOB MODEL_FILES
    "assets/models/*.obj"
    "assets/models/*.mtl"
    "assets/models/*.nkmesh"
)

foreach(MODEL ${MODEL_FILES})
    get_filename_component(MODEL_NAME ${MODEL} NAME)
    set(MODEL_OUTPUT
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/${MODEL_NAME}")
    add_custom_command(
        OUTPUT ${MODEL_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MODEL} ${MODEL_OUTPUT}
        DEPENDS ${MODEL}
        VERBATIM
    )
    list(APPEND MODEL_BINARY_FILES ${MODEL_OUTPUT})
endforeach(MODEL)

add_custom_target(MODELS DEPENDS ${MODEL_BINARY_FILES})
add_dependencies(engine MODELS)

file(GLOB TEXTURE_FILES "assets/textures/*.png")

foreach(TEXTURE ${TEXTURE_FILES})
    get_filename_component(TEXTURE_NAME ${TEXTURE} NAME)
    set(TEXTURE_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/textures/${TEXTURE_NAME}")
    add_custom_command(
        OUTPUT ${TEXTURE_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/textures/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEXTURE} ${TEXTURE_OUTPUT}
        DEPENDS ${TEXTURE}
        VERBATIM
    )
    list(APPEND TEXTURE_BINARY_FILES ${TEXTURE_OUTPUT})
endforeach(TEXTURE)

add_custom_target(TEXTURES DEPENDS ${TEXTURE_BINARY_FILES})
add_dependencies(engine TEXTURES)

file(GLOB MATERIAL_FILES "assets/materials/*.kmt")

foreach(MATERIAL ${MATERIAL_FILES})
    get_filename_component(MATERIAL_NAME ${MATERIAL} NAME)
    set(MATERIAL_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/materials/${MATERIAL_NAME}")
    add_custom_command(
        OUTPUT ${MATERIAL_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/materials/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MATERIAL} ${MATERIAL_OUTPUT}
        DEPENDS ${MATERIAL}
        VERBATIM
    )
    list(APPEND MATERIAL_BINARY_FILES ${MATERIAL_OUTPUT})
endforeach(MATERIAL)

add_custom_target(MATERIALS DEPENDS ${MATERIAL_BINARY_FILES})
add_dependencies(engine MATERIALS)
