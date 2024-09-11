#include "GScommon.hlsli"

[maxvertexcount(4)]
void main(
	point VertexOut gin[1], 
	uint primID : SV_PrimitiveID,
	inout TriangleStream< GeoOut > triStream)
{
	
	// normal toward camera
    float3 up = float3(0.0, 1.0, 0.0);
    float3 lookup = passConstants.gEyePosW - gin[0].centerW;
    lookup = normalize(lookup);
    lookup.y = -0.0f;
    float3 right = cross(up, lookup);
    
	// 计算四边形 四个顶点
    float halfWidth = 0.5 * gin[0].sizeW.x;
    float halfHeight = 0.5 * gin[0].sizeW.y;
	
    float4 v[4];
    v[0] = float4(gin[0].centerW + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(gin[0].centerW + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(gin[0].centerW - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(gin[0].centerW - halfWidth * right + halfHeight * up, 1.0f);
	
	// 纹理 ?
    float2 texC[4] =
    {
        float2(0.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0)
    };
	
    // 顶点变换到clip space
    GeoOut gout;
    [unroll]
	for (uint i = 0; i < 4; i++)
	{
        gout.posW = v[i].xyz;
        gout.posH = mul(v[i], passConstants.gViewProj);
        gout.normalW = lookup;
        gout.tex = texC[i];
        gout.PrimID = primID;
        
        triStream.Append(gout);
    }
}