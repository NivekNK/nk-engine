#include "nkpch.h"

#include "systems/memory_system.h"

#include "memory/allocator.h"

#include <vector>
#include <memory>

namespace nk::mem {
    namespace Type {
        u8 to_value(const AllocationType type) {
            return static_cast<u8>(type);
        }

        cstr to_cstr(const AllocationType type) {
            static constexpr cstr types[] = {"Init", "Allocate", "Free"};
            return types[Type::to_value(type)];
        }
    }

    struct AllocatorInfo {
        u64 size_bytes;
        std::string file;
        u32 line;
        AllocationType type;
    };

    struct AllocationStats {
        std::string name;
        std::string allocator;
        MemoryType::Value type;

        u64 size_bytes;
        u64 used_bytes;
        u64 allocation_count;

        std::vector<AllocatorInfo> allocator_log;
    };

    struct MemorySystemInfo {
        std::vector<AllocationStats> allocations;
    };

    std::string memory_in_bytes(u64 memory) {
        if (memory >= GiB()) {
            return std::format("{:.2f} GiB", memory / static_cast<f32>(GiB()));
        } else if (memory >= MiB()) {
            return std::format("{:.2f} MiB", memory / static_cast<f32>(MiB()));
        } else if (memory >= KiB()) {
            return std::format("{:.2f} KiB", memory / static_cast<f32>(KiB()));
        } else {
            return std::format("{:.2f} B", static_cast<f32>(memory));
        }
    }

    std::string memory_in_bytes(u64 allocated, u64 used) {
        if (used >= GiB()) {
            return std::format("{:.2f}/{:.2f} GiB",
                               allocated / static_cast<f32>(GiB()), used / static_cast<f32>(GiB()));
        } else if (used >= MiB()) {
            return std::format("{:.2f}/{:.2f} MiB",
                               allocated / static_cast<f32>(MiB()), used / static_cast<f32>(MiB()));
        } else if (used >= KiB()) {
            return std::format("{:.2f}/{:.2f} KiB",
                               allocated / static_cast<f32>(KiB()), used / static_cast<f32>(KiB()));
        } else {
            return std::format("{:.2f}/{:.2f} B",
                               static_cast<f32>(allocated), static_cast<f32>(used));
        }
    }

    void* get_memory_system_data() {
        return MemorySystem::get().data;
    }

    MemorySystemInfo& get_memory_system_info() {
        auto memory_system_info = static_cast<MemorySystemInfo*>(get_memory_system_data());
        return *memory_system_info;
    }

    MemorySystem& MemorySystem::init() {
        auto& instance = get();

        auto memory_system_info = new MemorySystemInfo();

        AllocationStats stats{
            .name = "Native Allocation",
            .allocator = "Native",
            .type = MemoryType::Native,
            .size_bytes = 0,
            .used_bytes = 0,
            .allocation_count = 0,
            .allocator_log = {},
        };
        memory_system_info->allocations.push_back(stats);
        instance.data = memory_system_info;

        instance.log_title("nk::MemorySystem initialized.");

        return instance;
    }

    void MemorySystem::shutdown() {
        auto& instance = get();

        auto memory_system_info = static_cast<MemorySystemInfo*>(instance.data);
        memory_system_info->allocations.clear();
        delete memory_system_info;

        instance.log_title("nk::MemorySystem shutdown.");
    }

    void MemorySystem::init_allocator(mem::Allocator* allocator, cstr file,
                                      u32 line, cstr name, MemoryType::Value type) {
        auto& memory_system_info = get_memory_system_info();

        AllocationStats stats{
            .name = name,
            .allocator = allocator->to_cstr(),
            .type = type,
            .size_bytes = allocator->get_size_bytes(),
            .used_bytes = allocator->get_used_bytes(),
            .allocation_count = allocator->get_allocation_count(),
            .allocator_log = {{
                .size_bytes = allocator->get_size_bytes(),
                .file = file,
                .line = line,
                .type = AllocationType::Init,
            }},
        };
        memory_system_info.allocations.push_back(stats);

        allocator->key = memory_system_info.allocations.size() - 1;
    }

    void MemorySystem::update_allocator(mem::Allocator* allocator, cstr file,
                                        u32 line, u64 size_bytes, AllocationType allocation_type) {
        auto& memory_system_info = get_memory_system_info();

        if (allocator->key >= memory_system_info.allocations.size())
            return;

        auto& value = memory_system_info.allocations.at(allocator->key);
        value.size_bytes = allocator->get_size_bytes();
        value.used_bytes = allocator->get_used_bytes();
        value.allocation_count = allocator->get_allocation_count();
        value.allocator_log.push_back({
            .size_bytes = size_bytes,
            .file = file,
            .line = line,
            .type = allocation_type,
        });
    }

