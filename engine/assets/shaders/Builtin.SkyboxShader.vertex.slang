struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
};

struct PushConstants
{
    float4x4 model;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformObject> global_ubo;

[[vk::push_constant]]
PushConstants u_push_constants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 direction : TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    float4x4 view_without_translation = global_ubo.view;
    view_without_translation[0][3] = 0.0;
    view_without_translation[1][3] = 0.0;
    view_without_translation[2][3] = 0.0;
    float4 clip = mul(
        global_ubo.projection,
        mul(view_without_translation,
            mul(u_push_constants.model, float4(input.position, 1.0))));
    output.position = clip.xyww;
    output.direction = input.position;
    return output;
}
