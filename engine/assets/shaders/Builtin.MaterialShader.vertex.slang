struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
    float4 ambient_color;
    float3 directional_light_direction;
    float4 directional_light_color;
    float3 view_position;
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
    [[vk::location(3)]] float4 tangent : TANGENT;
};

struct dto 
{
    float2 tex_coord;
    float3 normal;
    float3 world_position;
    float4 tangent;
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

    float3 transformed_tangent = mul(
        (float3x3)u_push_constants.model,
        input.tangent.xyz);
    transformed_tangent -= output.out_dto.normal * dot(
        output.out_dto.normal,
        transformed_tangent);
    float model_orientation = determinant(
        (float3x3)u_push_constants.model) < 0.0 ? -1.0 : 1.0;
    output.out_dto.tangent = float4(
        safe_normalize(transformed_tangent),
        input.tangent.w * model_orientation);

    float4 world_position = mul(
        u_push_constants.model,
        float4(input.position, 1.0));
    output.out_dto.world_position = world_position.xyz;
    output.position = mul(
        global_ubo.projection,
        mul(global_ubo.view, world_position));

    return output;
}
