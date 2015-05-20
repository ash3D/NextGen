/**
\author		Alexey Shaydurov aka ASH
\date		19.5.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "CoreRendererDX9.h"
//#include "ResourceManager.h"
#include <d3dx9.h>

#ifndef NO_BUILTIN_RENDERER

using namespace std;
using WRL::ComPtr;

const WRL::ComPtr<IDirect3D9> d3d(Direct3DCreate9(D3D_SDK_VERSION));

namespace
{
	inline D3DMULTISAMPLE_TYPE Multisample_DGLE_2_D3D(E_MULTISAMPLING_MODE dgleMultisample)
	{
		switch (dgleMultisample)
		{
		case MM_NONE:	return D3DMULTISAMPLE_NONE;
		case MM_2X:		return D3DMULTISAMPLE_2_SAMPLES;
		case MM_4X:		return D3DMULTISAMPLE_4_SAMPLES;
		case MM_8X:		return D3DMULTISAMPLE_8_SAMPLES;
		case MM_16X:	return D3DMULTISAMPLE_16_SAMPLES;
		default:
			assert(false);
			__assume(false);
		}
	}

	D3DSURFACE_DESC FlipRectY(const ComPtr<IDirect3DSurface9> &surface, uint &y, uint height)
	{
		y += height - 1;	// lower-left <-> upper-left

		D3DSURFACE_DESC surface_desc;
		AssertHR(surface->GetDesc(&surface_desc));
		y = surface_desc.Height - y;	// GL <-> D3D

		// rely on NRVO
		return surface_desc;
	}

	inline /*constexpr*/ UINT GetVertexElementStride(BYTE type)
	{
		switch (type)
		{
		case D3DDECLTYPE_FLOAT1:	return sizeof(FLOAT) * 1;
		case D3DDECLTYPE_FLOAT2:	return sizeof(FLOAT) * 2;
		case D3DDECLTYPE_FLOAT3:	return sizeof(FLOAT) * 3;
		case D3DDECLTYPE_FLOAT4:	return sizeof(FLOAT) * 4;
		case D3DDECLTYPE_D3DCOLOR:	return 4;
		case D3DDECLTYPE_UBYTE4:	return sizeof(BYTE) * 4;
		case D3DDECLTYPE_SHORT2:	return sizeof(SHORT) * 2;
		case D3DDECLTYPE_SHORT4:	return sizeof(SHORT) * 4;
		case D3DDECLTYPE_UBYTE4N:	return sizeof(BYTE) * 4;
		case D3DDECLTYPE_SHORT2N:	return sizeof(SHORT) * 2;
		case D3DDECLTYPE_SHORT4N:	return sizeof(SHORT) * 4;
		case D3DDECLTYPE_USHORT2N:	return sizeof(USHORT) * 2;
		case D3DDECLTYPE_USHORT4N:	return sizeof(USHORT) * 4;
		case D3DDECLTYPE_UDEC3:		case D3DDECLTYPE_DEC3N:	return 4;
		case D3DDECLTYPE_FLOAT16_2:	return sizeof(D3DXFLOAT16) * 2;
		case D3DDECLTYPE_FLOAT16_4:	return sizeof(D3DXFLOAT16) * 4;
		default:
			assert(false);
			__assume(false);
		}
	}

	static const/*expr*/ struct
	{
		uint TDrawDataDesc::*offset, TDrawDataDesc::*stride;
		BYTE type;
		BYTE usage;
	} vertexElementLUT[] =
	{
		{ &TDrawDataDesc::uiNormalOffset,			&TDrawDataDesc::uiNormalStride,			D3DDECLTYPE_FLOAT3,	D3DDECLUSAGE_NORMAL		},
		{ &TDrawDataDesc::uiTextureVertexOffset,	&TDrawDataDesc::uiTextureVertexStride,	D3DDECLTYPE_FLOAT2,	D3DDECLUSAGE_TEXCOORD	},
		{ &TDrawDataDesc::uiColorOffset,			&TDrawDataDesc::uiColorStride,			D3DDECLTYPE_FLOAT4,	D3DDECLUSAGE_COLOR		},
		{ &TDrawDataDesc::uiTangentOffset,			&TDrawDataDesc::uiTangentStride,		D3DDECLTYPE_FLOAT3,	D3DDECLUSAGE_TANGENT	},
		{ &TDrawDataDesc::uiBinormalOffset,			&TDrawDataDesc::uiBinormalStride,		D3DDECLTYPE_FLOAT3,	D3DDECLUSAGE_BINORMAL	}
	};

	namespace TexFormatImpl
	{
		// TODO: redesign for constexpr
		typedef uint_fast16_t TPackedLayout;

		// use C++14 constexpr variable template
		template<unsigned int ...layoutIdx>
		class PackedLayout
		{
			template<unsigned int idx, unsigned int ...rest>
			struct PackIdx
			{
				static const/*expr*/ TPackedLayout value = (idx & 7u) << sizeof...(rest) * 3u | PackIdx<rest...>::value;
			};

			template<unsigned int idx>
			struct PackIdx<idx>
			{
				static const/*expr*/ TPackedLayout value = idx & 7u;
			};
		public:
			static const/*expr*/ TPackedLayout value = sizeof...(layoutIdx) << 12u | PackIdx<layoutIdx...>::value;
		};

		template<TPackedLayout packedLayout>
		struct LayoutLength
		{
			static const/*expr*/ auto value = packedLayout >> 12u;
		};

		template<TPackedLayout packedLayout, unsigned idx>
		struct UnpackLayout
		{
			static const/*expr*/ auto value = packedLayout >> (LayoutLength<packedLayout>::value - 1u - idx) * 3u & 7u;
		};

		template<typename ...FormatLayouts>
		class CFormatLayoutArray
		{
			template<unsigned idx, typename, typename ...rest>
			struct atImpl
			{
				typedef typename atImpl<idx - 1, rest...>::type type;
			};

			template<typename FormatLayout, typename ...rest>
			struct atImpl<0, FormatLayout, rest...>
			{
				typedef FormatLayout type;
			};
		public:
			static const/*expr*/ unsigned length = sizeof...(FormatLayouts);

			template<unsigned idx>
			using at = typename atImpl<idx, FormatLayouts...>::type;
		};

		template<typename Format, Format inputFormat, TPackedLayout inputLayout>
		struct TFormatLayout
		{
			static const/*expr*/ Format format = inputFormat;
			static const/*expr*/ TPackedLayout layout = inputLayout;
		};

#		define DECL_FORMAT_LAYOUT(format, ...) TFormatLayout<decltype(format), format, PackedLayout<__VA_ARGS__>::value>

		typedef CFormatLayoutArray
		<
			DECL_FORMAT_LAYOUT(D3DFMT_X8R8G8B8, 2, 1, 0, ~0u),
			DECL_FORMAT_LAYOUT(D3DFMT_X8B8G8R8, 0, 1, 2, ~0u),
			DECL_FORMAT_LAYOUT(D3DFMT_A8B8G8R8, 0, 1, 2, 3),
			DECL_FORMAT_LAYOUT(D3DFMT_A8, 0),
			DECL_FORMAT_LAYOUT(D3DFMT_R8G8B8, 2, 1, 0),
			DECL_FORMAT_LAYOUT(D3DFMT_A8R8G8B8, 2, 1, 0, 3)
		> TD3DFormatLayoutArray;

		typedef CFormatLayoutArray
		<
			DECL_FORMAT_LAYOUT(TDF_RGB8, 0, 1, 2),
			DECL_FORMAT_LAYOUT(TDF_RGBA8, 0, 1, 2, 3),
			DECL_FORMAT_LAYOUT(TDF_ALPHA8, 0),
			DECL_FORMAT_LAYOUT(TDF_BGR8, 2, 1, 0),
			DECL_FORMAT_LAYOUT(TDF_BGRA8, 2, 1, 0, 3)
		> TDGLEFormatLayoutArray;

#		undef DECL_FORMAT_LAYOUT

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned srcIdx = 0, unsigned srcSize = LayoutLength<srcLayout>::value>
		struct FindSrcIdx
		{
			static const/*expr*/ unsigned value = UnpackLayout<srcLayout, srcIdx>::value == UnpackLayout<dstLayout, dstIdx>::value ? srcIdx : FindSrcIdx<srcLayout, dstLayout, dstIdx, srcIdx + 1>::value;
		};

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned srcSize>
		struct FindSrcIdx<srcLayout, dstLayout, dstIdx, srcSize, srcSize>
		{
			static const/*expr*/ unsigned value = ~0u;
		};

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx = 0, unsigned dstSize = LayoutLength<dstLayout>::value>
		struct FillTexel
		{
			template<class TSource, class TDest>
			static inline void apply(TSource &source, TDest &dest);
		};

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned dstSize>
		template<class TSource, class TDest>
		inline void FillTexel<srcLayout, dstLayout, dstIdx, dstSize>::apply(TSource &source, TDest &dest)
		{
			if (UnpackLayout<dstLayout, dstIdx>::value != 7u)
			{
				const/*expr*/ unsigned src_idx = FindSrcIdx<srcLayout, dstLayout, dstIdx>::value;
				dest[dstIdx] = src_idx == ~0u ? ~0u : source[src_idx];
			}
			FillTexel<srcLayout, dstLayout, dstIdx + 1, dstSize>::apply(source, dest);
		}

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstSize>
		struct FillTexel<srcLayout, dstLayout, dstSize, dstSize>
		{
			template<class TSource, class TDest>
			static inline void apply(TSource &source, TDest &dest) {}
		};

		template<TPackedLayout srcLayout, TPackedLayout dstLayout>
		inline void TransformRow(const void *const src, void *const dst, unsigned width)
		{
			const/*expr*/ auto src_len = LayoutLength<srcLayout>::value, dst_len = LayoutLength<dstLayout>::value;
			static_assert(src_len, "zero SrcLayout");
			static_assert(dst_len, "zero DstLayout");
			typedef const array<uint8_t, src_len> TSource;
			typedef array<uint8_t, dst_len> TDest;
			const auto source = static_cast<TSource *const>(src);
			const auto dest = static_cast<TDest *const>(dst);
			transform(source, source + width, dest, [](TSource &source)
			{
				register TDest dest;
				FillTexel<srcLayout, dstLayout>::apply(source, dest);
				return dest;	// rely on NRVO
			});
		}

		void CopyRow(const void *const src, void *const dst, unsigned length)
		{
			memcpy(dst, src, length);
		}

		template<TPackedLayout dgleFormatLayout, TPackedLayout d3dFormatLayout>
		void (*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		{
			return dgle2d3d ? TransformRow<dgleFormatLayout, d3dFormatLayout> : TransformRow<d3dFormatLayout, dgleFormatLayout>;
		}

		template<TPackedLayout dgleFormatLayout, TPackedLayout d3dFormatLayout>
		struct GetRowConvertion
		{
			static inline void (*(*apply())(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return RowConvertion<dgleFormatLayout, d3dFormatLayout>;
			};
		};

		template<TPackedLayout formatLayuot>
		struct GetRowConvertion<formatLayuot, formatLayuot>
		{
			static inline void (*(*apply())(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return RowCopy;
			};
		};

		template<unsigned idx = 0>
		struct IterateD3DRowConvertion
		{
			template<TPackedLayout dgleFormatLayout>
			static inline void (*(*apply(D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				typedef typename TD3DFormatLayoutArray::at<idx> TCurFormatLayout;
				return d3dFormat == TCurFormatLayout::format ? GetRowConvertion<dgleFormatLayout, TCurFormatLayout::layout>::apply() : IterateD3DRowConvertion<idx + 1>::apply<dgleFormatLayout>(d3dFormat);
			}
		};

		template<>
		struct IterateD3DRowConvertion<TD3DFormatLayoutArray::length>
		{
			template<TPackedLayout dgleFormatLayout>
			static inline void (*(*apply(D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return nullptr;
			}
		};

		template<unsigned idx = 0>
		inline void (*(*IterateDGLERowConvertion(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		{
			typedef typename TDGLEFormatLayoutArray::at<idx> TCurFormatLayout;
			return dgleFormat == TCurFormatLayout::format ? IterateD3DRowConvertion<>::apply<TCurFormatLayout::layout>(d3dFormat) : IterateDGLERowConvertion<idx + 1>(dgleFormat, d3dFormat);
		}

		/*
			workaround for VS 2013 bug (auto ... ->)
			TODO: try with newer version
		*/
		template<>
		//inline void (*(*IterateDGLERowConvertion<TDGLEFormatLayoutArray::length>(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		inline auto IterateDGLERowConvertion<TDGLEFormatLayoutArray::length>(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat) -> void (*(*)(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		{
			return nullptr;
		}

#if 0
		// TODO: use C++14 constexpr variable template
		struct BGRXFormatLayout
		{
			static const/*expr*/ unsigned value[];
		};

		const unsigned BGRXFormatLayout::value[] = { 2, 1, 0, ~0u };

		template<E_TEXTURE_DATA_FORMAT format>
		struct FormatLayout
		{
			static const/*expr*/ unsigned value[];
		};

#		define DECL_FORMAT_LAYOUT(format, ...)	\
			template<>							\
			const unsigned FormatLayout<format>::value[] = { __VA_ARGS__ };

		DECL_FORMAT_LAYOUT(TDF_RGB8, 0, 1, 2)
		DECL_FORMAT_LAYOUT(TDF_BGR8, 2, 1, 0)
		//DECL_FORMAT_LAYOUT(TDF_RGBA8, 0, 1, 2, 3)
#		undef DECL_FORMAT_TRAITS

		template<class SrcLayout, class DstLayout, unsigned dstIdx, unsigned srcIdx = 0, unsigned srcSize = extent<decltype(SrcLayout::value)>::value>
		struct FindSrcIdx
		{
			//static const/*expr*/ unsigned value = SrcLayout::value[srcIdx] == DstLayout::value[dstIdx] ? srcIdx : FindSrcIdx<SrcLayout, DstLayout, dstIdx, srcIdx + 1>::value;
			static inline unsigned value()
			{
				return SrcLayout::value[srcIdx] == DstLayout::value[dstIdx] ? srcIdx : FindSrcIdx<SrcLayout, DstLayout, dstIdx, srcIdx + 1>::value();
			}
		};

		template<class SrcLayout, class DstLayout, unsigned dstIdx, unsigned srcSize>
		struct FindSrcIdx<SrcLayout, DstLayout, dstIdx, srcSize, srcSize>
		{
			//static const/*expr*/ unsigned value = ~0u;
			static inline unsigned value()
			{
				return ~0u;
			}
		};

		template<class SrcLayout, class DstLayout, unsigned dstIdx = 0, unsigned dstSize = extent<decltype(DstLayout::value)>::value>
		struct FillTexel
		{
			template<class TSource, class TDest>
			static inline void apply(TSource &source, TDest &dest);
		};

		template<class SrcLayout, class DstLayout, unsigned dstIdx, unsigned dstSize>
		template<class TSource, class TDest>
		inline void FillTexel<SrcLayout, DstLayout, dstIdx, dstSize>::apply(TSource &source, TDest &dest)
		{
			if (DstLayout::value[dstIdx] != ~0u)
			{
				const/*expr*/ unsigned src_idx = FindSrcIdx<SrcLayout, DstLayout, dstIdx>::value();
				dest[dstIdx] = src_idx == ~0u ? ~0u : source[src_idx];
			}
			FillTexel<SrcLayout, DstLayout, dstIdx + 1, dstSize>::apply(source, dest);
		}

		template<class SrcLayout, class DstLayout, unsigned dstSize>
		struct FillTexel<SrcLayout, DstLayout, dstSize, dstSize>
		{
			template<class TSource, class TDest>
			static inline void apply(TSource &source, TDest &dest) {}
		};

		template<class SrcLayout, class DstLayout>
		inline void TransformRow(const void *const src, void *const dst, unsigned width)
		{
			const/*expr*/ auto src_len = extent<decltype(SrcLayout::value)>::value, dst_len = extent<decltype(DstLayout::value)>::value;
			static_assert(src_len, "zero SrcLayout");
			static_assert(dst_len, "zero DstLayout");
			typedef const array<uint8_t, src_len> TSource;
			typedef array<uint8_t, dst_len> TDest;
			const auto source = static_cast<TSource *const>(src);
			const auto dest = static_cast<TDest *const>(dst);
			transform(source, source + width, dest, [](TSource &source)
			{
				register TDest dest;
				FillTexel<SrcLayout, DstLayout>::apply(source, dest);
				return dest;	// rely on NRVO
			});
		}
#endif
	}

	void (*RowCopy(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
	{
		return TexFormatImpl::CopyRow;
	}

	inline void (*(*GetRowConvertion(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
	{
		return TexFormatImpl::IterateDGLERowConvertion(dgleFormat, d3dFormat);
	}
#if 0
	template<E_TEXTURE_DATA_FORMAT format>
	void WriteRow(const void *const src, void *const dst, unsigned width)
	{
		using namespace TexFormatImpl;
		TransformRow<FormatLayout<format>, BGRXFormatLayout>(src, dst, width);
	}

	template<E_TEXTURE_DATA_FORMAT format>
	void ReadRow(const void *const src, void *const dst, unsigned width)
	{
		using namespace TexFormatImpl;
		TransformRow<BGRXFormatLayout, FormatLayout<format>>(src, dst, width);
	}

	template<E_TEXTURE_DATA_FORMAT format>
	void (*(GetRowConvertion)(bool read))(const void *const src, void *const dst, unsigned length)
	{
		return read ? ReadRow<format> : WriteRow<format>;
	}

	void CopyRow(const void *const src, void *const dst, unsigned length)
	{
		memcpy(dst, src, length);
	}

	void (*(GetRowConvertion)(bool read))(const void *const src, void *const dst, unsigned length)
	{
		return CopyRow;
	}
#endif

	bool TexFormatSupported(const ComPtr<IDirect3DDevice9> &device, D3DFORMAT format)
	{
		D3DDISPLAYMODE mode;
		AssertHR(device->GetDisplayMode(0, &mode));
		return SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format, 0, D3DRTYPE_TEXTURE, format));
	}
}

#pragma region CDX9BufferContainer
class CDX9BufferContainer
#ifdef DX9_LEGACY_BASE_OBJECTS
	: public IDX9BufferContainer
#endif
{
protected:
	ComPtr<IDirect3DVertexBuffer9> _VB;
	ComPtr<IDirect3DIndexBuffer9> _IB;
	ComPtr<IDirect3DVertexDeclaration9> _VBDecl;

protected:
	CDX9BufferContainer(const ComPtr<IDirect3DVertexBuffer9> &VB, const ComPtr<IDirect3DIndexBuffer9> &IB, const ComPtr<IDirect3DVertexDeclaration9> &VBDecl) :
		_VB(VB), _IB(IB), _VBDecl(VBDecl) {}

	CDX9BufferContainer(CDX9BufferContainer &) = delete;
	void operator =(CDX9BufferContainer &) = delete;

public:
	DGLE_RESULT DGLE_API GetVB(IDirect3DVertexBuffer9 *&VB)
	{
		_VB.CopyTo(&VB);
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetIB(IDirect3DIndexBuffer9 *&IB)
	{
		_IB.CopyTo(&IB);
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetVBDecl(IDirect3DVertexDeclaration9 *&VBDecl)
	{
		_VBDecl.CopyTo(&VBDecl);
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetObjectType(E_ENGINE_OBJECT_TYPE &eType)
	{
		eType = EOT_MESH;
		return S_OK;
	}

#ifdef DX9_LEGACY_BASE_OBJECTS
	IDGLE_BASE_IMPLEMENTATION(IDX9BufferContainer, INTERFACE_IMPL(IBaseRenderObjectContainer, INTERFACE_IMPL_END))
#endif
};
#pragma endregion

namespace
{
	class CCoreGeometryBuffer : public ICoreGeometryBuffer, public CDX9BufferContainer
	{
		TDrawDataDesc _drawDataDesc;
		uint _verticesDataSize, _indicesDataSize,
			_verticesCount, _indicesCount;
		E_CORE_RENDERER_DRAW_MODE _drawMode;
		E_CORE_RENDERER_BUFFER_TYPE _bufferType;

	public:
		CCoreGeometryBuffer(const ComPtr<IDirect3DVertexBuffer9> &VB, const ComPtr<IDirect3DIndexBuffer9> &IB, const ComPtr<IDirect3DVertexDeclaration9> &VBDecl,
			const TDrawDataDesc &drawDesc, uint verDatSize, uint idxDatSize, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode, E_CORE_RENDERER_BUFFER_TYPE type) :
			CDX9BufferContainer(VB, IB, VBDecl), _drawDataDesc(drawDesc), _verticesDataSize(verDatSize),
			_indicesDataSize(idxDatSize), _verticesCount(verCnt), _indicesCount(idxCnt), _drawMode(mode), _bufferType(type)
		{}

		CCoreGeometryBuffer(CCoreGeometryBuffer &) = delete;
		void operator =(CCoreGeometryBuffer &) = delete;

		~CCoreGeometryBuffer()
		{
			delete[] _drawDataDesc.pData;
			delete[] _drawDataDesc.pIndexBuffer;
		}

	public:
		inline const ComPtr<IDirect3DVertexBuffer9> &GetVB() const { return _VB; }
		inline const ComPtr<IDirect3DIndexBuffer9> &GetIB() const { return _IB; }
		inline const ComPtr<IDirect3DVertexDeclaration9> &GetVBDecl() const { return _VBDecl; }
		inline E_CORE_RENDERER_DRAW_MODE GetDrawMode() const { return _drawMode; }
		inline uint GetVerticesCount() const { return _verticesCount; }
		inline uint GetIndicesCount() const { return _indicesCount; }
		inline const TDrawDataDesc &GetDrawDesc() const { return _drawDataDesc; }

	public:
		DGLE_RESULT DGLE_API GetGeometryData(TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize) override;

		DGLE_RESULT DGLE_API SetGeometryData(const TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize) override
		{
			if (uiVerticesDataSize != _verticesDataSize || uiIndexesDataSize != _indicesDataSize)
				return E_INVALIDARG;

			return Reallocate(stDesc, _verticesCount, _indicesCount, _drawMode);
		}

		DGLE_RESULT DGLE_API Reallocate(const TDrawDataDesc &stDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode) override;

		DGLE_RESULT DGLE_API GetBufferDimensions(uint &uiVerticesDataSize, uint &uiVerticesCount, uint &uiIndexesDataSize, uint &uiIndicesCount) override
		{
			uiVerticesDataSize = _verticesDataSize;
			uiVerticesCount = _verticesCount;
			uiIndexesDataSize = _indicesDataSize;
			uiIndicesCount = _indicesCount;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetBufferDrawDataDesc(TDrawDataDesc &stDesc) override
		{
			stDesc = _drawDataDesc;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetBufferDrawMode(E_CORE_RENDERER_DRAW_MODE &eMode) override
		{
			eMode = _drawMode;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetBufferType(E_CORE_RENDERER_BUFFER_TYPE &eType) override
		{
			eType = _bufferType;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetBaseObject(IBaseRenderObjectContainer *&prObj) override
		{
#ifdef DX9_LEGACY_BASE_OBJECTS
			prObj = this;
			return S_OK;
#else
			prObj = nullptr;
			return S_FALSE;
#endif
		}

		DGLE_RESULT DGLE_API Free() override
		{
			delete this;
			return S_OK;
		}

		IDGLE_BASE_IMPLEMENTATION(ICoreGeometryBuffer, INTERFACE_IMPL_END)
	};

	DGLE_RESULT DGLE_API CCoreGeometryBuffer::GetGeometryData(TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize)
	{
		if (uiVerticesDataSize != _verticesDataSize || uiIndexesDataSize != _indicesDataSize)
			return E_INVALIDARG;

		uint8 *data = stDesc.pData, *idx = stDesc.pIndexBuffer;
		stDesc = _drawDataDesc;
		stDesc.pData = data;
		stDesc.pIndexBuffer = idx;

		if (_bufferType == CRBT_SOFTWARE)
		{
			memcpy(stDesc.pData, _drawDataDesc.pData, _verticesDataSize);

			if (_indicesDataSize != 0)
				memcpy(stDesc.pIndexBuffer, _drawDataDesc.pIndexBuffer, _indicesDataSize);
		}
		else
		{
			AssertHR(_VB->Lock(0, _verticesDataSize, (void **)&data, D3DLOCK_READONLY));
			memcpy(stDesc.pData, data, _verticesDataSize);
			AssertHR(_VB->Unlock());

			if (_IB)
			{
				AssertHR(_IB->Lock(0, _verticesDataSize, (void **)&idx, D3DLOCK_READONLY));
				memcpy(stDesc.pIndexBuffer, idx, _indicesDataSize);
				AssertHR(_IB->Unlock());
			}
		}

		return S_OK;
	}

	DGLE_RESULT DGLE_API CCoreGeometryBuffer::Reallocate(const TDrawDataDesc &stDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode)
	{
		const uint vertices_data_size = uiVerticesCount * CCoreRendererDX9::GetVertexSize(stDesc),
			indices_data_size = uiIndicesCount * (stDesc.bIndexBuffer32 ? sizeof(uint32) : sizeof(uint16)) * (stDesc.pIndexBuffer ? 3 : 0);

		if (_indicesDataSize == 0 && indices_data_size != 0)
			return E_INVALIDARG;

		_verticesCount = uiVerticesCount;
		_drawMode = eMode;

		uint8 *data = _drawDataDesc.pData, *idx = _drawDataDesc.pIndexBuffer;
		_drawDataDesc = stDesc;
		_drawDataDesc.pData = data;
		_drawDataDesc.pIndexBuffer = idx;

		if (_bufferType == CRBT_SOFTWARE)
		{
			if (_verticesDataSize != vertices_data_size)
			{
				_verticesDataSize = vertices_data_size;
				delete[] _drawDataDesc.pData;
				_drawDataDesc.pData = new uint8[_verticesDataSize];
			}

			memcpy(_drawDataDesc.pData, stDesc.pData, _verticesDataSize);

			if (_indicesDataSize != 0)
			{
				_indicesCount = uiIndicesCount;

				if (_indicesDataSize != indices_data_size)
				{
					_indicesDataSize = indices_data_size;
					delete[] _drawDataDesc.pIndexBuffer;
					_drawDataDesc.pIndexBuffer = new uint8[_indicesDataSize];
				}

				memcpy(_drawDataDesc.pIndexBuffer, stDesc.pIndexBuffer, _indicesDataSize);
			}
		}
		else
		{
			const DWORD lock_flags = _bufferType == CRBT_HARDWARE_STATIC ? 0 : D3DLOCK_DISCARD;

			if (_verticesDataSize != vertices_data_size)
			{
				D3DVERTEXBUFFER_DESC VB_desc;
				AssertHR(_VB->GetDesc(&VB_desc));
				if (VB_desc.Size < vertices_data_size)
				{
					ComPtr<IDirect3DDevice9> device;
					AssertHR(_VB->GetDevice(&device));
					AssertHR(device->CreateVertexBuffer(vertices_data_size, VB_desc.Usage, VB_desc.FVF, VB_desc.Pool, &_VB, NULL));
				}
				_verticesDataSize = vertices_data_size;
			}

			AssertHR(_VB->Lock(0, _verticesDataSize, (void **)&data, lock_flags));
			memcpy(data, stDesc.pData, _verticesDataSize);
			AssertHR(_VB->Unlock());

			if (_IB)
			{
				if (_indicesDataSize != indices_data_size)
				{
					D3DINDEXBUFFER_DESC IB_desc;
					AssertHR(_IB->GetDesc(&IB_desc));
					if (IB_desc.Size < indices_data_size)
					{
						ComPtr<IDirect3DDevice9> device;
						AssertHR(_IB->GetDevice(&device));
						AssertHR(device->CreateIndexBuffer(indices_data_size, IB_desc.Usage, stDesc.bIndexBuffer32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, IB_desc.Pool, &_IB, NULL));
					}
					_indicesDataSize = indices_data_size;
				}

				AssertHR(_IB->Lock(0, _indicesDataSize, (void **)&idx, lock_flags));
				memcpy(idx, stDesc.pIndexBuffer, _indicesDataSize);
				AssertHR(_IB->Unlock());
			}
		}

		return S_OK;
	}
}

#pragma region CDX9Texture
class CDX9TextureContainer
#ifdef DX9_LEGACY_BASE_OBJECTS
	: public IDX9TextureContainer
#endif
{
protected:
	ComPtr<IDirect3DTexture9> _texture;

protected:
	CDX9TextureContainer(const ComPtr<IDirect3DTexture9> &texture) : _texture(texture) {}
	CDX9TextureContainer(CDX9TextureContainer &) = delete;
	void operator =(CDX9TextureContainer &) = delete;

public:
	DGLE_RESULT DGLE_API GetObjectType(E_ENGINE_OBJECT_TYPE &eType)
	{
		eType = EOT_TEXTURE;
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetTexture(IDirect3DTexture9 *&texture)
	{
		_texture.CopyTo(&texture);
		return S_OK;
	}

#ifdef DX9_LEGACY_BASE_OBJECTS
	IDGLE_BASE_IMPLEMENTATION(IOpenGLTextureContainer, INTERFACE_IMPL(IBaseRenderObjectContainer, INTERFACE_IMPL_END))
#endif
};
#pragma endregion

namespace
{
	class __declspec(uuid("{356F5347-3EF8-40C5-84A4-817993A23196}")) CCoreTexture final : public ICoreTexture, public CDX9TextureContainer
	{
		CCoreRendererDX9 &_parent;
		const E_TEXTURE_TYPE _type;
		const E_TEXTURE_DATA_FORMAT _format;
		const E_TEXTURE_LOAD_FLAGS _loadFlags;
		void (*(*const _RowConvertion)(bool dgle2d3d))(const void *const src, void *const dst, unsigned length);
		uint _bytesPerPixel;	// per block for compressed formats

	public:
		const D3DTEXTUREFILTERTYPE magFilter, minFilter, mipFilter;
		const D3DTEXTUREADDRESS addressMode;
		const DWORD anisoLevel;

	private:
		const bool _mipMaps;

	private:
		inline bool _Compressed() const;

		struct TDataSize
		{
			uint lw, lh, rowSize, size;
		} _DataSize(uint lod) const;

	public:
		CCoreTexture(
			CCoreRendererDX9 &parent, const ComPtr<IDirect3DTexture9> texture, E_TEXTURE_TYPE type, E_TEXTURE_DATA_FORMAT format, E_TEXTURE_LOAD_FLAGS loadFlags,
			void (*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length), uint bytesPerPixel,
			bool mipMaps, DWORD anisoLevel, DWORD addressCaps, DGLE_RESULT &ret);
		CCoreTexture(CCoreTexture &) = delete;
		void operator =(CCoreTexture &) = delete;

		inline const ComPtr<IDirect3DTexture9> &GetTex() const { return _texture; }

		DGLE_RESULT DGLE_API GetSize(uint &width, uint &height) override
		{
			D3DSURFACE_DESC desc;
			AssertHR(_texture->GetLevelDesc(0, &desc));
			width = desc.Width; height = desc.Height;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetDepth(uint &depth) override
		{
			depth = 0;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetType(E_TEXTURE_TYPE &eType) override
		{
			eType = _type;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetFormat(E_TEXTURE_DATA_FORMAT &eFormat) override
		{
			eFormat = _format;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetLoadFlags(E_TEXTURE_LOAD_FLAGS &eLoadFlags) override
		{
			eLoadFlags = _loadFlags;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetPixelData(uint8 *pData, uint &uiDataSize, uint uiLodLevel) override;

		DGLE_RESULT DGLE_API SetPixelData(const uint8 *pData, uint uiDataSize, uint uiLodLevel) override;

		DGLE_RESULT DGLE_API Reallocate(const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipMaps, E_TEXTURE_DATA_FORMAT eDataFormat) override;

		DGLE_RESULT DGLE_API GetBaseObject(IBaseRenderObjectContainer *&prObj) override
		{
#ifdef DX9_LEGACY_BASE_OBJECTS
			prObj = this;
			return S_OK;
#else
			prObj = nullptr;
			return S_FALSE;
#endif
		}

		DGLE_RESULT DGLE_API Free()
		{
			delete this;
			return S_OK;
		}

		IDGLE_BASE_IMPLEMENTATION(ICoreTexture, INTERFACE_IMPL_END)
	};

	inline bool CCoreTexture::_Compressed() const
	{
		switch (_format)
		{
		case TDF_DXT1:
		case TDF_DXT5:
			return true;
		default:
			return false;
		}
	}

	auto CCoreTexture::_DataSize(uint lod) const -> TDataSize
	{
		D3DSURFACE_DESC desc;
		AssertHR(_texture->GetLevelDesc(lod, &desc));
		if (_Compressed())
			(desc.Width += 3) /= 4, (desc.Height += 3) /= 4;

		const TDataSize result = { desc.Width, desc.Height, result.lw * _bytesPerPixel, result.rowSize * result.lh };
		return result;
	}

	CCoreTexture::CCoreTexture(
		CCoreRendererDX9 &parent, const ComPtr<IDirect3DTexture9> texture, E_TEXTURE_TYPE type, E_TEXTURE_DATA_FORMAT format, E_TEXTURE_LOAD_FLAGS loadFlags,
		void (*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length), uint bytesPerPixel,
		bool mipMaps, DWORD anisoLevel, DWORD addressCaps, DGLE_RESULT &ret) :
		_parent(parent), CDX9TextureContainer(texture), _type(type), _format(format), _loadFlags(loadFlags),
		_RowConvertion(RowConvertion), _bytesPerPixel(bytesPerPixel), _mipMaps(mipMaps), anisoLevel(anisoLevel),
		magFilter(loadFlags & TLF_FILTERING_NONE ? D3DTEXF_POINT : D3DTEXF_LINEAR),
		minFilter(loadFlags & TLF_FILTERING_NONE ? D3DTEXF_POINT : loadFlags & TLF_FILTERING_ANISOTROPIC ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR),
		mipFilter(mipMaps ? loadFlags & (TLF_FILTERING_NONE | TLF_FILTERING_BILINEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR : D3DTEXF_NONE),
		addressMode(loadFlags & TLF_COORDS_CLAMP ? D3DTADDRESS_CLAMP : loadFlags & TLF_COORDS_MIRROR_REPEAT ? D3DTADDRESS_MIRROR : loadFlags & TLF_COORDS_MIRROR_CLAMP ? D3DTADDRESS_MIRRORONCE : D3DTADDRESS_WRAP)
	{
		DWORD flag;
		switch (addressMode)
		{
		case D3DTADDRESS_WRAP:
			flag = D3DPTADDRESSCAPS_WRAP;
			break;
		case D3DTADDRESS_MIRROR:
			flag = D3DPTADDRESSCAPS_MIRROR;
			break;
		case D3DTADDRESS_CLAMP:
			flag = D3DPTADDRESSCAPS_CLAMP;
			break;
		case D3DTADDRESS_MIRRORONCE:
			flag = D3DPTADDRESSCAPS_MIRRORONCE;
			break;
		default:
			assert(false);
			__assume(false);
		}

		if (!(addressCaps & flag))
			ret = S_FALSE;

		AssertHR(_texture->SetPrivateData(__uuidof(CCoreTexture), this, sizeof this, 0));
	}

	DGLE_RESULT DGLE_API CCoreTexture::GetPixelData(uint8 *pData, uint &uiDataSize, uint uiLodLevel)
	{
		ICoreTexture *curRT;
		_parent.GetRenderTarget(curRT);
		if (curRT == this)
			return E_ABORT;

		if (!_mipMaps && uiLodLevel != 0)
			return E_INVALIDARG;

		auto data_size = _DataSize(uiLodLevel);

		if (!pData || data_size.size != uiDataSize)
		{
			uiDataSize = data_size.size;
			return S_FALSE;
		}

		const auto ReadRow = _RowConvertion(false);
		if (_RowConvertion == RowCopy)
			data_size.lw = data_size.rowSize;

		D3DLOCKED_RECT locked;
		AssertHR(_texture->LockRect(uiLodLevel, &locked, NULL, D3DLOCK_READONLY));
		for (uint row = 0; row < data_size.lh; row++, pData += data_size.rowSize, (uint8_t *&)locked.pBits += locked.Pitch)
			ReadRow(locked.pBits, pData, data_size.lw);
		AssertHR(_texture->UnlockRect(uiLodLevel));

		return S_OK;
	}

	DGLE_RESULT DGLE_API CCoreTexture::SetPixelData(const uint8 *pData, uint uiDataSize, uint uiLodLevel)
	{
		ICoreTexture *curRT;
		_parent.GetRenderTarget(curRT);
		if (curRT == this)
			return E_ABORT;

		if (!_mipMaps && uiLodLevel != 0)
			return E_INVALIDARG;

		auto data_size = _DataSize(uiLodLevel);

		if (data_size.size != uiDataSize)
			return E_INVALIDARG;

		const auto WriteRow = _RowConvertion(true);
		if (_RowConvertion == RowCopy)
			data_size.lw = data_size.rowSize;

		D3DLOCKED_RECT locked;
		AssertHR(_texture->LockRect(uiLodLevel, &locked, NULL, 0));
		for (uint row = 0; row < data_size.lh; row++, pData += data_size.rowSize, (const uint8_t *&)locked.pBits += locked.Pitch)
			WriteRow(pData, locked.pBits, data_size.lw);
		AssertHR(_texture->UnlockRect(uiLodLevel));

		return S_OK;
	}

	DGLE_RESULT DGLE_API CCoreTexture::Reallocate(const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipMaps, E_TEXTURE_DATA_FORMAT eDataFormat)
	{
		if (uiWidth == 0 || uiHeight == 0 || !_parent.GetNSQTexSupport() && uiWidth != uiHeight)
			return E_INVALIDARG;

		if (!pData || eDataFormat != _format)
			return E_INVALIDARG;

		const bool non_power_of_two = __popcnt(uiWidth) != 1 || __popcnt(uiHeight) != 1;
		if (uiWidth > _parent.GetMaxTextureWidth() || uiHeight > _parent.GetMaxTextureHeight() || !_parent.GetNPOTTexSupport() && non_power_of_two)
			return E_INVALIDARG;

		// DX does not support compressed textures with top mip level not multiple block size
		if (_Compressed() && uiWidth % 4 && uiHeight % 4)
			return E_INVALIDARG;

		DGLE_RESULT ret = S_OK;
		
		if (!_parent.GetMipmapSupport() && bMipMaps)
		{
			bMipMaps = false;
			ret = S_FALSE;
		}

		ComPtr<IDirect3DDevice9> device;
		AssertHR(_texture->GetDevice(&device));
		D3DSURFACE_DESC desc;
		AssertHR(_texture->GetLevelDesc(0, &desc));
		switch (device->CreateTexture(uiWidth, uiHeight, _mipMaps || bMipMaps ? 0 : 1, 0, desc.Format, D3DPOOL_MANAGED, &_texture, NULL))
		{
		case S_OK:					break;
		case D3DERR_INVALIDCALL:	return E_INVALIDARG;
		default:					return E_ABORT;
		}
		AssertHR(_texture->SetPrivateData(__uuidof(CCoreTexture), this, sizeof this, 0));

		uint lod = 0;
		do
		{
			if (!uiWidth) uiWidth = 1;
			if (!uiHeight) uiHeight = 1;

			if (FAILED(ret = SetPixelData(pData, _DataSize(lod).size, lod)))
				return E_ABORT;

			lod++;
		} while ((uiWidth /= 2) && (uiHeight /= 2) && bMipMaps);

		if (_mipMaps && !bMipMaps)
		{
			if (FAILED(D3DXFilterTexture(_texture.Get(), NULL, D3DX_DEFAULT, D3DX_DEFAULT)))
				ret = S_FALSE;
		}

		return ret;
	}
}

#pragma region CCoreRendererDX9
CCoreRendererDX9::CCoreRendererDX9(IEngineCore &engineCore) :
_engineCore(engineCore), _stInitResults(false),
_depthPool(CSurfacePool::E_TYPE::DEPTH),
_colorPool(CSurfacePool::E_TYPE::COLOR_LOCKABLE),
_MSAAcolorPool(CSurfacePool::E_TYPE::COLOR)
{
	assert(d3d);
}

inline uint CCoreRendererDX9::GetVertexSize(const TDrawDataDesc &stDesc)
{
	uint res = sizeof(float) * ((stDesc.bVertices2D ? 2 : 3) + (stDesc.uiNormalOffset != -1 ? 3 : 0) + (stDesc.uiTextureVertexOffset != -1 ? 2 : 0) +
		(stDesc.uiColorOffset != -1 ? 4 : 0) + (stDesc.uiTangentOffset != -1 ? 3 : 0) + (stDesc.uiBinormalOffset != -1 ? 3 : 0));

	if (stDesc.pAttribs)
	{
		/* not implemented */
	}

	return res;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Prepare(TCrRndrInitResults &stResults)
{
	return stResults = true, stResults ? S_OK : E_FAIL;
}

// rely on NRVO here
D3DPRESENT_PARAMETERS CCoreRendererDX9::_GetPresentParams(TEngineWindow &wnd) const
{
	TWindowHandle hwnd;
	AssertHR(_engineCore.GetWindowHandle(hwnd));
	D3DPRESENT_PARAMETERS present_params =
	{
		wnd.uiWidth,									// width
		wnd.uiHeight,									// heigth
		_16bitColor ? D3DFMT_R5G6B5 : D3DFMT_A8R8G8B8,	// format
		0,												// back buffer count, use default
		Multisample_DGLE_2_D3D(wnd.eMultisampling),		// MSAA
		0,												// MSAA quality
		D3DSWAPEFFECT_DISCARD,							// swap effect
		hwnd,											// device window (used for Present)
		!wnd.bFullScreen,								// is windowed
		TRUE,											// enable auto depth stencil
		D3DFMT_D24S8,									// auto depth stencil format
		0,												// flags
		0,												// refresh rate
		D3DPRESENT_INTERVAL_DEFAULT						// presentaion interval
	};
	if (FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present_params.BackBufferFormat, present_params.Windowed, present_params.MultiSampleType, NULL)) ||
		FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present_params.AutoDepthStencilFormat, present_params.Windowed, present_params.MultiSampleType, NULL)))
	{
		wnd.eMultisampling = MM_NONE;
		present_params.MultiSampleType = D3DMULTISAMPLE_NONE;
	}
	if (present_params.MultiSampleType == D3DMULTISAMPLE_NONE)
		present_params.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	return present_params;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Initialize(TCrRndrInitResults &stResults, TEngineWindow &stWin, E_ENGINE_INIT_FLAGS &eInitFlags)
{
	if (_stInitResults)
		return E_ABORT;

	_16bitColor = eInitFlags & EIF_FORCE_16_BIT_COLOR;

	LOG("Initializing Core Renderer...", LT_INFO);

	D3DPRESENT_PARAMETERS present_params = _GetPresentParams(stWin);
	if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present_params.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_params, &_device)))
	{
		AssertHR(Finalize());
		LOG("Can't create Direct3D Device.", LT_FATAL);
		stResults = false;
		return E_ABORT;
	}
	AssertHR(_device->BeginScene());

	D3DCAPS9 caps;
	AssertHR(_device->GetDeviceCaps(&caps));
	_maxTexResolution[0] = caps.MaxTextureWidth;
	_maxTexResolution[1] = caps.MaxTextureHeight;
	_maxAnisotropy = caps.MaxAnisotropy;
	_maxTexUnits = caps.MaxSimultaneousTextures;
	_maxTexStages = caps.MaxTextureBlendStages;
	_maxRTs = caps.NumSimultaneousRTs;
	_maxVertexStreams = caps.MaxStreams;
	_maxClipPlanes = caps.MaxUserClipPlanes;
	_maxVSFloatConsts = caps.MaxVertexShaderConst;
	_textureAddressCaps = caps.TextureAddressCaps;
	_NPOTTexSupport = !(caps.TextureCaps & D3DPTEXTURECAPS_POW2 || caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);
	_NSQTexSupport = !(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY);
	_mipmapSupport = caps.TextureCaps & D3DPTEXTURECAPS_MIPMAP;
	_anisoSupport = caps.RasterCaps & D3DPRASTERCAPS_ANISOTROPY;

	_FFP = new CFixedFunctionPipelineDX9(_device);

	AssertHR(BindTexture(NULL, 0));
	AssertHR(SetBlendState(TBlendStateDesc()));
	AssertHR(SetDepthStencilState(TDepthStencilDesc()));
	AssertHR(SetRasterizerState(TRasterizerStateDesc()));

	AssertHR(SetMatrix(MatrixIdentity(), MT_TEXTURE));
	AssertHR(SetMatrix(MatrixIdentity(), MT_PROJECTION));
	AssertHR(SetMatrix(MatrixIdentity(), MT_MODELVIEW));

	AssertHR(_engineCore.AddEventListener(ET_ON_PER_SECOND_TIMER, EventsHandler, this));

	_stInitResults = stResults;

	LOG("Core Renderer initialized.", LT_INFO);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Finalize()
{
	if (!_stInitResults)
		return E_ABORT;

	_engineCore.RemoveEventListener(ET_ON_PER_SECOND_TIMER, EventsHandler, this);

	delete _FFP;

	LOG("Core Renderer finalized.", LT_INFO);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::AdjustMode(TEngineWindow &stNewWin)
{
	_depthPool.Clear();
	_colorPool.Clear();
	_MSAAcolorPool.Clear();
	_offcreenDepth.Clear();
	_screenColorTarget.Reset();
	_screenDepthTarget.Reset();
	_PushStates();
	D3DPRESENT_PARAMETERS present_params = _GetPresentParams(stNewWin);
	switch (_device->Reset(&present_params))
	{
	case S_OK:
		break;
		// TODO: handle lost device here
	case D3DERR_DEVICELOST:
	default:
		assert(false);
	}
	AssertHR(_device->BeginScene());
	_PopStates();
	AssertHR(_device->GetRenderTarget(0, &_screenColorTarget));
	AssertHR(_device->GetDepthStencilSurface(&_screenDepthTarget));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::MakeCurrent()
{
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetClearColor(const TColor4 &stColor)
{
	_clearColor = stColor;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetClearColor(TColor4 &stColor)
{
	stColor = _clearColor;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Clear(bool bColor, bool bDepth, bool bStencil)
{
	DWORD flags = 0;

	if (bColor)	flags |= D3DCLEAR_TARGET;
	if (bDepth)	flags |= D3DCLEAR_ZBUFFER;
	if (bStencil) flags |= D3DCLEAR_STENCIL;

	AssertHR(_device->Clear(0, NULL, flags, _clearColor, 1, 0));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetViewport(uint x, uint y, uint width, uint height)
{
	_FlipRectY(y, height);

	const D3DVIEWPORT9 viewport =
	{
		x, y,
		width, height,
		0, 1
	};
	AssertHR(_device->SetViewport(&viewport));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetViewport(uint &x, uint &y, uint &width, uint &height)
{
	D3DVIEWPORT9 viewport;
	AssertHR(_device->GetViewport(&viewport));
	x = viewport.X;
	y = viewport.Y;
	width = viewport.Width;
	height = viewport.Height;

	_FlipRectY(y, height);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetScissorRectangle(uint x, uint y, uint width, uint height)
{
	_FlipRectY(y, height);

	const RECT rect =
	{
		x, y,
		x + width, y + height
	};
	AssertHR(_device->SetScissorRect(&rect));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetScissorRectangle(uint &x, uint &y, uint &width, uint &height)
{
	RECT rect;
	AssertHR(_device->GetScissorRect(&rect));
	x = rect.left;
	y = rect.top;
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	_FlipRectY(y, height);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetLineWidth(float fWidth)
{
	return (_lineWidth = fWidth) == 1 ? S_OK : S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetLineWidth(float &fWidth)
{
	fWidth = 1;
	return _lineWidth == 1 ? S_OK : S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetPointSize(float fSize)
{
	AssertHR(_device->SetRenderState(D3DRS_POINTSIZE, (DWORD &)fSize));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetPointSize(float &fSize)
{
	AssertHR(_device->GetRenderState(D3DRS_POINTSIZE, (DWORD *)&fSize));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ReadFrameBuffer(uint uiX, uint uiY, uint uiWidth, uint uiHeight, uint8 *pData, uint uiDataSize, E_TEXTURE_DATA_FORMAT eDataFormat)
{
	ComPtr<IDirect3DSurface9> frame_buffer;

	if (eDataFormat == TDF_DEPTH_COMPONENT24 || eDataFormat == TDF_DEPTH_COMPONENT32)
	{
		if (!(frame_buffer = _screenDepthTarget))
			AssertHR(_device->GetDepthStencilSurface(&frame_buffer));
	}
	else
		AssertHR(_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &frame_buffer));

	const auto desc = FlipRectY(frame_buffer, uiY, uiWidth);

	const RECT rect =
	{
		uiX, uiY,
		uiX + uiWidth, uiY + uiHeight
	}, *prect = &rect;

	if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		const auto resolved = _colorPool.GetSurface(_device.Get(), { uiWidth, uiHeight, desc.Format, D3DMULTISAMPLE_NONE });
		if (FAILED(_device->StretchRect(frame_buffer.Get(), prect, resolved.Get(), NULL, D3DTEXF_NONE)))
			return E_FAIL;
		prect = nullptr;
		frame_buffer = resolved;
	}

	bool need_format_adjust = false;
	unsigned int bytes;
	switch (eDataFormat)
	{
	case TDF_RGB8:
		bytes = 3;
		need_format_adjust = true;
		break;
	case TDF_RGBA8:
		bytes = 4;
		need_format_adjust = true;
		break;
	case TDF_ALPHA8:
		bytes = 1;
		need_format_adjust = true;
		break;
	case TDF_BGR8:
		bytes = 3;
		need_format_adjust = true;
		break;
	case TDF_BGRA8:
		bytes = 4;
		break;
	case TDF_DEPTH_COMPONENT32:
		if (desc.Format == D3DFMT_D32F_LOCKABLE)
		{
			bytes = 4;
			break;
		}
		else
			return E_INVALIDARG;
	default:
		return FPE_INVALID;
	}

	bytes *= uiWidth;
	unsigned int row_length = bytes;

	auto RowConvertion = RowCopy;
	if (need_format_adjust)
	{
		if (!(RowConvertion = GetRowConvertion(eDataFormat, desc.Format)))
			return E_FAIL;
		else if (RowConvertion != RowCopy)
			row_length = uiWidth;
	}
	const auto ReadRow = RowConvertion(false);

	if (uiDataSize < uiHeight * bytes)
		return E_INVALIDARG;

	D3DLOCKED_RECT locked;

	AssertHR(frame_buffer->LockRect(&locked, prect, D3DLOCK_READONLY));
	for (uint row = 0; row < uiHeight; row++, pData += bytes, (uint8 *&)locked.pBits += locked.Pitch)
		ReadRow(locked.pBits, pData, row_length);
	AssertHR(frame_buffer->UnlockRect());

	return S_OK;
}

#pragma region CSurfacePool
inline bool CCoreRendererDX9::CSurfacePool::TSurfaceDesc::operator ==(const TSurfaceDesc &src) const
{
	return width == src.width && height == src.height && format == src.format && MSAA == src.MSAA;
}

template<typename T>
static inline size_t GetHash(T value)
{
	return hash<T>()(value);
}

size_t CCoreRendererDX9::CSurfacePool::THash::operator ()(const TSurfaceDesc &src) const
{
	return GetHash(src.width) + GetHash(src.height) + GetHash(src.format) + src.MSAA;
}

static inline bool Used(IUnknown *object)
{
	object->AddRef();
	return object->Release() == 1;
}

CCoreRendererDX9::CSurfacePool::CSurfacePool(E_TYPE type) :
_createSurface(type == E_TYPE::DEPTH ? &IDirect3DDevice9::CreateDepthStencilSurface : &IDirect3DDevice9::CreateRenderTarget),
_boolParam(type != E_TYPE::COLOR)
{
}

ComPtr<IDirect3DSurface9> CCoreRendererDX9::CSurfacePool::GetSurface(IDirect3DDevice9 *device, const TSurfaces::key_type &desc)
{
	const auto range = _surfaces.equal_range(desc);

	// find unused
	const auto unused = find_if(range.first, range.second, [](TSurfaces::const_reference rt)
	{
		return Used(rt.second.surface.Get());
	});

	if (unused != range.second)
	{
		unused->second.idleTime = 0;
		return unused->second.surface;
	}

	// unused not found, create new
	TSurfaces::mapped_type surface;
	AssertHR((device->*_createSurface)(desc.width, desc.height, desc.format, desc.MSAA, 0, _boolParam, &surface.surface, NULL));
	_surfaces.insert({ desc, surface });
	return surface.surface;
}

void CCoreRendererDX9::CSurfacePool::Clean()
{
	auto cur_rt = _surfaces.begin();
	while (cur_rt != _surfaces.end())
	{
		if (!Used(cur_rt->second.surface.Get()) && ++cur_rt->second.idleTime > _maxIdle && _surfaces.size() > _maxPoolSize)
			cur_rt = _surfaces.erase(cur_rt);
		else
			++cur_rt;
	}
}
#pragma endregion

#pragma region COffscreenDepth
ComPtr<IDirect3DSurface9> CCoreRendererDX9::COffscreenDepth::Get(IDirect3DDevice9 *device, UINT width, UINT height, D3DMULTISAMPLE_TYPE MSAA)
{
	bool need_recreate = true;
	if (_surface)
	{
		D3DSURFACE_DESC desc;
		AssertHR(_surface->GetDesc(&desc));
		if (desc.Width >= width && desc.Height >= height && desc.MultiSampleType == MSAA)
			need_recreate = false;
	}
	
	if (need_recreate)
		AssertHR(device->CreateDepthStencilSurface(width, height, _offscreenDepthFormat, MSAA, 0, TRUE, &_surface, NULL));

	return _surface;
}
#pragma endregion

DGLE_RESULT DGLE_API CCoreRendererDX9::SetRenderTarget(ICoreTexture *pTexture)
{
	if (pTexture && _curRenderTarget && FAILED(SetRenderTarget(NULL)))
		return E_ABORT;

	if (!pTexture && _curRenderTarget)
	{
		ComPtr<IDirect3DSurface9> offscreen_target;
		AssertHR(_device->GetRenderTarget(0, &offscreen_target));
		if (offscreen_target)
		{
			D3DSURFACE_DESC offscreen_desc;
			AssertHR(offscreen_target->GetDesc(&offscreen_desc));
			if (offscreen_desc.MultiSampleType != D3DMULTISAMPLE_NONE)
			{
				const auto resolved = _colorPool.GetSurface(_device.Get(), { offscreen_desc.Width, offscreen_desc.Height, offscreen_desc.Format, D3DMULTISAMPLE_NONE });
				if (FAILED(_device->StretchRect(offscreen_target.Get(), NULL, resolved.Get(), NULL, D3DTEXF_NONE)))
					return E_FAIL;
				offscreen_target = resolved;
			}
			D3DLOCKED_RECT src_locked, dst_locked;
			AssertHR(offscreen_target->LockRect(&src_locked, NULL, D3DLOCK_READONLY));
			AssertHR(static_cast<CCoreTexture *>(_curRenderTarget)->GetTex()->LockRect(0, &dst_locked, NULL, 0));
			unsigned int row_size;
			switch (offscreen_desc.Format)
			{
			case D3DFMT_X8R8G8B8:
			case D3DFMT_A8R8G8B8:
			case D3DFMT_A8B8G8R8:
				row_size = 4;
				break;
			case D3DFMT_R8G8B8:
				row_size = 3;
				break;
			case D3DFMT_A8:
				row_size = 1;
				break;
			}
			row_size *= offscreen_desc.Width;
			for (unsigned int row = 0; row < offscreen_desc.Height; row++, (uint8_t *&)src_locked += src_locked.Pitch, (uint8_t *&)dst_locked.pBits += dst_locked.Pitch)
				memcpy(dst_locked.pBits, src_locked.pBits, row_size);
			AssertHR(offscreen_target->UnlockRect());
			AssertHR(static_cast<CCoreTexture *>(_curRenderTarget)->GetTex()->UnlockRect(0));
			AssertHR(_device->SetRenderTarget(0, _screenColorTarget.Get()));
			AssertHR(_device->SetDepthStencilSurface(_screenDepthTarget.Get()));
			AssertHR(_device->SetViewport(&_screenViewport));
			_curRenderTarget = nullptr;
		}
		else
			return E_FAIL;
	}
	else if (pTexture && !_curRenderTarget)
	{
		D3DSURFACE_DESC dst_desc;
		AssertHR(static_cast<CCoreTexture *>(pTexture)->GetTex()->GetLevelDesc(0, &dst_desc));
		switch (dst_desc.Format)
		{
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D32:
		case D3DFMT_D15S1:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D32_LOCKABLE:
		case D3DFMT_S8_LOCKABLE:
		case D3DFMT_D16:
			return E_INVALIDARG;
		}
		TEngineWindow wnd;
		AssertHR(_engineCore.GetCurrentWindow(wnd));
		dst_desc.MultiSampleType = Multisample_DGLE_2_D3D(wnd.eMultisampling);
		if (FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dst_desc.Format, !wnd.bFullScreen, dst_desc.MultiSampleType, NULL)) ||
			FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _offscreenDepthFormat, !wnd.bFullScreen, dst_desc.MultiSampleType, NULL)))
			dst_desc.MultiSampleType = D3DMULTISAMPLE_NONE;
		AssertHR(_device->GetRenderTarget(0, &_screenColorTarget));
		AssertHR(_device->GetDepthStencilSurface(&_screenDepthTarget));
		AssertHR(_device->GetViewport(&_screenViewport));
		CSurfacePool &rt_pool = dst_desc.MultiSampleType == D3DMULTISAMPLE_NONE ? _colorPool : _MSAAcolorPool;
		const auto color_target = rt_pool.GetSurface(_device.Get(), { dst_desc.Width, dst_desc.Height, dst_desc.Format, dst_desc.MultiSampleType });
		const auto depth_target = _offcreenDepth.Get(_device.Get(), dst_desc.Width, dst_desc.Height, dst_desc.MultiSampleType);
		if (!color_target || !depth_target)
			return E_FAIL;
		if (FAILED(_device->SetRenderTarget(0, color_target.Get())) || FAILED(_device->SetDepthStencilSurface(depth_target.Get())))
			return E_FAIL;
		_curRenderTarget = pTexture;
	}
	else
		return S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetRenderTarget(ICoreTexture *&prTexture)
{
	prTexture = _curRenderTarget;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CreateTexture(ICoreTexture *&prTex, const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipmapsPresented, E_CORE_RENDERER_DATA_ALIGNMENT eDataAlignment, E_TEXTURE_DATA_FORMAT eDataFormat, E_TEXTURE_LOAD_FLAGS eLoadFlags)
{
	if (uiWidth == 0 || uiHeight == 0 || !_NSQTexSupport && uiWidth != uiHeight)
		return E_INVALIDARG;

	const bool non_power_of_two = __popcnt(uiWidth) != 1 || __popcnt(uiHeight) != 1;
	if (uiWidth > _maxTexResolution[0] || uiHeight > _maxTexResolution[1] || !_NPOTTexSupport && non_power_of_two)
		return E_INVALIDARG;

	DGLE_RESULT ret = S_OK;

	D3DFORMAT tex_format;
	uint bytes_per_pixel;
	unsigned int row_size;
	const unsigned int *row_length = &row_size;
	auto RowConvertion = RowCopy;
	bool need_format_adjust = false;

#if 0
	bool supported = TexFormatSupported(_device, D3DFMT_X8R8G8B8);
	supported = TexFormatSupported(_device, D3DFMT_X8B8G8R8);
	supported = TexFormatSupported(_device, D3DFMT_A8B8G8R8);
	supported = TexFormatSupported(_device, D3DFMT_A8);
	supported = TexFormatSupported(_device, D3DFMT_R8G8B8);
	supported = TexFormatSupported(_device, D3DFMT_A8R8G8B8);
	supported = TexFormatSupported(_device, D3DFMT_DXT1);
	supported = TexFormatSupported(_device, D3DFMT_DXT5);
	supported = TexFormatSupported(_device, D3DFMT_D24X8);
	supported = TexFormatSupported(_device, D3DFMT_D24X4S4);
	supported = TexFormatSupported(_device, D3DFMT_D24S8);
	supported = TexFormatSupported(_device, D3DFMT_D16);
	supported = TexFormatSupported(_device, D3DFMT_D16_LOCKABLE);
	supported = TexFormatSupported(_device, D3DFMT_D32);
	supported = TexFormatSupported(_device, D3DFMT_D32F_LOCKABLE);

	UINT width, height, levels;
	D3DFORMAT format;
	HRESULT hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_X8R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_X8B8G8R8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_A8B8G8R8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_A8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_A8R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_DXT1), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_DXT5), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D24X8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D24X4S4), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D24S8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D16), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D16_LOCKABLE), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D32), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device.Get(), &width, &height, &levels, 0, &(format = D3DFMT_D32F_LOCKABLE), D3DPOOL_DEFAULT);
#endif

	switch (eDataFormat)
	{
	case TDF_RGB8:
		tex_format = D3DFMT_X8B8G8R8;
		bytes_per_pixel = 3;
		need_format_adjust = true;
		break;
	case TDF_RGBA8:
		tex_format = D3DFMT_A8B8G8R8;
		bytes_per_pixel = 4;
		need_format_adjust = true;
		break;
	case TDF_ALPHA8:
		tex_format = D3DFMT_A8;
		bytes_per_pixel = 1;
		need_format_adjust = true;
		break;
	case TDF_BGR8:
		tex_format = D3DFMT_R8G8B8;
		bytes_per_pixel = 3;
		need_format_adjust = true;
		break;
	case TDF_BGRA8:
		tex_format = D3DFMT_A8R8G8B8;
		bytes_per_pixel = 4;
		need_format_adjust = true;
		break;
	case TDF_DXT1:
		tex_format = D3DFMT_DXT1;
		bytes_per_pixel = 8; // per block
		break;
	case TDF_DXT5:
		tex_format = D3DFMT_DXT5;
		bytes_per_pixel = 16; // per block
		break;
	case TDF_DEPTH_COMPONENT24:
		//return E_INVALIDARG;
	case TDF_DEPTH_COMPONENT32:
		//tex_format = D3DFMT_D32F_LOCKABLE;
		//bytes_per_pixel = 4;
		//break;
	default: return E_INVALIDARG;
	}

	DWORD required_anisotropy = 4;

	if (eLoadFlags & TLF_FILTERING_ANISOTROPIC)
	{
		if (_anisoSupport)
		{
			if (eLoadFlags & TLF_ANISOTROPY_2X)
				required_anisotropy = 2;
			else
				if (eLoadFlags & TLF_ANISOTROPY_4X)
					required_anisotropy = 4;
				else
					if (eLoadFlags & TLF_ANISOTROPY_8X)
						required_anisotropy = 8;
					else
						if (eLoadFlags & TLF_ANISOTROPY_16X)
							required_anisotropy = 16;

			if (required_anisotropy > _maxAnisotropy)
				required_anisotropy = _maxAnisotropy;
		}
		else
		{
			(int &)eLoadFlags &= ~TLF_FILTERING_ANISOTROPIC;
			(int &)eLoadFlags |= TLF_FILTERING_TRILINEAR;

			(int &)eLoadFlags &= ~TLF_ANISOTROPY_2X;
			(int &)eLoadFlags &= ~TLF_ANISOTROPY_4X;
			(int &)eLoadFlags &= ~TLF_ANISOTROPY_8X;
			(int &)eLoadFlags &= ~TLF_ANISOTROPY_16X;

			ret = S_FALSE;
		}
	}

	if (eLoadFlags & TLF_FILTERING_TRILINEAR && !(eLoadFlags & TLF_GENERATE_MIPMAPS) && !bMipmapsPresented)
		(int &)eLoadFlags |= TLF_GENERATE_MIPMAPS;

	const bool is_compressed = eDataFormat == TDF_DXT1 || eDataFormat == TDF_DXT5;

	// DX does not support compressed textures with top mip level not multiple block size
	if (is_compressed && uiWidth % 4 && uiHeight % 4)
		return E_INVALIDARG;

	if (eLoadFlags & TLF_COMPRESS)
		ret = S_FALSE;

	unsigned long int mipmaps = 1;

	if (!_mipmapSupport && (eLoadFlags & TLF_GENERATE_MIPMAPS || bMipmapsPresented))
	{
		(int &)eLoadFlags &= ~TLF_GENERATE_MIPMAPS;
		bMipmapsPresented = false;
		ret = S_FALSE;
	}

	uint cur_align = 0;

	if (bMipmapsPresented)
	{
		_BitScanReverse(&mipmaps, max(uiWidth, uiHeight));
		mipmaps++;

		if (mipmaps > 4 && (eLoadFlags & TLF_DECREASE_QUALITY_MEDIUM || eLoadFlags & TLF_DECREASE_QUALITY_HIGH))
		{
			const unsigned int start_level = eLoadFlags & TLF_DECREASE_QUALITY_MEDIUM ? 1 : 2;
			mipmaps -= start_level;

			for (unsigned int l = 0; l < start_level; ++l)
			{
				if (eDataAlignment == CRDA_ALIGNED_BY_4)
					cur_align = GetPixelDataAlignmentIncrement((uint)uiWidth, bytes_per_pixel, 4);

				if (is_compressed)
					pData += ((uiWidth + 3) / 4) * ((uiHeight + 3) / 4) * bytes_per_pixel;
				else
					pData += uiHeight * (uiWidth * bytes_per_pixel + cur_align);

				uiWidth /= 2;
				uiHeight /= 2;
			}
		}
	}
	else if (eLoadFlags & TLF_GENERATE_MIPMAPS)
		mipmaps = 0;

	if (need_format_adjust)
	{
		if (FAILED(D3DXCheckTextureRequirements(_device.Get(), NULL, NULL, NULL, 0, &tex_format, D3DPOOL_MANAGED)))
			return E_FAIL;
		if (!(RowConvertion = GetRowConvertion(eDataFormat, tex_format)))
			return E_FAIL;
		else if (RowConvertion != RowCopy)
			row_length = &uiWidth;
	}

	ComPtr<IDirect3DTexture9> texture;
	switch (_device->CreateTexture(uiWidth, uiHeight, mipmaps, 0, tex_format, D3DPOOL_MANAGED, &texture, NULL))
	{
	case S_OK:					break;
	case D3DERR_INVALIDCALL:	return E_INVALIDARG;
	default:					return E_ABORT;
	}

	const auto WriteRow = RowConvertion(true);
	unsigned long cur_mipmap = 0;
	do
	{
		if (eDataAlignment == CRDA_ALIGNED_BY_4)
			cur_align = GetPixelDataAlignmentIncrement(uiWidth, bytes_per_pixel, 4);

		D3DLOCKED_RECT locked;
		AssertHR(texture->LockRect(cur_mipmap, &locked, NULL, 0));
		row_size = is_compressed ? (uiWidth + 3) / 4 * bytes_per_pixel : uiWidth * bytes_per_pixel;
		const unsigned int src_stride = row_size + cur_align;
		const unsigned int row_count = is_compressed ? (uiHeight + 3) / 4 : uiHeight;
		for (unsigned int row = 0; row < row_count; row++, pData += src_stride, (uint8_t *&)locked.pBits += locked.Pitch)
			WriteRow(pData, locked.pBits, *row_length);
		AssertHR(texture->UnlockRect(cur_mipmap));

		uiWidth /= 2;
		if (uiWidth == 0) uiWidth = 1;

		uiHeight /= 2;
		if (uiHeight == 0) uiHeight = 1;
	} while (++cur_mipmap < mipmaps);

	if (!bMipmapsPresented && eLoadFlags & TLF_GENERATE_MIPMAPS)
	{
		if (FAILED(D3DXFilterTexture(texture.Get(), NULL, D3DX_DEFAULT, D3DX_DEFAULT)))
		{
			(int &)eLoadFlags &= ~TLF_GENERATE_MIPMAPS;
			ret = S_FALSE;
		}
	}

	prTex = new CCoreTexture(*this, texture, TT_2D, eDataFormat, eLoadFlags, RowConvertion, bytes_per_pixel, eLoadFlags & TLF_GENERATE_MIPMAPS || bMipmapsPresented, required_anisotropy, _textureAddressCaps, ret);

	return ret;
}

namespace
{
	typedef D3DVERTEXELEMENT9 TVertexDecl[extent<decltype(vertexElementLUT)>::value + 2];	// +2 for position and D3DDECL_END()

	inline void SetVertexElement(TVertexDecl &elements, UINT stream, BYTE type, BYTE usage)
	{
		elements[stream] =
		{
			stream,					// Stream
			0,						// Offset
			type,					// Type
			D3DDECLMETHOD_DEFAULT,	// Method
			usage,					// Usage
			0						// UsageIndex
		};
	}

	template<unsigned idx = 0>
	inline void FillVertexDecl(const TDrawDataDesc &drawDesc, TVertexDecl &elements, UINT stream = 0)
	{
		if (drawDesc.*vertexElementLUT[idx].offset != -1)
			SetVertexElement(elements, stream++, vertexElementLUT[idx].type, vertexElementLUT[idx].usage);
		FillVertexDecl<idx + 1>(drawDesc, elements, stream);
	}

	template<>
	inline void FillVertexDecl<extent<decltype(vertexElementLUT)>::value>(const TDrawDataDesc &drawDesc, TVertexDecl &elements, UINT stream)
	{
		SetVertexElement(elements, stream, drawDesc.bVertices2D ? D3DDECLTYPE_FLOAT2 : D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION);
		elements[++stream] = D3DDECL_END();
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CreateGeometryBuffer(ICoreGeometryBuffer *&prBuffer, const TDrawDataDesc &stDrawDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode, E_CORE_RENDERER_BUFFER_TYPE eType)
{
	if (!stDrawDesc.pData || uiVerticesCount == 0 || uiIndicesCount && eMode == CRDM_POINTS)
		return E_INVALIDARG;

	const uint vertices_data_size = uiVerticesCount * GetVertexSize(stDrawDesc),
		indices_data_size = uiIndicesCount * (stDrawDesc.bIndexBuffer32 ? sizeof(uint32) : sizeof(uint16));

	TDrawDataDesc desc(stDrawDesc);
	desc.pData = NULL;
	desc.pIndexBuffer = NULL;

	ComPtr<IDirect3DVertexBuffer9> VB;
	ComPtr<IDirect3DIndexBuffer9> IB;
	ComPtr<IDirect3DVertexDeclaration9> VB_decl;

	DGLE_RESULT ret = S_OK;

	DWORD usage = 0;

	switch (eMode)
	{
	case CRDM_POINTS:
		usage |= D3DUSAGE_POINTS;
		break;
#if 0
	case CRDM_LINES:
		eType = CRBT_SOFTWARE;
		ret = S_FALSE;
		break;
#endif
	}

	D3DPOOL pool = D3DPOOL_MANAGED;
	switch (eType)
	{
	case CRBT_HARDWARE_DYNAMIC:
		eType = CRBT_HARDWARE_STATIC;
	case CRBT_SOFTWARE:
#if 0
		desc.pData = new uint8[vertices_data_size];
		memcpy(desc.pData, stDrawDesc.pData, vertices_data_size);

		if (indices_data_size != 0)
		{
			desc.pIndexBuffer = new uint8[indices_data_size];
			memcpy(desc.pIndexBuffer, stDrawDesc.pIndexBuffer, indices_data_size);
		}
		break;
#else
		// Draw[Indexed]PrimitiveUP supports 1 vertex stream only
		ret = false;
		if (eType == CRBT_SOFTWARE)
		{
			eType = CRBT_HARDWARE_DYNAMIC;
			usage |= D3DUSAGE_DYNAMIC;
			pool = D3DPOOL_DEFAULT;
		}
#endif
	case CRBT_HARDWARE_STATIC:
		{
			void *locked;

			AssertHR(_device->CreateVertexBuffer(vertices_data_size, usage, 0, D3DPOOL_MANAGED, &VB, NULL));
			AssertHR(VB->Lock(0, 0, &locked, 0));
			memcpy(locked, stDrawDesc.pData, vertices_data_size);
			AssertHR(VB->Unlock());
			
			if (indices_data_size)
			{
				AssertHR(_device->CreateIndexBuffer(indices_data_size, usage, stDrawDesc.bIndexBuffer32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, pool, &IB, NULL));
				AssertHR(IB->Lock(0, 0, &locked, 0));
				memcpy(locked, stDrawDesc.pIndexBuffer, indices_data_size);
				AssertHR(IB->Unlock());
			}

			TVertexDecl elements;
			FillVertexDecl(stDrawDesc, elements);
			AssertHR(_device->CreateVertexDeclaration(elements, &VB_decl));
		}
		break;
	default:
		return E_INVALIDARG;
	}

	prBuffer = new CCoreGeometryBuffer(VB, IB, VB_decl, desc, vertices_data_size, indices_data_size, uiVerticesCount, uiIndicesCount, eMode, eType);

	return ret;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleStateFilter(bool bEnabled)
{
	return S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::InvalidateStateFilter()
{
	return S_FALSE;
}

/*
	current push/pop implementation propably very slow
	possily not all states being saved/restored
*/

const/*expr*/ D3DRENDERSTATETYPE CCoreRendererDX9::_rederStateTypes[] =
{
	D3DRS_ZENABLE,
	D3DRS_FILLMODE,
	D3DRS_SHADEMODE,
	D3DRS_ZWRITEENABLE,
	D3DRS_ALPHATESTENABLE,
	D3DRS_LASTPIXEL,
	D3DRS_SRCBLEND,
	D3DRS_DESTBLEND,
	D3DRS_CULLMODE,
	D3DRS_ZFUNC,
	D3DRS_ALPHAREF,
	D3DRS_ALPHAFUNC,
	D3DRS_DITHERENABLE,
	D3DRS_ALPHABLENDENABLE,
	D3DRS_FOGENABLE,
	D3DRS_SPECULARENABLE,
	D3DRS_FOGCOLOR,
	D3DRS_FOGTABLEMODE,
	D3DRS_FOGSTART,
	D3DRS_FOGEND,
	D3DRS_FOGDENSITY,
	D3DRS_RANGEFOGENABLE,
	D3DRS_STENCILENABLE,
	D3DRS_STENCILFAIL,
	D3DRS_STENCILZFAIL,
	D3DRS_STENCILPASS,
	D3DRS_STENCILFUNC,
	D3DRS_STENCILREF,
	D3DRS_STENCILMASK,
	D3DRS_STENCILWRITEMASK,
	D3DRS_TEXTUREFACTOR,
	D3DRS_WRAP0,
	D3DRS_WRAP1,
	D3DRS_WRAP2,
	D3DRS_WRAP3,
	D3DRS_WRAP4,
	D3DRS_WRAP5,
	D3DRS_WRAP6,
	D3DRS_WRAP7,
	D3DRS_CLIPPING,
	D3DRS_LIGHTING,
	D3DRS_AMBIENT,
	D3DRS_FOGVERTEXMODE,
	D3DRS_COLORVERTEX,
	D3DRS_LOCALVIEWER,
	D3DRS_NORMALIZENORMALS,
	D3DRS_DIFFUSEMATERIALSOURCE,
	D3DRS_SPECULARMATERIALSOURCE,
	D3DRS_AMBIENTMATERIALSOURCE,
	D3DRS_EMISSIVEMATERIALSOURCE,
	D3DRS_VERTEXBLEND,
	D3DRS_CLIPPLANEENABLE,
	D3DRS_POINTSIZE,
	D3DRS_POINTSIZE_MIN,
	D3DRS_POINTSPRITEENABLE,
	D3DRS_POINTSCALEENABLE,
	D3DRS_POINTSCALE_A,
	D3DRS_POINTSCALE_B,
	D3DRS_POINTSCALE_C,
	D3DRS_MULTISAMPLEANTIALIAS,
	D3DRS_MULTISAMPLEMASK,
	D3DRS_PATCHEDGESTYLE,
	D3DRS_DEBUGMONITORTOKEN,
	D3DRS_POINTSIZE_MAX,
	D3DRS_INDEXEDVERTEXBLENDENABLE,
	D3DRS_COLORWRITEENABLE,
	D3DRS_TWEENFACTOR,
	D3DRS_BLENDOP,
	D3DRS_POSITIONDEGREE,
	D3DRS_NORMALDEGREE,
	D3DRS_SCISSORTESTENABLE,
	D3DRS_SLOPESCALEDEPTHBIAS,
	D3DRS_ANTIALIASEDLINEENABLE,
	D3DRS_MINTESSELLATIONLEVEL,
	D3DRS_MAXTESSELLATIONLEVEL,
	D3DRS_ADAPTIVETESS_X,
	D3DRS_ADAPTIVETESS_Y,
	D3DRS_ADAPTIVETESS_Z,
	D3DRS_ADAPTIVETESS_W,
	D3DRS_ENABLEADAPTIVETESSELLATION,
	D3DRS_TWOSIDEDSTENCILMODE,
	D3DRS_CCW_STENCILFAIL,
	D3DRS_CCW_STENCILZFAIL,
	D3DRS_CCW_STENCILPASS,
	D3DRS_CCW_STENCILFUNC,
	D3DRS_COLORWRITEENABLE1,
	D3DRS_COLORWRITEENABLE2,
	D3DRS_COLORWRITEENABLE3,
	D3DRS_BLENDFACTOR,
	D3DRS_SRGBWRITEENABLE,
	D3DRS_DEPTHBIAS,
	D3DRS_WRAP8,
	D3DRS_WRAP9,
	D3DRS_WRAP10,
	D3DRS_WRAP11,
	D3DRS_WRAP12,
	D3DRS_WRAP13,
	D3DRS_WRAP14,
	D3DRS_WRAP15,
	D3DRS_SEPARATEALPHABLENDENABLE,
	D3DRS_SRCBLENDALPHA,
	D3DRS_DESTBLENDALPHA,
	D3DRS_BLENDOPALPHA,
};
const/*expr*/ D3DSAMPLERSTATETYPE CCoreRendererDX9::_samplerStateTypes[] =
{
	D3DSAMP_ADDRESSU,
	D3DSAMP_ADDRESSV,
	D3DSAMP_ADDRESSW,
	D3DSAMP_BORDERCOLOR,
	D3DSAMP_MAGFILTER,
	D3DSAMP_MINFILTER,
	D3DSAMP_MIPFILTER,
	D3DSAMP_MIPMAPLODBIAS,
	D3DSAMP_MAXMIPLEVEL,
	D3DSAMP_MAXANISOTROPY,
	D3DSAMP_SRGBTEXTURE,
	D3DSAMP_ELEMENTINDEX,
	D3DSAMP_DMAPOFFSET,
};
const/*expr*/ D3DTEXTURESTAGESTATETYPE CCoreRendererDX9::_stageStateTypes[] =
{
	D3DTSS_COLOROP,
	D3DTSS_COLORARG1,
	D3DTSS_COLORARG2,
	D3DTSS_ALPHAOP,
	D3DTSS_ALPHAARG1,
	D3DTSS_ALPHAARG2,
	D3DTSS_BUMPENVMAT00,
	D3DTSS_BUMPENVMAT01,
	D3DTSS_BUMPENVMAT10,
	D3DTSS_BUMPENVMAT11,
	D3DTSS_TEXCOORDINDEX,
	D3DTSS_BUMPENVLSCALE,
	D3DTSS_BUMPENVLOFFSET,
	D3DTSS_TEXTURETRANSFORMFLAGS,
	D3DTSS_COLORARG0,
	D3DTSS_ALPHAARG0,
	D3DTSS_RESULTARG,
	D3DTSS_CONSTANT,
};

void CCoreRendererDX9::_PushStates()
{
	typedef decltype(_stateStack) TStateStack;
	TStateStack::value_type cur_state;

	transform(begin(_rederStateTypes), end(_rederStateTypes), cur_state.renderStates.begin(), [this](D3DRENDERSTATETYPE state)
	{
		DWORD value;
		AssertHR(_device->GetRenderState(state, &value));
		return value;
	});

	typedef decltype(cur_state.textureStates) TTextureStates;
	cur_state.textureStates = make_unique<TTextureStates::element_type []>(_maxTexStages);
	for (DWORD idx = 0; idx < _maxTexStages; idx++)
	{
		AssertHR(_device->GetTexture(idx, &cur_state.textureStates[idx].texture));
		transform(begin(_samplerStateTypes), end(_samplerStateTypes), cur_state.textureStates[idx].samplerStates.begin(), [=](D3DSAMPLERSTATETYPE state)
		{
			DWORD value;
			AssertHR(_device->GetSamplerState(idx, state, &value));
			return value;
		});
		transform(begin(_stageStateTypes), end(_stageStateTypes), cur_state.textureStates[idx].stageStates.begin(), [=](D3DTEXTURESTAGESTATETYPE state)
		{
			DWORD value;
			AssertHR(_device->GetTextureStageState(idx, state, &value));
			return value;
		});
	}

	AssertHR(_device->GetViewport(&cur_state.viewport));
	AssertHR(_device->GetScissorRect(&cur_state.scissorRect));

#ifdef SAVE_ALL_STATES
	typedef decltype(cur_state.clipPlanes) TClipPlanes;
	cur_state.clipPlanes = make_unique<TClipPlanes::element_type []>(_maxClipPlanes);
	for (DWORD idx = 0; idx < _maxClipPlanes; idx++)
		AssertHR(_device->GetClipPlane(idx, cur_state.clipPlanes[idx]));
#endif

	AssertHR(_device->GetIndices(&cur_state.IB));

	typedef decltype(cur_state.vertexStreams) TVertexStreams;
	cur_state.vertexStreams = make_unique<TVertexStreams::element_type []>(_maxVertexStreams);
	for (DWORD idx = 0; idx < _maxVertexStreams; idx++)
	{
		AssertHR(_device->GetStreamSource(idx, &cur_state.vertexStreams[idx].VB, &cur_state.vertexStreams[idx].offset, &cur_state.vertexStreams[idx].stride));
		AssertHR(_device->GetStreamSourceFreq(idx, &cur_state.vertexStreams[idx].freq));
	}

	AssertHR(_device->GetVertexDeclaration(&cur_state.VBDecl));
#ifdef SAVE_ALL_STATES
	AssertHR(_device->GetFVF(&cur_state.FVF));
	cur_state.NPatchMode = _device->GetNPatchMode();

	AssertHR(_device->GetVertexShader(&cur_state.VS));
	AssertHR(_device->GetPixelShader(&cur_state.PS));

	typedef decltype(cur_state.VSFloatConsts) TVSFloatConsts;
	cur_state.VSFloatConsts = make_unique<TVSFloatConsts::element_type []>(_maxVSFloatConsts);
	AssertHR(_device->GetVertexShaderConstantF(0, (float *)cur_state.VSFloatConsts.get(), _maxVSFloatConsts));
	AssertHR(_device->GetPixelShaderConstantF(0, (float *)cur_state.PSFloatConsts, extent<decltype(cur_state.PSFloatConsts), 0>::value));
	AssertHR(_device->GetVertexShaderConstantI(0, (int *)cur_state.VSIntConsts, extent<decltype(cur_state.VSIntConsts), 0>::value));
	AssertHR(_device->GetPixelShaderConstantI(0, (int *)cur_state.PSIntConsts, extent<decltype(cur_state.PSIntConsts), 0>::value));
	AssertHR(_device->GetVertexShaderConstantB(0, (BOOL *)cur_state.VSBoolConsts, extent<decltype(cur_state.VSBoolConsts), 0>::value));
	AssertHR(_device->GetPixelShaderConstantB(0, (BOOL *)cur_state.PSIntConsts, extent<decltype(cur_state.PSIntConsts), 0>::value));
#endif

	AssertHR(_device->GetMaterial(&cur_state.material));

	cur_state.clearColor = _clearColor;
	cur_state.lineWidth = _lineWidth;

	cur_state.selectedTexLayer = _selectedTexLayer;

	_stateStack.push(move(cur_state));

	_FFP->PushLights();
}

DGLE_RESULT DGLE_API CCoreRendererDX9::PushStates()
{
	typedef decltype(_rtBindingsStack) TRTBindingsStack;
	TRTBindingsStack::value_type cur_bindings;

	typedef decltype(cur_bindings.rendertargets) TSurfaces;
	cur_bindings.rendertargets = make_unique<TSurfaces::element_type []>(_maxRTs);
	for (DWORD idx = 0; idx < _maxRTs; idx++)
	{
		switch (_device->GetRenderTarget(idx, &cur_bindings.rendertargets[idx]))
		{
		case S_OK:
		case D3DERR_NOTFOUND:
			break;
		default:
			return E_ABORT;
		}
	}

	AssertHR(_device->GetDepthStencilSurface(&cur_bindings.deptStensil));

	_rtBindingsStack.push(move(cur_bindings));

	_PushStates();

	return S_OK;
}

void CCoreRendererDX9::_PopStates()
{
	const auto &saved_state = _stateStack.top();

	for (unsigned idx = 0; idx < saved_state.renderStates.size(); idx++)
		AssertHR(_device->SetRenderState(_rederStateTypes[idx], saved_state.renderStates[idx]));

	for (DWORD stage = 0; stage < _maxTexStages; stage++)
	{
		const auto &saved_stage_states = saved_state.textureStates[stage];
		AssertHR(_device->SetTexture(stage, saved_stage_states.texture.Get()));
		for (unsigned idx = 0; idx < saved_stage_states.samplerStates.size(); idx++)
			AssertHR(_device->SetSamplerState(stage, _samplerStateTypes[idx], saved_stage_states.samplerStates[idx]));
		for (unsigned idx = 0; idx < saved_stage_states.stageStates.size(); idx++)
			AssertHR(_device->SetTextureStageState(stage, _stageStateTypes[idx], saved_stage_states.stageStates[idx]));
	}

	AssertHR(_device->SetViewport(&saved_state.viewport));
	AssertHR(_device->SetScissorRect(&saved_state.scissorRect));

#ifdef SAVE_ALL_STATES
	for (DWORD idx = 0; idx < _maxClipPlanes; idx++)
		AssertHR(_device->SetClipPlane(idx, saved_state.clipPlanes[idx]));
#endif

	AssertHR(_device->SetIndices(saved_state.IB.Get()));

	for (DWORD idx = 0; idx < _maxVertexStreams; idx++)
	{
		AssertHR(_device->SetStreamSource(idx, saved_state.vertexStreams[idx].VB.Get(), saved_state.vertexStreams[idx].offset, saved_state.vertexStreams[idx].stride));
		AssertHR(_device->SetStreamSourceFreq(idx, saved_state.vertexStreams[idx].freq));
	}

	AssertHR(_device->SetVertexDeclaration(saved_state.VBDecl.Get()));
#ifdef SAVE_ALL_STATES
	AssertHR(_device->SetFVF(saved_state.FVF));
	AssertHR(_device->SetNPatchMode(saved_state.NPatchMode));

	AssertHR(_device->SetVertexShader(saved_state.VS.Get()));
	AssertHR(_device->SetPixelShader(saved_state.PS.Get()));

	AssertHR(_device->SetVertexShaderConstantF(0, (const float *)saved_state.VSFloatConsts.get(), _maxVSFloatConsts));
	AssertHR(_device->SetPixelShaderConstantF(0, (const float *)saved_state.PSFloatConsts, extent<decltype(saved_state.PSFloatConsts), 0>::value));
	AssertHR(_device->SetVertexShaderConstantI(0, (const int *)saved_state.VSIntConsts, extent<decltype(saved_state.VSIntConsts), 0>::value));
	AssertHR(_device->SetPixelShaderConstantI(0, (const int *)saved_state.PSIntConsts, extent<decltype(saved_state.PSIntConsts), 0>::value));
	AssertHR(_device->SetVertexShaderConstantB(0, (const BOOL *)saved_state.VSBoolConsts, extent<decltype(saved_state.VSBoolConsts), 0>::value));
	AssertHR(_device->SetPixelShaderConstantB(0, (const BOOL *)saved_state.PSIntConsts, extent<decltype(saved_state.PSIntConsts), 0>::value));
#endif

	AssertHR(_device->SetMaterial(&saved_state.material));

	_clearColor = saved_state.clearColor;
	_lineWidth = saved_state.lineWidth;

	_selectedTexLayer = saved_state.selectedTexLayer;

	_stateStack.pop();

	_FFP->PopLights();
}

DGLE_RESULT DGLE_API CCoreRendererDX9::PopStates()
{
	const auto &saved_bindings = _rtBindingsStack.top();

	for (DWORD idx = 0; idx < _maxRTs; idx++)
		AssertHR(_device->SetRenderTarget(idx, saved_bindings.rendertargets[idx].Get()));

	AssertHR(_device->SetDepthStencilSurface(saved_bindings.deptStensil.Get()));

	_rtBindingsStack.pop();

	_PopStates();

	return S_OK;
}

inline D3DTRANSFORMSTATETYPE CCoreRendererDX9::_MatrixType_DGLE_2_D3D(E_MATRIX_TYPE dgleType) const
{
	switch (dgleType)
	{
	case MT_MODELVIEW:	return D3DTS_VIEW;
	case MT_PROJECTION:	return D3DTS_PROJECTION;
	case MT_TEXTURE:	return D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0 + _selectedTexLayer);
	default:
		assert(false);
		__assume(false);
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetMatrix(const TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType)
{
	AssertHR(_device->SetTransform(_MatrixType_DGLE_2_D3D(eMatType), reinterpret_cast<const D3DMATRIX *>(stMatrix._2D)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetMatrix(TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType)
{
	AssertHR(_device->GetTransform(_MatrixType_DGLE_2_D3D(eMatType), reinterpret_cast<D3DMATRIX *>(stMatrix._2D)));
	return S_OK;
}

namespace
{
	inline D3DPRIMITIVETYPE PrimitiveType_DGLE_2_D3D(E_CORE_RENDERER_DRAW_MODE mode)
	{
		switch (mode)
		{
		case CRDM_POINTS:			return D3DPT_POINTLIST;
		case CRDM_LINES:			return D3DPT_LINELIST;
		case CRDM_LINE_STRIP:		return D3DPT_LINESTRIP;
		case CRDM_TRIANGLES:		return D3DPT_TRIANGLELIST;
		case CRDM_TRIANGLE_STRIP:	return D3DPT_TRIANGLESTRIP;
		case CRDM_TRIANGLE_FAN:		return D3DPT_TRIANGLEFAN;
		default:
			assert(false);
			__assume(false);
		}
	}

	inline UINT PrimitiveCount(E_CORE_RENDERER_DRAW_MODE mode, UINT vCount)
	{
		switch (mode)
		{
		case CRDM_POINTS:									return vCount;
		case CRDM_LINES:									return vCount / 2;
		case CRDM_LINE_STRIP:		assert(vCount >= 1);	return vCount - 1;
		case CRDM_TRIANGLES:								return vCount / 3;
		case CRDM_TRIANGLE_STRIP:
		case CRDM_TRIANGLE_FAN:		assert(vCount >= 2);	return vCount - 2;
		default:
			assert(false);
			__assume(false);
		}
	}

	template<typename IdxType>
	uint GetVCount(const IdxType *IB, uint iCount)
	{
		static_assert(is_same<IdxType, uint16>::value || is_same<IdxType, uint32>::value, "invalid IB format");
		assert(iCount > 0);
		return *max_element(IB, IB + iCount) + 1;
	}
}

template<unsigned idx>
inline void CCoreRendererDX9::_BindVB(const TDrawDataDesc &drawDesc, const ComPtr<IDirect3DVertexBuffer9> &VB, UINT stream) const
{
	const auto offset = drawDesc.*vertexElementLUT[idx].offset;
	if (offset != -1)
	{
		const auto stride = drawDesc.*vertexElementLUT[idx].stride ? drawDesc.*vertexElementLUT[idx].stride : GetVertexElementStride(vertexElementLUT[idx].type);
		AssertHR(_device->SetStreamSource(stream++, VB.Get(), offset, stride));
	}
	_BindVB<idx + 1>(drawDesc, VB, stream);
}

template<>
inline void CCoreRendererDX9::_BindVB<extent<decltype(vertexElementLUT)>::value>(const TDrawDataDesc &drawDesc, const ComPtr<IDirect3DVertexBuffer9> &VB, UINT stream) const
{
	const auto stride = drawDesc.uiVertexStride ? drawDesc.uiVertexStride : GetVertexElementStride(drawDesc.bVertices2D ? D3DDECLTYPE_FLOAT2 : D3DDECLTYPE_FLOAT3);
	AssertHR(_device->SetStreamSource(stream, VB.Get(), 0, stride));
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Draw(const TDrawDataDesc &stDrawDesc, E_CORE_RENDERER_DRAW_MODE eMode, uint uiCount)
{
	uint vCount = uiCount, iCount = 0;
	if (stDrawDesc.pIndexBuffer)
	{
		vCount = stDrawDesc.bIndexBuffer32 ? GetVCount((const uint32 *)stDrawDesc.pIndexBuffer, uiCount) : GetVCount((const uint32 *)stDrawDesc.pIndexBuffer, uiCount);
		iCount = uiCount;
	}

	ICoreGeometryBuffer *buff;
	if (FAILED(CreateGeometryBuffer(buff, stDrawDesc, vCount, iCount, eMode, CRBT_HARDWARE_DYNAMIC)))
		return E_ABORT;

	DGLE_RESULT ret = DrawBuffer(buff);
	AssertHR(ret);
	AssertHR(buff->Free());

	return ret;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::DrawBuffer(ICoreGeometryBuffer *pBuffer)
{
	if (CCoreGeometryBuffer *const buff = static_cast<CCoreGeometryBuffer *const>(pBuffer))
	{
		assert(buff->GetVB());

		assert(_FFP);
		const DWORD arg = buff->GetDrawDesc().uiColorOffset == ~0 && !_FFP->IsGlobalLightingEnabled() ? D3DTA_TFACTOR : D3DTA_CURRENT;
		AssertHR(_device->SetTextureStageState(0, D3DTSS_COLORARG2, arg));

		AssertHR(_device->SetVertexDeclaration(buff->GetVBDecl().Get()));
		_BindVB(buff->GetDrawDesc(), buff->GetVB());

		const auto primitive_type = PrimitiveType_DGLE_2_D3D(buff->GetDrawMode());
		const auto primitive_count = PrimitiveCount(buff->GetDrawMode(), buff->GetIB() ? buff->GetIndicesCount() : buff->GetVerticesCount());

		if (buff->GetIB())
		{
			AssertHR(_device->SetIndices(buff->GetIB().Get()));
			AssertHR(_device->DrawIndexedPrimitive(primitive_type, 0, 0, buff->GetVerticesCount(), 0, primitive_count));
		}
		else
			AssertHR(_device->DrawPrimitive(primitive_type, 0, primitive_count));

		return S_OK;
	}

	return S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetColor(const TColor4 &stColor)
{
	AssertHR(_device->SetRenderState(D3DRS_TEXTUREFACTOR, stColor));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetColor(TColor4 &stColor)
{
	DWORD color;
	AssertHR(_device->GetRenderState(D3DRS_TEXTUREFACTOR, &color));
	stColor = color;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleBlendState(bool bEnabled)
{
	AssertHR(_device->SetRenderState(D3DRS_ALPHABLENDENABLE, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleAlphaTestState(bool bEnabled)
{
	AssertHR(_device->SetRenderState(D3DRS_ALPHATESTENABLE, bEnabled));
	return S_OK;
}

void CCoreRendererDX9::_FlipRectY(uint &y, uint height) const
{
	// avoiding D3D Get* calls here and in FlipRectY may improve performance
	ComPtr<IDirect3DSurface9> rt;
	AssertHR(_device->GetRenderTarget(0, &rt));
	FlipRectY(rt, y, height);
}

namespace
{
	inline D3DBLEND BlendFactor_DGLE_2_D3D(E_BLEND_FACTOR dgleFactor)
	{
		switch (dgleFactor)
		{
		case BF_ZERO:					return D3DBLEND_ZERO;
		case BF_ONE:					return D3DBLEND_ONE;
		case BF_SRC_COLOR:				return D3DBLEND_SRCCOLOR;
		case BF_SRC_ALPHA:				return D3DBLEND_SRCALPHA;
		case BF_DST_COLOR:				return D3DBLEND_DESTCOLOR;
		case BF_DST_ALPHA:				return D3DBLEND_DESTALPHA;
		case BF_ONE_MINUS_SRC_COLOR:	return D3DBLEND_INVSRCCOLOR;
		case BF_ONE_MINUS_SRC_ALPHA:	return D3DBLEND_INVSRCALPHA;
		default:
			assert(false);
			__assume(false);
		}
	}

	inline E_BLEND_FACTOR BlendFactor_D3D_2_DGLE(D3DBLEND d3dFactor)
	{
		switch (d3dFactor)
		{
		case D3DBLEND_ZERO:			return BF_ZERO;
		case D3DBLEND_ONE:			return BF_ONE;
		case D3DBLEND_SRCCOLOR:		return BF_SRC_COLOR;
		case D3DBLEND_SRCALPHA:		return BF_SRC_ALPHA;
		case D3DBLEND_DESTCOLOR:	return BF_DST_COLOR;
		case D3DBLEND_DESTALPHA:	return BF_DST_ALPHA;
		case D3DBLEND_INVSRCCOLOR:	return BF_ONE_MINUS_SRC_COLOR;
		case D3DBLEND_INVSRCALPHA:	return BF_ONE_MINUS_SRC_ALPHA;
		default:
			assert(false);
			__assume(false);
		}
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetBlendState(const TBlendStateDesc &stState)
{
	AssertHR(_device->SetRenderState(D3DRS_ALPHABLENDENABLE, stState.bEnabled));
	AssertHR(_device->SetRenderState(D3DRS_SRCBLEND, BlendFactor_DGLE_2_D3D(stState.eSrcFactor)));
	AssertHR(_device->SetRenderState(D3DRS_DESTBLEND, BlendFactor_DGLE_2_D3D(stState.eDstFactor)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetBlendState(TBlendStateDesc &stState)
{
	DWORD value;

	AssertHR(_device->GetRenderState(D3DRS_ALPHABLENDENABLE, &value));
	stState.bEnabled = value;

	AssertHR(_device->GetRenderState(D3DRS_SRCBLEND, &value));
	stState.eSrcFactor = BlendFactor_D3D_2_DGLE(D3DBLEND(value));

	AssertHR(_device->GetRenderState(D3DRS_DESTBLEND, &value));
	stState.eDstFactor = BlendFactor_D3D_2_DGLE(D3DBLEND(value));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::BindTexture(ICoreTexture *pTex, uint uiTextureLayer)
{
	if (uiTextureLayer > _maxTexUnits)
		return E_INVALIDARG;

	_selectedTexLayer = uiTextureLayer;

	AssertHR(_device->SetTexture(uiTextureLayer, pTex ? static_cast<CCoreTexture *>(pTex)->GetTex().Get() : NULL));
	AssertHR(_device->SetTextureStageState(uiTextureLayer, D3DTSS_COLOROP, pTex ? D3DTOP_MODULATE : uiTextureLayer == 0 ? D3DTOP_SELECTARG2 : D3DTOP_DISABLE));

	if (const auto tex = static_cast<CCoreTexture *>(pTex))
	{
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_MAGFILTER, tex->magFilter));
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_MINFILTER, tex->minFilter));
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_MIPFILTER, tex->mipFilter));
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_ADDRESSU, tex->addressMode));
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_ADDRESSV, tex->addressMode));
		AssertHR(_device->SetSamplerState(uiTextureLayer, D3DSAMP_MAXANISOTROPY, tex->anisoLevel));
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetBindedTexture(ICoreTexture *&prTex, uint uiTextureLayer)
{
	if (uiTextureLayer > _maxTexUnits)
		return E_INVALIDARG;

	_selectedTexLayer = uiTextureLayer;

	ComPtr<IDirect3DBaseTexture9> d3dTex;
	AssertHR(_device->GetTexture(uiTextureLayer, &d3dTex));
	DWORD data_size;
	if (FAILED(d3dTex->GetPrivateData(__uuidof(CCoreTexture), &prTex, &data_size)))
		return E_FAIL;
	assert(data_size == sizeof prTex);

	return S_OK;
}

namespace
{
	inline D3DCMPFUNC CmpFunc_DGLE_2_D3D(E_COMPARISON_FUNC dgleFunc)
	{
		switch (dgleFunc)
		{
		case CF_NEVER:			return D3DCMP_NEVER;
		case CF_LESS:			return D3DCMP_LESS;
		case CF_EQUAL:			return D3DCMP_EQUAL;
		case CF_LESS_EQUAL:		return D3DCMP_LESSEQUAL;
		case CF_GREATER:		return D3DCMP_GREATER;
		case CF_NOT_EQUAL:		return D3DCMP_NOTEQUAL;
		case CF_GREATER_EQUAL:	return D3DCMP_GREATEREQUAL;
		case CF_ALWAYS:			return D3DCMP_ALWAYS;
		default:
			assert(false);
			__assume(false);
		}
	}

	inline E_COMPARISON_FUNC CmpFunc_D3D_2_DGLE(D3DCMPFUNC d3dFunc)
	{
		switch (d3dFunc)
		{
		case D3DCMP_NEVER:			return CF_NEVER;
		case D3DCMP_LESS:			return CF_LESS;
		case D3DCMP_EQUAL:			return CF_EQUAL;
		case D3DCMP_LESSEQUAL:		return CF_LESS_EQUAL;
		case D3DCMP_GREATER:		return CF_GREATER;
		case D3DCMP_NOTEQUAL:		return CF_NOT_EQUAL;
		case D3DCMP_GREATEREQUAL:	return CF_GREATER_EQUAL;
		case D3DCMP_ALWAYS:			return CF_ALWAYS;
		default:
			assert(false);
			__assume(false);
		}
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetDepthStencilState(const TDepthStencilDesc &stState)
{
	AssertHR(_device->SetRenderState(D3DRS_ZENABLE, stState.bDepthTestEnabled));
	AssertHR(_device->SetRenderState(D3DRS_ZWRITEENABLE, stState.bWriteToDepthBuffer));
	AssertHR(_device->SetRenderState(D3DRS_ZFUNC, CmpFunc_DGLE_2_D3D(stState.eDepthFunc)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetDepthStencilState(TDepthStencilDesc &stState)
{
	DWORD value;

	AssertHR(_device->GetRenderState(D3DRS_ZENABLE, &value));
	stState.bDepthTestEnabled = value;

	AssertHR(_device->GetRenderState(D3DRS_ZWRITEENABLE, &value));
	stState.bWriteToDepthBuffer = value;

	AssertHR(_device->GetRenderState(D3DRS_ZFUNC, &value));
	stState.eDepthFunc = CmpFunc_D3D_2_DGLE(D3DCMPFUNC(value));

	return S_OK;
}

namespace
{
	inline D3DCULL CullMode_DGLE_2_D3D(E_POLYGON_CULL_MODE dgleMode, bool frontCCW)
	{
		switch (dgleMode)
		{
		case PCM_NONE:	return D3DCULL_NONE;
		case PCM_FRONT:	return frontCCW ? D3DCULL_CCW : D3DCULL_CW;
		case PCM_BACK:	return frontCCW ? D3DCULL_CW : D3DCULL_CCW;
		default:
			assert(false);
			__assume(false);
		}
	}

	inline E_POLYGON_CULL_MODE CullMode_D3D_2_DGLE(D3DCULL d3dMode)
	{
		switch (d3dMode)
		{
		case D3DCULL_NONE:	return PCM_NONE;
		case D3DCULL_CW:	return PCM_BACK;
		case D3DCULL_CCW:	return PCM_FRONT;
		default:
			assert(false);
			__assume(false);
		}
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetRasterizerState(const TRasterizerStateDesc &stState)
{
	AssertHR(_device->SetRenderState(D3DRS_ALPHATESTENABLE, stState.bAlphaTestEnabled));
	AssertHR(_device->SetRenderState(D3DRS_ALPHAFUNC, CmpFunc_DGLE_2_D3D(stState.eAlphaTestFunc)));

	/*
		rounding towards 0 currently used
		TODO: consider rounding to nearest even instead
	*/
	AssertHR(_device->SetRenderState(D3DRS_ALPHAREF, stState.fAlphaTestRefValue * 255));

	AssertHR(_device->SetRenderState(D3DRS_SCISSORTESTENABLE, stState.bScissorEnabled));
	AssertHR(_device->SetRenderState(D3DRS_FILLMODE, stState.bWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID));
	AssertHR(_device->SetRenderState(D3DRS_CULLMODE, CullMode_DGLE_2_D3D(stState.eCullMode, stState.bFrontCounterClockwise)));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetRasterizerState(TRasterizerStateDesc &stState)
{
	DWORD value;

	AssertHR(_device->GetRenderState(D3DRS_ALPHATESTENABLE, &value));
	stState.bAlphaTestEnabled = value;

	AssertHR(_device->GetRenderState(D3DRS_ALPHAFUNC, &value));
	stState.eAlphaTestFunc = CmpFunc_D3D_2_DGLE(D3DCMPFUNC(value));

	AssertHR(_device->GetRenderState(D3DRS_ALPHAREF, &value));
	stState.fAlphaTestRefValue = value / 255.f;

	AssertHR(_device->GetRenderState(D3DRS_SCISSORTESTENABLE, &value));
	stState.bScissorEnabled = value;

	AssertHR(_device->GetRenderState(D3DRS_FILLMODE, &value));
	stState.bWireframe = value == D3DFILL_WIREFRAME;

	AssertHR(_device->GetRenderState(D3DRS_CULLMODE, &value));
	stState.eCullMode = CullMode_D3D_2_DGLE(D3DCULL(value));
	stState.bFrontCounterClockwise = true;

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Present()
{
	AssertHR(_device->EndScene());
	switch (_device->Present(NULL, NULL, NULL, NULL))
	{
		// TODO: handle lost device here
	}
	AssertHR(_device->BeginScene());

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetFixedFunctionPipelineAPI(IFixedFunctionPipeline *&prFFP)
{
	prFFP = _FFP;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetDeviceMetric(E_CORE_RENDERER_METRIC_TYPE eMetric, int &iValue)
{
	switch (eMetric)
	{
	case CRMT_MAX_TEXTURE_RESOLUTION:
		iValue = min(_maxTexResolution[0], _maxTexResolution[1]);
		break;
	case CRMT_MAX_ANISOTROPY_LEVEL:
		iValue = _maxAnisotropy;
		break;
	case CRMT_MAX_TEXTURE_LAYERS:
		iValue = _maxTexUnits;
		break;
	default:
		iValue = 0;
		return S_FALSE;
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::IsFeatureSupported(E_CORE_RENDERER_FEATURE_TYPE eFeature, bool &bIsSupported)
{
	switch (eFeature)
	{
	case CRFT_BUILTIN_FULLSCREEN_MODE:
		bIsSupported = false;
		break;
	case CRFT_BUILTIN_STATE_FILTER:
		bIsSupported = false;
		break;
	case CRFT_MULTISAMPLING:
		bIsSupported = true;
		break;
	case CRFT_VSYNC:
		bIsSupported = true;
		break;
	case CRFT_PROGRAMMABLE_PIPELINE:
		bIsSupported = false;
		break;
	case CRFT_LEGACY_FIXED_FUNCTION_PIPELINE_API:
		bIsSupported = true;
		break;
	case CRFT_BGRA_DATA_FORMAT:
		bIsSupported = TexFormatSupported(_device, D3DFMT_A8R8G8B8);
		break;
	case CRFT_TEXTURE_COMPRESSION:
		bIsSupported = TexFormatSupported(_device, D3DFMT_DXT1) && TexFormatSupported(_device, D3DFMT_DXT5);
		break;
	case CRFT_NON_POWER_OF_TWO_TEXTURES:
		bIsSupported = _NPOTTexSupport;
		break;
	case CRFT_DEPTH_TEXTURES:
		bIsSupported = false;	// TDF_DEPTH_COMPONENT24 currently unsupported
		break;
	case CRFT_TEXTURE_ANISOTROPY:
		bIsSupported = _anisoSupport;
		break;
	case CRFT_TEXTURE_MIPMAP_GENERATION:
		bIsSupported = _mipmapSupport;
		break;
	case CRFT_TEXTURE_MIRRORED_REPEAT:
		bIsSupported = _textureAddressCaps & D3DPTADDRESSCAPS_MIRROR;
		break;
	case CRFT_TEXTURE_MIRROR_CLAMP:
		bIsSupported = D3DPTADDRESSCAPS_MIRRORONCE;
		break;
	case CRFT_GEOMETRY_BUFFER:
		bIsSupported = true;
		break;
	case CRFT_FRAME_BUFFER:
		bIsSupported = true;
		break;

	default:
		bIsSupported = false;
		return S_FALSE;
	}

	return S_OK;
}

void DGLE_API CCoreRendererDX9::EventsHandler(void *pParameter, IBaseEvent *pEvent)
{
	E_EVENT_TYPE type;
	AssertHR(pEvent->GetEventType(type));

	switch (type)
	{
	case ET_ON_PER_SECOND_TIMER:
		PTHIS(CCoreRendererDX9)->_depthPool.Clean();
		PTHIS(CCoreRendererDX9)->_colorPool.Clean();
		PTHIS(CCoreRendererDX9)->_MSAAcolorPool.Clean();
		break;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetRendererType(E_CORE_RENDERER_TYPE &eType)
{
	eType = CRT_DIRECT_3D_9_0c;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetType(E_ENGINE_SUB_SYSTEM &eSubSystemType)
{
	eSubSystemType = ESS_CORE_RENDERER;
	return S_OK;
}
#pragma endregion

#endif