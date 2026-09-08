struct GlobalUniformObject
{
    float4x4 projection;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformObject> global_ubo;

struct VertexInput
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
    [[vk::location(2)]] float4 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
    [[vk::location(1)]] float alpha : COLOR;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(global_ubo.projection, float4(input.position, 0.0, 1.0));
    output.texcoord = input.texcoord;
    output.alpha = input.color.a;
    return output;
}
