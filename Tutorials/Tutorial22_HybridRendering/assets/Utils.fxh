
// Simple procedural sky and sun
float4 GetSkyColor(float3 Dir, float3 LightDir)
{
	Dir.y += 0.075;
	Dir = normalize(Dir);

	float CosTheta        = dot(Dir, LightDir);
    float ScatteringScale = pow(saturate(0.5 * (1.0 - CosTheta)), 0.2);	

	float3 SkyColor = pow(saturate(CosTheta - 0.02), 50.0) * saturate(LightDir.y * 5.0);
	
	float3 SkyDome = 
		float3(0.07, 0.11, 0.23) *
		lerp(max(ScatteringScale, 0.1), 1.0, saturate(Dir.y)) / max(Dir.y, 0.01);
		
	SkyDome *= 13.0 / max(length(SkyDome), 13.0);
	float3 Horizon = pow(SkyDome, float3(1.0, 1.0, 1.0) - SkyDome);
	SkyColor += lerp(Horizon, SkyDome / (SkyDome + 0.5), saturate(Dir.y * 2.0));
	
	SkyColor *= 1.0 + pow(1.0 - ScatteringScale, 10.0) * 10.0;
	SkyColor *= 1.0 - abs(1.0 - Dir.y) * 0.5;
	
	return float4(SkyColor, 1.0);	
}

float3 ScreenPosToWorldPos(float2 ScreenSpaceUV, float Depth, float4x4 ViewProjInv)
{
	float4 PosClipSpace;
    PosClipSpace.xy = ScreenSpaceUV * float2(2.0, -2.0) + float2(-1.0, 1.0);
    PosClipSpace.z = Depth;
    PosClipSpace.w = 1.0;
    float4 WorldPos = mul(PosClipSpace, ViewProjInv);
    return WorldPos.xyz / WorldPos.w;
}
