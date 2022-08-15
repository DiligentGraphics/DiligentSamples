#include "Structures.fxh"

RWTexture2D<float4/* format=r16f */>    g_HeightMapUAV;
RWTexture2D<float4/* format=rgba16f */> g_NormalMapUAV;

cbuffer TerrainConstantsCB
{
    TerrainConstants g_Constants;
};

groupshared float s_Height[GROUP_SIZE_WITH_BORDER * GROUP_SIZE_WITH_BORDER];


float Hash13(in float3 p)
{
    float3 p3 = frac(p * 0.5337);
    p3 += dot(p3, p3.yzx + 38.514);
    return frac((p3.x + p3.y) * p3.z) * 2.0 - 1.0;
}

float Noise(const float3 pos)
{
    const float2 Offset = float2(0., 1.);
    const float3 iPoint = floor(pos);
    const float3 fPoint = frac(pos);
    const float3 Weight = fPoint * fPoint * (3.0 - 2.0 * fPoint);

    return lerp(lerp(lerp(Hash13(iPoint + Offset.xxx), Hash13(iPoint + Offset.yxx), Weight.x), lerp(Hash13(iPoint + Offset.xxy), Hash13(iPoint + Offset.yxy), Weight.x), Weight.z),
                lerp(lerp(Hash13(iPoint + Offset.xyx), Hash13(iPoint + Offset.yyx), Weight.x), lerp(Hash13(iPoint + Offset.xyy), Hash13(iPoint + Offset.yyy), Weight.x), Weight.z),
                Weight.y);
}

float FBM(in float3 pos, const float lacunarity, const float persistence, const int octaveCount)
{
    const float3x3 rot = float3x3( 0.00,  0.80,  0.60,
                                  -0.80,  0.36, -0.48,
                                  -0.60, -0.48,  0.64);

    float value = 0.0;
    float pers  = 1.0;

    [unroll] for (int octave = 0; octave < octaveCount; ++octave)
    {
        value += Noise(pos) * pers;
        pos = mul(pos, rot) * lacunarity;
        pers *= persistence;
    }
    return value;
}

float3 Turbulence(const float3 pos, const float power, const float lacunarity, const float persistence, const int octaveCount)
{
    float3 distort = float3(
        FBM(pos + float3(0.189422, 0.993713, 0.478164), lacunarity, persistence, octaveCount),
        FBM(pos + float3(0.404647, 0.276611, 0.923049), lacunarity, persistence, octaveCount),
        FBM(pos + float3(0.821228, 0.171096, 0.684280), lacunarity, persistence, octaveCount));
    return distort * power + pos;
}

float3 ReadPos(int2 LocalPos)
{
    int2   Id = LocalPos + 1;
    float3 Pos;
    Pos.xz = float2(LocalPos);
    Pos.y  = s_Height[Id.x + Id.y * GROUP_SIZE_WITH_BORDER];
    Pos *= g_Constants.Scale;
    return Pos;
}

[numthreads(GROUP_SIZE_WITH_BORDER, GROUP_SIZE_WITH_BORDER, 1)]
void CSMain(uint2 GroupId : SV_GroupID,
            uint2 LocalId : SV_GroupThreadID)
{
    const int2 LocalPos  = int2(LocalId) - 1;
    const int2 GlobalPos = int2(GroupId) * int2(GROUP_SIZE, GROUP_SIZE) + LocalPos;

    uint2 Dim;
    g_HeightMapUAV.GetDimensions(Dim.x, Dim.y);

    float2 UV = float2(GlobalPos - int2(Dim / 2)) * 2.0 / float2(Dim);
    UV *= g_Constants.NoiseScale;
    UV.x += g_Constants.XOffset;

    float3 Pos = float3(UV, g_Constants.Animation);
    {
        const float persistence = 1.0;
        const float lacunarity  = 0.314 * 0.0001;

        Pos = Turbulence(Pos, 1.0, lacunarity, persistence, TERRAIN_OCTAVES);
    }

    float Height = 0.0;
    {
        const float persistence = 0.286;
        const float lacunarity  = 3.3;

        Height = max(0.0, FBM(Pos, lacunarity, persistence, NOISE_OCTAVES));
    }

    s_Height[LocalId.x + LocalId.y * GROUP_SIZE_WITH_BORDER] = Height;

    GroupMemoryBarrierWithGroupSync();

    if (all(GreaterEqual(LocalPos, int2(0, 0))) && all(Less(LocalPos, int2(GROUP_SIZE, GROUP_SIZE))))
    {
        //    v2    v3
        //      \   |
        //        \ |
        //  v1 -----x v0

        const int2   Offset = int2(-1, 0);
        const float3 Scale  = 1.0 / float3(float(Dim.x), 1.0, float(Dim.y));
        const float3 v0     = ReadPos(LocalPos + Offset.yy) * Scale;
        const float3 v1     = ReadPos(LocalPos + Offset.xy) * Scale;
        const float3 v2     = ReadPos(LocalPos + Offset.xx) * Scale;
        const float3 v3     = ReadPos(LocalPos + Offset.yx) * Scale;
        float3       Norm   = float3(0., 0., 0.);

        // Calculate smooth normal
        Norm += cross(v1 - v0, v2 - v0); // 1-0, 2-0
        Norm += cross(v2 - v0, v3 - v0); // 2-0, 3-0
        Norm = normalize(Norm);
        Norm.y *= -1.0;

        g_HeightMapUAV[GlobalPos] = Height.xxxx;
        g_NormalMapUAV[GlobalPos] = float4(Norm, 0.0);
    }
}
