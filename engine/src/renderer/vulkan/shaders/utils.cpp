#include "nkpch.h"

#include "vulkan/shaders/utils.h"

#include "platform/file.h"
#include "vulkan/device.h"

namespace nk {
    result<void, shader_error> create_shader_module(
        const cstr name,
        const cstr type,
        Device* device,
        VkAllocationCallbacks* allocator,
        const VkShaderStageFlagBits stage,
        ShaderStage* out_stage) {
        strbuf<512> shader_path;
        if (!format_to(shader_path, "assets/shaders/{}.{}.spv", name, type))
            return err(shader_error{
                .code = shader_error_code::path_format_failed,
                .file = file_error::none,
                .native_code = VK_SUCCESS,
            });
        out_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        File file{*device->allocator()};
        auto opened = file.open(shader_path.view(), FileMode::Read, true);
        if (!opened)
            return err(shader_error{
                .code = shader_error_code::file_failed,
                .file = opened.error(),
                .native_code = VK_SUCCESS,
            });

        auto binary = file.read_all_bytes();
        if (!binary)
            return err(shader_error{
                .code = shader_error_code::file_failed,
                .file = binary.error(),
                .native_code = VK_SUCCESS,
            });
        if (binary->empty() ||
            (binary->length() % sizeof(u32)) != 0 ||
            (reinterpret_cast<std::uintptr_t>(binary->data()) % alignof(u32)) != 0) {
            return err(shader_error{
                .code = shader_error_code::invalid_binary,
                .file = file_error::none,
                .native_code = VK_SUCCESS,
            });
        }

        out_stage->module_create_info.codeSize = binary->length();
        out_stage->module_create_info.pCode =
            reinterpret_cast<const u32*>(binary->data());

        auto closed = file.close();
        if (!closed)
            return err(shader_error{
                .code = shader_error_code::file_failed,
                .file = closed.error(),
                .native_code = VK_SUCCESS,
            });

        const VkResult creation_result = vkCreateShaderModule(
            device->get(),
            &out_stage->module_create_info,
            allocator,
            &out_stage->module);
        out_stage->module_create_info.codeSize = 0;
        out_stage->module_create_info.pCode = nullptr;
        if (creation_result != VK_SUCCESS)
            return err(shader_error{
                .code = shader_error_code::module_creation_failed,
                .file = file_error::none,
                .native_code = creation_result,
            });

        out_stage->pipeline_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        out_stage->pipeline_create_info.stage = stage;
        out_stage->pipeline_create_info.module = out_stage->module;
        out_stage->pipeline_create_info.pName = "main";

        return ok();
    }
}
