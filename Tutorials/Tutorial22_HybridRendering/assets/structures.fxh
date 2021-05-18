
struct GlobalConstants
{
    float4x4 ViewProj;
    float4x4 ViewProjInv;

    float4 LightPos;
    float4 SkyColor;
    float4 CameraPos;

    int  DrawMode; // AZ TODO: remove
    int  padding0;
    int  padding1;
    int  padding2;

    uint2 GBufferDimension;

    float MaxReflectionRayLength;
    float AmbientLight;
};

struct ObjectConstants
{
    uint ObjectDataOffset;
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
    float4x3 NormalMat;   // object space normal to world space, float4x3 used because float3x3 has different size in D3D12 (36 bytes) and Vulkan (42 bytes)
    uint     MaterialId;  // index in g_MaterialAttribs
    uint     FirstIndex;  // first index in index buffer
    uint     MeshId;      // index of vertex buffer in g_VertexBuffers and index of index buffer in g_IndexBuffers
    uint     padding0;
};

struct MaterialAttribs
{
    float4 BaseColorMask;
    uint   SampInd;           // index in g_Samplers[];
    uint   BaseColorTexInd;   // index in g_Textures[];
    uint   RoughnessTexInd;   // index in g_Textures[];
    uint   padding0;
};

// Small offset between ray intersection and new ray origin to avoid self-intersections.
#define SMALL_OFFSET 0.0005
