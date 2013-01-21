#include "stdafx.h"
#include <Unknwn.h>
#include "..\\..\\Include\\CPP\\DGLE2.h"
#include "Renderer.h"
#include <D3D11.h>
#include <D3DX11.h>
#include <D3DX10Math.h>
#include "D3dx11effect.h"
#include "com ptr defs.h"

#define ASSERT_HR(...) {HRESULT hr = __VA_ARGS__;_ASSERT(SUCCEEDED(hr));}

//#define _2D_FLOAT32_TO_FLOAT16

#if APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#error APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#endif

using namespace std;
using namespace DGLE2;
using namespace DGLE2::Renderer::HighLevel;
using namespace Textures;
using namespace Materials;
using namespace Geometry;
using namespace Instances;
using namespace Instances::_2D;
using namespace DisplayModes;

class CDtor: virtual DGLE2::Renderer::IDtor
{
	virtual void operator ~() const override
	{
		_ASSERTE(_externRef);
		_externRef.reset();
	}
protected:
	/*
	default shared_ptr's deleter can not access protected dtor
	dtor called from internal implementation-dependent class => 'friend shared_ptr<CDtor>;' does not help
	*/
	CDtor(): _externRef(this, [](const CDtor *dtor){delete dtor;}) {}
	/*
	C++11
	virtual ~CDtor() = default;
	*/
	virtual ~CDtor() {}
	const shared_ptr<CDtor> &GetRef()
	{
		return _externRef;
	}
	const shared_ptr<const CDtor> &GetRef() const
	{
		return _externRef;
	}
private:
	mutable shared_ptr<CDtor> _externRef;	// reference from lib user
};

template<class Parent>
class CRef: private CDtor
{
public:
	shared_ptr<Parent> GetRef()
	{
		return static_pointer_cast<Parent>(CDtor::GetRef());
	}
	shared_ptr<const Parent> GetRef() const
	{
		return static_pointer_cast<const Parent>(CDtor::GetRef());
	}
};

//template<class Container>
//class CIterDtor
//{
//public:
//	CIterDtor(Container &container, typename Container::iterator iter): _container(container), _iter(iter) {}
//	~CIterDtor()
//	{
//		_container.erase(_iter);
//	}
//	operator typename Container::reference()
//	{
//		return *_iter;
//	}
//	operator typename Container::const_reference()
//	{
//		return *_iter;
//	}
//private:
//	Container &_container;
//	const typename Container::iterator _iter;
//};
//
//template<class Container>
//class CIterHandle: public CIterDtor<Container>
//{
//public:
//	template<typename Item>
//	inline CIterHandle(Container &container, Item &&item);
//};
//
//template<class Container>
//template<typename Item>
//CIterHandle<Container>::CIterHandle(Container &container, Item &&item):
//	CIterDtor(container)
//{
//}

namespace DX11
{
	using namespace ComPtrs;

	inline const char *Format2String(DXGI_FORMAT format) noexcept
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

	inline const char *ScanlineOrdering2string(DXGI_MODE_SCANLINE_ORDER scanlineOrder) noexcept
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

	inline const char *Scaling2String(DXGI_MODE_SCALING scaling) noexcept
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

	string ModeDesc2String(const DXGI_MODE_DESC &desc)
	{
		return string("Format:\t") + Format2String(desc.Format) +
			"\nScanlineOrdering:\t" + ScanlineOrdering2string(desc.ScanlineOrdering) +
			"\nScaling:\t" + Scaling2String(desc.Scaling);
	}

	class CDynamicVB
	{
	public:
		CDynamicVB() noexcept {}
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
			callback(reinterpret_cast<uint8 *>(mapped.pData) + _offset);
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
		TQuad() noexcept {}
		TQuad(float x, float y, float width, float height, uint16 layer, float angle, uint32 color):
			pos(x, y), extents(width, height), layer(layer), angle(angle), color(color)
		{
		}
		bool operator <(const TQuad &quad) const noexcept
		{
			return layer < quad.layer;
		}
#ifdef _2D_FLOAT32_TO_FLOAT16
		D3DXVECTOR2_16F pos, extents;
		uint16 layer;
		D3DXFLOAT16 angle;
		uint32 color;
#else
		D3DXVECTOR2 pos, extents;
		uint16 layer;
		float angle;
		uint32 color;
#endif
	};
#pragma pack(pop)

