#include "nkpch.h"

#include "systems/memory_system.h"

#include "collections/dyarr.h"
#include "collections/map.h"
#include "core/hash.h"
#include "memory/allocator.h"

namespace nk::mem {
    struct MemorySystemStorageAccess {
        static void* data(MemorySystem& system) noexcept {
            return system.m_data;
        }
    };

    struct AllocationKey {
        AllocatorId allocator_id = invalid_allocator_id;
        void* address = nullptr;

        bool operator==(const AllocationKey&) const noexcept = default;
    };

    inline u64 hash64(
        const AllocationKey& key,
        const u64 seed = hash_seed::memory_allocations) noexcept {
        const u64 allocator_hash = hash64_bytes(
            &key.allocator_id,
            sizeof(key.allocator_id),
            seed);
        return nk::hash64(key.address, allocator_hash);
    }

    namespace {
        thread_local bool inside_tracker_callback = false;

        class TrackerCallbackScope {
        public:
            TrackerCallbackScope() noexcept {
                inside_tracker_callback = true;
            }

            ~TrackerCallbackScope() {
                inside_tracker_callback = false;
            }

            TrackerCallbackScope(const TrackerCallbackScope&) = delete;
            TrackerCallbackScope& operator=(const TrackerCallbackScope&) = delete;
        };

        struct AllocationInfo {
            u64 size_bytes = 0;
            cstr file = "<unknown>";
            u32 line = 0;
        };

        struct FreeInfo {
            u64 size_bytes = 0;
            cstr file = "<unknown>";
            u32 line = 0;
        };

        struct AllocationRecord {
            AllocationInfo allocated{};
            FreeInfo freed{};
        };

        struct AllocatorRecord {
            bool registered = false;
            cstr name = nullptr;
            cstr implementation = nullptr;
            u32 type = MemoryType::None;
            u64 reserved_bytes = 0;
            u64 used_bytes = 0;
            u64 peak_used_bytes = 0;
            u64 active_allocations = 0;
            AllocationInfo init{};
        };

        struct MemorySystemInfo {
            cl::dyarr<AllocatorRecord> allocators;
            cl::map<AllocationKey, AllocationRecord> allocations;

            bool init(MallocAllocator& allocator) noexcept {
                if (!allocators.dyarr_init(&allocator, 16))
                    return false;
                if (!allocations.map_init(
                        &allocator,
                        EarlyAllocationJournal::capacity,
                        hash_seed::memory_allocations)) {
                    (void)allocators.dyarr_shutdown();
                    return false;
                }
                return true;
            }

            void shutdown() noexcept {
                (void)allocations.map_shutdown();
                (void)allocators.dyarr_shutdown();
            }
        };

        cstr source_file(const SourceLocation source) noexcept {
            return source.file == nullptr ? "<unknown>" : source.file;
        }

        MemorySystemInfo* system_info(MemorySystem& system) noexcept {
            return static_cast<MemorySystemInfo*>(MemorySystemStorageAccess::data(system));
        }

        const MemorySystemInfo* system_info(const MemorySystem& system) noexcept {
            return static_cast<const MemorySystemInfo*>(
                MemorySystemStorageAccess::data(const_cast<MemorySystem&>(system)));
        }

        void memory_in_bytes(strbuf<64>& output, const u64 memory) noexcept {
            if (memory >= GiB()) {
                format_to(output, "{:.2f} GiB", memory / static_cast<f64>(GiB()));
            } else if (memory >= MiB()) {
                format_to(output, "{:.2f} MiB", memory / static_cast<f64>(MiB()));
            } else if (memory >= KiB()) {
                format_to(output, "{:.2f} KiB", memory / static_cast<f64>(KiB()));
            } else {
                format_to(output, "{:.2f} B", static_cast<f64>(memory));
            }
        }

