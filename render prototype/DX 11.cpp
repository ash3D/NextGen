#include "stdafx.h"
#include <vector>
#include <list>
#include <algorithm>
#include <Unknwn.h>
#include "..\\..\\Include\\CPP\\DGLE2.h"
#include "LowLevelRenderer.h"
#include <D3D11.h>
#include <D3DX11.h>
#include <D3DX10Math.h>
#include "D3dx11effect.h"
#include "com ptr defs.h"

#define ASSERT_HR(...) {HRESULT hr = __VA_ARGS__;_ASSERT(SUCCEEDED(hr));}

using namespace DGLE2;
using namespace DGLE2::LowLevelRenderer;

#if APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#error APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#endif

#pragma warning(disable: 4290)

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

	class CDynamicVB
	{
	public:
		CDynamicVB(ID3D11DevicePtr device, UINT minSize): _device(device), _size(minSize), _offset(0)
		{
			D3D11_BUFFER_DESC desc = {_size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
			ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
		}
		template<typename F>
		void Draw(ID3D11DeviceContextPtr context, UINT stride, UINT vcount, F callback)
		{
			const UINT size = stride * vcount;
			if (_size < size)
			{
				D3D11_BUFFER_DESC desc = {_size = size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
				ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
				_offset = 0;
			}
			const D3D11_MAP map_type = _offset + size <= _size ? D3D11_MAP_WRITE_NO_OVERWRITE : (_offset = 0, D3D11_MAP_WRITE_DISCARD);
			D3D11_MAPPED_SUBRESOURCE mapped;
			ASSERT_HR(context->Map(_VB, 0, map_type, 0, &mapped))
			callback(reinterpret_cast<ubyte *>(mapped.pData) + _offset);
			context->Unmap(_VB, 0);
			context->IASetVertexBuffers(0, 1, &_VB.GetInterfacePtr(), &stride, &_offset);
			context->Draw(vcount, 0);
			_offset += size;
		}
	private:
		ID3D11DevicePtr _device;
		ID3D11BufferPtr _VB;
		UINT _size, _offset;
	};

#pragma pack(push, 4)
	struct TQuad
	{
		TQuad() {}
		TQuad(float x, float y, float width, float height, uint16 layer, float angle, uint32 color):
			pos(x, y), extents(width, height), layer(layer), angle(angle), color(color)
		{
		}
		bool operator <(const TQuad &quad) const
		{
			return layer < quad.layer;
		}
		D3DXVECTOR2_16F pos, extents;
		uint16 layer;
		D3DXFLOAT16 angle;
		uint32 color;
		//D3DXVECTOR2 pos, extents;
		//uint16 layer;
		//float angle;
		//uint32 color;
	};
#pragma pack(pop)
	class CDeviceContext: public IDeviceContext
	{
		class CQuadFiller
		{
		public:
			CQuadFiller(float x, float y, float width, float height, uint16 layer, float angle, uint32 color):
				_quad(x, y, width, height, layer, angle, color)
			{
			}
			void operator ()(void *dst)
			{
				*reinterpret_cast<TQuad *>(dst) = _quad;
			}
		private:
			TQuad _quad;
		};
	public:
		CDeviceContext(ID3D11DevicePtr device, ID3D11InputLayoutPtr quadLayout, ID3DX11EffectPtr effect2D, ID3DX11EffectPass *rectPass, ID3DX11EffectPass *ellipsePass, ID3DX11EffectPass *ellipseAAPass, IDevice &dev):
			_2DVB(device, sizeof(TQuad) * 64), _device(device), _quadLayout(quadLayout),
			_effect2D(effect2D), _rectPass(rectPass), _ellipsePass(ellipsePass), _ellipseAAPass(ellipseAAPass), _dev(dev), _curLayer(~0)
		{
			device->GetImmediateContext(&_deviceContext);
		}
		virtual ~CDeviceContext()
		{
			std::for_each(_rects.begin(), _rects.end(), [](TRects::value_type rect){delete rect;});
		}
		virtual void DrawPoint(float x, float y, uint32 color, float size) override;
		virtual void DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width) override;
		virtual void DrawRect(float x, float y, float width, float height, uint32 color, float angle) override;
		virtual void DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], float angle) override;
		virtual void DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color, float angle) override;
		virtual void DrawCircle(float x, float y, float r, uint32 color) override;
		virtual void DrawEllipse(float x, float y, float rx, float ry, uint32 color, bool AA, float angle) override;
		virtual void test() override
		{
			std::for_each(_rects.begin(), _rects.end(), [](TRects::value_type rect){delete rect;});
			_rects.clear();
			_curLayer = ~0;
		}
	private:
		CDynamicVB _2DVB;
		ID3D11DeviceContextPtr _deviceContext;
		ID3D11DevicePtr _device;
		ID3D11InputLayoutPtr _quadLayout;
		ID3DX11EffectPtr _effect2D;	// hold ref for effect passes
		ID3DX11EffectPass *const _rectPass, *const _ellipsePass, *const _ellipseAAPass;
		IDevice &_dev;
		typedef std::vector<_2D::IRect *> TRects;
		TRects _rects;
		unsigned short _curLayer;
	};

	const class CDisplayModes: public IDisplayModes
	{
	public:
		CDisplayModes()
		{
			IDXGIFactory1Ptr factory;
			ASSERT_HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))
			IDXGIAdapter1Ptr adapter;
			ASSERT_HR(factory->EnumAdapters1(0, &adapter))
			UINT modes_count;
			IDXGIOutputPtr output;
			ASSERT_HR(adapter->EnumOutputs(0, &output))
			ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, NULL))
			_modes.resize(modes_count);
			ASSERT_HR(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_SCALING, &modes_count, _modes.data()))
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

	namespace
	{
		struct
		{
			template<class Class>
			void operator ()(Class *object) const
			{
				object->parent = NULL;
			}
		} parentNuller;

		class CWin32Heap
		{
		public:
			CWin32Heap() throw(const char *): _handle(HeapCreate(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE, 0, 0))
			{
				if (!_handle) throw "fail to create heap";
			}
			~CWin32Heap() throw(const char *)
			{
				if (!HeapDestroy(_handle)) throw "fail to destroy heap";
			}
			operator HANDLE() const throw()
			{
				return _handle;
			}
		private:
			const HANDLE _handle;
		};

		template<typename T>
		class CWin32HeapAllocator: public std::allocator<T>
		{
			template<typename T>
			friend class CWin32HeapAllocator;
		public:
			template<typename Other>
			struct rebind
			{
				typedef CWin32HeapAllocator<Other> other;
			};
			CWin32HeapAllocator(HANDLE heap = HANDLE(_get_heap_handle())) throw(): _heap(heap) {}
			template<typename Other>
			CWin32HeapAllocator(const CWin32HeapAllocator<Other> &src) throw(): _heap(src._heap) {}
			pointer allocate(size_type count, const void * = NULL) throw(std::bad_alloc)
			{
				try
				{
					return pointer(HeapAlloc(_heap, 0, count * sizeof(T)));
				}
				catch(...)
				{
					throw std::bad_alloc();
				}
			}
			void deallocate(pointer ptr, size_type) throw(const char *)
			{
				if (!HeapFree(_heap, 0, ptr)) throw "fail to free heap block";
			}
		private:
			const HANDLE _heap;
		};
	}

	class CDevice: public IDevice
	{
	public:
		// C++ 0X
		// use delegating ctors
		CDevice(HWND hwnd, uint modeIdx, bool fullscreen, bool multithreaded):
			_dynamicRectsAllocator(_dedicatedHeap),
			//_staticRectsAllocator(0), _dynamicRectsAllocator(0),
			//_staticEllipsesAllocator(0), _dynamicEllipsesAllocator(0),
			//_staticEllipsesAAAllocator(0), _dynamicEllipsesAAAllocator(0),
			_staticRects(_staticRectsAllocator), _dynamicRects(_dynamicRectsAllocator),
			_staticEllipses(_staticEllipsesAllocator), _dynamicEllipses(_dynamicEllipsesAllocator),
			_staticEllipsesAA(_staticEllipsesAAAllocator), _dynamicEllipsesAA(_dynamicEllipsesAAAllocator),
			_dynamic2DVBSize(0), _static2DDirty(false)
		{
			Init(hwnd, displayModes.GetDX11Mode(modeIdx), fullscreen, multithreaded);
		}
		CDevice(HWND hwnd, uint width, uint height, bool fullscreen, uint refreshRate, bool multithreaded/*, format*/):
			_dynamicRectsAllocator(_dedicatedHeap),
			//_staticRectsAllocator(0), _dynamicRectsAllocator(0),
			//_staticEllipsesAllocator(0), _dynamicEllipsesAllocator(0),
			//_staticEllipsesAAAllocator(0), _dynamicEllipsesAAAllocator(0),
			_staticRects(_staticRectsAllocator), _dynamicRects(_dynamicRectsAllocator),
			_staticEllipses(_staticEllipsesAllocator), _dynamicEllipses(_dynamicEllipsesAllocator),
			_staticEllipsesAA(_staticEllipsesAAAllocator), _dynamicEllipsesAA(_dynamicEllipsesAAAllocator),
			_dynamic2DVBSize(0), _static2DDirty(false)
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
			ASSERT_HR(_swapChain->SetFullscreenState(FALSE, NULL))
			std::for_each(_rectHandles.begin(), _rectHandles.end(), parentNuller);
			std::for_each(_ellipseHandles.begin(), _ellipseHandles.end(), parentNuller);
		}
		virtual IDeviceContext *GetDeviceContext() override
		{
			return new CDeviceContext(_device, _quadLayout, _effect2D, _rectPass, _ellipsePass, _ellipseAAPass, *this);
		}
		virtual void SetMode(uint width, uint height) override
		{
		}
		virtual void SetMode(uint idx) override
		{
		}
		virtual void ToggleFullscreen(bool fullscreen) override
		{
		}
		virtual void test() override
		{
			Draw2DScene();
			ASSERT_HR(_swapChain->Present(0, 0))
			ID3D11DeviceContextPtr immediate_comtext;
			_device->GetImmediateContext(&immediate_comtext);
			const FLOAT color[] = {0, 0, 0, 0};
			ID3D11Texture2DPtr rt;
			ASSERT_HR(_swapChain->GetBuffer(0, __uuidof(rt), reinterpret_cast<void **>(&rt)))
			ID3D11RenderTargetViewPtr rt_view;
			D3D11_RENDER_TARGET_VIEW_DESC rt_view_desc = {DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_TEXTURE2DMS};
			rt_view_desc.Texture2D.MipSlice = 0;
			ASSERT_HR(_device->CreateRenderTargetView(rt, &rt_view_desc, &rt_view))
			immediate_comtext->ClearRenderTargetView(rt_view, color);
			immediate_comtext->OMSetRenderTargets(1, &rt_view.GetInterfacePtr(), _zbufferView);
		}
		virtual _2D::IRect *AddRect(bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle) override;
		virtual _2D::IEllipse *AddEllipse(bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle) override;
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

			ID3D11DeviceContextPtr immediate_context;
			ASSERT_HR(D3D11CreateDeviceAndSwapChain(
				NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
				&_swapChain, &_device, NULL, &immediate_context))

			// create Zbuffer
			const D3D11_TEXTURE2D_DESC zbuffer_desc =
			{
				swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height,
				1, 1, DXGI_FORMAT_D16_UNORM,
				swap_chain_desc.SampleDesc, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0
			};
			D3D11_DEPTH_STENCIL_VIEW_DESC zbuffer_view_desc =
			{
				DXGI_FORMAT_UNKNOWN,
				D3D11_DSV_DIMENSION_TEXTURE2D,
				0
			};
			zbuffer_view_desc.Texture2D.MipSlice = 0;
			ASSERT_HR(_device->CreateTexture2D(&zbuffer_desc, NULL, &_zbuffer))
			ASSERT_HR(_device->CreateDepthStencilView(_zbuffer, &zbuffer_view_desc, &_zbufferView))

			// setup viewport
			const D3D11_VIEWPORT viewport = {0, 0, modeDesc.Width, modeDesc.Height, 0, 1};
			immediate_context->RSSetViewports(1, &viewport);

			// create 2D effect
			UINT shader_flags = 0, effect_flags = 0;
