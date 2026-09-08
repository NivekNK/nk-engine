#pragma once

#include "core/frame_metrics.h"
#include "core/result.h"
#include "core/strview.h"
#include "renderer/picking.h"
#include "renderer/render_packet.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Camera;
    class CameraSystem;
    class GeometrySystem;
    class JobSystem;
    class MaterialSystem;
    class Platform;
    class Renderer;
    class ResourceSystem;
    class ShaderSystem;
    class TextureSystem;
    namespace mem { class Allocator; }

    struct ApplicationConfig {
        strview name;
        i16 start_pos_x;
        i16 start_pos_y;
        u32 start_width;
        u32 start_height;
    };

    enum class app_error_code : u8 {
        initialization_failed,
        update_failed,
        frame_build_failed,
    };

    struct app_error {
        app_error_code code = app_error_code::initialization_failed;
        i32 native_code = 0;
    };

    class AppServices final {
    public:
        AppServices() noexcept = default;

        [[nodiscard]] mem::Allocator& allocator() const noexcept;
        [[nodiscard]] Platform& platform() const noexcept;
        [[nodiscard]] Renderer& renderer() const noexcept;
        [[nodiscard]] ResourceSystem& resources() const noexcept;
        [[nodiscard]] TextureSystem& textures() const noexcept;
        [[nodiscard]] ShaderSystem& shaders() const noexcept;
        [[nodiscard]] MaterialSystem& materials() const noexcept;
        [[nodiscard]] GeometrySystem& geometries() const noexcept;
        [[nodiscard]] CameraSystem& cameras() const noexcept;
        [[nodiscard]] JobSystem& jobs() const noexcept;

        [[nodiscard]] result<PickId, pick_error> register_pick_object(
            PickObject object) const noexcept;
        [[nodiscard]] result<void, pick_error> release_pick_object(
            PickId id) const noexcept;
        [[nodiscard]] result<u64, pick_error> request_pick(
            f32 logical_x,
            f32 logical_y,
            f32 content_scale,
            u64 scene_revision,
            PickRequestKind kind = PickRequestKind::hover) const noexcept;
        [[nodiscard]] bool poll_pick_result(PickResult& result) const noexcept;
        void discard_pick_scene(u64 scene_revision) const noexcept;

    private:
        AppServices(
            mem::Allocator& allocator,
            Platform& platform,
            Renderer& renderer,
            ResourceSystem& resources,
            TextureSystem& textures,
            ShaderSystem& shaders,
            MaterialSystem& materials,
            GeometrySystem& geometries,
            CameraSystem& cameras,
            JobSystem& jobs) noexcept;

        mem::Allocator* m_allocator = nullptr;
        Platform* m_platform = nullptr;
        Renderer* m_renderer = nullptr;
        ResourceSystem* m_resources = nullptr;
        TextureSystem* m_textures = nullptr;
        ShaderSystem* m_shaders = nullptr;
        MaterialSystem* m_materials = nullptr;
        GeometrySystem* m_geometries = nullptr;
        CameraSystem* m_cameras = nullptr;
        JobSystem* m_jobs = nullptr;

        friend class Engine;
    };

    struct FrameContext {
        f64 delta_time = 0.0;
        u64 frame_number = 0;
        u32 width = 0;
        u32 height = 0;
        f32 content_scale = 1.0f;
        bool benchmark = false;
        Camera* camera = nullptr;
        FrameMetricsSnapshot metrics{};
    };

    class FrameBuilder final {
    public:
        FrameBuilder(mem::Allocator& scratch, const f64 delta_time) noexcept
            : packet{.delta_time = delta_time}, m_scratch{&scratch} {}

        RenderPacket packet{};

        // Engine resets this allocator only after draw_frame has consumed all
        // borrowed packet pointers. Store only frame-lifetime data here.
        [[nodiscard]] mem::Allocator& scratch() const noexcept {
            return *m_scratch;
        }

    private:
        mem::Allocator* m_scratch = nullptr;
    };

    class App {
    public:
        const ApplicationConfig initial_config;

        virtual ~App() = default;

    protected:
        explicit App(const ApplicationConfig config)
            : initial_config{config} {}

        virtual result<void, app_error> initialize(AppServices&) {
            return ok();
        }
        virtual result<void, app_error> update(const FrameContext&) {
            return ok();
        }
        virtual result<void, app_error> build_frame(
            const FrameContext&,
            FrameBuilder&) {
            return ok();
        }
        virtual void on_resized(u32, u32) {}
        virtual void frame_complete(frame_outcome) noexcept {}
        virtual void shutdown() noexcept {}
        [[nodiscard]] virtual bool pending_work() const noexcept {
            return false;
        }

    private:
        static App* create(mem::Allocator* allocator);
        static void destroy(mem::Allocator* allocator, nk::App* app);

        friend class Engine;
    };
}
