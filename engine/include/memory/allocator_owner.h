#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "core/os.h"
#include "memory/allocator.h"

namespace nk::mem {
    class AllocatorOwner {
    public:
        using Destroy = void (*)(Allocator*) noexcept;

        struct Ownership {
            Allocator* allocator;
            Destroy destroy;
        };

        AllocatorOwner() noexcept = default;

        ~AllocatorOwner() noexcept {
            reset();
        }

        AllocatorOwner(const AllocatorOwner&) = delete;
        AllocatorOwner& operator=(const AllocatorOwner&) = delete;

        AllocatorOwner(AllocatorOwner&& other) noexcept {
            _move_from(other);
        }

        AllocatorOwner& operator=(AllocatorOwner&& other) noexcept {
            if (this == &other)
                return *this;

            reset();
            _move_from(other);
            return *this;
        }

        template <typename A, typename... Args>
            requires std::derived_from<A, Allocator> &&
                     (!std::is_abstract_v<A>) &&
                     std::is_nothrow_destructible_v<A> &&
                     std::is_constructible_v<A, Args...>
        static AllocatorOwner make_native(
            const SourceLocation source,
            Args&&... args)
            noexcept(std::is_nothrow_constructible_v<A, Args...>) {
#if NK_MEMORY_TRACKING_ENABLED
            void* storage = os::_native_allocate(
                source.file,
                source.line,
                sizeof(A),
                alignof(A));
#else
            static_cast<void>(source);
            void* storage = os::_native_allocate(sizeof(A), alignof(A));
#endif
            if (storage == nullptr)
                return {};

            A* allocator =
                std::construct_at(static_cast<A*>(storage), std::forward<Args>(args)...);
#if NK_MEMORY_TRACKING_ENABLED
            return AllocatorOwner{
                allocator,
                &_destroy_native_tracked<A>};
#else
            return AllocatorOwner{
                allocator,
                &_destroy_native_untracked<A>};
#endif
        }

        template <typename A, typename... Args>
            requires std::derived_from<A, Allocator> &&
                     (!std::is_abstract_v<A>) &&
                     std::is_nothrow_destructible_v<A> &&
                     std::is_constructible_v<A, Args...>
        static AllocatorOwner make_native_untracked(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<A, Args...>) {
            void* storage = os::_native_allocate(sizeof(A), alignof(A));
            if (storage == nullptr)
                return {};

            A* allocator =
                std::construct_at(static_cast<A*>(storage), std::forward<Args>(args)...);
            return AllocatorOwner{
                allocator,
                &_destroy_native_untracked<A>};
        }

        template <typename A>
            requires std::derived_from<A, Allocator> &&
                     (!std::is_abstract_v<A>) &&
                     std::is_nothrow_destructible_v<A>
        static AllocatorOwner adopt_native(
            A* allocator,
            const SourceLocation source) noexcept {
            if (allocator == nullptr)
                return {};

#if NK_MEMORY_TRACKING_ENABLED
            static_cast<void>(source);
            return AllocatorOwner{
                allocator,
                &_destroy_native_tracked<A>};
#else
            static_cast<void>(source);
            return AllocatorOwner{
                allocator,
                &_destroy_native_untracked<A>};
#endif
        }

        template <typename A>
            requires std::derived_from<A, Allocator> &&
                     (!std::is_abstract_v<A>) &&
                     std::is_nothrow_destructible_v<A>
        static AllocatorOwner adopt_native_untracked(A* allocator) noexcept {
            if (allocator == nullptr)
                return {};

            return AllocatorOwner{
                allocator,
                &_destroy_native_untracked<A>};
        }

        void reset() noexcept {
            if (m_allocator != nullptr)
                m_destroy(m_allocator);
            _reset();
        }

        Ownership release() noexcept {
            const Ownership ownership{
                .allocator = m_allocator,
                .destroy = m_destroy,
            };
            _reset();
            return ownership;
        }

        Allocator* get() const noexcept {
            return m_allocator;
        }

        explicit operator bool() const noexcept {
            return m_allocator != nullptr;
        }

    private:
        AllocatorOwner(
            Allocator* allocator,
            const Destroy destroy) noexcept
            : m_allocator{allocator},
              m_destroy{destroy} {}

        template <typename A>
        static void _destroy_native_untracked(
            Allocator* allocator) noexcept {
            A* typed_allocator = static_cast<A*>(allocator);
            std::destroy_at(typed_allocator);
            os::_native_free(typed_allocator, sizeof(A));
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename A>
        static void _destroy_native_tracked(
            Allocator* allocator) noexcept {
            A* typed_allocator = static_cast<A*>(allocator);
            std::destroy_at(typed_allocator);
            os::_native_free(
                __FILE__,
                __LINE__,
                typed_allocator,
                sizeof(A));
        }
#endif

        void _move_from(AllocatorOwner& other) noexcept {
            m_allocator = other.m_allocator;
            m_destroy = other.m_destroy;
            other._reset();
        }

        void _reset() noexcept {
            m_allocator = nullptr;
            m_destroy = nullptr;
        }

        Allocator* m_allocator = nullptr;
        Destroy m_destroy = nullptr;
    };
}