	const class CDisplayModes: public IDisplayModes
	{
		class CDesc: CDtor, public IDesc
		{
		public:
			CDesc(const string &&desc): _desc(desc) {}
			virtual operator const char *() const override
			{
				return _desc.c_str();
			}
		private:
			const string _desc;
			//virtual ~CDesc() = default;
		};
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
		virtual uint Count() const override
		{
			return _modes.size();
		}
		virtual TDispModeDesc Get(uint idx) const override
		{
			TDispMode mode =
			{
				_modes[idx].Width,
				_modes[idx].Height,
				float(_modes[idx].RefreshRate.Numerator) / _modes[idx].RefreshRate.Denominator
			};
			return TDispModeDesc(mode, new CDesc(ModeDesc2String(_modes[idx])));
		}
		const DXGI_MODE_DESC &GetDX11Mode(uint idx) const
		{
			return _modes[idx];
		}
	private:
		vector<DXGI_MODE_DESC> _modes;
	} displayModes;

	namespace
	{
		class CWin32Heap
		{
		public:
			CWin32Heap(): _handle(HeapCreate(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE, 0, 0))
			{
				if (!_handle) throw "fail to create heap";
			}
			~CWin32Heap()
			{
				if (!HeapDestroy(_handle)) throw "fail to destroy heap";
			}
			operator HANDLE() const noexcept
			{
				return _handle;
			}
		private:
			const HANDLE _handle;
		};

		template<typename T>
		class CWin32HeapAllocator: public allocator<T>
		{
			template<typename T>
			friend class CWin32HeapAllocator;
		public:
			template<typename Other>
			struct rebind
			{
				typedef CWin32HeapAllocator<Other> other;
			};
			CWin32HeapAllocator(HANDLE heap = HANDLE(_get_heap_handle())) noexcept: _heap(heap) {}
			template<typename Other>
			CWin32HeapAllocator(const CWin32HeapAllocator<Other> &src) noexcept: _heap(src._heap) {}
			pointer allocate(size_type count, const void * = NULL)
			{
				try
				{
					return pointer(HeapAlloc(_heap, 0, count * sizeof(T)));
				}
				catch (...)
				{
					throw bad_alloc();
				}
			}
			void deallocate(pointer ptr, size_type)
			{
				if (!HeapFree(_heap, 0, ptr)) throw "fail to free heap block";
			}
		private:
			HANDLE _heap;
		};
	}

