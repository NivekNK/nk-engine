[[vk::binding(0, 0)]]
cbuffer GlobalUniformObject : register(b0, space0)
{
    float4x4 projection;
    float4x4 view;
};

struct VSInput {
    float3 position : POSITION;
};

struct VSOutput {
    float4 position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float4 position = float4(input.position, 1.0);
    output.position = mul(projection, mul(view, position));
    return output;
}