        void memory_in_bytes(
            strbuf<96>& output,
            const u64 reserved,
            const u64 used) noexcept {
            if (used >= GiB() || reserved >= GiB()) {
                format_to(
                    output,
                    "{:.2f}/{:.2f} GiB",
                    reserved / static_cast<f64>(GiB()),
                    used / static_cast<f64>(GiB()));
            } else if (used >= MiB() || reserved >= MiB()) {
                format_to(
                    output,
                    "{:.2f}/{:.2f} MiB",
                    reserved / static_cast<f64>(MiB()),
                    used / static_cast<f64>(MiB()));
            } else if (used >= KiB() || reserved >= KiB()) {
                format_to(
                    output,
                    "{:.2f}/{:.2f} KiB",
                    reserved / static_cast<f64>(KiB()),
                    used / static_cast<f64>(KiB()));
            } else {
                format_to(
                    output,
                    "{:.2f}/{:.2f} B",
                    static_cast<f64>(reserved),
                    static_cast<f64>(used));
            }
        }

        void update_statistics(
            AllocatorRecord& destination,
            const AllocatorStatistics& source) noexcept {
            destination.reserved_bytes = source.reserved_bytes;
            destination.used_bytes = source.used_bytes;
            destination.peak_used_bytes = source.peak_used_bytes;
            destination.active_allocations = source.active_allocations;
        }

        void remove_allocations_for(
            MemorySystemInfo& info,
            const AllocatorId allocator_id) noexcept {
            for (;;) {
                bool removed = false;
                for (const auto entry : info.allocations) {
                    if (entry.key.allocator_id != allocator_id)
                        continue;
                    const AllocationKey key = entry.key;
                    (void)info.allocations.remove(key);
                    removed = true;
                    break;
                }
                if (!removed)
                    return;
            }
        }
    }

    AllocationTracker& default_allocation_tracker() noexcept {
        return MemorySystem::get();
    }

    MemorySystem& MemorySystem::init() {
        MemorySystem& instance = get();

        if (instance.m_state == MemorySystemState::Ready)
            return instance;
        if (instance.m_state == MemorySystemState::Bootstrapping ||
            instance.m_state == MemorySystemState::ShuttingDown) {
            instance.log_error("nk::MemorySystem init rejected during a state transition.");
            return instance;
        }

        if (instance.m_state == MemorySystemState::Stopped) {
            instance.m_journal.clear();
            instance.m_next_allocator_id = native_allocator_id + 1;
            instance.m_dropped_event_count = 0;
            instance.m_reentrant_event_count = 0;
            instance.m_metadata_failure_count = 0;
            instance.m_state = MemorySystemState::Cold;
        }

        instance.m_state = MemorySystemState::Bootstrapping;
        auto* info = instance.m_metadata_allocator._construct_t_args<MemorySystemInfo>(
            __FILE__,
            __LINE__);
        if (info == nullptr || !info->init(instance.m_metadata_allocator)) {
            if (info != nullptr) {
                (void)instance.m_metadata_allocator._deconstruct_t<MemorySystemInfo>(
                    __FILE__,
                    __LINE__,
                    info);
            }
            instance.m_state = MemorySystemState::Stopped;
            instance.log_error("nk::MemorySystem could not initialize its metadata storage.");
            return instance;
        }

        instance.m_data = info;
        instance.apply_register(native_allocator_id, {
            .name = "Native Allocation",
            .implementation = "Native",
            .memory_type = MemoryType::Native,
            .source = {__FILE__, __LINE__},
            .statistics = {},
        });

        if (!instance.m_journal.complete()) {
            instance.m_dropped_event_count += instance.m_journal.dropped_count();
            info->shutdown();
            (void)instance.m_metadata_allocator._deconstruct_t<MemorySystemInfo>(
                __FILE__,
                __LINE__,
                info);
            instance.m_data = nullptr;
            instance.m_journal.clear();
            instance.m_state = MemorySystemState::Stopped;
            instance.log_error("nk::MemorySystem bootstrap failed: early allocation journal overflow.");
            return instance;
        }

        instance.replay_journal();
        instance.m_journal.clear();
        instance.m_state = MemorySystemState::Ready;
        instance.log_title("nk::MemorySystem initialized.");
        return instance;
    }

