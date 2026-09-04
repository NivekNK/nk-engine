#pragma once

#include "core/input_codes.h"
#include "renderer/lighting.h"

namespace nk {
    [[nodiscard]] constexpr bool render_view_mode_from_key(
        const KeyCodeFlag keycode,
        RenderViewMode& mode) noexcept {
        switch (keycode) {
            case KeyCode::Num0:
                mode = RenderViewMode::default_lit;
                return true;
            case KeyCode::Num1:
                mode = RenderViewMode::lighting_only;
                return true;
            case KeyCode::Num2:
                mode = RenderViewMode::normals;
                return true;
            default:
                return false;
        }
    }
}
