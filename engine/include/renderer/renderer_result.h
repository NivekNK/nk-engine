#pragma once

#include "core/defines.h"

namespace nk {
    enum class renderer_error_code : u8 {
        out_of_memory,
        initialization_failed,
        shader_path_failed,
        shader_file_failed,
        shader_binary_invalid,
        shader_module_creation_failed,
        descriptor_creation_failed,
        pipeline_creation_failed,
        buffer_creation_failed,
        buffer_memory_failed,
        image_creation_failed,
        image_memory_failed,
        image_view_creation_failed,
        render_pass_creation_failed,
        framebuffer_creation_failed,
        object_resource_failed,
        device_wait_failed,
        fence_wait_failed,
        fence_reset_failed,
        command_buffer_failed,
        queue_submit_failed,
        swapchain_acquire_failed,
        swapchain_present_failed,
        texture_sampler_creation_failed,
    };

    struct renderer_error {
        renderer_error_code code;
        i32 native_code;
    };

    enum class frame_outcome : u8 {
        rendered,
        skipped_swapchain_recreation,
    };

    enum class swapchain_outcome : u8 {
        ready,
        out_of_date,
        suboptimal,
    };
}
