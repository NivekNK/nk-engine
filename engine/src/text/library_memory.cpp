#include "nkpch.h"

#include "resources/text_memory.h"
#include "memory/malloc_allocator.h"
#include "core/thread.h"

namespace {
    struct alignas(std::max_align_t) Block {
        nk::u64 size;
    };

    struct LibraryMemory {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::Mutex mutex;
        nk::TextLibraryMemory statistics{};
        LibraryMemory() { (void)mutex.init(); }
    };

    LibraryMemory& memory() {
        // Constructed before HB registers its atexit cleanup callbacks. Those
        // callbacks run before this allocator's destructor on both platforms.
        static LibraryMemory storage;
        return storage;
    }
}

extern "C" void* hb_malloc_impl(std::size_t size) {
    if (size == 0 || size > SIZE_MAX - sizeof(Block))
        return nullptr;
    auto& storage = memory();
    auto lock = nk::LockGuard::acquire(storage.mutex);
    if (!lock)
        return nullptr;
    auto* block = static_cast<Block*>(storage.allocator.allocate_raw(
        size + sizeof(Block), alignof(Block)));
    if (block == nullptr)
        return nullptr;
    block->size = size;
    storage.statistics.live_bytes += size;
    ++storage.statistics.live_blocks;
    storage.statistics.peak_bytes = std::max(
        storage.statistics.peak_bytes, storage.statistics.live_bytes);
    return block + 1;
}

extern "C" void hb_free_impl(void* pointer) {
    if (pointer == nullptr)
        return;
    auto& storage = memory();
    auto lock = nk::LockGuard::acquire(storage.mutex);
    if (!lock)
        return;
    auto* block = static_cast<Block*>(pointer) - 1;
    const nk::u64 size = block->size;
    if (storage.allocator.free_raw(block, size + sizeof(Block))) {
        storage.statistics.live_bytes -= size;
        --storage.statistics.live_blocks;
    }
}

extern "C" void* hb_calloc_impl(std::size_t count, std::size_t size) {
    if (count == 0 || size == 0 || count > SIZE_MAX / size)
        return nullptr;
    void* result = hb_malloc_impl(count * size);
    if (result != nullptr)
        std::memset(result, 0, count * size);
    return result;
}

extern "C" void* hb_realloc_impl(void* original, std::size_t size) {
    if (size == 0) {
        hb_free_impl(original);
        return nullptr;
    }
    if (original == nullptr)
        return hb_malloc_impl(size);
    void* replacement = hb_malloc_impl(size);
    if (replacement == nullptr)
        return nullptr;
    std::memcpy(replacement, original,
        std::min<std::size_t>((static_cast<Block*>(original) - 1)->size, size));
    hb_free_impl(original);
    return replacement;
}

nk::TextLibraryMemory nk::harfbuzz_memory_statistics() noexcept {
    auto& storage = memory();
    auto lock = LockGuard::acquire(storage.mutex);
    return lock ? storage.statistics : TextLibraryMemory{};
}
