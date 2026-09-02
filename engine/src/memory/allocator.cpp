#include "nkpch.h"

#include "memory/allocator.h"

namespace nk::mem {
    namespace {
        void allocator_diagnostic(cstr message, std::size_t length) noexcept {
            constexpr cstr color = "\033[38;2;233;38;109m";
            os::write(color, std::char_traits<char>::length(color));
            os::write(message, length);
            os::write("\033[0m\n", 5);
            os::flush();
        }

        template <std::size_t N>
        void allocator_diagnostic(const char (&message)[N]) noexcept {
            allocator_diagnostic(message, N - 1);
        }
    }

    Allocator::Allocator() noexcept
        : m_reserved_bytes{0},
          m_used_bytes{0},
          m_peak_used_bytes{0},
          m_active_allocations{0},
          m_data{nullptr},
          m_lifecycle{AllocatorLifecycle::Uninitialized}
#if NK_MEMORY_TRACKING_ENABLED
          , m_tracker{nullptr},
          m_allocator_id{invalid_allocator_id}
#endif
    {
    }

    Allocator::~Allocator() {
        if (m_active_allocations != 0 || m_used_bytes != 0) {
            allocator_diagnostic("nk::mem::Allocator destroyed with active allocations.");
        }
        _detach_tracker_unchecked();
    }

    Allocator::Allocator(Allocator&& other) noexcept
        : Allocator() {
        _move_from(other);
    }

    Allocator& Allocator::operator=(Allocator&& other) noexcept {
        if (this == &other)
            return *this;

        if (!_can_accept_move()) {
            allocator_diagnostic("nk::mem::Allocator move assignment rejected: destination still owns active allocations.");
            return *this;
        }

        _detach_tracker_unchecked();
        _move_from(other);
        return *this;
    }

    bool Allocator::_begin_initialization() noexcept {
        if (m_lifecycle == AllocatorLifecycle::Uninitialized)
            return true;

        allocator_diagnostic("nk::mem::Allocator cannot be initialized more than once.");
        return false;
    }

    void Allocator::_complete_initialization(Untracked) noexcept {
        m_lifecycle = AllocatorLifecycle::Initialized;
    }

#if NK_MEMORY_TRACKING_ENABLED
    bool Allocator::_complete_initialization(
        AllocationTracker& tracker,
        const AllocatorDescriptor& descriptor) noexcept {
        m_tracker = &tracker;
        m_allocator_id = tracker.register_allocator(*this, descriptor);
        if (m_allocator_id == invalid_allocator_id) {
            allocator_diagnostic("nk::mem::Allocator tracker registration failed.");
            m_tracker = nullptr;
            m_lifecycle = AllocatorLifecycle::InitializationFailed;
            return false;
        }
        m_lifecycle = AllocatorLifecycle::Initialized;
        return true;
    }

    bool Allocator::attach_tracker(
        AllocationTracker& tracker,
        cstr name,
        u32 memory_type,
        SourceLocation source) noexcept {
        if (!is_initialized() || is_tracked() || m_active_allocations != 0) {
            allocator_diagnostic("nk::mem::Allocator tracker attach rejected by allocator state.");
            return false;
        }

        m_tracker = &tracker;
        m_allocator_id = tracker.register_allocator(*this, {
            .name = name,
            .implementation = to_cstr(),
            .memory_type = memory_type,
            .source = source,
            .statistics = statistics(),
        });
        if (m_allocator_id == invalid_allocator_id) {
            m_tracker = nullptr;
            return false;
        }
        return true;
    }

    bool Allocator::detach_tracker() noexcept {
        if (!is_initialized() || m_active_allocations != 0) {
            allocator_diagnostic("nk::mem::Allocator tracker detach rejected by allocator state.");
            return false;
        }

        _detach_tracker_unchecked();
        return true;
    }
#endif

    void Allocator::_detach_tracker_unchecked() noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id)
            m_tracker->unregister_allocator(m_allocator_id);
        m_tracker = nullptr;
        m_allocator_id = invalid_allocator_id;
#endif
    }

    void* Allocator::_allocate_raw(u64 size_bytes, u64 alignment) noexcept {
        return _allocate_impl({nullptr, 0}, size_bytes, alignment);
    }

    void* Allocator::_allocate_zeroed_raw(u64 size_bytes, u64 alignment) noexcept {
        void* data = _allocate_impl({nullptr, 0}, size_bytes, alignment);
        if (data != nullptr)
            std::memset(data, 0, static_cast<std::size_t>(size_bytes));
        return data;
    }

    bool Allocator::_free_raw(void* data, u64 size_bytes) noexcept {
        return _free_impl({nullptr, 0}, data, size_bytes);
    }

#if NK_MEMORY_TRACKING_ENABLED
    void* Allocator::_allocate_raw(
        cstr file,
        u32 line,
        u64 size_bytes,
        u64 alignment) noexcept {
        return _allocate_impl({file, line}, size_bytes, alignment);
    }

    void* Allocator::_allocate_zeroed_raw(
        cstr file,
        u32 line,
        u64 size_bytes,
        u64 alignment) noexcept {
        void* data = _allocate_impl({file, line}, size_bytes, alignment);
        if (data != nullptr)
            std::memset(data, 0, static_cast<std::size_t>(size_bytes));
        return data;
    }

    bool Allocator::_free_raw(
        cstr file,
        u32 line,
        void* data,
        u64 size_bytes) noexcept {
        return _free_impl({file, line}, data, size_bytes);
    }
