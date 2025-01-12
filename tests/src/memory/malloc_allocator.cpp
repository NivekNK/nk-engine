#include <gtest/gtest.h>

#include "systems/memory_system.h"
#include "memory/malloc_allocator.h"

TEST(MallocAllocator, MallocAllocatorInit) {
    NK_MEMORY_SYSTEM_INIT();

    nk::u32* data = native_allocate_lot(nk::u32, 5);

    nk::mem::MallocAllocator allocator; 
    allocator.allocator_init(nk::mem::MallocAllocator, "Test", nk::MemoryType::System);

    constexpr nk::u8 lot = 10;
    nk::u32* lot_data = allocator.allocate_lot_t(nk::u32, lot);

    NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

    allocator.free_lot_t(nk::u32, lot_data, lot);
    native_free_lot(nk::u32, data, 5);

    NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

    NK_MEMORY_SYSTEM_SHUTDOWN();
}
