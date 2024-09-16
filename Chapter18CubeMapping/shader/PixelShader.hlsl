#include "common.hlsli"
#include "LightingUtil.hlsli"


float4 main(VertexOut input) : SV_TARGET
{
    MaterialData matData = gMaterialData[objConstants.gMaterialIndex];
    float4 diffuseAlbedo = matData.gDiffuseAlbedo;
    float3 fresnelR0 = matData.gFresnelR0;
    float roughness = matData.gRoughness;
    
    uint diffuseMapIndex = matData.gDiffuseMapIndex;
    
    
    diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamLinearClamp, input.tex);
// alpha tested
    clip(diffuseAlbedo.a - 0.1f);
    
    input.normal = normalize(input.normal);
    
    float3 toEyeW = passConstants.gEyePosW - input.positionW;
    float distance = length(toEyeW);
    toEyeW /= distance; // normalize
    
    // 
    float4 ambient = passConstants.gAmbientLight * diffuseAlbedo;
    
    const float shininess = 1.0f - roughness;
    
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        input.normal, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    
    
    float3 r = reflect(-toEyeW, input.normal);
    float4 reflectionColor = gCubeMap.Sample(gsamLinearClamp, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, input.normal, r);
    
    litColor.rgb += shininess * reflectionColor.rgb * fresnelFactor;
    
    
    // fog
    //float fogAmount = saturate((distance - passConstants.gFogStart) / passConstants.gFogRange);
    //litColor = lerp(litColor, passConstants.gFogColor, fogAmount);
    
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}