#include "common.hlsli"

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    output.posL = input.posL;
    
    return output;
}

