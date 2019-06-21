#ifndef OBJECT3D_MATERIAL_INCLUDED
#define OBJECT3D_MATERIAL_INCLUDED

#ifdef ENABBLE_TEX
Texture2D textures[];
#endif

cbuffer MaterialConstants : register(b0, space1)
{
	float3	albedo;
	uint	descriptorTableOffset;
	float	TVBrighntess;

#ifdef ENABBLE_TEX
	// TextureID is an enum to be defined before thhis file #inclusion
	Texture2D SelectTexture(TextureID id)
	{
		return textures[descriptorTableOffset + id];
	}
#endif
}

#endif	// OBJECT3D_MATERIAL_INCLUDED