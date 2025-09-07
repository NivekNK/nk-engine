#include "nkpch.h"

#include "vulkan/shaders/utils.h"

#include "platform/file.h"
#include "vulkan/device.h"

namespace nk {
    bool create_shader_module(cstr name, cstr type, Device* device, VkAllocationCallbacks* allocator, VkShaderStageFlagBits stage, ShaderStage* out_stage) {
        str shader_path = std::format("assets/shaders/{}.{}.spv", name, type);
        out_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        File file;
        if (!file.open(shader_path.c_str(), FileMode::Read, true)) {
            ErrorLog("Unable to read shader module: {}", shader_path);
            return false;
        }

        u64 size = 0;
        u8* file_buffer = nullptr;
        if (!file.read_all_bytes(&file_buffer, &size)) {
            ErrorLog("Unable to binary read shader module: {}", shader_path);
            return false;
        }

        out_stage->module_create_info.codeSize = size;
        out_stage->module_create_info.pCode = (u32*)file_buffer;

        file.close();

        VulkanCheck(vkCreateShaderModule(
            device->get(),
            &out_stage->module_create_info,
            allocator,
            &out_stage->module));

        out_stage->pipeline_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        out_stage->pipeline_create_info.stage = stage;
        out_stage->pipeline_create_info.module = out_stage->module;
        out_stage->pipeline_create_info.pName = "main";

        if (file_buffer != nullptr) {
            native_free_lot(u8, file_buffer, size);
            file_buffer = nullptr;
        }

        return true;
    }
}
