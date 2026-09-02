struct LocalUniformObject
{
    float4 diffuse_color;
};

[[vk::binding(0, 1)]]
ConstantBuffer<LocalUniformObject> object_ubo;

[[vk::binding(1, 1)]]
Sampler2D diffuse_sampler;

struct dto
{
    float2 tex_coord;
};

struct FragmentInput
{
    [[vk::location(1)]] dto in_dto;
};

float4 main(FragmentInput input) : SV_Target0
{
    return object_ubo.diffuse_color *
        diffuse_sampler.Sample(input.in_dto.tex_coord);
}