#ifdef _DEBUG
			shader_flags |= D3D10_SHADER_DEBUG;
#endif
			if (!multithreaded)
				effect_flags |= D3D10_EFFECT_SINGLE_THREADED;
			ID3D10BlobPtr compiled_effect, effect_errors;
			ASSERT_HR(D3DX11CompileFromFile(TEXT("..\\2D.fx"), NULL, NULL, NULL, "fx_5_0", shader_flags, effect_flags, NULL, &compiled_effect, &effect_errors, NULL))
#ifdef _DEBUG
			if (effect_errors)
				OutputDebugStringA(LPCSTR(effect_errors->GetBufferPointer()));
#endif
			ASSERT_HR(D3DX11CreateEffectFromMemory(compiled_effect->GetBufferPointer(), compiled_effect->GetBufferSize(), 0, _device, &_effect2D))

			// get technique passes
			_rectPass = _effect2D->GetTechniqueByName("Rect")->GetPassByIndex(0);
			_ellipsePass = _effect2D->GetTechniqueByName("Ellipse")->GetPassByIndex(0);
			_ellipseAAPass = _effect2D->GetTechniqueByName("EllipseAA")->GetPassByIndex(0);
			_ASSERT(_rectPass);
			_ASSERT(_ellipsePass);
			_ASSERT(_ellipseAAPass);

			// init uniforms
			const float res[4] = {modeDesc.Width, modeDesc.Height};
			ASSERT_HR(_effect2D->GetVariableByName("targetRes")->AsVector()->SetFloatVector(res))

			// create 2D IA state objects
			const D3D11_INPUT_ELEMENT_DESC quadDesc[] =
			{
				"POSITION_EXTENTS",	0,	DXGI_FORMAT_R16G16B16A16_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ORDER",			0,	DXGI_FORMAT_R16_UNORM,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ANGLE",			0,	DXGI_FORMAT_R16_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"QUAD_COLOR",		0,	DXGI_FORMAT_R8G8B8A8_UNORM,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0
			};
			//const D3D11_INPUT_ELEMENT_DESC quadDesc[] =
			//{
			//	"POSITION_EXTENTS",	0,	DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
			//	"ORDER",			0,	DXGI_FORMAT_R16_UNORM,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
			//	"ANGLE",			0,	DXGI_FORMAT_R32_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
			//	"QUAD_COLOR",		0,	DXGI_FORMAT_R8G8B8A8_UNORM,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0
			//};
			D3DX11_PASS_DESC pass_desc;
			ASSERT_HR(_effect2D->GetTechniqueByName("Rect")->GetPassByIndex(0)->GetDesc(&pass_desc))
			ASSERT_HR(_device->CreateInputLayout(quadDesc, _countof(quadDesc), pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &_quadLayout))
			//ASSERT_HR(_device->CreateInputLayout(quadDesc, _countof(quadDesc), _effect2D->GetVariableByName("Quad_VS")->AsShader()->))
		}
		CWin32Heap _dedicatedHeap;
		CWin32HeapAllocator<TQuad> _staticRectsAllocator, _dynamicRectsAllocator;
		CWin32HeapAllocator<TQuad> _staticEllipsesAllocator, _dynamicEllipsesAllocator;
		CWin32HeapAllocator<TQuad> _staticEllipsesAAAllocator, _dynamicEllipsesAAAllocator;
		void Draw2DScene();
		ID3D11DevicePtr _device;
		//ID3D11DeviceContextPtr _immediateContext;
		IDXGISwapChainPtr _swapChain;
		ID3D11Texture2DPtr _zbuffer;
		ID3D11DepthStencilViewPtr _zbufferView;
		ID3DX11EffectPtr _effect2D;
		ID3D11InputLayoutPtr _quadLayout;
		//class CRect;
		//class CEllipse;
		//typedef std::list<CRect> TRects;
		//typedef std::list<CEllipse> TEllipses;
		typedef std::list<TQuad, CWin32HeapAllocator<TQuad>> TQuads;
		class CRectHandle;
		class CEllipseHandle;
		typedef std::list<CRectHandle *> TRectHandles;
		typedef std::list<CEllipseHandle *> TEllipseHandles;
		TQuads _staticRects, _dynamicRects;
		TQuads _staticEllipses, _dynamicEllipses;
		TQuads _staticEllipsesAA, _dynamicEllipsesAA;
		TRectHandles _rectHandles;
		TEllipseHandles _ellipseHandles;
		ID3D11BufferPtr _static2DVB, _dynamic2DVB;
		ID3DX11EffectPass *_rectPass, *_ellipsePass, *_ellipseAAPass;
		UINT _dynamic2DVBSize;
		bool _static2DDirty;
	};

