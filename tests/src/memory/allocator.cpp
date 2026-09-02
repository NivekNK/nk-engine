#include <gtest/gtest.h>

#include "memory/allocator.h"
#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"

namespace {
    class ProbeAllocator final : public nk::mem::Allocator {
    public:
        void init() {}
        nk::cstr to_cstr() const noexcept override { return "ProbeAllocator"; }

        nk::u64 backend_allocation_calls = 0;

    protected:
        void* _do_allocate(nk::u64 size_bytes, nk::u64) noexcept override {
            ++backend_allocation_calls;
            if (m_active_allocations != 0 || size_bytes > sizeof(m_storage))
                return nullptr;

            m_reserved_bytes = size_bytes;
            m_used_bytes = size_bytes;
            ++m_active_allocations;
            _update_peak();
            return m_storage;
        }

        bool _do_free(void* data, nk::u64 size_bytes) noexcept override {
            if (data != m_storage || m_active_allocations == 0 || size_bytes != m_used_bytes)
                return false;
            m_reserved_bytes = 0;
            m_used_bytes = 0;
            m_active_allocations = 0;
            return true;
        }

    private:
        alignas(64) nk::u8 m_storage[256]{};
    };

    class FailingAllocator final : public nk::mem::Allocator {
    public:
        void init() {}
        nk::cstr to_cstr() const noexcept override { return "FailingAllocator"; }

    protected:
        void* _do_allocate(nk::u64, nk::u64) noexcept override { return nullptr; }
        bool _do_free(void*, nk::u64) noexcept override { return false; }
    };

    class RecordingTracker final : public nk::mem::AllocationTracker {
    public:
        nk::mem::AllocatorId register_allocator(
            nk::mem::Allocator&,
            const nk::mem::AllocatorDescriptor& descriptor) noexcept override {
            ++register_count;
            last_descriptor = descriptor;
            registered_allocator_id = next_id++;
            return registered_allocator_id;
        }

        void unregister_allocator(nk::mem::AllocatorId allocator_id) noexcept override {
            ++unregister_count;
            last_allocator_id = allocator_id;
        }

        void on_allocate(const nk::mem::AllocationEvent& event) noexcept override {
            ++allocation_count;
            last_allocation = event;
            allocation_freed = false;
        }

        nk::mem::FreeValidation validate_free(
            nk::mem::AllocatorId allocator_id,
            void* address,
            nk::u64 size_bytes) noexcept override {
            ++validation_count;
            if (allocator_id != registered_allocator_id)
                return nk::mem::FreeValidation::UnknownAllocator;
            if (allocation_count == 0 || address != last_allocation.address)
                return nk::mem::FreeValidation::UnknownAddress;
            if (allocation_freed)
                return nk::mem::FreeValidation::AlreadyFreed;
            if (size_bytes != last_allocation.size_bytes)
                return nk::mem::FreeValidation::SizeMismatch;
            return nk::mem::FreeValidation::Valid;
        }

        void on_free(const nk::mem::AllocationEvent& event) noexcept override {
            ++free_count;
            last_allocation = event;
            allocation_freed = true;
        }

        void on_reset(const nk::mem::AllocatorResetEvent&) noexcept override {
            ++reset_count;
        }

        nk::cstr allocator_name(nk::mem::AllocatorId) const noexcept override {
            return "Recorded";
        }

        nk::mem::AllocatorId next_id = 17;
        nk::mem::AllocatorId registered_allocator_id = nk::mem::invalid_allocator_id;
        nk::mem::AllocatorId last_allocator_id = nk::mem::invalid_allocator_id;
        nk::u32 register_count = 0;
        nk::u32 unregister_count = 0;
        nk::u32 allocation_count = 0;
        nk::u32 validation_count = 0;
        nk::u32 free_count = 0;
        nk::u32 reset_count = 0;
        nk::mem::AllocatorDescriptor last_descriptor{};
        nk::mem::AllocationEvent last_allocation{};
        bool allocation_freed = false;
    };

    struct ConstructionProbe {
        ConstructionProbe() { ++construction_count; }
        static inline nk::u32 construction_count = 0;
    };
}

