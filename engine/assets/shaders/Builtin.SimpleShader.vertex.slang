// -----------------------------------------------------------
// Structures & Bindings
// -----------------------------------------------------------

struct PointLight {
    float4 position; // w ignored
    float4 color;    // w is intensity
};

struct GlobalUbo {
    float4x4 projection;
    float4x4 view;
    float4x4 invView;
    float4 ambientLightColor; // w is intensity
    PointLight pointLights[10];
    int numLights;
};

struct PushConstants {
    float4x4 modelMatrix;
    float4x4 normalMatrix;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> ubo;

[[vk::push_constant]]
PushConstants push;

// -----------------------------------------------------------
// Inputs & Outputs
// -----------------------------------------------------------

struct VertexInput {
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 color;
    [[vk::location(2)]] float3 normal;
    [[vk::location(3)]] float2 uv;
};

struct VertexOutput {
    // SV_Position is the equivalent of gl_Position
    float4 position : SV_Position;
    
    [[vk::location(0)]] float3 fragColor;
    [[vk::location(1)]] float3 fragPosWorld;
    [[vk::location(2)]] float3 fragNormalWorld;
    [[vk::location(3)]] float2 fragUv;
};

// -----------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------

[shader("vertex")]
VertexOutput main(VertexInput input) {
    VertexOutput output;

    // Calculate World Position
    // Slang uses mul(matrix, vector)
    float4 positionWorld = mul(push.modelMatrix, float4(input.position, 1.0));
    
    // Calculate Clip Position: Proj * View * World
    output.position = mul(ubo.projection, mul(ubo.view, positionWorld));
    
    // Calculate Normal in World Space
    // Cast to float3x3 to extract rotation/scale part
    float3x3 normalMat = (float3x3)push.normalMatrix;
    
    output.fragNormalWorld = normalize(mul(normalMat, input.normal));
    output.fragPosWorld = positionWorld.xyz;
    output.fragColor = input.color;
    output.fragUv = input.uv;

    return output;
}
