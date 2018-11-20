#ifndef OBJECT3D_VS_2_PS_INCLUDED
#define OBJECT3D_VS_2_PS_INCLUDED

struct XformedVertex
{
	float3 viewDir : ViewDir, N : NORMAL;
};

#endif	// OBJECT3D_VS_2_PS_INCLUDED