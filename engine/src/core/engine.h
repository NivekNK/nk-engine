#pragma once

#include "systems/event_system.h"
#include "core/clock.h"
#include "collections/dyarr.h"
#include "resources/mesh.h"
#include "systems/job_system.h"

namespace nk {
    namespace mem { class Allocator; }
    class App;
    class Platform;
    class Renderer;
    class ResourceSystem;
    class TextureSystem;
    class ShaderSystem;
    class MaterialSystem;
    class GeometrySystem;
    class CameraSystem;
    class JobSystem;
    class TextOverlay;
    struct Material;
    struct Geometry;

    class Engine {
    public:
        ~Engine() = default;

        static bool init() { return get().init_impl(); }

        static void shutdown() { get().shutdown_impl(); }

        static void run() { get().run_impl(); }

        static void exit() { get().exit_impl(); }

        static Engine& get() {
            static Engine instance;
            return instance;
        }

    private:
        Engine() = default;

        bool init_impl();
        void shutdown_impl();
        void run_impl();
        void exit_impl();

        bool update(f64 delta_time);
        bool render(f64 delta_time);
        bool resize(u32 width, u32 height);
        bool run_writable_texture_smoke();
        struct StaticMeshLoad;
        [[nodiscard]] bool queue_static_mesh(
            strview name,
            glm::vec3 position,
            glm::vec3 scale = glm::vec3{1.0f});
        static result<void, job_error> load_static_mesh_cpu(
            StaticMeshLoad& load) noexcept;
        static void complete_static_mesh_load(
            StaticMeshLoad& load,
            const result<void, job_error>& outcome) noexcept;
        void cycle_debug_texture();
        bool set_debug_sampler(u8 preset);
        void cycle_debug_sampler();
        [[nodiscard]] bool assign_pick_id(
            Mesh& mesh,
            PickObjectKind kind = PickObjectKind::geometry);
        void release_pick_ids() noexcept;

        mem::Allocator* m_allocator = nullptr;
        App* m_app = nullptr;
        Platform* m_platform = nullptr;
        Renderer* m_renderer = nullptr;
        ResourceSystem* m_resource_system = nullptr;
        TextureSystem* m_texture_system = nullptr;
        ShaderSystem* m_shader_system = nullptr;
        MaterialSystem* m_material_system = nullptr;
        GeometrySystem* m_geometry_system = nullptr;
        CameraSystem* m_camera_system = nullptr;
        JobSystem* m_job_system = nullptr;
        TextOverlay* m_text_overlay = nullptr;
        cl::dyarr<Mesh> m_test_meshes;
        Geometry* m_test_ui_geometry = nullptr;
        PickId m_test_ui_pick_id{};
        Geometry* m_skybox_geometry = nullptr;
        Material* m_test_material = nullptr;
        u8 m_debug_texture_index = 0;
        u8 m_debug_sampler_index = 0;
        bool m_sampler_demo = false;
        bool m_accept_mesh_publication = false;
        u32 m_pending_mesh_loads = 0;
        u64 m_next_pick_owner = 1;
        u64 m_scene_revision = 1;
        PickId m_last_hover_pick{};
        bool m_pick_pointer_initialized = false;
        bool m_initialized = false;

        Clock m_clock;
        f64 m_last_time = 0.0;

        friend bool on_resized(SystemEventCode, void*, void*, EventContext);
        friend bool on_key(SystemEventCode, void*, void*, EventContext);
    };
}
