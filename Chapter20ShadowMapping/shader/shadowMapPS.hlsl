#include "common.hlsli"

float main(VertexOut input) : SV_DEPTH
{
    // return depth-z
    return input.depth;
}