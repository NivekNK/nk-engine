#include "nkpch.h"

#include "vulkan/shaders/object_shader.h"

#include "vulkan/device.h"
#include "vulkan/shaders/utils.h"

#define BUILTIN_SHADER_NAME_OBJECT "Builtin.ObjectShader"

namespace nk {
    void ObjectShader::init(Device* device, VkAllocationCallbacks* vulkan_allocator) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        char stage_type_strings[shader_stage_count][10] = { "vertex", "fragment" };
        VkShaderStageFlagBits stage_types[shader_stage_count] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
        for (u32 i = 0; i < shader_stage_count; i++) {
            DebugLog("Creating {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_OBJECT);
            if (!create_shader_module(BUILTIN_SHADER_NAME_OBJECT, stage_type_strings[i], device, vulkan_allocator, stage_types[i], &m_stages[i])) {
                ErrorLog("Unable to create {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_OBJECT);
                return;
            }
        }

        // TODO: Descriptors
    }

    void ObjectShader::shutdown() {
        for (u32 i = 0; i < shader_stage_count; i++) {
            if (m_stages[i].module != nullptr) {
                vkDestroyShaderModule(m_device->get(), m_stages[i].module, m_vulkan_allocator);
                m_stages[i].module = nullptr;
            }
        }

        m_device = nullptr;
        m_vulkan_allocator = nullptr;
    }

    void ObjectShader::use() {
        
    }
}
