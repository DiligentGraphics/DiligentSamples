
#include "structures.fxh"

ConstantBuffer<Constants> g_ConstantsCB;

[shader("miss")]
void main(inout PrimaryRayPayload payload)
{
    const float3 Palette[] = {
        float3(0.32, 0.00, 0.92),
        float3(0.00, 0.22, 0.90),
        float3(0.02, 0.67, 0.98),
        float3(0.41, 0.79, 1.00),
        float3(0.78, 1.00, 1.00),
        float3(1.00, 1.00, 1.00)
    };

    // Generate sky color.
    float factor  = clamp((WorldRayDirection().y + 0.5) / 1.5 * 4.0, 0.0, 4.0);
    int   idx     = floor(factor);
          factor -= float(idx);
    float3 color  = lerp(Palette[idx], Palette[idx+1], factor);

    payload.Color = color;
    //payload.Depth = RayTCurrent(); // bug in DXC for SPIRV
    payload.Depth = g_ConstantsCB.ClipPlanes.y;
}
