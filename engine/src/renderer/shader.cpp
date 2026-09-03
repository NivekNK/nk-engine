#include "nkpch.h"

#include "renderer/shader.h"

namespace nk {
    namespace {
        renderer_error invalid_metadata(
            const shader_config_error error) noexcept {
            return {
                .code = renderer_error_code::shader_config_invalid,
                .native_code = static_cast<i32>(error),
            };
        }
    }

    result<void, renderer_error> ShaderMetadata::init(
        mem::Allocator* allocator,
        const ShaderConfig& config) {
        if (allocator == nullptr || initialized()) {
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });
        }

        auto validated = validate_shader_config(config);
        if (!validated)
            return err(invalid_metadata(validated.error()));

        if (config.uniforms.length() > numeric::u16_max)
            return err(invalid_metadata(
                shader_config_error::metadata_limits_exceeded));

        for (const ShaderUniformConfig& uniform : config.uniforms) {
            const u32 size = shader_uniform_size(uniform);
            if (uniform.offset > numeric::u16_max ||
                size > numeric::u16_max ||
                uniform.binding > numeric::u16_max) {
                return err(invalid_metadata(
                    shader_config_error::metadata_limits_exceeded));
            }
        }

        if (!m_uniforms.arr_init(allocator, config.uniforms.length()))
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });

        for (u64 index = 0; index < config.uniforms.length(); ++index) {
            const ShaderUniformConfig& uniform = config.uniforms[index];
            m_uniforms[index] = {
                .offset = static_cast<u16>(uniform.offset),
                .size = static_cast<u16>(shader_uniform_size(uniform)),
                .binding = static_cast<u16>(uniform.binding),
                .type = uniform.type,
                .scope = uniform.scope,
            };
        }
        return ok();
    }

    void ShaderMetadata::shutdown() noexcept {
        if (m_uniforms.allocator() != nullptr)
            (void)m_uniforms.arr_shutdown();
    }

    const ShaderUniformMetadata* ShaderMetadata::uniform(
        const ShaderUniformHandle handle) const noexcept {
        if (!handle.valid() || handle.index >= m_uniforms.length())
            return nullptr;
        return &m_uniforms[handle.index];
    }
}
