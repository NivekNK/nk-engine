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
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 texcoord : TEXCOORD;
};

struct dto 
{
    float2 tex_coord;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(1)]] dto out_dto;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    output.out_dto.tex_coord = input.texcoord;

    float4x4 mvp = mul(global_ubo.projection, mul(global_ubo.view, u_push_constants.model));
    output.position = mul(mvp, float4(input.position, 1.0));

    return output;
}
