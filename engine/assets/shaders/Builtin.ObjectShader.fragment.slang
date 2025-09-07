struct FSOutput {
    float4 color : SV_Target0;
};

FSOutput main() {
    FSOutput output;
    output.color = float4(1.0, 1.0, 1.0, 1.0);
    return output;
};
