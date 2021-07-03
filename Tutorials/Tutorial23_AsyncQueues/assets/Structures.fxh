
struct DrawConstants
{
    float4x4 ModelViewProj;
    float4x4 NormalMat;

    float3 LightDir;
    float  AmbientLight;
};

struct TerrainConstants
{
    float3 Scale;
    float  UVScale;

    float XOffset;
    float Animation;
    float NoiseScale;
    float padding0;
};

struct PostProcessConstants
{
    float4x4 ViewProjInv;

    float3 CameraPos;
    float  _padding0;

    float3 FogColor;
    float  _padding1;
};
