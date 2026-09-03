struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
    float4 ambient_color;
    float3 directional_light_direction;
    float4 directional_light_color;
};

struct PushConstants
{
    float4x4 model;
    float4x4 normal_matrix;
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
    float3 normal;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(1)]] dto out_dto;
};

float3 safe_normalize(float3 value)
{
    float largest_component = max(
        max(abs(value.x), abs(value.y)),
        abs(value.z));
    if (!(largest_component > 1.0e-20) ||
        !(largest_component < 3.402823466e+38)) {
        return float3(0.0, 0.0, 0.0);
    }
    float3 scaled = value / largest_component;
    return scaled * rsqrt(dot(scaled, scaled));
}

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    output.out_dto.tex_coord = input.texcoord;

    float3 transformed_normal = mul(
        (float3x3)u_push_constants.normal_matrix,
        input.normal);
    output.out_dto.normal = safe_normalize(transformed_normal);

    float4x4 mvp = mul(global_ubo.projection, mul(global_ubo.view, u_push_constants.model));
    output.position = mul(mvp, float4(input.position, 1.0));

    return output;
}
