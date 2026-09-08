struct MaterialUniformObject
{
    float4 diffuse_color;
    uint blend_mode;
    float alpha_cutoff;
};

struct PushConstants
{
    float4x4 model;
    uint pick_id;
};

[[vk::binding(0, 1)]]
ConstantBuffer<MaterialUniformObject> material;

[[vk::binding(1, 1)]]
Sampler2D diffuse_texture;

[[vk::push_constant]]
PushConstants root;

struct FragmentInput
{
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

uint main(FragmentInput input) : SV_Target0
{
    float alpha = material.diffuse_color.a *
        diffuse_texture.Sample(input.texcoord).a;
    if (alpha <= 0.001)
        discard;
    return root.pick_id;
}
