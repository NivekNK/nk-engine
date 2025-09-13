[[vk::binding(0, 0)]]
cbuffer GlobalUniformObject : register(b0, space0)
{
    float4x4 projection;
    float4x4 view;
};

[[vk::push_constant]]
cbuffer PushConstants : register(b1, space0)
{
    float4x4 model;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 world_position = mul(model, float4(input.position, 1.0));
    float4 view_position  = mul(view, world_position);
    output.position = mul(projection, view_position);
    return output;
}
