#pragma once

#include "collections/arr.h"
#include "collections/map.h"
#include "core/result.h"
#include "core/str.h"
#include "core/strbuf.h"
#include "renderer/renderer.h"
#include "systems/shader_names.h"

namespace nk {
    class ResourceSystem;

    struct ShaderSystemConfig {
        u16 max_shader_count = 16;
        u16 max_uniform_count = 64;
        u8 max_global_samplers = 8;
        u8 max_instance_samplers = 8;
    };

    enum class shader_system_error_code : u8 {
        invalid_name,
        not_initialized,
        capacity_exceeded,
        duplicate_shader,
        shader_not_found,
        uniform_not_found,
        invalid_uniform,
        invalid_config,
        out_of_memory,
        resource_failed,
        renderer_failed,
    };

    struct shader_system_error {
        shader_system_error_code code;
        i32 native_code;
    };

    class ShaderSystem final {
    public:
        ShaderSystem() = default;
        ~ShaderSystem();

        ShaderSystem(const ShaderSystem&) = delete;
        ShaderSystem& operator=(const ShaderSystem&) = delete;
        ShaderSystem(ShaderSystem&&) = delete;
        ShaderSystem& operator=(ShaderSystem&&) = delete;

        [[nodiscard]] static result<ShaderSystem*, shader_system_error> create(
            mem::Allocator& allocator,
            Renderer& renderer,
            ResourceSystem& resources,
            ShaderSystemConfig config = {});
        static void destroy(mem::Allocator& allocator, ShaderSystem* system);

        [[nodiscard]] result<ShaderHandle, shader_system_error> load(
            strview resource_name);
        [[nodiscard]] result<ShaderHandle, shader_system_error> create(
            const ShaderConfig& config,
            RenderPassKind render_pass);
        [[nodiscard]] result<void, shader_system_error> destroy(
            ShaderHandle shader);
        [[nodiscard]] result<void, shader_system_error> destroy(
            strview name);

        [[nodiscard]] result<ShaderHandle, shader_system_error> handle(
            strview name) const;
        [[nodiscard]] result<ShaderUniformHandle, shader_system_error> uniform(
            ShaderHandle shader,
            strview name) const;
        [[nodiscard]] result<ShaderUniformMetadata, shader_system_error>
        uniform_metadata(
            ShaderHandle shader,
            ShaderUniformHandle uniform) const;

        [[nodiscard]] result<void, shader_system_error> use(
            ShaderHandle shader);
        [[nodiscard]] result<void, shader_system_error> use(strview name);
        [[nodiscard]] result<void, shader_system_error> bind_globals();
        [[nodiscard]] result<void, shader_system_error> bind_instance(
            u32 instance_id);
        [[nodiscard]] result<void, shader_system_error> apply_globals();
        [[nodiscard]] result<void, shader_system_error> apply_instance(
            bool needs_update = true);
        [[nodiscard]] result<u32, shader_system_error> acquire_instance(
            ShaderHandle shader);
        [[nodiscard]] result<void, shader_system_error> release_instance(
            ShaderHandle shader,
            u32 instance_id);

        template <ShaderUniformValue T>
        [[nodiscard]] result<void, shader_system_error> set_uniform(
            const ShaderUniformHandle uniform,
            const T& value) {
            const ShaderUniformMetadata* metadata = current_uniform(uniform);
            if (metadata == nullptr)
                return err(current_uniform_error(uniform));
            if (metadata->type != shader_uniform_type<T>() ||
                metadata->size != sizeof(T) || metadata->array_length != 1) {
                return err(shader_system_error{
                    shader_system_error_code::invalid_uniform,
                    0,
                });
            }
            auto set = m_renderer->set_shader_uniform(
                m_current_shader,
                uniform,
                value);
            if (!set)
                return err(from_renderer(set.error()));
            return ok();
        }

        [[nodiscard]] result<void, shader_system_error> set_uniform_custom(
            ShaderUniformHandle uniform,
            const void* data,
            u32 size);
        [[nodiscard]] result<void, shader_system_error> set_sampler(
            ShaderUniformHandle uniform,
            TextureBinding binding,
            u32 array_index = 0);

        [[nodiscard]] ShaderHandle current_shader() const noexcept {
            return m_current_shader;
        }
        [[nodiscard]] u16 loaded_count() const noexcept {
            return m_loaded_count;
        }
        [[nodiscard]] const ShaderSystemConfig& config() const noexcept {
            return m_config;
        }

    private:
        struct ShaderRecord {
            strbuf<127> name;
            ShaderHandle handle;
            ShaderMetadata metadata;
            cl::map<str, ShaderUniformHandle> uniform_lookup;
            bool occupied = false;
        };

        [[nodiscard]] result<void, shader_system_error> init(
            mem::Allocator& allocator,
            Renderer& renderer,
            ResourceSystem& resources,
            ShaderSystemConfig config);
        void shutdown() noexcept;
        void reset_record(ShaderRecord& record) noexcept;
        [[nodiscard]] u16 find_free_slot() const noexcept;
        [[nodiscard]] ShaderRecord* find_record(ShaderHandle shader) noexcept;
        [[nodiscard]] const ShaderRecord* find_record(
            ShaderHandle shader) const noexcept;
        [[nodiscard]] const ShaderUniformMetadata* current_uniform(
            ShaderUniformHandle uniform) const noexcept;
        [[nodiscard]] shader_system_error current_uniform_error(
            ShaderUniformHandle uniform) const noexcept;
        [[nodiscard]] static shader_system_error from_renderer(
            renderer_error error) noexcept;

        mem::Allocator* m_allocator = nullptr;
        Renderer* m_renderer = nullptr;
        ResourceSystem* m_resources = nullptr;
        ShaderSystemConfig m_config{};
        cl::arr<ShaderRecord> m_shaders;
        cl::map<str, u16> m_shader_lookup;
        ShaderHandle m_current_shader;
        u16 m_loaded_count = 0;
        bool m_initialized = false;
    };
}
