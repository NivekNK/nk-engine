#pragma once

#include "vulkan/vk.h"

namespace nk {
    struct DescriptorState {
        // One per frame
        u32 generations[3];
    };

    struct ObjectShaderObjectState {
        static constexpr u32 descriptor_count = 1;

        // Per frame
        VkDescriptorSet descriptor_sets[3];

        // Per descriptor
        DescriptorState descriptor_states[descriptor_count];
    };
}
