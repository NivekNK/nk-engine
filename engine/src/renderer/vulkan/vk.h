#pragma once

#include <vulkan/vulkan.h>

#if NK_ENABLE_ASSERT
    #define VulkanCheck(expr) Assert(expr == VK_SUCCESS)
#else
    #define VulkanCheck(expr) expr
#endif
