
#include "structures.fxh"

static const float3 Pallete[] = {
    float3(0.32, 0.00, 0.92),
    float3(0.00, 0.22, 0.90),
    float3(0.02, 0.67, 0.98),
    float3(0.41, 0.79, 1.00),
    float3(0.78, 1.00, 1.00),
    float3(1.00, 1.00, 1.00)
};

[shader("miss")]
void main(inout PrimaryRayPayload payload)
{
    // Generate sky color.
    float factor  = clamp((-WorldRayDirection().y + 0.5) / 1.5 * 4.0, 0.0, 4.0);
    int   idx     = floor(factor);
          factor -= float(idx);
    float3 color  = lerp(Pallete[idx], Pallete[idx+1], factor);

    payload.Color = color;
    payload.Depth = RayTCurrent();
}
