#pragma once

#include "memory/allocator.h"

namespace nk {
    class LinearAllocator : public Allocator {
    public:
        LinearAllocator();

        virtual ~LinearAllocator() override {}

        LinearAllocator(LinearAllocator&& other)
            : Allocator{std::move(other)},
              m_own_memory{other.m_own_memory} {
            other.m_own_memory = false;
        }

        LinearAllocator& operator=(LinearAllocator&& right) {
            Allocator::operator=(std::move(right));
            m_own_memory = right.m_own_memory;
            right.m_own_memory = false;
            return *this;
        }

        NK_DEFINE_ALLOCATOR_INIT(LinearAllocator, total_size, memory, (const u64 total_size, void* memory = nullptr), (
            AssertMsg(total_size > 0, "Trying to init nk::LinearAllocator with no memory!");
            if (memory == nullptr) {
                memory = std::calloc(1, total_size);
                m_own_memory = true;
            } else {
                m_own_memory = false;
            }
        ))

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) override;
        virtual void free_raw_impl(void* const ptr, const u64 size_bytes) override {}

        void free_all();

    private:
        bool m_own_memory;
    };
}
