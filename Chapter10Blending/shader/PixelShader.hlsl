#include "common.hlsli"
#include "LightingUtil.hlsli"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamLinearClamp : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinearClamp, input.tex) * matConstants.gDiffuseAlbedo;
    
    clip(diffuseAlbedo.a - 0.1f);
    
    input.normal = normalize(input.normal);
    
    float3 toEyeW = normalize(passConstants.gEyePosW - input.positionW);
    
    // sample
    float4 ambient = passConstants.gAmbientLight * diffuseAlbedo * 0.1;
    
    const float shininess = 1.0f - matConstants.gRoughness;
    
    Material mat = { diffuseAlbedo, matConstants.gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        input.normal, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}