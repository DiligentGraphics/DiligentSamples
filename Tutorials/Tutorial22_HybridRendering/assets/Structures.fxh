
#define RENDER_MODE_SHADED           0
#define RENDER_MODE_G_BUFFER_COLOR   1
#define RENDER_MODE_G_BUFFER_NORMAL  2
#define RENDER_MODE_DIFFUSE_LIGHTING 3
#define RENDER_MODE_REFLECTIONS      4
#define RENDER_MODE_FRESNEL_TERM     5

struct GlobalConstants
{
    float4x4 ViewProj;
    float4x4 ViewProjInv;

    float4 LightDir;
    float4 CameraPos;

    int   DrawMode;
    float MaxRayLength;
    float AmbientLight;
    int   padding0;
};

struct ObjectConstants
{
    uint ObjectAttribsOffset; // offset in g_ObjectAttribs
    uint padding0;
    uint padding1;
    uint padding2;
};

struct Vertex
{
    float PosX, PosY, PosZ;
    float NormX, NormY, NormZ;
    float U, V;
};

struct ObjectAttribs
{
    float4x4 ModelMat;    // object space position to world space
#ifdef METAL
    float3x3 NormalMat;   // In Metal float4x3 type has 64 bytes size, but size of float3x3 is 48 bytes as float4x3 in HLSL.
#else
    float4x3 NormalMat;   // object space normal to world space, float4x3 used because float3x3 has different size in D3D12 (36 bytes) and Vulkan (48 bytes)
#endif

    uint MaterialId;  // index in g_MaterialAttribs
    uint FirstIndex;  // first index in index buffer
    uint FirstVertex; // first vertex in vertex buffer
    uint MeshId;      // Unused. Can be used to select index and vertex buffer in the buffer array.
};

struct MaterialAttribs
{
    float4 BaseColorMask;
    uint   SampInd;         // index in g_Samplers[];
    uint   BaseColorTexInd; // index in g_Textures[];
    uint   padding0;
    uint   padding1;
};

// Small offset between ray intersection and new ray origin to avoid self-intersections.
#define SMALL_OFFSET 0.0001
