#include "common.hlsli"
#include "LightingUtil.hlsli"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamLinearClamp : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinearClamp, input.tex) * matConstants.gDiffuseAlbedo;
// alpha tested
    clip(diffuseAlbedo.a - 0.1f);
    
    input.normal = normalize(input.normal);
    
    float3 toEyeW = passConstants.gEyePosW - input.positionW;
    float distance = length(toEyeW);
    toEyeW /= distance; // normalize
    
    // 
    float4 ambient = passConstants.gAmbientLight * diffuseAlbedo;
    
    const float shininess = 1.0f - matConstants.gRoughness;
    
    Material mat = { diffuseAlbedo, matConstants.gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        input.normal, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    
    // fog
    //float fogAmount = saturate((distance - passConstants.gFogStart) / passConstants.gFogRange);
    //litColor = lerp(litColor, passConstants.gFogColor, fogAmount);
    
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}