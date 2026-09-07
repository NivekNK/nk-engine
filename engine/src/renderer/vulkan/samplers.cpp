#include "nkpch.h"
#include "vulkan/samplers.h"
#include <glm/common.hpp>

namespace nk::vk {
    result<VkSamplerCreateInfo, renderer_error> sampler_create_info(
        const SamplerConfig& config, const SamplerCaps caps) {
        if (!config.valid() || (caps.anisotropy &&
                (!std::isfinite(caps.max_anisotropy) || caps.max_anisotropy < 1)))
            return err(renderer_error{renderer_error_code::sampler_config_invalid, 0});
        constexpr VkFilter filters[]{VK_FILTER_NEAREST, VK_FILTER_LINEAR};
        constexpr VkSamplerMipmapMode mip_filters[]{VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR};
        constexpr VkSamplerAddressMode wraps[]{VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.minFilter = filters[static_cast<u8>(config.min_filter)];
        info.magFilter = filters[static_cast<u8>(config.mag_filter)];
        info.mipmapMode = mip_filters[static_cast<u8>(config.mip_filter)];
        info.addressModeU = wraps[static_cast<u8>(config.wrap_u)];
        info.addressModeV = wraps[static_cast<u8>(config.wrap_v)];
        info.addressModeW = wraps[static_cast<u8>(config.wrap_w)];
        info.maxAnisotropy = caps.anisotropy ? glm::min(config.anisotropy, caps.max_anisotropy) : 1;
        info.anisotropyEnable = info.maxAnisotropy > 1 ? VK_TRUE : VK_FALSE;
        info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        info.compareOp = VK_COMPARE_OP_ALWAYS;
        // Image views define available mip levels, so one sampler can serve any image.
        info.maxLod = VK_LOD_CLAMP_NONE;
        return ok(info);
    }

    result<void, renderer_error> Samplers::init(mem::Allocator& allocator,
        VkDevice device, VkAllocationCallbacks* callbacks, SamplerCaps caps, Dispatch dispatch) {
        if (m_device != VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
            dispatch.create == nullptr || dispatch.destroy == nullptr || dispatch.wait == nullptr)
            return err(renderer_error{renderer_error_code::initialization_failed, 0});
        if (caps.max_count == 0) return err(renderer_error{renderer_error_code::sampler_capacity_exceeded, 0});
        const u32 capacity = glm::min(4096u, caps.max_count);
        if (!m_slots.arr_init(&allocator, capacity)) return err(renderer_error{renderer_error_code::out_of_memory, 0});
        m_device = device;
        m_callbacks = callbacks;
        m_caps = caps;
        m_dispatch = dispatch;
        return ok();
    }

    result<SamplerHandle, renderer_error> Samplers::create(const SamplerConfig& config) {
        if (m_device == VK_NULL_HANDLE) return err(renderer_error{renderer_error_code::initialization_failed, 0});
        auto info = sampler_create_info(config, m_caps);
        if (!info) return err(info.error());
        for (u32 i = 0; i < m_slots.length(); ++i) {
            Slot& slot = m_slots[i];
            if (slot.sampler != VK_NULL_HANDLE || slot.generation == numeric::invalid_id) continue;
            VkSampler native = VK_NULL_HANDLE;
            const VkResult created = m_dispatch.create(m_device, &*info, m_callbacks, &native);
            if (created != VK_SUCCESS)
                return err(renderer_error{renderer_error_code::texture_sampler_creation_failed, static_cast<i32>(created)});
            slot.sampler = native;
            ++m_live;
            return ok(SamplerHandle{i, slot.generation});
        }
        return err(renderer_error{renderer_error_code::sampler_capacity_exceeded, 0});
    }

    VkSampler Samplers::resolve(SamplerHandle handle) const noexcept {
        if (!handle.valid() || handle.index >= m_slots.length()) return VK_NULL_HANDLE;
        const Slot& slot = m_slots[handle.index];
        return slot.generation == handle.generation ? slot.sampler : VK_NULL_HANDLE;
    }

    result<void, renderer_error> Samplers::release(SamplerHandle handle) {
        const VkSampler sampler = resolve(handle);
        if (sampler == VK_NULL_HANDLE) return err(renderer_error{renderer_error_code::sampler_handle_invalid, 0});
        // Same conservative lifetime boundary as resource upload/destruction:
        // never destroy a sampler referenced by an in-flight descriptor.
        const VkResult waited = m_dispatch.wait(m_device);
        if (waited != VK_SUCCESS)
            return err(renderer_error{renderer_error_code::device_wait_failed, static_cast<i32>(waited)});
        m_dispatch.destroy(m_device, sampler, m_callbacks);
        Slot& slot = m_slots[handle.index];
        slot.sampler = VK_NULL_HANDLE;
        ++slot.generation; // Retire instead of wrapping the invalid generation.
        --m_live;
        return ok();
    }

    void Samplers::shutdown() noexcept {
        for (const Slot& slot : m_slots)
            if (slot.sampler != VK_NULL_HANDLE) m_dispatch.destroy(m_device, slot.sampler, m_callbacks);
        m_slots.arr_shutdown();
        m_device = VK_NULL_HANDLE;
        m_callbacks = nullptr;
        m_live = 0;
    }
}
