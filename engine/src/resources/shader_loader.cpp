#include "nkpch.h"

#include "resources/loaders.h"

#include <charconv>

#include "core/format.h"
#include "core/str.h"
#include "memory/allocator.h"
#include "platform/file.h"
#include "resources/shader_resource.h"

namespace nk {
    namespace {
        resource_error parse_failure(
            const shader_resource_parse_error error) noexcept {
            return {
                resource_error_code::invalid_data,
                static_cast<i32>(error),
            };
        }

        resource_error file_failure(const file_error error) noexcept {
            return {
                resource_error_code::file_failed,
                static_cast<i32>(error),
            };
        }

        strview trim(const strview text) noexcept {
            u64 begin = 0;
            u64 end = text.length();
            while (begin < end) {
                const char character = text[begin];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                ++begin;
            }
            while (end > begin) {
                const char character = text[end - 1];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                --end;
            }
            return text.substr(begin, end - begin);
        }

        bool split_fields(
            const strview text,
            strview* fields,
            const u32 capacity,
            u32& count) noexcept {
            count = 0;
            u64 offset = 0;
            while (offset <= text.length()) {
                if (count == capacity)
                    return false;
                const u64 separator = text.find(',', offset);
                const u64 end = separator == strview::npos
                    ? text.length()
                    : separator;
                fields[count] = trim(text.substr(offset, end - offset));
                if (fields[count].empty())
                    return false;
                ++count;
                if (separator == strview::npos)
                    break;
                offset = separator + 1;
            }
            return count != 0;
        }

        bool parse_u32(const strview text, u32& value) noexcept {
            if (text.empty())
                return false;
            const char* begin = text.data();
            const char* end = begin + text.length();
            const auto parsed = std::from_chars(begin, end, value);
            return parsed.ec == std::errc{} && parsed.ptr == end;
        }

        bool parse_bool(const strview text, bool& value) noexcept {
            if (text == strview{"true"} || text == strview{"1"}) {
                value = true;
                return true;
            }
            if (text == strview{"false"} || text == strview{"0"}) {
                value = false;
                return true;
            }
            return false;
        }

        bool parse_stage(const strview text, ShaderStage& stage) noexcept {
            if (text == strview{"vertex"})
                stage = ShaderStage::vertex;
            else if (text == strview{"geometry"})
                stage = ShaderStage::geometry;
            else if (text == strview{"fragment"})
                stage = ShaderStage::fragment;
            else
                return false;
            return true;
        }

        bool parse_scope(const strview text, ShaderScope& scope) noexcept {
            if (text == strview{"global"})
                scope = ShaderScope::global;
            else if (text == strview{"instance"})
                scope = ShaderScope::instance;
            else if (text == strview{"local"})
                scope = ShaderScope::local;
            else
                return false;
            return true;
        }

