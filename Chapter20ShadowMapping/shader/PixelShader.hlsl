#include "common.hlsli"
#include "LightingUtil.hlsli"

float CalcShadowFactor(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float depth = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy).r;
    
    return depth + 0.005 < shadowPosH.z ? 0 : 1;
}

float3 TangentToWorldSpace(float3 normalMapSample, float3 tangent, float3 normal)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // TBN
    float3 N = normal;
    float3 T = normalize(tangent - dot(tangent, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    
    float3 normalW = mul(normalT, TBN);
 
    return normalW;
}

float4 main(VertexOut input) : SV_TARGET
{
    MaterialData matData = gMaterialData[objConstants.gMaterialIndex];
    float4 diffuseAlbedo = matData.gDiffuseAlbedo;
    float3 fresnelR0 = matData.gFresnelR0;
    float roughness = matData.gRoughness;
    
    uint diffuseMapIndex = matData.gDiffuseMapIndex;
    uint normalMapIndex = matData.gNormalMapIndex;
    
    input.normal = normalize(input.normal);
    
    float4 normalMapSample = gNormalMap[normalMapIndex].Sample(gsamLinearClamp, input.tex);
    
    float3 normalW = TangentToWorldSpace(normalMapSample.rgb, input.tangentW, input.normal);
    //float normalW = input.normal;
    
    diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamLinearClamp, input.tex);
    
// alpha tested
    clip(diffuseAlbedo.a - 0.1f);
    
    float3 toEyeW = passConstants.gEyePosW - input.positionW;
    float distance = length(toEyeW);
    toEyeW /= distance; // normalize
    
    // 
    float4 ambient = passConstants.gAmbientLight * diffuseAlbedo;
    
    const float shininess = (1.0f - roughness);
    
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    
    float3 shadowFactor = 1.0f;
    shadowFactor[0] = CalcShadowFactor(input.ShadowPosH);
    
    float4 directLight = ComputeLighting(passConstants.Lights, mat, input.positionW,
        normalW, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    
    // reflection 
    float3 r = reflect(-toEyeW, normalW);
    float4 reflectionColor = gCubeMap.Sample(gsamLinearClamp, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, normalW, r);
    
    litColor.rgb += shininess * reflectionColor.rgb * fresnelFactor;
    
    // fog
    //float fogAmount = saturate((distance - passConstants.gFogStart) / passConstants.gFogRange);
    //litColor = lerp(litColor, passConstants.gFogColor, fogAmount);
    
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}