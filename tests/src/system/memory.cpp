#include <gtest/gtest.h>

#define NK_ACTIVE_MEMORY_SYSTEM TRUE
#include "system/memory.h"

TEST(MemorySystem, MemorySystemInit) {
    NK_MEMORY_SYSTEM_INIT();
    NK_MEMORY_SYSTEM_SHUTDOWN();
}
