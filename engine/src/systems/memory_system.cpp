#include "nkpch.h"

#include "systems/memory_system.h"

#include "memory/allocator.h"

#include <unordered_map>
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

    struct AllocationInfo {
        u64 size_bytes;
        std::string file;
        u32 line;
    };

    struct FreeInfo {
        u64 size_bytes;
        std::string file;
        u32 line;
    };

    struct AllocatorInfo {
        AllocationInfo allocated;
        FreeInfo freed;
    };

    struct AllocationStats {
        std::string name;
        std::string allocator;
        MemoryType::Value type;

        u64 size_bytes;
        u64 used_bytes;
        u64 allocation_count;

        AllocationInfo init;
        std::unordered_map<void*, AllocatorInfo> allocator_log;
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
        return MemorySystem::get().m_data;
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
            .init = {
                .size_bytes = 0,
                .file = __FILE__,
                .line = __LINE__,
            },
            .allocator_log = {},
        };
        memory_system_info->allocations.push_back(stats);
        instance.m_data = memory_system_info;

        instance.log_title("nk::MemorySystem initialized.");

        return instance;
    }

    void MemorySystem::shutdown() {
        auto& instance = get();

        auto memory_system_info = static_cast<MemorySystemInfo*>(instance.m_data);
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
            .init = {
                .size_bytes = allocator->get_size_bytes(),
                .file = file,
                .line = line,
            },
            .allocator_log = {},
        };
        memory_system_info.allocations.push_back(stats);

        allocator->m_key = memory_system_info.allocations.size() - 1;
    }

    void MemorySystem::update_allocator(mem::Allocator* allocator, cstr file,
                                        u32 line, void* data, u64 size_bytes,
                                        AllocationType allocation_type) {
        auto& memory_system_info = get_memory_system_info();

        if (allocator->m_key >= memory_system_info.allocations.size())
            return;

        auto& value = memory_system_info.allocations.at(allocator->m_key);
        value.size_bytes = allocator->get_size_bytes();
        value.used_bytes = allocator->get_used_bytes();
        value.allocation_count = allocator->get_allocation_count();

        if (allocation_type == AllocationType::Allocate) {
            auto [it, emplaced] = value.allocator_log.try_emplace(data, AllocatorInfo {
                .allocated = {
                    .size_bytes = size_bytes,
                    .file = file,
                    .line = line,
                },
                .freed = {
                    .size_bytes = 0,
                },
            });

            if (emplaced)
                return;

            it->second.allocated = AllocationInfo {
                .size_bytes = size_bytes,
                .file = file,
                .line = line,
            };
        } else if (allocation_type == AllocationType::Free) {
            auto it = value.allocator_log.find(data);
            if (it != value.allocator_log.end()) {
                it->second.freed = FreeInfo {
                    .size_bytes = size_bytes,
                    .file = file,
                    .line = line,
                };
            }
        }
    }

    void MemorySystem::native_allocation(cstr file, u32 line, void* data, u64 size_bytes,
                                         AllocationType allocation_type) {
        auto& memory_system_info = get_memory_system_info();
        auto& value = memory_system_info.allocations.at(0);

        if (allocation_type == AllocationType::Allocate) {
            value.size_bytes += size_bytes;
            value.used_bytes += size_bytes;
            value.allocation_count++;

            auto [it, emplaced] = value.allocator_log.try_emplace(data, AllocatorInfo {
                .allocated = {
                    .size_bytes = size_bytes,
                    .file = file,
                    .line = line,
                },
                .freed = {
                    .size_bytes = 0,
                },
            });

            if (emplaced)
                return;

            it->second.allocated = AllocationInfo {
                .size_bytes = size_bytes,
                .file = file,
                .line = line,
            };
        } else if (allocation_type == AllocationType::Free) {
            value.size_bytes -= size_bytes;
            value.used_bytes -= size_bytes;
            value.allocation_count--;

            auto it = value.allocator_log.find(data);
            if (it != value.allocator_log.end()) {
                it->second.freed = FreeInfo {
                    .size_bytes = size_bytes,
                    .file = file,
                    .line = line,
                };
            }
        }
    }

    void MemorySystem::log_report(bool detailed) {
        auto& instance = get();
        auto& memory_system_info = get_memory_system_info();

        instance.log_title("nk::MemorySystem Report");

        for (const auto& stats : memory_system_info.allocations) {
            std::string header = std::format("[Allocator: {}] ({})", stats.name, stats.allocator);
            instance.log_info(header.c_str());

            if (stats.type != MemoryType::Native) {
                std::string init_info = std::format("  - Initialized at: {}:{} with size {}",
                                                    stats.init.file,
                                                    stats.init.line,
                                                    memory_in_bytes(stats.init.size_bytes));
                instance.log_info(init_info.c_str());
            }

            std::string usage_info;
            if (stats.type == MemoryType::Native) {
                usage_info = std::format("  - Usage: {}, {} allocations",
                                        memory_in_bytes(stats.used_bytes),
                                        stats.allocation_count);
            } else {
                usage_info = std::format("  - Usage: {}, {} allocations",
                                        memory_in_bytes(stats.size_bytes, stats.used_bytes),
                                        stats.allocation_count);
            }
            instance.log_info(usage_info.c_str());

            u64 leak_count = 0;
            u64 leaked_bytes = 0;

            for (const auto& [address, info] : stats.allocator_log) {
                if (info.freed.size_bytes == 0) {
                    leak_count++;
                    leaked_bytes += info.allocated.size_bytes;
                    if (detailed) {
                        std::string leak_msg = std::format("  [LEAK] {} allocated at {}:{} was never freed. Address: {}",
                                                        memory_in_bytes(info.allocated.size_bytes),
                                                        info.allocated.file,
                                                        info.allocated.line,
                                                        address);
                        instance.log_error(leak_msg.c_str());
                    }
                } else if (info.freed.size_bytes != info.allocated.size_bytes) {
                    std::string partial_free_msg = std::format("  [WARN] Mismatch at Address: {}. Allocated {}, but freed {}.",
                                                            address,
                                                            memory_in_bytes(info.allocated.size_bytes),
                                                            memory_in_bytes(info.freed.size_bytes));
                    instance.log_warn(partial_free_msg.c_str());
                    if (detailed) {
                        std::string alloc_details = std::format("    - Allocated at: {}:{}", info.allocated.file, info.allocated.line);
                        instance.log_info(alloc_details.c_str());
                        std::string free_details = std::format("    - Freed at: {}:{}", info.freed.file, info.freed.line);
                        instance.log_info(free_details.c_str());
                    }
                } else {
                    if (detailed) {
                        std::string ok_msg = std::format("  [OK] {} at Address: {}",
                                                        memory_in_bytes(info.allocated.size_bytes),
                                                        address);
                        instance.log_trace(ok_msg.c_str());
                        std::string alloc_details = std::format("    - Allocated at: {}:{}", info.allocated.file, info.allocated.line);
                        instance.log_trace(alloc_details.c_str());
                        std::string free_details = std::format("    - Freed at: {}:{}", info.freed.file, info.freed.line);
                        instance.log_trace(free_details.c_str());
                    }
                }
            }

            if (leak_count > 0) {
                std::string leak_summary = std::format("  - Leaks: {} leak(s) found, totalling {}.",
                                                    leak_count,
                                                    memory_in_bytes(leaked_bytes));
                instance.log_error(leak_summary.c_str());
            } else {
                instance.log_info("  - Leaks: 0 leak(s) found.");
            }
        }
        instance.log_title("End of nk::MemorySystem Report");
    }

    std::string_view MemorySystem::get_allocator_name(Allocator* allocator) {
        const u32 key = allocator->m_key;
        auto& memory_system_info = get_memory_system_info();

        if (key >= memory_system_info.allocations.size())
            return "Invalid";

        auto& value = memory_system_info.allocations.at(key);
        return value.name;
    }

    void MemorySystem::log(cstr color, cstr msg, std::size_t msg_size) {
        // TODO: Add general mutex for logging std::lock_guard<std::mutex> lock(mutex);
        os::write(color, 19);
        os::write(msg, msg_size);
        os::write("\033[0m\n", 5);
        os::flush();
    }

    void memory_system_extended_memory_type(const std::function<nk::MemoryType::Value()>& max_memory_type, const std::function<cstr(MemoryType::Value)>& memory_type_to_cstr) {
        MemoryType::Internal::max = max_memory_type;
        MemoryType::Internal::extended_to_cstr = memory_type_to_cstr;
    }
}
