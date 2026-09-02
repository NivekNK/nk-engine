project(editor)

add_executable(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    GLM_FORCE_SWIZZLE
)

target_sources(${PROJECT_NAME}
PRIVATE
    src/editor.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE engine)
