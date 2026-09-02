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
    src/core/hash.cpp
    src/core/str.cpp
    src/memory/allocator.cpp
    src/memory/malloc_allocator.cpp
    src/memory/linear_allocator.cpp
    src/systems/logging_system.cpp
    src/systems/event_system.cpp
    src/systems/input_system.cpp
    src/platform/platform.cpp
    src/platform/file.cpp
    src/resources/image_loader.cpp
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

    # Temporal
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
        src/platform/platform_linux.cpp
        src/platform/platform_wayland.cpp
        "${XDG_SHELL_PROTOCOL_HEADER}"
        "${XDG_SHELL_PROTOCOL_SOURCE}"
    )
    list(APPEND PLATFORM_LIBRARIES
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

file(GLOB MODEL_FILES "assets/models/*.obj")

# The name of the models are simply for example "flat_vase.obj" i just need to copy them to the build directory, copy it with the extension file
foreach(MODEL ${MODEL_FILES})
    get_filename_component(MODEL_NAME ${MODEL} NAME_WLE)
    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/${MODEL_NAME}.obj
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MODEL} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/${MODEL_NAME}.obj
        DEPENDS ${MODEL}
    )
    list(APPEND MODEL_BINARY_FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/models/${MODEL_NAME}.obj)
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
