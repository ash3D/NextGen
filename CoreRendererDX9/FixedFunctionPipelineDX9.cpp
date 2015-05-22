/**
\author		Alexey Shaydurov aka ASH
\date		22.5.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "FixedFunctionPipelineDX9.h"

using WRL::ComPtr;

namespace
{
	inline D3DCOLORVALUE Color_DGLE_2_D3D(const TColor4 &dgleColor)
	{
		const D3DCOLORVALUE d3dColor = { dgleColor.r, dgleColor.g, dgleColor.b, dgleColor.a };
		return d3dColor;
	}

	inline TColor4 Color_D3D_2_DGLE(const D3DCOLORVALUE &d3dColor)
	{
		TColor4 dgleColor;
		dgleColor.SetColorF(d3dColor.r, d3dColor.g, d3dColor.b, d3dColor.a);
		return dgleColor;
	}

	inline D3DVECTOR Vector_DGLE_2_D3D(const TVector3 &dgleVector)
	{
		const D3DVECTOR d3dVector = { dgleVector.x, dgleVector.y, dgleVector.z };
		return d3dVector;
	}

	inline TVector3 Vector_D3D_2_DGLE(const D3DVECTOR &d3dVector)
	{
		return TVector3(d3dVector.x, d3dVector.y, d3dVector.z);
	}
}

const float CFixedFunctionPipelineDX9::_attenuationFactor = 1.75f;

CFixedFunctionPipelineDX9::CFixedFunctionPipelineDX9(const ComPtr<IDirect3DDevice9> &device) : _device(device)
{
	D3DCAPS9 caps;
	AssertHR(_device->GetDeviceCaps(&caps));

	_maxLights = caps.MaxActiveLights;

	AssertHR(_device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR));
	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(50, 50, 50, 255)));

	for (int i = 0; i < _maxLights; ++i)
	{
		AssertHR(_device->LightEnable(i, FALSE));
		const D3DLIGHT9 lightDesc =
		{
			D3DLIGHT_DIRECTIONAL,
			{ 1, 1, 1, 1 },				// Diffuse
			{ 1, 1, 1, 1 },				// Specular
			{ 0, 0, 0, 1 },				// Ambient
			{ 0, 0, 0 },				// Position (ignored for directional light)
			{ 0, 0, 1 },				// Direction
			sqrt(FLT_MAX),				// Range (ignored for directional light)
			0,							// Falloff
			1,							// Attenuation0
			_attenuationFactor / 100.f,	// Attenuation1
			0,							// Attenuation2
			0,							// Theta (ignored for directional light)
			M_PI						// Phi (ignored for directional light)
		};
		AssertHR(_device->SetLight(i, &lightDesc));
	}
}

CFixedFunctionPipelineDX9::~CFixedFunctionPipelineDX9() = default;

void CFixedFunctionPipelineDX9::PushLights()
{
	TLightStateStack::value_type cur_state(new TLightState[_maxLights]);
	for (DWORD i = 0; i < _maxLights; i++)
	{
		D3DLIGHT9 light;
		if (SUCCEEDED(_device->GetLight(i, &light)))
			cur_state[i].light.reset(new D3DLIGHT9(light));

		if (FAILED(_device->GetLightEnable(i, &cur_state[i].enabled)))
			cur_state[i].enabled = -1;
	}
	_lightStateStack.push(move(cur_state));
}

void CFixedFunctionPipelineDX9::PopLights()
{
	for (DWORD i = 0; i < _maxLights; i++)
	{
		const auto &saved_state = _lightStateStack.top()[i];
		if (saved_state.light)
			AssertHR(_device->SetLight(i, saved_state.light.get()));
		if (saved_state.enabled != -1)
			AssertHR(_device->LightEnable(i, saved_state.enabled));
	}
	_lightStateStack.pop();
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::PushStates()
{
	// straightforward unoptimized implementation
	TStateStack::value_type curStates;
	AssertHR(_device->GetRenderState(D3DRS_FOGENABLE, &curStates.fogEnable));
	AssertHR(_device->GetRenderState(D3DRS_FOGCOLOR, &curStates.fogColor));
	AssertHR(_device->GetRenderState(D3DRS_FOGSTART, &curStates.fogStart));
	AssertHR(_device->GetRenderState(D3DRS_FOGEND, &curStates.fogEnd));
	AssertHR(_device->GetRenderState(D3DRS_FOGDENSITY, &curStates.fogDensity));
	AssertHR(_device->GetRenderState(D3DRS_LIGHTING, &curStates.lightingEnable));
	AssertHR(_device->GetRenderState(D3DRS_AMBIENT, &curStates.globalAmbient));
	AssertHR(_device->GetMaterial(&curStates.material));
	_stateStack.push(curStates);
	PushLights();
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::PopStates()
{
	AssertHR(_device->SetRenderState(D3DRS_FOGENABLE, _stateStack.top().fogEnable));
	AssertHR(_device->SetRenderState(D3DRS_FOGCOLOR, _stateStack.top().fogColor));
	AssertHR(_device->SetRenderState(D3DRS_FOGSTART, _stateStack.top().fogStart));
	AssertHR(_device->SetRenderState(D3DRS_FOGEND, _stateStack.top().fogEnd));
	AssertHR(_device->SetRenderState(D3DRS_FOGDENSITY, _stateStack.top().fogDensity));
	AssertHR(_device->SetRenderState(D3DRS_LIGHTING, _stateStack.top().lightingEnable));
	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, _stateStack.top().globalAmbient));
	AssertHR(_device->SetMaterial(&_stateStack.top().material));
	_stateStack.pop();
	_lightStateStack.pop();
	return S_OK;
}

/*
	material Set* methods uses inefficient get/set pattern (possibly leading to CPU/GPU sync)
	it used to ensure coherency with external state modfications
*/

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialDiffuseColor(const TColor4 &stColor)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Ambient = material.Diffuse = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialSpecularColor(const TColor4 &stColor)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Specular = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetMaterialShininess(float fShininess)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	material.Power = (128.f / 100.f) * (100.f - fShininess);
	AssertHR(_device->SetMaterial(&material));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialDiffuseColor(TColor4 &stColor)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	stColor = Color_D3D_2_DGLE(material.Diffuse);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialSpecularColor(TColor4 &stColor)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	stColor = Color_D3D_2_DGLE(material.Specular);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaterialShininess(float &fShininess)
{
	D3DMATERIAL9 material;
	AssertHR(_device->GetMaterial(&material));
	fShininess = 100.f - material.Power * (100.f / 128.f);
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ToggleGlobalLighting(bool bEnabled)
{
	AssertHR(_device->SetRenderState(D3DRS_LIGHTING, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetGloablAmbientLight(const TColor4 &stColor)
{
	AssertHR(_device->SetRenderState(D3DRS_AMBIENT, stColor));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetMaxLightsPerPassCount(uint &uiCount)
{
	uiCount = _maxLights;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::IsGlobalLightingEnabled(bool &bEnabled)
{
	bEnabled = IsGlobalLightingEnabled();
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetGloablAmbientLight(TColor4 &stColor)
{
	DWORD ambient;
	AssertHR(_device->GetRenderState(D3DRS_AMBIENT, &ambient));
	stColor = ambient;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightEnabled(uint uiIdx, bool bEnabled)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	AssertHR(_device->LightEnable(uiIdx, bEnabled));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightColor(uint uiIdx, const TColor4 &stColor)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Diffuse = light.Specular = Color_DGLE_2_D3D(stColor);
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetLightIntensity(uint uiIdx, float fIntensity)
{
	if (uiIdx >= (uint)_maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Attenuation0 = 1.f / fIntensity;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureDirectionalLight(uint uiIdx, const TVector3 &stDirection)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Direction = Vector_DGLE_2_D3D(stDirection);
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigurePointLight(uint uiIdx, const TPoint3 &stPosition, float fRange)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_POINT;
	light.Position = Vector_DGLE_2_D3D(stPosition);
	light.Attenuation1 = _attenuationFactor / fRange;
	light.Range = fRange;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureSpotLight(uint uiIdx, const TPoint3 &stPosition, const TVector3 &stDirection, float fRange, float fSpotAngle)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	light.Type = D3DLIGHT_SPOT;
	light.Position = Vector_DGLE_2_D3D(stPosition);
	light.Direction = Vector_DGLE_2_D3D(stDirection);
	light.Phi = fSpotAngle * (M_PI / 180.F);
	light.Attenuation1 = _attenuationFactor / fRange;
	light.Range = fRange;
	AssertHR(_device->SetLight(uiIdx, &light));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightEnabled(uint uiIdx, bool &bEnabled)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	BOOL enabled;
	AssertHR(_device->GetLightEnable(uiIdx, &enabled));
	bEnabled = enabled;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightColor(uint uiIdx, TColor4 &stColor)
{
	if (uiIdx >= (uint)_maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	stColor = Color_D3D_2_DGLE(light.Diffuse);

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightIntensity(uint uiIdx, float &fIntensity)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	fIntensity = 1.f / light.Attenuation0;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetLightType(uint uiIdx, E_LIGHT_TYPE &eType)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));
	switch (light.Type)
	{
	case D3DLIGHT_DIRECTIONAL:
		return LT_DIRECTIONAL;
	case  D3DLIGHT_POINT:
		return LT_POINT;
	case  D3DLIGHT_SPOT:
		return LT_SPOT;
	default:
		assert(false);
		__assume(false);
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetDirectionalLightConfiguration(uint uiIdx, TVector3 &stDirection)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_DIRECTIONAL)
		return E_INVALIDARG;

	stDirection = Vector_D3D_2_DGLE(light.Direction);

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetPointLightConfiguration(uint uiIdx, TPoint3 &stPosition, float &fRange)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_POINT)
		return E_INVALIDARG;

	stPosition = Vector_D3D_2_DGLE(light.Position);

	fRange = light.Range;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetSpotLightConfiguration(uint uiIdx, TPoint3 &stPosition, TVector3 &stDirection, float &fRange, float &fSpotAngle)
{
	if (uiIdx >= _maxLights)
		return E_INVALIDARG;

	D3DLIGHT9 light;
	AssertHR(_device->GetLight(uiIdx, &light));

	if (light.Type != D3DLIGHT_SPOT)
		return E_INVALIDARG;

	stPosition = Vector_D3D_2_DGLE(light.Position);
	stDirection = Vector_D3D_2_DGLE(light.Direction);

	fSpotAngle = light.Phi * (180.F / M_PI);
	fRange = light.Range;

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetFogEnabled(bool bEnabled)
{
	AssertHR(_device->SetRenderState(D3DRS_FOGENABLE, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::SetFogColor(const TColor4 &stColor)
{
	AssertHR(_device->SetRenderState(D3DRS_FOGCOLOR, stColor));
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::ConfigureFog(float fStart, float fEnd)
{
	AssertHR(_device->SetRenderState(D3DRS_FOGSTART, reinterpret_cast<const DWORD &>(fStart)));
	AssertHR(_device->SetRenderState(D3DRS_FOGEND, reinterpret_cast<const DWORD &>(fEnd)));

	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogEnabled(bool &bEnabled)
{
	DWORD enabled;
	AssertHR(_device->GetRenderState(D3DRS_FOGENABLE, &enabled));
	bEnabled = enabled;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogColor(TColor4 &stColor)
{
	DWORD color;
	AssertHR(_device->GetRenderState(D3DRS_FOGCOLOR, &color));
	stColor = color;
	return S_OK;
}

DGLE_RESULT DGLE_API CFixedFunctionPipelineDX9::GetFogConfiguration(float &fStart, float &fEnd)
{
	AssertHR(_device->GetRenderState(D3DRS_FOGSTART, reinterpret_cast<DWORD *>(&fStart)));
	AssertHR(_device->GetRenderState(D3DRS_FOGEND, reinterpret_cast<DWORD *>(&fEnd)));
	return S_OK;
}