//	class CDevice::CRect: public _2D::IRect, private TQuad
//	{
//		virtual ~CRect()
//		{
//			if (parent)
//				(parent->*_container).erase(_thisIter);
//		}
//	public:
//		inline CRect(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle):
//			parent(parent), TQuad(x, y, width, height, layer, angle, color),
//			_container(dynamic ? &CDevice::_dynamicRects : &CDevice::_staticRects),
//			_thisIter(((parent->*_container).push_front(this), (parent->*_container).begin()))
////			_thisIter((parent->*_container).insert(std::lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(std::less<CRect>())), this))
//		{
//			if (!dynamic)
//				parent->_static2DDirty = true;
//		}
//		inline bool operator <(const CRect &rect) const
//		{
//			return TQuad::operator <(rect);
//		}
//		inline operator const TQuad &() const
//		{
//			return *this;
//		}
//		CDevice *parent;
//	private:
//		TRects CDevice::*const _container;
//		const TRects::const_iterator _thisIter;
//	};
//
//	class CDevice::CEllipse: public _2D::IEllipse, private TQuad
//	{
//		// C++ 0X
//		//CEllipse(const CEllipse &) = deleted;
//		//CEllipse &operator =(const CEllipse &) = deleted;
//		virtual ~CEllipse()
//		{
//			if (parent)
//				(parent->*_container).erase(_thisIter);
//		}
//	public:
//		inline CEllipse(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle):
//			parent(parent), TQuad(x, y, rx, ry, layer, angle, color),
//			_container(dynamic ? AA ? &CDevice::_dynamicEllipsesAA : &CDevice::_dynamicEllipses : AA ? &CDevice::_staticEllipsesAA : &CDevice::_staticEllipses),
//			_thisIter(((parent->*_container).push_front(this), (parent->*_container).begin()))
////			_thisIter((parent->*_container).insert(std::lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(std::less<CEllipse>())), this))
//		{
//			if (!dynamic)
//				parent->_static2DDirty = true;
//		}
//		inline bool operator <(const CEllipse &rect) const
//		{
//			return TQuad::operator <(rect);
//		}
//		inline operator const TQuad &() const
//		{
//			return *this;
//		}
//		CDevice *parent;
//	private:
//		TEllipses CDevice::*const _container;
//		const TEllipses::const_iterator _thisIter;
//	};

	class CDevice::CRectHandle: public _2D::IRect
	{
		// C++ 0X
		//CRect(const CRect &) = deleted;
		//CRect &operator =(const CRect &) = deleted;
		virtual void SetPos(float x, float y) override
		{
			_rect->pos.x = x;
			_rect->pos.y = y;
		}
		virtual void SetExtents(float x, float y) override
		{
			_rect->extents.x = x;
			_rect->extents.y = y;
		}
		virtual void SetColor(uint32 color) override
		{
			_rect->color = color;
		}
		virtual void SetAngle(float angle) override
		{
			_rect->angle = angle;
		}
		virtual ~CRectHandle()
		{
			if (parent)
			{
				(parent->*_container).erase(_rect);
				parent->_rectHandles.erase(_thisIter);
			}
		}
	public:
		CRectHandle(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle):
			parent(parent),
			_container(dynamic ? &CDevice::_dynamicRects : &CDevice::_staticRects),
			_rect(((parent->*_container).push_front(TQuad(x, y, width, height, layer, angle, color)), (parent->*_container).begin())),
			_thisIter((parent->_rectHandles.push_front(this), parent->_rectHandles.begin()))
		{
			if (!dynamic)
				parent->_static2DDirty = true;
		}
		CDevice *parent;
	private:
		TQuads CDevice::*const _container;
		const TQuads::iterator _rect;
		const TRectHandles::const_iterator _thisIter;
	};

	class CDevice::CEllipseHandle: public _2D::IEllipse
	{
		// C++ 0X
		//CRect(const CRect &) = deleted;
		//CRect &operator =(const CRect &) = deleted;
		virtual void SetPos(float x, float y) override
		{
			_ellipse->pos.x = x;
			_ellipse->pos.y = y;
		}
		virtual void SetRadii(float rx, float ry) override
		{
			_ellipse->extents.x = rx;
			_ellipse->extents.y = ry;
		}
		virtual void SetColor(uint32 color) override
		{
			_ellipse->color = color;
		}
		virtual void SetAngle(float angle) override
		{
			_ellipse->angle = angle;
		}
		virtual ~CEllipseHandle()
		{
			if (parent)
			{
				(parent->*_container).erase(_ellipse);
				parent->_ellipseHandles.erase(_thisIter);
			}
		}
	public:
		CEllipseHandle(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle):
			parent(parent),
			_container(dynamic ? &CDevice::_dynamicRects : &CDevice::_staticRects),
			_ellipse(((parent->*_container).push_front(TQuad(x, y, rx, ry, layer, angle, color)), (parent->*_container).begin())),
			_thisIter((parent->_ellipseHandles.push_front(this), parent->_ellipseHandles.begin()))
		{
			if (!dynamic)
				parent->_static2DDirty = true;
		}
		CDevice *parent;
	private:
		TQuads CDevice::*const _container;
		const TQuads::iterator _ellipse;
		const TEllipseHandles::const_iterator _thisIter;
	};
}

