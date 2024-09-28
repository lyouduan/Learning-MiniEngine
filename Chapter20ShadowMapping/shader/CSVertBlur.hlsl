#include "CSCommon.hlsli"

[numthreads(1, N, 1)]
void main(int3 groupThreadID : SV_GroupThreadID,
                int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
    
    // 填入本地线程存储区，减少带宽负载
    // 对N个像素模糊，需要R + N + R个像素
    
    // 处理左边界
    if(groupThreadID.y < gBlurRadius)
    {
        int y = max(dispatchThreadID.y - gBlurRadius, 0);
        gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
    }
    // 处理右边界
    if(groupThreadID.y >= N - gBlurRadius)
    {
        int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y - 1);
        gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
    }
    
    
    // 正常范围，偏移gBlurRadius个像素，同时防止图像边界越界
    gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy - 1)];
    
    // 等待所有线程完成
    GroupMemoryBarrierWithGroupSync();
    
    // 处理每个像素
    float2 blurColor = float2(0, 0);
    
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        int k = groupThreadID.y + gBlurRadius + i;
        blurColor += weights[i + gBlurRadius] * gCache[k];
    }
    
    gOutput[dispatchThreadID.xy] = blurColor;
}