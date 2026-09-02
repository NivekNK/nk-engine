#include "nkpch.h"

#include "vulkan/shaders/utils.h"

#include "collections/dyarr.h"
#include "systems/resource_system.h"
#include "vulkan/device.h"

namespace nk {
    result<void, shader_error> create_shader_module(
        const cstr name,
        const cstr type,
        ResourceSystem& resources,
        Device* device,
        VkAllocationCallbacks* allocator,
        const VkShaderStageFlagBits stage,
        ShaderStage* out_stage) {
        strbuf<512> shader_path;
        if (!format_to(shader_path, "shaders/{}.{}.spv", name, type))
            return err(shader_error{
                .code = shader_error_code::path_format_failed,
                .resource = {
                    resource_error_code::invalid_name,
                    0,
                },
                .native_code = VK_SUCCESS,
            });
        out_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        auto loaded = resources.load(shader_path.view(), ResourceType::binary);
        if (!loaded) {
            return err(shader_error{
                .code = shader_error_code::resource_failed,
                .resource = loaded.error(),
                .native_code = VK_SUCCESS,
            });
        }

        const cl::dyarr<u8>* binary = loaded->as<cl::dyarr<u8>>();
        if (binary->empty() ||
            (binary->length() % sizeof(u32)) != 0 ||
            (reinterpret_cast<std::uintptr_t>(binary->data()) % alignof(u32)) != 0) {
            (void)resources.unload(*loaded);
            return err(shader_error{
                .code = shader_error_code::invalid_binary,
                .resource = {
                    resource_error_code::invalid_data,
                    0,
                },
                .native_code = VK_SUCCESS,
            });
        }

        out_stage->module_create_info.codeSize = binary->length();
        out_stage->module_create_info.pCode =
            reinterpret_cast<const u32*>(binary->data());

        const VkResult creation_result = vkCreateShaderModule(
            device->get(),
            &out_stage->module_create_info,
            allocator,
            &out_stage->module);
        out_stage->module_create_info.codeSize = 0;
        out_stage->module_create_info.pCode = nullptr;
        auto unloaded = resources.unload(*loaded);
        if (!unloaded) {
            if (creation_result == VK_SUCCESS) {
                vkDestroyShaderModule(
                    device->get(),
                    out_stage->module,
                    allocator);
                out_stage->module = nullptr;
            }
            return err(shader_error{
                .code = shader_error_code::resource_failed,
                .resource = unloaded.error(),
                .native_code = VK_SUCCESS,
            });
        }
        if (creation_result != VK_SUCCESS)
            return err(shader_error{
                .code = shader_error_code::module_creation_failed,
                .resource = {
                    resource_error_code::invalid_data,
                    0,
                },
                .native_code = creation_result,
            });

        out_stage->pipeline_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        out_stage->pipeline_create_info.stage = stage;
        out_stage->pipeline_create_info.module = out_stage->module;
        out_stage->pipeline_create_info.pName = "main";

        return ok();
    }
}
