
float4 SkyColor(float3 Dir, float3 LightDir)
{
	float CosTheta        = dot(Dir, LightDir);
    float ScatteringScale = pow(saturate(0.5 * (1.0 - CosTheta)), 0.2);	

	float3 SkyColor = pow(saturate(CosTheta - 0.02), 50.0) * saturate(LightDir.y * 5.0);
	
	float3 SkyDome = 
		float3(0.07, 0.11, 0.23) *
		lerp(max(ScatteringScale, 0.1), 1.0, saturate(Dir.y)) / max(Dir.y, 0.01);
		
	SkyDome *= 11.0 / max(length(SkyDome), 11.0);
	float3 Horizon = pow(SkyDome, float3(1.0, 1.0, 1.0) - SkyDome);
	SkyColor += lerp(Horizon, SkyDome / (SkyDome + 0.5), saturate(Dir.y * 2.0));
	
	SkyColor *= 1.0 + pow(1.0 - ScatteringScale, 10.0) * 10.0;
	SkyColor *= 1.0 - abs(1.0 - Dir.y) * 0.5;
	
	return float4(SkyColor, 1.0);	
}
