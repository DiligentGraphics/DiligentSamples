#include "Structures.fxh"

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEX_COORD;
};

cbuffer cbMapConstants
{
    MapConstants g_MapConstants;
};

#ifdef VERTEX_SHADER
void VSmain(in uint vid : SV_VertexID,
            out PSInput PSIn)
{
    PSIn.UV = float2(vid & 1, vid >> 1);

    float2 XRange = g_MapConstants.ScreenRectLR;
    float2 YRange = g_MapConstants.ScreenRectTB;

    PSIn.Pos = float4(lerp(XRange.x, XRange.y, PSIn.UV.x), lerp(YRange.x, YRange.y, PSIn.UV.y), 0.0, 1.0);
}
#endif


#ifdef PIXEL_SHADER
cbuffer cbPlayerConstants
{
    PlayerConstants g_PlayerConstants;
};
Texture2D<float4> g_SDFMap; // R16
SamplerState      g_SDFMap_sampler;

static const float3 WallColor         = float3(0.0, 0.0, 1.0);
static const float3 PlayerColor       = float3(0.0, 2.0, 0.0);
static const float3 AmbientLightColor = float3(0.3, 0.4, 0.5) * 0.7;
static const float3 FlashLightColor   = float3(1.0, 1.0, 1.0) * 0.6;
static const float3 AmbientLight      = float(0.001).xxx;
static const float3 TeleportColor     = float3(0.4, 0.0, 0.0);
static const float3 TeleportWaveColor = float3(0.5, 0.2, 0.0);

float3 Blend(float3 src, float3 dst, float factor)
{
    return lerp(src, dst, factor);
}

// returns triangle wave /\/\ 
float BumpStep(float x, float edge0, float edge1)
{
    x = saturate((x - edge0) / (edge1 - edge0));
    return 1.0 - abs(x - 0.5) * 2.0;
}

// returns distance (sphere radius) in pixels
float ReadSDF(float2 pos)
{
    float SDFScale = 0.75; // calculated SDF may be a little bit inaccurate
    return g_SDFMap.SampleLevel(g_SDFMap_sampler, pos * g_MapConstants.MapToUV, 0).r * SDFScale;
}

// returns 1 if light is visible
float TraceRay(float2 LightPos, float2 Origin, bool InsideWall)
{
    const float  TMax    = max(distance(LightPos, Origin) - 0.001, 0.001); // distance from current pixel to light source.
    const float  MinDist = 0.00625;                      // some minimal value.
    const int    MaxIter = 128;                          // maximum number of loop cycles to prevent infinite loops.
    const float2 Dir     = normalize(LightPos - Origin); // ray marching direction.
    float2       Pos     = Origin;                       // ray marching start position.
    float        t       = 0.0;                          // ray length
    int          i       = 0;

    // trace ray inside wall to highlight nearest walls
    if (InsideWall)
    {
        [loop] for (; i < MaxIter; ++i)
        {
            float d = -ReadSDF(Pos);

            // stop ray marching on positive value, this means ray in the empty space
            if (d < MinDist)
                break;

            t += d;
            Pos = Origin + Dir * t;
        }

        // add small offset to skip border between wall and empty space
        t += 0.2;
        Pos = Origin + Dir * t;
    }

    // trace ray through the empty space
    [loop] for (; i < MaxIter; ++i)
    {
        // read the maximum radius of the sphere inside which there are no intersections
        float d = ReadSDF(Pos);

        // stop ray marching on negative or too small distance
        if (d < MinDist)
            break;
        
        // me can increase ray length to the sphere radius
        t += d;
        Pos = Origin + Dir * t;

        // stop ray marching if ray distance is equal or greater than distance from current pixel to the light source
        if (t > TMax)
            break;
    }

    return t > TMax ? 1.0 : 0.0;
}

