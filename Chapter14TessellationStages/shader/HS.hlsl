#include "common.hlsli"

PatchTess CalcHSPatchConstants(
	InputPatch<VertexOut, 16> ip,
	uint PatchID : SV_PrimitiveID)
{
    PatchTess pt;

    pt.EdgeTess[0] = 25;
    pt.EdgeTess[1] = 25;
    pt.EdgeTess[2] = 25;
    pt.EdgeTess[3] = 25;
	
    pt.InsideTess[0] = 25;
    pt.InsideTess[1] = 25;

    return pt;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("CalcHSPatchConstants")]
[maxtessfactor(64.0f)]
HullOut main(
	InputPatch<VertexOut, 16> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
    HullOut hout;

    hout.PosL = ip[i].posL;

    return hout;
}
