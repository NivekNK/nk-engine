// -----------------------------------------------------------
// Structures & Bindings
// -----------------------------------------------------------
// Note: These must match the layout in the vertex shader 
// to ensure the Pipeline Layout is compatible.

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
    float4x4 modelMatrix;
    float4x4 normalMatrix;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> ubo;

[[vk::push_constant]]
PushConstants push;

// -----------------------------------------------------------
// Inputs
// -----------------------------------------------------------

struct FragmentInput {
    // The system position (gl_Position) is implicitly passed but 
    // usually not needed in pixel shader logic unless doing screen-space effects.
    
    [[vk::location(0)]] float3 fragColor;
    [[vk::location(1)]] float3 fragPosWorld;
    [[vk::location(2)]] float3 fragNormalWorld;
};

// -----------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------

[shader("fragment")]
float4 main(FragmentInput input) : SV_Target {
    float3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    float3 specularLight = float3(0.0);
    float3 surfaceNormal = normalize(input.fragNormalWorld);

    // Extract camera position from Inverse View Matrix
    float3 cameraPosWorld = ubo.invView[3].xyz;
    float3 viewDirection = normalize(cameraPosWorld - input.fragPosWorld);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        float3 directionToLight = light.position.xyz - input.fragPosWorld;
        
        // Distance squared and attenuation
        float distSq = dot(directionToLight, directionToLight);
        float attenuation = 1.0 / distSq; 
        
        directionToLight = normalize(directionToLight);

        // Diffuse
        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0.0);
        float3 intensity = light.color.xyz * light.color.w * attenuation;

        diffuseLight += intensity * cosAngIncidence;

        // Specular lighting (Blinn-Phong)
        float3 halfAngle = normalize(directionToLight + viewDirection);
        float blinnTerm = dot(surfaceNormal, halfAngle);
        blinnTerm = clamp(blinnTerm, 0.0, 1.0);
        blinnTerm = pow(blinnTerm, 512.0);
        
        specularLight += intensity * blinnTerm;
    }
    
    return float4(diffuseLight * input.fragColor + specularLight * input.fragColor, 1.0);
}
