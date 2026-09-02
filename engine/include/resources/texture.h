#pragma once

#include "core/defines.h"

namespace nk {
    struct Texture {
        u32 id = numeric::invalid_id;
        u32 width = 0;
        u32 height = 0;
        u8 channel_count = 0;
        bool has_transparency = false;
        u32 generation = numeric::invalid_id;
        void* m_internal_data = nullptr;

        bool valid() const noexcept {
            return generation != numeric::invalid_id &&
                   m_internal_data != nullptr;
        }
    };
}
