#include "ssaoCommon.hlsli"
static const int gSampleCount = 16;

float NdcDepthToViewDepth(float z_ndc)
{
    float viewZ = cbssao.gProj[3][2] / (z_ndc - cbssao.gProj[2][2]);

    return viewZ;
}

float4 UVToView(float2 UV, float DNCDepth, float4x4 InvProj)
{
    float4 dnc = float4(UV.x * 2.0 - 1.0, 1.0 - UV.y * 2.0, DNCDepth, 1.0);
    
    float4 view = mul(dnc, InvProj);
    view /= view.w;
    
    return view;

}

float2 ViewToUV(float4 viewPos, float4x4 Proj)
{
    float4 dnc = mul(viewPos, Proj);
    dnc /= dnc.w;
    
    float2 uv = float2(0.5 + 0.5 * dnc.x, 0.5 - 0.5 * dnc.y);
    
    return uv;

}
// Determines how much the point R occludes the point P as a function of DistZ.
float OcclusionFunction(float DistZ)
{
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \ 
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps       Start           End       
	//
    float gOcclusionRadius = 0.05f;
    float gOcclusionFadeStart = 0.2f;
    float gOcclusionFadeEnd = 1.0f;
    float gSurfaceEpsilon = 0.005f;
    
    float Occlusion = 0.0f;
    if (DistZ > gSurfaceEpsilon)
    {
        float FadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        Occlusion = saturate((gOcclusionFadeEnd - DistZ) / FadeLength);
    }
	
    return Occlusion;
}

float main(VertexOut pin) : SV_TARGET
{
	// Get view space normal
    float3 normalV = NormalMap.Sample(gsamLinearClamp, pin.TexC).rgb;
    
	// Get view space P
    float pz = DepthMap.Sample(gsamLinearClamp, pin.TexC).r;
    //pz = NdcDepthToViewDepth(pz);
    //float3 p = (pz / pin.PosV.z) * pin.PosV;
    
    float3 p = UVToView(pin.TexC, pz, cbssao.gInvProj).xyz;
    
    float Occlusion = 0.0f;
    const float Phi = 3.1415 * (3.0 - sqrt(5.0f));
    for (int i = 0; i < gSampleCount; ++i)
    {
        float t = (i / float(gSampleCount - 1)) * 2 - 1.0;
        float Radius = sqrt(1.0f - t * t); // Radius at y
        float Theta = Phi * i; // Golden angle increment
        
        float3 Offset;
        Offset.x = Radius * cos(Theta);
        Offset.y = t; // y goes from 1 to - 1
        Offset.z = Radius * sin(Theta);
        
        // Flip offset if it is behind P.
        float Flip = sign(dot(Offset, normalV));
        
        // Sample point q
        float3 q = p + Flip * 0.05 * Offset;
        
        float2 uv = ViewToUV(float4(q, 1.0f), cbssao.gProj);
        
        float rz = DepthMap.Sample(gsamLinearClamp, uv).r;
        //rz = NdcDepthToViewDepth(rz);
        //
        //float3 r = (rz / q.z) * q;
        
        float3 r = UVToView(uv, rz, cbssao.gInvProj).xyz;
        
        // Test whether R occludes P
        float dp = max(dot(normalV, normalize(r - p)), 0.0);
        float occlusion = OcclusionFunction(p.z - r.z);
        
        Occlusion += dp * occlusion;
    }
    
    Occlusion /= gSampleCount;
    
    float AmbientAccess = 1.0f - Occlusion;
	//
    return saturate(pow(AmbientAccess, 6.0f));

}