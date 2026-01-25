struct LocalUniformObject
{
    float4 diffuse_color;
};

[[vk::binding(0, 1)]]
ConstantBuffer<LocalUniformObject> object_ubo;

float4 main() : SV_Target0 {
    return object_ubo.diffuse_color;
};
