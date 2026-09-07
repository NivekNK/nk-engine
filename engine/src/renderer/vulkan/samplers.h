#pragma once

#include "collections/arr.h"
#include "core/result.h"
#include "renderer/sampler.h"
#include "renderer/renderer_result.h"
#include "vulkan/vk.h"

namespace nk::vk {
    struct SampledImageKey {
        const Texture* texture = nullptr;
        u32 generation = numeric::invalid_id;
        SamplerHandle sampler{};
        bool operator==(const SampledImageKey&) const noexcept = default;
    };
    struct SamplerCaps {
        bool anisotropy = false;
        f32 max_anisotropy = 1;
        u32 max_count = 0;
    };

    [[nodiscard]] result<VkSamplerCreateInfo, renderer_error> sampler_create_info(
        const SamplerConfig& config, SamplerCaps caps);

    // Native objects are independent from images. Handles are borrowed identities;
    // material owners explicitly create/release, shaders only resolve them.
    class Samplers {
    public:
        struct Dispatch {
            PFN_vkCreateSampler create = vkCreateSampler;
            PFN_vkDestroySampler destroy = vkDestroySampler;
            PFN_vkDeviceWaitIdle wait = vkDeviceWaitIdle;
        };
        Samplers() = default;
        Samplers(const Samplers&) = delete;
        Samplers& operator=(const Samplers&) = delete;
        [[nodiscard]] result<void, renderer_error> init(mem::Allocator& allocator,
            VkDevice device, VkAllocationCallbacks* callbacks, SamplerCaps caps, Dispatch dispatch);
        // Caller drains submissions and destroys shaders before registry shutdown.
        void shutdown() noexcept;
        [[nodiscard]] result<SamplerHandle, renderer_error> create(const SamplerConfig& config);
        [[nodiscard]] result<void, renderer_error> release(SamplerHandle handle);
        VkSampler resolve(SamplerHandle handle) const noexcept;
        u32 live_count() const noexcept { return m_live; }
    private:
        struct Slot { VkSampler sampler = VK_NULL_HANDLE; u32 generation = 0; };
        cl::arr<Slot> m_slots;
        VkDevice m_device = VK_NULL_HANDLE;
        VkAllocationCallbacks* m_callbacks = nullptr;
        SamplerCaps m_caps{};
        Dispatch m_dispatch{};
        u32 m_live = 0;
    };
}
