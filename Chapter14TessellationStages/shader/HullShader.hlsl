#include "common.hlsli"

PatchTess ConstantHS(
	InputPatch<VertexOut, NUM_CONTROL_POINTS> patch,
	uint PatchID : SV_PrimitiveID)
{
    PatchTess pt;

    float3 centerL = 0.25 * (patch[0].posL + patch[1].posL + patch[2].posL + patch[3].posL);
	
    float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;
	
	// distance from eye postion
    float d = distance(centerW, gEyePosW);

    const float d0 = 20.0f;
	const float d1 = 100.0f;
    float tess = 64.0f * saturate((d1 - d) / (d1 - d0));
	
    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.EdgeTess[3] = tess;
	
    pt.InsideTess[0] = tess;
    pt.InsideTess[1] = tess;
	
    return pt;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut main( 
	InputPatch<VertexOut, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
    HullOut hout;

    hout.PosL = ip[i].posL;

    return hout;
}
