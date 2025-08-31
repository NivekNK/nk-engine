cmake_minimum_required(VERSION 3.24)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create a symlink/copy of compile_commands.json to workspace root for IDE integration
# This will be executed after the project is configured
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_HOME_DIRECTORY}/compile_commands.json"
    COMMENT "Copying compile_commands.json to workspace root for IDE integration"
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
)

# Disable exceptions
add_compile_options(
    -fno-exceptions
    -D_HAS_EXCEPTIONS=0
    -Wall
    -Wextra
    -pedantic-errors
    -Wvla
    -Werror=vla
)

if (UNIX AND NOT APPLE)
    set(OPERATIVE_SYSTEM "Linux")
elseif(WIN32)
    set(OPERATIVE_SYSTEM "Win32")
else()
    message(FATAL_ERROR "Operative system not supported!")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}")

project(nk-engine)

if (WIN32)
    add_compile_definitions(NK_PLATFORM_WINDOWS)
elseif (UNIX AND NOT APPLE)
    add_compile_definitions(NK_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Platform not supported!")
endif()

add_compile_definitions(
    $<$<CONFIG:Debug>:NK_PROJECT_PATH=\"${CMAKE_HOME_DIRECTORY}\">
    $<$<CONFIG:RelWithDebInfo>:NK_PROJECT_PATH=\"${CMAKE_HOME_DIRECTORY}\">
)

add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(tests)
