Texture2D<float>                     g_SrcTex; // R channel contains: 0 - empty, 1 - wall
SamplerState                         g_SrcTex_sampler;
RWTexture2D<float /* format=r16f */> g_DstTex;

bool ReadWallFlag(float2 uv)
{
    return g_SrcTex.SampleLevel(g_SrcTex_sampler, uv, 0).r > 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint2 Dim;
    g_DstTex.GetDimensions(Dim.x, Dim.y);
    if (GlobalInvocationID.x >= Dim.x || GlobalInvocationID.y >= Dim.y)
        return;

    const int2   center     = int2(GlobalInvocationID.xy);
    const float2 scale      = 1.0 / float2(Dim);
    float        dist       = float(RADIUS * 2) * DIST_SCALE;
    bool         InsideWall = ReadWallFlag((float2(center) + 0.5) * scale);

    for (int y = -RADIUS; y <= RADIUS; ++y)
    {
        for (int x = -RADIUS; x <= RADIUS; ++x)
        {
            int2 pos    = center + int2(x, y);
            bool IsWall = ReadWallFlag((float2(pos) + 0.5) * scale);

            if (InsideWall != IsWall)
                dist = min(dist, length(float2(x, y)) * DIST_SCALE);
        }
    }

    dist = min(dist, float(RADIUS) * DIST_SCALE);

    if (InsideWall)
        dist = -dist;

    g_DstTex[center] = dist;
}
