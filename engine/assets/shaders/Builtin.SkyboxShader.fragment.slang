[[vk::binding(1, 1)]]
SamplerCube cube_sampler;

struct FragmentInput
{
    [[vk::location(0)]] float3 direction : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    return cube_sampler.Sample(normalize(input.direction));
}
