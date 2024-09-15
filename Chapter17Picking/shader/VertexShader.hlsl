#include "common.hlsli"

VertexOut main(VertexIn input, uint instanceID : SV_InstanceID) 
{
    VertexOut output;
    
    InstanceData instData = gInstanceData[instanceID];
    float4x4 world = instData.gWorld;
    float4x4 texTransform = instData.gTexTransform;
    uint matIndex = instData.gMaterialIndex;
    
    output.MatIndex = matIndex;
    
    MaterialData matData = gMaterialData[matIndex];
    
    float4 posW = mul(float4(input.position, 1.0), world);
    output.positionW = posW.xyz;
    output.positionH = mul(posW, passConstants.gViewProj);
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    output.normal = mul(input.normal, (float3x3) world);
    
    float4 tex = mul(float4(input.tex, 0.0, 1.0), texTransform);
    output.tex = mul(tex, matData.gMatTransform).xy;
    
    return output;
}

