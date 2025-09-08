struct FSInput {
    float3 worldPos : TEXCOORD0;
};

float4 main(FSInput input) : SV_Target0 {
    return float4(input.worldPos + 0.5, 1.0);
};
