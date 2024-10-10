#include "common.hlsli"

struct PixelOutput
{
    float4 Normal : SV_TARGET0;
    float4 WorldPos : SV_TARGET1;
};

PixelOutput main(VertexOut input) //: SV_TARGET
{
    PixelOutput pout;
    
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
    
    pout.Normal = float4(normalV, 0.0);
    
    // test deferred rendering
    pout.WorldPos = float4(input.positionW, 0.0);
    
    return pout;

}