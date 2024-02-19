project(editor)

add_executable(${PROJECT_NAME})

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
else()
    message(FATAL_ERROR "Platform not supported!")
endif()

target_sources(${PROJECT_NAME}
PRIVATE
    src/editor.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE engine)
