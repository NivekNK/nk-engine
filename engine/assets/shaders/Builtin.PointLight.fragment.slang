// -----------------------------------------------------------
// Estructuras y Bindings
// (Deben coincidir con el Vertex Shader)
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

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> ubo;

[[vk::push_constant]]
PushConstants push;

static const float M_PI = 3.1415926538;

// -----------------------------------------------------------
// Input
// -----------------------------------------------------------

struct FragmentInput {
    [[vk::location(0)]] float2 fragOffset;
};

// -----------------------------------------------------------
// Main Fragment
// -----------------------------------------------------------

[shader("fragment")]
float4 main(FragmentInput input) : SV_Target {
    // Cálculo de distancia (length también funcionaría)
    float dis = sqrt(dot(input.fragOffset, input.fragOffset));
    
    if (dis >= 1.0) {
        discard;
    }

    float cosDis = 0.5 * (cos(dis * M_PI) + 1.0); // Rango 1 -> 0
    
    return float4(push.color.xyz + 0.5 * cosDis, cosDis);
}
