#include "nkpch.h"

#include "core/engine.h"
#include "core/render_view_controls.h"

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
#include "resources/static_mesh_resource.h"

#include <glm/gtc/quaternion.hpp>

// TODO: Temporal include
#include "core/camera.h"

namespace nk {
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
            *m_resource_system);
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

        const auto load_static_mesh = [this](const strview name) {
            auto resource = m_resource_system->load(
                name,
                ResourceType::static_mesh);
            if (!resource) {
                const resource_error error = resource.error();
                ErrorLog(
                    "Static mesh '{}' failed to load: resource_error={}, "
                    "native_code={}",
                    name,
                    static_cast<u32>(error.code),
                    error.native_code);
                return false;
            }

            auto mesh = Mesh::create(
                *m_allocator,
                *m_geometry_system,
                *resource->as<StaticMeshResource>());
            auto unloaded = m_resource_system->unload(*resource);
            if (!mesh) {
                const mesh_error error = mesh.error();
                ErrorLog(
                    "Static mesh '{}' failed to create: mesh_error={}, "
                    "geometry_index={}, geometry_error={}, native_code={}",
                    name,
                    static_cast<u32>(error.code),
                    error.geometry_index,
                    error.geometry_error,
                    error.native_code);
                return false;
            }
            if (!unloaded) {
                ErrorLog(
                    "Static mesh resource '{}' failed to unload: "
                    "resource_error={}, native_code={}",
                    name,
                    static_cast<u32>(unloaded.error().code),
                    unloaded.error().native_code);
                return false;
            }

            auto added = m_test_meshes.dyarr_emplace_back(std::move(*mesh));
            if (!added) {
                ErrorLog("Unable to store static mesh '{}'.", name);
                mesh->reset();
                return false;
            }
            InfoLog(
                "Static mesh '{}' created with {} material group(s).",
                name,
                m_test_meshes.dyarr_last().geometry_count());
            return true;
        };

        if (!load_static_mesh("falcon") || !load_static_mesh("sponza")) {
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
        m_test_meshes[3].transform().set_position({15.0f, 0.0f, 1.0f});
        m_test_meshes[4].transform().set_position({15.0f, 0.0f, 1.0f});
        m_test_meshes[4].transform().set_scale(glm::vec3{0.05f});
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

        Camera::init(m_renderer);

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

        if (m_test_meshes.allocator() != nullptr)
            (void)m_test_meshes.dyarr_shutdown();
        m_test_material = nullptr;
        if (m_geometry_system != nullptr) {
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

    void Engine::run_impl() {
        if (!m_initialized)
            return;

        m_clock.start();
        m_clock.update();
        m_last_time = m_clock.elapsed();

        f64 running_time = 0;
        u64 frame_count = 0;
        f64 target_frame_seconds = 1.0f / 60;
        u64 smoke_test_frames = 0;
        bool smoke_test_cycle_render_modes = false;
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

            if (!m_platform->suspended()) {
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

                if (!render(delta)) {
                    FatalLog("nk::App::run render failed. shutting douwn.");
                    m_platform->close();
                    break;
                }

                if (!m_test_meshes.empty()) {
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
                };
                auto frame = m_renderer->draw_frame(*m_material_system, {
                    .delta_time = delta,
                    .lighting = scene_lighting,
                    .mesh_count = static_cast<u32>(m_test_meshes.length()),
                    .meshes = m_test_meshes.data(),
                    .ui_geometry_count =
                        m_test_ui_geometry == nullptr ? 0u : 1u,
                    .ui_geometries = &ui_geometry,
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

                // Figure out how long the frame took
                f64 frame_end_time = m_platform->get_absolute_time();
                f64 frame_elapsed_time = frame_end_time - frame_start_time;
                running_time += frame_elapsed_time;
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
                if (smoke_test_frames > 1 &&
                    *frame == frame_outcome::rendered) {
                    if (stable_frame_warmed_up) {
                        stable_frame_allocation_events +=
                            mem::MemorySystem::get().allocation_event_count() -
                            frame_allocation_baseline;
                        ++stable_frames_checked;
                    } else {
                        stable_frame_warmed_up = true;
                    }
                }
#endif
                if (smoke_test_frames != 0 && frame_count >= smoke_test_frames)
                    m_platform->close();

                // Update last time
                m_last_time = current_time;
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

        m_platform->close();
    }

    bool Engine::update(f64 delta_time) {
        return m_app->update(delta_time);
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
