#include "common.hlsli"

float4 main(VertexOut input) : SV_TARGET
{
    MaterialData matData = gMaterialData[objConstants.gMaterialIndex];
    float4 diffuseAlbedo = matData.gDiffuseAlbedo;
    float3 fresnelR0 = matData.gFresnelR0;
    float roughness = matData.gRoughness;
    
    uint diffuseMapIndex = matData.gDiffuseMapIndex;
    uint normalMapIndex = matData.gNormalMapIndex;
    
    
    // normal located world space
    input.normal = normalize(input.normal);

    // trasfer to view space
    float3 normalV = mul(input.normal, (float3x3)passConstants.gView);
    
    return float4(normalV, 0.0);
}