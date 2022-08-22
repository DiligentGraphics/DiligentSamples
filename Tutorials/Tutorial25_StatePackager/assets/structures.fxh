#ifndef _STRUCTURES_FXH_
#define _STRUCTURES_FXH_

struct ShaderConstants
{
	uint  uScreenWidth;
	uint  uScreenHeight;
	float fScreenWidth;
	float fScreenHeight;

	float fLastSampleCount;
	float fNewSampleCount;
	int   iNumBounces;
	int   iNumSamplesPerFrame;

	float4x4 ViewProjMat;
	float4x4 ViewProjInvMat;
};

#endif // _STRUCTURES_FXH_
