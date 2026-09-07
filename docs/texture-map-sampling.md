# Per-map texture sampling

`Texture` is a shared image, not a sampling policy. Each material's `TextureMap`
borrows that image and owns one sampler acquisition. Different materials can use
the same image with different filters, wrapping and anisotropy without uploading
the pixels again. World materials have diffuse/specular/normal maps; UI uses one.

This adapts [Kohi's texture map definitions](https://raw.githubusercontent.com/travisvroman/kohi/879ccc411f44d5df28ffdcc9c30d27a0febd6a3e/engine/src/resources/resource_types.h)
with NK's C++ configs, generational handles, explicit allocators and `result`.
Image/sampler independence also follows the separation in
[NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI), but the compatibility
backend retains native `VkSampler` and combined-image descriptors. It does not
require descriptor heaps, mesh shaders or other new GPU extensions.

## Configuration

Existing `.kmt` files need no changes: defaults remain linear min/mag/mip,
repeat U/V/W and requested anisotropy 16. Set only fields that should differ:

```ini
diffuse_map_name=orange_lines_512
diffuse_sampler.min_filter=nearest
diffuse_sampler.mag_filter=nearest
diffuse_sampler.mip_filter=nearest
diffuse_sampler.wrap_u=mirrored_repeat
diffuse_sampler.wrap_v=clamp_to_edge
diffuse_sampler.wrap_w=repeat
diffuse_sampler.anisotropy=1

specular_sampler.anisotropy=8
normal_sampler.wrap_u=repeat
```

Each `diffuse_sampler.`, `specular_sampler.` and `normal_sampler.` prefix accepts
the same seven fields. Filters accept `nearest`/`linear`; wrap accepts `repeat`,
`mirrored_repeat`, `clamp_to_edge`, `clamp_to_border`. Unknown sampler fields,
invalid enum names, non-finite anisotropy and values below 1 are errors.
Anisotropy 1 is off; otherwise the backend clamps to the enabled device limit.
Unsupported anisotropy disables it instead of rejecting the physical device.
`NK_VULKAN_ANISOTROPY=0` exercises that fallback on supported hardware too.

Vulkan translation occurs only in `vk::sampler_create_info`. Normalized UVs and
opaque float-black border color are fixed policy; mip availability belongs to the
image view. Native object count and anisotropy limits follow
[VkSamplerCreateInfo requirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkSamplerCreateInfo.html).
No identical-sampler cache is introduced without evidence that it is useful.

C++ configuration uses the same fields:

```cpp
MaterialConfig config;
config.name.assign("pixelated_wall");
config.diffuse_map_name.assign("paving");
config.diffuse_sampler = {
    .min_filter = TextureFilter::nearest,
    .mag_filter = TextureFilter::nearest,
    .mip_filter = TextureFilter::nearest,
    .wrap_u = TextureWrap::mirrored_repeat,
    .anisotropy = 1.0f,
};
auto material = materials.acquire(config);
```

For a live material use `materials.set_sampler(material, TextureUse::diffuse,
config.diffuse_sampler)` and handle its `result`. Call between frames. Replacement
is transactional, does not change image reference counts, and invalidates material
and per-frame descriptor state even when the image is unchanged. Do not directly
edit sampling fields or release a copied handle from a borrowed material.

Custom shader callers pass `map.binding()` to `ShaderSystem::set_sampler`.
Bindings are copied borrowed values, not pointers into temporary material objects.
Their owners must remain alive while used. Empty bindings use the renderer's
default image/sampler; stale explicit sampler handles fail. Resource release waits
for submitted work; steady drawing performs no sampler creation or idle wait.
See [lifetime contracts](renderer-resource-lifetime-contracts.md).

## Binary compatibility

`.nkmesh` is now version **2**. It stores all three requested sampler configs as
fixed-width scalars, not effective device-clamped values or GPU handles. Version 1
is explicitly incompatible: with OBJ available, it regenerates automatically;
binary-only exports must be regenerated from their source. No source model,
texture, Slang shader or dependency revision was changed.
See [the binary format](static-mesh-binary-format.md).

## Interactive and automated checks

Normal scene:

```bash
nix run .#build -- Release
nix run .#run -- Release
```

Optional sampler demonstration (Sponza/Falcon remain behind the UI):

```bash
NK_SAMPLER_DEMO=1 nix run .#run -- Release
```

Press **P** to cycle linear/repeat, nearest/repeat, nearest/mirrored-repeat,
linear/clamp-to-edge and linear/clamp-to-border. The left panel samples beyond
`[0,1]` to expose wrap; the right panel magnifies edges of the same image for
nearest/linear comparison. The console names the active preset. Anisotropy is
disabled for these comparisons. Panels scale down to fit tiled windows and resize.
Without this flag, the usual scene is unchanged.

Automated cycling with Vulkan synchronization validation requested:

```bash
NK_SAMPLER_DEMO=1 NK_SMOKE_TEST_CYCLE_SAMPLERS=1 NK_SMOKE_TEST_FRAMES=300 \
VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
nix run .#run -- Debug
```

Add `NK_VULKAN_LEGACY=1 NK_VULKAN_ANISOTROPY=0` to exercise both compatibility
paths. This checks GPU validation, sampler-only descriptor changes and teardown;
it is not an automated pixel-difference assertion. Resource mutations happen
between frames, outside the steady-frame allocation counter.
