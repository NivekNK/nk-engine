#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "collections/map.h"
#include "core/hash.h"
#include "memory/malloc_allocator.h"

namespace {
    using Clock = std::chrono::steady_clock;

    constexpr nk::u64 operation_count = 20000;
    constexpr std::size_t sample_count = 31;

    struct Distribution {
        nk::u64 p50;
        nk::u64 p95;
        nk::u64 p99;
        nk::u64 checksum;
    };

    template <typename Operation>
    Distribution measure(Operation&& operation) {
        std::array<nk::u64, sample_count> samples{};
        nk::u64 checksum = 0;
        for (std::size_t sample = 0; sample < sample_count; ++sample) {
            const auto start = Clock::now();
            checksum ^= operation(sample);
            const auto end = Clock::now();
            samples[sample] = static_cast<nk::u64>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        }
        std::sort(samples.begin(), samples.end());
        return {
            .p50 = samples[(sample_count * 50) / 100],
            .p95 = samples[(sample_count * 95) / 100],
            .p99 = samples[(sample_count * 99) / 100],
            .checksum = checksum,
        };
    }

    template <typename Operation>
    Distribution measure_map(Operation&& operation) {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        return measure([&](const std::size_t sample) {
            nk::cl::map<nk::u64, nk::u64> values;
            if (!values.map_init(&allocator, operation_count, nk::hash_seed::deterministic))
                std::abort();
            const nk::u64 checksum = operation(values, static_cast<nk::u64>(sample));
            if (!values.map_shutdown())
                std::abort();
            return checksum;
        });
    }

    Distribution benchmark_insert() {
        return measure_map([](auto& values, const nk::u64 sample) {
            nk::u64 checksum = sample;
            for (nk::u64 key = 0; key < operation_count; ++key) {
                if (!values.insert(key, key ^ sample))
                    std::abort();
                checksum ^= key;
            }
            return checksum ^ values.length();
        });
    }

    Distribution benchmark_lookup() {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::cl::map<nk::u64, nk::u64> values;
        if (!values.map_init(&allocator, operation_count, nk::hash_seed::deterministic))
            std::abort();
        for (nk::u64 key = 0; key < operation_count; ++key) {
            if (!values.insert(key, key * 3))
                std::abort();
        }
        const Distribution result = measure([&](const std::size_t sample) {
            nk::u64 checksum = static_cast<nk::u64>(sample);
            for (nk::u64 index = 0; index < operation_count; ++index) {
                const nk::u64 key = (index * 2654435761ull + sample) % operation_count;
                const nk::u64* value = values.find(key);
                if (value == nullptr)
                    std::abort();
                checksum ^= *value;
            }
            return checksum;
        });
        if (!values.map_shutdown())
            std::abort();
        return result;
    }

    Distribution benchmark_remove() {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        std::array<nk::u64, sample_count> samples{};
        nk::u64 checksum = 0;
        for (std::size_t sample = 0; sample < sample_count; ++sample) {
            nk::cl::map<nk::u64, nk::u64> values;
            if (!values.map_init(&allocator, operation_count, nk::hash_seed::deterministic))
                std::abort();
            for (nk::u64 key = 0; key < operation_count; ++key) {
                if (!values.insert(key, key))
                    std::abort();
            }

            const auto start = Clock::now();
            for (nk::u64 key = 0; key < operation_count; ++key) {
                if (!values.remove(key))
                    std::abort();
                checksum ^= key + static_cast<nk::u64>(sample);
            }
            const auto end = Clock::now();
            samples[sample] = static_cast<nk::u64>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
            if (!values.empty() || !values.map_shutdown())
                std::abort();
        }
        std::sort(samples.begin(), samples.end());
        return {
            .p50 = samples[(sample_count * 50) / 100],
            .p95 = samples[(sample_count * 95) / 100],
            .p99 = samples[(sample_count * 99) / 100],
            .checksum = checksum,
        };
    }

    constexpr std::array<nk::cstr, 24> engine_keys{
        "engine/assets/shaders/Builtin.ObjectShader.vert.slang",
        "engine/assets/shaders/Builtin.ObjectShader.frag.slang",
        "engine/assets/models/smooth_vase.obj",
        "engine/assets/models/flat_vase.obj",
        "renderer.vulkan.swapchain.images",
        "renderer.vulkan.command_buffers",
        "renderer.vulkan.object_shader.descriptor_sets",
        "renderer.vulkan.object_shader.global_uniform",
        "renderer.vulkan.device.graphics_queue",
        "renderer.vulkan.device.present_queue",
        "platform.wayland.xdg_surface",
        "platform.wayland.xdg_toplevel",
        "platform.wayland.keyboard",
        "platform.wayland.pointer",
        "systems.memory.allocators",
        "systems.memory.allocations",
        "systems.logging.styles",
        "systems.events.listeners",
        "systems.input.keyboard",
        "systems.input.mouse",
        "editor.application",
        "engine.frame_allocator",
        "engine.application_allocator",
        "engine.renderer_allocator",
    };

    nk::u64 fnv1a(const nk::strview value, nk::u64 seed) noexcept {
        nk::u64 hash = 14695981039346656037ull ^ seed;
        for (const char character : value) {
            hash ^= static_cast<nk::u8>(character);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    template <typename Hash>
    Distribution benchmark_hash(Hash&& hash) {
        return measure([&](const std::size_t sample) {
            nk::u64 checksum = 0;
            for (nk::u64 index = 0; index < operation_count; ++index) {
                const nk::strview key{engine_keys[(index + sample) % engine_keys.size()]};
                checksum ^= hash(key, nk::hash_seed::deterministic + index);
            }
            return checksum;
        });
    }

    void print(const nk::cstr metric, const Distribution& value) {
        std::printf(
            "metric=%s operations=%llu p50_ns=%llu p95_ns=%llu p99_ns=%llu p50_ns_per_op=%.3f checksum=%llu\n",
            metric,
            static_cast<unsigned long long>(operation_count),
            static_cast<unsigned long long>(value.p50),
            static_cast<unsigned long long>(value.p95),
            static_cast<unsigned long long>(value.p99),
            static_cast<double>(value.p50) / static_cast<double>(operation_count),
            static_cast<unsigned long long>(value.checksum));
    }
}

int main() {
    std::printf("collections_benchmark_version=1 build_type=%s\n", NK_BENCHMARK_BUILD_TYPE);
    print("map.insert", benchmark_insert());
    print("map.lookup", benchmark_lookup());
    print("map.remove", benchmark_remove());
    print("hash.rapidhash_v3.engine_keys", benchmark_hash([](const auto key, const auto seed) {
        return nk::hash64(key, seed);
    }));
    print("hash.fnv1a_benchmark_baseline.engine_keys", benchmark_hash(fnv1a));
    return 0;
}