	class CRenderer: private CRef<CRenderer>, public IRenderer
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
		// C++11
		// use delegating ctors
		CRenderer(HWND hwnd, uint modeIdx, bool fullscreen, bool multithreaded):
			_dynamicRectsAllocator(_dedicatedHeap),
			//_staticRectsAllocator(0), _dynamicRectsAllocator(0),
			//_staticEllipsesAllocator(0), _dynamicEllipsesAllocator(0),
			//_staticEllipsesAAAllocator(0), _dynamicEllipsesAAAllocator(0),
			_staticRects(_staticRectsAllocator), _dynamicRects(_dynamicRectsAllocator),
			_staticEllipses(_staticEllipsesAllocator), _dynamicEllipses(_dynamicEllipsesAllocator),
			_staticEllipsesAA(_staticEllipsesAAAllocator), _dynamicEllipsesAA(_dynamicEllipsesAAAllocator),
			_dynamic2DVBSize(0), _static2DDirty(false),
			_VBSie(64), _VBStart(0), _VCount(0), _count(0), _curLayer(~0)
		{
			Init(hwnd, displayModes.GetDX11Mode(modeIdx), fullscreen, multithreaded);
		}
		CRenderer(HWND hwnd, uint width, uint height, bool fullscreen, uint refreshRate, bool multithreaded/*, format*/):
			_dynamicRectsAllocator(_dedicatedHeap),
			//_staticRectsAllocator(0), _dynamicRectsAllocator(0),
			//_staticEllipsesAllocator(0), _dynamicEllipsesAllocator(0),
			//_staticEllipsesAAAllocator(0), _dynamicEllipsesAAAllocator(0),
			_staticRects(_staticRectsAllocator), _dynamicRects(_dynamicRectsAllocator),
			_staticEllipses(_staticEllipsesAllocator), _dynamicEllipses(_dynamicEllipsesAllocator),
			_staticEllipsesAA(_staticEllipsesAAAllocator), _dynamicEllipsesAA(_dynamicEllipsesAAAllocator),
			_dynamic2DVBSize(0), _static2DDirty(false),
			_VBSie(64), _VBStart(0), _VCount(0), _count(0), _curLayer(~0)
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
		virtual void SetMode(uint width, uint height) override
		{
		}
		virtual void SetMode(uint idx) override
		{
		}
		virtual void ToggleFullscreen(bool fullscreen) override
		{
		}
		virtual void NextFrame() override
		{
			// immediate 2D
			_immediateContext->Unmap(_VB, 0);
			if (_VCount)
			{
				_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				_immediateContext->IASetInputLayout(_quadLayout);
				ASSERT_HR(_rectPass->Apply(0, _immediateContext))
				const UINT stride = sizeof(TQuad), offset = 0;
				_immediateContext->IASetVertexBuffers(0, 1, &_VB.GetInterfacePtr(), &stride, &offset);
				_immediateContext->Draw(_VCount, _VBStart);
				_VBStart = _VCount = 0;
			}
			if (_VBSie < _count)
			{
				D3D11_BUFFER_DESC desc = {sizeof(TQuad) * (_VBSie = _count), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
				ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
			}
			D3D11_MAPPED_SUBRESOURCE mapped;
			ASSERT_HR(_immediateContext->Map(_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))
			_mappedVB = reinterpret_cast<TQuad *>(mapped.pData);
			_count = 0;
			_curLayer = ~0;

			Draw2DScene();

			// test
			ASSERT_HR(_effect2D->GetVariableByName("buff")->AsShaderResource()->SetResource(_srv))
			//ASSERT_HR(_effect2D->GetVariableByName("rwbuff")->AsUnorderedAccessView()->SetUnorderedAccessView(_uav))
			ASSERT_HR(_effect2D->GetTechniqueByName("test")->GetPassByIndex(0)->Apply(0, _immediateContext))
			_immediateContext->Dispatch(32768, 1, 1);
			_immediateContext->Flush();

			ASSERT_HR(_swapChain->Present(0, 0))
			ID3D11DeviceContextPtr immediate_comtext;
			const FLOAT color[] = {0, 0, 0, 0};
			ID3D11Texture2DPtr rt;
			ASSERT_HR(_swapChain->GetBuffer(0, __uuidof(rt), reinterpret_cast<void **>(&rt)))
			ID3D11RenderTargetViewPtr rt_view;
			D3D11_RENDER_TARGET_VIEW_DESC rt_view_desc = {DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_TEXTURE2DMS};
			rt_view_desc.Texture2D.MipSlice = 0;
			ASSERT_HR(_device->CreateRenderTargetView(rt, &rt_view_desc, &rt_view))
			_immediateContext->ClearRenderTargetView(rt_view, color);
			_immediateContext->ClearDepthStencilView(_zbufferView, D3D11_CLEAR_DEPTH, 1, 0);
			_immediateContext->OMSetRenderTargets(1, &rt_view.GetInterfacePtr(), _zbufferView);
		}

		virtual Materials::IMaterial *CreateMaterial() override;
		virtual Geometry::IMesh *CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords) override;
		virtual IInstance *CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material) override;

		virtual void DrawPoint(float x, float y, uint32 color, float size) override;
		virtual void DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width) override;
		virtual void DrawRect(float x, float y, float width, float height, uint32 color, Textures::ITexture2D *texture, float angle) override;
		virtual void DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], Textures::ITexture2D *texture, float angle) override;
		virtual void DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color) override;
		virtual void DrawCircle(float x, float y, float r, uint32 color) override;
		virtual void DrawEllipse(float x, float y, float rx, float ry, uint32 color, bool AA, float angle) override;

		virtual IRect *AddRect(bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle) override;
		virtual IEllipse *AddEllipse(bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle) override;
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
				&_swapChain, &_device, NULL, &_immediateContext))

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
			_immediateContext->RSSetViewports(1, &viewport);

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
#ifdef _2D_FLOAT32_TO_FLOAT16
			const D3D11_INPUT_ELEMENT_DESC quadDesc[] =
			{
				"POSITION_EXTENTS",	0,	DXGI_FORMAT_R16G16B16A16_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ORDER",			0,	DXGI_FORMAT_R16_UNORM,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ANGLE",			0,	DXGI_FORMAT_R16_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"QUAD_COLOR",		0,	DXGI_FORMAT_R8G8B8A8_UNORM,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0
			};