    void MemorySystem::native_allocation(cstr file, u32 line, u64 size_bytes,
                                         AllocationType allocation_type) {
        auto& memory_system_info = get_memory_system_info();
        auto& value = memory_system_info.allocations.at(0);

        if (allocation_type == AllocationType::Allocate) {
            value.size_bytes += size_bytes;
            value.used_bytes += size_bytes;
            value.allocation_count++;
        } else {
            value.size_bytes -= size_bytes;
            value.used_bytes -= size_bytes;
            value.allocation_count--;
        }

        value.allocator_log.push_back({
            .size_bytes = size_bytes,
            .file = file,
            .line = line,
            .type = allocation_type,
        });
    }

    void MemorySystem::log_report(bool detailed) {
        auto& instance = get();

        u64 total_allocated = 0;
        u64 total_freed = 0;

        u64 type_allocated[MemoryType::max()];
        std::memset(type_allocated, 0, sizeof(type_allocated));

        u64 type_freed[MemoryType::max()];
        std::memset(type_freed, 0, sizeof(type_freed));

        u64 type_count[MemoryType::max()];
        std::memset(type_count, 0, sizeof(type_count));

        std::string details;
        bool there_are_details = false;

        auto& memory_system_info = get_memory_system_info();

        instance.log_title("\n\nGeneral Memory Usage:\n");
        for (const AllocationStats& stats : memory_system_info.allocations) {
            instance.log_text(
                "Name: {}\n"
                "Allocator: {}\n"
                "Type: {}\n"
                "Memory: {}\n"
                "Allocation Count: {}\n",
                stats.name,
                stats.allocator,
                MemoryType::to_cstr(stats.type),
                memory_in_bytes(stats.size_bytes, stats.used_bytes),
                stats.allocation_count);

            if (detailed) {
                if (stats.allocator_log.empty())
                    continue;

                details += std::format("- {}:\n", stats.name);
                for (auto& log : stats.allocator_log) {
                    std::string type;
                    switch (log.type) {
                        case AllocationType::Init:
                            type = "Init";
                            type_allocated[stats.type] += log.size_bytes;
                            total_allocated += log.size_bytes;
                            break;
                        case AllocationType::Allocate:
                            type_count[stats.type] += 1;
                            type_allocated[stats.type] += log.size_bytes;
                            total_allocated += log.size_bytes;
                            type = "Allocate";
                            break;
                        case AllocationType::Free:
                            type_freed[stats.type] += log.size_bytes;
                            total_freed += log.size_bytes;
                            type = "Free";
                            break;
                    }
                    details += std::format("    {}: {} ({}:{})\n",
                                           type,
                                           memory_in_bytes(log.size_bytes),
                                           log.file, log.line);
                }
                details += "\n";

                there_are_details = true;
            }
        }

        if (detailed && there_are_details) {
            details += "Memory Types:\n\n";
            details += std::format("{:<15} {:<15} {:<15} {:<15}\n", "Types", "Allocated", "Freed", "Count");

            for (u32 i = 0; i < MemoryType::max(); i++) {
                details += std::format("{:<15} {:<15} {:<15} {:<15}\n",
                    MemoryType::to_cstr(i), memory_in_bytes(type_allocated[i]), memory_in_bytes(type_freed[i]), type_count[i]);
            }

            details += std::format("\n\nTotal Allocated: {}\n", memory_in_bytes(total_allocated));
            details += std::format("Total Freed: {}\n", memory_in_bytes(total_freed));
            details += std::format("\nCurrent Memory Usage: {}\n", memory_in_bytes(total_allocated - total_freed));

            instance.log_title("\n\nDetailed Memory Usage:\n");
            instance.log_text(details);
        }
    }

    std::string_view MemorySystem::get_allocator_name(u32 key) {
        auto& memory_system_info = get_memory_system_info();

        if (key >= memory_system_info.allocations.size())
            return;

        auto& value = memory_system_info.allocations.at(key);
        return value.name;
    }

    void MemorySystem::log(cstr color, cstr msg, size_t msg_size) {
        // TODO: Add general mutex for logging std::lock_guard<std::mutex> lock(mutex);
        os::write(color, 19);
        os::write(msg, msg_size);
        os::write("\n\033[0m", 5);
        os::flush();
    }

    void memory_system_extended_memory_type(const std::function<nk::MemoryType::Value()>& max_memory_type, const std::function<cstr(MemoryType::Value)>& memory_type_to_cstr) {
        MemoryType::Internal::max = max_memory_type;
        MemoryType::Internal::extended_to_cstr = memory_type_to_cstr;
    }
}
