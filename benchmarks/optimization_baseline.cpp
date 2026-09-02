#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "collections/dyarr.h"
#include "collections/map.h"
#include "core/hash.h"
#include "core/str.h"
#include "core/strbuf.h"
#include "memory/malloc_allocator.h"
#include "resources/texture.h"

#if defined(_MSC_VER)
    #define NK_BENCH_NOINLINE __declspec(noinline)
#else
    #define NK_BENCH_NOINLINE __attribute__((noinline))
#endif

namespace nk::benchmark {
    struct AllocationKey {
        u64 allocator_id = 0;
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

    struct AllocationRecord {
        u64 fields[6]{};
    };
}

namespace {
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t sample_count = 15;
    constexpr nk::u64 scalar_operation_count = 500000;
    constexpr nk::u64 allocation_operation_count = 100000;
    constexpr nk::u64 lookup_operation_count = 100000;
    constexpr nk::u64 dyarr_element_count = 100000;
    constexpr nk::u64 string_operation_count = 100000;
    constexpr nk::u64 owning_contract_operation_count = 20000;

    enum class BaselineStatus : nk::u8 {
        Success,
        Failure,
    };

    struct Distribution {
        nk::u64 p50 = 0;
        nk::u64 p95 = 0;
        nk::u64 p99 = 0;
        nk::u64 checksum = 0;
    };

