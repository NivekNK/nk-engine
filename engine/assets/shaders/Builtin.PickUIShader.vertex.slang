struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
};

struct PushConstants
{
    float4x4 model;
    uint pick_id;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformObject> global_ubo;

[[vk::push_constant]]
PushConstants root;

struct VertexInput
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(
        global_ubo.projection,
        mul(global_ubo.view, mul(root.model, float4(input.position, 0.0, 1.0))));
    output.texcoord = float2(input.texcoord.x, 1.0 - input.texcoord.y);
    return output;
}
