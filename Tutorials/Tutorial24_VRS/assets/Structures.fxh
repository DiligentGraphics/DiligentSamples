
struct Constants
{
    float4x4 WorldViewProj;
    int      PrimitiveShadingRate;
    int      DrawMode; // 0 - Cube, 1 - Cube + Shading rate
    float    SurfaceScale;
    float    padding;
};