    template <typename Operation>
    Distribution measure(Operation&& operation) {
        std::array<nk::u64, sample_count> samples{};
        nk::u64 checksum = 0;
        for (std::size_t sample = 0; sample < sample_count; ++sample) {
            nk::u64 sample_checksum = 0;
            const auto start = Clock::now();
            operation(static_cast<nk::u64>(sample), sample_checksum);
            const auto end = Clock::now();
            samples[sample] = static_cast<nk::u64>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
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

    void print(
        const nk::cstr metric,
        const nk::u64 operations,
        const Distribution& distribution) {
        std::printf(
            "metric=%s operations=%llu p50_ns=%llu p95_ns=%llu p99_ns=%llu p50_ns_per_op=%.3f checksum=%llu\n",
            metric,
            static_cast<unsigned long long>(operations),
            static_cast<unsigned long long>(distribution.p50),
            static_cast<unsigned long long>(distribution.p95),
            static_cast<unsigned long long>(distribution.p99),
            static_cast<double>(distribution.p50) / static_cast<double>(operations),
            static_cast<unsigned long long>(distribution.checksum));
    }

    NK_BENCH_NOINLINE bool baseline_bool_out(
        const nk::u64 input,
        nk::u64* output) noexcept {
        if ((input & 1023u) == 1023u)
            return false;
        *output = nk::hash64(input);
        return true;
    }

    NK_BENCH_NOINLINE nk::u64* baseline_nullable(
        const nk::u64 input,
        nk::u64* output) noexcept {
        if ((input & 1023u) == 1023u)
            return nullptr;
        *output = nk::hash64(input);
        return output;
    }

    NK_BENCH_NOINLINE BaselineStatus baseline_status_out(
        const nk::u64 input,
        nk::u64* output) noexcept {
        if ((input & 1023u) == 1023u)
            return BaselineStatus::Failure;
        *output = nk::hash64(input);
        return BaselineStatus::Success;
    }

    template <typename T, typename Factory>
    NK_BENCH_NOINLINE bool baseline_bool_out_payload(
        const nk::u64 input,
        T* output,
        Factory& factory) {
        if ((input & 1023u) == 1023u)
            return false;
        *output = factory(input);
        return true;
    }

    template <typename T, typename Factory>
    NK_BENCH_NOINLINE T* baseline_nullable_payload(
        const nk::u64 input,
        T* output,
        Factory& factory) {
        if ((input & 1023u) == 1023u)
            return nullptr;
        *output = factory(input);
        return output;
    }

    template <typename T, typename Factory>
    NK_BENCH_NOINLINE BaselineStatus baseline_status_out_payload(
        const nk::u64 input,
        T* output,
        Factory& factory) {
        if ((input & 1023u) == 1023u)
            return BaselineStatus::Failure;
        *output = factory(input);
        return BaselineStatus::Success;
    }

    nk::u64 payload_checksum(const nk::u32 value) noexcept { return value; }
    nk::u64 payload_checksum(const nk::u64 value) noexcept { return value; }
    nk::u64 payload_checksum(void* const value) noexcept {
        return static_cast<nk::u64>(reinterpret_cast<std::uintptr_t>(value));
    }
    nk::u64 payload_checksum(const nk::Texture& value) noexcept {
        return value.id ^ value.width ^ value.height ^ value.generation;
    }
    nk::u64 payload_checksum(const nk::str& value) noexcept {
        return value.length() == 0 ? 0 : static_cast<nk::u8>(value[0]);
    }
    nk::u64 payload_checksum(const nk::cl::dyarr<nk::u8>& value) {
        return value.empty() ? 0 : value[0];
    }

    template <typename T, typename Factory>
    void benchmark_payload_contracts(
        const nk::cstr payload_name,
        const nk::u64 operations,
        Factory&& factory) {
        auto bool_distribution = measure([&](const nk::u64 sample, nk::u64& checksum) {
            T output = factory(sample);
            for (nk::u64 index = 0; index < operations; ++index) {
                if (baseline_bool_out_payload(index + sample, &output, factory))
                    checksum ^= payload_checksum(output);
            }
        });
        auto nullable_distribution = measure([&](const nk::u64 sample, nk::u64& checksum) {
            T output = factory(sample);
            for (nk::u64 index = 0; index < operations; ++index) {
                if (T* result = baseline_nullable_payload(index + sample, &output, factory))
                    checksum ^= payload_checksum(*result);
            }
        });
        auto status_distribution = measure([&](const nk::u64 sample, nk::u64& checksum) {
            T output = factory(sample);
            for (nk::u64 index = 0; index < operations; ++index) {
                if (baseline_status_out_payload(index + sample, &output, factory) ==
                    BaselineStatus::Success) {
                    checksum ^= payload_checksum(output);
                }
            }
        });

        char metric[96]{};
        const auto print_contract = [&](const nk::cstr contract, const Distribution& value) {
            const int written = std::snprintf(
                metric,
                sizeof(metric),
                "contract.%s.%s",
                contract,
                payload_name);
            if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(metric))
                std::abort();
            print(metric, operations, value);
        };
        print_contract("bool_out", bool_distribution);
        print_contract("nullable", nullable_distribution);
        print_contract("status_out", status_distribution);
    }

    void print_payload_contract_benchmarks() {
        benchmark_payload_contracts<nk::u32>(
            "u32",
            scalar_operation_count,
            [](const nk::u64 input) { return static_cast<nk::u32>(nk::hash64(input)); });
        benchmark_payload_contracts<nk::u64>(
            "u64",
            scalar_operation_count,
            [](const nk::u64 input) { return nk::hash64(input); });
        benchmark_payload_contracts<void*>(
            "pointer",
            scalar_operation_count,
            [](const nk::u64 input) {
                return reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>((input + 1) * 16));
            });
        benchmark_payload_contracts<nk::Texture>(
            "texture",
            scalar_operation_count,
            [](const nk::u64 input) {
                return nk::Texture{
                    .id = static_cast<nk::u32>(input),
                    .width = 1920,
                    .height = 1080,
                    .channel_count = 4,
                    .has_transparency = true,
                    .generation = static_cast<nk::u32>(input >> 1),
                    .m_internal_data = reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>((input + 1) * 16)),
                };
            });

        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        benchmark_payload_contracts<nk::str>(
            "str_sso",
            owning_contract_operation_count,
            [&](const nk::u64 input) {
                return nk::str{
                    allocator,
                    (input & 1u) == 0 ? "payload-even" : "payload-odd"};
            });
        benchmark_payload_contracts<nk::cl::dyarr<nk::u8>>(
            "dyarr_u8",
            owning_contract_operation_count,
            [&](const nk::u64 input) {
                nk::cl::dyarr<nk::u8> value;
                if (!value._dyarr_init(&allocator, 1))
                    std::abort();
                const nk::u8 byte = static_cast<nk::u8>(input);
                if (!value._dyarr_push_copy(byte))
                    std::abort();
                return value;
            });
    }

    Distribution benchmark_bool_out() {
        return measure([](const nk::u64 sample, nk::u64& checksum) {
            for (nk::u64 index = 0; index < scalar_operation_count; ++index) {
                nk::u64 value = 0;
                if (baseline_bool_out(index + sample, &value))
                    checksum ^= value;
            }
        });
    }

    Distribution benchmark_nullable() {
        return measure([](const nk::u64 sample, nk::u64& checksum) {
            for (nk::u64 index = 0; index < scalar_operation_count; ++index) {
                nk::u64 value = 0;
                if (const nk::u64* result = baseline_nullable(index + sample, &value))
                    checksum ^= *result;
            }
        });
    }

