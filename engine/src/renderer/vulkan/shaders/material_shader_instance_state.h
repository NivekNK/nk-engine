#pragma once

#include "vulkan/vk.h"
#include "collections/arr.h"

namespace nk {
    struct DescriptorState {
        // One per frame
        cl::arr<u32> generations;
    };

    struct MaterialShaderInstanceState {
        static constexpr u32 descriptor_count = 2;

        // Per frame
        cl::arr<VkDescriptorSet> descriptor_sets;

        // Per descriptor
        DescriptorState descriptor_states[descriptor_count];
    };
}
