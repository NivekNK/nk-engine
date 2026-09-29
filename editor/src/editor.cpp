#include <core/app_creator.h>

#include <collections/dyarr.h>
#include <core/camera.h>
#include <core/input.h>
#include <memory/malloc_allocator.h>
#include <resources/material.h>
#include <resources/mesh.h>
#include <resources/static_mesh_resource.h>
#include <systems/geometry_system.h>
#include <systems/job_system.h>
#include <systems/material_system.h>
#include <systems/resource_system.h>
#include <systems/shader_names.h>
#include <text/text_overlay.h>

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Editor final : public nk::App {
public:
    Editor() : nk::App({
        .name = "Editor",
        .start_pos_x = 100,
        .start_pos_y = 100,
        .start_width = 1280,
        .start_height = 720,
    }) {
        DebugLog("Editor created.");
    }

    ~Editor() override {
        DebugLog("Editor destroyed.");
    }

    nk::result<void, nk::app_error> initialize(
        nk::AppServices& services) override {
        m_services = services;
        m_services_valid = true;
        m_accept_mesh_publication = false;

        if (!m_meshes.dyarr_init(&m_services.allocator(), 5))
            return initialization_error();

        nk::MaterialConfig skybox_material{};
        skybox_material.name.assign("skybox_material");
        skybox_material.shader_name.assign(nk::builtin_skybox_shader_name);
        skybox_material.type = nk::MaterialType::skybox;
        skybox_material.diffuse_map_name.assign("skybox");
        skybox_material.diffuse_sampler.wrap_u =
            nk::TextureWrap::clamp_to_edge;
        skybox_material.diffuse_sampler.wrap_v =
            nk::TextureWrap::clamp_to_edge;
        skybox_material.diffuse_sampler.wrap_w =
            nk::TextureWrap::clamp_to_edge;
        auto skybox = nk::GeometrySystem::generate_cube(
            m_services.allocator(),
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            "skybox_geometry",
            skybox_material.name.view());
        if (!skybox)
            return initialization_error(static_cast<nk::i32>(skybox.error().code));
        auto skybox_geometry = m_services.geometries().acquire(
            *skybox,
            skybox_material,
            true);
        if (!skybox_geometry)
            return initialization_error(skybox_geometry.error().native_code);
        m_skybox_geometry = *skybox_geometry;

        if (!add_cube(10.0f, "test geometry", "paving") ||
            !add_cube(5.0f, "test geometry 2", "cobblestone") ||
            !add_cube(2.0f, "test geometry 3", "paving2")) {
            return initialization_error();
        }

        m_meshes[1].transform().set_position({10.0f, 0.0f, 1.0f});
        if (!m_meshes[1].transform().set_parent(
                &m_meshes[0].transform())) {
            return initialization_error();
        }
        m_meshes[2].transform().set_position({5.0f, 0.0f, 1.0f});
        if (!m_meshes[2].transform().set_parent(
                &m_meshes[1].transform())) {
            return initialization_error();
        }
        m_test_material = m_meshes[0].geometry(0)->material;

        nk::Geometry2DConfig ui_config{};
        if (!ui_config.vertices.dyarr_init_list(
                &m_services.allocator(),
                {
                    {{0.0f, 0.0f}, {0.0f, 0.0f}},
                    {{512.0f, 512.0f}, {1.0f, 1.0f}},
                    {{0.0f, 512.0f}, {0.0f, 1.0f}},
                    {{512.0f, 0.0f}, {1.0f, 0.0f}},
                }) ||
            !ui_config.indices.dyarr_init_list(
                &m_services.allocator(),
                {2u, 1u, 0u, 3u, 0u, 1u})) {
            return initialization_error();
        }
        ui_config.name.assign("test_ui_geometry");
        ui_config.material_name.assign("test_ui_material");

        const nk::cstr sampler_demo = std::getenv("NK_SAMPLER_DEMO");
        m_sampler_demo = sampler_demo != nullptr &&
            std::strcmp(sampler_demo, "1") == 0;
        const nk::cstr ui_demo = std::getenv("NK_UI_DEMO");
        m_ui_demo = m_sampler_demo ||
            (ui_demo != nullptr && std::strcmp(ui_demo, "1") == 0);
        if (m_sampler_demo) {
            if (!ui_config.vertices.dyarr_resize(8) ||
                !ui_config.indices.dyarr_resize(12)) {
                return initialization_error();
            }
            for (nk::u32 panel = 0; panel < 2; ++panel) {
                const nk::f32 x = panel == 0 ? 16.0f : 528.0f;
                const glm::vec2 uv_min = panel == 0
                    ? glm::vec2{-0.25f, -0.25f}
                    : glm::vec2{0.70f, 0.02f};
                const glm::vec2 uv_max = panel == 0
                    ? glm::vec2{1.25f, 1.25f}
                    : glm::vec2{0.765f, 0.085f};
                ui_config.vertices[panel * 4] = {{x, 16}, uv_min};
                ui_config.vertices[panel * 4 + 1] = {
                    {x + 480, 496}, uv_max};
                ui_config.vertices[panel * 4 + 2] = {
                    {x, 496}, {uv_min.x, uv_max.y}};
                ui_config.vertices[panel * 4 + 3] = {
                    {x + 480, 16}, {uv_max.x, uv_min.y}};
                constexpr nk::u32 indices[]{2, 1, 0, 3, 0, 1};
                for (nk::u32 index = 0; index < 6; ++index)
                    ui_config.indices[panel * 6 + index] =
                        indices[index] + panel * 4;
            }
        }

        auto ui_geometry = m_services.geometries().acquire(ui_config, true);
        if (!ui_geometry)
            return initialization_error(ui_geometry.error().native_code);
        m_ui_geometry = *ui_geometry;
        auto ui_pick = m_services.register_pick_object({
            .kind = nk::PickObjectKind::ui,
            .owner = m_next_pick_owner++,
        });
        if (!ui_pick)
            return initialization_error(static_cast<nk::i32>(ui_pick.error()));
        m_ui_pick_id = *ui_pick;

        if (m_sampler_demo && !set_debug_sampler(0))
            return initialization_error();
        configure_sampler_smoke();

        const nk::cstr text_overlay = std::getenv("NK_TEXT_OVERLAY");
        if (text_overlay == nullptr || std::strcmp(text_overlay, "0") != 0) {
            m_text_overlay =
                m_services.allocator().construct_t(nk::TextOverlay);
            if (m_text_overlay == nullptr)
                return initialization_error();
            auto initialized = m_text_overlay->init(
                m_services.allocator(),
                m_services.renderer(),
                m_services.shaders(),
                m_services.resources().asset_base_path(),
                1.0f);
            if (!initialized)
                return initialization_error(static_cast<nk::i32>(initialized.error()));
        }

        m_lighting.directional.color = {0.4f, 0.4f, 0.2f, 1.0f};
        m_lighting.point_lights[0] = {
            .position = {-5.5f, 0.0f, -5.5f},
            .color = {0.0f, 1.0f, 0.0f, 1.0f},
            .constant = 1.0f,
            .linear = 0.35f,
            .quadratic = 0.44f,
        };
        m_lighting.point_lights[1] = {
            .position = {5.5f, 0.0f, -5.5f},
            .color = {1.0f, 0.0f, 0.0f, 1.0f},
            .constant = 1.0f,
            .linear = 0.35f,
            .quadratic = 0.44f,
        };
        m_lighting.point_light_count = nk::max_point_light_count;

        m_accept_mesh_publication = true;
        if (!queue_static_mesh("falcon", {15.0f, 0.0f, 1.0f}) ||
            !queue_static_mesh(
                "sponza",
                {15.0f, 0.0f, 1.0f},
                glm::vec3{0.05f})) {
            return initialization_error();
        }

        m_initialized = true;
        return nk::ok();
    }

    nk::result<void, nk::app_error> update(
        const nk::FrameContext& context) override {
        nk::Camera* camera = context.camera;
        if (camera == nullptr)
            return nk::err(nk::app_error{nk::app_error_code::update_failed});
        if (!m_camera_initialized) {
            camera->set_position({10.5f, 5.0f, 9.5f});
            m_camera_initialized = true;
        }

        if (nk::Input::is_key_down(nk::KeyCode::A) ||
            nk::Input::is_key_down(nk::KeyCode::Left)) {
            camera->yaw(static_cast<nk::f32>(context.delta_time));
        }
        if (nk::Input::is_key_down(nk::KeyCode::D) ||
            nk::Input::is_key_down(nk::KeyCode::Right)) {
            camera->yaw(static_cast<nk::f32>(-context.delta_time));
        }
        if (nk::Input::is_key_down(nk::KeyCode::Up))
            camera->pitch(static_cast<nk::f32>(context.delta_time));
        if (nk::Input::is_key_down(nk::KeyCode::Down))
            camera->pitch(static_cast<nk::f32>(-context.delta_time));

        glm::vec3 velocity{0.0f};
        if (nk::Input::is_key_down(nk::KeyCode::W))
            velocity += camera->forward();
        if (nk::Input::is_key_down(nk::KeyCode::S))
            velocity += camera->backward();
        if (nk::Input::is_key_down(nk::KeyCode::Q))
            velocity += camera->left();
        if (nk::Input::is_key_down(nk::KeyCode::E))
            velocity += camera->right();
        if (nk::Input::is_key_down(nk::KeyCode::Space))
            velocity.y += 1.0f;
        if (nk::Input::is_key_down(nk::KeyCode::X))
            velocity.y -= 1.0f;
        if (!glm::all(glm::epsilonEqual(
                glm::vec3{0.0f}, velocity, 0.0002f))) {
            camera->translate(
                glm::normalize(velocity) * 100.0f *
                static_cast<nk::f32>(context.delta_time));
        }

        if (pressed(nk::KeyCode::T))
            cycle_debug_texture();
        if (pressed(nk::KeyCode::P))
            cycle_debug_sampler();
        update_sampler_smoke(context.frame_number);

        if (!context.benchmark && !m_meshes.empty()) {
            const glm::quat rotation = glm::angleAxis(
                static_cast<nk::f32>(0.5 * context.delta_time),
                glm::vec3{0.0f, 1.0f, 0.0f});
            constexpr nk::u64 rotating_mesh_count = 3;
            for (nk::u64 index = 0;
                 index < rotating_mesh_count && index < m_meshes.length();
                 ++index) {
                m_meshes[index].transform().rotate(rotation);
            }
        }

        update_picking(context);
        return nk::ok();
    }

    nk::result<void, nk::app_error> build_frame(
        const nk::FrameContext& context,
        nk::FrameBuilder& builder) override {
        m_ui_render_data = {
            .model = glm::mat4{1.0f},
            .geometry = m_ui_geometry,
            .pick_id = m_ui_pick_id,
        };
        if (m_sampler_demo) {
            const nk::f32 scale = glm::min(
                1.0f,
                glm::min(
                    static_cast<nk::f32>(context.width) / 1024.0f,
                    static_cast<nk::f32>(context.height) / 512.0f));
            m_ui_render_data.model = glm::scale(
                glm::mat4{1.0f},
                glm::vec3{scale, scale, 1.0f});
        }

        const nk::TextFrame* text_frame = nullptr;
        if (m_text_overlay != nullptr) {
            auto prepared = m_text_overlay->frame(
                context.width,
                context.height,
                context.content_scale,
                context.delta_time,
                context.metrics);
            if (!prepared) {
                return nk::err(nk::app_error{
                    nk::app_error_code::frame_build_failed,
                    static_cast<nk::i32>(prepared.error()),
                });
            }
            text_frame = *prepared;
        }

        builder.packet = {
            .delta_time = context.delta_time,
            .lighting = m_lighting,
            .skybox_geometry = {
                .model = glm::mat4{1.0f},
                .geometry = m_skybox_geometry,
            },
            .mesh_count = static_cast<nk::u32>(m_meshes.length()),
            .meshes = m_meshes.data(),
            .ui_geometry_count = m_ui_demo && m_ui_geometry != nullptr
                ? 1u
                : 0u,
            .ui_geometries = &m_ui_render_data,
            .text = text_frame,
        };
        return nk::ok();
    }

    void frame_complete(const nk::frame_outcome outcome) noexcept override {
        if (m_text_overlay != nullptr &&
            outcome == nk::frame_outcome::rendered) {
            m_text_overlay->acknowledge_frame();
        }
    }

    void shutdown() noexcept override {
        if (!m_services_valid)
            return;
        m_accept_mesh_publication = false;

        for (nk::u64 index = 0; index < m_meshes.length(); ++index) {
            const nk::PickId id = m_meshes[index].pick_id();
            if (id.valid())
                (void)m_services.release_pick_object(id);
            m_meshes[index].set_pick_id({});
        }
        if (m_ui_pick_id.valid())
            (void)m_services.release_pick_object(m_ui_pick_id);
        m_ui_pick_id = {};

        if (m_meshes.allocator() != nullptr)
            (void)m_meshes.dyarr_shutdown();
        m_test_material = nullptr;

        if (m_text_overlay != nullptr) {
            const auto stats = m_text_overlay->statistics();
            InfoLog(
                "Text: {} shape calls, {} cached glyphs, {} atlas pages ({} bytes)",
                stats.shape_calls,
                stats.rasterized_glyphs,
                stats.pages,
                stats.atlas_bytes);
            m_services.allocator().deconstruct_t(
                nk::TextOverlay,
                m_text_overlay);
            m_text_overlay = nullptr;
        }
        if (m_ui_geometry != nullptr) {
            m_services.geometries().release(m_ui_geometry);
            m_ui_geometry = nullptr;
        }
        if (m_skybox_geometry != nullptr) {
            m_services.geometries().release(m_skybox_geometry);
            m_skybox_geometry = nullptr;
        }
        m_initialized = false;
        m_camera_initialized = false;
    }

    [[nodiscard]] bool pending_work() const noexcept override {
        return m_pending_mesh_loads != 0;
    }

private:
    struct StaticMeshLoad {
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::Resource resource{};
        Editor* app = nullptr;
        nk::strbuf<255> name;
        glm::vec3 position{0.0f};
        glm::vec3 scale{1.0f};
        nk::resource_error failure{
            nk::resource_error_code::invalid_data, 0};
    };

    static nk::result<void, nk::app_error> initialization_error(
        const nk::i32 native_code = 0) {
        return nk::err(nk::app_error{
            nk::app_error_code::initialization_failed,
            native_code,
        });
    }

    [[nodiscard]] bool add_cube(
        const nk::f32 size,
        const nk::strview name,
        const nk::strview material_name) {
        auto config = nk::GeometrySystem::generate_cube(
            m_services.allocator(),
            size,
            size,
            size,
            1.0f,
            1.0f,
            name,
            material_name);
        if (!config)
            return false;
        auto mesh = nk::Mesh::create(
            m_services.allocator(),
            m_services.geometries(),
            {&*config, 1});
        if (!mesh)
            return false;
        auto added = m_meshes.dyarr_emplace_back(std::move(*mesh));
        if (!added) {
            mesh->reset();
            return false;
        }
        return assign_pick_id(m_meshes.dyarr_last());
    }

    [[nodiscard]] bool assign_pick_id(
        nk::Mesh& mesh,
        const nk::PickObjectKind kind = nk::PickObjectKind::geometry) {
        auto registered = m_services.register_pick_object({
            .kind = kind,
            .owner = m_next_pick_owner++,
        });
        if (!registered)
            return false;
        mesh.set_pick_id(*registered);
        return true;
    }

    [[nodiscard]] bool queue_static_mesh(
        const nk::strview name,
        const glm::vec3 position,
        const glm::vec3 scale = glm::vec3{1.0f}) {
        StaticMeshLoad load{};
        load.app = this;
        load.position = position;
        load.scale = scale;
        if (!load.name.assign(name))
            return false;
        auto submitted = m_services.jobs().submit(
            nk::JobPriority::high,
            std::move(load),
            &Editor::load_static_mesh_cpu,
            &Editor::complete_static_mesh_load);
        if (!submitted) {
            ErrorLog(
                "Static mesh '{}' could not be queued: job_error={}, native_code={}",
                name,
                static_cast<nk::u32>(submitted.error().code),
                submitted.error().native_code);
            return false;
        }
        ++m_pending_mesh_loads;
        InfoLog("Static mesh '{}' queued for asynchronous loading.", name);
        return true;
    }

    static nk::result<void, nk::job_error> load_static_mesh_cpu(
        StaticMeshLoad& load) noexcept {
        auto loaded = load.app->m_services.resources().load_detached(
            load.allocator,
            load.name.view(),
            nk::ResourceType::static_mesh);
        if (!loaded) {
            load.failure = loaded.error();
            return nk::err(nk::job_error{
                nk::job_error_code::execution_failed,
                load.failure.native_code,
            });
        }
        std::destroy_at(std::addressof(load.resource));
        std::construct_at(
            std::addressof(load.resource),
            std::move(*loaded));
        return nk::ok();
    }

    static void complete_static_mesh_load(
        StaticMeshLoad& load,
        const nk::result<void, nk::job_error>& outcome) noexcept {
        Editor* app = load.app;
        if (app == nullptr)
            return;
        if (app->m_pending_mesh_loads != 0)
            --app->m_pending_mesh_loads;

        if (outcome && app->m_accept_mesh_publication &&
            load.resource.data != nullptr) {
            auto mesh = nk::Mesh::create(
                app->m_services.allocator(),
                app->m_services.geometries(),
                *load.resource.as<nk::StaticMeshResource>());
            if (mesh) {
                mesh->transform().set_position(load.position);
                mesh->transform().set_scale(load.scale);
                auto added = app->m_meshes.dyarr_emplace_back(
                    std::move(*mesh));
                if (added) {
                    if (!app->assign_pick_id(app->m_meshes.dyarr_last()))
                        ErrorLog("Static mesh '{}' has no pick identity.", load.name.view());
                    ++app->m_scene_revision;
                    if (app->m_scene_revision == 0)
                        ++app->m_scene_revision;
                    app->m_services.discard_pick_scene(app->m_scene_revision);
                    InfoLog(
                        "Static mesh '{}' published with {} material group(s).",
                        load.name.view(),
                        app->m_meshes.dyarr_last().geometry_count());
                } else {
                    mesh->reset();
                    ErrorLog("Unable to publish static mesh '{}'.", load.name.view());
                }
            } else {
                const nk::mesh_error error = mesh.error();
                ErrorLog(
                    "Static mesh '{}' publication failed: mesh_error={}, geometry_index={}, geometry_error={}, native_code={}",
                    load.name.view(),
                    static_cast<nk::u32>(error.code),
                    error.geometry_index,
                    error.geometry_error,
                    error.native_code);
            }
        } else if (!outcome &&
                   outcome.error().code != nk::job_error_code::cancelled) {
            ErrorLog(
                "Static mesh '{}' asynchronous load failed: resource_error={}, native_code={}",
                load.name.view(),
                static_cast<nk::u32>(load.failure.code),
                load.failure.native_code);
        }

        if (load.resource.data != nullptr) {
            auto unloaded = app->m_services.resources().unload_detached(
                load.allocator,
                load.resource);
            if (!unloaded) {
                ErrorLog(
                    "Static mesh '{}' detached payload cleanup failed: resource_error={}",
                    load.name.view(),
                    static_cast<nk::u32>(unloaded.error().code));
            }
        }
    }

    [[nodiscard]] static bool pressed(const nk::KeyCodeFlag key) noexcept {
        return nk::Input::is_key_down(key) && nk::Input::was_key_up(key);
    }

    void update_picking(const nk::FrameContext& context) {
        nk::i16 mouse_x = 0;
        nk::i16 mouse_y = 0;
        nk::i16 previous_mouse_x = 0;
        nk::i16 previous_mouse_y = 0;
        nk::Input::get_mouse_position(mouse_x, mouse_y);
        nk::Input::get_previous_mouse_position(
            previous_mouse_x,
            previous_mouse_y);
        const bool moved = !m_pick_pointer_initialized ||
            mouse_x != previous_mouse_x || mouse_y != previous_mouse_y;
        const bool clicked =
            nk::Input::is_mouse_button_down(nk::MouseButton::Left) &&
            nk::Input::was_mouse_button_up(nk::MouseButton::Left);
        if (moved || clicked) {
            auto requested = m_services.request_pick(
                static_cast<nk::f32>(mouse_x),
                static_cast<nk::f32>(mouse_y),
                context.content_scale,
                m_scene_revision,
                clicked
                    ? nk::PickRequestKind::click
                    : nk::PickRequestKind::hover);
            if (!requested && requested.error() != nk::pick_error::queue_full) {
                WarnLog(
                    "Pick request rejected: {}.",
                    static_cast<nk::u32>(requested.error()));
            }
            m_pick_pointer_initialized = true;
        }

        nk::PickResult result{};
        if (!m_services.poll_pick_result(result))
            return;
        if (result.request.kind == nk::PickRequestKind::click) {
            InfoLog(
                "Pick click {}: kind={}, owner={}, retired frame={}.",
                result.request.sequence,
                static_cast<nk::u32>(result.object.kind),
                result.object.owner,
                result.retired_frame);
        } else if (result.id != m_last_hover_pick) {
            m_last_hover_pick = result.id;
            DebugLog(
                "Pick hover changed: kind={}, owner={}.",
                static_cast<nk::u32>(result.object.kind),
                result.object.owner);
        }
    }

    void cycle_debug_texture() {
        if (m_test_material == nullptr)
            return;
        struct TextureSet {
            nk::strview diffuse;
            nk::strview specular;
            nk::strview normal;
        };
        constexpr TextureSet sets[]{
            {{"cobblestone", 11}, {"cobblestone_SPEC", 16},
             {"cobblestone_NRM", 15}},
            {{"paving", 6}, {"paving_SPEC", 11}, {"paving_NRM", 10}},
            {{"paving2", 7}, {"paving2_SPEC", 12}, {"paving2_NRM", 11}},
        };
        const TextureSet& next = sets[m_debug_texture_index];
        auto changed = m_services.materials().set_texture_maps(
            *m_test_material,
            next.diffuse,
            next.specular,
            next.normal);
        if (!changed) {
            ErrorLog(
                "Texture map cycle failed: material_error={}, native_code={}",
                static_cast<nk::u32>(changed.error().code),
                changed.error().native_code);
            return;
        }
        m_debug_texture_index = static_cast<nk::u8>(
            (m_debug_texture_index + 1) % 3);
    }

    [[nodiscard]] bool set_debug_sampler(const nk::u8 preset) {
        if (preset >= 5 || m_ui_geometry == nullptr ||
            m_ui_geometry->material == nullptr) {
            return false;
        }
        nk::SamplerConfig sampling;
        sampling.anisotropy = 1.0f;
        constexpr nk::cstr names[]{
            "linear / repeat",
            "nearest / repeat",
            "nearest / mirrored_repeat",
            "linear / clamp_to_edge",
            "linear / clamp_to_border",
        };
        if (preset == 1 || preset == 2) {
            sampling.min_filter = nk::TextureFilter::nearest;
            sampling.mag_filter = nk::TextureFilter::nearest;
            sampling.mip_filter = nk::TextureFilter::nearest;
        }
        if (preset == 2) {
            sampling.wrap_u = nk::TextureWrap::mirrored_repeat;
            sampling.wrap_v = nk::TextureWrap::mirrored_repeat;
            sampling.wrap_w = nk::TextureWrap::mirrored_repeat;
        } else if (preset == 3) {
            sampling.wrap_u = nk::TextureWrap::clamp_to_edge;
            sampling.wrap_v = nk::TextureWrap::clamp_to_edge;
            sampling.wrap_w = nk::TextureWrap::clamp_to_edge;
        } else if (preset == 4) {
            sampling.wrap_u = nk::TextureWrap::clamp_to_border;
            sampling.wrap_v = nk::TextureWrap::clamp_to_border;
            sampling.wrap_w = nk::TextureWrap::clamp_to_border;
        }
        auto changed = m_services.materials().set_sampler(
            *m_ui_geometry->material,
            nk::TextureUse::diffuse,
            sampling);
        if (!changed) {
            ErrorLog(
                "Sampler demo update failed: material_error={}, native_code={}",
                static_cast<nk::u32>(changed.error().code),
                changed.error().native_code);
            return false;
        }
        m_debug_sampler_index = preset;
        InfoLog(
            "Sampler demo {}: {}. Left: wrap; right: magnified filtering. Press P to cycle.",
            preset,
            names[preset]);
        return true;
    }

    void cycle_debug_sampler() {
        if (!m_sampler_demo) {
            InfoLog("Enable NK_SAMPLER_DEMO=1 to inspect wrap and filtering with P.");
            return;
        }
        (void)set_debug_sampler(static_cast<nk::u8>(
            (m_debug_sampler_index + 1) % 5));
    }

    void configure_sampler_smoke() {
        const nk::cstr cycle =
            std::getenv("NK_SMOKE_TEST_CYCLE_SAMPLERS");
        m_cycle_samplers = m_sampler_demo && cycle != nullptr &&
            std::strcmp(cycle, "1") == 0;
        if (!m_cycle_samplers)
            return;
        const nk::cstr frames = std::getenv("NK_SMOKE_TEST_FRAMES");
        if (frames == nullptr)
            return;
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(frames, &end, 10);
        if (end != frames && *end == '\0')
            m_smoke_test_frames = static_cast<nk::u64>(parsed);
    }

    void update_sampler_smoke(const nk::u64 frame_number) {
        if (!m_cycle_samplers || m_smoke_test_frames < 5)
            return;
        const nk::u8 preset = static_cast<nk::u8>(glm::min(
            nk::u64{4},
            frame_number / (m_smoke_test_frames / 5)));
        if (preset != m_debug_sampler_index)
            (void)set_debug_sampler(preset);
    }

    nk::AppServices m_services{};
    nk::cl::dyarr<nk::Mesh> m_meshes;
    nk::Geometry* m_ui_geometry = nullptr;
    nk::Geometry* m_skybox_geometry = nullptr;
    nk::Material* m_test_material = nullptr;
    nk::TextOverlay* m_text_overlay = nullptr;
    nk::GeometryRenderData m_ui_render_data{};
    nk::SceneLighting m_lighting{};
    nk::PickId m_ui_pick_id{};
    nk::PickId m_last_hover_pick{};
    nk::u64 m_next_pick_owner = 1;
    nk::u64 m_scene_revision = 1;
    nk::u64 m_smoke_test_frames = 0;
    nk::u32 m_pending_mesh_loads = 0;
    nk::u8 m_debug_texture_index = 0;
    nk::u8 m_debug_sampler_index = 0;
    bool m_services_valid = false;
    bool m_initialized = false;
    bool m_camera_initialized = false;
    bool m_ui_demo = false;
    bool m_sampler_demo = false;
    bool m_cycle_samplers = false;
    bool m_accept_mesh_publication = false;
    bool m_pick_pointer_initialized = false;
};

CREATE_APP(Editor)
