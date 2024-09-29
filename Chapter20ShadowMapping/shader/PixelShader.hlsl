#include "common.hlsli"
#include "LightingUtil.hlsli"

float PCF(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    // PCF
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    
    float dx = 1.0 / (float)width;
    const float2 offsets[9] =
    {
        float2(-dx, -dx),
        float2(0.0, -dx),
        float2(dx, -dx),
        float2(-dx, 0.0),
        float2(0.0, 0.0),
        float2(dx, 0.0),
        float2(-dx, dx),
        float2(0.0, dx),
        float2(dx, dx),
    };
    
    float percentLit = 0.0f;
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float depth = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy + offsets[i]).r;
        
        if (depth + bias > curDepth)
            percentLit += 1;
    }
    
    return percentLit / 9.0;
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

float VSM(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    float2 SampleValue = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy).xy;
    
    float Mean = SampleValue.x;
    
    // 切尔比夫不等式成立条件 t>u
    // 只有当当前片段的深度 t 大于或等于采样区域中的平均深度 μ 时，切比雪夫不等式才能有效地估算光源被遮挡的概率
    if (curDepth <= Mean + bias)
        return 1.0f;

    float Variance = SampleValue.y - Mean * Mean;
    
    float Diff = curDepth - Mean;
    
    float Result = Variance / (Variance + Diff * Diff);
    
    return clamp(Result, 0.0, 1.0);
    
}


float ESM(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    float exp_cd = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy).r;
    
    float f = exp(-80 * curDepth) * exp_cd;
    
    return saturate(f);
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
    
    float bias = 0.005;
    float3 shadowFactor = 1.0f;
    shadowFactor[0] = ESM(input.ShadowPosH);
    
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