    Distribution benchmark_status_out() {
        return measure([](const nk::u64 sample, nk::u64& checksum) {
            for (nk::u64 index = 0; index < scalar_operation_count; ++index) {
                nk::u64 value = 0;
                if (baseline_status_out(index + sample, &value) == BaselineStatus::Success)
                    checksum ^= value;
            }
        });
    }

    Distribution benchmark_allocation(
        const nk::u64 size_bytes,
        const nk::u64 alignment) {
        return measure([=](const nk::u64 sample, nk::u64& checksum) {
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            for (nk::u64 index = 0; index < allocation_operation_count; ++index) {
                auto* data = static_cast<nk::u8*>(
                    allocator.allocate_raw(size_bytes, alignment));
                if (data == nullptr)
                    std::abort();
                data[0] = static_cast<nk::u8>(index + sample);
                checksum += data[0];
                if (!allocator.free_raw(data, size_bytes))
                    std::abort();
            }
            if (allocator.get_active_allocation_count() != 0 ||
                allocator.get_used_bytes() != 0) {
                std::abort();
            }
        });
    }

    template <
        typename K,
        typename V,
        typename KeyAt,
        typename MissingKeyAt,
        typename ValueAt,
        typename ValueChecksum>
    void benchmark_map_lookup(
        const nk::cstr hit_metric,
        const nk::cstr miss_metric,
        const nk::u64 entry_count,
        KeyAt&& key_at,
        MissingKeyAt&& missing_key_at,
        ValueAt&& value_at,
        ValueChecksum&& value_checksum) {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::cl::map<K, V> values;
        if (!values.map_init(
                &allocator,
                entry_count,
                nk::hash_seed::deterministic)) {
            std::abort();
        }
        for (nk::u64 index = 0; index < entry_count; ++index) {
            K key = key_at(index);
            V value = value_at(index);
            if (!values.insert(std::move(key), std::move(value)))
                std::abort();
        }

        const nk::u64 storage_bytes = allocator.get_used_bytes();
        std::printf(
            "map_layout=%s entries=%llu capacity=%llu storage_bytes=%llu bytes_per_live=%.3f\n",
            hit_metric,
            static_cast<unsigned long long>(values.length()),
            static_cast<unsigned long long>(values.capacity()),
            static_cast<unsigned long long>(storage_bytes),
            static_cast<double>(storage_bytes) / static_cast<double>(values.length()));

        print(hit_metric, lookup_operation_count, measure([&](
            const nk::u64 sample,
            nk::u64& checksum) {
            for (nk::u64 index = 0; index < lookup_operation_count; ++index) {
                const K key = key_at((index * 2654435761ull + sample) % entry_count);
                const V* value = values.find(key);
                if (value == nullptr)
                    std::abort();
                checksum ^= value_checksum(*value);
            }
        }));

        print(miss_metric, lookup_operation_count, measure([&](
            const nk::u64 sample,
            nk::u64& checksum) {
            for (nk::u64 index = 0; index < lookup_operation_count; ++index) {
                const K key = missing_key_at(index + sample);
                const V* value = values.find(key);
                checksum += value == nullptr ? 1 : value_checksum(*value);
            }
        }));

        if (!values.map_shutdown())
            std::abort();
    }

    void benchmark_u64_map(const nk::u64 entry_count, const nk::cstr suffix) {
        nk::strbuf<96> hit_metric{"map.u64.lookup_hit."};
        nk::strbuf<96> miss_metric{"map.u64.lookup_miss."};
        hit_metric.append(suffix);
        miss_metric.append(suffix);
        benchmark_map_lookup<nk::u64, nk::u64>(
            hit_metric.cstr(),
            miss_metric.cstr(),
            entry_count,
            [](const nk::u64 index) { return index; },
            [=](const nk::u64 index) { return entry_count + index + 1; },
            [](const nk::u64 index) { return index * 3; },
            [](const nk::u64 value) { return value; });
    }

    void benchmark_u32_map(const nk::u64 entry_count, const nk::cstr suffix) {
        nk::strbuf<96> hit_metric{"map.u32.lookup_hit."};
        nk::strbuf<96> miss_metric{"map.u32.lookup_miss."};
        hit_metric.append(suffix);
        miss_metric.append(suffix);
        benchmark_map_lookup<nk::u32, nk::u64>(
            hit_metric.cstr(),
            miss_metric.cstr(),
            entry_count,
            [](const nk::u64 index) { return static_cast<nk::u32>(index); },
            [=](const nk::u64 index) {
                return static_cast<nk::u32>(entry_count + index + 1);
            },
            [](const nk::u64 index) { return index * 3; },
            [](const nk::u64 value) { return value; });
    }

