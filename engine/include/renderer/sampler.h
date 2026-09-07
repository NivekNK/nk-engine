#pragma once

#include <cmath>
#include "core/defines.h"

namespace nk {
    enum class TextureFilter : u8 { nearest, linear };
    enum class TextureWrap : u8 { repeat, mirrored_repeat, clamp_to_edge, clamp_to_border };

    struct SamplerConfig {
        TextureFilter min_filter = TextureFilter::linear;
        TextureFilter mag_filter = TextureFilter::linear;
        TextureFilter mip_filter = TextureFilter::linear;
        TextureWrap wrap_u = TextureWrap::repeat;
        TextureWrap wrap_v = TextureWrap::repeat;
        TextureWrap wrap_w = TextureWrap::repeat;
        // 1 disables anisotropy. The backend clamps to supported capabilities.
        f32 anisotropy = 16.0f;

        bool valid() const noexcept {
            return min_filter <= TextureFilter::linear && mag_filter <= TextureFilter::linear &&
                mip_filter <= TextureFilter::linear && wrap_u <= TextureWrap::clamp_to_border &&
                wrap_v <= TextureWrap::clamp_to_border && wrap_w <= TextureWrap::clamp_to_border &&
                std::isfinite(anisotropy) && anisotropy >= 1.0f;
        }
        bool operator==(const SamplerConfig&) const noexcept = default;
    };

    // Borrowed identity, meaningful only in its creating renderer. The owner
    // explicitly releases it; copies do not acquire additional ownership.
    struct SamplerHandle {
        u32 index = numeric::invalid_id;
        u32 generation = numeric::invalid_id;
        bool valid() const noexcept {
            return index != numeric::invalid_id && generation != numeric::invalid_id;
        }
        bool operator==(const SamplerHandle&) const noexcept = default;
    };

    struct Texture;
    struct TextureBinding {
        Texture* texture = nullptr;
        SamplerHandle sampler{};
    };
}
