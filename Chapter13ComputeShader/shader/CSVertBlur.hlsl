#include "CSCommon.hlsli"
[numthreads(1, N, 1)]
void main(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
	// Put in an array for each indexing.
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//
	
	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
    if (groupThreadID.y < gBlurRadius)
    {
		// Clamp out of bound samples that occur at image borders.
        int y = max(dispatchThreadID.y - gBlurRadius, 0);
        gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
    }
    if (groupThreadID.y >= N - gBlurRadius)
    {
		// Clamp out of bound samples that occur at image borders.
        int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
        gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
    }
	
	// Clamp out of bound samples that occur at image borders.
    gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];


	// Wait for all threads to finish.
    GroupMemoryBarrierWithGroupSync();
	
	//
	// Now blur each pixel.
	//

    float4 blurColor = float4(0, 0, 0, 0);
	
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        int k = groupThreadID.y + gBlurRadius + i;
		
        blurColor += weights[i + gBlurRadius] * gCache[k];
    }
	
    gOutput[dispatchThreadID.xy] = blurColor;
}