    void benchmark_allocation_key_map(const nk::u64 entry_count) {
        benchmark_map_lookup<
            nk::benchmark::AllocationKey,
            nk::benchmark::AllocationRecord>(
            "map.allocation_key.lookup_hit.load_80",
            "map.allocation_key.lookup_miss.load_80",
            entry_count,
            [](const nk::u64 index) {
                return nk::benchmark::AllocationKey{
                    .allocator_id = index % 32,
                    .address = reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>((index + 1) * 32)),
                };
            },
            [=](const nk::u64 index) {
                return nk::benchmark::AllocationKey{
                    .allocator_id = (index + 7) % 32,
                    .address = reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>((entry_count + index + 1) * 32)),
                };
            },
            [](const nk::u64 index) {
                nk::benchmark::AllocationRecord value{};
                value.fields[0] = index * 3;
                return value;
            },
            [](const nk::benchmark::AllocationRecord& value) {
                return value.fields[0];
            });
    }

    constexpr nk::u64 string_entry_count = 1638;

    struct StringKeys {
        std::array<std::array<char, 32>, string_entry_count * 2> storage{};

        StringKeys() noexcept {
            for (nk::u64 index = 0; index < storage.size(); ++index) {
                const int written = std::snprintf(
                    storage[index].data(),
                    storage[index].size(),
                    "resource/key/%08llu",
                    static_cast<unsigned long long>(index));
                if (written <= 0 ||
                    static_cast<std::size_t>(written) >= storage[index].size()) {
                    std::abort();
                }
            }
        }

        nk::strview at(const nk::u64 index) const noexcept {
            return nk::strview{storage[index].data()};
        }
    };

    void benchmark_strview_map() {
        static const StringKeys keys;
        benchmark_map_lookup<nk::strview, nk::u64>(
            "map.strview.lookup_hit.load_80",
            "map.strview.lookup_miss.load_80",
            string_entry_count,
            [&](const nk::u64 index) { return keys.at(index); },
            [&](const nk::u64 index) {
                return keys.at(string_entry_count + index % string_entry_count);
            },
            [](const nk::u64 index) { return index * 3; },
            [](const nk::u64 value) { return value; });
    }

    Distribution benchmark_map_churn() {
        return measure([](const nk::u64 sample, nk::u64& checksum) {
            constexpr nk::u64 entry_count = 6553;
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            nk::cl::map<nk::u64, nk::u64> values;
            std::array<nk::u64, entry_count> current_keys{};
            if (!values.map_init(
                    &allocator,
                    entry_count,
                    nk::hash_seed::deterministic)) {
                std::abort();
            }
            for (nk::u64 index = 0; index < entry_count; ++index) {
                current_keys[index] = index;
                if (!values.insert(index, index * 3))
                    std::abort();
            }

            for (nk::u64 operation = 0; operation < lookup_operation_count; ++operation) {
                const nk::u64 slot = operation % entry_count;
                if (!values.remove(current_keys[slot]))
                    std::abort();
                const nk::u64 replacement =
                    entry_count + operation + sample * lookup_operation_count;
                if (!values.insert(replacement, replacement * 3))
                    std::abort();
                current_keys[slot] = replacement;
                checksum ^= replacement;
            }

            if (values.length() != entry_count || !values.map_shutdown())
                std::abort();
        });
    }

    Distribution benchmark_dyarr_push() {
        return measure([](const nk::u64 sample, nk::u64& checksum) {
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            nk::cl::dyarr<nk::u64> values;
            if (!values.dyarr_init(&allocator, 0))
                std::abort();
            for (nk::u64 index = 0; index < dyarr_element_count; ++index) {
                const nk::u64 value = index + sample;
                if (!values.dyarr_push_copy(value))
                    std::abort();
            }
            checksum = values[values.length() - 1] ^ values.capacity();
            if (!values.dyarr_shutdown())
                std::abort();
        });
    }

    struct StringInput {
        std::array<char, 257> data{};

        StringInput() noexcept {
            for (std::size_t index = 0; index + 1 < data.size(); ++index)
                data[index] = static_cast<char>('a' + index % 26);
            data.back() = '\0';
        }

        nk::strview view(const nk::u64 length) const noexcept {
            return {data.data(), length};
        }
    };

    Distribution benchmark_str_assign(const nk::u64 length) {
        static const StringInput input;
        return measure([=](const nk::u64 sample, nk::u64& checksum) {
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            nk::str value{allocator};
            if (!value.reserve(length))
                std::abort();
            for (nk::u64 operation = 0; operation < string_operation_count; ++operation) {
                if (!value.assign(input.view(length)))
                    std::abort();
                if (length != 0)
                    checksum += static_cast<nk::u8>(value[operation % length]);
                else
                    checksum += value.length();
                value.clear();
            }
            checksum ^= sample + value.capacity();
        });
    }

    Distribution benchmark_str_move(const nk::u64 length) {
        static const StringInput input;
        return measure([=](const nk::u64 sample, nk::u64& checksum) {
            nk::mem::MallocAllocator allocator{nk::mem::untracked};
            nk::str left{allocator, input.view(length)};
            nk::str right{allocator};
            for (nk::u64 operation = 0; operation < string_operation_count; ++operation) {
                if ((operation & 1u) == 0)
                    right = std::move(left);
                else
                    left = std::move(right);
                checksum += left.length() + right.length();
            }
            checksum ^= sample;
        });
    }

    void print_layouts() {
        std::printf(
            "layout sizeof_texture=%zu sizeof_str=%zu alignof_str=%zu str_inline_capacity=%llu sizeof_dyarr_u8=%zu sizeof_map_u64_record=%zu\n",
            sizeof(nk::Texture),
            sizeof(nk::str),
            alignof(nk::str),
            static_cast<unsigned long long>(nk::str::inline_capacity),
            sizeof(nk::cl::dyarr<nk::u8>),
            sizeof(nk::cl::map<nk::u64, nk::u64>));
    }

    void print_allocation_benchmarks() {
        constexpr std::array<nk::u64, 4> sizes{16, 64, 256, 4096};
        constexpr std::array<nk::u64, 4> alignments{
            alignof(std::max_align_t),
            32,
            64,
            256,
        };

        for (const nk::u64 size : sizes) {
            for (const nk::u64 alignment : alignments) {
                nk::strbuf<96> metric;
                char name[96]{};
                const int written = std::snprintf(
                    name,
                    sizeof(name),
                    "allocator.allocate_free.size_%llu.align_%llu",
                    static_cast<unsigned long long>(size),
                    static_cast<unsigned long long>(alignment));
                if (written <= 0)
                    std::abort();
                metric.assign(nk::strview{name});
                print(
                    metric.cstr(),
                    allocation_operation_count,
                    benchmark_allocation(size, alignment));
            }
        }
    }

    void print_string_benchmarks() {
        constexpr std::array<nk::u64, 6> lengths{0, 8, 23, 24, 64, 256};
        for (const nk::u64 length : lengths) {
            char assign_name[64]{};
            char move_name[64]{};
            const int assign_written = std::snprintf(
                assign_name,
                sizeof(assign_name),
                "str.assign.length_%llu",
                static_cast<unsigned long long>(length));
            const int move_written = std::snprintf(
                move_name,
                sizeof(move_name),
                "str.move.length_%llu",
                static_cast<unsigned long long>(length));
            if (assign_written <= 0 || move_written <= 0)
                std::abort();
            print(
                assign_name,
                string_operation_count,
                benchmark_str_assign(length));
            print(
                move_name,
                string_operation_count,
                benchmark_str_move(length));
        }
    }
}