#endif

    void* Allocator::_allocate_impl(
        SourceLocation source,
        u64 size_bytes,
        u64 alignment) noexcept {
        if (!is_initialized()) {
            allocator_diagnostic("nk::mem::Allocator allocation rejected before initialization.");
            return nullptr;
        }

        if (size_bytes == 0)
            return nullptr;

        if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
            alignment > static_cast<u64>(std::numeric_limits<std::size_t>::max())) {
            allocator_diagnostic("nk::mem::Allocator allocation rejected invalid alignment.");
            return nullptr;
        }

        if (size_bytes > static_cast<u64>(std::numeric_limits<std::size_t>::max())) {
            allocator_diagnostic("nk::mem::Allocator allocation rejected unrepresentable size.");
            return nullptr;
        }

        void* data = _do_allocate(size_bytes, alignment);
        if (data == nullptr)
            return nullptr;

#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id) {
            m_tracker->on_allocate({
                .allocator_id = m_allocator_id,
                .address = data,
                .size_bytes = size_bytes,
                .alignment = alignment,
                .source = source,
                .statistics = statistics(),
            });
        }
#else
        static_cast<void>(source);
#endif
        return data;
    }

    bool Allocator::_free_impl(
        SourceLocation source,
        void* data,
        u64 size_bytes) noexcept {
        if (!is_initialized()) {
            allocator_diagnostic("nk::mem::Allocator free rejected before initialization.");
            return false;
        }

        if (data == nullptr || size_bytes == 0)
            return false;

#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id) {
            const FreeValidation validation =
                m_tracker->validate_free(m_allocator_id, data, size_bytes);
            if (validation != FreeValidation::Valid) {
                switch (validation) {
                    case FreeValidation::UnknownAllocator:
                        allocator_diagnostic("nk::mem::Allocator free rejected unknown allocator.");
                        break;
                    case FreeValidation::UnknownAddress:
                        allocator_diagnostic("nk::mem::Allocator free rejected unknown address.");
                        break;
                    case FreeValidation::SizeMismatch:
                        allocator_diagnostic("nk::mem::Allocator free rejected size mismatch.");
                        break;
                    case FreeValidation::AlreadyFreed:
                        allocator_diagnostic("nk::mem::Allocator free rejected double free.");
                        break;
                    case FreeValidation::TrackerUnavailable:
                        allocator_diagnostic("nk::mem::Allocator free rejected unavailable tracker.");
                        break;
                    case FreeValidation::Valid:
                        break;
                }
                return false;
            }
        }
#endif

        if (!_do_free(data, size_bytes))
            return false;

#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id) {
            m_tracker->on_free({
                .allocator_id = m_allocator_id,
                .address = data,
                .size_bytes = size_bytes,
                .alignment = 0,
                .source = source,
                .statistics = statistics(),
            });
        }
#else
        static_cast<void>(source);
#endif
        return true;
    }

    void Allocator::_notify_reset(SourceLocation source) noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id) {
            m_tracker->on_reset({
                .allocator_id = m_allocator_id,
                .source = source,
                .statistics = statistics(),
            });
        }
#else
        static_cast<void>(source);
#endif
    }

    bool Allocator::_can_accept_move() const noexcept {
        return m_active_allocations == 0 && m_used_bytes == 0;
    }

    void Allocator::_move_from(Allocator& other) noexcept {
        m_reserved_bytes = other.m_reserved_bytes;
        m_used_bytes = other.m_used_bytes;
        m_peak_used_bytes = other.m_peak_used_bytes;
        m_active_allocations = other.m_active_allocations;
        m_data = other.m_data;
        m_lifecycle = other.m_lifecycle;
#if NK_MEMORY_TRACKING_ENABLED
        m_tracker = other.m_tracker;
        m_allocator_id = other.m_allocator_id;
#endif
        other._reset_moved_from();
    }

    void Allocator::_reset_moved_from() noexcept {
        m_reserved_bytes = 0;
        m_used_bytes = 0;
        m_peak_used_bytes = 0;
        m_active_allocations = 0;
        m_data = nullptr;
        m_lifecycle = AllocatorLifecycle::MovedFrom;
#if NK_MEMORY_TRACKING_ENABLED
        m_tracker = nullptr;
        m_allocator_id = invalid_allocator_id;
#endif
    }

    bool Allocator::is_tracked() const noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        return m_tracker != nullptr && m_allocator_id != invalid_allocator_id;
#else
        return false;
#endif
    }

    AllocatorId Allocator::allocator_id() const noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        return m_allocator_id;
#else
        return invalid_allocator_id;
#endif
    }

    cstr Allocator::_allocator_name() const noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        if (m_tracker != nullptr && m_allocator_id != invalid_allocator_id)
            return m_tracker->allocator_name(m_allocator_id);
#endif
        return is_initialized() ? "Untracked" : "Invalid";
    }
}
