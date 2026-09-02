#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace nk::mem {
    // Storage eligibility is intentionally weaker than any individual
    // construction or movement requirement. Operations constrain T themselves.
    template <typename T>
    concept ObjectStorageType = std::is_object_v<T> &&
                                !std::is_const_v<T> &&
                                !std::is_volatile_v<T> &&
                                !std::is_array_v<T>;

    template <typename T>
    inline constexpr bool is_trivially_relocatable_v =
        std::is_trivially_copyable_v<T>;

    template <typename T>
    concept TriviallyRelocatable = ObjectStorageType<T> &&
                                   is_trivially_relocatable_v<T>;

    template <typename T>
    concept RelocatableObject = ObjectStorageType<T> &&
                                std::is_destructible_v<T> &&
                                (TriviallyRelocatable<T> ||
                                 std::is_move_constructible_v<T>);

    template <ObjectStorageType T, typename... Args>
        requires std::is_constructible_v<T, Args...>
    [[nodiscard]] T* construct_object(T* destination, Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return std::construct_at(destination, std::forward<Args>(args)...);
    }

    template <ObjectStorageType T>
        requires std::is_default_constructible_v<T>
    void construct_value_range(T* destination, const std::size_t count)
        noexcept(std::is_nothrow_default_constructible_v<T>) {
        for (std::size_t index = 0; index < count; ++index)
            std::construct_at(destination + index);
    }

    template <ObjectStorageType T>
        requires std::is_copy_constructible_v<T>
    // destination is raw storage and must not overlap the live source range.
    void construct_copy_range(
        T* destination,
        const T* source,
        const std::size_t count)
        noexcept(std::is_nothrow_copy_constructible_v<T>) {
        if (count == 0)
            return;

        if constexpr (TriviallyRelocatable<T>) {
            std::memcpy(destination, source, count * sizeof(T));
        } else {
            for (std::size_t index = 0; index < count; ++index)
                std::construct_at(destination + index, source[index]);
        }
    }

    template <ObjectStorageType T>
        requires std::is_move_constructible_v<T>
    // destination is raw storage and must not overlap the live source range.
    // Moving does not end the source lifetimes.
    void construct_move_range(
        T* destination,
        T* source,
        const std::size_t count)
        noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (count == 0 || destination == source)
            return;

        if constexpr (TriviallyRelocatable<T>) {
            std::memcpy(destination, source, count * sizeof(T));
        } else {
            for (std::size_t index = 0; index < count; ++index)
                std::construct_at(destination + index, std::move(source[index]));
        }
    }

    template <ObjectStorageType T>
        requires std::is_move_assignable_v<T>
    // Both ranges contain live objects. Overlap is supported.
    void move_assign_range(
        T* destination,
        T* source,
        const std::size_t count)
        noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (count == 0 || destination == source)
            return;

        if constexpr (TriviallyRelocatable<T>) {
            std::memmove(destination, source, count * sizeof(T));
        } else {
            const auto destination_address =
                reinterpret_cast<std::uintptr_t>(destination);
            const auto source_address = reinterpret_cast<std::uintptr_t>(source);
            const auto byte_count = count * sizeof(T);
            const bool overlaps_to_the_right =
                destination_address > source_address &&
                destination_address - source_address < byte_count;

            if (overlaps_to_the_right) {
                for (std::size_t index = count; index > 0; --index)
                    destination[index - 1] = std::move(source[index - 1]);
                return;
            }

            for (std::size_t index = 0; index < count; ++index)
                destination[index] = std::move(source[index]);
        }
    }

    template <ObjectStorageType T>
        requires std::is_destructible_v<T>
    // Every object in data[0, count) must be alive.
    void destroy_range(T* data, const std::size_t count)
        noexcept(std::is_nothrow_destructible_v<T>) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (std::size_t index = count; index > 0; --index)
                std::destroy_at(data + index - 1);
        }
    }

    template <RelocatableObject T>
    // source contains live objects. destination is raw storage outside the
    // overlap, and every source lifetime ends before this function returns.
    void relocate_range(
        T* destination,
        T* source,
        const std::size_t count)
        noexcept(
            TriviallyRelocatable<T> ||
            (std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_destructible_v<T>)) {
        if (count == 0 || destination == source)
            return;

        if constexpr (TriviallyRelocatable<T>) {
            std::memmove(destination, source, count * sizeof(T));
        } else {
            const auto destination_address =
                reinterpret_cast<std::uintptr_t>(destination);
            const auto source_address = reinterpret_cast<std::uintptr_t>(source);
            const auto byte_count = count * sizeof(T);
            const bool overlaps_to_the_right =
                destination_address > source_address &&
                destination_address - source_address < byte_count;

            if (overlaps_to_the_right) {
                for (std::size_t index = count; index > 0; --index) {
                    std::construct_at(
                        destination + index - 1,
                        std::move(source[index - 1]));
                    std::destroy_at(source + index - 1);
                }
                return;
            }

            for (std::size_t index = 0; index < count; ++index) {
                std::construct_at(destination + index, std::move(source[index]));
                std::destroy_at(source + index);
            }
        }
    }
}