#else
			const D3D11_INPUT_ELEMENT_DESC quadDesc[] =
			{
				"POSITION_EXTENTS",	0,	DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ORDER",			0,	DXGI_FORMAT_R16_UNORM,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"ANGLE",			0,	DXGI_FORMAT_R32_FLOAT,			0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0,
				"QUAD_COLOR",		0,	DXGI_FORMAT_R8G8B8A8_UNORM,		0,	D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0
			};
#endif
			D3DX11_PASS_DESC pass_desc;
			ASSERT_HR(_effect2D->GetTechniqueByName("Rect")->GetPassByIndex(0)->GetDesc(&pass_desc))
			ASSERT_HR(_device->CreateInputLayout(quadDesc, _countof(quadDesc), pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &_quadLayout))
			//ASSERT_HR(_device->CreateInputLayout(quadDesc, _countof(quadDesc), _effect2D->GetVariableByName("Quad_VS")->AsShader()->))

			// immediate 2D
			new(&_2DVB) CDynamicVB(_device, sizeof(TQuad) * 64);
			D3D11_BUFFER_DESC desc = {sizeof(TQuad) * _VBSie, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
			ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
			D3D11_MAPPED_SUBRESOURCE mapped;
			_immediateContext->Map(_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			_mappedVB = reinterpret_cast<TQuad *>(mapped.pData);

			// test
			desc.ByteWidth = 32768*512*4;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_buf))
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {DXGI_FORMAT_R32_TYPELESS, D3D11_SRV_DIMENSION_BUFFEREX};
			srv_desc.BufferEx.FirstElement = 0;
			srv_desc.BufferEx.NumElements = 32768*512;
			srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			ASSERT_HR(_device->CreateShaderResourceView(_buf, &srv_desc, &_srv))
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {DXGI_FORMAT_R32_TYPELESS, D3D11_UAV_DIMENSION_BUFFER};
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = 32768*512;
			uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			ASSERT_HR(_device->CreateUnorderedAccessView(_buf, &uav_desc, &_uav))
			const D3D11_TEXTURE2D_DESC textureDesc =
			{
				4096, 4096, 1, 1,
				DXGI_FORMAT_R32G32B32_UINT,
				{1, 0},
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0, 0
			};
			ID3D11Texture2DPtr texture;
			ASSERT_HR(_device->CreateTexture2D(&textureDesc, NULL, &texture))
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MostDetailedMip = 0;
			srv_desc.Texture2D.MipLevels = 1;
			ASSERT_HR(_device->CreateShaderResourceView(texture, &srv_desc, &_textureView))
			_immediateContext->CSSetShaderResources(0, 1, &_textureView.GetInterfacePtr());
		}

		virtual ~CRenderer()
		{
			ASSERT_HR(_swapChain->SetFullscreenState(FALSE, NULL))
		}

		void Draw2DScene();
	private:
		CWin32Heap _dedicatedHeap;
		CWin32HeapAllocator<TQuad> _staticRectsAllocator, _dynamicRectsAllocator;
		CWin32HeapAllocator<TQuad> _staticEllipsesAllocator, _dynamicEllipsesAllocator;
		CWin32HeapAllocator<TQuad> _staticEllipsesAAAllocator, _dynamicEllipsesAAAllocator;
		ID3D11DevicePtr _device;
		ID3D11DeviceContextPtr _immediateContext;
		IDXGISwapChainPtr _swapChain;
		ID3D11Texture2DPtr _zbuffer;
		ID3D11DepthStencilViewPtr _zbufferView;
		//class CRect;
		//class CEllipse;
		//typedef list<CRect> TRects;
		//typedef list<CEllipse> TEllipses;

		// 2D state objects
		ID3DX11EffectPtr _effect2D;
		ID3D11InputLayoutPtr _quadLayout;
		ID3DX11EffectPass *_rectPass, *_ellipsePass, *_ellipseAAPass;

		// immediate 2D
		CDynamicVB _2DVB;
		ID3D11BufferPtr _VB;
		TQuad *_mappedVB;
		UINT _VBSie, _VBStart, _VCount;
		unsigned _count;
		unsigned short _curLayer;

		// 2D scene
		typedef list<TQuad, CWin32HeapAllocator<TQuad>> TQuads;
		class CQuadHandle;
		class CRectHandle;
		class CEllipseHandle;
		TQuads _staticRects, _dynamicRects;
		TQuads _staticEllipses, _dynamicEllipses;
		TQuads _staticEllipsesAA, _dynamicEllipsesAA;
		ID3D11BufferPtr _static2DVB, _dynamic2DVB;
		UINT _dynamic2DVBSize;
		bool _static2DDirty;

		// test
		ID3D11BufferPtr _buf;
		ID3D11ShaderResourceViewPtr _srv, _textureView;
		ID3D11UnorderedAccessViewPtr _uav;
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
////			_thisIter((parent->*_container).insert(lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(less<CRect>())), this))
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
//		// C++11
//		//CEllipse(const CEllipse &) = delete;
//		//CEllipse &operator =(const CEllipse &) = delete;
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
////			_thisIter((parent->*_container).insert(lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(less<CEllipse>())), this))
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

	class CRenderer::CQuadHandle: ::CDtor
	{
		// C++11
		//CQuadHandle(const CQuadHandle &) = delete;
		//CQuadHandle &operator =(const CQuadHandle &) = delete;
	protected:
		CQuadHandle(shared_ptr<CRenderer> &&renderer, TQuads CRenderer::*const container, TQuad &&quad, bool dynamic):
			_renderer(move(renderer)),
			_container(container),
			_quad(((*_renderer.*_container).push_front(move(quad)), (*_renderer.*_container).begin()))
		{
			if (!dynamic)
				_renderer->_static2DDirty = true;
		}
		virtual ~CQuadHandle()
		{
			(*_renderer.*_container).erase(_quad);
		}
	protected:
		const shared_ptr<CRenderer> _renderer;
	private:
		TQuads CRenderer::*const _container;
	protected:
		const TQuads::iterator _quad;
	};

	class CRenderer::CRectHandle: private CQuadHandle, public IRect
	{
		// C++11
		//CRectHandle(const CRectHandle &) = delete;
		//CRectHandle &operator =(const CRectHandle &) = delete;
		virtual void SetPos(float x, float y) override
		{
			_quad->pos.x = x;
			_quad->pos.y = y;
		}
		virtual void SetExtents(float x, float y) override
		{
			_quad->extents.x = x;
			_quad->extents.y = y;
		}
		virtual void SetColor(uint32 color) override
		{
			_quad->color = color;
		}
		virtual void SetAngle(float angle) override
		{
			_quad->angle = angle;
		}
	public:
		CRectHandle(shared_ptr<CRenderer> &&renderer, bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle):
			CQuadHandle(
				move(renderer),
				dynamic ? &CRenderer::_dynamicRects : &CRenderer::_staticRects,
				TQuad(x, y, width, height, layer, angle, color),
				dynamic)
		{
		}
	private:
		//virtual ~CRectHandle() = default;
	};

	class CRenderer::CEllipseHandle: private CQuadHandle, public IEllipse
	{
		// C++11
		//CEllipseHandle(const CEllipseHandle &) = delete;
		//CEllipseHandle &operator =(const CEllipseHandle &) = delete;
		virtual void SetPos(float x, float y) override
		{
			_quad->pos.x = x;
			_quad->pos.y = y;
		}
		virtual void SetRadii(float rx, float ry) override
		{
			_quad->extents.x = rx;
			_quad->extents.y = ry;
		}
		virtual void SetColor(uint32 color) override
		{
			_quad->color = color;
		}
		virtual void SetAngle(float angle) override
		{
			_quad->angle = angle;
		}
	public:
		CEllipseHandle(shared_ptr<CRenderer> &&renderer, bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle):
			CQuadHandle(
				move(renderer),
				dynamic ? AA ? &CRenderer::_dynamicEllipsesAA : &CRenderer::_dynamicEllipses : AA? &CRenderer::_staticEllipsesAA : &CRenderer::_staticEllipses,
				TQuad(x, y, rx, ry, layer, angle, color),
				dynamic)
		{
		}
	private:
		//virtual ~CEllipseHandle() = default;
	};

	class CMesh: public CRef<CMesh>, public Geometry::IMesh
	{
		friend IMesh *CRenderer::CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords);
		virtual AABB<3> GetAABB() const override;
	private:
		CMesh(ID3D11Device *device, uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords);
		//virtual ~CMesh() = default;
	private:
		ID3D11BufferPtr	_geometry;
		UINT			_ibOffset, _vbOffsets[1];
		AABB<3>			_aabb;
		DXGI_FORMAT		_IBFormat;
	};

	inline AABB<3> CMesh::GetAABB() const
	{
		return _aabb;
	}

	// 1 call site => inline
	// TODO: clarify 'vcount <= D3D11_16BIT_INDEX_STRIP_CUT_VALUE + 1'
	inline CMesh::CMesh(ID3D11Device *device, uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords):
		_aabb(reinterpret_cast<const float (*)[3]>(coords), reinterpret_cast<const float (*)[3]>(coords) + vcount),
		_IBFormat(vcount <= D3D11_16BIT_INDEX_STRIP_CUT_VALUE + 1 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT)
	{
		decltype(declval<D3D11_BUFFER_DESC>().ByteWidth)
			idxSize = (_IBFormat == DXGI_FORMAT_R16_UINT ? sizeof(uint16) : sizeof(uint32)) * icount,
			coordsSize = sizeof *coords * 3 * vcount;
		const D3D11_BUFFER_DESC desc =
		{
			idxSize + coordsSize,								//ByteWidth
			D3D11_USAGE_IMMUTABLE,								//Usage
			D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER,	//BindFlags
			0,													//CPUAccessFlags
			0,													//MiscFlags
			0													//StructureByteStride
		};

		unique_ptr<uint8 []> buf(new uint8[desc.ByteWidth]);

		_vbOffsets[0] = 0;
		memcpy(buf.get() + _vbOffsets[0], coords, coordsSize);

		_ibOffset = _vbOffsets[0] + coordsSize;
		const auto bufIdx = buf.get() + _ibOffset;
		if (_IBFormat == DXGI_FORMAT_R32_UINT)
			memcpy(bufIdx, idx, idxSize);
		else
			copy(idx, idx + icount, reinterpret_cast<uint16 *>(bufIdx));

		const D3D11_SUBRESOURCE_DATA data =
		{
			buf.get(),	//pSysMem
			0,			//SysMemPitch
			0			//SysMemSlicePitch
		};

		ASSERT_HR(device->CreateBuffer(&desc, &data, &_geometry));
	}

	class CMaterial: public CRef<CMaterial>, public Materials::IMaterial
	{
		friend IMaterial *CRenderer::CreateMaterial();
		virtual void SetAmbientColor(const float color[3]) override;
		virtual void SetDiffuseColor(const float color[3]) override;
		virtual void SetSpecularColor(const float color[3]) override;
		virtual void SetHeightScale(float scale) override;
		virtual void SetEnvAmount(float amount) override;
		virtual void SetShininess(float shininess) override;
		virtual void SetTexMappingMode(Materials::E_TEX_MAPPING texMapping) override;
		virtual void SetNormalTechnique(Materials::E_NORMAL_TECHNIQUE technique) override;
		virtual void SetParallaxTechnique(Materials::E_PARALLAX_TECHNIQUE technique) override;
		virtual void SetDiffuseTexture(IMatrialTexture *texture) override;
		virtual void SetSpecularTexture(IMatrialTexture *texture) override;
		virtual void SetNormalTexture(IMatrialTexture *texture) override;
		virtual void SetHeightTexture(IMatrialTexture *texture) override;
		virtual void SetEnvTexture(IMatrialTexture *texture) override;
		virtual void SetEnvMask(IMatrialTexture *texture) override;
	private:
		CMaterial();
		//virtual ~CMaterial() = default;
	private:
		float
										_ambientColor[3],
										_diffuseColor[3],
										_specularColor[3],

										_heightScale,
										_envAmount,
										_shininess;

		Materials::E_TEX_MAPPING		_texMapping;
		Materials::E_NORMAL_TECHNIQUE	_normalTechnique;
		Materials::E_PARALLAX_TECHNIQUE	_parallaxTechnique;

		//shared_ptr<>
		//								_diffuseTexture,
		//								_specularTexture,
		//								_normalTexture,
		//								_heightTexture,
		//								_envTexture,
		//								_envMask;
	};

	inline void CMaterial::SetAmbientColor(const float color[3])
	{
		memcpy(_ambientColor, color, sizeof _ambientColor);
	}

	inline void CMaterial::SetDiffuseColor(const float color[3])
	{
		memcpy(_diffuseColor, color, sizeof _diffuseColor);
	}

	inline void CMaterial::SetSpecularColor(const float color[3])
	{
		memcpy(_specularColor, color, sizeof _specularColor);
	}

	inline void CMaterial::SetHeightScale(float scale)
	{
		_heightScale = scale;
	};

	inline void CMaterial::SetEnvAmount(float amount)
	{
		_envAmount = amount;
	};

	inline void CMaterial::SetShininess(float shininess)
	{
		_shininess = shininess;
	};

	inline void CMaterial::SetTexMappingMode(Materials::E_TEX_MAPPING texMapping)
	{
		_texMapping = texMapping;
	};

	inline void CMaterial::SetNormalTechnique(Materials::E_NORMAL_TECHNIQUE technique)
	{
		_normalTechnique = technique;
	};

	inline void CMaterial::SetParallaxTechnique(Materials::E_PARALLAX_TECHNIQUE technique)
	{
		_parallaxTechnique = technique;
	};

	inline void CMaterial::SetDiffuseTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetSpecularTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetNormalTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetHeightTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetEnvTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetEnvMask(IMatrialTexture *texture)
	{
	};

	// 1 call site => inline
	inline CMaterial::CMaterial():
		_heightScale(1), _envAmount(0), _shininess(128),
		_texMapping(Materials::E_TEX_MAPPING::UV), _normalTechnique(Materials::E_NORMAL_TECHNIQUE::UNPERTURBED), _parallaxTechnique(Materials::E_PARALLAX_TECHNIQUE::NONE)
	{
		fill(_ambientColor, _ambientColor + _countof(_ambientColor), 1);
		fill(_diffuseColor, _diffuseColor + _countof(_diffuseColor), 1);
		fill(_specularColor, _specularColor + _countof(_specularColor), 1);
	}

	class CInstance: CDtor, public IInstance
	{
		friend IInstance *CRenderer::CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material);
	private:
		CInstance(shared_ptr<const CMesh> &&mesh, shared_ptr<const CMaterial> &&material);
		//virtual ~CInstance() = default;
	private:
		const shared_ptr<const CMesh>		_mesh;
		const shared_ptr<const CMaterial>	_material;
	};

	// 1 call site => inline
	inline CInstance::CInstance(shared_ptr<const CMesh> &&mesh, shared_ptr<const CMaterial> &&material):
		_mesh(mesh), _material(material)
	{
	}
}

