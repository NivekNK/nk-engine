#include "nkpch.h"
#include "vulkan/graphics_commands.h"

#include "vulkan/shaders/vulkan_shader.h"

#include "systems/resource_system.h"
#include "vulkan/command_buffer.h"
#include "vulkan/device.h"
#include "vulkan/render_pass.h"
#include "vulkan/resources/texture_data.h"

namespace nk {
    namespace {
        VkShaderStageFlags to_vulkan_stages(
            const ShaderStage stages) noexcept {
            VkShaderStageFlags result = 0;
            if (has_shader_stage(stages, ShaderStage::vertex))
                result |= VK_SHADER_STAGE_VERTEX_BIT;
            if (has_shader_stage(stages, ShaderStage::geometry))
                result |= VK_SHADER_STAGE_GEOMETRY_BIT;
            if (has_shader_stage(stages, ShaderStage::fragment))
                result |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (has_shader_stage(stages, ShaderStage::compute))
                result |= VK_SHADER_STAGE_COMPUTE_BIT;
            return result;
        }

        VkFormat to_vulkan_format(
            const ShaderAttributeType type) noexcept {
            switch (type) {
                case ShaderAttributeType::f32:
                    return VK_FORMAT_R32_SFLOAT;
                case ShaderAttributeType::f32_2:
                    return VK_FORMAT_R32G32_SFLOAT;
                case ShaderAttributeType::f32_3:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case ShaderAttributeType::f32_4:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                case ShaderAttributeType::i8:
                    return VK_FORMAT_R8_SINT;
                case ShaderAttributeType::i8_2:
                    return VK_FORMAT_R8G8_SINT;
                case ShaderAttributeType::i8_3:
                    return VK_FORMAT_R8G8B8_SINT;
                case ShaderAttributeType::i8_4:
                    return VK_FORMAT_R8G8B8A8_SINT;
                case ShaderAttributeType::u8:
                    return VK_FORMAT_R8_UINT;
                case ShaderAttributeType::u8_2:
                    return VK_FORMAT_R8G8_UINT;
                case ShaderAttributeType::u8_3:
                    return VK_FORMAT_R8G8B8_UINT;
                case ShaderAttributeType::u8_4:
                    return VK_FORMAT_R8G8B8A8_UINT;
                case ShaderAttributeType::i16:
                    return VK_FORMAT_R16_SINT;
                case ShaderAttributeType::i16_2:
                    return VK_FORMAT_R16G16_SINT;
                case ShaderAttributeType::i16_3:
                    return VK_FORMAT_R16G16B16_SINT;
                case ShaderAttributeType::i16_4:
                    return VK_FORMAT_R16G16B16A16_SINT;
                case ShaderAttributeType::u16:
                    return VK_FORMAT_R16_UINT;
                case ShaderAttributeType::u16_2:
                    return VK_FORMAT_R16G16_UINT;
                case ShaderAttributeType::u16_3:
                    return VK_FORMAT_R16G16B16_UINT;
                case ShaderAttributeType::u16_4:
                    return VK_FORMAT_R16G16B16A16_UINT;
                case ShaderAttributeType::i32:
                    return VK_FORMAT_R32_SINT;
                case ShaderAttributeType::i32_2:
                    return VK_FORMAT_R32G32_SINT;
                case ShaderAttributeType::i32_3:
                    return VK_FORMAT_R32G32B32_SINT;
                case ShaderAttributeType::i32_4:
                    return VK_FORMAT_R32G32B32A32_SINT;
                case ShaderAttributeType::u32:
                    return VK_FORMAT_R32_UINT;
                case ShaderAttributeType::u32_2:
                    return VK_FORMAT_R32G32_UINT;
                case ShaderAttributeType::u32_3:
                    return VK_FORMAT_R32G32B32_UINT;
                case ShaderAttributeType::u32_4:
                    return VK_FORMAT_R32G32B32A32_UINT;
            }
            return VK_FORMAT_UNDEFINED;
        }

        VkDescriptorType to_vulkan_descriptor_type(
            const ShaderDescriptorType type) noexcept {
            return type == ShaderDescriptorType::uniform_buffer
                ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }

        bool checked_add(const u64 left, const u64 right, u64& out) noexcept {
            if (left > numeric::u64_max - right)
                return false;
            out = left + right;
            return true;
        }

        bool checked_multiply(
            const u64 left,
            const u64 right,
            u64& out) noexcept {
            if (left != 0 && right > numeric::u64_max / left)
                return false;
            out = left * right;
            return true;
        }

        bool align_up(
            const u64 value,
            const u64 alignment,
            u64& out) noexcept {
            const u64 mask = alignment - 1;
            if (value > numeric::u64_max - mask)
                return false;
            out = (value + mask) & ~mask;
            return true;
        }

        renderer_error initialization_error() noexcept {
            return {
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            };
        }

        renderer_error descriptor_error(const VkResult result) noexcept {
            return {
                .code = renderer_error_code::descriptor_creation_failed,
                .native_code = static_cast<i32>(result),
            };
        }

        renderer_error invalid_shader_state() noexcept {
            return {renderer_error_code::shader_state_invalid, 0};
        }

        renderer_error invalid_uniform() noexcept {
            return {renderer_error_code::shader_uniform_invalid, 0};
        }
    }

