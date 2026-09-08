#include "nkpch.h"

#include "core/app.h"

#include "renderer/renderer.h"

namespace nk {
    AppServices::AppServices(
        mem::Allocator& allocator,
        Platform& platform,
        Renderer& renderer,
        ResourceSystem& resources,
        TextureSystem& textures,
        ShaderSystem& shaders,
        MaterialSystem& materials,
        GeometrySystem& geometries,
        CameraSystem& cameras,
        JobSystem& jobs) noexcept
        : m_allocator{&allocator},
          m_platform{&platform},
          m_renderer{&renderer},
          m_resources{&resources},
          m_textures{&textures},
          m_shaders{&shaders},
          m_materials{&materials},
          m_geometries{&geometries},
          m_cameras{&cameras},
          m_jobs{&jobs} {}

    mem::Allocator& AppServices::allocator() const noexcept { return *m_allocator; }
    Platform& AppServices::platform() const noexcept { return *m_platform; }
    Renderer& AppServices::renderer() const noexcept { return *m_renderer; }
    ResourceSystem& AppServices::resources() const noexcept { return *m_resources; }
    TextureSystem& AppServices::textures() const noexcept { return *m_textures; }
    ShaderSystem& AppServices::shaders() const noexcept { return *m_shaders; }
    MaterialSystem& AppServices::materials() const noexcept { return *m_materials; }
    GeometrySystem& AppServices::geometries() const noexcept { return *m_geometries; }
    CameraSystem& AppServices::cameras() const noexcept { return *m_cameras; }
    JobSystem& AppServices::jobs() const noexcept { return *m_jobs; }

    result<PickId, pick_error> AppServices::register_pick_object(
        const PickObject object) const noexcept {
        return m_renderer->register_pick_object(object);
    }

    result<void, pick_error> AppServices::release_pick_object(
        const PickId id) const noexcept {
        return m_renderer->release_pick_object(id);
    }

    result<u64, pick_error> AppServices::request_pick(
        const f32 logical_x,
        const f32 logical_y,
        const f32 content_scale,
        const u64 scene_revision,
        const PickRequestKind kind) const noexcept {
        return m_renderer->request_pick(
            logical_x,
            logical_y,
            content_scale,
            scene_revision,
            kind);
    }

    bool AppServices::poll_pick_result(PickResult& result) const noexcept {
        return m_renderer->poll_pick_result(result);
    }

    void AppServices::discard_pick_scene(
        const u64 scene_revision) const noexcept {
        m_renderer->discard_pick_scene(scene_revision);
    }
}
