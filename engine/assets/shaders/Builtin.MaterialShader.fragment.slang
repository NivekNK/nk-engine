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
Sampler2D material_samplers[2];

static const uint diffuse_sampler_index = 0;
static const uint specular_sampler_index = 1;

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

    float3 normal = safe_normalize(input.in_dto.normal);
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