namespace DX11
{
	IRect *CRenderer::AddRect(bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle)
	{
		return new CRectHandle(GetRef(), dynamic, layer, x, y, width, height, color, angle);
	}

	IEllipse *CRenderer::AddEllipse(bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle)
	{
		return new CEllipseHandle(GetRef(), dynamic, layer, x, y, rx, ry, color, AA, angle);
	}

	Materials::IMaterial *CRenderer::CreateMaterial()
	{
		return new CMaterial;
	}

	Geometry::IMesh *CRenderer::CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords)
	{
		return new CMesh(_device, icount, idx, vcount, coords);
	}

	IInstance *CRenderer::CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material)
	{
		return new CInstance(static_cast<const CMesh &>(mesh).GetRef(), static_cast<const CMaterial &>(material).GetRef());
	}

	void CRenderer::Draw2DScene()
	{
#pragma region("static")
		// (re)create VB if nesessary
		if (_static2DDirty)
		{
			_staticRects.sort();
			_staticEllipses.sort();
			_staticEllipsesAA.sort();

			vector<TQuad> vb_shadow(_staticRects.size() + _staticEllipses.size() + _staticEllipsesAA.size());
			copy(_staticRects.begin(), _staticRects.end(), vb_shadow.begin());
			copy(_staticEllipses.begin(), _staticEllipses.end(), vb_shadow.begin() + _staticRects.size());
			copy(_staticEllipsesAA.begin(), _staticEllipsesAA.end(), vb_shadow.begin() + _staticRects.size() + _staticEllipses.size());
			
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
			_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			_immediateContext->IASetInputLayout(_quadLayout);
			UINT stride = sizeof(TQuad), offset = 0;
			auto draw = [&](ID3DX11EffectPass *pass, UINT vcount)
			{
				ASSERT_HR(pass->Apply(0, _immediateContext))
				_immediateContext->IASetVertexBuffers(0, 1, &_static2DVB.GetInterfacePtr(), &stride, &offset);
				_immediateContext->Draw(vcount, 0);
				offset += vcount * sizeof(TQuad);
			};
			if (!_staticRects.empty()) draw(_rectPass, _staticRects.size());
			if (!_staticEllipses.empty()) draw(_ellipsePass, _staticEllipses.size());
			if (!_staticEllipsesAA.empty()) draw(_ellipseAAPass, _staticEllipsesAA.size());
		}
#pragma endregion
#pragma region("dynamic")
		_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		_immediateContext->IASetInputLayout(_quadLayout);
		if (!_dynamicRects.empty())
		{
			static list<TQuad> vb_shadow;
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
			ASSERT_HR(_immediateContext->Map(_dynamic2DVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))
			copy(_dynamicRects.begin(), _dynamicRects.end(), reinterpret_cast<TQuad *>(mapped.pData));
			//copy(vb_shadow.begin(), vb_shadow.end(), reinterpret_cast<TQuad *>(mapped.pData));
			//memcpy(mapped.pData, vb_shadow.data(), _dynamic2DVBSize);
			_immediateContext->Unmap(_dynamic2DVB, 0);
			UINT stride = sizeof(TQuad), offset = 0;
			ASSERT_HR(_rectPass->Apply(0, _immediateContext))
			_immediateContext->IASetVertexBuffers(0, 1, &_dynamic2DVB.GetInterfacePtr(), &stride, &offset);
			_immediateContext->Draw(_dynamicRects.size(), 0);
		}
#pragma endregion
	}

	void CRenderer::DrawPoint(float x, float y, uint32 color, float size)
	{
	}

	void CRenderer::DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width)
	{
	}

	void CRenderer::DrawRect(float x, float y, float width, float height, uint32 color, Textures::ITexture2D *texture, float angle)
	{
		if (_VCount == _VBSie)
		{
			_immediateContext->Unmap(_VB, 0);
			_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			_immediateContext->IASetInputLayout(_quadLayout);
			ASSERT_HR(_rectPass->Apply(0, _immediateContext))
			const UINT stride = sizeof(TQuad), offset = 0;
			_immediateContext->IASetVertexBuffers(0, 1, &_VB.GetInterfacePtr(), &stride, &offset);
			_immediateContext->Draw(_VCount, _VBStart);
			_VBStart = _VCount = 0;
			D3D11_MAPPED_SUBRESOURCE mapped;
			_immediateContext->Map(_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			_mappedVB = reinterpret_cast<TQuad *>(mapped.pData);
		}
		*_mappedVB++ = TQuad(x, y, width, height, _curLayer, angle, color);
		//new(_mappedVB++) TQuad(x, y, width, height, _curLayer, angle, color);
		_VCount++;
		_count++;
		//_rects.push_back(_dev.AddRect(true, _curLayer--, x, y, width, height, color, angle));
		//ASSERT_HR(_rectPass->Apply(0, _deviceContext))
		//_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		//_deviceContext->IASetInputLayout(_quadLayout);
		//_2DVB.Draw(_deviceContext, sizeof(TQuad), 1, CQuadFiller(x, y, width, height, 0, angle, color));
	}

	void CRenderer::DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], Textures::ITexture2D *texture, float angle)
	{
	}

	void CRenderer::DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color)
	{
	}

	void CRenderer::DrawCircle(float x, float y, float r, uint32 color)
	{
	}

	void CRenderer::DrawEllipse(float x, float y, float rx, float ry, uint32 color, bool AA, float angle)
	{
		ASSERT_HR((AA ? _ellipseAAPass : _ellipsePass)->Apply(0, _immediateContext))
		_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		_immediateContext->IASetInputLayout(_quadLayout);
		_2DVB.Draw(_immediateContext, sizeof(TQuad), 1, CQuadFiller(x, y, rx, ry, 0, angle, color));
	}
}

extern const IDisplayModes &Renderer::HighLevel::DisplayModes::GetDisplayModes()
{
	return DX11::displayModes;
}

extern IRenderer *Renderer::HighLevel::CreateRenderer(HWND hwnd, uint width, uint height, bool fullscreen, uint refreshRate, bool multithreaded)
{
	return new DX11::CRenderer(hwnd, width, height, fullscreen, refreshRate, multithreaded);
}