#include "nkpch.h"

#include "vulkan/shaders/utils.h"

#include "collections/dyarr.h"
#include "systems/resource_system.h"
#include "vulkan/device.h"

namespace nk {
    namespace {
        VkShaderStageFlagBits to_vulkan_stage(
            const ShaderStage stage) noexcept {
            switch (stage) {
                case ShaderStage::vertex:
                    return VK_SHADER_STAGE_VERTEX_BIT;
                case ShaderStage::geometry:
                    return VK_SHADER_STAGE_GEOMETRY_BIT;
                case ShaderStage::fragment:
                    return VK_SHADER_STAGE_FRAGMENT_BIT;
                case ShaderStage::compute:
                    return VK_SHADER_STAGE_COMPUTE_BIT;
                case ShaderStage::none:
                    break;
            }
            return static_cast<VkShaderStageFlagBits>(0);
        }
    }

    result<void, renderer_error> create_shader_module(
        const strview name,
        const ShaderStageConfig& config,
        ResourceSystem& resources,
        Device* device,
        VkAllocationCallbacks* allocator,
        VulkanShaderStage* out_stage) {
        if (device == nullptr || out_stage == nullptr ||
            out_stage->module != nullptr ||
            !out_stage->entry_point.assign(config.entry_point)) {
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });
        }

        strbuf<512> shader_path;
        if (!format_to(
                shader_path,
                "shaders/{}.{}.spv",
                name,
                config.file_suffix)) {
            return err(renderer_error{
                .code = renderer_error_code::shader_path_failed,
                .native_code = 0,
            });
        }
        out_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        auto loaded = resources.load(shader_path.view(), ResourceType::binary);
        if (!loaded) {
            return err(renderer_error{
                .code = renderer_error_code::shader_file_failed,
                .native_code = static_cast<i32>(loaded.error().code),
            });
        }

        const cl::dyarr<u8>* binary = loaded->as<cl::dyarr<u8>>();
        if (binary->empty() ||
            (binary->length() % sizeof(u32)) != 0 ||
            (reinterpret_cast<std::uintptr_t>(binary->data()) % alignof(u32)) != 0) {
            (void)resources.unload(*loaded);
            return err(renderer_error{
                .code = renderer_error_code::shader_binary_invalid,
                .native_code = 0,
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
            return err(renderer_error{
                .code = renderer_error_code::shader_file_failed,
                .native_code = static_cast<i32>(unloaded.error().code),
            });
        }
        if (creation_result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::shader_module_creation_failed,
                .native_code = static_cast<i32>(creation_result),
            });

        out_stage->pipeline_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        out_stage->pipeline_create_info.stage = to_vulkan_stage(config.stage);
        out_stage->pipeline_create_info.module = out_stage->module;
        out_stage->pipeline_create_info.pName = out_stage->entry_point.cstr();

        return ok();
    }
}
