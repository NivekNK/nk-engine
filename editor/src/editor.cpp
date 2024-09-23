#include <nk/app_creator.h>

#include <math/vec2.h>

NK_EXTEND_MEMORY_TYPE(Test)

class Editor : public nk::App {
public:
    Editor() : nk::App({
        .name = "Editor",
        .start_pos_x = 100,
        .start_pos_y = 100,
        .start_width = 1280,
        .start_height = 720
    }) {
        NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE();
        DebugLog("Create Editor");

        nk::vec2 v0(0.0f, 1.0f);

        nk::vec2 v1(3.0f, 2.0f);

        nk::vec2 result = v0 + v1;

        DebugLog("Result: {}, {}", result.x, result.elements[1]);
    }

    virtual ~Editor() override {
        DebugLog("Editor destroyed");
    }
};

CREATE_APP(Editor)
