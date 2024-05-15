#include "nkpch.h"

#include "system/logging_system.h"
#include "system/memory_system.h"

#include "memory/allocator.h"

namespace nk {
    class DebugMallocAllocator : public Allocator {
    public:
        DebugMallocAllocator() : Allocator() {}

        virtual ~DebugMallocAllocator() override {}

        DebugMallocAllocator(DebugMallocAllocator&& other)
            : Allocator{std::move(other)} {}

        DebugMallocAllocator& operator=(DebugMallocAllocator&& right) {
            Allocator::operator=(std::move(right));
            return *this;
        }

        DebugMallocAllocator(const DebugMallocAllocator&) = delete;
        DebugMallocAllocator& operator=(DebugMallocAllocator&) = delete;
        virtual cstr to_cstr() const override { return "DebugMallocAllocator"; }

        virtual str name() const override { return "ignore"; }

        void allocator_init_impl() {
            m_size_bytes = 0;
            m_start = nullptr;
        }

        virtual void* allocate_raw_impl(const u64 size_bytes, [[maybe_unused]] const u64 alignment) override {
            m_allocation_count++;
            m_size_bytes += size_bytes;
            m_used_bytes += size_bytes;
            m_start = std::calloc(1, size_bytes);
            return m_start;
        }

        virtual void free_raw_impl(void* const ptr, const u64 size_bytes) override {
            m_allocation_count--;
            m_size_bytes -= size_bytes;
            m_used_bytes -= size_bytes;
            std::free(ptr);
            m_start = nullptr;
        }
    };

    MemorySystem* MemorySystem::s_instance = nullptr;

    void MemorySystem::init() {
        AssertMsg(s_instance == nullptr, "MemorySystem is already initialized!");
        s_instance = new MemorySystem();
        s_instance->m_allocations.init(s_instance->m_allocator, 12);
    }

    void MemorySystem::shutdown() {
        Assert(s_instance != nullptr);

        for (MemorySystemStats& value : s_instance->m_allocations) {
            value.allocation_log.free();
        }

        s_instance->m_allocations.free();
        delete s_instance->m_allocator;
        std::free(s_instance);
    }

    void MemorySystem::expanded_memory_type(u64 max_memory_type, const std::function<cstr(MemoryTypeValue)>& memory_type_to_cstr) {
        AssertMsg(s_instance != nullptr, "MemorySystem not initialized!");

        m_max_memory_type = max_memory_type;
        m_memory_type_to_cstr = std::move(memory_type_to_cstr);
    }

    void MemorySystem::insert(Allocator& allocator) {
        if (allocator.name() == "ignore")
            return;

        AssertMsg(s_instance != nullptr, "MemorySystem not initialized!");

        allocator.set_index(m_allocations.length());

        MemorySystemStats& inserted_stats = m_allocations.insert_use(allocator.index());
        inserted_stats.name = allocator.cname();
        inserted_stats.allocator = allocator.to_cstr();
        inserted_stats.type = allocator.memory_type();
        inserted_stats.size_bytes = allocator.size();
        inserted_stats.used_bytes = allocator.used();
        inserted_stats.allocation_count = allocator.allocation_count();
        inserted_stats.allocation_log.init(m_allocator, 5);
    }

    void MemorySystem::update(const Allocator& allocator, const u64 size_bytes, str file, const u32 line, AllocationType type) {
        if (allocator.name() == "ignore")
            return;

        AssertMsg(s_instance != nullptr, "MemorySystem not initialized!");

        if (auto value = m_allocations[allocator.index()]) {
            auto& value_ref = value->get();
            value_ref.name = allocator.name();
            value_ref.type = allocator.memory_type();
            value_ref.size_bytes = allocator.size();
            value_ref.used_bytes = allocator.used();
            value_ref.allocation_count = allocator.allocation_count();

            const u64 pos = file.find("nk-engine");
            if (pos != std::string::npos) {
                file.erase(0, pos + 10);
            }

            value_ref.allocation_log.push({
                .size_bytes = size_bytes,
                .file = file,
                .line = line,
                .type = type,
            });
        }
    }

    str memory_in_bytes(u64 memory) {
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

    str memory_in_bytes(u64 allocated, u64 used) {
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

    void MemorySystem::log_report(bool detailed) {
        AssertMsg(s_instance != nullptr, "MemorySystem not initialized!");

        // u64 total_allocated = 0;
        //
        // u64 type_allocated[m_max_memory_type];
        // memset(type_allocated, 0, sizeof type_allocated);
        //
        // u64 type_used[m_max_memory_type];
        // memset(type_used, 0, sizeof type_used);

        str details;
        bool there_are_details = false;

        StyledLog(4, -1, 1, "\n\nGeneral Memory Usage:\n");
        for (const MemorySystemStats& value : m_allocations) {
            StyledLog(7, -1, -1,
                      "Name: {}\n"
                      "Allocator: {}\n"
                      "Type: {}\n"
                      "Memory: {}\n"
                      "Allocation Count: {}\n",
                      value.name, value.allocator,
                      value.type > MemoryType::max() ?
                            m_memory_type_to_cstr(value.type) : MemoryType::to_cstr(value.type),
                      memory_in_bytes(value.size_bytes, value.used_bytes),
                      value.allocation_count);

            if (detailed) {
                if (value.allocation_log.empty())
                    continue;

                details += std::format("- {}:\n", value.name);
                for (auto& log : value.allocation_log) {
                    str type;
                    switch (log.type) {
                        case AllocationType::Init:
                            type = "Init";
                            break;
                        case AllocationType::Allocate:
                            type = "Allocate";
                            break;
                        case AllocationType::Free:
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
            StyledLog(4, -1, 1, "\n\nDetailed Memory Usage:\n");
            StyledLog(7, -1, -1, details.c_str());
        }
    }

    MemorySystem::MemorySystem() {
        auto allocator = new DebugMallocAllocator();
        allocator->allocator_init_impl();
        m_allocator = allocator;
    }
}