namespace DX11
{
	_2D::IRect *CDevice::AddRect(bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle)
	{
		return new CRectHandle(this, dynamic, layer, x, y, width, height, color, angle);
	}

	_2D::IEllipse *CDevice::AddEllipse(bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle)
	{
		return new CEllipseHandle(this, dynamic, layer, x, y, rx, ry, color, AA, angle);
	}

	void CDevice::Draw2DScene()
	{
		ID3D11DeviceContextPtr immediate_comtext;
		_device->GetImmediateContext(&immediate_comtext);
#pragma region("static")
		// (re)create VB if nesessary
		if (_static2DDirty)
		{
			_staticRects.sort();
			_staticEllipses.sort();
			_staticEllipsesAA.sort();

			std::vector<TQuad> vb_shadow(_staticRects.size() + _staticEllipses.size() + _staticEllipsesAA.size());
			std::copy(_staticRects.begin(), _staticRects.end(), vb_shadow.begin());
			std::copy(_staticEllipses.begin(), _staticEllipses.end(), vb_shadow.begin() + _staticRects.size());
			std::copy(_staticEllipsesAA.begin(), _staticEllipsesAA.end(), vb_shadow.begin() + _staticRects.size() + _staticEllipses.size());
			
			const D3D11_BUFFER_DESC VB_desc =
			{
				vb_shadow.size() * sizeof(TQuad),	//ByteWidth
				D3D11_USAGE_DEFAULT,				//Usage
				D3D11_BIND_VERTEX_BUFFER,			//BindFlags
				0,									//CPUAccessFlags
				0,									//MiscFlags
				0									//StructureByteStride
			};
			const D3D11_SUBRESOURCE_DATA init_data = {vb_shadow.data(), 0, 0};
			ASSERT_HR(_device->CreateBuffer(&VB_desc, &init_data, &_static2DVB))
			_static2DDirty = false;
		}

		// draw
		if (_static2DVB)
		{
			immediate_comtext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			immediate_comtext->IASetInputLayout(_quadLayout);
			UINT stride = sizeof(TQuad), offset = 0;
			auto draw = [&](ID3DX11EffectPass *pass, UINT vcount)
			{
				ASSERT_HR(pass->Apply(0, immediate_comtext))
				immediate_comtext->IASetVertexBuffers(0, 1, &_static2DVB.GetInterfacePtr(), &stride, &offset);
				immediate_comtext->Draw(vcount, 0);
				offset += vcount * sizeof(TQuad);
			};
			if (!_staticRects.empty()) draw(_rectPass, _staticRects.size());
			if (!_staticEllipses.empty()) draw(_ellipsePass, _staticEllipses.size());
			if (!_staticEllipsesAA.empty()) draw(_ellipseAAPass, _staticEllipsesAA.size());
		}
#pragma endregion
#pragma region("dynamic")
		immediate_comtext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		immediate_comtext->IASetInputLayout(_quadLayout);
		if (!_dynamicRects.empty())
		{
			static std::list<TQuad> vb_shadow;
			// (re)create VB if nesessary
			if (!_dynamic2DVB || _dynamic2DVBSize < _dynamicRects.size() * sizeof(TQuad))
			{
				const D3D11_BUFFER_DESC VB_desc =
				{
					_dynamic2DVBSize = _dynamicRects.size() * sizeof(TQuad),	//ByteWidth
					D3D11_USAGE_DYNAMIC,										//Usage
					D3D11_BIND_VERTEX_BUFFER,									//BindFlags
					D3D11_CPU_ACCESS_WRITE,										//CPUAccessFlags
					0,															//MiscFlags
					0															//StructureByteStride
				};
				ASSERT_HR(_device->CreateBuffer(&VB_desc, NULL, &_dynamic2DVB))
				vb_shadow.assign(_dynamicRects.begin(), _dynamicRects.end());
			}
			D3D11_MAPPED_SUBRESOURCE mapped;
			ASSERT_HR(immediate_comtext->Map(_dynamic2DVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))
			std::copy(_dynamicRects.begin(), _dynamicRects.end(), reinterpret_cast<TQuad *>(mapped.pData));
			//std::copy(vb_shadow.begin(), vb_shadow.end(), reinterpret_cast<TQuad *>(mapped.pData));
			//memcpy(mapped.pData, vb_shadow.data(), _dynamic2DVBSize);
			immediate_comtext->Unmap(_dynamic2DVB, 0);
			UINT stride = sizeof(TQuad), offset = 0;
			ASSERT_HR(_rectPass->Apply(0, immediate_comtext))
			immediate_comtext->IASetVertexBuffers(0, 1, &_dynamic2DVB.GetInterfacePtr(), &stride, &offset);
			immediate_comtext->Draw(_dynamicRects.size(), 0);
		}
#pragma endregion
	}

	void CDeviceContext::DrawPoint(float x, float y, uint32 color, float size)
	{
	}

	void CDeviceContext::DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width)
	{
	}

	void CDeviceContext::DrawRect(float x, float y, float width, float height, uint32 color, float angle)
	{
		_rects.push_back(_dev.AddRect(true, _curLayer--, x, y, width, height, color, angle));
		//ASSERT_HR(_rectPass->Apply(0, _deviceContext))
		//_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		//_deviceContext->IASetInputLayout(_quadLayout);
		//_2DVB.Draw(_deviceContext, sizeof(TQuad), 1, CQuadFiller(x, y, width, height, 0, angle, color));
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

	void CDeviceContext::DrawEllipse(float x, float y, float rx, float ry, uint32 color, bool AA, float angle)
	{
		ASSERT_HR((AA ? _ellipseAAPass : _ellipsePass)->Apply(0, _deviceContext))
		_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		_deviceContext->IASetInputLayout(_quadLayout);
		_2DVB.Draw(_deviceContext, sizeof(TQuad), 1, CQuadFiller(x, y, rx, ry, 0, angle, color));
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