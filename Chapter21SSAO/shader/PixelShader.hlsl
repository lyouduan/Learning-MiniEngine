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


float PCSS(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    
    float dx = 1.0 / (float) width;
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
    
    // 1. 计算平均遮挡深度
    float average_depth = 0.0f;
    int SampleCount = 0;
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float depth = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy + offsets[i]).r;
        
        // 平均遮挡深度
        if (depth + bias < curDepth)
        {
            average_depth += depth;
            SampleCount++;
        }
    }
    
    // 无遮挡
    if (SampleCount < 1)
        return 1.0;
    
    average_depth /= SampleCount;
    
    // 2. 计算滤波核大小
    float lightSize = 10.0;
    float RadiusSize = lightSize * (curDepth - average_depth) / average_depth;
    RadiusSize = clamp(RadiusSize, 1.0, 5.0);
    
    // 3. PCF
    float percentLit = 0.0f;
    SampleCount = 0;
    [unroll]
    for (int k = -RadiusSize; k <= RadiusSize; ++k)
    {
        for (int m = -RadiusSize; m <= RadiusSize; ++m)
        {
            float2 offset = float2(k * dx, m * dx);
            float depth = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy + offset).r;
            if (depth + bias > curDepth)
                percentLit += 1;
            
            SampleCount++;
        }
    }
    
    return percentLit / SampleCount;
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

float chebyshevUpperBound(float2 moments, float depth)
{
    if (depth <= moments.x + 0.005)
        return 1.0f;
    
    float variance = moments.y - (moments.x * moments.x);
    
    float diff = depth - moments.x;
    
    float Result = variance / (variance + diff * diff);
    
    return clamp(Result, 0.0, 1.0);
}

float VSM(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    float2 SampleValue = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy).xy;
    
    return chebyshevUpperBound(SampleValue, curDepth);
}


float VSSM(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    // 1. 计算平均遮挡深度
    // 利用VSM加速求平均遮挡深度
    float average_depth = 0.0f;
    float2 SampleValue = gShadowMap.SampleLevel(gsamLinearClamp, shadowPosH.xy, 0.0).xy;
    
    // p1* Zunocc + p2 * Zocc = Zavg
    // 得到未遮挡深度的概率
    float p1 = chebyshevUpperBound(SampleValue, curDepth);
    // 遮挡深度的概率
    float p2 = 1 - p1;
    // 假设未遮挡深度等于当前深度
    float Zunocc = curDepth;
    // z-mean
    float Zavg = SampleValue.x;
    float Zocc = (Zavg - p1 * Zunocc) / p2;
    
    average_depth = Zocc;
    
    // 2. 计算滤波核大小
    float lightSize = 50.0;
    float RadiusSize = lightSize * (curDepth - average_depth) / average_depth;
    RadiusSize = clamp(RadiusSize, 1.0, 5.0);
   
    // 3. VSM
    float mipLevel = RadiusSize * 4.0 / 5.0;
    float2 moment = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy, RadiusSize).xy;
    
    return chebyshevUpperBound(moment, curDepth);
}


float ESM(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float curDepth = shadowPosH.z;
    
    float exp_cd = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy).r;
    
    float f = exp(-80 * curDepth) * exp_cd;
    
    return saturate(f);
}

float Wrap(float x, float c)
{
    return exp(c * x);
}

float EVSM(float4 shadowPosH, float bias)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    // exp偏移
    float curDepth = shadowPosH.z;
    //curDepth = 2.0 * curDepth - 1.0f;
    float2 Exp_depth = float2(Wrap(curDepth, 30), -Wrap(curDepth, -30));
    
    float4 SampleValue = gShadowMap.Sample(gsamLinearClamp, shadowPosH.xy);
    
    float p1 = chebyshevUpperBound(SampleValue.xy, Exp_depth.x);
    float p2 = chebyshevUpperBound(SampleValue.zw, Exp_depth.y);
    
    return min(p1, p2);
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
    shadowFactor[0] = PCF(input.ShadowPosH, bias);
    //shadowFactor[0] = VSSM(input.ShadowPosH, bias);
    //shadowFactor[0] = PCSS(input.ShadowPosH, bias);
    //shadowFactor[0] = EVSM(input.ShadowPosH, bias);
    //shadowFactor[0] = VSM(input.ShadowPosH, bias);
    //shadowFactor[0] = ESM(input.ShadowPosH);
    
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