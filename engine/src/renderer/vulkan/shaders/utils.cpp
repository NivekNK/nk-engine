#include "nkpch.h"

#include "vulkan/shaders/utils.h"

#include "platform/file.h"
#include "vulkan/device.h"

namespace nk {
    bool create_shader_module(cstr name, cstr type, Device* device, VkAllocationCallbacks* allocator, VkShaderStageFlagBits stage, ShaderStage* out_stage) {
        strbuf<512> shader_path;
        if (!format_to(shader_path, "assets/shaders/{}.{}.spv", name, type)) {
            ErrorLog("Unable to build shader path for {}.{}", name, type);
            return false;
        }
        out_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        File file{*device->allocator()};
        if (!file.open(shader_path.view(), FileMode::Read, true)) {
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
            device->allocator()->free_lot_t(u8, file_buffer, size);
            file_buffer = nullptr;
        }

        return true;
    }
}
