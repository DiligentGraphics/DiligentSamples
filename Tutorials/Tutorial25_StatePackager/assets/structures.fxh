#ifndef _STRUCTURES_FXH_
#define _STRUCTURES_FXH_

struct ShaderConstants
{
	uint  uScreenWidth;
	uint  uScreenHeight;
	float fScreenWidth;
	float fScreenHeight;

	float fLastSampleCount;
	float fCurrSampleCount;
	int   iNumBounces;
	int   iNumSamplesPerFrame;

	uint uFrameSeed1;
	uint uFrameSeed2;
	int  iShowOnlyLastBounce;
	int  Padding0;

	float2 f2LightPosXZ;
	float2 f2LightSizeXZ;

	float4x4 ViewProjMat;
	float4x4 ViewProjInvMat;

	float4 f4LightIntensity;
};

#endif // _STRUCTURES_FXH_
