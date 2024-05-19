#include "nkpch.h"

#include "memory/memory_type.h"

#include "system/memory_system.h"

namespace nk {
    void memory_system_expanded_memory_type(const u64 max_memory_type, const std::function<cstr(MemoryTypeValue)>& memory_type_to_cstr) {
        if (nk::MemorySystem::is_initialized())
            nk::MemorySystem::get().expanded_memory_type(max_memory_type, memory_type_to_cstr);
    }
}
