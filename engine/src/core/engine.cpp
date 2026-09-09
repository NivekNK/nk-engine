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

        if (!m_frame_allocator.is_initialized() &&
            m_frame_allocator
                    ._allocator_init_untracked<mem::LinearAllocator>(
                        frame_scratch_capacity,
                        m_frame_scratch_memory) == nullptr) {
            ErrorLog("Frame scratch allocator initialization failed.");
            shutdown_impl();
            return false;
        }
        (void)m_frame_allocator.reset();

        AppServices app_services{
            *m_allocator,
            *m_platform,
            *m_renderer,
            *m_resource_system,
            *m_texture_system,
            *m_shader_system,
            *m_material_system,
            *m_geometry_system,
            *m_camera_system,
            *m_job_system,
        };
        m_app_started = true;
        auto app_initialized = m_app->initialize(app_services);
        if (!app_initialized) {
            ErrorLog(
                "Application initialization failed: app_error={}, native_code={}.",
                static_cast<u32>(app_initialized.error().code),
                app_initialized.error().native_code);
            shutdown_impl();
            return false;
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
        if (m_app_started && m_app != nullptr) {
            m_app->shutdown();
            m_app_started = false;
        }
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

        if (m_geometry_system != nullptr) {
            GeometrySystem::destroy(*m_allocator, m_geometry_system);
            m_geometry_system = nullptr;
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
        (void)m_frame_allocator.reset();
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
        const cstr benchmark_option = std::getenv("NK_BENCHMARK");
        const bool benchmark = benchmark_option != nullptr &&
            std::strcmp(benchmark_option, "1") == 0;
        constexpr u64 benchmark_warmup = 60;
        u64 benchmark_frames = 0;
        u64 benchmark_gpu_samples = 0;
        f64 benchmark_seconds = 0.0;
        f64 benchmark_gpu_ms = 0.0;
        f64 benchmark_max_ms = 0.0;
        u64 benchmark_candidates = 0;
        u64 benchmark_visible = 0;
        u64 benchmark_culled = 0;
        u64 benchmark_draws = 0;
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

        while (m_platform->running()) {
            if (!m_platform->pump_messages()) {
                m_platform->close();
            }

            // Quit requests from input/window events must not start another frame.
            if (!m_platform->running())
                break;

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

                if (m_job_system != nullptr) {
                    auto jobs_updated = m_job_system->update();
                    if (!jobs_updated) {
                        ErrorLog(
                            "Job completion update failed: job_error={}, native_code={}",
                            static_cast<u32>(jobs_updated.error().code),
                            jobs_updated.error().native_code);
                        m_platform->close();
                        break;
                    }
                }
                Camera* camera = m_camera_system == nullptr
                    ? nullptr
                    : m_camera_system->active();
                FrameContext context{
                    .delta_time = delta,
                    .frame_number = frame_count,
                    .width = m_platform->width(),
                    .height = m_platform->height(),
                    .content_scale = m_platform->content_scale(),
                    .benchmark = benchmark,
                    .camera = camera,
                    .metrics = m_frame_metrics.snapshot(),
                };
                auto updated = m_app->update(context);
                if (!updated || camera == nullptr) {
                    FatalLog(
                        "Application update failed: app_error={}, native_code={}.",
                        updated
                            ? static_cast<u32>(app_error_code::update_failed)
                            : static_cast<u32>(updated.error().code),
                        updated ? 0 : updated.error().native_code);
                    m_platform->close();
                    break;
                }
                m_renderer->set_view(camera->view(), camera->position());
                const f64 update_end_time =
                    m_platform->get_absolute_time();

                if (!m_frame_allocator.reset()) {
                    FatalLog("Frame scratch allocator reset failed.");
                    m_platform->close();
                    break;
                }
                FrameBuilder builder{m_frame_allocator, delta};
                auto built = m_app->build_frame(context, builder);
                if (!built) {
                    FatalLog(
                        "Application frame build failed: app_error={}, native_code={}.",
                        static_cast<u32>(built.error().code),
                        built.error().native_code);
                    m_platform->close();
                    break;
                }
                const f64 render_start_time =
                    m_platform->get_absolute_time();
                auto frame = m_renderer->draw_frame(
                    *m_material_system,
                    builder.packet);
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
                m_app->frame_complete(*frame);
                PickingFrameLatency picking_timing{};
                (void)m_renderer->take_pick_timing(picking_timing);

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
                    benchmark_candidates += metrics.latest.draw.candidates;
                    benchmark_visible += metrics.latest.draw.visible;
                    benchmark_culled += metrics.latest.draw.culled;
                    benchmark_draws += metrics.latest.draw.draws;
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
                const bool stable_scene_ready = !m_app->pending_work() &&
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
            InfoLog("Benchmark: {}x{}, frames={}, warmup={}, avg_ms={:.3f}, fps={:.2f}, max_ms={:.3f}, gpu_ms={:.3f}, gpu_samples={}, avg_candidates={:.2f}, avg_visible={:.2f}, avg_culled={:.2f}, avg_draws={:.2f}",
                m_platform->width(), m_platform->height(), benchmark_frames,
                benchmark_warmup, benchmark_seconds * 1000.0 / benchmark_frames,
                benchmark_frames / benchmark_seconds, benchmark_max_ms,
                benchmark_gpu_samples ? benchmark_gpu_ms / benchmark_gpu_samples : -1.0,
                benchmark_gpu_samples,
                static_cast<f64>(benchmark_candidates) / benchmark_frames,
                static_cast<f64>(benchmark_visible) / benchmark_frames,
                static_cast<f64>(benchmark_culled) / benchmark_frames,
                static_cast<f64>(benchmark_draws) / benchmark_frames);
        }
        m_platform->close();
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
