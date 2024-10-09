#include "common.hlsli"

float main(VertexOut input) : SV_DEPTH
{
    // return depth-z
    
    //  ESM: exp(cz), c=-10
    return input.depth;
}