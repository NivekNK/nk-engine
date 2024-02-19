#include "nkpch.h"

#include "system/memory_system.h"

namespace nk {
    MemorySystem* MemorySystem::s_instance = nullptr;

    MemorySystem::~MemorySystem() {

    }

    void MemorySystem::init() {
        Assert(s_instance == nullptr)

        s_instance = new MemorySystem();
    }

    void MemorySystem::shutdown() {
        delete s_instance;
    }

    void MemorySystem::insert(const Allocator& allocator) {

    }

    void MemorySystem::allocate_memory(const Allocator& allocator, const u64 size_bytes, str file, const u32 line) {

    }

    void MemorySystem::free_memory(const Allocator& allocator, const u64 size_bytes, str file, const u32 line) {

    }

    void MemorySystem::report() {

    }
}