TEST(Allocator, RejectsOperationsBeforeInitialization) {
    ProbeAllocator allocator;

    EXPECT_EQ(allocator._allocate_raw(8, alignof(nk::u64)), nullptr);
    EXPECT_FALSE(allocator._free_raw(reinterpret_cast<void*>(1), 8));
    EXPECT_EQ(allocator.backend_allocation_calls, 0);
}

TEST(Allocator, ValidatesSizeAlignmentAndTypedOverflow) {
    ProbeAllocator allocator;
    ASSERT_NE(allocator._allocator_init_untracked<ProbeAllocator>(), nullptr);

    EXPECT_EQ(allocator._allocate_raw(0, alignof(nk::u64)), nullptr);
    EXPECT_EQ(allocator._allocate_raw(8, 0), nullptr);
    EXPECT_EQ(allocator._allocate_raw(8, 3), nullptr);
    EXPECT_EQ(allocator._allocate_lot_t<nk::u64>(nk::numeric::u64_max), nullptr);
    EXPECT_EQ(allocator.backend_allocation_calls, 0);

    void* data = allocator._allocate_raw(32, 32);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(data) % 32, 0);
    EXPECT_EQ(allocator.get_reserved_bytes(), 32);
    EXPECT_EQ(allocator.get_used_bytes(), 32);
    EXPECT_EQ(allocator.get_peak_used_bytes(), 32);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    EXPECT_TRUE(allocator._free_raw(data, 32));
    EXPECT_EQ(allocator.get_used_bytes(), 0);
    EXPECT_EQ(allocator.get_peak_used_bytes(), 32);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Allocator, DoesNotConstructWhenAllocationFails) {
    FailingAllocator allocator;
    ASSERT_NE(allocator._allocator_init_untracked<FailingAllocator>(), nullptr);
    ConstructionProbe::construction_count = 0;

    EXPECT_EQ(allocator._construct_t<ConstructionProbe>(), nullptr);
    EXPECT_EQ(ConstructionProbe::construction_count, 0);
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(Allocator, SupportsExplicitTrackerAndControlledDetach) {
    RecordingTracker tracker;
    ProbeAllocator allocator;
    ASSERT_NE(
        allocator._allocator_init_tracked<ProbeAllocator>(
            tracker,
            __FILE__,
            __LINE__,
            "Tracked probe",
            7),
        nullptr);

    EXPECT_TRUE(allocator.is_tracked());
    EXPECT_EQ(tracker.register_count, 1);
    EXPECT_STREQ(allocator._allocator_name(), "Recorded");

    void* data = allocator._allocate_raw(16, 8);
    ASSERT_NE(data, nullptr);
    EXPECT_FALSE(allocator.detach_tracker());
    EXPECT_FALSE(allocator._free_raw(data, 8));
    EXPECT_TRUE(allocator._free_raw(data, 16));
    EXPECT_FALSE(allocator._free_raw(data, 16));
    EXPECT_TRUE(allocator.detach_tracker());

    EXPECT_EQ(tracker.allocation_count, 1);
    EXPECT_EQ(tracker.validation_count, 3);
    EXPECT_EQ(tracker.free_count, 1);
    EXPECT_EQ(tracker.unregister_count, 1);
}

TEST(Allocator, CanAttachTrackerToInitializedUntrackedAllocator) {
    RecordingTracker tracker;
    ProbeAllocator allocator;
    ASSERT_NE(allocator._allocator_init_untracked<ProbeAllocator>(), nullptr);

    EXPECT_TRUE(allocator.attach_tracker(
        tracker,
        "Attached probe",
        11,
        {__FILE__, __LINE__}));

    EXPECT_TRUE(allocator.is_tracked());
    EXPECT_EQ(tracker.register_count, 1);
    EXPECT_TRUE(allocator.detach_tracker());
}

TEST(Allocator, RejectsUnavailableTrackerWithoutFallingBackToUntracked) {
    RecordingTracker tracker;
    tracker.next_id = nk::mem::invalid_allocator_id;
    ProbeAllocator allocator;

    EXPECT_EQ(
        allocator._allocator_init_tracked<ProbeAllocator>(
            tracker,
            __FILE__,
            __LINE__,
            "Rejected probe",
            12),
        nullptr);
    EXPECT_EQ(allocator.lifecycle(), nk::mem::AllocatorLifecycle::InitializationFailed);
    EXPECT_FALSE(allocator.is_tracked());
    EXPECT_EQ(allocator._allocate_raw(8, alignof(nk::u64)), nullptr);
    EXPECT_EQ(allocator.backend_allocation_calls, 0);
}
#endif

TEST(Allocator, MoveAssignmentTransfersOnlyIntoEmptyDestination) {
    nk::mem::MallocAllocator source{nk::mem::untracked};
    nk::mem::MallocAllocator destination{nk::mem::untracked};

    void* source_data = source._allocate_raw(24, alignof(std::max_align_t));
    ASSERT_NE(source_data, nullptr);

    destination = std::move(source);
    EXPECT_EQ(source.lifecycle(), nk::mem::AllocatorLifecycle::MovedFrom);
    EXPECT_EQ(destination.get_used_bytes(), 24);
    EXPECT_TRUE(destination._free_raw(source_data, 24));

    nk::mem::MallocAllocator occupied_source{nk::mem::untracked};
    nk::mem::MallocAllocator occupied_destination{nk::mem::untracked};
    void* occupied_source_data = occupied_source._allocate_raw(8, alignof(nk::u64));
    void* occupied_destination_data = occupied_destination._allocate_raw(16, alignof(nk::u64));
    ASSERT_NE(occupied_source_data, nullptr);
    ASSERT_NE(occupied_destination_data, nullptr);

    occupied_destination = std::move(occupied_source);
    EXPECT_EQ(occupied_destination.get_used_bytes(), 16);
    EXPECT_EQ(occupied_source.get_used_bytes(), 8);

    EXPECT_TRUE(occupied_destination._free_raw(occupied_destination_data, 16));
    EXPECT_TRUE(occupied_source._free_raw(occupied_source_data, 8));
}

TEST(Allocator, EarlyJournalHasFixedCheckedCapacity) {
    nk::mem::EarlyAllocationJournal journal;
    const nk::mem::EarlyAllocationRecord record{
        .type = nk::mem::EarlyAllocationEventType::Allocate,
        .allocator_id = 3,
        .descriptor = {},
        .allocation = {},
        .reset = {},
    };

    for (nk::u32 index = 0; index < nk::mem::EarlyAllocationJournal::capacity; ++index)
        ASSERT_TRUE(journal.push(record));

    EXPECT_FALSE(journal.push(record));
    EXPECT_EQ(journal.count(), nk::mem::EarlyAllocationJournal::capacity);
    EXPECT_EQ(journal.dropped_count(), 1);
    EXPECT_FALSE(journal.complete());

    journal.clear();
    EXPECT_EQ(journal.count(), 0);
    EXPECT_TRUE(journal.complete());
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(Allocator, RawOsMemoryIsUntrackedAndEarlyEventsAreReplayed) {
    auto& memory_system = nk::mem::MemorySystem::get();
    ASSERT_EQ(memory_system.state(), nk::mem::MemorySystemState::Cold);

    void* raw_data = (nk::os::allocate_raw)(32, alignof(std::max_align_t));
    ASSERT_NE(raw_data, nullptr);
    EXPECT_EQ(memory_system.journal_count(), 0);
    EXPECT_TRUE((nk::os::free_raw)(raw_data, 32));

    void* native_data = native_allocate(16, alignof(std::max_align_t));
    ASSERT_NE(native_data, nullptr);
    EXPECT_EQ(memory_system.journal_count(), 1);

    {
        nk::mem::MallocAllocator allocator;
        ASSERT_NE(
            allocator._allocator_init_tracked<nk::mem::MallocAllocator>(
                memory_system,
                __FILE__,
                __LINE__,
                "Early allocator",
                nk::MemoryType::Test),
            nullptr);
        void* allocator_data = allocator._allocate_raw(24, alignof(std::max_align_t));
        ASSERT_NE(allocator_data, nullptr);
        EXPECT_EQ(memory_system.journal_count(), 3);

        nk::mem::MemorySystem::init();
        EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Ready);
        EXPECT_EQ(memory_system.journal_count(), 0);
        EXPECT_STREQ(allocator._allocator_name(), "Early allocator");

        EXPECT_TRUE(allocator._free_raw(allocator_data, 24));
    }

    native_free(native_data, 16);
    nk::mem::MemorySystem::shutdown();
    EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Stopped);
}
#endif
