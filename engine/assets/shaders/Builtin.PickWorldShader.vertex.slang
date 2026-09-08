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
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 texcoord : TEXCOORD;
    [[vk::location(3)]] float4 tangent : TANGENT;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float4 world_position = mul(root.model, float4(input.position, 1.0));
    output.position = mul(global_ubo.projection, mul(global_ubo.view, world_position));
    output.texcoord = input.texcoord;
    return output;
}
