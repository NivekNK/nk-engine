#include <gtest/gtest.h>

#include "core/thread.h"
#include "memory/malloc_allocator.h"
#include "memory/synchronized_allocator.h"
#include "systems/memory_system.h"

namespace {
    struct WorkerContext {
        nk::mem::Allocator* allocator = nullptr;
        nk::u32 repetitions = 0;
        bool succeeded = true;
    };

    nk::u32 allocate_repeatedly(void* raw_context) noexcept {
        auto& context = *static_cast<WorkerContext*>(raw_context);
        for (nk::u32 iteration = 0;
             iteration < context.repetitions;
             ++iteration) {
            const nk::u64 alignment = nk::u64{1} << (iteration % 9);
            const nk::u64 size = 17 + iteration % 113;
            void* data = context.allocator->_allocate_raw(size, alignment);
            if (data == nullptr ||
                reinterpret_cast<std::uintptr_t>(data) % alignment != 0) {
                context.succeeded = false;
                return 1;
            }
            std::memset(data, static_cast<int>(iteration & 0xff), size);
            if (!context.allocator->_free_raw(data, size)) {
                context.succeeded = false;
                return 2;
            }
        }
        return 0;
    }
}

TEST(SynchronizedAllocator, RejectsInvalidOrAlreadyTrackedBacking) {
    nk::mem::MallocAllocator unavailable;
    nk::mem::SynchronizedAllocator invalid;
    EXPECT_EQ(
        invalid._allocator_init_untracked<nk::mem::SynchronizedAllocator>(
            unavailable),
        nullptr);

#if NK_MEMORY_TRACKING_ENABLED
    auto& tracker = nk::mem::MemorySystem::init();
    ASSERT_EQ(tracker.state(), nk::mem::MemorySystemState::Ready);
    nk::mem::MallocAllocator tracked;
    ASSERT_NE(
        tracked.allocator_init(
            nk::mem::MallocAllocator,
            "Tracked backing",
            nk::MemoryType::Test),
        nullptr);
    nk::mem::SynchronizedAllocator duplicate_tracking;
    EXPECT_EQ(
        duplicate_tracking._allocator_init_untracked<
            nk::mem::SynchronizedAllocator>(tracked),
        nullptr);
    nk::mem::MemorySystem::shutdown();
#endif
}

TEST(SynchronizedAllocator, SerializesSharedAllocationsAndPreservesAlignment) {
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::SynchronizedAllocator allocator{
        nk::mem::untracked,
        backing};
    ASSERT_TRUE(allocator.is_initialized());
    ASSERT_EQ(allocator.backing_allocator(), &backing);

    constexpr nk::u32 worker_count = 4;
    constexpr nk::u32 repetitions = 2000;
    WorkerContext contexts[worker_count]{};
    nk::Thread workers[worker_count];
    for (nk::u32 index = 0; index < worker_count; ++index) {
        contexts[index] = {&allocator, repetitions, true};
        ASSERT_TRUE(workers[index].start(allocate_repeatedly, &contexts[index]));
    }
    for (nk::u32 index = 0; index < worker_count; ++index) {
        auto joined = workers[index].join();
        ASSERT_TRUE(joined);
        EXPECT_EQ(*joined, 0u);
        EXPECT_TRUE(contexts[index].succeeded);
    }

    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
    EXPECT_EQ(allocator.get_used_bytes(), 0u);
    EXPECT_EQ(backing.get_active_allocation_count(), 0u);
    EXPECT_GT(allocator.get_peak_used_bytes(), 0u);
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(SynchronizedAllocator, SerializesTrackedWorkerAllocationEvents) {
    auto& tracker = nk::mem::MemorySystem::init();
    ASSERT_EQ(tracker.state(), nk::mem::MemorySystemState::Ready);
    const nk::u64 events_before = tracker.allocation_event_count();

    {
        nk::mem::MallocAllocator backing{nk::mem::untracked};
        nk::mem::SynchronizedAllocator allocator;
        ASSERT_NE(
            allocator.allocator_init(
                nk::mem::SynchronizedAllocator,
                "Shared worker allocator",
                nk::MemoryType::Test,
                backing),
            nullptr);

        constexpr nk::u32 worker_count = 4;
        constexpr nk::u32 repetitions = 500;
        WorkerContext contexts[worker_count]{};
        nk::Thread workers[worker_count];
        for (nk::u32 index = 0; index < worker_count; ++index) {
            contexts[index] = {&allocator, repetitions, true};
            ASSERT_TRUE(workers[index].start(
                allocate_repeatedly,
                &contexts[index]));
        }
        for (nk::u32 index = 0; index < worker_count; ++index) {
            auto joined = workers[index].join();
            ASSERT_TRUE(joined);
            EXPECT_EQ(*joined, 0u);
            EXPECT_TRUE(contexts[index].succeeded);
        }
        EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
    }

    EXPECT_EQ(
        tracker.allocation_event_count(),
        events_before + 4u * 500u);
    EXPECT_EQ(tracker.metadata_failure_count(), 0u);
    nk::mem::MemorySystem::shutdown();
}
#endif
