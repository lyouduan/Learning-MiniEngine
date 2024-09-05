struct PassConstants
{
    float4x4 MVP;
};

ConstantBuffer<PassConstants> passConstants : register(b0);

struct VertexIn
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
    float4 color : COLOR;
    float4 position : SV_Position; // only omit at last one
};

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    output.position = mul(float4(input.position, 1.0), passConstants.MVP);
    output.color = input.color;
    
    return output;
}

