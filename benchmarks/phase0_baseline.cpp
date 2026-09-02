#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#include "collections/arr.h"
#include "collections/dyarr.h"
#include "core/assertion.h"
#include "memory/linear_allocator.h"
#include "memory/malloc_allocator.h"
#include "systems/logging_system.h"

namespace {
    struct GlobalAllocationCounters {
        std::uint64_t allocations;
        std::uint64_t allocated_bytes;
        std::uint64_t frees;
    };

    bool g_count_global_allocations = false;
    GlobalAllocationCounters g_global_allocation_counters{};

    void record_global_allocation(std::size_t size) noexcept {
        if (!g_count_global_allocations)
            return;

        ++g_global_allocation_counters.allocations;
        g_global_allocation_counters.allocated_bytes += size;
    }

    void record_global_free(void* data) noexcept {
        if (g_count_global_allocations && data != nullptr)
            ++g_global_allocation_counters.frees;
    }

    void* allocate_global(std::size_t size) {
        const std::size_t actual_size = size == 0 ? 1 : size;
        void* data = std::malloc(actual_size);
        if (data == nullptr)
            std::abort();

        record_global_allocation(actual_size);
        return data;
    }

    GlobalAllocationCounters count_global_allocations(void (*operation)()) {
        g_global_allocation_counters = {};
        g_count_global_allocations = true;
        operation();
        g_count_global_allocations = false;
        return g_global_allocation_counters;
    }

    using Clock = std::chrono::steady_clock;
    using Nanoseconds = std::chrono::nanoseconds;

    constexpr std::size_t sample_count = 7;
    constexpr nk::u64 malloc_operation_count = 200000;
    constexpr nk::u64 growth_element_count = 100000;
    constexpr nk::u64 edit_initial_element_count = 10000;
    constexpr nk::u64 edit_operation_count = 2000;

    struct Measurement {
        const char* name;
        nk::u64 operations;
        nk::u64 median_nanoseconds;
        nk::u64 checksum;
    };

    template <typename Operation>
    Measurement measure(const char* name, nk::u64 operations, Operation&& operation) {
        std::array<nk::u64, sample_count> samples{};
        nk::u64 checksum = 0;

        for (std::size_t sample = 0; sample < sample_count; ++sample) {
            nk::u64 sample_checksum = 0;
            samples[sample] = operation(sample_checksum);
            checksum ^= sample_checksum + static_cast<nk::u64>(sample);
        }

        std::sort(samples.begin(), samples.end());
        return {
            .name = name,
            .operations = operations,
            .median_nanoseconds = samples[sample_count / 2],
            .checksum = checksum,
        };
    }

    nk::u64 elapsed_nanoseconds(Clock::time_point start, Clock::time_point end) {
        return static_cast<nk::u64>(std::chrono::duration_cast<Nanoseconds>(end - start).count());
    }

    Measurement measure_malloc_allocate_free() {
        return measure("malloc.allocate_free_64B", malloc_operation_count, [](nk::u64& checksum) {
            nk::mem::MallocAllocator allocator;
            allocator.allocator_init_untracked(nk::mem::MallocAllocator);

            const auto start = Clock::now();
            for (nk::u64 index = 0; index < malloc_operation_count; ++index) {
                auto* block = static_cast<nk::u8*>(allocator.allocate_raw(64, alignof(std::max_align_t)));
                if (block == nullptr)
                    std::abort();

                block[0] = static_cast<nk::u8>(index);
                checksum += block[0];
                allocator.free_raw(block, 64);
            }
            const auto end = Clock::now();

            if (allocator.get_allocation_count() != 0 || allocator.get_used_bytes() != 0)
                std::abort();

            return elapsed_nanoseconds(start, end);
        });
    }

    Measurement measure_dyarr_growth() {
        return measure("dyarr.grow_push_u64", growth_element_count, [](nk::u64& checksum) {
            nk::mem::MallocAllocator allocator;
            allocator.allocator_init_untracked(nk::mem::MallocAllocator);
            nk::cl::dyarr<nk::u64> values;

            const auto start = Clock::now();
            values.dyarr_init(&allocator, 0);
            for (nk::u64 value = 0; value < growth_element_count; ++value)
                values.dyarr_push_copy(value);
            const auto end = Clock::now();

            checksum = values.length() ^ values.capacity() ^ values[values.length() - 1];
            values.dyarr_shutdown();
            return elapsed_nanoseconds(start, end);
        });
    }

    Measurement measure_dyarr_insert() {
        return measure("dyarr.insert_middle_u64", edit_operation_count, [](nk::u64& checksum) {
            nk::mem::MallocAllocator allocator;
            allocator.allocator_init_untracked(nk::mem::MallocAllocator);
            nk::cl::dyarr<nk::u64> values;
            values.dyarr_init(&allocator, edit_initial_element_count + edit_operation_count);

            for (nk::u64 value = 0; value < edit_initial_element_count; ++value)
                values.dyarr_push_copy(value);

            const auto start = Clock::now();
            for (nk::u64 value = 0; value < edit_operation_count; ++value)
                values.dyarr_insert_copy(values.length() / 2, value);
            const auto end = Clock::now();

            checksum = values.length() ^ values[values.length() / 2];
            values.dyarr_shutdown();
            return elapsed_nanoseconds(start, end);
        });
    }