    result<void, renderer_error> VulkanShader::init_descriptor_state(
        DescriptorState& state) {
        if (!state.generations.arr_init(m_allocator, m_frame_count) ||
            !state.images.arr_init(m_allocator, m_frame_count)) {
            release_descriptor_state(state);
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        for (u32 image = 0; image < m_frame_count; ++image) {
            state.generations[image] = numeric::invalid_id;
        }
        return ok();
    }

    void VulkanShader::release_descriptor_state(
        DescriptorState& state) noexcept {
        state.images.arr_shutdown();
        if (state.generations.allocator() != nullptr)
            (void)state.generations.arr_shutdown();
    }

    void VulkanShader::release_instance_state(InstanceState& state) noexcept {
        state.uploaded_revisions.arr_shutdown();
        state.uniform_revision = 1;
        for (DescriptorState& sampler : state.sampler_states)
            release_descriptor_state(sampler);
        if (state.sampler_states.allocator() != nullptr)
            (void)state.sampler_states.arr_shutdown();
        release_descriptor_state(state.uniform_state);
        if (state.descriptor_sets.allocator() != nullptr)
            (void)state.descriptor_sets.arr_shutdown();
    }

    result<void, renderer_error> VulkanShader::init_sampler_slots(
        const ShaderConfig& config) {
        u32 counts[2]{};
        for (const ShaderDescriptorSetConfig& set : config.descriptor_sets) {
            const u32 scope_index = set.scope == ShaderScope::global ? 0 : 1;
            for (const ShaderDescriptorBindingConfig& binding : set.bindings) {
                if (binding.type != ShaderDescriptorType::sampler)
                    continue;
                if (binding.count > max_sampler_count - counts[scope_index]) {
                    return err(renderer_error{
                        renderer_error_code::shader_config_invalid,
                        static_cast<i32>(
                            shader_config_error::metadata_limits_exceeded),
                    });
                }
                counts[scope_index] += binding.count;
            }
        }

        if (counts[0] != 0 &&
            (!m_global_sampler_slots.arr_init(m_allocator, counts[0]) ||
             !m_global_textures.arr_init(m_allocator, counts[0]) ||
             !m_global_sampler_states.arr_init(m_allocator, counts[0]))) {
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }

        u64 instance_texture_count = 0;
        if (!checked_multiply(
                counts[1], m_max_instances, instance_texture_count)) {
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        if (counts[1] != 0 &&
            (!m_instance_sampler_slots.arr_init(m_allocator, counts[1]) ||
             !m_instance_textures.arr_init(
                 m_allocator, instance_texture_count))) {
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }

        u32 next_slots[2]{};
        for (const ShaderDescriptorSetConfig& set : config.descriptor_sets) {
            const u32 scope_index = set.scope == ShaderScope::global ? 0 : 1;
            cl::arr<SamplerSlot>& slots = scope_index == 0
                ? m_global_sampler_slots
                : m_instance_sampler_slots;
            for (const ShaderDescriptorBindingConfig& binding : set.bindings) {
                if (binding.type != ShaderDescriptorType::sampler)
                    continue;
                for (u32 element = 0; element < binding.count; ++element) {
                    slots[next_slots[scope_index]++] = {
                        binding.binding,
                        element,
                    };
                }
            }
        }

        for (TextureBinding& texture : m_global_textures)
            texture = {};
        for (TextureBinding& texture : m_instance_textures)
            texture = {};
        for (DescriptorState& state : m_global_sampler_states) {
            auto initialized = init_descriptor_state(state);
            if (!initialized)
                return err(initialized.error());
        }
        return ok();
    }

    u32 VulkanShader::sampler_slot(
        const ShaderScope scope,
        const u32 binding,
        const u32 array_element) const noexcept {
        const cl::arr<SamplerSlot>& slots = scope == ShaderScope::global
            ? m_global_sampler_slots
            : m_instance_sampler_slots;
        for (u32 index = 0; index < slots.length(); ++index) {
            if (slots[index].binding == binding &&
                slots[index].array_element == array_element) {
                return index;
            }
        }
        return numeric::invalid_id;
    }

    Texture* VulkanShader::valid_texture(Texture* texture) const noexcept {
        if (texture != nullptr && texture->valid())
            return texture;
        if (m_default_texture != nullptr && m_default_texture->valid())
            return m_default_texture;
        return nullptr;
    }

    result<void, renderer_error> VulkanShader::init(
        const ShaderConfig& config,
        const u32 width,
        const u32 height,
        const u32 frame_count,
        RenderPass* render_pass,
        Device* device,
        mem::Allocator* allocator,
        ResourceSystem* resources,
        VkAllocationCallbacks* vulkan_allocator,
        Texture* default_texture, vk::Samplers* samplers, SamplerHandle default_sampler) {
        if (initialized() || width == 0 || height == 0 || frame_count == 0 ||
            render_pass == nullptr || device == nullptr ||
            device->get() == nullptr || allocator == nullptr ||
            resources == nullptr || samplers == nullptr || samplers->resolve(default_sampler) == VK_NULL_HANDLE) {
            return err(initialization_error());
        }

        auto validated = validate_shader_config(config);
        if (!validated) {
            return err(renderer_error{
                .code = renderer_error_code::shader_config_invalid,
                .native_code = static_cast<i32>(validated.error()),
            });
        }
        if (!m_name.assign(config.name)) {
            return err(renderer_error{
                .code = renderer_error_code::shader_config_invalid,
                .native_code = static_cast<i32>(
                    shader_config_error::invalid_name),
            });
        }

        m_device = device;
        m_allocator = allocator;
        m_vulkan_allocator = vulkan_allocator;
        m_default_texture = default_texture;
        m_samplers = samplers;
        m_default_sampler = default_sampler;
        m_frame_count = frame_count;
        m_max_instances = config.max_instances;

        auto fail = [this](const renderer_error error)
            -> result<void, renderer_error> {
            shutdown();
            return err(error);
        };

        auto metadata_initialized = m_metadata.init(allocator, config);
        if (!metadata_initialized)
            return fail(metadata_initialized.error());

        auto sampler_slots_initialized = init_sampler_slots(config);
        if (!sampler_slots_initialized)
            return fail(sampler_slots_initialized.error());

        if (!m_stages.arr_init(m_allocator, config.stages.length()) ||
            !m_push_constants.arr_init(
                m_allocator, config.push_constants.length()) ||
            !m_descriptor_set_layouts.arr_init(
                m_allocator, config.descriptor_sets.length())) {
            return fail({renderer_error_code::out_of_memory, 0});
        }
        for (u64 index = 0; index < config.push_constants.length(); ++index) {
            m_push_constants[index] = config.push_constants[index];
            if (m_push_constants[index].offset >
                    device->max_push_constant_size() ||
                m_push_constants[index].size >
                    device->max_push_constant_size() -
                    m_push_constants[index].offset) {
                return fail({
                    renderer_error_code::shader_config_invalid,
                    static_cast<i32>(
                        shader_config_error::invalid_push_constant),
                });
            }
        }

        for (u64 index = 0; index < config.stages.length(); ++index) {
            DebugLog(
                "Creating {} shader module for '{}'",
                config.stages[index].file_suffix,
                config.name);
            auto created = create_shader_module(
                config.name,
                config.stages[index],
                *resources,
                m_device,
                m_vulkan_allocator,
                &m_stages[index]);
            if (!created)
                return fail(created.error());
        }

        u64 uniform_descriptor_count = 0;
        u64 sampler_descriptor_count = 0;
        u64 max_descriptor_set_count = 0;
        for (u64 set_index = 0;
             set_index < config.descriptor_sets.length();
             ++set_index) {
            const ShaderDescriptorSetConfig& set =
                config.descriptor_sets[set_index];
            if (set.scope == ShaderScope::global)
                m_global_set_index = static_cast<u32>(set_index);
            else
                m_instance_set_index = static_cast<u32>(set_index);

            cl::arr<VkDescriptorSetLayoutBinding> bindings;
            if (!bindings.arr_init(m_allocator, set.bindings.length()))
                return fail({renderer_error_code::out_of_memory, 0});

            u64 set_multiplier = frame_count;
            if (set.scope == ShaderScope::instance &&
                !checked_multiply(
                    set_multiplier, config.max_instances, set_multiplier)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            if (!checked_add(
                    max_descriptor_set_count,
                    set_multiplier,
                    max_descriptor_set_count) ||
                max_descriptor_set_count > numeric::u32_max) {
                return fail({renderer_error_code::out_of_memory, 0});
            }

            for (u64 binding_index = 0;
                 binding_index < set.bindings.length();
                 ++binding_index) {
                const ShaderDescriptorBindingConfig& configured =
                    set.bindings[binding_index];
                bindings[binding_index] = {
                    .binding = configured.binding,
                    .descriptorType =
                        to_vulkan_descriptor_type(configured.type),
                    .descriptorCount = configured.count,
                    .stageFlags = to_vulkan_stages(configured.stages),
                    .pImmutableSamplers = nullptr,
                };

                u64 descriptor_count = 0;
                if (!checked_multiply(
                        set_multiplier,
                        configured.count,
                        descriptor_count)) {
                    return fail({renderer_error_code::out_of_memory, 0});
                }
                u64& total = configured.type ==
                        ShaderDescriptorType::uniform_buffer
                    ? uniform_descriptor_count
                    : sampler_descriptor_count;
                if (!checked_add(total, descriptor_count, total) ||
                    total > numeric::u32_max) {
                    return fail({renderer_error_code::out_of_memory, 0});
                }

                if (configured.type ==
                    ShaderDescriptorType::uniform_buffer) {
                    if (set.scope == ShaderScope::global) {
                        m_global_uniform_binding = configured.binding;
                        m_global_uniform_size = configured.element_size;
                    } else {
                        m_instance_uniform_binding = configured.binding;
                        m_instance_uniform_size = configured.element_size;
                    }
                }
            }

            VkDescriptorSetLayoutCreateInfo layout_info{};
            layout_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_info.bindingCount = static_cast<u32>(bindings.length());
            layout_info.pBindings = bindings.data();
            const VkResult layout_result = vkCreateDescriptorSetLayout(
                m_device->get(),
                &layout_info,
                m_vulkan_allocator,
                &m_descriptor_set_layouts[set_index]);
            if (layout_result != VK_SUCCESS)
                return fail(descriptor_error(layout_result));
        }

        VkDescriptorPoolSize pool_sizes[2]{};
        u32 pool_size_count = 0;
        if (uniform_descriptor_count != 0) {
            pool_sizes[pool_size_count++] = {
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                static_cast<u32>(uniform_descriptor_count),
            };
        }
        if (sampler_descriptor_count != 0) {
            pool_sizes[pool_size_count++] = {
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                static_cast<u32>(sampler_descriptor_count),
            };
        }

        VkResult native_result = VK_SUCCESS;
        if (max_descriptor_set_count != 0) {
            VkDescriptorPoolCreateInfo pool_info{};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags =
                VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.poolSizeCount = pool_size_count;
            pool_info.pPoolSizes = pool_sizes;
            pool_info.maxSets = static_cast<u32>(max_descriptor_set_count);
            native_result = vkCreateDescriptorPool(
                m_device->get(),
                &pool_info,
                m_vulkan_allocator,
                &m_descriptor_pool);
            if (native_result != VK_SUCCESS)
                return fail(descriptor_error(native_result));
        }

        if (m_global_set_index != numeric::invalid_id) {
            cl::arr<VkDescriptorSetLayout> layouts;
            if (!layouts.arr_init(m_allocator, m_frame_count) ||
                !m_global_descriptor_sets.arr_init(
                    m_allocator, m_frame_count)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            for (VkDescriptorSetLayout& layout : layouts) {
                layout = m_descriptor_set_layouts[m_global_set_index];
            }

            VkDescriptorSetAllocateInfo allocation_info{};
            allocation_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocation_info.descriptorPool = m_descriptor_pool;
            allocation_info.descriptorSetCount = m_frame_count;
            allocation_info.pSetLayouts = layouts.data();
            native_result = vkAllocateDescriptorSets(
                m_device->get(),
                &allocation_info,
                m_global_descriptor_sets.data());
            if (native_result != VK_SUCCESS)
                return fail(descriptor_error(native_result));
        }

        const VkMemoryPropertyFlags optional_device_local =
            m_device->supports_device_local_host_visible()
                ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                : 0;
        if (m_global_uniform_size != 0) {
            u64 global_buffer_size = 0;
            if (!align_up(m_global_uniform_size,
                    m_device->uniform_buffer_offset_alignment(), m_global_uniform_stride) ||
                !checked_multiply(m_global_uniform_stride, m_frame_count, global_buffer_size) ||
                !m_global_descriptor_written.arr_init(m_allocator, m_frame_count))
                return fail({renderer_error_code::out_of_memory, 0});
            auto initialized = m_global_uniform_buffer.init(
                m_device,
                m_vulkan_allocator,
                global_buffer_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    optional_device_local,
                true);
            if (!initialized)
                return fail(initialized.error());
            if (!m_global_uniform_data.arr_init(
                    allocator, m_global_uniform_size)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
        }

        if (m_instance_uniform_size != 0) {
            if (!align_up(
                    m_instance_uniform_size,
                    m_device->uniform_buffer_offset_alignment(),
                    m_instance_uniform_stride)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            u64 instance_buffer_size = 0;
            if (!checked_multiply(
                    m_instance_uniform_stride,
                    m_max_instances,
                    m_instance_frame_stride) ||
                !checked_multiply(m_instance_frame_stride, m_frame_count, instance_buffer_size)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            auto initialized = m_instance_uniform_buffer.init(
                m_device,
                m_vulkan_allocator,
                instance_buffer_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    optional_device_local,
                true);
            if (!initialized)
                return fail(initialized.error());
            u64 instance_data_size = 0;
            if (!checked_multiply(
                    m_instance_uniform_size,
                    m_max_instances,
                    instance_data_size)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            if (!m_instance_uniform_data.arr_init(
                    allocator,
                    instance_data_size)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
        }

        if (m_max_instances != 0 &&
            !m_instance_states.arr_init(allocator, m_max_instances)) {
            return fail({renderer_error_code::out_of_memory, 0});
        }

        cl::arr<VkVertexInputAttributeDescription> attributes;
        cl::arr<VkPipelineShaderStageCreateInfo> stages;
        cl::arr<VkPushConstantRange> push_constants;
        if (!attributes.arr_init(m_allocator, config.attributes.length()) ||
            !stages.arr_init(m_allocator, m_stages.length()) ||
            !push_constants.arr_init(
                m_allocator, config.push_constants.length())) {
            return fail({renderer_error_code::out_of_memory, 0});
        }
        for (u64 index = 0; index < config.attributes.length(); ++index) {
            const ShaderAttributeConfig& configured = config.attributes[index];
            attributes[index] = {
                .location = configured.location,
                .binding = 0,
                .format = to_vulkan_format(configured.type),
                .offset = configured.offset,
            };
        }
        for (u64 index = 0; index < m_stages.length(); ++index)
            stages[index] = m_stages[index].pipeline_create_info;
        for (u64 index = 0; index < config.push_constants.length(); ++index) {
            push_constants[index] = {
                .stageFlags =
                    to_vulkan_stages(config.push_constants[index].stages),
                .offset = config.push_constants[index].offset,
                .size = config.push_constants[index].size,
            };
        }

        VkViewport viewport{};
        viewport.y = static_cast<f32>(height);
        viewport.width = static_cast<f32>(width);
        viewport.height = -static_cast<f32>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = {width, height};

        PipelineCreateInfo pipeline_create_info{
            .device = m_device,
            .vulkan_allocator = m_vulkan_allocator,
            .render_pass = render_pass,
            .attribute_count = static_cast<u32>(attributes.length()),
            .attributes = attributes.data(),
            .descriptor_set_layout_count =
                static_cast<u32>(m_descriptor_set_layouts.length()),
            .descriptor_set_layouts = m_descriptor_set_layouts.data(),
            .push_constant_range_count =
                static_cast<u32>(push_constants.length()),
            .push_constant_ranges = push_constants.data(),
            .stage_count = static_cast<u32>(stages.length()),
            .stages = stages.data(),
            .viewport = viewport,
            .scissor = scissor,
            .vertex_stride = config.vertex_stride,
            .is_wireframe = config.wireframe,
            .depth_test_enabled = config.depth_test_enabled,
            .depth_write_enabled = config.depth_test_enabled,
            .blend_enabled = false,
        };
        auto pipeline_initialized = m_opaque_pipeline.init(
            pipeline_create_info);
        if (!pipeline_initialized)
            return fail(pipeline_initialized.error());
        pipeline_create_info.depth_write_enabled = false;
        pipeline_create_info.blend_enabled = true;
        pipeline_initialized = m_transparent_pipeline.init(
            pipeline_create_info);
        if (!pipeline_initialized)
            return fail(pipeline_initialized.error());

        return ok();
    }

    void VulkanShader::shutdown() {
        for (InstanceState& state : m_instance_states)
            release_instance_state(state);
        for (DescriptorState& state : m_global_sampler_states)
            release_descriptor_state(state);

        if (m_device != nullptr && m_device->get() != nullptr) {
            m_transparent_pipeline.shutdown();
            m_opaque_pipeline.shutdown();
            m_instance_uniform_buffer.shutdown();
            m_global_uniform_buffer.shutdown();

            if (m_descriptor_pool != nullptr) {
                vkDestroyDescriptorPool(
                    m_device->get(),
                    m_descriptor_pool,
                    m_vulkan_allocator);
                m_descriptor_pool = nullptr;
            }
            for (VkDescriptorSetLayout& layout :
                 m_descriptor_set_layouts) {
                if (layout != nullptr) {
                    vkDestroyDescriptorSetLayout(
                        m_device->get(), layout, m_vulkan_allocator);
                    layout = nullptr;
                }
            }
            for (VulkanShaderStage& stage : m_stages) {
                if (stage.module != nullptr) {
                    vkDestroyShaderModule(
                        m_device->get(),
                        stage.module,
                        m_vulkan_allocator);
                    stage.module = nullptr;
                }
            }
        }

        (void)m_instance_states.arr_shutdown();
        (void)m_global_sampler_states.arr_shutdown();
        (void)m_instance_textures.arr_shutdown();
        (void)m_global_textures.arr_shutdown();
        (void)m_instance_sampler_slots.arr_shutdown();
        (void)m_global_sampler_slots.arr_shutdown();
        (void)m_instance_uniform_data.arr_shutdown();
        (void)m_global_uniform_data.arr_shutdown();
        (void)m_global_descriptor_written.arr_shutdown();
        (void)m_global_descriptor_sets.arr_shutdown();
        (void)m_descriptor_set_layouts.arr_shutdown();
        (void)m_push_constants.arr_shutdown();
        (void)m_stages.arr_shutdown();
        m_metadata.shutdown();
        m_device = nullptr;
        m_allocator = nullptr;
        m_vulkan_allocator = nullptr;
        m_default_texture = nullptr;
        m_samplers = nullptr;
        m_default_sampler = {};
        m_name.clear();
        m_descriptor_pool = nullptr;
        m_frame_count = 0;
        m_max_instances = 0;
        m_global_set_index = numeric::invalid_id;
        m_instance_set_index = numeric::invalid_id;
        m_global_uniform_binding = numeric::invalid_id;
        m_instance_uniform_binding = numeric::invalid_id;
        m_global_uniform_size = 0;
        m_global_uniform_stride = 0;
        m_instance_uniform_size = 0;
        m_instance_uniform_stride = 0;
        m_instance_frame_stride = 0;
        m_bound_instance_id = numeric::invalid_id;
        m_globals_bound = false;
        m_blend_mode = MaterialBlendMode::opaque;
    }

    result<void, renderer_error> VulkanShader::use(
        const CommandBuffer& command_buffer,
        const MaterialBlendMode blend_mode) {
        if (!initialized())
            return err(invalid_shader_state());
        m_blend_mode = blend_mode;
        active_pipeline().bind(
            &command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS);
        m_globals_bound = false;
        m_bound_instance_id = numeric::invalid_id;
        return ok();
    }

    result<void, renderer_error> VulkanShader::bind_globals() {
        if (!initialized())
            return err(invalid_shader_state());
        m_globals_bound = true;
        m_bound_instance_id = numeric::invalid_id;
        return ok();
    }

    result<void, renderer_error> VulkanShader::bind_instance(
        const u32 instance_id) {
        if (!initialized() || instance_id >= m_instance_states.length() ||
            m_instance_states[instance_id].descriptor_sets.allocator() ==
                nullptr) {
            return err(invalid_shader_state());
        }
        m_bound_instance_id = instance_id;
        return ok();
    }

    result<void, renderer_error> VulkanShader::set_uniform(
        const CommandBuffer& command_buffer,
        const ShaderUniformHandle uniform_handle,
        const ShaderUniformType type,
        const void* data,
        const u32 size) {
        const ShaderUniformMetadata* uniform =
            m_metadata.uniform(uniform_handle);
        if (uniform == nullptr || data == nullptr || size == 0 ||
            uniform->type != type || uniform->size != size ||
            type == ShaderUniformType::sampler_2d) {
            return err(invalid_uniform());
        }

        switch (uniform->scope) {
            case ShaderScope::global:
                if (!m_globals_bound ||
                    uniform->offset > m_global_uniform_data.length() ||
                    uniform->size >
                        m_global_uniform_data.length() - uniform->offset) {
                    return err(invalid_shader_state());
                }
                std::memcpy(
                    m_global_uniform_data.data() + uniform->offset,
                    data,
                    size);
                return ok();
            case ShaderScope::instance: {
                if (m_bound_instance_id >= m_instance_states.length() ||
                    uniform->offset > m_instance_uniform_size ||
                    uniform->size >
                        m_instance_uniform_size - uniform->offset) {
                    return err(invalid_shader_state());
                }
                const u64 base =
                    m_instance_uniform_size * m_bound_instance_id;
                u8* destination = m_instance_uniform_data.data() + base + uniform->offset;
                if (std::memcmp(destination, data, size) != 0) {
                    std::memcpy(destination, data, size);
                    InstanceState& instance = m_instance_states[m_bound_instance_id];
                    if (++instance.uniform_revision == 0) {
                        instance.uniform_revision = 1;
                        for (u64& revision : instance.uploaded_revisions)
                            revision = 0;
                    }
                }
                return ok();
            }
            case ShaderScope::local:
                return push_constant(
                    command_buffer,
                    uniform->offset,
                    uniform->size,
                    data);
        }
        return err(invalid_uniform());
    }

    result<void, renderer_error> VulkanShader::set_sampler(
        const ShaderUniformHandle uniform_handle,
        TextureBinding binding,
        const u32 array_index) {
        const ShaderUniformMetadata* uniform =
            m_metadata.uniform(uniform_handle);
        if (uniform == nullptr ||
            uniform->type != ShaderUniformType::sampler_2d ||
            uniform->scope == ShaderScope::local ||
            array_index >= uniform->array_length) {
            return err(invalid_uniform());
        }

        const u32 slot = sampler_slot(
            uniform->scope,
            uniform->binding,
            static_cast<u32>(uniform->offset) + array_index);
        if (slot == numeric::invalid_id)
            return err(invalid_uniform());

        if (binding.sampler == SamplerHandle{}) binding.sampler = m_default_sampler;
        if (m_samplers->resolve(binding.sampler) == VK_NULL_HANDLE)
            return err(renderer_error{renderer_error_code::sampler_handle_invalid, 0});

        if (uniform->scope == ShaderScope::global) {
            m_global_textures[slot] = binding;
            return ok();
        }
        if (m_bound_instance_id >= m_instance_states.length())
            return err(invalid_shader_state());
        m_instance_textures[
            m_bound_instance_id * m_instance_sampler_slots.length() + slot] =
            binding;
        return ok();
    }

    result<void, renderer_error> VulkanShader::apply_globals(
        const CommandBuffer& command_buffer,
        const u32 frame_index) {
        if (!m_globals_bound || frame_index >= m_frame_count)
            return err(invalid_shader_state());
        if (m_global_set_index == numeric::invalid_id) {
            if (m_global_uniform_size == 0 &&
                m_global_sampler_slots.empty()) {
                return ok();
            }
            return err(invalid_shader_state());
        }
        if (frame_index >= m_global_descriptor_sets.length())
            return err(invalid_shader_state());

        const VkDescriptorSet descriptor =
            m_global_descriptor_sets[frame_index];
        VkWriteDescriptorSet writes[max_sampler_count + 1]{};
        VkDescriptorImageInfo image_infos[max_sampler_count]{};
        VkDescriptorBufferInfo buffer_info{};
        u32 write_count = 0;

        if (m_global_uniform_size != 0) {
            auto loaded = m_global_uniform_buffer.load_data(
                m_global_uniform_stride * frame_index,
                m_global_uniform_size,
                0,
                m_global_uniform_data.data());
            if (!loaded)
                return err(loaded.error());

            if (!m_global_descriptor_written[frame_index]) {
                buffer_info = {
                    .buffer = m_global_uniform_buffer.get(),
                    .offset = m_global_uniform_stride * frame_index,
                    .range = m_global_uniform_size,
                };
                VkWriteDescriptorSet& write = writes[write_count++];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptor;
                write.dstBinding = m_global_uniform_binding;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo = &buffer_info;
                m_global_descriptor_written[frame_index] = true;
            }
        }

        u32 image_info_count = 0;
        for (u32 slot = 0; slot < m_global_sampler_slots.length(); ++slot) {
            const TextureBinding binding = m_global_textures[slot];
            Texture* texture = valid_texture(binding.texture);
            const SamplerHandle sampler = binding.sampler == SamplerHandle{} ? m_default_sampler : binding.sampler;
            const VkSampler native_sampler = m_samplers->resolve(sampler);
            if (native_sampler == VK_NULL_HANDLE)
                return err(renderer_error{renderer_error_code::sampler_handle_invalid, 0});
            if (texture == nullptr)
                return err(invalid_shader_state());

            DescriptorState& state = m_global_sampler_states[slot];
            const vk::SampledImageKey key{texture, texture->generation, sampler};
            if (state.images[frame_index] == key) {
                continue;
            }
            TextureData* texture_data =
                static_cast<TextureData*>(texture->m_internal_data);
            VkDescriptorImageInfo& image_info =
                image_infos[image_info_count++];
            image_info.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture_data->image.get_view();
            image_info.sampler = native_sampler;

            const SamplerSlot& configured = m_global_sampler_slots[slot];
            VkWriteDescriptorSet& write = writes[write_count++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor;
            write.dstBinding = configured.binding;
            write.dstArrayElement = configured.array_element;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &image_info;
            state.images[frame_index] = key;
        }

        update_descriptors(write_count, writes);

        vkCmdBindDescriptorSets(
            command_buffer.get(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            active_pipeline().get_layout(),
            m_global_set_index,
            1,
            &descriptor,
            0,
            nullptr);
        return ok();
    }

    result<void, renderer_error> VulkanShader::apply_instance(
        const CommandBuffer& command_buffer,
        const u32 frame_index,
        const bool /*needs_update*/) {
        if (m_bound_instance_id >= m_instance_states.length() ||
            frame_index >= m_frame_count) {
            return err(invalid_shader_state());
        }

        InstanceState& instance = m_instance_states[m_bound_instance_id];
        if (frame_index >= instance.descriptor_sets.length())
            return err(invalid_shader_state());

        const VkDescriptorSet descriptor =
            instance.descriptor_sets[frame_index];
        VkWriteDescriptorSet writes[max_sampler_count + 1]{};
        VkDescriptorImageInfo image_infos[max_sampler_count]{};
        VkDescriptorBufferInfo buffer_info{};
        u32 write_count = 0;

        if (m_instance_uniform_size != 0) {
            u32& generation =
                instance.uniform_state.generations[frame_index];
            if (instance.uploaded_revisions[frame_index] != instance.uniform_revision) {
                auto loaded = m_instance_uniform_buffer.load_data(
                    m_instance_frame_stride * frame_index +
                        m_instance_uniform_stride * m_bound_instance_id,
                    m_instance_uniform_size,
                    0,
                    m_instance_uniform_data.data() +
                        m_instance_uniform_size * m_bound_instance_id);
                if (!loaded)
                    return err(loaded.error());
                instance.uploaded_revisions[frame_index] = instance.uniform_revision;
            }
            if (generation == numeric::invalid_id) {
                buffer_info = {
                    .buffer = m_instance_uniform_buffer.get(),
                    .offset =
                        m_instance_frame_stride * frame_index +
                            m_instance_uniform_stride * m_bound_instance_id,
                    .range = m_instance_uniform_size,
                };
                VkWriteDescriptorSet& write = writes[write_count++];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptor;
                write.dstBinding = m_instance_uniform_binding;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo = &buffer_info;
                generation = 0;
            }
        }

        u32 image_info_count = 0;
        for (u32 slot = 0; slot < m_instance_sampler_slots.length(); ++slot) {
            const TextureBinding binding = m_instance_textures[
                    m_bound_instance_id *
                        m_instance_sampler_slots.length() +
                    slot];
            Texture* texture = valid_texture(binding.texture);
            const SamplerHandle sampler = binding.sampler == SamplerHandle{} ? m_default_sampler : binding.sampler;
            const VkSampler native_sampler = m_samplers->resolve(sampler);
            if (native_sampler == VK_NULL_HANDLE)
                return err(renderer_error{renderer_error_code::sampler_handle_invalid, 0});
            if (texture == nullptr)
                return err(invalid_shader_state());

            DescriptorState& state = instance.sampler_states[slot];
            const vk::SampledImageKey key{texture, texture->generation, sampler};
            if (state.images[frame_index] == key) {
                continue;
            }
            TextureData* texture_data =
                static_cast<TextureData*>(texture->m_internal_data);
            VkDescriptorImageInfo& image_info =
                image_infos[image_info_count++];
            image_info.imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = texture_data->image.get_view();
            image_info.sampler = native_sampler;

            const SamplerSlot& configured = m_instance_sampler_slots[slot];
            VkWriteDescriptorSet& write = writes[write_count++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptor;
            write.dstBinding = configured.binding;
            write.dstArrayElement = configured.array_element;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &image_info;
            state.images[frame_index] = key;
        }

        update_descriptors(write_count, writes);
        bind_instance_descriptor_set(command_buffer, descriptor);
        return ok();
    }

    result<u32, renderer_error> VulkanShader::acquire_resources() {
        u32 instance_id = numeric::invalid_id;
        for (u32 index = 0; index < m_instance_states.length(); ++index) {
            if (m_instance_states[index].descriptor_sets.allocator() ==
                nullptr) {
                instance_id = index;
                break;
            }
        }
        if (instance_id == numeric::invalid_id) {
            return err(renderer_error{
                renderer_error_code::object_resource_failed,
                0,
            });
        }

        InstanceState& instance = m_instance_states[instance_id];
        auto sets_allocated =
            allocate_instance_descriptor_sets(instance.descriptor_sets);
        if (!sets_allocated)
            return err(sets_allocated.error());

        auto fail = [this, &instance](const renderer_error error)
            -> result<u32, renderer_error> {
            if (instance.descriptor_sets.allocator() != nullptr)
                (void)release_instance_descriptor_sets(
                    instance.descriptor_sets);
            release_instance_state(instance);
            return err(error);
        };

        if (m_instance_uniform_size != 0) {
            auto initialized = init_descriptor_state(instance.uniform_state);
            if (!initialized)
                return fail(initialized.error());
            if (!instance.uploaded_revisions.arr_init(m_allocator, m_frame_count))
                return fail({renderer_error_code::out_of_memory, 0});
        }
        if (!m_instance_sampler_slots.empty()) {
            if (!instance.sampler_states.arr_init(
                    m_allocator, m_instance_sampler_slots.length())) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            for (DescriptorState& state : instance.sampler_states) {
                auto initialized = init_descriptor_state(state);
                if (!initialized)
                    return fail(initialized.error());
            }
        }

        if (m_instance_uniform_size != 0) {
            std::memset(
                m_instance_uniform_data.data() +
                    m_instance_uniform_size * instance_id,
                0,
                m_instance_uniform_size);
        }
        for (u32 slot = 0; slot < m_instance_sampler_slots.length(); ++slot) {
            m_instance_textures[
                instance_id * m_instance_sampler_slots.length() + slot] =
                {};
        }
        return ok(instance_id);
    }

    result<void, renderer_error> VulkanShader::release_resources(
        const u32 instance_id) {
        if (instance_id >= m_instance_states.length())
            return err(invalid_shader_state());
        InstanceState& instance = m_instance_states[instance_id];
        if (instance.descriptor_sets.allocator() == nullptr)
            return err(invalid_shader_state());

        auto released = release_instance_descriptor_sets(
            instance.descriptor_sets);
        if (!released)
            return err(released.error());
        release_instance_state(instance);
        for (u32 slot = 0; slot < m_instance_sampler_slots.length(); ++slot) {
            m_instance_textures[
                instance_id * m_instance_sampler_slots.length() + slot] =
                {};
        }
        if (m_bound_instance_id == instance_id)
            m_bound_instance_id = numeric::invalid_id;
        return ok();
    }

    result<void, renderer_error> VulkanShader::push_constant(
        const CommandBuffer& command_buffer,
        const ShaderStage stages,
        const u32 offset,
        const u32 size,
        const void* data) const {
        if (!initialized() || data == nullptr || size == 0)
            return err(initialization_error());

        bool allowed = false;
        for (const ShaderPushConstantConfig& range : m_push_constants) {
            const u8 requested_stages = static_cast<u8>(stages);
            const u8 range_stages = static_cast<u8>(range.stages);
            if ((requested_stages & ~range_stages) == 0 &&
                offset >= range.offset && offset - range.offset <= range.size &&
                size <= range.size - (offset - range.offset)) {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return err(initialization_error());

        vk::GraphicsCommands{*m_device, command_buffer.get()}.push_root(
            active_pipeline().get_layout(), to_vulkan_stages(stages), offset, size, data);
        return ok();
    }

    result<void, renderer_error> VulkanShader::push_constant(
        const CommandBuffer& command_buffer,
        const u32 offset,
        const u32 size,
        const void* data) const {
        for (const ShaderPushConstantConfig& range : m_push_constants) {
            if (offset >= range.offset &&
                offset - range.offset <= range.size &&
                size <= range.size - (offset - range.offset)) {
                return push_constant(
                    command_buffer, range.stages, offset, size, data);
            }
        }
        return err(initialization_error());
    }

    result<void, renderer_error>
    VulkanShader::allocate_instance_descriptor_sets(
        cl::arr<VkDescriptorSet>& sets) {
        if (!initialized() || m_instance_set_index == numeric::invalid_id ||
            sets.allocator() != nullptr) {
            return err(initialization_error());
        }
        if (!sets.arr_init(m_allocator, m_frame_count))
            return err(renderer_error{renderer_error_code::out_of_memory, 0});

        cl::arr<VkDescriptorSetLayout> layouts;
        if (!layouts.arr_init(m_allocator, m_frame_count)) {
            (void)sets.arr_shutdown();
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        for (VkDescriptorSetLayout& layout : layouts)
            layout = m_descriptor_set_layouts[m_instance_set_index];

        VkDescriptorSetAllocateInfo allocation_info{};
        allocation_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = m_descriptor_pool;
        allocation_info.descriptorSetCount = m_frame_count;
        allocation_info.pSetLayouts = layouts.data();
        const VkResult result = vkAllocateDescriptorSets(
            m_device->get(), &allocation_info, sets.data());
        if (result != VK_SUCCESS) {
            (void)sets.arr_shutdown();
            return err(descriptor_error(result));
        }
        return ok();
    }

    result<void, renderer_error>
    VulkanShader::release_instance_descriptor_sets(
        cl::arr<VkDescriptorSet>& sets) {
        if (!initialized() || sets.allocator() == nullptr || sets.empty())
            return err(initialization_error());

        const VkResult wait_result = vkDeviceWaitIdle(m_device->get());
        if (wait_result != VK_SUCCESS) {
            return err(renderer_error{
                renderer_error_code::device_wait_failed,
                static_cast<i32>(wait_result),
            });
        }
        const VkResult free_result = vkFreeDescriptorSets(
            m_device->get(),
            m_descriptor_pool,
            static_cast<u32>(sets.length()),
            sets.data());
        if (free_result != VK_SUCCESS)
            return err(descriptor_error(free_result));
        (void)sets.arr_shutdown();
        return ok();
    }

    void VulkanShader::bind_instance_descriptor_set(
        const CommandBuffer& command_buffer,
        const VkDescriptorSet set) const {
        vkCmdBindDescriptorSets(
            command_buffer.get(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            active_pipeline().get_layout(),
            m_instance_set_index,
            1,
            &set,
            0,
            nullptr);
    }

    void VulkanShader::update_descriptors(
        const u32 write_count,
        const VkWriteDescriptorSet* writes) const {
        if (write_count != 0) {
            vkUpdateDescriptorSets(
                m_device->get(), write_count, writes, 0, nullptr);
        }
    }
}
