struct LocalUniformObject
{
    float4 diffuse_color;
    float shininess;
};

struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
    float4 ambient_color;
    float3 directional_light_direction;
    float4 directional_light_color;
    float3 view_position;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUniformObject> global_ubo;

[[vk::binding(0, 1)]]
ConstantBuffer<LocalUniformObject> object_ubo;

[[vk::binding(1, 1)]]
Sampler2D material_samplers[3];

static const uint diffuse_sampler_index = 0;
static const uint specular_sampler_index = 1;
static const uint normal_sampler_index = 2;

struct dto
{
    float2 tex_coord;
    float3 normal;
    float3 world_position;
    float4 tangent;
};

struct FragmentInput
{
    [[vk::location(1)]] dto in_dto;
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

float4 main(FragmentInput input) : SV_Target0 {
    float4 sampled_color = material_samplers[diffuse_sampler_index].Sample(
        input.in_dto.tex_coord);
    float4 surface_color = object_ubo.diffuse_color * sampled_color;

    float3 geometric_normal = safe_normalize(input.in_dto.normal);
    float3 tangent = input.in_dto.tangent.xyz - geometric_normal * dot(
        geometric_normal,
        input.in_dto.tangent.xyz);
    tangent = safe_normalize(tangent);
    float3 bitangent = safe_normalize(cross(geometric_normal, tangent)) *
        input.in_dto.tangent.w;
    float3 tangent_normal =
        material_samplers[normal_sampler_index]
            .Sample(input.in_dto.tex_coord)
            .rgb * 2.0 - 1.0;
    float3 normal = safe_normalize(
        tangent * tangent_normal.x +
        bitangent * tangent_normal.y +
        geometric_normal * tangent_normal.z);
    float3 light_to_surface = safe_normalize(
        global_ubo.directional_light_direction);
    float diffuse_factor = max(dot(normal, -light_to_surface), 0.0);

    float3 ambient = global_ubo.ambient_color.rgb * surface_color.rgb;
    float3 diffuse = global_ubo.directional_light_color.rgb *
        diffuse_factor * surface_color.rgb;
    float3 specular = float3(0.0, 0.0, 0.0);
    if (diffuse_factor > 0.0) {
        float3 view_direction = safe_normalize(
            global_ubo.view_position - input.in_dto.world_position);
        float3 half_direction = safe_normalize(
            view_direction - light_to_surface);
        float specular_factor = pow(
            max(dot(normal, half_direction), 0.0),
            max(object_ubo.shininess, 1.0));
        float3 specular_sample =
            material_samplers[specular_sampler_index]
                .Sample(input.in_dto.tex_coord)
                .rgb;
        specular = global_ubo.directional_light_color.rgb *
            specular_factor * specular_sample;
    }
    return float4(ambient + diffuse + specular, surface_color.a);
};
