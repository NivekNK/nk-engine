#pragma once

#include "core/defines.h"

namespace nk {
    struct TextLibraryMemory {
        u64 live_bytes = 0;
        u64 peak_bytes = 0;
        u64 live_blocks = 0;
    };

    // HarfBuzz owns process-wide interned languages and function tables, which
    // it frees at process exit. They must never borrow a FontSystem allocator.
    [[nodiscard]] TextLibraryMemory harfbuzz_memory_statistics() noexcept;
}
