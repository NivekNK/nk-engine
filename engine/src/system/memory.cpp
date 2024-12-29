#include "nkpch.h"

#include "system/memory.h"

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace nk {
// TODO: Move to a general function or platform specific initialization
#if defined(NK_PLATFORM_WINDOWS)
    void activate_virtual_terminal_impl() {
        HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode;
        GetConsoleMode(handleOut, &consoleMode);
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(handleOut, consoleMode);
    }
    #define activate_virtual_terminal() activate_virtual_terminal_impl()
#else
    #define activate_virtual_terminal()
#endif

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

    MemorySystem& MemorySystem::init() {
        auto& instance = get();

        activate_virtual_terminal();

        auto ansicolor_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        ansicolor_sink->set_color(spdlog::level::debug, "\033[38;2;31;151;199m");

        instance.m_logger = std::make_shared<spdlog::logger>("TEST", std::move(ansicolor_sink));
        spdlog::details::registry::instance().initialize_logger(instance.m_logger);

        instance.m_logger->set_pattern("%^%v%$");
        instance.m_logger->set_level(spdlog::level::trace);

        instance.m_logger->trace("MemorySystem initialized.");

        return instance;
    }

    void MemorySystem::shutdown() {
        auto& instance = get();
        instance.m_allocations.clear();

        instance.m_logger->trace("MemorySystem shutdown.");
    }

    void MemorySystem::init_allocator(
        Allocator* allocator, cstr file, u32 line, cstr name, MemoryType::Value type) {
        auto& instance = get();

        MemorySystemInternal::AllocationStats stats{
            .name = name,
            .allocator = allocator->to_cstr(),
            .type = type,
            .size_bytes = allocator->size_bytes(),
            .used_bytes = allocator->used_bytes(),
            .allocation_count = allocator->allocation_count(),
            .allocator_log = {{
                .size_bytes = allocator->size_bytes(),
                .file = file,
                .line = line,
                .type = MemorySystemInternal::AllocationType::Init,
            }},
        };
        instance.m_allocations.push_back(stats);

        allocator->m_key = instance.m_allocations.size() - 1;
    }

    void MemorySystem::update_allocator(
        Allocator* allocator, cstr file, u32 line, u64 size_bytes,
        MemorySystemInternal::AllocationType::Value allocation_type) {
        auto& instance = get();

        if (allocator->m_key >= instance.m_allocations.size()) {
            return;
        }

        auto& value = instance.m_allocations.at(allocator->m_key);
        value.size_bytes = allocator->size_bytes();
        value.used_bytes = allocator->used_bytes();
        value.allocation_count = allocator->allocation_count();
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

        std::string details;
        bool there_are_details = false;

        instance.m_logger->debug("\n\nGeneral Memory Usage:\n");
        for (const MemorySystemInternal::AllocationStats& stats : instance.m_allocations) {
            instance.m_logger->trace(
                "Name: {}\n"
                "Allocator: {}\n"
                "Type: {}\n"
                "Memory: {}\n"
                "Allocation Count: {}\n",
                stats.name,
                stats.allocator,
                MemoryType::to_cstr(stats.type),
                memory_in_bytes(stats.size_bytes, stats.used_bytes),
                stats.allocation_count
            );

            if (detailed) {
                if (stats.allocator_log.empty()) continue;

                details += std::format("- {}:\n", stats.name);
                for (auto& log : stats.allocator_log) {
                    std::string type;
                    switch (log.type) {
                        case MemorySystemInternal::AllocationType::Init:
                            type = "Init";
                            type_allocated[stats.type] += log.size_bytes;
                            total_allocated += log.size_bytes;
                            break;
                        case MemorySystemInternal::AllocationType::Allocate:
                            type_allocated[stats.type] += log.size_bytes;
                            total_allocated += log.size_bytes;
                            type = "Allocate";
                            break;
                        case MemorySystemInternal::AllocationType::Free:
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
            details += std::format("{:<15} {:<15} {:<15}\n", "Types", "Allocated", "Freed");

            for (u32 i = 0; i < MemoryType::max(); i++) {
                details += std::format("{:<15} {:<15} {:<15}\n", MemoryType::to_cstr(i), memory_in_bytes(type_allocated[i]), memory_in_bytes(type_freed[i]));
            }

            details += std::format("\n\nTotal Allocated: {}\n", memory_in_bytes(total_allocated));
            details += std::format("Total Freed: {}\n", memory_in_bytes(total_freed));
            details += std::format("\nCurrent Memory Usage: {}\n", memory_in_bytes(total_allocated - total_freed));

            instance.m_logger->debug("\n\nDetailed Memory Usage:\n");
            instance.m_logger->trace(details);
        }
    }

    void memory_system_extended_memory_type(const std::function<nk::MemoryType::Value()>& max_memory_type, const std::function<cstr(MemoryType::Value)>& memory_type_to_cstr) {
        MemoryType::Internal::max = max_memory_type;
        MemoryType::Internal::extended_to_cstr = memory_type_to_cstr;
    }
}
