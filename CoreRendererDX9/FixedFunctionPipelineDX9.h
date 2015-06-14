/**
\author		Alexey Shaydurov aka ASH
\date		15.6.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <d3d9.h>
#include "Common.h"

namespace WRL = Microsoft::WRL;

inline D3DCOLOR SwapRB(D3DCOLOR color)
{
	auto &bytes = reinterpret_cast<uint8_t (&)[4]>(color);
	std::swap(bytes[0], bytes[2]);
	return color;
}

class CFixedFunctionPipelineDX9 final : public IFixedFunctionPipeline
{
	WRL::ComPtr<IDirect3DDevice9> _device;

	static const/*expr*/ float _attenuationFactor;

	const DWORD _maxLights;
	const std::unique_ptr<TMatrix4x4 []> _viewXforms;

	struct TLightState
	{
		// enable_if (here and in some other places) - workaround for VS 2013
		std::unique_ptr<std::pair<D3DLIGHT9, std::enable_if<true, decltype(_viewXforms)>::type::element_type>> light;
		BOOL enabled;
	};
	typedef std::stack<std::unique_ptr<TLightState []>> TLightStateStack;
	TLightStateStack _lightStateStack;

	struct TStates
	{
		DWORD fogEnable, fogColor, fogStart, fogEnd, fogDensity, lightingEnable, globalAmbient;
		D3DMATERIAL9 material;
	};
	typedef std::stack<TStates> TStateStack;
	TStateStack _stateStack;

public:

	CFixedFunctionPipelineDX9(const WRL::ComPtr<IDirect3DDevice9> &device);
	~CFixedFunctionPipelineDX9();

	inline bool IsGlobalLightingEnabled() const
	{
		DWORD enabled;
		AssertHR(_device->GetRenderState(D3DRS_LIGHTING, &enabled));
		return enabled;
	}

	void PushLights();
	void PopLights();

	DGLE_RESULT DGLE_API PushStates() override;
	DGLE_RESULT DGLE_API PopStates() override;

	DGLE_RESULT DGLE_API SetMaterialDiffuseColor(const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API SetMaterialSpecularColor(const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API SetMaterialShininess(float fShininess) override;

	DGLE_RESULT DGLE_API GetMaterialDiffuseColor(TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetMaterialSpecularColor(TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetMaterialShininess(float &fShininess) override;

	DGLE_RESULT DGLE_API ToggleGlobalLighting(bool bEnabled) override;
	DGLE_RESULT DGLE_API SetGloablAmbientLight(const TColor4 &stColor) override;

	DGLE_RESULT DGLE_API GetMaxLightsPerPassCount(uint &uiCount) override;
	DGLE_RESULT DGLE_API IsGlobalLightingEnabled(bool &bEnabled) override;
	DGLE_RESULT DGLE_API GetGloablAmbientLight(TColor4 &stColor) override;

	DGLE_RESULT DGLE_API SetLightEnabled(uint uiIdx, bool bEnabled) override;
	DGLE_RESULT DGLE_API SetLightColor(uint uiIdx, const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API SetLightIntensity(uint uiIdx, float fIntensity) override;
	DGLE_RESULT DGLE_API ConfigureDirectionalLight(uint uiIdx, const TVector3 &stDirection) override;
	DGLE_RESULT DGLE_API ConfigurePointLight(uint uiIdx, const TPoint3 &stPosition, float fRange) override;
	DGLE_RESULT DGLE_API ConfigureSpotLight(uint uiIdx, const TPoint3 &stPosition, const TVector3 &stDirection, float fRange, float fSpotAngle) override;

	DGLE_RESULT DGLE_API GetLightEnabled(uint uiIdx, bool &bEnabled) override;
	DGLE_RESULT DGLE_API GetLightColor(uint uiIdx, TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetLightIntensity(uint uiIdx, float &fIntensity) override;
	DGLE_RESULT DGLE_API GetLightType(uint uiIdx, E_LIGHT_TYPE &eType) override;
	DGLE_RESULT DGLE_API GetDirectionalLightConfiguration(uint uiIdx, TVector3 &stDirection) override;
	DGLE_RESULT DGLE_API GetPointLightConfiguration(uint uiIdx, TPoint3 &stPosition, float &fRange) override;
	DGLE_RESULT DGLE_API GetSpotLightConfiguration(uint uiIdx, TPoint3 &stPosition, TVector3 &stDirection, float &fRange, float &fSpotAngle) override;

	DGLE_RESULT DGLE_API SetFogEnabled(bool bEnabled) override;
	DGLE_RESULT DGLE_API SetFogColor(const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API ConfigureFog(float fStart, float fEnd) override;

	DGLE_RESULT DGLE_API GetFogEnabled(bool &bEnabled) override;
	DGLE_RESULT DGLE_API GetFogColor(TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetFogConfiguration(float &fStart, float &fEnd) override;

	IDGLE_BASE_IMPLEMENTATION(IFixedFunctionPipeline, INTERFACE_IMPL_END)
};