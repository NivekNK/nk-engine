#pragma once

namespace nk {
    struct Texture {
        u32 id;
        u32 width;
        u32 height;
        u8 channel_count;
        bool has_transparency;
        u32 generation;
        void* m_internal_data;
    };
}
