// -----------------------------------------------------------
// Estructuras y Bindings
// -----------------------------------------------------------

struct PointLight {
    float4 position; 
    float4 color;    
};

struct GlobalUbo {
    float4x4 projection;
    float4x4 view;
    float4x4 invView;
    float4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
};

struct PushConstants {
    float4 position;
    float4 color;
    float radius;
};

// Slang permite definir arrays constantes estáticos
static const float2 OFFSETS[6] = {
    float2(-1.0, -1.0),
    float2(-1.0, 1.0),
    float2(1.0, -1.0),
    float2(1.0, -1.0),
    float2(-1.0, 1.0),
    float2(1.0, 1.0)
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> ubo;

[[vk::push_constant]]
PushConstants push;

// -----------------------------------------------------------
// Output
// -----------------------------------------------------------

struct VertexOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 fragOffset;
};

// -----------------------------------------------------------
// Main Vertex
// -----------------------------------------------------------

[shader("vertex")]
VertexOutput main(uint vertexIndex : SV_VertexID) {
    VertexOutput output;
    
    output.fragOffset = OFFSETS[vertexIndex];

    // En Slang/HLSL, matriz[0] retorna la primera fila.
    // La fila 0 de la View Matrix es el vector "Right" de la cámara en mundo.
    // La fila 1 de la View Matrix es el vector "Up" de la cámara en mundo.
    float3 cameraRightWorld = ubo.view[0].xyz;
    float3 cameraUpWorld    = ubo.view[1].xyz;

    float3 positionWorld = push.position.xyz
        + push.radius * output.fragOffset.x * cameraRightWorld
        + push.radius * output.fragOffset.y * cameraUpWorld;

    // Multiplicación: Proj * View * WorldPos
    output.position = mul(ubo.projection, mul(ubo.view, float4(positionWorld, 1.0)));
    
    return output;
}
