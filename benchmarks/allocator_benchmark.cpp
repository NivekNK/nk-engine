#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "memory/free_list_allocator.h"
#include "memory/malloc_allocator.h"

namespace {
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t sample_count = 21;
    constexpr nk::u64 operation_count = 200'000;

    struct Distribution {
        nk::u64 p50;
        nk::u64 p95;
        nk::u64 p99;
        nk::u64 checksum;
    };

    template <typename Sample>
    Distribution measure(Sample&& sample_operation) {
        std::array<nk::u64, sample_count> samples{};
        nk::u64 checksum = 0;
        for (std::size_t sample = 0; sample < sample_count; ++sample) {
            nk::u64 sample_checksum = 0;
            samples[sample] = sample_operation(
                static_cast<nk::u64>(sample),
                sample_checksum);
            checksum ^= sample_checksum + static_cast<nk::u64>(sample);
        }

        std::sort(samples.begin(), samples.end());
        return {
            .p50 = samples[(sample_count * 50) / 100],
            .p95 = samples[(sample_count * 95) / 100],
            .p99 = samples[(sample_count * 99) / 100],
            .checksum = checksum,
        };
    }

    nk::u64 run_sample(
        nk::mem::Allocator& allocator,
        const nk::u64 size_bytes,
        const nk::u64 alignment,
        const nk::u64 sample,
        nk::u64& checksum) {
        const auto start = Clock::now();
        for (nk::u64 index = 0; index < operation_count; ++index) {
            auto* data = static_cast<nk::u8*>(
                allocator._allocate_raw(size_bytes, alignment));
            if (data == nullptr)
                std::abort();

            data[0] = static_cast<nk::u8>(index + sample);
            data[size_bytes - 1] = static_cast<nk::u8>(index ^ sample);
            checksum += data[0];
            checksum ^= data[size_bytes - 1];
            if (!allocator._free_raw(data, size_bytes))
                std::abort();
        }
        const auto end = Clock::now();

        if (allocator.get_active_allocation_count() != 0 ||
            allocator.get_used_bytes() != 0) {
            std::abort();
        }
        return static_cast<nk::u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count());
    }

    Distribution benchmark_malloc(
        const nk::u64 size_bytes,
        const nk::u64 alignment) {
        return measure([=](
                           const nk::u64 sample,
                           nk::u64& checksum) {
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            return run_sample(
                allocator,
                size_bytes,
                alignment,
                sample,
                checksum);
        });
    }

    Distribution benchmark_free_list(
        const nk::u64 size_bytes,
        const nk::u64 alignment) {
        return measure([=](
                           const nk::u64 sample,
                           nk::u64& checksum) {
            nk::mem::MallocAllocator parent{nk::mem::untracked};
            nk::mem::FreeListAllocator allocator{
                nk::mem::untracked,
                parent,
                parent,
                MiB(1),
                4};
            if (!allocator.is_initialized())
                std::abort();
            return run_sample(
                allocator,
                size_bytes,
                alignment,
                sample,
                checksum);
        });
    }

    void print(
        const nk::cstr allocator,
        const nk::u64 size_bytes,
        const nk::u64 alignment,
        const Distribution result) {
        std::printf(
            "allocator=%s size=%llu alignment=%llu operations=%llu p50_ns=%llu p95_ns=%llu p99_ns=%llu p50_ns_per_op=%.3f checksum=%llu\n",
            allocator,
            static_cast<unsigned long long>(size_bytes),
            static_cast<unsigned long long>(alignment),
            static_cast<unsigned long long>(operation_count),
            static_cast<unsigned long long>(result.p50),
            static_cast<unsigned long long>(result.p95),
            static_cast<unsigned long long>(result.p99),
            static_cast<double>(result.p50) /
                static_cast<double>(operation_count),
            static_cast<unsigned long long>(result.checksum));
    }
}

int main() {
    std::printf(
        "allocator_benchmark_version=1 build_type=%s samples=%zu\n",
        NK_BENCHMARK_BUILD_TYPE,
        sample_count);

    constexpr std::array<nk::u64, 3> sizes{16, 256, 4096};
    constexpr std::array<nk::u64, 3> alignments{16, 64, 256};
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        print(
            "malloc",
            sizes[index],
            alignments[index],
            benchmark_malloc(sizes[index], alignments[index]));
        print(
            "free_list",
            sizes[index],
            alignments[index],
            benchmark_free_list(sizes[index], alignments[index]));
    }
    return 0;
}
