#include "nkpch.h"

#include "core/engine.h"
#include "core/render_view_controls.h"
#include "core/thread.h"

#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"
#include "core/app.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "systems/input_system.h"
#include "systems/texture_system.h"
#include "systems/shader_system.h"
#include "systems/material_system.h"
#include "systems/geometry_system.h"
#include "systems/resource_system.h"
#include "systems/camera_system.h"
#include "systems/job_system.h"
#include "resources/static_mesh_resource.h"
#include "text/text_overlay.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/camera.h"

namespace nk {
    struct Engine::StaticMeshLoad {
        mem::MallocAllocator allocator{mem::untracked};
        Resource resource{};
        Engine* engine = nullptr;
        strbuf<255> name;
        glm::vec3 position{0.0f};
        glm::vec3 scale{1.0f};
        resource_error failure{resource_error_code::invalid_data, 0};
    };

    bool on_event(SystemEventCode code, void* sender, void* listener, EventContext context) {
        switch (code) {
            case SystemEventCode::ApplicationQuit: {
                Engine::exit();
                return true;
            }
        }
        return false;
    }

    bool on_key(SystemEventCode code, void* sender, void* listener, EventContext context) {
        if (code == SystemEventCode::KeyPressed) {
            // NOTE: Test code, remove later
            KeyCodeFlag keycode = context.data.u16[0];
            RenderViewMode render_view_mode{};
            if (render_view_mode_from_key(keycode, render_view_mode)) {
                EventContext render_mode_context{};
                render_mode_context.data.u32[0] =
                    static_cast<u32>(render_view_mode);
                return EventSystem::fire_event(
                    SystemEventCode::SetRenderViewMode,
                    nullptr,
                    render_mode_context);
            }
            switch (keycode) {
                case KeyCode::Escape:
                    // Temporary exit shortcut until application-level controls exist.
                    EventSystem::fire_event(SystemEventCode::ApplicationQuit, nullptr, EventContext{});
                    return true;
                case KeyCode::RAlt:
                    DebugLog("'Right Alt' key pressed in window.");
                    break;
                case KeyCode::LAlt:
                    DebugLog("'Left Alt' key pressed in window.");
                    break;
                case KeyCode::RShift:
                    DebugLog("'Right Shift' key pressed in window.");
                    break;
                case KeyCode::LShift:
                    DebugLog("'Left Shift' key pressed in window.");
                    break;
                case KeyCode::RCtrl:
                    DebugLog("'Right Ctrl' key pressed in window.");
                    break;
                case KeyCode::LCtrl:
                    DebugLog("'Left Ctrl' key pressed in window.");
                    break;
                case KeyCode::T: {
                    Engine::get().cycle_debug_texture();
                    return true;
                }
                case KeyCode::P:
                    Engine::get().cycle_debug_sampler();
                    return true;
                default:
                    DebugLog("'{}' key pressed in window.", static_cast<char>(keycode));
                    break;
            }
        } else if (code == SystemEventCode::KeyReleased) {
            // NOTE: Test code, remove later
            KeyCodeFlag keycode = context.data.u16[0];
            RenderViewMode render_view_mode{};
            if (render_view_mode_from_key(keycode, render_view_mode))
                return true;
            switch (keycode) {
                case KeyCode::RAlt:
                    DebugLog("'Right Alt' key released in window.");
                    break;
                case KeyCode::LAlt:
                    DebugLog("'Left Alt' key released in window.");
                    break;
                case KeyCode::RShift:
                    DebugLog("'Right Shift' key released in window.");
                    break;
                case KeyCode::LShift:
                    DebugLog("'Left Shift' key released in window.");
                    break;
                case KeyCode::RCtrl:
                    DebugLog("'Right Ctrl' key released in window.");
                    break;
                case KeyCode::LCtrl:
                    DebugLog("'Left Ctrl' key released in window.");
                    break;
                case KeyCode::T:
                case KeyCode::P:
                case KeyCode::Escape:
                    return true;
                default:
                    DebugLog("'{}' key released in window.", static_cast<char>(keycode));
                    break;
            }
        }
        return false;
    }

    bool on_resized(SystemEventCode code, void* sender, void* listener, EventContext context) {
        if (code != SystemEventCode::Resized)
            return false;

        const u32 width = context.data.u32[0];
        const u32 height = context.data.u32[1];

        Engine& engine = Engine::get();
        return engine.resize(width, height);
    }

    void Engine::exit_impl() {
        if (m_platform != nullptr)
            m_platform->close();
    }

