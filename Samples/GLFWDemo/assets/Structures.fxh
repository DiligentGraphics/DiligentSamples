

struct PlayerConstants
{
    float2 PlayerPos;
    float  PlayerRadius;
    float  AmbientLightRadius;
    float2 FlashLightDir;
    float  FlashLightPower;
    float  FlshLightMaxDist;
};

struct MapConstants
{
    float2 ScreenRectLR; // left, right
    float2 ScreenRectTB; // top, bottom
    float2 UVToMap;
    float2 MapToUV;
    float2 TeleportPos;
    float  TeleportRadius;
    float  TeleportWaveRadius;
};
