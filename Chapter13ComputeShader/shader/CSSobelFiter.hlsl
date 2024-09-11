
Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.258f, 0.114f));
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 c[3][3];
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int2 xy = dispatchThreadID.xy + int2(j - 1, i - 1);
            c[i][j] = gInput[xy];
        }
    }
    
    // This kernel detects edges in the vertical direction
    // [-1, 0, 1]
    // [-2, 0, 2]
    // [-1, 0, 1]
    float4 Gx = -1.0 * c[0][0] - 2.0 * c[1][0] - 1.0 * c[2][0] + 1.0 * c[0][2] + 2.0 * c[1][2] + 1.0 * c[2][2];
    
     // This kernel detects edges in the horizontal  direction
    // [ 1,  2,  1]
    // [ 0,  0,  0]
    // [-1, -2, -1]
    float4 Gy = -1.0 * c[2][0] - 2.0 * c[2][1] - 1.0 * c[2][1] + 1.0 * c[0][0] + 2.0 * c[0][1] + 1.0 * c[0][2];

    // 灰度图梯度变化率
    float4 mag = sqrt(Gx * Gx + Gy * Gy);
    
    mag = 1.0f - saturate(CalcLuminance(mag.rgb));
    
    gOutput[dispatchThreadID.xy] = mag;
}