int main() {
#if defined(__clang__)
    constexpr nk::cstr compiler = "clang " __clang_version__;
#elif defined(__GNUC__)
    constexpr nk::cstr compiler = "gcc " __VERSION__;
#else
    constexpr nk::cstr compiler = "unknown";
#endif

    std::printf(
        "optimization_baseline_version=1 build_type=%s compiler=%s\n",
        NK_OPTIMIZATION_BUILD_TYPE,
        compiler);
    print_layouts();
    print("contract.bool_out.legacy_u64", scalar_operation_count, benchmark_bool_out());
    print("contract.nullable.legacy_u64", scalar_operation_count, benchmark_nullable());
    print("contract.status_out.legacy_u64", scalar_operation_count, benchmark_status_out());
    print_payload_contract_benchmarks();
    print_allocation_benchmarks();
    benchmark_u32_map(4096, "load_50");
    benchmark_u32_map(6553, "load_80");
    benchmark_u64_map(4096, "load_50");
    benchmark_u64_map(6553, "load_80");
    benchmark_allocation_key_map(6553);
    benchmark_strview_map();
    print("map.u64.churn.load_80", lookup_operation_count, benchmark_map_churn());
    print("dyarr.push_copy_u64", dyarr_element_count, benchmark_dyarr_push());
    print_string_benchmarks();
    return 0;
}

#undef NK_BENCH_NOINLINE
