#include <nk/app_creator.h>

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
    }

    virtual ~Editor() override {
        DebugLog("Editor destroyed");
    }
};

CREATE_APP(Editor)
