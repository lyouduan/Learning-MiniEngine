#include "GScommon.hlsli"

#include "LightingUtil.hlsli"

Texture2DArray gDiffuseMap : register(t0);

SamplerState gsamLinearClamp : register(s0);

float4 main(GeoOut input) : SV_Target
{
    float3 uvw = float3(input.tex, input.PrimID % 4);

    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinearClamp, uvw) * matConstants.gDiffuseAlbedo;
// alpha tested
    clip(diffuseAlbedo.a - 0.1f);
    
    input.normalW = normalize(input.normalW);
    
    float3 toEyeW = passConstants.gEyePosW - input.posW;
    float distance = length(toEyeW);
    toEyeW /= distance; // normalize
    
    // 
    float4 ambient = passConstants.gAmbientLight * diffuseAlbedo;
    
    const float shininess = 1.0f - matConstants.gRoughness;
    
    Material mat = { diffuseAlbedo, matConstants.gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.posW,
        input.normalW, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    
    // fog
    float fogAmount = saturate((distance - passConstants.gFogStart) / passConstants.gFogRange);
    litColor = lerp(litColor, passConstants.gFogColor, fogAmount);
    
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}