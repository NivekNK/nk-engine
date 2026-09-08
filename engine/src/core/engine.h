#pragma once

#include <cstddef>

#include "systems/event_system.h"
#include "core/clock.h"
#include "core/frame_metrics.h"
#include "memory/linear_allocator.h"

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

        bool resize(u32 width, u32 height);
        bool run_writable_texture_smoke();

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
        bool m_app_started = false;
        bool m_initialized = false;

        Clock m_clock;
        FrameMetrics m_frame_metrics;
        static constexpr u64 frame_scratch_capacity = 64 * 1024;
        alignas(std::max_align_t) u8 m_frame_scratch_memory[
            frame_scratch_capacity]{};
        mem::LinearAllocator m_frame_allocator;
        f64 m_last_time = 0.0;

        friend bool on_resized(SystemEventCode, void*, void*, EventContext);
        friend bool on_key(SystemEventCode, void*, void*, EventContext);
    };
}
