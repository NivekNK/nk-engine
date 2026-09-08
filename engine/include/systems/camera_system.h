#pragma once

#include "collections/arr.h"
#include "collections/map.h"
#include "core/camera.h"
#include "core/result.h"
#include "core/str.h"

namespace nk {
    namespace mem { class Allocator; }

    inline constexpr strview default_camera_name{"default", 7};

    struct CameraHandle {
        u32 index = numeric::invalid_id;
        u32 generation = numeric::invalid_id;

        [[nodiscard]] bool valid() const noexcept {
            return index != numeric::invalid_id &&
                   generation != numeric::invalid_id;
        }

        bool operator==(const CameraHandle&) const noexcept = default;
    };

    enum class camera_error_code : u8 {
        invalid_name,
        already_initialized,
        not_initialized,
        capacity_exceeded,
        out_of_memory,
        invalid_handle,
        default_camera_release,
    };

    struct camera_error {
        camera_error_code code;
    };

    class CameraSystem final {
    public:
        static constexpr u32 default_max_camera_count = 61;

        CameraSystem() noexcept = default;
        ~CameraSystem();

        CameraSystem(const CameraSystem&) = delete;
        CameraSystem& operator=(const CameraSystem&) = delete;
        CameraSystem(CameraSystem&&) = delete;
        CameraSystem& operator=(CameraSystem&&) = delete;

        [[nodiscard]] static result<CameraSystem*, camera_error> create(
            mem::Allocator& allocator,
            u32 max_camera_count = default_max_camera_count);
        static void destroy(mem::Allocator& allocator, CameraSystem* system);
        [[nodiscard]] static CameraSystem* current() noexcept;

        [[nodiscard]] result<CameraHandle, camera_error> acquire(strview name);
        [[nodiscard]] result<void, camera_error> release(CameraHandle handle);
        [[nodiscard]] result<void, camera_error> set_active(CameraHandle handle);

        [[nodiscard]] Camera* get(CameraHandle handle) noexcept;
        [[nodiscard]] const Camera* get(CameraHandle handle) const noexcept;
        [[nodiscard]] Camera* active() noexcept { return get(m_active); }
        [[nodiscard]] const Camera* active() const noexcept {
            return get(m_active);
        }
        [[nodiscard]] CameraHandle active_handle() const noexcept {
            return m_active;
        }
        [[nodiscard]] CameraHandle default_handle() const noexcept {
            return m_default;
        }
        [[nodiscard]] u64 reference_count(strview name) const noexcept;
        [[nodiscard]] u32 loaded_count() const noexcept { return m_loaded_count; }

    private:
        struct CameraSlot {
            Camera camera{};
            u32 generation = numeric::invalid_id;
            bool occupied = false;
        };

        struct CameraReference {
            CameraHandle handle{};
            u64 count = 0;
        };

        [[nodiscard]] result<void, camera_error> init(
            mem::Allocator& allocator,
            u32 max_camera_count);
        void shutdown() noexcept;
        [[nodiscard]] u32 find_free_slot() const noexcept;

        mem::Allocator* m_allocator = nullptr;
        cl::arr<CameraSlot> m_cameras;
        cl::map<str, CameraReference> m_references;
        CameraHandle m_default{};
        CameraHandle m_active{};
        u32 m_loaded_count = 0;
        bool m_initialized = false;
    };
}