        bool parse_attribute_type(
            const strview text,
            ShaderAttributeType& type) noexcept {
#define NK_SHADER_ATTRIBUTE_CASE(name) \
            if (text == strview{#name}) { \
                type = ShaderAttributeType::name; \
                return true; \
            }
            NK_SHADER_ATTRIBUTE_CASE(f32)
            NK_SHADER_ATTRIBUTE_CASE(f32_2)
            NK_SHADER_ATTRIBUTE_CASE(f32_3)
            NK_SHADER_ATTRIBUTE_CASE(f32_4)
            NK_SHADER_ATTRIBUTE_CASE(i8)
            NK_SHADER_ATTRIBUTE_CASE(i8_2)
            NK_SHADER_ATTRIBUTE_CASE(i8_3)
            NK_SHADER_ATTRIBUTE_CASE(i8_4)
            NK_SHADER_ATTRIBUTE_CASE(u8)
            NK_SHADER_ATTRIBUTE_CASE(u8_2)
            NK_SHADER_ATTRIBUTE_CASE(u8_3)
            NK_SHADER_ATTRIBUTE_CASE(u8_4)
            NK_SHADER_ATTRIBUTE_CASE(i16)
            NK_SHADER_ATTRIBUTE_CASE(i16_2)
            NK_SHADER_ATTRIBUTE_CASE(i16_3)
            NK_SHADER_ATTRIBUTE_CASE(i16_4)
            NK_SHADER_ATTRIBUTE_CASE(u16)
            NK_SHADER_ATTRIBUTE_CASE(u16_2)
            NK_SHADER_ATTRIBUTE_CASE(u16_3)
            NK_SHADER_ATTRIBUTE_CASE(u16_4)
            NK_SHADER_ATTRIBUTE_CASE(i32)
            NK_SHADER_ATTRIBUTE_CASE(i32_2)
            NK_SHADER_ATTRIBUTE_CASE(i32_3)
            NK_SHADER_ATTRIBUTE_CASE(i32_4)
            NK_SHADER_ATTRIBUTE_CASE(u32)
            NK_SHADER_ATTRIBUTE_CASE(u32_2)
            NK_SHADER_ATTRIBUTE_CASE(u32_3)
            NK_SHADER_ATTRIBUTE_CASE(u32_4)
#undef NK_SHADER_ATTRIBUTE_CASE
            return false;
        }

        bool parse_uniform_type(
            const strview text,
            ShaderUniformType& type) noexcept {
#define NK_SHADER_UNIFORM_CASE(name) \
            if (text == strview{#name}) { \
                type = ShaderUniformType::name; \
                return true; \
            }
            NK_SHADER_UNIFORM_CASE(f32)
            NK_SHADER_UNIFORM_CASE(f32_2)
            NK_SHADER_UNIFORM_CASE(f32_3)
            NK_SHADER_UNIFORM_CASE(f32_4)
            NK_SHADER_UNIFORM_CASE(i8)
            NK_SHADER_UNIFORM_CASE(u8)
            NK_SHADER_UNIFORM_CASE(i16)
            NK_SHADER_UNIFORM_CASE(u16)
            NK_SHADER_UNIFORM_CASE(i32)
            NK_SHADER_UNIFORM_CASE(u32)
            NK_SHADER_UNIFORM_CASE(mat4)
            NK_SHADER_UNIFORM_CASE(sampler_2d)
            NK_SHADER_UNIFORM_CASE(custom)
#undef NK_SHADER_UNIFORM_CASE
            return false;
        }

        u32 uniform_alignment(const ShaderUniformType type) noexcept {
            switch (type) {
                case ShaderUniformType::i8:
                case ShaderUniformType::u8:
                    return 1;
                case ShaderUniformType::i16:
                case ShaderUniformType::u16:
                    return 2;
                case ShaderUniformType::f32:
                case ShaderUniformType::i32:
                case ShaderUniformType::u32:
                case ShaderUniformType::custom:
                    return 4;
                case ShaderUniformType::f32_2:
                    return 8;
                case ShaderUniformType::f32_3:
                case ShaderUniformType::f32_4:
                case ShaderUniformType::mat4:
                    return 16;
                case ShaderUniformType::sampler_2d:
                    return 1;
            }
            return 1;
        }

        bool align_up(
            const u32 value,
            const u32 alignment,
            u32& result) noexcept {
            const u32 mask = alignment - 1;
            if (value > numeric::u32_max - mask)
                return false;
            result = (value + mask) & ~mask;
            return true;
        }
    }

    result<void, resource_error> ShaderResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (name.empty() || !out_resource.name.assign(name) ||
            !format_to(
                out_resource.full_path,
                "{}/{}/{}.shadercfg",
                asset_base_path,
                type_path(),
                name)) {
            return err(resource_error{resource_error_code::invalid_name, 0});
        }

        File file{allocator};
        auto opened = file.open(out_resource.full_path.view(), FileMode::Read, false);
        if (!opened)
            return err(file_failure(opened.error()));

        ShaderResourceConfig* parsed =
            allocator.construct_t(ShaderResourceConfig);
        if (parsed == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});
        out_resource.data = parsed;
        out_resource.data_size = sizeof(ShaderResourceConfig);

        bool version_seen = false;
        bool name_seen = false;
        bool render_pass_seen = false;
        bool max_instances_seen = false;
        bool wireframe_seen = false;
        bool depth_test_seen = false;
        u32 version = 0;

        auto fail = [](const shader_resource_parse_error error)
            -> result<void, resource_error> {
            return err(parse_failure(error));
        };

