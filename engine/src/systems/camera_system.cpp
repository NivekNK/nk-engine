#include "nkpch.h"

#include "systems/camera_system.h"

#include "memory/allocator.h"

namespace nk {
    namespace {
        CameraSystem* active_camera_system = nullptr;

        u32 next_generation(const u32 generation) noexcept {
            if (generation == numeric::invalid_id)
                return 0;
            const u32 next = generation + 1;
            return next == numeric::invalid_id ? 0 : next;
        }
    }

    CameraSystem::~CameraSystem() {
        shutdown();
    }

    result<CameraSystem*, camera_error> CameraSystem::create(
        mem::Allocator& allocator,
        const u32 max_camera_count) {
        if (active_camera_system != nullptr) {
            return err(camera_error{
                camera_error_code::already_initialized,
            });
        }

        CameraSystem* system = allocator.construct_t(CameraSystem);
        if (system == nullptr)
            return err(camera_error{camera_error_code::out_of_memory});

        auto initialized = system->init(allocator, max_camera_count);
        if (!initialized) {
            const camera_error error = initialized.error();
            allocator.deconstruct_t(CameraSystem, system);
            return err(error);
        }
        active_camera_system = system;
        return ok(system);
    }

    void CameraSystem::destroy(
        mem::Allocator& allocator,
        CameraSystem* system) {
        if (system == nullptr)
            return;
        if (active_camera_system == system)
            active_camera_system = nullptr;
        allocator.deconstruct_t(CameraSystem, system);
    }

    CameraSystem* CameraSystem::current() noexcept {
        return active_camera_system;
    }

    result<void, camera_error> CameraSystem::init(
        mem::Allocator& allocator,
        const u32 max_camera_count) {
        if (m_initialized)
            return err(camera_error{camera_error_code::already_initialized});
        if (max_camera_count == 0)
            return err(camera_error{camera_error_code::capacity_exceeded});

        m_allocator = &allocator;
        if (!m_cameras.arr_init(&allocator, max_camera_count)) {
            shutdown();
            return err(camera_error{camera_error_code::out_of_memory});
        }
        auto references = m_references.map_init(
            &allocator,
            max_camera_count,
            hash_seed::deterministic);
        if (!references) {
            shutdown();
            return err(camera_error{camera_error_code::out_of_memory});
        }

        m_initialized = true;
        auto acquired = acquire(default_camera_name);
        if (!acquired) {
            const camera_error error = acquired.error();
            shutdown();
            return err(error);
        }
        m_default = *acquired;
        m_active = m_default;
        return ok();
    }

    void CameraSystem::shutdown() noexcept {
        m_active = {};
        m_default = {};
        m_loaded_count = 0;
        m_initialized = false;
        if (m_references.allocator() != nullptr)
            (void)m_references.map_shutdown();
        if (m_cameras.allocator() != nullptr)
            (void)m_cameras.arr_shutdown();
        m_allocator = nullptr;
    }

    result<CameraHandle, camera_error> CameraSystem::acquire(
        const strview name) {
        if (!m_initialized)
            return err(camera_error{camera_error_code::not_initialized});
        if (name.empty())
            return err(camera_error{camera_error_code::invalid_name});

        CameraReference* existing = m_references.find(name);
        if (existing != nullptr) {
            ++existing->count;
            return ok(existing->handle);
        }

        const u32 slot_index = find_free_slot();
        if (slot_index == numeric::invalid_id) {
            return err(camera_error{
                camera_error_code::capacity_exceeded,
            });
        }

        CameraSlot& slot = m_cameras[slot_index];
        const CameraHandle handle{
            .index = slot_index,
            .generation = slot.generation == numeric::invalid_id
                ? 0
                : slot.generation,
        };
        str owned_name{*m_allocator};
        if (!owned_name.assign(name))
            return err(camera_error{camera_error_code::out_of_memory});

        auto inserted = m_references.insert(
            std::move(owned_name),
            CameraReference{.handle = handle, .count = 1});
        if (!inserted)
            return err(camera_error{camera_error_code::out_of_memory});

        slot.camera.reset();
        slot.generation = handle.generation;
        slot.occupied = true;
        ++m_loaded_count;
        return ok(handle);
    }

    result<void, camera_error> CameraSystem::release(
        const CameraHandle handle) {
        if (!m_initialized)
            return err(camera_error{camera_error_code::not_initialized});
        CameraSlot* slot = nullptr;
        if (handle.index < m_cameras.length()) {
            CameraSlot& candidate = m_cameras[handle.index];
            if (candidate.occupied && candidate.generation == handle.generation)
                slot = &candidate;
        }
        if (slot == nullptr)
            return err(camera_error{camera_error_code::invalid_handle});
        if (handle == m_default) {
            return err(camera_error{
                camera_error_code::default_camera_release,
            });
        }

        strview registered_name{};
        CameraReference* reference = nullptr;
        for (auto entry : m_references) {
            if (entry.value.handle == handle) {
                registered_name = entry.key.view();
                reference = &entry.value;
                break;
            }
        }
        if (reference == nullptr)
            return err(camera_error{camera_error_code::invalid_handle});

        if (reference->count > 1) {
            --reference->count;
            return ok();
        }

        if (m_active == handle)
            m_active = m_default;
        if (!m_references.remove(registered_name))
            return err(camera_error{camera_error_code::invalid_handle});

        slot->camera.reset();
        slot->generation = next_generation(slot->generation);
        slot->occupied = false;
        --m_loaded_count;
        return ok();
    }

    result<void, camera_error> CameraSystem::set_active(
        const CameraHandle handle) {
        if (!m_initialized)
            return err(camera_error{camera_error_code::not_initialized});
        if (get(handle) == nullptr)
            return err(camera_error{camera_error_code::invalid_handle});
        m_active = handle;
        return ok();
    }

    Camera* CameraSystem::get(const CameraHandle handle) noexcept {
        if (!m_initialized || handle.index >= m_cameras.length())
            return nullptr;
        CameraSlot& slot = m_cameras[handle.index];
        if (!slot.occupied || slot.generation != handle.generation)
            return nullptr;
        return &slot.camera;
    }

    const Camera* CameraSystem::get(const CameraHandle handle) const noexcept {
        return const_cast<CameraSystem*>(this)->get(handle);
    }

    u64 CameraSystem::reference_count(const strview name) const noexcept {
        if (!m_initialized)
            return 0;
        const CameraReference* reference = m_references.find(name);
        return reference == nullptr ? 0 : reference->count;
    }

    u32 CameraSystem::find_free_slot() const noexcept {
        for (u32 index = 0; index < m_cameras.length(); ++index) {
            if (!m_cameras[index].occupied)
                return index;
        }
        return numeric::invalid_id;
    }
}
