#include "nkpch.h"

#include "vulkan/shaders/vulkan_shader.h"

#include "systems/resource_system.h"
#include "vulkan/command_buffer.h"
#include "vulkan/device.h"
#include "vulkan/render_pass.h"

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
    }

    result<void, renderer_error> VulkanShader::init(
        const ShaderConfig& config,
        const u32 width,
        const u32 height,
        const u32 image_count,
        RenderPass* render_pass,
        Device* device,
        mem::Allocator* allocator,
        ResourceSystem* resources,
        VkAllocationCallbacks* vulkan_allocator) {
        if (initialized() || width == 0 || height == 0 || image_count == 0 ||
            render_pass == nullptr || device == nullptr ||
            device->get() == nullptr || allocator == nullptr ||
            resources == nullptr) {
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
        m_image_count = image_count;
        m_max_instances = config.max_instances;

        auto fail = [this](const renderer_error error)
            -> result<void, renderer_error> {
            shutdown();
            return err(error);
        };

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

            u64 set_multiplier = image_count;
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
                } else if (set.scope == ShaderScope::instance &&
                           m_instance_sampler_binding == numeric::invalid_id) {
                    m_instance_sampler_binding = configured.binding;
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

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = pool_size_count;
        pool_info.pPoolSizes = pool_sizes;
        pool_info.maxSets = static_cast<u32>(max_descriptor_set_count);
        VkResult native_result = vkCreateDescriptorPool(
            m_device->get(),
            &pool_info,
            m_vulkan_allocator,
            &m_descriptor_pool);
        if (native_result != VK_SUCCESS)
            return fail(descriptor_error(native_result));

        if (m_global_set_index != numeric::invalid_id) {
            cl::arr<VkDescriptorSetLayout> layouts;
            if (!layouts.arr_init(m_allocator, m_image_count) ||
                !m_global_descriptor_sets.arr_init(
                    m_allocator, m_image_count)) {
                return fail({renderer_error_code::out_of_memory, 0});
            }
            for (VkDescriptorSetLayout& layout : layouts) {
                layout = m_descriptor_set_layouts[m_global_set_index];
            }

            VkDescriptorSetAllocateInfo allocation_info{};
            allocation_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocation_info.descriptorPool = m_descriptor_pool;
            allocation_info.descriptorSetCount = m_image_count;
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
            auto initialized = m_global_uniform_buffer.init(
                m_device,
                m_vulkan_allocator,
                m_global_uniform_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    optional_device_local,
                true);
            if (!initialized)
                return fail(initialized.error());
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
                    instance_buffer_size)) {
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

        auto pipeline_initialized = m_pipeline.init({
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
        });
        if (!pipeline_initialized)
            return fail(pipeline_initialized.error());

        return ok();
    }

    void VulkanShader::shutdown() {
        if (m_device != nullptr && m_device->get() != nullptr) {
            m_pipeline.shutdown();
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

        (void)m_global_descriptor_sets.arr_shutdown();
        (void)m_descriptor_set_layouts.arr_shutdown();
        (void)m_push_constants.arr_shutdown();
        (void)m_stages.arr_shutdown();
        m_device = nullptr;
        m_allocator = nullptr;
        m_vulkan_allocator = nullptr;
        m_name.clear();
        m_descriptor_pool = nullptr;
        m_image_count = 0;
        m_max_instances = 0;
        m_global_set_index = numeric::invalid_id;
        m_instance_set_index = numeric::invalid_id;
        m_global_uniform_binding = numeric::invalid_id;
        m_instance_uniform_binding = numeric::invalid_id;
        m_instance_sampler_binding = numeric::invalid_id;
        m_global_uniform_size = 0;
        m_instance_uniform_size = 0;
        m_instance_uniform_stride = 0;
    }

    void VulkanShader::use(const CommandBuffer& command_buffer) {
        m_pipeline.bind(&command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    result<void, renderer_error> VulkanShader::apply_global_uniform(
        const CommandBuffer& command_buffer,
        const u32 image_index,
        const void* data,
        const u64 size) {
        if (!initialized() || data == nullptr ||
            image_index >= m_global_descriptor_sets.length() ||
            m_global_uniform_binding == numeric::invalid_id ||
            size != m_global_uniform_size) {
            return err(initialization_error());
        }

        auto loaded = m_global_uniform_buffer.load_data(0, size, 0, data);
        if (!loaded)
            return err(loaded.error());

        const VkDescriptorSet descriptor =
            m_global_descriptor_sets[image_index];
        VkDescriptorBufferInfo buffer_info{
            .buffer = m_global_uniform_buffer.get(),
            .offset = 0,
            .range = m_global_uniform_size,
        };
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptor;
        write.dstBinding = m_global_uniform_binding;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buffer_info;
        vkUpdateDescriptorSets(m_device->get(), 1, &write, 0, nullptr);

        vkCmdBindDescriptorSets(
            command_buffer.get(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.get_layout(),
            m_global_set_index,
            1,
            &descriptor,
            0,
            nullptr);
        return ok();
    }

    result<void, renderer_error> VulkanShader::load_instance_uniform(
        const u32 instance_id,
        const void* data,
        const u64 size) {
        if (!initialized() || data == nullptr || instance_id >= m_max_instances ||
            size != m_instance_uniform_size ||
            m_instance_uniform_stride == 0) {
            return err(initialization_error());
        }
        return m_instance_uniform_buffer.load_data(
            m_instance_uniform_stride * instance_id,
            size,
            0,
            data);
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

        vkCmdPushConstants(
            command_buffer.get(),
            m_pipeline.get_layout(),
            to_vulkan_stages(stages),
            offset,
            size,
            data);
        return ok();
    }

    result<void, renderer_error>
    VulkanShader::allocate_instance_descriptor_sets(
        cl::arr<VkDescriptorSet>& sets) {
        if (!initialized() || m_instance_set_index == numeric::invalid_id ||
            sets.allocator() != nullptr) {
            return err(initialization_error());
        }
        if (!sets.arr_init(m_allocator, m_image_count))
            return err(renderer_error{renderer_error_code::out_of_memory, 0});

        cl::arr<VkDescriptorSetLayout> layouts;
        if (!layouts.arr_init(m_allocator, m_image_count)) {
            (void)sets.arr_shutdown();
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        for (VkDescriptorSetLayout& layout : layouts)
            layout = m_descriptor_set_layouts[m_instance_set_index];

        VkDescriptorSetAllocateInfo allocation_info{};
        allocation_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocation_info.descriptorPool = m_descriptor_pool;
        allocation_info.descriptorSetCount = m_image_count;
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
            m_pipeline.get_layout(),
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
