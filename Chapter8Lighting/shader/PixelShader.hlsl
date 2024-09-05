#include "common.hlsli"
#include "LightingUtil.hlsli"

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif
float4 main(VertexOut input) : SV_TARGET
{
    input.normal = normalize(input.normal);
    
    float3 toEyeW = normalize(passConstants.gEyePosW - input.positionW);
    float4 ambient = passConstants.gAmbientLight * materialConstants.gDiffuseAlbedo;
    
    const float shininess = 1.0f - materialConstants.gRoughness;
    
    Material mat = { materialConstants.gDiffuseAlbedo, materialConstants.gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        input.normal, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    litColor.a = materialConstants.gDiffuseAlbedo.a;
    
    return litColor;
}