float4 PSmain(in PSInput PSIn) : SV_TARGET
{
    const float2 PosOnMap       = PSIn.UV * g_MapConstants.UVToMap; // position on map in pixels
    const float  DistToPlayer   = distance(PosOnMap, g_PlayerConstants.PlayerPos);
    const float2 DirToPlayer    = normalize(g_PlayerConstants.PlayerPos - PosOnMap);
    const float  DistToTeleport = distance(g_MapConstants.TeleportPos, PosOnMap);
    float4       Color          = float4(0.2, 0.2, 0.2, 1.0);
    float3       LightColor     = float3(0.0, 0.0, 0.0);
    bool         TraceLight     = false;
    const bool   InsideWall     = ReadSDF(PosOnMap) < 0.0;
    
    // draw walls
    Color.rgb = Blend(Color.rgb, WallColor, (InsideWall ? 1.0 : 0.0));

    // draw teleport
    {
        float Factor = saturate(1.0 - DistToTeleport / g_MapConstants.TeleportRadius);
        Color.rgb    = Blend(Color.rgb, TeleportColor, Factor);
    }

    // calculate flash light color
    if (g_PlayerConstants.FlashLightPower > 0.0 &&
        dot(g_PlayerConstants.FlashLightDir, -DirToPlayer) > 0.0)
    {
        const float  TanOfAngle = 0.5; // tangent of flash light cone angle
        const float2 Dir        = g_PlayerConstants.FlashLightDir;

        // calculate distance to ray using line equation
        float2 Begin      = g_PlayerConstants.PlayerPos;
        float2 End        = Begin + Dir;
        float  DistToRay  = abs((-Dir.y * PosOnMap.x) + (Dir.x * PosOnMap.y) + (Begin.x * End.y - End.x * Begin.y));
        float  ConeRadius = DistToPlayer * TanOfAngle;

        float Atten  = DistToPlayer / g_PlayerConstants.FlshLightMaxDist;
        Atten        = saturate(1.0 - Atten * Atten);
        float Factor = saturate(1.0 - DistToRay / ConeRadius) * g_PlayerConstants.FlashLightPower * Atten;
        LightColor   = Blend(LightColor, FlashLightColor, Factor);
        TraceLight   = TraceLight || Factor > 0.0;
    }

    // calculate color for ambient light around of player
    {
        float Factor = DistToPlayer / g_PlayerConstants.AmbientLightRadius;
        Factor       = saturate(1.0 - sqrt(Factor));
        LightColor   = Blend(LightColor, AmbientLightColor, Factor);
        TraceLight   = TraceLight || Factor > 0.0;
    }

    // ray marching from current pixel to player (light source) position
    if (TraceLight)
    {
        // 0 - totally shaded, 1 - totally illuminated, other - penumbra
        float Shading = TraceRay(g_PlayerConstants.PlayerPos, PosOnMap, InsideWall) * 0.5;

        // for soft shadows
        float2 Norm = float2(-DirToPlayer.y, DirToPlayer.x); // left normal to the line
        Shading += TraceRay(g_PlayerConstants.PlayerPos, PosOnMap + Norm * 0.125, InsideWall) * 0.25;
        Shading += TraceRay(g_PlayerConstants.PlayerPos, PosOnMap - Norm * 0.125, InsideWall) * 0.25;

        Color.rgb = (Color.rgb * LightColor * Shading) + (Color.rgb * AmbientLight);
    }
    else
    {
        Color.rgb *= AmbientLight;
    }

    // draw teleport wave
    {
        const float WaveWidth = 0.2;
        float       Shading   = 1.0 - smoothstep(0.0, 0.0001, dot(Color.rgb, Color.rgb)); // hide wave if it is illuminated
        float       Wave      = BumpStep(DistToTeleport, g_MapConstants.TeleportWaveRadius, g_MapConstants.TeleportWaveRadius + WaveWidth);
        Color.rgb             = Blend(Color.rgb, TeleportWaveColor, Wave * Shading);
    }

    // draw player
    {
        float Factor = saturate(1.0 - DistToPlayer / g_PlayerConstants.PlayerRadius);
        Color.rgb    = Blend(Color.rgb, PlayerColor, Factor);
    }

    return Color;
}
#endif
