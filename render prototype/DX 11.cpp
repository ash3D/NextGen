#include "stdafx.h"
#include <vector>
#include <Unknwn.h>
#include "..\\..\\DGLE2\\Include\\CPP\\DGLE2.h"
#include "LowLevelRenderer.h"
#include <D3D11.h>
#include <D3DX11async.h>
#include "D3dx11effect.h"
#include "com ptr defs.h"

#define ASSERT_HR(...) _ASSERT(SUCCEEDED(__VA_ARGS__))

using namespace DGLE2;
using namespace DGLE2::LowLevelRenderer;

#if APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#error APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#endif

namespace DX11
{
	using namespace ComPtrs;

	inline const char *Format2String(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_UNKNOWN:						return "DXGI_FORMAT_UNKNOWN";
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:			return "DXGI_FORMAT_R32G32B32A32_TYPELESS";
		case DXGI_FORMAT_R32G32B32A32_FLOAT:			return "DXGI_FORMAT_R32G32B32A32_FLOAT";
		case DXGI_FORMAT_R32G32B32A32_UINT:				return "DXGI_FORMAT_R32G32B32A32_UINT";
		case DXGI_FORMAT_R32G32B32A32_SINT:				return "DXGI_FORMAT_R32G32B32A32_SINT";
		case DXGI_FORMAT_R32G32B32_TYPELESS:			return "DXGI_FORMAT_R32G32B32_TYPELESS";
		case DXGI_FORMAT_R32G32B32_FLOAT:				return "DXGI_FORMAT_R32G32B32_FLOAT";
		case DXGI_FORMAT_R32G32B32_UINT:				return "DXGI_FORMAT_R32G32B32_UINT";
		case DXGI_FORMAT_R32G32B32_SINT:				return "DXGI_FORMAT_R32G32B32_SINT";
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:			return "DXGI_FORMAT_R16G16B16A16_TYPELESS";
		case DXGI_FORMAT_R16G16B16A16_FLOAT:			return "DXGI_FORMAT_R16G16B16A16_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_UNORM:			return "DXGI_FORMAT_R16G16B16A16_UNORM";
		case DXGI_FORMAT_R16G16B16A16_UINT:				return "DXGI_FORMAT_R16G16B16A16_UINT";
		case DXGI_FORMAT_R16G16B16A16_SNORM:			return "DXGI_FORMAT_R16G16B16A16_SNORM";
		case DXGI_FORMAT_R16G16B16A16_SINT:				return "DXGI_FORMAT_R16G16B16A16_SINT";
		case DXGI_FORMAT_R32G32_TYPELESS:				return "DXGI_FORMAT_R32G32_TYPELESS";
		case DXGI_FORMAT_R32G32_FLOAT:					return "DXGI_FORMAT_R32G32_FLOAT";
		case DXGI_FORMAT_R32G32_UINT:					return "DXGI_FORMAT_R32G32_UINT";
		case DXGI_FORMAT_R32G32_SINT:					return "DXGI_FORMAT_R32G32_SINT";
		case DXGI_FORMAT_R32G8X24_TYPELESS:				return "DXGI_FORMAT_R32G8X24_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:			return "DXGI_FORMAT_D32_FLOAT_S8X24_UINT";
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:		return "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS";
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:		return "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT";
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:			return "DXGI_FORMAT_R10G10B10A2_TYPELESS";
		case DXGI_FORMAT_R10G10B10A2_UNORM:				return "DXGI_FORMAT_R10G10B10A2_UNORM";
		case DXGI_FORMAT_R10G10B10A2_UINT:				return "DXGI_FORMAT_R10G10B10A2_UINT";
		case DXGI_FORMAT_R11G11B10_FLOAT:				return "DXGI_FORMAT_R11G11B10_FLOAT";
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:				return "DXGI_FORMAT_R8G8B8A8_TYPELESS";
		case DXGI_FORMAT_R8G8B8A8_UNORM:				return "DXGI_FORMAT_R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:			return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R8G8B8A8_UINT:					return "DXGI_FORMAT_R8G8B8A8_UINT";
		case DXGI_FORMAT_R8G8B8A8_SNORM:				return "DXGI_FORMAT_R8G8B8A8_SNORM";
		case DXGI_FORMAT_R8G8B8A8_SINT:					return "DXGI_FORMAT_R8G8B8A8_SINT";
		case DXGI_FORMAT_R16G16_TYPELESS:				return "DXGI_FORMAT_R16G16_TYPELESS";
		case DXGI_FORMAT_R16G16_FLOAT:					return "DXGI_FORMAT_R16G16_FLOAT";
		case DXGI_FORMAT_R16G16_UNORM:					return "DXGI_FORMAT_R16G16_UNORM";
		case DXGI_FORMAT_R16G16_UINT:					return "DXGI_FORMAT_R16G16_UINT";
		case DXGI_FORMAT_R16G16_SNORM:					return "DXGI_FORMAT_R16G16_SNORM";
		case DXGI_FORMAT_R16G16_SINT:					return "DXGI_FORMAT_R16G16_SINT";
		case DXGI_FORMAT_R32_TYPELESS:					return "DXGI_FORMAT_R32_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT:						return "DXGI_FORMAT_D32_FLOAT";
		case DXGI_FORMAT_R32_FLOAT:						return "DXGI_FORMAT_R32_FLOAT";
		case DXGI_FORMAT_R32_UINT:						return "DXGI_FORMAT_R32_UINT";
		case DXGI_FORMAT_R32_SINT:						return "DXGI_FORMAT_R32_SINT";
		case DXGI_FORMAT_R24G8_TYPELESS:				return "DXGI_FORMAT_R24G8_TYPELESS";
		case DXGI_FORMAT_D24_UNORM_S8_UINT:				return "DXGI_FORMAT_D24_UNORM_S8_UINT";
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:			return "DXGI_FORMAT_R24_UNORM_X8_TYPELESS";
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:			return "DXGI_FORMAT_X24_TYPELESS_G8_UINT";
		case DXGI_FORMAT_R8G8_TYPELESS:					return "DXGI_FORMAT_R8G8_TYPELESS";
		case DXGI_FORMAT_R8G8_UNORM:					return "DXGI_FORMAT_R8G8_UNORM";
		case DXGI_FORMAT_R8G8_UINT:						return "DXGI_FORMAT_R8G8_UINT";
		case DXGI_FORMAT_R8G8_SNORM:					return "DXGI_FORMAT_R8G8_SNORM";
		case DXGI_FORMAT_R8G8_SINT:						return "DXGI_FORMAT_R8G8_SINT";
		case DXGI_FORMAT_R16_TYPELESS:					return "DXGI_FORMAT_R16_TYPELESS";
		case DXGI_FORMAT_R16_FLOAT:						return "DXGI_FORMAT_R16_FLOAT";
		case DXGI_FORMAT_D16_UNORM:						return "DXGI_FORMAT_D16_UNORM";
		case DXGI_FORMAT_R16_UNORM:						return "DXGI_FORMAT_R16_UNORM";
		case DXGI_FORMAT_R16_UINT:						return "DXGI_FORMAT_R16_UINT";
		case DXGI_FORMAT_R16_SNORM:						return "DXGI_FORMAT_R16_SNORM";
		case DXGI_FORMAT_R16_SINT:						return "DXGI_FORMAT_R16_SINT";
		case DXGI_FORMAT_R8_TYPELESS:					return "DXGI_FORMAT_R8_TYPELESS";
		case DXGI_FORMAT_R8_UNORM:						return "DXGI_FORMAT_R8_UNORM";
		case DXGI_FORMAT_R8_UINT:						return "DXGI_FORMAT_R8_UINT";
		case DXGI_FORMAT_R8_SNORM:						return "DXGI_FORMAT_R8_SNORM";
		case DXGI_FORMAT_R8_SINT:						return "DXGI_FORMAT_R8_SINT";
		case DXGI_FORMAT_A8_UNORM:						return "DXGI_FORMAT_A8_UNORM";
		case DXGI_FORMAT_R1_UNORM:						return "DXGI_FORMAT_R1_UNORM";
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:			return "DXGI_FORMAT_R9G9B9E5_SHAREDEXP";
		case DXGI_FORMAT_R8G8_B8G8_UNORM:				return "DXGI_FORMAT_R8G8_B8G8_UNORM";
		case DXGI_FORMAT_G8R8_G8B8_UNORM:				return "DXGI_FORMAT_G8R8_G8B8_UNORM";
		case DXGI_FORMAT_BC1_TYPELESS:					return "DXGI_FORMAT_BC1_TYPELESS";
		case DXGI_FORMAT_BC1_UNORM:						return "DXGI_FORMAT_BC1_UNORM";
		case DXGI_FORMAT_BC1_UNORM_SRGB:				return "DXGI_FORMAT_BC1_UNORM_SRGB";
		case DXGI_FORMAT_BC2_TYPELESS:					return "DXGI_FORMAT_BC2_TYPELESS";
		case DXGI_FORMAT_BC2_UNORM:						return "DXGI_FORMAT_BC2_UNORM";
		case DXGI_FORMAT_BC2_UNORM_SRGB:				return "DXGI_FORMAT_BC2_UNORM_SRGB";
		case DXGI_FORMAT_BC3_TYPELESS:					return "DXGI_FORMAT_BC3_TYPELESS";
		case DXGI_FORMAT_BC3_UNORM:						return "DXGI_FORMAT_BC3_UNORM";
		case DXGI_FORMAT_BC3_UNORM_SRGB:				return "DXGI_FORMAT_BC3_UNORM_SRGB";
		case DXGI_FORMAT_BC4_TYPELESS:					return "DXGI_FORMAT_BC4_TYPELESS";
		case DXGI_FORMAT_BC4_UNORM:						return "DXGI_FORMAT_BC4_UNORM";
		case DXGI_FORMAT_BC4_SNORM:						return "DXGI_FORMAT_BC4_SNORM";
		case DXGI_FORMAT_BC5_TYPELESS:					return "DXGI_FORMAT_BC5_TYPELESS";
		case DXGI_FORMAT_BC5_UNORM:						return "DXGI_FORMAT_BC5_UNORM";
		case DXGI_FORMAT_BC5_SNORM:						return "DXGI_FORMAT_BC5_SNORM";
		case DXGI_FORMAT_B5G6R5_UNORM:					return "DXGI_FORMAT_B5G6R5_UNORM";
		case DXGI_FORMAT_B5G5R5A1_UNORM:				return "DXGI_FORMAT_B5G5R5A1_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM:				return "DXGI_FORMAT_B8G8R8A8_UNORM";
		case DXGI_FORMAT_B8G8R8X8_UNORM:				return "DXGI_FORMAT_B8G8R8X8_UNORM";
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:	return "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM";
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:				return "DXGI_FORMAT_B8G8R8A8_TYPELESS";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:			return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:				return "DXGI_FORMAT_B8G8R8X8_TYPELESS";
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:			return "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB";
		case DXGI_FORMAT_BC6H_TYPELESS:					return "DXGI_FORMAT_BC6H_TYPELESS";
		case DXGI_FORMAT_BC6H_UF16:						return "DXGI_FORMAT_BC6H_UF16";
		case DXGI_FORMAT_BC6H_SF16:						return "DXGI_FORMAT_BC6H_SF16";
		case DXGI_FORMAT_BC7_TYPELESS:					return "DXGI_FORMAT_BC7_TYPELESS";
		case DXGI_FORMAT_BC7_UNORM:						return "DXGI_FORMAT_BC7_UNORM";
		case DXGI_FORMAT_BC7_UNORM_SRGB:				return "DXGI_FORMAT_BC7_UNORM_SRGB";
		default:
			_ASSERT(0);
			return "";
		}
	}

	inline const char *ScanlineOrdering2string(DXGI_MODE_SCANLINE_ORDER scanlineOrder)
	{
		switch (scanlineOrder)
		{
		case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:			return "DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED";
		case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:			return "DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE";
		case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:	return "DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST";
		case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:	return "DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST";
		default:
			_ASSERT(0);
			return "";
		}
	}

	inline const char *Scaling2String(DXGI_MODE_SCALING scaling)
	{
		switch (scaling)
		{
		case DXGI_MODE_SCALING_UNSPECIFIED:	return "DXGI_MODE_SCALING_UNSPECIFIED";
		case DXGI_MODE_SCALING_CENTERED:	return "DXGI_MODE_SCALING_CENTERED";
		case DXGI_MODE_SCALING_STRETCHED:	return "DXGI_MODE_SCALING_STRETCHED";
		default:
			_ASSERT(0);
			return "";
		}
	}

	std::string ModeDesc2String(const DXGI_MODE_DESC &desc)
	{
		return std::string("Format:\t") + Format2String(desc.Format) +
			"\nScanlineOrdering:\t" + ScanlineOrdering2string(desc.ScanlineOrdering) +
			"\nScaling:\t" + Scaling2String(desc.Scaling);
	}

	class CDeviceContext: public IDeviceContext
	{
	public:
		CDeviceContext(ID3D11DevicePtr device, ID3DX11EffectPtr effect2D): _effect2D(effect2D)
		{
			device->GetImmediateContext(&_deviceContext);
		}
		virtual void DrawPoint(float x, float y, uint32 color, float size);
		virtual void DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width);
		virtual void DrawRect(float x, float y, float width, float height, uint32 color, float angle);
		virtual void DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], float angle);
		virtual void DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color, float angle);
		virtual void DrawCircle(float x, float y, float r, uint32 color);
		virtual void DrawEllipse(float x, float y, float rx, float ry, uint32 color, float angle);
	private:
		ID3D11DeviceContextPtr _deviceContext;
		ID3DX11EffectPtr _effect2D;
	};

	const class CDisplayModes: public IDisplayModes
	{
	public:
		CDisplayModes()
		{
			IDXGIFactory1Ptr factory;
			ASSERT_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)));
			IDXGIAdapter1Ptr adapter;
			ASSERT_HR(factory->EnumAdapters1(0, &adapter));
			UINT modes_count;
			IDXGIOutputPtr output;
			ASSERT_HR(adapter->EnumOutputs(0, &output));
			ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, NULL));
			_modes.resize(modes_count);
			ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, &_modes.front()));
		}
		virtual uint Count() const
		{
			return _modes.size();
		}
		virtual TDispMode operator [](uint idx) const
		{
			TDispMode mode =
			{
				_modes[idx].Width,
				_modes[idx].Height,
				float(_modes[idx].RefreshRate.Numerator) / _modes[idx].RefreshRate.Denominator,
				ModeDesc2String(_modes[idx])
			};
			return mode;
		}
		const DXGI_MODE_DESC &GetDX11Mode(uint idx) const
		{
			return _modes[idx];
		}
	private:
		std::vector<DXGI_MODE_DESC> _modes;
	} displayModes;

	class CDevice: public IDevice
	{
	public:
		CDevice(HWND hwnd, uint modeIdx, bool fullscreen, bool multithreaded)
		{
			Init(hwnd, displayModes.GetDX11Mode(modeIdx), fullscreen, multithreaded);
		}
		CDevice(HWND hwnd, uint width, uint height, bool fullscreen, uint refreshRate, bool multithreaded/*, format*/)
		{
			const DXGI_MODE_DESC mode_desc =
			{
				width, height, {refreshRate, 1},
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
				DXGI_MODE_SCALING_UNSPECIFIED
			};
			Init(hwnd, mode_desc, fullscreen, multithreaded);
		}
		virtual ~CDevice()
		{
			ASSERT_HR(_swapChain->SetFullscreenState(FALSE, NULL));
		}
		virtual IDeviceContext *GetDeviceContext()
		{
			return new CDeviceContext(_device, _effect2D);
		}
		virtual void SetMode(uint width, uint height)
		{
		}
		virtual void SetMode(uint idx)
		{
		}
		virtual void ToggleFullscreen(bool fullscreen)
		{
		}
		virtual void test()
		{
			ASSERT_HR(_swapChain->Present(0, 0));
			ID3D11DeviceContextPtr immediate_comtext;
			_device->GetImmediateContext(&immediate_comtext);
			const FLOAT color[] = {0, 0, 0, 0};
			ID3D11Texture2DPtr rt;
			ASSERT_HR(_swapChain->GetBuffer(0, __uuidof(rt), reinterpret_cast<void **>(&rt)));
			ID3D11RenderTargetViewPtr rt_view;
			D3D11_RENDER_TARGET_VIEW_DESC rt_view_desc = {DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_TEXTURE2D};
			rt_view_desc.Texture2D.MipSlice = 0;
			ASSERT_HR(_device->CreateRenderTargetView(rt, &rt_view_desc, &rt_view));
			immediate_comtext->ClearRenderTargetView(rt_view, color);
			immediate_comtext->OMSetRenderTargets(1, &rt_view, NULL);
		}
	private:
		void Init(HWND hwnd, const DXGI_MODE_DESC &modeDesc, bool fullscreen, bool multithreaded)
		{
			DXGI_SWAP_CHAIN_DESC swap_chain_desc =
			{
				modeDesc,
				1, 0,
				DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT, 2, hwnd,
				!fullscreen, DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
			};
			UINT flags =
#ifdef _DEBUG
				D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
#else
				0
#endif
			;
			if (!multithreaded) flags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

			ASSERT_HR(D3D11CreateDeviceAndSwapChain(
				NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
				&_swapChain, &_device, NULL, NULL));

			// create 2D effect
			UINT shader_flags = 0, effect_flags = 0;
#ifdef _DEBUG
			shader_flags |= D3D10_SHADER_DEBUG;
#endif
			if (!multithreaded)
				effect_flags |= D3D10_EFFECT_SINGLE_THREADED;
			ID3D10BlobPtr compiled_effect, effect_errors;
			ASSERT_HR(D3DX11CompileFromFile(TEXT("2D.fx"), NULL, NULL, NULL, "fx_5_0", shader_flags, effect_flags, NULL, &compiled_effect, &effect_errors, NULL));
#ifdef _DEBUG
			if (effect_errors)
				OutputDebugStringA(LPCSTR(effect_errors->GetBufferPointer()));
#endif
			ASSERT_HR(D3DX11CreateEffectFromMemory(compiled_effect->GetBufferPointer(), compiled_effect->GetBufferSize(), 0, _device, &_effect2D));
		}
		ID3D11DevicePtr _device;
		//ID3D11DeviceContextPtr _immediateContext;
		IDXGISwapChainPtr _swapChain;
		ID3DX11EffectPtr _effect2D;
	};
}

namespace DX11
{
	void CDeviceContext::DrawPoint(float x, float y, uint32 color, float size)
	{
	}

	void CDeviceContext::DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width)
	{
	}

	void CDeviceContext::DrawRect(float x, float y, float width, float height, uint32 color, float angle)
	{
	}

	void CDeviceContext::DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], float angle)
	{
	}

	void CDeviceContext::DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color, float angle)
	{
	}

	void CDeviceContext::DrawCircle(float x, float y, float r, uint32 color)
	{
	}

	void CDeviceContext::DrawEllipse(float x, float y, float rx, float ry, uint32 color, float angle)
	{
	}
}

const IDisplayModes &DGLE2::LowLevelRenderer::GetDisplayModes()
{
	return DX11::displayModes;
}

IDevice *DGLE2::LowLevelRenderer::CreateDevice(HWND hwnd, uint width, uint height, bool fullscreen, uint refreshRate, bool multithreaded)
{
	return new DX11::CDevice(hwnd, width, height, fullscreen, refreshRate, multithreaded);
}