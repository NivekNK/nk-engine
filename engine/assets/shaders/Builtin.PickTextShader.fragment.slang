struct PushConstants
{
    uint pick_id;
};

// Instance samplers start at binding 1; binding 0 is reserved for the
// instance uniform buffer by the shader resource layout.
[[vk::binding(1, 1)]]
Sampler2D atlas;

[[vk::push_constant]]
PushConstants root;

struct FragmentInput
{
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
    [[vk::location(1)]] float alpha : COLOR;
};

uint main(FragmentInput input) : SV_Target0
{
    if (atlas.Sample(input.texcoord).r * input.alpha <= 0.001)
        discard;
    return root.pick_id;
}
