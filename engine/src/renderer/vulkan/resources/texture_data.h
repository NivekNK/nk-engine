#pragma once

#include "vulkan/vk.h"

#include "vulkan/image.h"

namespace nk {
    struct TextureData {
        Image image;
        VkSampler sampler;
    };
}
