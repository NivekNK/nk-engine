project(editor)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
PRIVATE
    src/editor.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE engine)
