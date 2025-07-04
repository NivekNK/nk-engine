cmake_minimum_required(VERSION 3.24)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
