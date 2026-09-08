struct Globals { float4x4 projection; };
[[vk::binding(0, 0)]] ConstantBuffer<Globals> globals;
struct VertexInput {
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD;
    [[vk::location(2)]] float4 color : COLOR;
};
struct VertexOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD;
    [[vk::location(1)]] float4 color : COLOR;
};
VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(globals.projection, float4(input.position, 0, 1));
    output.uv = input.uv; // Atlas rows are already top-to-bottom.
    output.color = input.color;
    return output;
}
