#include "common.hlsli"

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    //MaterialData matData = gMaterialData[objConstants.gMaterialIndex];
    
    float4 posW = mul(float4(input.position, 1.0), objConstants.gWorld);
    
    output.positionW = posW.xyz;
    output.positionH = mul(posW, mul(passConstants.gView, passConstants.gProj));
    
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    output.normal = mul(input.normal, (float3x3)objConstants.gWorld);
    
    output.tangentW = mul(input.tangentU, (float3x3) objConstants.gWorld);
    
    float4 tex = mul(float4(input.tex, 0.0, 1.0), objConstants.gTexTransform);
    output.tex = mul(tex, objConstants.gMatTransform).xy;
   
    // 将世界坐标的点，转换到阴影贴图的纹理坐标空间
    output.ShadowPosH = mul(posW, passConstants.gShadowTransform);
    
    // //0-10, 10-50, 50-100, 100-1000
    float cascadePlaneDistances[3] = { 10, 20, 100.0 };
    int layer = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (output.positionH.z < cascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }
    if (layer == -1)
    {
        layer = 3;
    }
    
    output.index = layer;
    output.CSMPosH = mul(posW, passConstants.gCSShadowTransform[layer]);
    
    
    return output;
}

