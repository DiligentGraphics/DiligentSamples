#ifndef _CSG_HELPERS_HLSLI_
#define _CSG_HELPERS_HLSLI_

#include "ConstantBufferDeclarations.hlsli"

// Octahedron normal encoding helpers
float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

// Pack normal (oct16) + material index (5 bits) + object ID (8 bits) + IsExit (1 bit) into a single uint
// Layout: NormalX(8) | NormalY(8) | MaterialIdx(5) | ObjectID(8) | IsExit(1) | unused(2)
uint PackFragmentData(float3 Normal, uint MaterialIdx, uint ObjectID, uint IsExit)
{
    Normal /= (abs(Normal.x) + abs(Normal.y) + abs(Normal.z));
    float2 Oct = Normal.z >= 0.0 ? Normal.xy : OctWrap(Normal.xy);
    Oct = Oct * 0.5 + 0.5;

    uint nx = uint(clamp(Oct.x * 255.0, 0.0, 255.0));
    uint ny = uint(clamp(Oct.y * 255.0, 0.0, 255.0));
    return (nx) | (ny << 8u) | ((MaterialIdx & 0x1Fu) << 16u) | ((ObjectID & 0xFFu) << 21u) | ((IsExit & 0x1u) << 29u);
}

void UnpackFragmentData(uint Packed, out float3 Normal, out uint MaterialIdx, out uint ObjectID, out uint IsExit)
{
    float2 Oct;
    Oct.x = float((Packed >>  0u) & 0xFFu) / 255.0;
    Oct.y = float((Packed >>  8u) & 0xFFu) / 255.0;
    MaterialIdx = (Packed >> 16u) & 0x1Fu;
    ObjectID    = (Packed >> 21u) & 0xFFu;
    IsExit      = (Packed >> 29u) & 0x1u;

    Oct = Oct * 2.0 - 1.0;
    Normal = float3(Oct.x, Oct.y, 1.0 - abs(Oct.x) - abs(Oct.y));
    if (Normal.z < 0.0)
        Normal.xy = OctWrap(Normal.xy);
    Normal = normalize(Normal);
}

// Evaluate CSG boolean result for a pair of objects
bool EvalCSG(uint CSGOp, bool InsideA, bool InsideB)
{
    switch (CSGOp)
    {
        case CSG_OP_UNION:                return InsideA || InsideB;
        case CSG_OP_SUBTRACTION:          return InsideA && !InsideB;
        case CSG_OP_INTERSECTION:         return InsideA && InsideB;
        case CSG_OP_SYMMETRIC_DIFFERENCE: return InsideA != InsideB;
        default:                          return InsideA || InsideB;
    }
}

#endif // _CSG_HELPERS_HLSLI_
