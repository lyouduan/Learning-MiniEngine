#include "common.hlsli"
#include "LightingUtil.hlsli"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamLinearClamp : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    input.normal = normalize(input.normal);
    
    float3 toEyeW = normalize(passConstants.gEyePosW - input.positionW);
    float4 ambient = passConstants.gAmbientLight * materialConstants.gDiffuseAlbedo;
    
    const float shininess = 1.0f - materialConstants.gRoughness;
    
    // sample
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinearClamp, input.tex) * materialConstants.gDiffuseAlbedo;
    
    Material mat = { diffuseAlbedo, materialConstants.gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        input.normal, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    litColor.a = materialConstants.gDiffuseAlbedo.a;
    
    return gDiffuseMap.Sample(gsamLinearClamp, input.tex);
}