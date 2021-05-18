

float4 SkyColor(float3 Dir)
{
    const float3 Pallete[] = {
        float3(0.32, 0.00, 0.92),
        float3(0.00, 0.22, 0.90),
        float3(0.02, 0.67, 0.98),
        float3(0.41, 0.79, 1.00),
        float3(0.78, 1.00, 1.00),
        float3(1.00, 1.00, 1.00)
    };

    // Generate sky color.
    float factor  = clamp((Dir.y + 0.5) / 1.5 * 4.0, 0.0, 4.0);
    int   idx     = floor(factor);
          factor -= float(idx);
    float4 color  = float4(lerp(Pallete[idx], Pallete[idx+1], factor), 1.0);

	return color;
}