    void MemorySystem::shutdown() {
        MemorySystem& instance = get();
        if (instance.m_state != MemorySystemState::Ready &&
            instance.m_state != MemorySystemState::ShuttingDown)
            return;

        instance.m_state = MemorySystemState::ShuttingDown;
        MemorySystemInfo* info = system_info(instance);
        if (info != nullptr) {
            info->shutdown();
            (void)instance.m_metadata_allocator._deconstruct_t<MemorySystemInfo>(
                __FILE__,
                __LINE__,
                info);
        }
        instance.m_data = nullptr;
        instance.m_state = MemorySystemState::Stopped;
        instance.log_title("nk::MemorySystem shutdown.");
    }

    AllocatorId MemorySystem::register_allocator(
        [[maybe_unused]] Allocator& allocator,
        const AllocatorDescriptor& descriptor) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return invalid_allocator_id;
        }
        TrackerCallbackScope callback_scope;

        if (m_state == MemorySystemState::Stopped ||
            m_state == MemorySystemState::ShuttingDown ||
            m_next_allocator_id == invalid_allocator_id) {
            return invalid_allocator_id;
        }

        const AllocatorId allocator_id = m_next_allocator_id++;
        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            journal({
                .type = EarlyAllocationEventType::RegisterAllocator,
                .allocator_id = allocator_id,
                .descriptor = descriptor,
                .allocation = {},
                .reset = {},
            });
        } else {
            apply_register(allocator_id, descriptor);
        }
        return allocator_id;
    }

    void MemorySystem::unregister_allocator(const AllocatorId allocator_id) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return;
        }
        TrackerCallbackScope callback_scope;

        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            journal({
                .type = EarlyAllocationEventType::UnregisterAllocator,
                .allocator_id = allocator_id,
                .descriptor = {},
                .allocation = {},
                .reset = {},
            });
        } else if (m_state == MemorySystemState::Ready) {
            apply_unregister(allocator_id);
        }
    }

    void MemorySystem::on_allocate(const AllocationEvent& event) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return;
        }
        TrackerCallbackScope callback_scope;

        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            journal({
                .type = EarlyAllocationEventType::Allocate,
                .allocator_id = event.allocator_id,
                .descriptor = {},
                .allocation = event,
                .reset = {},
            });
        } else if (m_state == MemorySystemState::Ready) {
            apply_allocate(event);
        }
    }

    FreeValidation MemorySystem::validate_free(
        const AllocatorId allocator_id,
        void* address,
        const u64 size_bytes) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return FreeValidation::TrackerUnavailable;
        }
        TrackerCallbackScope callback_scope;

        if (address == nullptr)
            return FreeValidation::UnknownAddress;
        if (size_bytes == 0)
            return FreeValidation::SizeMismatch;

        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            bool allocator_registered = allocator_id == native_allocator_id;
            bool address_seen = false;
            bool address_freed = false;
            u64 allocated_size = 0;

            for (u32 index = 0; index < m_journal.count(); ++index) {
                const EarlyAllocationRecord& record = m_journal[index];
                switch (record.type) {
                    case EarlyAllocationEventType::RegisterAllocator:
                        if (record.allocator_id == allocator_id)
                            allocator_registered = true;
                        break;
                    case EarlyAllocationEventType::UnregisterAllocator:
                        if (record.allocator_id == allocator_id)
                            allocator_registered = false;
                        break;
                    case EarlyAllocationEventType::Allocate:
                        if (record.allocation.allocator_id == allocator_id &&
                            record.allocation.address == address) {
                            address_seen = true;
                            address_freed = false;
                            allocated_size = record.allocation.size_bytes;
                        }
                        break;
                    case EarlyAllocationEventType::Free:
                        if (record.allocation.allocator_id == allocator_id &&
                            record.allocation.address == address && address_seen) {
                            address_freed = true;
                        }
                        break;
                    case EarlyAllocationEventType::Reset:
                        if (record.reset.allocator_id == allocator_id && address_seen)
                            address_freed = true;
                        break;
                }
            }

            if (!allocator_registered)
                return FreeValidation::UnknownAllocator;
            if (!address_seen)
                return FreeValidation::UnknownAddress;
            if (address_freed)
                return FreeValidation::AlreadyFreed;
            if (allocated_size != size_bytes)
                return FreeValidation::SizeMismatch;
            return FreeValidation::Valid;
        }

        if (m_state != MemorySystemState::Ready)
            return FreeValidation::TrackerUnavailable;

        const MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || allocator_id >= info->allocators.length())
            return FreeValidation::UnknownAllocator;

        const AllocatorRecord& stats = info->allocators[allocator_id];
        if (!stats.registered)
            return FreeValidation::UnknownAllocator;

        const AllocationRecord* allocation = info->allocations.find({allocator_id, address});
        if (allocation == nullptr)
            return FreeValidation::UnknownAddress;
        if (allocation->freed.size_bytes != 0)
            return FreeValidation::AlreadyFreed;
        if (allocation->allocated.size_bytes != size_bytes)
            return FreeValidation::SizeMismatch;
        return FreeValidation::Valid;
    }

    void MemorySystem::on_free(const AllocationEvent& event) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return;
        }
        TrackerCallbackScope callback_scope;

        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            journal({
                .type = EarlyAllocationEventType::Free,
                .allocator_id = event.allocator_id,
                .descriptor = {},
                .allocation = event,
                .reset = {},
            });
        } else if (m_state == MemorySystemState::Ready) {
            apply_free(event);
        }
    }

    void MemorySystem::on_reset(const AllocatorResetEvent& event) noexcept {
        if (inside_tracker_callback) {
            ++m_reentrant_event_count;
            return;
        }
        TrackerCallbackScope callback_scope;

        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            journal({
                .type = EarlyAllocationEventType::Reset,
                .allocator_id = event.allocator_id,
                .descriptor = {},
                .allocation = {},
                .reset = event,
            });
        } else if (m_state == MemorySystemState::Ready) {
            apply_reset(event);
        }
    }

    bool MemorySystem::journal(const EarlyAllocationRecord& record) noexcept {
        const bool stored = m_journal.push(record);
        if (!stored && m_journal.dropped_count() == 1) {
            constexpr char message[] =
                "nk::MemorySystem early allocation journal overflow; report will be incomplete.\n";
            os::write(message, sizeof(message) - 1);
            os::flush();
        }
        return stored;
    }

    void MemorySystem::replay_journal() {
        for (u32 index = 0; index < m_journal.count(); ++index) {
            const EarlyAllocationRecord& record = m_journal[index];
            switch (record.type) {
                case EarlyAllocationEventType::RegisterAllocator:
                    apply_register(record.allocator_id, record.descriptor);
                    break;
                case EarlyAllocationEventType::UnregisterAllocator:
                    apply_unregister(record.allocator_id);
                    break;
                case EarlyAllocationEventType::Allocate:
                    apply_allocate(record.allocation);
                    break;
                case EarlyAllocationEventType::Free:
                    apply_free(record.allocation);
                    break;
                case EarlyAllocationEventType::Reset:
                    apply_reset(record.reset);
                    break;
            }
        }
    }

    void MemorySystem::apply_register(
        const AllocatorId allocator_id,
        const AllocatorDescriptor& descriptor) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr)
            return;
        if (allocator_id >= info->allocators.length() &&
            !info->allocators.dyarr_resize(static_cast<u64>(allocator_id) + 1)) {
            ++m_metadata_failure_count;
            return;
        }

        AllocatorRecord& stats = info->allocators[allocator_id];
        stats.registered = true;
        stats.name = descriptor.name == nullptr ? "<unnamed>" : descriptor.name;
        stats.implementation = descriptor.implementation == nullptr
            ? "<unknown>"
            : descriptor.implementation;
        stats.type = descriptor.memory_type;
        update_statistics(stats, descriptor.statistics);
        stats.init = {
            .size_bytes = descriptor.statistics.reserved_bytes,
            .file = source_file(descriptor.source),
            .line = descriptor.source.line,
        };
        remove_allocations_for(*info, allocator_id);
    }

    void MemorySystem::apply_unregister(const AllocatorId allocator_id) noexcept {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || allocator_id >= info->allocators.length())
            return;
        info->allocators[allocator_id].registered = false;
    }

    void MemorySystem::apply_allocate(const AllocationEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.length())
            return;

        AllocatorRecord& stats = info->allocators[event.allocator_id];
        if (event.allocator_id == native_allocator_id) {
            stats.reserved_bytes += event.size_bytes;
            stats.used_bytes += event.size_bytes;
            stats.peak_used_bytes = MaxValue(stats.peak_used_bytes, stats.used_bytes);
            ++stats.active_allocations;
        } else {
            update_statistics(stats, event.statistics);
        }

        if (!info->allocations.insert_or_assign(
                {event.allocator_id, event.address},
                {
                    .allocated = {
                        .size_bytes = event.size_bytes,
                        .file = source_file(event.source),
                        .line = event.source.line,
                    },
                    .freed = {},
                })) {
            ++m_metadata_failure_count;
        }
    }

    void MemorySystem::apply_free(const AllocationEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.length())
            return;

        AllocatorRecord& stats = info->allocators[event.allocator_id];
        if (event.allocator_id == native_allocator_id) {
            stats.reserved_bytes = event.size_bytes > stats.reserved_bytes
                ? 0
                : stats.reserved_bytes - event.size_bytes;
            stats.used_bytes = event.size_bytes > stats.used_bytes
                ? 0
                : stats.used_bytes - event.size_bytes;
            if (stats.active_allocations > 0)
                --stats.active_allocations;
        } else {
            update_statistics(stats, event.statistics);
        }

        AllocationRecord* allocation = info->allocations.find(
            {event.allocator_id, event.address});
        if (allocation != nullptr) {
            allocation->freed = {
                .size_bytes = event.size_bytes,
                .file = source_file(event.source),
                .line = event.source.line,
            };
        }
    }

    void MemorySystem::apply_reset(const AllocatorResetEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.length())
            return;

        AllocatorRecord& stats = info->allocators[event.allocator_id];
        update_statistics(stats, event.statistics);
        remove_allocations_for(*info, event.allocator_id);
    }

    cstr MemorySystem::allocator_name(const AllocatorId allocator_id) const noexcept {
        if (m_state == MemorySystemState::Cold ||
            m_state == MemorySystemState::Bootstrapping) {
            for (u32 index = m_journal.count(); index > 0; --index) {
                const EarlyAllocationRecord& record = m_journal[index - 1];
                if (record.type == EarlyAllocationEventType::RegisterAllocator &&
                    record.allocator_id == allocator_id) {
                    return record.descriptor.name == nullptr
                        ? "<unnamed>"
                        : record.descriptor.name;
                }
            }
            return "Invalid";
        }

        const MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || allocator_id >= info->allocators.length())
            return "Invalid";
        const cstr name = info->allocators[allocator_id].name;
        return name == nullptr ? "Invalid" : name;
    }

    void MemorySystem::log_report(const bool detailed) {
        MemorySystem& instance = get();
        if (detailed && instance.m_state == MemorySystemState::Ready)
            instance.m_state = MemorySystemState::ShuttingDown;
        MemorySystemInfo* info = system_info(instance);
        if (info == nullptr)
            return;

        instance.log_title("nk::MemorySystem Report");
        for (AllocatorRecord& stats : info->allocators) {
            if (stats.name == nullptr)
                continue;

            instance.log_info(
                "[Allocator: {}] ({}){}",
                stats.name,
                stats.implementation,
                stats.registered ? "" : " [detached]");

            if (stats.type != MemoryType::Native) {
                strbuf<64> initialized_size;
                memory_in_bytes(initialized_size, stats.init.size_bytes);
                instance.log_info(
                    "  - Initialized at: {}:{} with reserved size {}",
                    stats.init.file,
                    stats.init.line,
                    initialized_size);
            }

            strbuf<64> used;
            strbuf<96> reserved_used;
            if (stats.type == MemoryType::Native)
                memory_in_bytes(used, stats.used_bytes);
            else
                memory_in_bytes(reserved_used, stats.reserved_bytes, stats.used_bytes);
            strbuf<64> peak;
            memory_in_bytes(peak, stats.peak_used_bytes);
            instance.log_info(
                "  - Usage: {}, peak {}, {} active allocation(s)",
                stats.type == MemoryType::Native ? used.view() : reserved_used.view(),
                peak,
                stats.active_allocations);

            const AllocatorId allocator_id = static_cast<AllocatorId>(
                &stats - info->allocators.data());
            u64 leak_count = 0;
            u64 leaked_bytes = 0;
            for (const auto entry : info->allocations) {
                if (entry.key.allocator_id != allocator_id)
                    continue;

                const AllocationRecord& allocation = entry.value;
                if (allocation.freed.size_bytes == 0) {
                    ++leak_count;
                    leaked_bytes += allocation.allocated.size_bytes;
                    if (detailed) {
                        strbuf<64> allocation_size;
                        memory_in_bytes(allocation_size, allocation.allocated.size_bytes);
                        instance.log_error(
                            "  [LEAK] {} allocated at {}:{} was never freed. Address: {}",
                            allocation_size,
                            allocation.allocated.file,
                            allocation.allocated.line,
                            entry.key.address);
                    }
                } else if (allocation.freed.size_bytes != allocation.allocated.size_bytes) {
                    strbuf<64> allocated_size;
                    strbuf<64> freed_size;
                    memory_in_bytes(allocated_size, allocation.allocated.size_bytes);
                    memory_in_bytes(freed_size, allocation.freed.size_bytes);
                    instance.log_warn(
                        "  [WARN] Address {} allocated {}, but freed {}.",
                        entry.key.address,
                        allocated_size,
                        freed_size);
                } else if (detailed) {
                    strbuf<64> allocation_size;
                    memory_in_bytes(allocation_size, allocation.allocated.size_bytes);
                    instance.log_trace(
                        "  [OK] {} at Address: {}",
                        allocation_size,
                        entry.key.address);
                }
            }

            if (leak_count == 0) {
                instance.log_info("  - Leaks: 0 leak(s) found.");
            } else {
                strbuf<64> leaked_size;
                memory_in_bytes(leaked_size, leaked_bytes);
                instance.log_error(
                    "  - Leaks: {} leak(s) found, totalling {}.",
                    leak_count,
                    leaked_size);
            }
        }

        const AllocatorStatistics metadata = instance.m_metadata_allocator.statistics();
        strbuf<96> metadata_usage;
        memory_in_bytes(metadata_usage, metadata.reserved_bytes, metadata.used_bytes);
        strbuf<64> metadata_peak;
        memory_in_bytes(metadata_peak, metadata.peak_used_bytes);
        instance.log_info(
            "[Metadata allocator] usage {}, peak {}, {} active allocation(s)",
            metadata_usage,
            metadata_peak,
            metadata.active_allocations);

        if (instance.m_dropped_event_count != 0 ||
            instance.m_reentrant_event_count != 0 ||
            instance.m_metadata_failure_count != 0) {
            instance.log_warn(
                "Tracking incomplete: {} dropped journal event(s), {} reentrant event(s), {} metadata failure(s).",
                instance.m_dropped_event_count,
                instance.m_reentrant_event_count,
                instance.m_metadata_failure_count);
        }
        instance.log_title("End of nk::MemorySystem Report");
    }

    void MemorySystem::log_report_intermediate() {
        MemorySystem& instance = get();
        MemorySystemInfo* info = system_info(instance);
        if (info == nullptr)
            return;

        instance.log_title("nk::MemorySystem Usage Report");
        for (const AllocatorRecord& stats : info->allocators) {
            if (stats.name == nullptr)
                continue;

            strbuf<64> used;
            strbuf<96> reserved_used;
            if (stats.type == MemoryType::Native)
                memory_in_bytes(used, stats.used_bytes);
            else
                memory_in_bytes(reserved_used, stats.reserved_bytes, stats.used_bytes);
            strbuf<64> peak;
            memory_in_bytes(peak, stats.peak_used_bytes);
            instance.log_info(
                "[Allocator: {}] ({}) - {}, peak {}, {} active",
                stats.name,
                stats.implementation,
                stats.type == MemoryType::Native ? used.view() : reserved_used.view(),
                peak,
                stats.active_allocations);
        }
        instance.log_title("End of nk::MemorySystem Usage Report");
    }

    void MemorySystem::log(const cstr color, const strview msg) {
        const strview color_view{color};
        os::write(color_view.data(), color_view.length());
        os::write(msg.data(), msg.length());
        os::write("\033[0m\n", 5);
        os::flush();
    }
}

namespace nk {
    void memory_system_extended_memory_type(
        const MemoryType::Provider& provider) noexcept {
        MemoryType::Internal::provider = &provider;
    }
}