    bool Engine::init_impl() {
        if (m_initialized)
            return true;

        m_allocator = native_construct(mem::MallocAllocator);
        if (m_allocator == nullptr)
            return false;
        if (m_allocator->allocator_init(mem::MallocAllocator, "App", MemoryType::App) == nullptr) {
            native_deconstruct(mem::MallocAllocator, m_allocator);
            m_allocator = nullptr;
            return false;
        }

        m_app = App::create(m_allocator);
        if (m_app == nullptr) {
            shutdown_impl();
            return false;
        }

        const u32 logical_processors = Thread::logical_processor_count();
        const u32 available_workers = logical_processors > 1
            ? logical_processors - 1
            : 1;
        const u32 worker_count = available_workers > 4
            ? 4
            : available_workers;
        auto job_system = JobSystem::create(
            *m_allocator,
            {.worker_count = worker_count, .max_jobs = 1024});
        if (!job_system) {
            ErrorLog(
                "Job system initialization failed: job_error={}, native_code={}",
                static_cast<u32>(job_system.error().code),
                job_system.error().native_code);
            shutdown_impl();
            return false;
        }
        m_job_system = *job_system;

        auto camera_system = CameraSystem::create(*m_allocator);
        if (!camera_system) {
            ErrorLog(
                "Camera system initialization failed: camera_error={}.",
                static_cast<u32>(camera_system.error().code));
            shutdown_impl();
            return false;
        }
        m_camera_system = *camera_system;

        auto resource_system = ResourceSystem::create(*m_allocator);
        if (!resource_system) {
            const resource_error error = resource_system.error();
            ErrorLog(
                "Resource system initialization failed: resource_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_resource_system = *resource_system;

        m_platform = Platform::create(m_allocator, m_app->initial_config);
        if (m_platform == nullptr || !m_platform->running()) {
            shutdown_impl();
            return false;
        }

        auto renderer = Renderer::create(
            m_allocator,
            m_platform,
            m_resource_system,
            m_app->initial_config.name);
        if (!renderer) {
            const renderer_error& error = renderer.error();
            ErrorLog(
                "Renderer initialization failed: renderer_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_renderer = *renderer;

        auto texture_system = TextureSystem::create(
            *m_allocator,
            *m_renderer,
            *m_resource_system,
            TextureSystem::default_max_texture_count,
            m_job_system);
        if (!texture_system) {
            const texture_error error = texture_system.error();
            ErrorLog(
                "Texture system initialization failed: texture_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_texture_system = *texture_system;

        const cstr writable_smoke =
            std::getenv("NK_SMOKE_TEST_WRITABLE_TEXTURE");
        if (writable_smoke != nullptr &&
            std::strcmp(writable_smoke, "1") == 0 &&
            !run_writable_texture_smoke()) {
            shutdown_impl();
            return false;
        }

        auto shader_system = ShaderSystem::create(
            *m_allocator,
            *m_renderer,
            *m_resource_system);
        if (!shader_system) {
            const shader_system_error error = shader_system.error();
            ErrorLog(
                "Shader system initialization failed: shader_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_shader_system = *shader_system;

        auto material_shader =
            m_shader_system->load(builtin_material_shader_name);
        if (!material_shader) {
            const shader_system_error error = material_shader.error();
            ErrorLog(
                "Built-in material shader failed: shader_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        auto ui_shader = m_shader_system->load(builtin_ui_shader_name);
        if (!ui_shader) {
            const shader_system_error error = ui_shader.error();
            ErrorLog(
                "Built-in UI shader failed: shader_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        auto skybox_shader =
            m_shader_system->load(builtin_skybox_shader_name);
        if (!skybox_shader) {
            const shader_system_error error = skybox_shader.error();
            ErrorLog(
                "Built-in skybox shader failed: shader_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }

        auto material_system = MaterialSystem::create(
            *m_allocator,
            *m_shader_system,
            *m_texture_system,
            *m_resource_system);
        if (!material_system) {
            const material_error error = material_system.error();
            ErrorLog(
                "Material system initialization failed: material_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_material_system = *material_system;

        auto geometry_system = GeometrySystem::create(
            *m_allocator,
            *m_renderer,
            *m_material_system);
        if (!geometry_system) {
            const geometry_error error = geometry_system.error();
            ErrorLog(
                "Geometry system initialization failed: geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_geometry_system = *geometry_system;

        MaterialConfig skybox_material{};
        skybox_material.name.assign("skybox_material");
        skybox_material.shader_name.assign(builtin_skybox_shader_name);
        skybox_material.type = MaterialType::skybox;
        skybox_material.diffuse_map_name.assign("skybox");
        skybox_material.diffuse_sampler.wrap_u = TextureWrap::clamp_to_edge;
        skybox_material.diffuse_sampler.wrap_v = TextureWrap::clamp_to_edge;
        skybox_material.diffuse_sampler.wrap_w = TextureWrap::clamp_to_edge;
        auto skybox = GeometrySystem::generate_cube(
            *m_allocator,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            "skybox_geometry",
            skybox_material.name.view());
        if (!skybox) {
            ErrorLog("Skybox geometry generation failed.");
            shutdown_impl();
            return false;
        }
        auto skybox_geometry = m_geometry_system->acquire(
            *skybox,
            skybox_material,
            true);
        if (!skybox_geometry) {
            const geometry_error error = skybox_geometry.error();
            ErrorLog(
                "Skybox creation failed: geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_skybox_geometry = *skybox_geometry;

        if (!m_test_meshes.dyarr_init(m_allocator, 5)) {
            ErrorLog("Test mesh collection allocation failed.");
            shutdown_impl();
            return false;
        }

        auto cube = GeometrySystem::generate_cube(
            *m_allocator,
            10.0f,
            10.0f,
            10.0f,
            1.0f,
            1.0f,
            "test geometry",
            "paving");
        if (!cube) {
            shutdown_impl();
            return false;
        }
        auto first_mesh = Mesh::create(
            *m_allocator,
            *m_geometry_system,
            {&*cube, 1});
        if (!first_mesh) {
            const mesh_error error = first_mesh.error();
            ErrorLog(
                "Test mesh creation failed: mesh_error={}, geometry_index={}, "
                "geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.geometry_index,
                error.geometry_error,
                error.native_code);
            shutdown_impl();
            return false;
        }
        auto first_added = m_test_meshes.dyarr_emplace_back(
            std::move(*first_mesh));
        if (!first_added) {
            ErrorLog("Unable to store the first test mesh.");
            first_mesh->reset();
            shutdown_impl();
            return false;
        }
        if (!assign_pick_id(m_test_meshes.dyarr_last())) {
            shutdown_impl();
            return false;
        }

        auto second_cube = GeometrySystem::generate_cube(
            *m_allocator,
            5.0f,
            5.0f,
            5.0f,
            1.0f,
            1.0f,
            "test geometry 2",
            "cobblestone");
        if (!second_cube) {
            shutdown_impl();
            return false;
        }
        auto second_mesh = Mesh::create(
            *m_allocator,
            *m_geometry_system,
            {&*second_cube, 1});
        if (!second_mesh) {
            const mesh_error error = second_mesh.error();
            ErrorLog(
                "Second test mesh creation failed: mesh_error={}, "
                "geometry_index={}, geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.geometry_index,
                error.geometry_error,
                error.native_code);
            shutdown_impl();
            return false;
        }
        auto second_added = m_test_meshes.dyarr_emplace_back(
            std::move(*second_mesh));
        if (!second_added) {
            ErrorLog("Unable to store the second test mesh.");
            second_mesh->reset();
            shutdown_impl();
            return false;
        }
        if (!assign_pick_id(m_test_meshes.dyarr_last())) {
            shutdown_impl();
            return false;
        }

        auto third_cube = GeometrySystem::generate_cube(
            *m_allocator,
            2.0f,
            2.0f,
            2.0f,
            1.0f,
            1.0f,
            "test geometry 3",
            "paving2");
        if (!third_cube) {
            shutdown_impl();
            return false;
        }
        auto third_mesh = Mesh::create(
            *m_allocator,
            *m_geometry_system,
            {&*third_cube, 1});
        if (!third_mesh) {
            const mesh_error error = third_mesh.error();
            ErrorLog(
                "Third test mesh creation failed: mesh_error={}, "
                "geometry_index={}, geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.geometry_index,
                error.geometry_error,
                error.native_code);
            shutdown_impl();
            return false;
        }
        auto third_added = m_test_meshes.dyarr_emplace_back(
            std::move(*third_mesh));
        if (!third_added) {
            ErrorLog("Unable to store the third test mesh.");
            third_mesh->reset();
            shutdown_impl();
            return false;
        }
        if (!assign_pick_id(m_test_meshes.dyarr_last())) {
            shutdown_impl();
            return false;
        }

        m_accept_mesh_publication = true;
        if (!queue_static_mesh(
                "falcon",
                {15.0f, 0.0f, 1.0f}) ||
            !queue_static_mesh(
                "sponza",
                {15.0f, 0.0f, 1.0f},
                glm::vec3{0.05f})) {
            shutdown_impl();
            return false;
        }

        m_test_meshes[1].transform().set_position({10.0f, 0.0f, 1.0f});
        auto second_parented = m_test_meshes[1].transform().set_parent(
            &m_test_meshes[0].transform());
        if (!second_parented) {
            ErrorLog("Unable to parent the second test mesh.");
            shutdown_impl();
            return false;
        }
        m_test_meshes[2].transform().set_position({5.0f, 0.0f, 1.0f});
        auto third_parented = m_test_meshes[2].transform().set_parent(
            &m_test_meshes[1].transform());
        if (!third_parented) {
            ErrorLog("Unable to parent the third test mesh.");
            shutdown_impl();
            return false;
        }
        m_test_material = m_test_meshes[0].geometry(0)->material;

        Geometry2DConfig ui_config{};
        if (!ui_config.vertices.dyarr_init_list(
                m_allocator,
                {
                    {{0.0f, 0.0f}, {0.0f, 0.0f}},
                    {{512.0f, 512.0f}, {1.0f, 1.0f}},
                    {{0.0f, 512.0f}, {0.0f, 1.0f}},
                    {{512.0f, 0.0f}, {1.0f, 0.0f}},
                }) ||
            !ui_config.indices.dyarr_init_list(
                m_allocator,
                {2u, 1u, 0u, 3u, 0u, 1u})) {
            ErrorLog("Test UI geometry allocation failed.");
            shutdown_impl();
            return false;
        }
        ui_config.name.assign("test_ui_geometry");
        ui_config.material_name.assign("test_ui_material");

        const char* sampler_demo = std::getenv("NK_SAMPLER_DEMO");
        m_sampler_demo = sampler_demo != nullptr && std::strcmp(sampler_demo, "1") == 0;
        if (m_sampler_demo) {
            // Same image/material, two UV regions: outside [0,1] for wrap, and
            // magnified text edges for nearest/linear. No new texture or shader.
            if (!ui_config.vertices.dyarr_resize(8) || !ui_config.indices.dyarr_resize(12)) {
                shutdown_impl();
                return false;
            }
            for (u32 panel = 0; panel < 2; ++panel) {
                const f32 x = panel == 0 ? 16.0f : 528.0f;
                const glm::vec2 uv_min = panel == 0 ? glm::vec2{-.25f, -.25f} : glm::vec2{.70f, .02f};
                const glm::vec2 uv_max = panel == 0 ? glm::vec2{1.25f, 1.25f} : glm::vec2{.765f, .085f};
                ui_config.vertices[panel * 4] = {{x, 16}, uv_min};
                ui_config.vertices[panel * 4 + 1] = {{x + 480, 496}, uv_max};
                ui_config.vertices[panel * 4 + 2] = {{x, 496}, {uv_min.x, uv_max.y}};
                ui_config.vertices[panel * 4 + 3] = {{x + 480, 16}, {uv_max.x, uv_min.y}};
                constexpr u32 indices[]{2, 1, 0, 3, 0, 1};
                for (u32 i = 0; i < 6; ++i) ui_config.indices[panel * 6 + i] = indices[i] + panel * 4;
            }
        }

        auto test_ui_geometry = m_geometry_system->acquire(ui_config, true);
        if (!test_ui_geometry) {
            const geometry_error error = test_ui_geometry.error();
            ErrorLog(
                "Test UI geometry creation failed: geometry_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_test_ui_geometry = *test_ui_geometry;
        auto ui_pick = m_renderer->register_pick_object({
            .kind = PickObjectKind::ui,
            .owner = m_next_pick_owner++,
        });
        if (!ui_pick) {
            ErrorLog("Unable to register the test UI pick identity: {}.",
                static_cast<u32>(ui_pick.error()));
            shutdown_impl();
            return false;
        }
        m_test_ui_pick_id = *ui_pick;
        if (m_sampler_demo && !set_debug_sampler(0)) {
            shutdown_impl();
            return false;
        }

        const cstr text_overlay = std::getenv("NK_TEXT_OVERLAY");
        if (text_overlay == nullptr || std::strcmp(text_overlay, "0") != 0) {
            m_text_overlay = m_allocator->construct_t(TextOverlay);
            if (m_text_overlay == nullptr) { shutdown_impl(); return false; }
            auto initialized = m_text_overlay->init(*m_allocator, *m_renderer,
                *m_shader_system, m_resource_system->asset_base_path(), m_platform->content_scale());
            if (!initialized) {
                ErrorLog("Text overlay initialization failed: {}", static_cast<u32>(initialized.error()));
                shutdown_impl(); return false;
            }
        }
        m_clock.init(m_platform);

        EventSystem::register_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::register_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::register_event(SystemEventCode::KeyReleased, nullptr, on_key);
        EventSystem::register_event(SystemEventCode::Resized, nullptr, on_resized);
        EventSystem::register_event(
            SystemEventCode::SetRenderViewMode,
            m_renderer,
            on_render_view_mode);

        m_initialized = true;
        return true;
    }

    void Engine::shutdown_impl() {
        m_accept_mesh_publication = false;
        if (m_job_system != nullptr)
            m_job_system->shutdown();
        if (m_initialized) {
            EventSystem::unregister_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
            EventSystem::unregister_event(SystemEventCode::KeyPressed, nullptr, on_key);
            EventSystem::unregister_event(SystemEventCode::KeyReleased, nullptr, on_key);
            EventSystem::unregister_event(SystemEventCode::Resized, nullptr, on_resized);
            EventSystem::unregister_event(
                SystemEventCode::SetRenderViewMode,
                m_renderer,
                on_render_view_mode);
        }

        release_pick_ids();
        if (m_test_meshes.allocator() != nullptr)
            (void)m_test_meshes.dyarr_shutdown();
        m_test_material = nullptr;
        if (m_text_overlay) {
            const auto stats = m_text_overlay->statistics();
            InfoLog("Text: {} shape calls, {} cached glyphs, {} atlas pages ({} bytes)",
                stats.shape_calls, stats.rasterized_glyphs, stats.pages, stats.atlas_bytes);
            m_allocator->deconstruct_t(TextOverlay, m_text_overlay);
            m_text_overlay = nullptr;
        }
        if (m_geometry_system != nullptr) {
            if (m_skybox_geometry != nullptr) {
                m_geometry_system->release(m_skybox_geometry);
                m_skybox_geometry = nullptr;
            }
            GeometrySystem::destroy(*m_allocator, m_geometry_system);
            m_geometry_system = nullptr;
            m_test_ui_geometry = nullptr;
        }
        if (m_material_system != nullptr) {
            MaterialSystem::destroy(*m_allocator, m_material_system);
            m_material_system = nullptr;
        }
        if (m_shader_system != nullptr) {
            ShaderSystem::destroy(*m_allocator, m_shader_system);
            m_shader_system = nullptr;
        }
        if (m_texture_system != nullptr) {
            TextureSystem::destroy(*m_allocator, m_texture_system);
            m_texture_system = nullptr;
        }
        if (m_renderer != nullptr) {
            Renderer::destroy(m_allocator, m_renderer);
            m_renderer = nullptr;
        }
        if (m_resource_system != nullptr) {
            ResourceSystem::destroy(*m_allocator, m_resource_system);
            m_resource_system = nullptr;
        }
        if (m_camera_system != nullptr) {
            CameraSystem::destroy(*m_allocator, m_camera_system);
            m_camera_system = nullptr;
        }
        if (m_job_system != nullptr) {
            JobSystem::destroy(*m_allocator, m_job_system);
            m_job_system = nullptr;
        }
        if (m_platform != nullptr) {
            Platform::destroy(m_allocator, m_platform);
            m_platform = nullptr;
        }
        if (m_app != nullptr) {
            App::destroy(m_allocator, m_app);
            m_app = nullptr;
        }
        if (m_allocator != nullptr) {
            native_deconstruct(mem::MallocAllocator, m_allocator);
            m_allocator = nullptr;
        }
        m_initialized = false;
        m_pending_mesh_loads = 0;
    }

    bool Engine::queue_static_mesh(
        const strview name,
        const glm::vec3 position,
        const glm::vec3 scale) {
        if (m_job_system == nullptr || m_resource_system == nullptr ||
            name.empty()) {
            return false;
        }
        StaticMeshLoad load{};
        load.engine = this;
        load.position = position;
        load.scale = scale;
        if (!load.name.assign(name))
            return false;
        auto submitted = m_job_system->submit(
            JobPriority::high,
            std::move(load),
            &Engine::load_static_mesh_cpu,
            &Engine::complete_static_mesh_load);
        if (!submitted) {
            ErrorLog(
                "Static mesh '{}' could not be queued: job_error={}, native_code={}",
                name,
                static_cast<u32>(submitted.error().code),
                submitted.error().native_code);
            return false;
        }
        ++m_pending_mesh_loads;
        InfoLog("Static mesh '{}' queued for asynchronous loading.", name);
        return true;
    }

    result<void, job_error> Engine::load_static_mesh_cpu(
        StaticMeshLoad& load) noexcept {
        auto loaded = load.engine->m_resource_system->load_detached(
            load.allocator,
            load.name.view(),
            ResourceType::static_mesh);
        if (!loaded) {
            load.failure = loaded.error();
            return err(job_error{
                job_error_code::execution_failed,
                load.failure.native_code,
            });
        }
        std::destroy_at(std::addressof(load.resource));
        std::construct_at(
            std::addressof(load.resource),
            std::move(*loaded));
        return ok();
    }

    void Engine::complete_static_mesh_load(
        StaticMeshLoad& load,
        const result<void, job_error>& outcome) noexcept {
        Engine* engine = load.engine;
        if (engine == nullptr)
            return;
        if (engine->m_pending_mesh_loads != 0)
            --engine->m_pending_mesh_loads;

        if (outcome && engine->m_accept_mesh_publication &&
            load.resource.data != nullptr) {
            auto mesh = Mesh::create(
                *engine->m_allocator,
                *engine->m_geometry_system,
                *load.resource.as<StaticMeshResource>());
            if (mesh) {
                mesh->transform().set_position(load.position);
                mesh->transform().set_scale(load.scale);
                auto added = engine->m_test_meshes.dyarr_emplace_back(
                    std::move(*mesh));
                if (added) {
                    if (!engine->assign_pick_id(
                            engine->m_test_meshes.dyarr_last())) {
                        ErrorLog(
                            "Static mesh '{}' has no pick identity.",
                            load.name.view());
                    }
                    ++engine->m_scene_revision;
                    engine->m_renderer->discard_pick_scene(
                        engine->m_scene_revision);
                    InfoLog(
                        "Static mesh '{}' published with {} material group(s).",
                        load.name.view(),
                        engine->m_test_meshes.dyarr_last().geometry_count());
                } else {
                    mesh->reset();
                    ErrorLog("Unable to publish static mesh '{}'.", load.name.view());
                }
            } else {
                const mesh_error error = mesh.error();
                ErrorLog(
                    "Static mesh '{}' publication failed: mesh_error={}, geometry_index={}, geometry_error={}, native_code={}",
                    load.name.view(),
                    static_cast<u32>(error.code),
                    error.geometry_index,
                    error.geometry_error,
                    error.native_code);
            }
        } else if (!outcome &&
                   outcome.error().code != job_error_code::cancelled) {
            ErrorLog(
                "Static mesh '{}' asynchronous load failed: resource_error={}, native_code={}",
                load.name.view(),
                static_cast<u32>(load.failure.code),
                load.failure.native_code);
        }

        if (load.resource.data != nullptr) {
            auto unloaded = engine->m_resource_system->unload_detached(
                load.allocator,
                load.resource);
            if (!unloaded) {
                ErrorLog(
                    "Static mesh '{}' detached payload cleanup failed: resource_error={}",
                    load.name.view(),
                    static_cast<u32>(unloaded.error().code));
            }
        }
    }

    bool Engine::assign_pick_id(
        Mesh& mesh,
        const PickObjectKind kind) {
        if (m_renderer == nullptr)
            return false;
        auto registered = m_renderer->register_pick_object({
            .kind = kind,
            .owner = m_next_pick_owner++,
        });
        if (!registered) {
            ErrorLog(
                "Unable to register mesh pick identity: {}.",
                static_cast<u32>(registered.error()));
            return false;
        }
        mesh.set_pick_id(*registered);
        return true;
    }

    void Engine::release_pick_ids() noexcept {
        if (m_renderer != nullptr) {
            for (u64 index = 0; index < m_test_meshes.length(); ++index) {
                const PickId id = m_test_meshes[index].pick_id();
                if (id.valid())
                    (void)m_renderer->release_pick_object(id);
                m_test_meshes[index].set_pick_id({});
            }
            if (m_test_ui_pick_id.valid())
                (void)m_renderer->release_pick_object(m_test_ui_pick_id);
        }
        m_test_ui_pick_id = {};
        m_last_hover_pick = {};
        m_pick_pointer_initialized = false;
    }

    void Engine::cycle_debug_texture() {
        if (m_material_system == nullptr || m_test_material == nullptr)
            return;

        struct DebugTextureSet {
            strview diffuse;
            strview specular;
            strview normal;
        };
        constexpr DebugTextureSet texture_sets[]{
            {{"cobblestone", 11}, {"cobblestone_SPEC", 16},
             {"cobblestone_NRM", 15}},
            {{"paving", 6}, {"paving_SPEC", 11}, {"paving_NRM", 10}},
            {{"paving2", 7}, {"paving2_SPEC", 12}, {"paving2_NRM", 11}},
        };
        constexpr u8 texture_count =
            sizeof(texture_sets) / sizeof(texture_sets[0]);
        const DebugTextureSet& next = texture_sets[m_debug_texture_index];

        auto changed = m_material_system->set_texture_maps(
            *m_test_material,
            next.diffuse,
            next.specular,
            next.normal);
        if (!changed) {
            const material_error error = changed.error();
            ErrorLog(
                "Texture map cycle failed: material_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            return;
        }

        m_debug_texture_index =
            static_cast<u8>((m_debug_texture_index + 1) % texture_count);
    }

    bool Engine::run_writable_texture_smoke() {
        constexpr strview name{"__nk_writable_texture_smoke", 27};
        auto acquired = m_texture_system->acquire_writable(
            name,
            4,
            4,
            4,
            true);
        if (!acquired) {
            ErrorLog(
                "Writable texture smoke create failed: texture_error={}, native_code={}",
                static_cast<u32>(acquired.error().code),
                acquired.error().native_code);
            return false;
        }

        Texture* texture = *acquired;
        u8 initial_pixels[4 * 4 * 4]{};
        for (u32 index = 0; index < 4 * 4; ++index) {
            initial_pixels[index * 4] = static_cast<u8>(index * 13);
            initial_pixels[index * 4 + 1] = static_cast<u8>(255 - index * 11);
            initial_pixels[index * 4 + 2] = 127;
            initial_pixels[index * 4 + 3] = 255;
        }
        auto written = m_texture_system->write(
            *texture,
            {0, 0, 4, 4},
            cl::slice<const u8>{initial_pixels});
        if (!written) {
            ErrorLog(
                "Writable texture smoke upload failed: texture_error={}, native_code={}",
                static_cast<u32>(written.error().code),
                written.error().native_code);
            m_texture_system->release(name);
            return false;
        }

        auto resized = m_texture_system->resize(*texture, 8, 4);
        if (!resized) {
            ErrorLog(
                "Writable texture smoke resize failed: texture_error={}, native_code={}",
                static_cast<u32>(resized.error().code),
                resized.error().native_code);
            m_texture_system->release(name);
            return false;
        }

        u8 region_pixels[4 * 2 * 4]{};
        for (u32 index = 0; index < 4 * 2; ++index) {
            region_pixels[index * 4] = 255;
            region_pixels[index * 4 + 1] = 64;
            region_pixels[index * 4 + 2] = 192;
            region_pixels[index * 4 + 3] = 255;
        }
        written = m_texture_system->write(
            *texture,
            {2, 1, 4, 2},
            cl::slice<const u8>{region_pixels});
        if (!written) {
            ErrorLog(
                "Writable texture smoke region upload failed: texture_error={}, native_code={}",
                static_cast<u32>(written.error().code),
                written.error().native_code);
            m_texture_system->release(name);
            return false;
        }

        const u32 generation = texture->generation;
        m_texture_system->release(name);
        InfoLog(
            "Writable texture smoke passed (create, full upload, resize, region upload; generation {}).",
            generation);
        return true;
    }

    bool Engine::set_debug_sampler(u8 preset) {
        if (preset >= 5 || m_test_ui_geometry == nullptr || m_test_ui_geometry->material == nullptr)
            return false;
        SamplerConfig sampling;
        sampling.anisotropy = 1;
        constexpr cstr names[]{"linear / repeat", "nearest / repeat", "nearest / mirrored_repeat",
            "linear / clamp_to_edge", "linear / clamp_to_border"};
        if (preset == 1 || preset == 2)
            sampling.min_filter = sampling.mag_filter = sampling.mip_filter = TextureFilter::nearest;
        if (preset == 2) sampling.wrap_u = sampling.wrap_v = sampling.wrap_w = TextureWrap::mirrored_repeat;
        else if (preset == 3) sampling.wrap_u = sampling.wrap_v = sampling.wrap_w = TextureWrap::clamp_to_edge;
        else if (preset == 4) sampling.wrap_u = sampling.wrap_v = sampling.wrap_w = TextureWrap::clamp_to_border;
        auto changed = m_material_system->set_sampler(*m_test_ui_geometry->material, TextureUse::diffuse, sampling);
        if (!changed) {
            ErrorLog("Sampler demo update failed: material_error={}, native_code={}",
                static_cast<u32>(changed.error().code), changed.error().native_code);
            return false;
        }
        m_debug_sampler_index = preset;
        InfoLog("Sampler demo {}: {}. Left: wrap; right: magnified filtering. Press P to cycle.", preset, names[preset]);
        return true;
    }

    void Engine::cycle_debug_sampler() {
        if (!m_sampler_demo) {
            InfoLog("Enable NK_SAMPLER_DEMO=1 to inspect wrap and filtering with P.");
            return;
        }
        (void)set_debug_sampler(static_cast<u8>((m_debug_sampler_index + 1) % 5));
    }

    void Engine::run_impl() {
        if (!m_initialized)
            return;

        m_clock.start();
        m_clock.update();
        m_last_time = m_clock.elapsed();

        u64 frame_count = 0;
        f64 target_frame_seconds = 1.0f / 60;
        u64 smoke_test_frames = 0;
        bool smoke_test_cycle_render_modes = false;
        const char* cycle_samplers = std::getenv("NK_SMOKE_TEST_CYCLE_SAMPLERS");
        const bool smoke_test_cycle_samplers = m_sampler_demo && cycle_samplers != nullptr &&
            std::strcmp(cycle_samplers, "1") == 0;
        const cstr benchmark_option = std::getenv("NK_BENCHMARK");
        const bool benchmark = benchmark_option != nullptr &&
            std::strcmp(benchmark_option, "1") == 0;
        constexpr u64 benchmark_warmup = 60;
        u64 benchmark_frames = 0;
        u64 benchmark_gpu_samples = 0;
        f64 benchmark_seconds = 0.0;
        f64 benchmark_gpu_ms = 0.0;
        f64 benchmark_max_ms = 0.0;
        m_frame_metrics.reset();
#if NK_MEMORY_TRACKING_ENABLED
        u64 stable_frame_allocation_events = 0;
        u64 stable_frames_checked = 0;
        bool stable_frame_warmed_up = false;
#endif
        if (const cstr configured_frames = std::getenv("NK_SMOKE_TEST_FRAMES");
            configured_frames != nullptr) {
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(configured_frames, &end, 10);
            if (end != configured_frames && *end == '\0')
                smoke_test_frames = static_cast<u64>(parsed);
        }
        if (const cstr configured_modes =
                std::getenv("NK_SMOKE_TEST_CYCLE_RENDER_MODES");
            configured_modes != nullptr && configured_modes[0] == '1' &&
            configured_modes[1] == '\0') {
            smoke_test_cycle_render_modes = true;
        }

        SceneLighting scene_lighting{};
        scene_lighting.directional.color = {0.6f, 0.6f, 0.6f, 1.0f};
        scene_lighting.point_lights[0] = {
            .position = {-5.5f, 0.0f, -5.5f},
            .color = {0.0f, 1.0f, 0.0f, 1.0f},
            .constant = 1.0f,
            .linear = 0.35f,
            .quadratic = 0.44f,
        };
        scene_lighting.point_lights[1] = {
            .position = {5.5f, 0.0f, -5.5f},
            .color = {1.0f, 0.0f, 0.0f, 1.0f},
            .constant = 1.0f,
            .linear = 0.35f,
            .quadratic = 0.44f,
        };
        scene_lighting.point_light_count = max_point_light_count;

        while (m_platform->running()) {
            if (!m_platform->pump_messages()) {
                m_platform->close();
            }

            // Quit requests from input/window events must not start another frame.
            if (!m_platform->running())
                break;

            if (!m_platform->suspended()) {
                if (smoke_test_cycle_samplers && smoke_test_frames >= 5) {
                    const u8 preset = static_cast<u8>(glm::min(u64{4}, frame_count / (smoke_test_frames / 5)));
                    if (preset != m_debug_sampler_index && !set_debug_sampler(preset)) {
                        m_platform->close();
                        break;
                    }
                }
                if (smoke_test_cycle_render_modes && smoke_test_frames >= 3) {
                    const u64 mode_segment = smoke_test_frames / 3;
                    RenderViewMode desired_mode =
                        RenderViewMode::default_lit;
                    if (frame_count >= mode_segment * 2)
                        desired_mode = RenderViewMode::normals;
                    else if (frame_count >= mode_segment)
                        desired_mode = RenderViewMode::lighting_only;

                    if (m_renderer->render_view_mode() != desired_mode) {
                        EventContext mode_context{};
                        mode_context.data.u32[0] =
                            static_cast<u32>(desired_mode);
                        EventSystem::fire_event(
                            SystemEventCode::SetRenderViewMode,
                            nullptr,
                            mode_context);
                    }
                }
#if NK_MEMORY_TRACKING_ENABLED
                const u64 frame_allocation_baseline =
                    mem::MemorySystem::get().allocation_event_count();
#endif
                // Update clock and get delta time
                m_clock.update();
                f64 current_time = m_clock.elapsed();
                f64 delta = current_time - m_last_time;
                f64 frame_start_time = m_platform->get_absolute_time();

                if (!update(delta)) {
                    FatalLog("nk::App::run update failed. shutting douwn.");
                    m_platform->close();
                    break;
                }
                const f64 update_end_time =
                    m_platform->get_absolute_time();

                if (!render(delta)) {
                    FatalLog("nk::App::run render failed. shutting douwn.");
                    m_platform->close();
                    break;
                }
                if (!benchmark && !m_test_meshes.empty()) {
                    const glm::quat rotation = glm::angleAxis(
                        static_cast<f32>(0.5 * delta),
                        glm::vec3{0.0f, 1.0f, 0.0f});
                    constexpr u64 rotating_demo_mesh_count = 3;
                    for (u64 index = 0;
                         index < rotating_demo_mesh_count &&
                             index < m_test_meshes.length();
                         ++index) {
                        m_test_meshes[index].transform().rotate(rotation);
                    }
                }
                GeometryRenderData ui_geometry{
                    .model = glm::mat4{1.0f},
                    .geometry = m_test_ui_geometry,
                    .pick_id = m_test_ui_pick_id,
                };
                if (m_sampler_demo) {
                    const f32 scale = glm::min(1.0f, glm::min(
                        static_cast<f32>(m_platform->width()) / 1024.0f,
                        static_cast<f32>(m_platform->height()) / 512.0f));
                    ui_geometry.model = glm::scale(glm::mat4{1.0f}, glm::vec3{scale, scale, 1});
                }
                const TextFrame* text_frame = nullptr;
                if (m_text_overlay) {
                    auto prepared = m_text_overlay->frame(m_platform->width(), m_platform->height(),
                        m_platform->content_scale(), delta, m_frame_metrics.snapshot());
                    if (!prepared) {
                        ErrorLog("Text overlay failed: {}", static_cast<u32>(prepared.error()));
                        m_platform->close(); break;
                    }
                    text_frame = *prepared;
                }

                i16 mouse_x = 0;
                i16 mouse_y = 0;
                i16 previous_mouse_x = 0;
                i16 previous_mouse_y = 0;
                Input::get_mouse_position(mouse_x, mouse_y);
                Input::get_previous_mouse_position(
                    previous_mouse_x,
                    previous_mouse_y);
                const bool mouse_moved = !m_pick_pointer_initialized ||
                    mouse_x != previous_mouse_x ||
                    mouse_y != previous_mouse_y;
                const bool mouse_clicked =
                    Input::is_mouse_button_down(MouseButton::Left) &&
                    Input::was_mouse_button_up(MouseButton::Left);
                if (mouse_moved || mouse_clicked) {
                    auto requested = m_renderer->request_pick(
                        static_cast<f32>(mouse_x),
                        static_cast<f32>(mouse_y),
                        m_platform->content_scale(),
                        m_scene_revision,
                        mouse_clicked
                            ? PickRequestKind::click
                            : PickRequestKind::hover);
                    if (!requested &&
                        requested.error() != pick_error::queue_full) {
                        WarnLog(
                            "Pick request rejected: {}.",
                            static_cast<u32>(requested.error()));
                    }
                    m_pick_pointer_initialized = true;
                }
                const f64 render_start_time =
                    m_platform->get_absolute_time();
                auto frame = m_renderer->draw_frame(*m_material_system, {
                    .delta_time = delta,
                    .lighting = scene_lighting,
                    .skybox_geometry = {
                        .model = glm::mat4{1.0f},
                        .geometry = m_skybox_geometry,
                    },
                    .mesh_count = static_cast<u32>(m_test_meshes.length()),
                    .meshes = m_test_meshes.data(),
                    .ui_geometry_count =
                        m_test_ui_geometry == nullptr ? 0u : 1u,
                    .ui_geometries = &ui_geometry,
                    .text = text_frame,
                });
                if (!frame) {
                    const renderer_error& error = frame.error();
                    ErrorLog(
                        "Rendering failed: renderer_error={}, native_code={}",
                        static_cast<u32>(error.code),
                        error.native_code);
                    m_platform->close();
                    break;
                }
                const f64 render_end_time =
                    m_platform->get_absolute_time();
                if (m_text_overlay && *frame == frame_outcome::rendered)
                    m_text_overlay->acknowledge_frame();

                PickResult pick_result{};
                PickingFrameLatency picking_timing{};
                if (m_renderer->poll_pick_result(pick_result)) {
                    picking_timing = {
                        .request_frame = pick_result.request.submitted_frame,
                        .retired_frame = pick_result.retired_frame,
                        .frames = static_cast<f64>(
                            pick_result.retired_frame -
                            pick_result.request.submitted_frame),
                    };
                    if (pick_result.request.kind == PickRequestKind::click) {
                        InfoLog(
                            "Pick click {}: kind={}, owner={}, retired frame={}.",
                            pick_result.request.sequence,
                            static_cast<u32>(pick_result.object.kind),
                            pick_result.object.owner,
                            pick_result.retired_frame);
                    } else if (pick_result.id != m_last_hover_pick) {
                        m_last_hover_pick = pick_result.id;
                        DebugLog(
                            "Pick hover changed: kind={}, owner={}.",
                            static_cast<u32>(pick_result.object.kind),
                            pick_result.object.owner);
                    }
                }

                // Figure out how long the frame took
                f64 frame_end_time = m_platform->get_absolute_time();
                f64 frame_elapsed_time = frame_end_time - frame_start_time;
                m_frame_metrics.push({
                    .frame_number = frame_count,
                    .frame_ms = delta * 1000.0,
                    .cpu_ms = frame_elapsed_time * 1000.0,
                    .update_ms = (update_end_time - frame_start_time) * 1000.0,
                    .build_ms = (render_start_time - update_end_time) * 1000.0,
                    .render_ms = (render_end_time - render_start_time) * 1000.0,
                    .gpu = m_renderer->gpu_frame_timing(),
                    .picking = picking_timing,
                    .draw = m_renderer->frame_draw_counters(),
                });
                const FrameMetricsSnapshot metrics =
                    m_frame_metrics.snapshot();
                if (benchmark && frame_count >= benchmark_warmup &&
                    *frame == frame_outcome::rendered) {
                    ++benchmark_frames;
                    benchmark_seconds += metrics.latest.frame_ms / 1000.0;
                    if (metrics.latest.frame_ms > benchmark_max_ms)
                        benchmark_max_ms = metrics.latest.frame_ms;
                    if (metrics.latest.gpu.valid()) {
                        ++benchmark_gpu_samples;
                        benchmark_gpu_ms +=
                            metrics.latest.gpu.milliseconds;
                    }
                }
                f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

                if (remaining_seconds > 0) {
                    u64 remaining_ms = remaining_seconds * 1000;

                    // If there is time left, give it back to the OS
                    bool limit_frames = false;
                    if (remaining_ms > 0 && limit_frames) {
                        m_platform->sleep(remaining_ms - 1);
                    }

                }

                InputSystem::update(delta);
                ++frame_count;
#if NK_MEMORY_TRACKING_ENABLED
                const bool stable_scene_ready = m_pending_mesh_loads == 0 &&
                    (m_job_system == nullptr || m_job_system->pending_count() == 0);
                if (smoke_test_frames > 1 && *frame == frame_outcome::rendered &&
                    stable_scene_ready) {
                    if (stable_frame_warmed_up) {
                        stable_frame_allocation_events +=
                            mem::MemorySystem::get().allocation_event_count() -
                            frame_allocation_baseline;
                        ++stable_frames_checked;
                    } else {
                        stable_frame_warmed_up = true;
                    }
                } else if (!stable_scene_ready) {
                    // The frame that publishes the final asynchronous result
                    // can allocate before pending_count reaches zero. Exclude
                    // it, and warm the now-stable scene on the next frame.
                    stable_frame_warmed_up = false;
                }
#endif
                if (smoke_test_frames != 0 && frame_count >= smoke_test_frames)
                    m_platform->close();

                // Update last time
                m_last_time = current_time;
            } else {
                // Event pumps are deliberately non-blocking on XCB and
                // Wayland. Back off only while rendering is suspended so a
                // minimized/zero-extent window cannot consume a full CPU core.
                m_platform->sleep(16);
            }
        }

#if NK_MEMORY_TRACKING_ENABLED
        if (smoke_test_frames > 1) {
            if (stable_frame_allocation_events == 0) {
                InfoLog(
                    "Stable-frame allocation check: 0 allocation events across {} checked frame(s).",
                    stable_frames_checked);
            } else {
                ErrorLog(
                    "Stable-frame allocation check: {} unexpected allocation event(s) across {} checked frame(s).",
                    stable_frame_allocation_events,
                    stable_frames_checked);
            }
        }
#endif

        if (benchmark && benchmark_frames != 0) {
            InfoLog("Benchmark: {}x{}, frames={}, warmup={}, avg_ms={:.3f}, fps={:.2f}, max_ms={:.3f}, gpu_ms={:.3f}, gpu_samples={}",
                m_platform->width(), m_platform->height(), benchmark_frames,
                benchmark_warmup, benchmark_seconds * 1000.0 / benchmark_frames,
                benchmark_frames / benchmark_seconds, benchmark_max_ms,
                benchmark_gpu_samples ? benchmark_gpu_ms / benchmark_gpu_samples : -1.0,
                benchmark_gpu_samples);
        }
        m_platform->close();
    }

    bool Engine::update(f64 delta_time) {
        if (m_job_system != nullptr) {
            auto jobs_updated = m_job_system->update();
            if (!jobs_updated) {
                ErrorLog(
                    "Job completion update failed: job_error={}, native_code={}",
                    static_cast<u32>(jobs_updated.error().code),
                    jobs_updated.error().native_code);
                return false;
            }
        }
        if (!m_app->update(delta_time))
            return false;
        Camera* camera = m_camera_system == nullptr
            ? nullptr
            : m_camera_system->active();
        if (camera == nullptr)
            return false;
        m_renderer->set_view(camera->view(), camera->position());
        return true;
    }

    bool Engine::render(f64 delta_time) {
        return m_app->render(delta_time);
    }

    bool Engine::resize(u32 width, u32 height) {
        if (width != m_platform->width() || height != m_platform->height()) {
            m_platform->on_resized(width, height);
            if (width == 0 || height == 0) {
                InfoLog("nk::Engine::on_resized Window minimized, suspending application.");
                m_platform->set_suspended(true);
            } else {
                if (m_platform->suspended()) {
                    InfoLog("nk::Engine::on_resized Window restored, resuming application.");
                    m_platform->set_suspended(false);
                }
                m_app->on_resized(width, height);
                m_renderer->resize(width, height);
            }
            return true;
        }
        return false;
    }
}
