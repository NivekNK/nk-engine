#pragma once

#include "vulkan/vk.h"

#include <vector>

namespace nk {
    struct DescriptorState {
        // One per frame
        std::vector<u32> generations;
    };

    struct ObjectShaderObjectState {
        static constexpr u32 descriptor_count = 2;

        // Per frame
        std::vector<VkDescriptorSet> descriptor_sets;

        // Per descriptor
        DescriptorState descriptor_states[descriptor_count];
    };
}
