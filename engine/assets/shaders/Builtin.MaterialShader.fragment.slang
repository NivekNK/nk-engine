struct LocalUniformObject
{
    float4 diffuse_color;
    float shininess;
};

static const uint max_point_light_count = 2;
static const uint render_view_mode_lighting_only = 1;
static const uint render_view_mode_normals = 2;

struct PointLight
{
    float3 position;
    float constant;
    float4 color;
    float linear;
    float quadratic;
    float2 padding;
};

struct GlobalUniformObject
{
    float4x4 projection;
    float4x4 view;
    float4 ambient_color;
    float3 directional_light_direction;
    float4 directional_light_color;
    float3 view_position;
    uint point_light_count;
    PointLight point_lights[max_point_light_count];
    uint render_view_mode;
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

float3 calculate_light(
    float3 light_color,
    float3 surface_color,
    float3 specular_sample,
    float3 normal,
    float3 view_direction,
    float3 surface_to_light,
    float shininess)
{
    float diffuse_factor = max(dot(normal, surface_to_light), 0.0);
    float3 diffuse = light_color * diffuse_factor * surface_color;
    if (!(diffuse_factor > 0.0)) {
        return diffuse;
    }

    float3 half_direction = safe_normalize(
        view_direction + surface_to_light);
    float specular_factor = pow(
        max(dot(normal, half_direction), 0.0),
        max(shininess, 1.0));
    return diffuse + light_color * specular_factor * specular_sample;
}

float3 calculate_point_light(
    PointLight light,
    float3 surface_color,
    float3 specular_sample,
    float3 normal,
    float3 view_direction,
    float3 world_position,
    float shininess)
{
    float3 to_light = light.position - world_position;
    float distance = length(to_light);
    if (!(distance > 1.0e-20) ||
        !(distance < 3.402823466e+38)) {
        return float3(0.0, 0.0, 0.0);
    }

    float attenuation_denominator = light.constant +
        light.linear * distance +
        light.quadratic * distance * distance;
    if (!(attenuation_denominator > 0.0) ||
        !(attenuation_denominator < 3.402823466e+38)) {
        return float3(0.0, 0.0, 0.0);
    }

    float3 contribution = calculate_light(
        light.color.rgb,
        surface_color,
        specular_sample,
        normal,
        view_direction,
        to_light / distance,
        shininess);
    return contribution / attenuation_denominator;
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

    if (global_ubo.render_view_mode == render_view_mode_normals) {
        return float4(normal * 0.5 + 0.5, 1.0);
    }

    float3 specular_sample = material_samplers[specular_sampler_index]
        .Sample(input.in_dto.tex_coord)
        .rgb;
    float3 lighting_surface_color = surface_color.rgb;
    if (global_ubo.render_view_mode == render_view_mode_lighting_only) {
        lighting_surface_color = float3(1.0, 1.0, 1.0);
        specular_sample = float3(1.0, 1.0, 1.0);
    }
    float3 view_direction = safe_normalize(
        global_ubo.view_position - input.in_dto.world_position);
    float3 accumulated_light =
        global_ubo.ambient_color.rgb * lighting_surface_color;

    accumulated_light += calculate_light(
        global_ubo.directional_light_color.rgb,
        lighting_surface_color,
        specular_sample,
        normal,
        view_direction,
        -safe_normalize(global_ubo.directional_light_direction),
        object_ubo.shininess);

    uint point_light_count = min(
        global_ubo.point_light_count,
        max_point_light_count);
    for (uint index = 0; index < point_light_count; ++index) {
        accumulated_light += calculate_point_light(
            global_ubo.point_lights[index],
            lighting_surface_color,
            specular_sample,
            normal,
            view_direction,
            input.in_dto.world_position,
            object_ubo.shininess);
    }
    return float4(accumulated_light, surface_color.a);
};
