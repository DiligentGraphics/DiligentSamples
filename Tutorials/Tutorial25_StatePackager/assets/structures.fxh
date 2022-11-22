#ifndef _STRUCTURES_FXH_
#define _STRUCTURES_FXH_

struct LightAttribs
{
	float2 f2PosXZ;
	float2 f2SizeXZ;
	float4 f4Intensity;
	float4 f4Normal;
};

struct ShaderConstants
{
	uint2  u2ScreenSize;
	float2 f2ScreenSize;

	float fLastSampleCount;
	float fCurrSampleCount;
	int   iNumBounces;
	int   iNumSamplesPerFrame;

	uint uFrameSeed1;
	uint uFrameSeed2;
	int  iShowOnlyLastBounce;
	int  iUseNEE;

	float4x4 ViewProjMat;
	float4x4 ViewProjInvMat;
	float4   CameraPos;

	LightAttribs Light;
};

#endif // _STRUCTURES_FXH_
