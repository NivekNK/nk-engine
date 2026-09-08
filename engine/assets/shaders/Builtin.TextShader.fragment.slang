[[vk::binding(1, 1)]] Sampler2D atlas;
struct FragmentInput {
    [[vk::location(0)]] float2 uv : TEXCOORD;
    [[vk::location(1)]] float4 color : COLOR;
};
float4 main(FragmentInput input) : SV_Target0 {
    float coverage = atlas.Sample(input.uv).r;
    return float4(input.color.rgb, input.color.a * coverage);
}
