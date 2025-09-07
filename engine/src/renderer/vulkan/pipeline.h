#pragma once

#include "vulkan/vk.h"

namespace nk {
    class Pipeline {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

    private:
        VkPipeline m_pipeline;
        VkPipelineLayout m_layout;
    };
}