        str line{allocator};
        while (true) {
            auto read = file.read_line(line);
            if (!read)
                return err(file_failure(read.error()));
            if (*read == read_line_outcome::end_of_file)
                break;

            const strview content = trim(line.view());
            if (content.empty() || content[0] == '#')
                continue;
            const u64 separator = content.find('=');
            if (separator == strview::npos)
                return fail(shader_resource_parse_error::invalid_syntax);

            const strview key = trim(content.substr(0, separator));
            const strview value = trim(content.substr(separator + 1));
            if (key.empty() || value.empty())
                return fail(shader_resource_parse_error::invalid_syntax);

            if (key == strview{"version"}) {
                if (version_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                version_seen = true;
                if (!parse_u32(value, version))
                    return fail(shader_resource_parse_error::invalid_number);
            } else if (key == strview{"name"}) {
                if (name_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                name_seen = true;
                if (!parsed->m_name.assign(value))
                    return fail(shader_resource_parse_error::invalid_name);
            } else if (key == strview{"render_pass"}) {
                if (render_pass_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                render_pass_seen = true;
                if (value == strview{"world"})
                    parsed->m_render_pass = RenderPassKind::world;
                else if (value == strview{"ui"})
                    parsed->m_render_pass = RenderPassKind::ui;
                else
                    return fail(shader_resource_parse_error::invalid_render_pass);
            } else if (key == strview{"max_instances"}) {
                if (max_instances_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                max_instances_seen = true;
                if (!parse_u32(value, parsed->m_max_instances))
                    return fail(shader_resource_parse_error::invalid_number);
            } else if (key == strview{"wireframe"}) {
                if (wireframe_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                wireframe_seen = true;
                if (!parse_bool(value, parsed->m_wireframe))
                    return fail(shader_resource_parse_error::invalid_boolean);
            } else if (key == strview{"depth_test"}) {
                if (depth_test_seen)
                    return fail(shader_resource_parse_error::duplicate_property);
                depth_test_seen = true;
                if (!parse_bool(value, parsed->m_depth_test_enabled))
                    return fail(shader_resource_parse_error::invalid_boolean);
            } else if (key == strview{"stage"}) {
                if (parsed->m_stage_count ==
                    ShaderResourceConfig::max_stage_count) {
                    return fail(
                        shader_resource_parse_error::stage_capacity_exceeded);
                }
                strview fields[3];
                u32 field_count = 0;
                if (!split_fields(value, fields, 3, field_count) ||
                    (field_count != 2 && field_count != 3)) {
                    return fail(shader_resource_parse_error::invalid_stage);
                }
                const u32 index = parsed->m_stage_count;
                ShaderStage stage = ShaderStage::none;
                const strview entry_point = field_count == 3
                    ? fields[2]
                    : strview{"main"};
                if (!parse_stage(fields[0], stage) ||
                    !parsed->m_stage_suffixes[index].assign(fields[1]) ||
                    !parsed->m_stage_entry_points[index].assign(entry_point)) {
                    return fail(shader_resource_parse_error::invalid_stage);
                }
                parsed->m_stages[index] = {
                    stage,
                    parsed->m_stage_suffixes[index].view(),
                    parsed->m_stage_entry_points[index].view(),
                };
                ++parsed->m_stage_count;
            } else if (key == strview{"attribute"}) {
                if (parsed->m_attribute_count ==
                    ShaderResourceConfig::max_attribute_count) {
                    return fail(
                        shader_resource_parse_error::attribute_capacity_exceeded);
                }
                strview fields[2];
                u32 field_count = 0;
                if (!split_fields(value, fields, 2, field_count) ||
                    field_count != 2) {
                    return fail(shader_resource_parse_error::invalid_attribute);
                }
                ShaderAttributeType type = ShaderAttributeType::f32;
                const u32 index = parsed->m_attribute_count;
                if (!parse_attribute_type(fields[0], type) ||
                    !parsed->m_attribute_names[index].assign(fields[1])) {
                    return fail(shader_resource_parse_error::invalid_attribute);
                }
                const u32 size = shader_attribute_size(type);
                if (parsed->m_vertex_stride > numeric::u32_max - size)
                    return fail(shader_resource_parse_error::invalid_layout);
                parsed->m_attributes[index] = {
                    parsed->m_attribute_names[index].view(),
                    type,
                    index,
                    parsed->m_vertex_stride,
                };
                parsed->m_vertex_stride += size;
                ++parsed->m_attribute_count;
            } else if (key == strview{"uniform"}) {
                if (parsed->m_uniform_count ==
                    ShaderResourceConfig::max_uniform_count) {
                    return fail(
                        shader_resource_parse_error::uniform_capacity_exceeded);
                }
                strview fields[5];
                u32 field_count = 0;
                if (!split_fields(value, fields, 5, field_count) ||
                    field_count < 3 || field_count > 5) {
                    return fail(shader_resource_parse_error::invalid_uniform);
                }

                ShaderUniformType type = ShaderUniformType::f32;
                ShaderScope scope = ShaderScope::global;
                u32 array_length = 1;
                u32 custom_size = 0;
                if (!parse_uniform_type(fields[0], type) ||
                    !parse_scope(fields[1], scope)) {
                    return fail(shader_resource_parse_error::invalid_uniform);
                }
                if (field_count >= 4 &&
                    (!parse_u32(fields[3], array_length) ||
                     array_length == 0 ||
                     array_length > numeric::u8_max)) {
                    return fail(shader_resource_parse_error::invalid_number);
                }
                if (type == ShaderUniformType::custom) {
                    if (field_count != 5 ||
                        !parse_u32(fields[4], custom_size) ||
                        custom_size == 0) {
                        return fail(shader_resource_parse_error::invalid_uniform);
                    }
                } else if (field_count == 5) {
                    return fail(shader_resource_parse_error::invalid_uniform);
                }
                if (type == ShaderUniformType::sampler_2d &&
                    scope == ShaderScope::local) {
                    return fail(shader_resource_parse_error::invalid_uniform);
                }

                const u32 index = parsed->m_uniform_count;
                if (!parsed->m_uniform_names[index].assign(fields[2]))
                    return fail(shader_resource_parse_error::invalid_uniform);
                parsed->m_uniforms[index] = {
                    .name = parsed->m_uniform_names[index].view(),
                    .type = type,
                    .scope = scope,
                    .binding = 0,
                    .offset = 0,
                    .custom_size = custom_size,
                    .array_length = array_length,
                };
                ++parsed->m_uniform_count;
            } else {
                return fail(shader_resource_parse_error::invalid_syntax);
            }
        }

        if (!version_seen || !name_seen || !render_pass_seen ||
            parsed->m_stage_count == 0 || parsed->m_attribute_count == 0 ||
            parsed->m_uniform_count == 0) {
            return fail(shader_resource_parse_error::missing_property);
        }
        if (version != ShaderResourceConfig::format_version)
            return fail(shader_resource_parse_error::unsupported_version);

        ShaderStage configured_stages = ShaderStage::none;
        for (u32 index = 0; index < parsed->m_stage_count; ++index)
            configured_stages = configured_stages | parsed->m_stages[index].stage;

        u32 uniform_sizes[3]{};
        u32 sampler_counts[2]{};
        for (u32 index = 0; index < parsed->m_uniform_count; ++index) {
            ShaderUniformConfig& uniform = parsed->m_uniforms[index];
            const u32 scope_index = static_cast<u32>(uniform.scope);
            if (uniform.type == ShaderUniformType::sampler_2d) {
                if (scope_index >= 2 ||
                    sampler_counts[scope_index] >
                        numeric::u32_max - uniform.array_length) {
                    return fail(shader_resource_parse_error::invalid_layout);
                }
                uniform.binding = 1;
                uniform.offset = sampler_counts[scope_index];
                sampler_counts[scope_index] += uniform.array_length;
                continue;
            }

            u32 aligned_offset = 0;
            const u32 alignment = uniform.scope == ShaderScope::local
                ? (uniform_alignment(uniform.type) < 4
                    ? 4
                    : uniform_alignment(uniform.type))
                : uniform_alignment(uniform.type);
            if (!align_up(
                    uniform_sizes[scope_index],
                    alignment,
                    aligned_offset)) {
                return fail(shader_resource_parse_error::invalid_layout);
            }
            const u32 size = shader_uniform_size(uniform);
            if (size == 0 || aligned_offset > numeric::u32_max - size)
                return fail(shader_resource_parse_error::invalid_layout);
            uniform.binding = 0;
            uniform.offset = aligned_offset;
            uniform_sizes[scope_index] = aligned_offset + size;
        }

        auto add_set = [parsed, configured_stages](
                           const ShaderScope scope,
                           ShaderDescriptorBindingConfig* bindings,
                           const u32 uniform_size,
                           const u32 sampler_count) noexcept {
            u32 binding_count = 0;
            if (uniform_size != 0) {
                bindings[binding_count++] = {
                    0,
                    ShaderDescriptorType::uniform_buffer,
                    1,
                    configured_stages,
                    uniform_size,
                };
            }
            if (sampler_count != 0) {
                bindings[binding_count++] = {
                    1,
                    ShaderDescriptorType::sampler,
                    sampler_count,
                    configured_stages,
                    0,
                };
            }
            if (binding_count != 0) {
                parsed->m_descriptor_sets[parsed->m_descriptor_set_count++] = {
                    scope,
                    {bindings, binding_count},
                };
            }
        };
        add_set(
            ShaderScope::global,
            parsed->m_global_bindings,
            uniform_sizes[static_cast<u32>(ShaderScope::global)],
            sampler_counts[static_cast<u32>(ShaderScope::global)]);
        add_set(
            ShaderScope::instance,
            parsed->m_instance_bindings,
            uniform_sizes[static_cast<u32>(ShaderScope::instance)],
            sampler_counts[static_cast<u32>(ShaderScope::instance)]);

        const u32 local_size =
            uniform_sizes[static_cast<u32>(ShaderScope::local)];
        if (local_size != 0) {
            u32 push_constant_size = 0;
            if (!align_up(local_size, 4, push_constant_size))
                return fail(shader_resource_parse_error::invalid_layout);
            parsed->m_push_constants[0] = {
                configured_stages,
                0,
                push_constant_size,
            };
            parsed->m_push_constant_count = 1;
        }

        const ShaderConfig config = parsed->config();
        if (!validate_shader_config(config))
            return fail(shader_resource_parse_error::invalid_layout);

        auto closed = file.close();
        if (!closed)
            return err(file_failure(closed.error()));
        return ok();
    }

    void ShaderResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr) {
            allocator.deconstruct_t(
                ShaderResourceConfig,
                resource.as<ShaderResourceConfig>());
        }
        resource.data = nullptr;
        resource.data_size = 0;
    }
}
