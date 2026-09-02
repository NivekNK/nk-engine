#include "nkpch.h"

#include "systems/memory_system.h"

#include "memory/allocator.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace nk::mem {
    struct MemorySystemStorageAccess {
        static void* data(MemorySystem& system) noexcept {
            return system.m_data;
        }
    };

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
            std::string file;
            u32 line = 0;
        };

        struct FreeInfo {
            u64 size_bytes = 0;
            std::string file;
            u32 line = 0;
        };

        struct AllocatorInfo {
            AllocationInfo allocated;
            FreeInfo freed;
        };

        struct AllocationStats {
            bool registered = false;
            std::string name;
            std::string allocator;
            u32 type = 0;
            u64 reserved_bytes = 0;
            u64 used_bytes = 0;
            u64 peak_used_bytes = 0;
            u64 active_allocations = 0;
            AllocationInfo init;
            std::unordered_map<void*, AllocatorInfo> allocation_log;
        };

        struct MemorySystemInfo {
            std::vector<AllocationStats> allocators;
        };

        cstr source_file(SourceLocation source) noexcept {
            return source.file == nullptr ? "<unknown>" : source.file;
        }

        MemorySystemInfo* system_info(MemorySystem& system) noexcept {
            return static_cast<MemorySystemInfo*>(MemorySystemStorageAccess::data(system));
        }

        std::string memory_in_bytes(u64 memory) {
            if (memory >= GiB()) {
                return std::format("{:.2f} GiB", memory / static_cast<f32>(GiB()));
            }
            if (memory >= MiB()) {
                return std::format("{:.2f} MiB", memory / static_cast<f32>(MiB()));
            }
            if (memory >= KiB()) {
                return std::format("{:.2f} KiB", memory / static_cast<f32>(KiB()));
            }
            return std::format("{:.2f} B", static_cast<f32>(memory));
        }

        std::string memory_in_bytes(u64 reserved, u64 used) {
            if (used >= GiB() || reserved >= GiB()) {
                return std::format(
                    "{:.2f}/{:.2f} GiB",
                    reserved / static_cast<f32>(GiB()),
                    used / static_cast<f32>(GiB()));
            }
            if (used >= MiB() || reserved >= MiB()) {
                return std::format(
                    "{:.2f}/{:.2f} MiB",
                    reserved / static_cast<f32>(MiB()),
                    used / static_cast<f32>(MiB()));
            }
            if (used >= KiB() || reserved >= KiB()) {
                return std::format(
                    "{:.2f}/{:.2f} KiB",
                    reserved / static_cast<f32>(KiB()),
                    used / static_cast<f32>(KiB()));
            }
            return std::format(
                "{:.2f}/{:.2f} B",
                static_cast<f32>(reserved),
                static_cast<f32>(used));
        }

        void update_statistics(
            AllocationStats& destination,
            const AllocatorStatistics& source) noexcept {
            destination.reserved_bytes = source.reserved_bytes;
            destination.used_bytes = source.used_bytes;
            destination.peak_used_bytes = source.peak_used_bytes;
            destination.active_allocations = source.active_allocations;
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
            instance.m_state = MemorySystemState::Cold;
        }

        instance.m_state = MemorySystemState::Bootstrapping;
        auto* info = new (std::nothrow) MemorySystemInfo();
        if (info == nullptr) {
            instance.m_state = MemorySystemState::Stopped;
            instance.log_error("nk::MemorySystem could not allocate its transitional metadata.");
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
            delete info;
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
        if (instance.m_state != MemorySystemState::Ready)
            return;

        instance.m_state = MemorySystemState::ShuttingDown;
        delete system_info(instance);
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

    void MemorySystem::unregister_allocator(AllocatorId allocator_id) noexcept {
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
            constexpr cstr message =
                "nk::MemorySystem early allocation journal overflow; report will be incomplete.\n";
            os::write(message, std::char_traits<char>::length(message));
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
        AllocatorId allocator_id,
        const AllocatorDescriptor& descriptor) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr)
            return;
        if (info->allocators.size() <= allocator_id)
            info->allocators.resize(static_cast<std::size_t>(allocator_id) + 1);

        AllocationStats& stats = info->allocators[allocator_id];
        stats.registered = true;
        stats.name = descriptor.name == nullptr ? "<unnamed>" : descriptor.name;
        stats.allocator =
            descriptor.implementation == nullptr ? "<unknown>" : descriptor.implementation;
        stats.type = descriptor.memory_type;
        update_statistics(stats, descriptor.statistics);
        stats.init = {
            .size_bytes = descriptor.statistics.reserved_bytes,
            .file = source_file(descriptor.source),
            .line = descriptor.source.line,
        };
        stats.allocation_log.clear();
    }

    void MemorySystem::apply_unregister(AllocatorId allocator_id) noexcept {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || allocator_id >= info->allocators.size())
            return;
        info->allocators[allocator_id].registered = false;
    }

    void MemorySystem::apply_allocate(const AllocationEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.size())
            return;

        AllocationStats& stats = info->allocators[event.allocator_id];
        if (event.allocator_id == native_allocator_id) {
            stats.reserved_bytes += event.size_bytes;
            stats.used_bytes += event.size_bytes;
            stats.peak_used_bytes = MaxValue(stats.peak_used_bytes, stats.used_bytes);
            ++stats.active_allocations;
        } else {
            update_statistics(stats, event.statistics);
        }

        auto [iterator, inserted] = stats.allocation_log.try_emplace(
            event.address,
            AllocatorInfo{
                .allocated = {
                    .size_bytes = event.size_bytes,
                    .file = source_file(event.source),
                    .line = event.source.line,
                },
                .freed = {},
            });
        if (!inserted) {
            iterator->second.allocated = {
                .size_bytes = event.size_bytes,
                .file = source_file(event.source),
                .line = event.source.line,
            };
            iterator->second.freed = {};
        }
    }

    void MemorySystem::apply_free(const AllocationEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.size())
            return;

        AllocationStats& stats = info->allocators[event.allocator_id];
        if (event.allocator_id == native_allocator_id) {
            stats.reserved_bytes =
                event.size_bytes > stats.reserved_bytes ? 0 : stats.reserved_bytes - event.size_bytes;
            stats.used_bytes =
                event.size_bytes > stats.used_bytes ? 0 : stats.used_bytes - event.size_bytes;
            if (stats.active_allocations > 0)
                --stats.active_allocations;
        } else {
            update_statistics(stats, event.statistics);
        }

        auto iterator = stats.allocation_log.find(event.address);
        if (iterator != stats.allocation_log.end()) {
            iterator->second.freed = {
                .size_bytes = event.size_bytes,
                .file = source_file(event.source),
                .line = event.source.line,
            };
        }
    }

    void MemorySystem::apply_reset(const AllocatorResetEvent& event) {
        MemorySystemInfo* info = system_info(*this);
        if (info == nullptr || event.allocator_id >= info->allocators.size())
            return;

        AllocationStats& stats = info->allocators[event.allocator_id];
        update_statistics(stats, event.statistics);
        stats.allocation_log.clear();
    }

    cstr MemorySystem::allocator_name(AllocatorId allocator_id) const noexcept {
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

        const auto* info = static_cast<const MemorySystemInfo*>(m_data);
        if (info == nullptr || allocator_id >= info->allocators.size())
            return "Invalid";
        return info->allocators[allocator_id].name.c_str();
    }

    void MemorySystem::log_report(bool detailed) {
        MemorySystem& instance = get();
        MemorySystemInfo* info = system_info(instance);
        if (info == nullptr)
            return;

        instance.log_title("nk::MemorySystem Report");
        for (const AllocationStats& stats : info->allocators) {
            if (stats.name.empty())
                continue;

            instance.log_info(std::format(
                "[Allocator: {}] ({}){}",
                stats.name,
                stats.allocator,
                stats.registered ? "" : " [detached]"));

            if (stats.type != MemoryType::Native) {
                instance.log_info(std::format(
                    "  - Initialized at: {}:{} with reserved size {}",
                    stats.init.file,
                    stats.init.line,
                    memory_in_bytes(stats.init.size_bytes)));
            }

            instance.log_info(std::format(
                "  - Usage: {}, peak {}, {} active allocation(s)",
                stats.type == MemoryType::Native
                    ? memory_in_bytes(stats.used_bytes)
                    : memory_in_bytes(stats.reserved_bytes, stats.used_bytes),
                memory_in_bytes(stats.peak_used_bytes),
                stats.active_allocations));

            u64 leak_count = 0;
            u64 leaked_bytes = 0;
            for (const auto& [address, allocation] : stats.allocation_log) {
                if (allocation.freed.size_bytes == 0) {
                    ++leak_count;
                    leaked_bytes += allocation.allocated.size_bytes;
                    if (detailed) {
                        instance.log_error(std::format(
                            "  [LEAK] {} allocated at {}:{} was never freed. Address: {}",
                            memory_in_bytes(allocation.allocated.size_bytes),
                            allocation.allocated.file,
                            allocation.allocated.line,
                            address));
                    }
                } else if (allocation.freed.size_bytes != allocation.allocated.size_bytes) {
                    instance.log_warn(std::format(
                        "  [WARN] Address {} allocated {}, but freed {}.",
                        address,
                        memory_in_bytes(allocation.allocated.size_bytes),
                        memory_in_bytes(allocation.freed.size_bytes)));
                } else if (detailed) {
                    instance.log_trace(std::format(
                        "  [OK] {} at Address: {}",
                        memory_in_bytes(allocation.allocated.size_bytes),
                        address));
                }
            }

            if (leak_count == 0) {
                instance.log_info("  - Leaks: 0 leak(s) found.");
            } else {
                instance.log_error(std::format(
                    "  - Leaks: {} leak(s) found, totalling {}.",
                    leak_count,
                    memory_in_bytes(leaked_bytes)));
            }
        }

        if (instance.m_dropped_event_count != 0 || instance.m_reentrant_event_count != 0) {
            instance.log_warn(std::format(
                "Tracking incomplete: {} dropped journal event(s), {} reentrant event(s).",
                instance.m_dropped_event_count,
                instance.m_reentrant_event_count));
        }
        instance.log_title("End of nk::MemorySystem Report");
    }

    void MemorySystem::log_report_intermediate() {
        MemorySystem& instance = get();
        MemorySystemInfo* info = system_info(instance);
        if (info == nullptr)
            return;

        instance.log_title("nk::MemorySystem Usage Report");
        for (const AllocationStats& stats : info->allocators) {
            if (stats.name.empty())
                continue;
            instance.log_info(std::format(
                "[Allocator: {}] ({}) - {}, peak {}, {} active",
                stats.name,
                stats.allocator,
                stats.type == MemoryType::Native
                    ? memory_in_bytes(stats.used_bytes)
                    : memory_in_bytes(stats.reserved_bytes, stats.used_bytes),
                memory_in_bytes(stats.peak_used_bytes),
                stats.active_allocations));
        }
        instance.log_title("End of nk::MemorySystem Usage Report");
    }

    void MemorySystem::log(cstr color, cstr msg, std::size_t msg_size) {
        os::write(color, std::char_traits<char>::length(color));
        os::write(msg, msg_size);
        os::write("\033[0m\n", 5);
        os::flush();
    }
}

namespace nk {
    void memory_system_extended_memory_type(
        const std::function<MemoryType::Value()>& max_memory_type,
        const std::function<cstr(MemoryType::Value)>& memory_type_to_cstr) {
        MemoryType::Internal::max = max_memory_type;
        MemoryType::Internal::extended_to_cstr = memory_type_to_cstr;
    }
}
