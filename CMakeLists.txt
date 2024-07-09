cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if (UNIX AND NOT APPLE)
    set(OPERATIVE_SYSTEM "Linux")
elseif(WIN32)
    set(OPERATIVE_SYSTEM "Win32")
else()
    message(FATAL_ERROR "Operative system not supported!")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin/${OPERATIVE_SYSTEM}-${CMAKE_BUILD_TYPE}")

project(nk-engine)

add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(tests)