    Measurement measure_dyarr_remove() {
        return measure("dyarr.remove_middle_u64", edit_operation_count, [](nk::u64& checksum) {
            nk::mem::MallocAllocator allocator;
            allocator.allocator_init_untracked(nk::mem::MallocAllocator);
            nk::cl::dyarr<nk::u64> values;
            values.dyarr_init(&allocator, edit_initial_element_count);

            for (nk::u64 value = 0; value < edit_initial_element_count; ++value)
                values.dyarr_push_copy(value);

            const auto start = Clock::now();
            for (nk::u64 index = 0; index < edit_operation_count; ++index) {
                auto removed = values.dyarr_remove(values.length() / 2);
                if (!removed.has_value())
                    std::abort();
                checksum ^= *removed + index;
            }
            const auto end = Clock::now();

            checksum ^= values.length();
            values.dyarr_shutdown();
            return elapsed_nanoseconds(start, end);
        });
    }

    nk::LoggingSystemConfig logging_config() {
        auto config = nk::LoggingSystem::get_default_config();
        config.show_file = false;
        config.show_time = false;
        config.file_output = false;
        return config;
    }

    void initialize_logging() {
        const auto config = logging_config();
        nk::LoggingSystem::init(config);
    }

    void write_plain_log() {
        nk::LoggingSystem::log(
            nk::LoggingLevel::Info,
            __FILE__,
            __LINE__,
            "Phase 0 plain logging allocation baseline with a message long enough to exceed common string SSO capacities.");
    }

    void write_formatted_log() {
        nk::LoggingSystem::log(
            nk::LoggingLevel::Info,
            __FILE__,
            __LINE__,
            "Phase 0 formatted logging allocation baseline: value={}, text={}",
            42,
            "a deliberately non-trivial formatted payload");
    }

    void report_formatted_assertion() {
#if NK_DEV_MODE <= NK_DEBUG
        nk::report_assert_failure(
            "phase0_expression",
            __FILE__,
            __LINE__,
            "formatted assertion value={}, text={}",
            42,
            "a deliberately non-trivial formatted payload");
#endif
    }

    void print_measurement(const Measurement& measurement) {
        const double nanoseconds_per_operation =
            static_cast<double>(measurement.median_nanoseconds) / static_cast<double>(measurement.operations);
        std::printf(
            "metric=%s operations=%llu median_ns=%llu ns_per_op=%.3f checksum=%llu\n",
            measurement.name,
            static_cast<unsigned long long>(measurement.operations),
            static_cast<unsigned long long>(measurement.median_nanoseconds),
            nanoseconds_per_operation,
            static_cast<unsigned long long>(measurement.checksum));
    }

    void print_allocation_measurement(const char* name, const GlobalAllocationCounters& counters) {
        std::printf(
            "allocation_site=%s allocations=%llu allocated_bytes=%llu frees=%llu\n",
            name,
            static_cast<unsigned long long>(counters.allocations),
            static_cast<unsigned long long>(counters.allocated_bytes),
            static_cast<unsigned long long>(counters.frees));
    }
}

void* operator new(std::size_t size) {
    return allocate_global(size);
}

void* operator new[](std::size_t size) {
    return allocate_global(size);
}

void operator delete(void* data) noexcept {
    record_global_free(data);
    std::free(data);
}

void operator delete[](void* data) noexcept {
    record_global_free(data);
    std::free(data);
}

void operator delete(void* data, std::size_t) noexcept {
    record_global_free(data);
    std::free(data);
}

void operator delete[](void* data, std::size_t) noexcept {
    record_global_free(data);
    std::free(data);
}

int main() {
#if defined(__clang__)
    constexpr const char* compiler = "clang " __clang_version__;
#elif defined(__GNUC__)
    constexpr const char* compiler = "gcc " __VERSION__;
#else
    constexpr const char* compiler = "unknown";
#endif

    std::printf("phase0_baseline_version=1 build_type=%s compiler=%s\n", NK_PHASE0_BUILD_TYPE, compiler);
    std::printf(
        "layout sizeof_allocator=%zu alignof_allocator=%zu sizeof_malloc_allocator=%zu sizeof_linear_allocator=%zu sizeof_arr_u64=%zu sizeof_dyarr_u64=%zu\n",
        sizeof(nk::mem::Allocator),
        alignof(nk::mem::Allocator),
        sizeof(nk::mem::MallocAllocator),
        sizeof(nk::mem::LinearAllocator),
        sizeof(nk::cl::arr<nk::u64>),
        sizeof(nk::cl::dyarr<nk::u64>));

    print_measurement(measure_malloc_allocate_free());
    print_measurement(measure_dyarr_growth());
    print_measurement(measure_dyarr_insert());
    print_measurement(measure_dyarr_remove());

    print_allocation_measurement("logging.init", count_global_allocations(initialize_logging));
    print_allocation_measurement("logging.plain", count_global_allocations(write_plain_log));
    print_allocation_measurement("logging.formatted", count_global_allocations(write_formatted_log));
    print_allocation_measurement("assertion.formatted", count_global_allocations(report_formatted_assertion));
    nk::LoggingSystem::shutdown();

    return 0;
}
