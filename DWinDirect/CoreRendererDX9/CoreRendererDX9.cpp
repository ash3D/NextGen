/**
\author		Alexey Shaydurov aka ASH
\date		09.03.2017 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "CoreRendererDX9.h"
#include <d3dx9.h>
#include <algorithm>
#include <numeric>
#ifndef USE_CIRCULAR_BUFFER
#include <iterator>
#endif
#include <intrin.h>
#include "general math.h"

#define USE_REF_DEVICE			0
#define SYNC_RT_TEX_LAZY		1
#define WAIT_FOR_ALL_QUERIES	1

#if USE_REF_DEVICE
#	define DEVTYPE D3DDEVTYPE_REF
#else
#	define DEVTYPE D3DDEVTYPE_HAL
#endif

#define PROFILER_CMD_NAME "crdx9_profiler"

constexpr float GPUTimeGraphScale = 10.f;

using namespace std;

const IDirect3D9Ptr d3d(Direct3DCreate9(D3D_SDK_VERSION));

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

	D3DSURFACE_DESC FlipRectY(const IDirect3DSurface9Ptr &surface, uint &y, uint height)
	{
		y += height - 1;	// lower-left <-> upper-left

		D3DSURFACE_DESC surface_desc;
		AssertHR(surface->GetDesc(&surface_desc));
		y = surface_desc.Height - y;	// GL <-> D3D

		// rely on NRVO
		return surface_desc;
	}

	inline void operator +=(D3DDEVINFO_D3D9INTERFACETIMINGS &left, const D3DDEVINFO_D3D9INTERFACETIMINGS &right)
	{
		left.WaitingForGPUToUseApplicationResourceTimePercent += right.WaitingForGPUToUseApplicationResourceTimePercent;
		left.WaitingForGPUToAcceptMoreCommandsTimePercent += right.WaitingForGPUToAcceptMoreCommandsTimePercent;
		left.WaitingForGPUToStayWithinLatencyTimePercent += right.WaitingForGPUToStayWithinLatencyTimePercent;
		left.WaitingForGPUExclusiveResourceTimePercent += right.WaitingForGPUExclusiveResourceTimePercent;
		left.WaitingForGPUOtherTimePercent += right.WaitingForGPUOtherTimePercent;
	}

	inline D3DDEVINFO_D3D9INTERFACETIMINGS operator /(const D3DDEVINFO_D3D9INTERFACETIMINGS &left, unsigned int right)
	{
		return
		{
			left.WaitingForGPUToUseApplicationResourceTimePercent / right,
			left.WaitingForGPUToAcceptMoreCommandsTimePercent / right,
			left.WaitingForGPUToStayWithinLatencyTimePercent / right,
			left.WaitingForGPUExclusiveResourceTimePercent / right,
			left.WaitingForGPUOtherTimePercent / right
		};
	}

	inline void operator +=(D3DDEVINFO_D3D9PIPELINETIMINGS &left, const D3DDEVINFO_D3D9PIPELINETIMINGS &right)
	{
		left.VertexProcessingTimePercent += right.VertexProcessingTimePercent;
		left.PixelProcessingTimePercent += right.PixelProcessingTimePercent;
		left.OtherGPUProcessingTimePercent += right.OtherGPUProcessingTimePercent;
		left.GPUIdleTimePercent += right.GPUIdleTimePercent;
	}

	inline D3DDEVINFO_D3D9PIPELINETIMINGS operator /(const D3DDEVINFO_D3D9PIPELINETIMINGS &left, unsigned int right)
	{
		return
		{
			left.VertexProcessingTimePercent / right,
			left.PixelProcessingTimePercent / right,
			left.OtherGPUProcessingTimePercent / right,
			left.GPUIdleTimePercent / right
		};
	}

	inline void operator +=(D3DDEVINFO_D3D9BANDWIDTHTIMINGS &left, const D3DDEVINFO_D3D9BANDWIDTHTIMINGS &right)
	{
		left.MaxBandwidthUtilized += right.MaxBandwidthUtilized;
		left.FrontEndUploadMemoryUtilizedPercent += right.FrontEndUploadMemoryUtilizedPercent;
		left.VertexRateUtilizedPercent += right.VertexRateUtilizedPercent;
		left.TriangleSetupRateUtilizedPercent += right.TriangleSetupRateUtilizedPercent;
		left.FillRateUtilizedPercent += right.FillRateUtilizedPercent;
	}

	inline D3DDEVINFO_D3D9BANDWIDTHTIMINGS operator /(const D3DDEVINFO_D3D9BANDWIDTHTIMINGS &left, unsigned int right)
	{
		return
		{
			left.MaxBandwidthUtilized / right,
			left.FrontEndUploadMemoryUtilizedPercent / right,
			left.VertexRateUtilizedPercent / right,
			left.TriangleSetupRateUtilizedPercent / right,
			left.FillRateUtilizedPercent / right
		};
	}

	inline void operator +=(D3DDEVINFO_D3D9STAGETIMINGS &left, const D3DDEVINFO_D3D9STAGETIMINGS &right)
	{
		left.MemoryProcessingPercent += right.MemoryProcessingPercent;
		left.ComputationProcessingPercent += right.ComputationProcessingPercent;
	};

	inline D3DDEVINFO_D3D9STAGETIMINGS operator /(const D3DDEVINFO_D3D9STAGETIMINGS &left, unsigned int right)
	{
		return
		{
			left.MemoryProcessingPercent / right,
			left.ComputationProcessingPercent / right
		};
	}

	inline void operator +=(D3DDEVINFO_D3D9CACHEUTILIZATION &left, const D3DDEVINFO_D3D9CACHEUTILIZATION &right)
	{
		left.TextureCacheHitRate += right.TextureCacheHitRate;
		left.PostTransformVertexCacheHitRate += right.PostTransformVertexCacheHitRate;
	};

	inline D3DDEVINFO_D3D9CACHEUTILIZATION operator /(const D3DDEVINFO_D3D9CACHEUTILIZATION &left, unsigned int right)
	{
		return
		{
			left.TextureCacheHitRate / right,
			left.PostTransformVertexCacheHitRate / right
		};
	}
}

static inline bool Unused(IUnknown *object)
{
	object->AddRef();
	return object->Release() == 1;
}

#pragma region geometry
/*
	Currently each vertex attribute mapped to dedicated stream which is bad from GPU's input assembler point of view
	but simplifies handling of source geometry with arbitrary attribute offsets and strides.
	Consider rearrangement of geometry data in fewer streams for static buffers in order to optimize GPU performance.
	*/

namespace
{
	inline uint GetVertexSize(const TDrawDataDesc &desc)
	{
		uint res = sizeof(float) * ((desc.bVertices2D ? 2 : 3) + (desc.uiNormalOffset != -1 ? 3 : 0) + (desc.uiTextureVertexOffset != -1 ? 2 : 0) +
			(desc.uiColorOffset != -1 ? 4 : 0) + (desc.uiTangentOffset != -1 ? 3 : 0) + (desc.uiBinormalOffset != -1 ? 3 : 0));

		if (desc.pAttribs)
		{
			/* not implemented */
		}

		return res;
	}

	inline uint GetVerticesDataSize(const TDrawDataDesc &desc, uint verticesCount)
	{
		return verticesCount * GetVertexSize(desc);
	}

	inline uint GetIndicesDataSize(const TDrawDataDesc &desc, uint indicesCount)
	{
		return indicesCount * (desc.bIndexBuffer32 ? sizeof(uint32) : sizeof(uint16));
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

	static constexpr struct
	{
		uint TDrawDataDesc::*offset, TDrawDataDesc::*stride;
		BYTE type;
		BYTE usage;
	} vertexElementLUT[] =
	{
		{ &TDrawDataDesc::uiNormalOffset,			&TDrawDataDesc::uiNormalStride,			D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL		},
		{ &TDrawDataDesc::uiTextureVertexOffset,	&TDrawDataDesc::uiTextureVertexStride,	D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD	},
		{ &TDrawDataDesc::uiColorOffset,			&TDrawDataDesc::uiColorStride,			D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_COLOR		},
		{ &TDrawDataDesc::uiTangentOffset,			&TDrawDataDesc::uiTangentStride,		D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TANGENT	},
		{ &TDrawDataDesc::uiBinormalOffset,			&TDrawDataDesc::uiBinormalStride,		D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_BINORMAL	}
	};
}

#pragma region CGeometryProviderBase
class CCoreRendererDX9::CGeometryProviderBase
{
	IDirect3DVertexDeclaration9Ptr _VBDecl;

protected:
	CCoreRendererDX9 &_parent;
	TDrawDataDesc _drawDataDesc;
	E_CORE_RENDERER_DRAW_MODE _drawMode;
	uint _verticesCount, _indicesCount;

protected:
	CGeometryProviderBase(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, E_CORE_RENDERER_DRAW_MODE mode, uint verCnt = 0, uint idxCnt = 0);

	CGeometryProviderBase(CGeometryProviderBase &) = delete;
	void operator =(CGeometryProviderBase &) = delete;

	virtual ~CGeometryProviderBase() = default;

public:
	virtual const IDirect3DVertexBuffer9Ptr &GetVB() const = 0;
	virtual const IDirect3DIndexBuffer9Ptr &GetIB() const = 0;
	const IDirect3DVertexDeclaration9Ptr &GetVBDecl() const { return _VBDecl; }

	// returns byte offset
	virtual unsigned int SetupVB() = 0, SetupIB() = 0;

public:
	inline E_CORE_RENDERER_DRAW_MODE GetDrawMode() const { return _drawMode; }
	inline uint GetVerticesCount() const { return _verticesCount; }
	inline uint GetIndicesCount() const { return _indicesCount; }
	inline const TDrawDataDesc &GetDrawDesc() const { return _drawDataDesc; }

protected:
	void _UpdateVBDecl();
};

CCoreRendererDX9::CGeometryProviderBase::CGeometryProviderBase(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, E_CORE_RENDERER_DRAW_MODE mode, uint verCnt, uint idxCnt) :
_VBDecl(parent._VBDeclCache.GetDecl(parent._device, drawDesc)), _parent(parent), _drawDataDesc(drawDesc), _drawMode(mode), _verticesCount(verCnt), _indicesCount(idxCnt)
{}

inline void CCoreRendererDX9::CGeometryProviderBase::_UpdateVBDecl()
{
	_VBDecl = _parent._VBDeclCache.GetDecl(_parent._device, _drawDataDesc);
}
#pragma endregion

#pragma region CGeometryProvider
class CCoreRendererDX9::CGeometryProvider : virtual public CGeometryProviderBase
{
protected:
	CDynamicVB *_VB;
	CDynamicIB *_IB;

protected:
	// TODO: consider using C++11 inheriting ctor (note: may be prevented due to virtual inheritance)
	CGeometryProvider(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, E_CORE_RENDERER_DRAW_MODE mode) :
		CGeometryProviderBase(parent, drawDesc, mode) {}

public:
	CGeometryProvider(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, E_CORE_RENDERER_DRAW_MODE mode, uint verCnt, uint idxCnt, CDynamicVB *VB, CDynamicIB *IB) :
		CGeometryProviderBase(parent, drawDesc, mode, verCnt, idxCnt), _VB(VB), _IB(IB) {}

public:
	virtual const IDirect3DVertexBuffer9Ptr &GetVB() const override final { assert(_VB);  return _VB->GetVB(); }
	virtual const IDirect3DIndexBuffer9Ptr &GetIB() const override final { assert(_IB); return _IB->GetIB(); }
	virtual unsigned int SetupVB() override final, SetupIB() override final;
};

unsigned int CCoreRendererDX9::CGeometryProvider::SetupVB()
{
	assert(_VB);
	return _VB->FillSegment(_drawDataDesc.pData, GetVerticesDataSize(_drawDataDesc, _verticesCount));
}

unsigned int CCoreRendererDX9::CGeometryProvider::SetupIB()
{
	assert(_IB);
	return _IB->FillSegment(_drawDataDesc.pIndexBuffer, GetIndicesDataSize(_drawDataDesc, _indicesCount));
}
#pragma endregion

#pragma region CCoreGeometryBufferBase
class CCoreRendererDX9::CCoreGeometryBufferBase : virtual public CGeometryProviderBase, public ICoreGeometryBuffer
{
	using CGeometryProviderBase::GetVB;
	using CGeometryProviderBase::GetIB;
	using CGeometryProviderBase::GetVBDecl;

	class CDX9BufferContainer final : public IDX9BufferContainer
	{
		// need to store reference since offsetof() may be unsafe here due to virtual inheritance
		const CGeometryProviderBase &_geomProvider;

	public:
		CDX9BufferContainer(const CGeometryProviderBase &geomProvider) : _geomProvider(geomProvider) {}

	public:
		DGLE_RESULT DGLE_API GetVB(IDirect3DVertexBuffer9 *&VB) override
		{
			VB = _geomProvider.GetVB().GetInterfacePtr();
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetIB(IDirect3DIndexBuffer9 *&IB) override
		{
			IB = _geomProvider.GetIB().GetInterfacePtr();
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetVBDecl(IDirect3DVertexDeclaration9 *&VBDecl) override
		{
			VBDecl = _geomProvider.GetVBDecl().GetInterfacePtr();
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetObjectType(E_ENGINE_OBJECT_TYPE &eType) override
		{
			eType = EOT_MESH;
			return S_OK;
		}

		IDGLE_BASE_IMPLEMENTATION(IDX9BufferContainer, INTERFACE_IMPL(IBaseRenderObjectContainer, INTERFACE_IMPL_END))
	} _bufferContainer{ *this };

protected:
	CCoreGeometryBufferBase(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, E_CORE_RENDERER_DRAW_MODE mode) :
		CGeometryProviderBase(parent, drawDesc, mode)
	{
		_drawDataDesc.pData = _drawDataDesc.pIndexBuffer = nullptr;
	}

	virtual ~CCoreGeometryBufferBase() override
	{
		delete[] _drawDataDesc.pData;
		delete[] _drawDataDesc.pIndexBuffer;
	}

protected:
	void _Reallocate(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode);

private:
	virtual void _GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const = 0;
	virtual void _ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode) = 0;

public:
	DGLE_RESULT DGLE_API GetGeometryData(TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize) override final;

	DGLE_RESULT DGLE_API SetGeometryData(const TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize) override final;

	DGLE_RESULT DGLE_API Reallocate(const TDrawDataDesc &stDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode) override final;

	DGLE_RESULT DGLE_API GetBufferDimensions(uint &uiVerticesDataSize, uint &uiVerticesCount, uint &uiIndexesDataSize, uint &uiIndicesCount) override final;

	DGLE_RESULT DGLE_API GetBufferDrawDataDesc(TDrawDataDesc &stDesc) override final
	{
		stDesc = _drawDataDesc;
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetBufferDrawMode(E_CORE_RENDERER_DRAW_MODE &eMode) override final
	{
		eMode = _drawMode;
		return S_OK;
	}

	DGLE_RESULT DGLE_API GetBaseObject(IBaseRenderObjectContainer *&prObj) override final
	{
		prObj = &_bufferContainer;
		return S_OK;
	}

	DGLE_RESULT DGLE_API Free() override final
	{
		delete this;
		return S_OK;
	}

	IDGLE_BASE_IMPLEMENTATION(ICoreGeometryBuffer, INTERFACE_IMPL_END)
};

void CCoreRendererDX9::CCoreGeometryBufferBase::_Reallocate(const TDrawDataDesc &stDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode)
{
	_ReallocateImpl(stDesc, GetVerticesDataSize(stDesc, uiVerticesCount), GetIndicesDataSize(stDesc, uiIndicesCount), eMode);

	uint8 *const data = _drawDataDesc.pData, *const idx = _drawDataDesc.pIndexBuffer;
	_drawDataDesc = stDesc;
	_drawDataDesc.pData = data;
	_drawDataDesc.pIndexBuffer = idx;

	_drawMode = eMode;

	_verticesCount = uiVerticesCount;
	_indicesCount = uiIndicesCount;

	_UpdateVBDecl();
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreGeometryBufferBase::GetGeometryData(TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize)
{
	CHECK_DEVICE(_parent);

	if (uiVerticesDataSize != GetVerticesDataSize(stDesc, _verticesCount) || uiIndexesDataSize != GetIndicesDataSize(stDesc, _indicesCount))
		return E_INVALIDARG;

	uint8 *data = stDesc.pData, *idx = stDesc.pIndexBuffer;
	stDesc = _drawDataDesc;
	stDesc.pData = data;
	stDesc.pIndexBuffer = idx;

	_GetGeometryDataImpl(stDesc.pData, uiVerticesDataSize, stDesc.pIndexBuffer, uiIndexesDataSize);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreGeometryBufferBase::SetGeometryData(const TDrawDataDesc &stDesc, uint uiVerticesDataSize, uint uiIndexesDataSize)
{
	CHECK_DEVICE(_parent);

	if (uiVerticesDataSize != GetVerticesDataSize(_drawDataDesc, _verticesCount) || uiIndexesDataSize != GetIndicesDataSize(_drawDataDesc, _indicesCount))
		return E_INVALIDARG;

	return Reallocate(stDesc, _verticesCount, _indicesCount, _drawMode);
}

// inline for call from SetGeometryData
inline DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreGeometryBufferBase::Reallocate(const TDrawDataDesc &stDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode)
{
	CHECK_DEVICE(_parent);

	if (bool(_indicesCount) != bool(uiIndicesCount))
		return E_INVALIDARG;

	try
	{
		_Reallocate(stDesc, uiVerticesCount, uiIndicesCount, eMode);
	}
	catch (const HRESULT hr)
	{
		return hr;
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreGeometryBufferBase::GetBufferDimensions(uint &uiVerticesDataSize, uint &uiVerticesCount, uint &uiIndexesDataSize, uint &uiIndicesCount)
{
	uiVerticesDataSize = GetVerticesDataSize(_drawDataDesc, _verticesCount);
	uiVerticesCount = _verticesCount;
	uiIndexesDataSize = GetIndicesDataSize(_drawDataDesc, _indicesCount);
	uiIndicesCount = _indicesCount;
	return S_OK;
}
#pragma endregion

#pragma region CCoreGeometryBufferSoftware
class CCoreRendererDX9::CCoreGeometryBufferSoftware final : public CCoreGeometryBufferBase, CGeometryProvider
{
public:
	CCoreGeometryBufferSoftware(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode);

private:
	virtual void _GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const override;
	virtual void _ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode) override;

public:
	DGLE_RESULT DGLE_API GetBufferType(E_CORE_RENDERER_BUFFER_TYPE &eType) override
	{
		eType = CRBT_SOFTWARE;
		return S_OK;
	}
};

// 1 call site
inline CCoreRendererDX9::CCoreGeometryBufferSoftware::CCoreGeometryBufferSoftware(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode) :
CGeometryProviderBase(parent, drawDesc, mode), CCoreGeometryBufferBase(parent, drawDesc, mode), CGeometryProvider(parent, drawDesc, mode)
{
	_Reallocate(drawDesc, verCnt, idxCnt, mode);
}

void CCoreRendererDX9::CCoreGeometryBufferSoftware::_GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const
{
	memcpy(vertexData, _drawDataDesc.pData, verticesDataSize);

	if (indicesDataSize)
		memcpy(indexData, _drawDataDesc.pIndexBuffer, indicesDataSize);
}

void CCoreRendererDX9::CCoreGeometryBufferSoftware::_ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode)
{
	const bool is_points = drawMode == CRDM_POINTS;

	_VB = _parent._GetImmediateVB(is_points);

	if (GetVerticesDataSize(drawDesc, _verticesCount) != verticesDataSize)
	{
		uint8 *const new_data = new uint8[verticesDataSize];
		delete[] _drawDataDesc.pData;
		_drawDataDesc.pData = new_data;
	}

	memcpy(_drawDataDesc.pData, drawDesc.pData, verticesDataSize);

	if (indicesDataSize)
	{
		_IB = _parent._GetImmediateIB(is_points, drawDesc.bIndexBuffer32);

		if (GetIndicesDataSize(drawDesc, _indicesCount) != indicesDataSize)
		{
			uint8 *const new_data = new uint8[indicesDataSize];
			delete[] _drawDataDesc.pIndexBuffer;
			_drawDataDesc.pIndexBuffer = new_data;
		}

		memcpy(_drawDataDesc.pIndexBuffer, drawDesc.pIndexBuffer, indicesDataSize);
	}
	else
	{
		delete[] _drawDataDesc.pIndexBuffer;
		_drawDataDesc.pIndexBuffer = nullptr;
		_IB = nullptr;
	}
}
#pragma endregion

namespace
{
	template<class Base>
	class CDynamicBuffer final : public Base
	{
		unsigned int _offset;

	public:
		using Base::Base;

	public:
		void FillSegment(const void *data, unsigned int size) { _offset = Base::FillSegment(data, size); }
		unsigned int GetOffset() const { return _offset; }

	private:
		void _OnGrow(const IDirect3DResource9Ptr &oldBuffer, unsigned int oldOffset) override;
	};

	template<class Base>
	void CDynamicBuffer<Base>::_OnGrow(const IDirect3DResource9Ptr &oldBufferBase, unsigned int oldOffset)
	{
		const typename Base::InterfacePtr old_buffer = oldBufferBase;
		void *locked;
		const auto size = oldOffset - _offset;
		AssertHR(old_buffer->Lock(_offset, size, &locked, D3DLOCK_READONLY));
		FillSegment(locked, size);
		AssertHR(old_buffer->Unlock());
	}
}

#pragma region CCoreGeometryBufferDynamic
class CCoreRendererDX9::CCoreGeometryBufferDynamic final : public CCoreGeometryBufferBase
{
	CDynamicBuffer<CDynamicVB> _VB;
	CDynamicBuffer<CDynamicIB> _IB;

public:
	CCoreGeometryBufferDynamic(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode);

public:
	virtual const IDirect3DVertexBuffer9Ptr &GetVB() const override { return _VB.GetVB(); }
	virtual const IDirect3DIndexBuffer9Ptr &GetIB() const override { return _IB.GetIB(); }
	virtual unsigned int SetupVB() override { return _VB.GetOffset(); }
	virtual unsigned int SetupIB() override { return _IB.GetOffset(); }

private:
	virtual void _GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const override;
	virtual void _ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode) override;

public:
	DGLE_RESULT DGLE_API GetBufferType(E_CORE_RENDERER_BUFFER_TYPE &eType) override
	{
		eType = CRBT_HARDWARE_DYNAMIC;
		return S_OK;
	}
};

// 1 call site
inline CCoreRendererDX9::CCoreGeometryBufferDynamic::CCoreGeometryBufferDynamic(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode) :
CGeometryProviderBase(parent, drawDesc, mode), CCoreGeometryBufferBase(parent, drawDesc, mode), _VB(parent, mode == CRDM_POINTS)
{
	if (idxCnt)
	{
		// consider using swap with temp for exception safety
		_IB.~CDynamicBuffer();
		try
		{
			new(&_IB) decltype(_IB)(parent, mode == CRDM_POINTS, drawDesc.bIndexBuffer32);
		}
		catch (...)
		{
			new(&_IB) decltype(_IB);
			throw;
		}
	}
	_Reallocate(drawDesc, verCnt, idxCnt, mode);
}

void CCoreRendererDX9::CCoreGeometryBufferDynamic::_GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const
{
	void *locked;

	AssertHR(_VB.GetVB()->Lock(_VB.GetOffset(), verticesDataSize, &locked, D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY));
	memcpy(vertexData, locked, verticesDataSize);
	AssertHR(_VB.GetVB()->Unlock());

	if (indicesDataSize)
	{
		AssertHR(_IB.GetIB()->Lock(_IB.GetOffset(), indicesDataSize, &locked, D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY));
		memcpy(indexData, locked, indicesDataSize);
		AssertHR(_IB.GetIB()->Unlock());
	}
}

void CCoreRendererDX9::CCoreGeometryBufferDynamic::_ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode)
{
	const bool old_is_points = _drawMode == CRDM_POINTS, new_is_points = drawMode == CRDM_POINTS, points_changed = old_is_points != new_is_points;

	if (points_changed)
		_VB.Reset(new_is_points);

	_VB.FillSegment(drawDesc.pData, verticesDataSize);

	if (indicesDataSize)
	{
		if (points_changed || _drawDataDesc.bIndexBuffer32 != drawDesc.bIndexBuffer32)
			_IB.Reset(new_is_points, drawDesc.bIndexBuffer32);

		_IB.FillSegment(drawDesc.pIndexBuffer, indicesDataSize);
	}
	else
	{
		_IB.~CDynamicBuffer();
		new(&_IB) decltype(_IB);
	}
}
#pragma endregion

#pragma region CCoreGeometryBufferStatic
class CCoreRendererDX9::CCoreGeometryBufferStatic final : public CCoreGeometryBufferBase
{
	IDirect3DVertexBuffer9Ptr _VB;
	IDirect3DIndexBuffer9Ptr _IB;

public:
	CCoreGeometryBufferStatic(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode);

public:
	virtual const IDirect3DVertexBuffer9Ptr &GetVB() const override { return _VB; }
	virtual const IDirect3DIndexBuffer9Ptr &GetIB() const override { return _IB; }
	virtual unsigned int SetupVB() override { return 0; }
	virtual unsigned int SetupIB() override { return 0; }

private:
	virtual void _GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const override;
	virtual void _ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode) override;

public:
	DGLE_RESULT DGLE_API GetBufferType(E_CORE_RENDERER_BUFFER_TYPE &eType) override
	{
		eType = CRBT_HARDWARE_STATIC;
		return S_OK;
	}
};

// 1 call site
inline CCoreRendererDX9::CCoreGeometryBufferStatic::CCoreGeometryBufferStatic(CCoreRendererDX9 &parent, const TDrawDataDesc &drawDesc, uint verCnt, uint idxCnt, E_CORE_RENDERER_DRAW_MODE mode) :
CGeometryProviderBase(parent, drawDesc, mode), CCoreGeometryBufferBase(parent, drawDesc, mode)
{
	_Reallocate(drawDesc, verCnt, idxCnt, mode);
}

void CCoreRendererDX9::CCoreGeometryBufferStatic::_GetGeometryDataImpl(void *vertexData, uint verticesDataSize, void *indexData, uint indicesDataSize) const
{
	void *locked;

	AssertHR(_VB->Lock(0, verticesDataSize, &locked, D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY));
	memcpy(vertexData, locked, verticesDataSize);
	AssertHR(_VB->Unlock());

	if (indicesDataSize)
	{
		AssertHR(_IB->Lock(0, indicesDataSize, &locked, D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY));
		memcpy(indexData, locked, indicesDataSize);
		AssertHR(_IB->Unlock());
	}
}

void CCoreRendererDX9::CCoreGeometryBufferStatic::_ReallocateImpl(const TDrawDataDesc &drawDesc, uint verticesDataSize, uint indicesDataSize, E_CORE_RENDERER_DRAW_MODE drawMode)
{
	void *locked;

	const DWORD old_usage = _drawMode == CRDM_POINTS ? D3DUSAGE_POINTS : 0, new_usage = drawMode == CRDM_POINTS ? D3DUSAGE_POINTS : 0;
	const bool usage_changed = old_usage != new_usage;

	auto VB = _VB;
	if (usage_changed || GetVerticesDataSize(drawDesc, _verticesCount) != verticesDataSize)
		CheckHR(_parent._device->CreateVertexBuffer(verticesDataSize, new_usage, 0, D3DPOOL_MANAGED, &VB, NULL));

	AssertHR(VB->Lock(0, verticesDataSize, &locked, D3DLOCK_READONLY));
	memcpy(locked, drawDesc.pData, verticesDataSize);
	AssertHR(VB->Unlock());

	if (indicesDataSize)
	{
		if (usage_changed || _drawDataDesc.bIndexBuffer32 != drawDesc.bIndexBuffer32 || GetIndicesDataSize(drawDesc, _indicesCount) != indicesDataSize)
		{
			decltype(_IB) IB;
			CheckHR(_parent._device->CreateIndexBuffer(indicesDataSize, new_usage, drawDesc.bIndexBuffer32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, D3DPOOL_MANAGED, &IB, NULL));
			_IB = IB;
		}

		AssertHR(_IB->Lock(0, indicesDataSize, &locked, D3DLOCK_READONLY));
		memcpy(locked, drawDesc.pIndexBuffer, indicesDataSize);
		AssertHR(_IB->Unlock());
	}

	_VB = VB;
}
#pragma endregion
#pragma endregion

#pragma region texture
namespace
{
	void (*RowCopy(bool dgle2d3d))(const void *const src, void *const dst, unsigned length);

	namespace TexFormatImpl
	{
		// TODO: redesign for constexpr
		typedef uint_fast16_t TPackedLayout;

		namespace PackedLayoutImpl
		{
			template<unsigned int idx, unsigned int ...rest>
			static constexpr TPackedLayout PackIdx = (idx & 7u) << sizeof...(rest) * 3u | PackIdx<rest...>;

			template<unsigned int idx>
			static constexpr TPackedLayout PackIdx<idx> = idx & 7u;
		}

		template<unsigned int ...layoutIdx>
		static constexpr TPackedLayout PackedLayout = sizeof...(layoutIdx) << 12u | PackedLayoutImpl::PackIdx<layoutIdx...>;

		template<TPackedLayout packedLayout>
		static constexpr auto LayoutLength = packedLayout >> 12u;

		template<TPackedLayout packedLayout, unsigned idx>
		static constexpr auto UnpackLayout = packedLayout >> (LayoutLength<packedLayout> - 1u - idx) * 3u & 7u;

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
			static constexpr unsigned length = sizeof...(FormatLayouts);

			template<unsigned idx>
			using at = typename atImpl<idx, FormatLayouts...>::type;
		};

		template<typename Format, Format inputFormat, TPackedLayout inputLayout>
		struct TFormatLayout
		{
			static constexpr Format format = inputFormat;
			static constexpr TPackedLayout layout = inputLayout;
		};

#		define DECL_FORMAT_LAYOUT(format, ...) TFormatLayout<decltype(format), format, PackedLayout<__VA_ARGS__>>

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

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned srcIdx = 0, unsigned srcSize = LayoutLength<srcLayout>>
		static constexpr unsigned FindSrcIdx = UnpackLayout<srcLayout, srcIdx> == UnpackLayout<dstLayout, dstIdx> ? srcIdx : FindSrcIdx<srcLayout, dstLayout, dstIdx, srcIdx + 1>;

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned srcSize>
		static constexpr unsigned FindSrcIdx<srcLayout, dstLayout, dstIdx, srcSize, srcSize> = ~0u;

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx = 0, unsigned dstSize = LayoutLength<dstLayout>>
		struct FillTexel
		{
			template<class TSource, class TDest>
			static inline void apply(TSource &source, TDest &dest);
		};

		template<TPackedLayout srcLayout, TPackedLayout dstLayout, unsigned dstIdx, unsigned dstSize>
		template<class TSource, class TDest>
		inline void FillTexel<srcLayout, dstLayout, dstIdx, dstSize>::apply(TSource &source, TDest &dest)
		{
			if (UnpackLayout<dstLayout, dstIdx> != 7u)
			{
				constexpr unsigned src_idx = FindSrcIdx<srcLayout, dstLayout, dstIdx>;
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
			constexpr auto src_len = LayoutLength<srcLayout>, dst_len = LayoutLength<dstLayout>;
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
		void(*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		{
			return dgle2d3d ? TransformRow<dgleFormatLayout, d3dFormatLayout> : TransformRow < d3dFormatLayout, dgleFormatLayout > ;
		}

		template<TPackedLayout dgleFormatLayout, TPackedLayout d3dFormatLayout>
		struct GetRowConvertion
		{
			static inline void(*(*apply())(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return RowConvertion<dgleFormatLayout, d3dFormatLayout>;
			};
		};

		template<TPackedLayout formatLayuot>
		struct GetRowConvertion<formatLayuot, formatLayuot>
		{
			static inline void(*(*apply())(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return RowCopy;
			};
		};

		template<unsigned idx = 0>
		struct IterateD3DRowConvertion
		{
			template<TPackedLayout dgleFormatLayout>
			static inline void(*(*apply(D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				typedef typename TD3DFormatLayoutArray::at<idx> TCurFormatLayout;
				return d3dFormat == TCurFormatLayout::format ? GetRowConvertion<dgleFormatLayout, TCurFormatLayout::layout>::apply() : IterateD3DRowConvertion<idx + 1>::template apply<dgleFormatLayout>(d3dFormat);
			}
		};

		template<>
		struct IterateD3DRowConvertion<TD3DFormatLayoutArray::length>
		{
			template<TPackedLayout dgleFormatLayout>
			static inline void(*(*apply(D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
			{
				return nullptr;
			}
		};

		template<unsigned idx = 0>
		inline void(*(*IterateDGLERowConvertion(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		{
			typedef typename TDGLEFormatLayoutArray::at<idx> TCurFormatLayout;
			return dgleFormat == TCurFormatLayout::format ? IterateD3DRowConvertion<>::apply<TCurFormatLayout::layout>(d3dFormat) : IterateDGLERowConvertion<idx + 1>(dgleFormat, d3dFormat);
		}

		/*
		workaround for VS 2013/2015/2017 bug (auto ... ->)
		TODO: try with newer version
		*/
		template<>
		//inline void (*(*IterateDGLERowConvertion<TDGLEFormatLayoutArray::length>(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat))(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
		inline auto IterateDGLERowConvertion<TDGLEFormatLayoutArray::length>(E_TEXTURE_DATA_FORMAT dgleFormat, D3DFORMAT d3dFormat) -> void(*(*)(bool dgle2d3d))(const void *const src, void *const dst, unsigned length)
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

		template<class SrcLayout, class DstLayout, unsigned dstIdx, unsigned srcIdx = 0, unsigned srcSize = size(SrcLayout::value)>
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

		template<class SrcLayout, class DstLayout, unsigned dstIdx = 0, unsigned dstSize = size(DstLayout::value)>
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
			const/*expr*/ auto src_len = size(SrcLayout::value), dst_len = size(DstLayout::value);
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
	void(*(GetRowConvertion)(bool read))(const void *const src, void *const dst, unsigned length)
	{
		return read ? ReadRow<format> : WriteRow<format>;
	}

	void CopyRow(const void *const src, void *const dst, unsigned length)
	{
		memcpy(dst, src, length);
	}

	void(*(GetRowConvertion)(bool read))(const void *const src, void *const dst, unsigned length)
	{
		return CopyRow;
	}
#endif

	bool TexFormatSupported(const IDirect3DDevice9Ptr &device, D3DFORMAT format)
	{
		D3DDISPLAYMODE mode;
		AssertHR(device->GetDisplayMode(0, &mode));
		return SUCCEEDED(d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, DEVTYPE, mode.Format, 0, D3DRTYPE_TEXTURE, format));
	}
}

#pragma region CCoreTexture
class __declspec(uuid("{356F5347-3EF8-40C5-84A4-817993A23196}")) CCoreRendererDX9::CCoreTexture final : public ICoreTexture
{
	struct CDX9TextureContainer final : public IDX9TextureContainer
	{
		IDirect3DTexture9Ptr texture;

	public:
		DGLE_RESULT DGLE_API GetObjectType(E_ENGINE_OBJECT_TYPE &eType) override
		{
			eType = EOT_TEXTURE;
			return S_OK;
		}

		DGLE_RESULT DGLE_API GetTexture(IDirect3DTexture9 *&texture) override
		{
			texture = this->texture.GetInterfacePtr();
			return S_OK;
		}

		IDGLE_BASE_IMPLEMENTATION(IDX9TextureContainer, INTERFACE_IMPL(IBaseRenderObjectContainer, INTERFACE_IMPL_END))
	} _textureContainer;
	CCoreRendererDX9 &_parent;
	CBroadcast<>::CCallbackHandle _clearCallbackHandle;
	CBroadcast<const IDirect3DDevice9Ptr &>::CCallbackHandle _restoreCallbackHandle;
	const E_TEXTURE_TYPE _type;
	const E_TEXTURE_LOAD_FLAGS _loadFlags;
	void (*(*const _RowConvertion)(bool dgle2d3d))(const void *const src, void *const dst, unsigned length);
	const unsigned int _bytesPerPixel;	// per block for compressed formats

public:
	const E_TEXTURE_DATA_FORMAT format;
	const D3DTEXTUREFILTERTYPE magFilter, minFilter, mipFilter;
	const D3DTEXTUREADDRESS addressMode;
	const DWORD anisoLevel;

private:
	const bool _mipMaps;

public:
	struct TInit
	{
		const bool is_depth;

	private:
		CCoreRendererDX9 &_parent;
		const E_TEXTURE_DATA_FORMAT _format;
		const D3DFORMAT _DXFormat;
		void (*(*const _RowConvertion)(bool dgle2d3d))(const void *const src, void *const dst, unsigned length);
		const unsigned int _bytesPerPixel;

	private:
		inline TInit(bool is_depth, CCoreRendererDX9 &parent, E_TEXTURE_DATA_FORMAT format, D3DFORMAT DXFormat,
			void (*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length), unsigned int bytesPerPixel);
		friend class CCoreTexture;
	};

	static TInit GetInit(CCoreRendererDX9 &parent, E_TEXTURE_DATA_FORMAT format);

private:
	inline bool _Compressed() const;

	struct TDataSize
	{
		unsigned int w, h, rowSize;
	};
	
	TDataSize _DataSize(unsigned int width, unsigned int height, unsigned int alignment) const, _DataSize(unsigned int lod, unsigned int alignment = 0) const;

public:
	CCoreTexture(const TInit &init, E_TEXTURE_TYPE type, const uint8_t *data, unsigned int width, unsigned int height, bool mipsPresented, E_CORE_RENDERER_DATA_ALIGNMENT dataAlignment, E_TEXTURE_LOAD_FLAGS loadFlags, DWORD anisoLevel, DGLE_RESULT &ret);
	CCoreTexture(CCoreTexture &) = delete;
	void operator =(CCoreTexture &) = delete;
	~CCoreTexture();

public:
	inline bool IsDepth() const;
	inline const IDirect3DTexture9Ptr &GetTex() const { return _textureContainer.texture; }
	void SetTex(const IDirect3DTexture9Ptr &texture);
	void SyncRT();

private:
	void _SetTex(const IDirect3DTexture9Ptr &texture);
	void _SetPixelData(const uint8_t *&data, const TDataSize &dataSize, unsigned int lod);
	void _Reallocate(const uint8_t *data, unsigned int width, unsigned int height, unsigned int mipmaps, unsigned int alignment, D3DFORMAT format);
	void _Clear();
	void _Restore(const IDirect3DDevice9Ptr &device, unsigned int width, unsigned int height, bool mipmaps, D3DFORMAT format);

public:
	DGLE_RESULT DGLE_API GetSize(uint &width, uint &height) override;
	DGLE_RESULT DGLE_API GetDepth(uint &depth) override;
	DGLE_RESULT DGLE_API GetType(E_TEXTURE_TYPE &eType) override;
	DGLE_RESULT DGLE_API GetFormat(E_TEXTURE_DATA_FORMAT &eFormat) override;
	DGLE_RESULT DGLE_API GetLoadFlags(E_TEXTURE_LOAD_FLAGS &eLoadFlags) override;
	DGLE_RESULT DGLE_API GetPixelData(uint8 *pData, uint &uiDataSize, uint uiLodLevel) override;
	DGLE_RESULT DGLE_API SetPixelData(const uint8 *pData, uint uiDataSize, uint uiLodLevel) override;
	DGLE_RESULT DGLE_API Reallocate(const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipMaps, E_TEXTURE_DATA_FORMAT eDataFormat) override;
	DGLE_RESULT DGLE_API GetBaseObject(IBaseRenderObjectContainer *&prObj) override;
	DGLE_RESULT DGLE_API Free() override;

	IDGLE_BASE_IMPLEMENTATION(ICoreTexture, INTERFACE_IMPL_END)
};

inline CCoreRendererDX9::CCoreTexture::TInit::TInit(bool is_depth, CCoreRendererDX9 &parent, E_TEXTURE_DATA_FORMAT format, D3DFORMAT DXFormat,
	void (*RowConvertion(bool dgle2d3d))(const void *const src, void *const dst, unsigned length), unsigned int bytesPerPixel) :
is_depth(is_depth), _parent(parent), _format(format), _DXFormat(DXFormat), _RowConvertion(RowConvertion), _bytesPerPixel(bytesPerPixel)
{}

auto CCoreRendererDX9::CCoreTexture::GetInit(CCoreRendererDX9 &parent, E_TEXTURE_DATA_FORMAT format) -> TInit
{
	D3DFORMAT DX_format;
	uint bytes_per_pixel;
	auto RowConvertion = RowCopy;
	bool need_format_adjust = false, is_depth = false;

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
	HRESULT hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_X8R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_X8B8G8R8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_A8B8G8R8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_A8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_A8R8G8B8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_DXT1), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_DXT5), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D24X8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D24X4S4), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D24S8), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D16), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D16_LOCKABLE), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D32), D3DPOOL_DEFAULT);
	hr = D3DXCheckTextureRequirements(_device, &width, &height, &levels, 0, &(format = D3DFMT_D32F_LOCKABLE), D3DPOOL_DEFAULT);
#endif

	switch (format)
	{
	case TDF_RGB8:
		DX_format = D3DFMT_X8B8G8R8;
		bytes_per_pixel = 3;
		need_format_adjust = true;
		break;
	case TDF_RGBA8:
		DX_format = D3DFMT_A8B8G8R8;
		bytes_per_pixel = 4;
		need_format_adjust = true;
		break;
	case TDF_ALPHA8:
		DX_format = D3DFMT_A8;
		bytes_per_pixel = 1;
		need_format_adjust = true;
		break;
	case TDF_BGR8:
		DX_format = D3DFMT_R8G8B8;
		bytes_per_pixel = 3;
		need_format_adjust = true;
		break;
	case TDF_BGRA8:
		DX_format = D3DFMT_A8R8G8B8;
		bytes_per_pixel = 4;
		need_format_adjust = true;
		break;
	case TDF_DXT1:
		DX_format = D3DFMT_DXT1;
		bytes_per_pixel = 8; // per block
		break;
	case TDF_DXT5:
		DX_format = D3DFMT_DXT5;
		bytes_per_pixel = 16; // per block
		break;
	case TDF_DEPTH_COMPONENT24:
		DX_format = D3DFMT_D24S8;
		bytes_per_pixel = 3;
		is_depth = true;
		break;
	case TDF_DEPTH_COMPONENT32:
		DX_format = D3DFMT_D32F_LOCKABLE;
		bytes_per_pixel = 4;
		is_depth = true;
		break;
	default: throw E_INVALIDARG;
	}

	if (need_format_adjust || is_depth)
	{
		if (FAILED(D3DXCheckTextureRequirements(parent._device, NULL, NULL, NULL, 0, &DX_format, is_depth ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED)))
			throw E_FAIL;
		if (need_format_adjust && !(RowConvertion = GetRowConvertion(format, DX_format)))
			throw E_FAIL;
	}

	return{ is_depth, parent, format, DX_format, RowConvertion, bytes_per_pixel };
}

inline bool CCoreRendererDX9::CCoreTexture::_Compressed() const
{
	switch (format)
	{
	case TDF_DXT1:
	case TDF_DXT5:
		return true;
	default:
		return false;
	}
}

inline bool CCoreRendererDX9::CCoreTexture::IsDepth() const
{
	switch (format)
	{
	case TDF_DEPTH_COMPONENT24:
	case TDF_DEPTH_COMPONENT32:
		return true;
	default:
		return false;
	}
}

auto CCoreRendererDX9::CCoreTexture::_DataSize(unsigned int width, unsigned int height, unsigned int alignment) const -> TDataSize
{
	if (_Compressed())
		(width += 3) /= 4, (height += 3) /= 4;

	auto row_size = width * _bytesPerPixel;
	alignment = (1u << alignment) - 1u;
	row_size += alignment - (row_size - 1u & alignment);

	return{ _RowConvertion == RowCopy ? row_size : width, height, row_size };
}

auto CCoreRendererDX9::CCoreTexture::_DataSize(unsigned int lod, unsigned int alignment) const -> TDataSize
{
	D3DSURFACE_DESC desc;
	AssertHR(GetTex()->GetLevelDesc(lod, &desc));

	return _DataSize(desc.Width, desc.Height, alignment);
}

CCoreRendererDX9::CCoreTexture::CCoreTexture(const TInit &init, E_TEXTURE_TYPE type, const uint8_t *data, unsigned int width, unsigned int height, bool mipsPresented, E_CORE_RENDERER_DATA_ALIGNMENT dataAlignment, E_TEXTURE_LOAD_FLAGS loadFlags, DWORD anisoLevel, DGLE_RESULT &ret) :
_parent(init._parent), _type(type), format(init._format), _loadFlags(loadFlags),
_RowConvertion(init._RowConvertion), _bytesPerPixel(init._bytesPerPixel), _mipMaps(loadFlags & TLF_GENERATE_MIPMAPS || mipsPresented), anisoLevel(anisoLevel),
magFilter(loadFlags & TLF_FILTERING_NONE ? D3DTEXF_POINT : D3DTEXF_LINEAR),
minFilter(loadFlags & TLF_FILTERING_NONE ? D3DTEXF_POINT : loadFlags & TLF_FILTERING_ANISOTROPIC ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR),
mipFilter(loadFlags & (TLF_FILTERING_NONE | TLF_FILTERING_BILINEAR) ? D3DTEXF_POINT : D3DTEXF_LINEAR),
addressMode(loadFlags & TLF_COORDS_CLAMP ? D3DTADDRESS_CLAMP : loadFlags & TLF_COORDS_MIRROR_REPEAT ? D3DTADDRESS_MIRROR : loadFlags & TLF_COORDS_MIRROR_CLAMP ? D3DTADDRESS_MIRRORONCE : D3DTADDRESS_WRAP)
{
	const auto alignment = [dataAlignment]() -> unsigned int
	{
		switch (dataAlignment)
		{
		case CRDA_ALIGNED_BY_1:	return 0;
		case CRDA_ALIGNED_BY_4:	return 2;
		default:
			assert(false);
			__assume(false);
		}
	} ();

	unsigned long int mipmaps = 1;
	if (mipsPresented)
	{
		_BitScanReverse(&mipmaps, max(width, height));
		mipmaps++;

		if (mipmaps > 4 && (loadFlags & TLF_DECREASE_QUALITY_MEDIUM || loadFlags & TLF_DECREASE_QUALITY_HIGH))
		{
			const unsigned int start_level = loadFlags & TLF_DECREASE_QUALITY_MEDIUM ? 1 : 2;
			mipmaps -= start_level;

			auto cur_width = width, cur_height = height;

			for (unsigned int l = 0; l < start_level; ++l)
			{
				const auto data_size = _DataSize(cur_width, cur_height, alignment);
				data += data_size.h * data_size.rowSize;

				cur_width = max(cur_width / 2u, 1u);
				cur_height = max(cur_height / 2u, 1u);
			}

			width = min(cur_width, width);
			height = min(cur_height, height);
		}
	}
	else if (loadFlags & TLF_GENERATE_MIPMAPS)
		mipmaps = 0;

	_Reallocate(data, width, height, mipmaps, alignment, init._DXFormat);

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

	if (!(_parent._textureAddressCaps & flag))
		ret = S_FALSE;
}

CCoreRendererDX9::CCoreTexture::~CCoreTexture()
{
	if (_parent._curRenderTarget == this)
		_parent.SetRenderTarget(nullptr);
	CCoreTexture *const null = nullptr;
	AssertHR(GetTex()->SetPrivateData(__uuidof(CCoreTexture), &null, sizeof null, 0));
}

void CCoreRendererDX9::CCoreTexture::SetTex(const IDirect3DTexture9Ptr &texture)
{
	_SetTex(texture);
	D3DSURFACE_DESC desc;
	AssertHR(texture->GetLevelDesc(0, &desc));
	_clearCallbackHandle = desc.Pool == D3DPOOL_MANAGED ? nullptr : _parent._clearBroadcast.AddCallback(bind(&CCoreTexture::_Clear, this));
	_restoreCallbackHandle = desc.Pool == D3DPOOL_MANAGED ? nullptr : _parent._restoreBroadcast.AddCallback(bind(&CCoreTexture::_Restore, this, placeholders::_1, desc.Width, desc.Height, texture->GetLevelCount() != 1, desc.Format));
}

void CCoreRendererDX9::CCoreTexture::SyncRT()
{
	if (IsDepth())
		return;

	assert(GetTex());
	D3DSURFACE_DESC desc;
	AssertHR(GetTex()->GetLevelDesc(0, &desc));
	if (desc.Pool == D3DPOOL_DEFAULT)
	{
		unsigned int row_size;
		switch (desc.Format)
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
		default:
			assert(false);
			__assume(false);
		}
		row_size *= desc.Width;

		IDirect3DSurface9Ptr rt_surface;
		AssertHR(GetTex()->GetSurfaceLevel(0, &rt_surface));
		const auto &lockable_surface = _parent._rendertargetCache.GetRendertarget(_parent._device, desc.Width, desc.Height, desc.Format);
		const RECT lockable_rect = { 0, 0, desc.Width, desc.Height };
		AssertHR(_parent._device->StretchRect(rt_surface, NULL, lockable_surface, &lockable_rect, D3DTEXF_NONE));

		D3DLOCKED_RECT src_locked, dst_locked;
		AssertHR(lockable_surface->LockRect(&src_locked, &lockable_rect, D3DLOCK_READONLY));
		SetTex(_parent._texturePools[true][GetTex()->GetLevelCount() != 1].GetTexture(_parent._device, { desc.Width, desc.Height, desc.Format }));
		AssertHR(GetTex()->LockRect(0, &dst_locked, NULL, 0));
		for (unsigned int row = 0; row < desc.Height; row++, (uint8_t *&)src_locked.pBits += src_locked.Pitch, (uint8_t *&)dst_locked.pBits += dst_locked.Pitch)
			memcpy(dst_locked.pBits, src_locked.pBits, row_size);
		AssertHR(lockable_surface->UnlockRect());
		AssertHR(GetTex()->UnlockRect(0));

		if (GetTex()->GetLevelCount() != 1)
			AssertHR(D3DXFilterTexture(GetTex(), NULL, D3DX_DEFAULT, D3DX_DEFAULT));

		for (decltype(_maxTexUnits) cur_stage = 0; cur_stage < _parent._maxTexUnits; cur_stage++)
		{
			IDirect3DBaseTexture9Ptr d3dTex;
			AssertHR(_parent._device->GetTexture(cur_stage, &d3dTex));

			if (!d3dTex)
				continue;

			CCoreTexture *bound_tex;
			DWORD data_size;
			if (FAILED(d3dTex->GetPrivateData(__uuidof(CCoreTexture), &bound_tex, &data_size)) || !bound_tex)
				continue;
			assert(data_size == sizeof bound_tex);

			if (bound_tex == this)
				AssertHR(_parent._device->SetTexture(cur_stage, GetTex()));
		}
	}
}

void CCoreRendererDX9::CCoreTexture::_SetTex(const IDirect3DTexture9Ptr &texture)
{
	assert(texture);
	CCoreTexture *ptr;
	if (GetTex())
		AssertHR(GetTex()->SetPrivateData(__uuidof(CCoreTexture), &(ptr = nullptr), sizeof ptr, 0));
	_textureContainer.texture = texture;
	AssertHR(GetTex()->SetPrivateData(__uuidof(CCoreTexture), &(ptr = this), sizeof ptr, 0));
}

void CCoreRendererDX9::CCoreTexture::_SetPixelData(const uint8_t *&data, const TDataSize &dataSize, unsigned int lod)
{
	const auto WriteRow = _RowConvertion(true);

	D3DLOCKED_RECT locked;
	AssertHR(GetTex()->LockRect(lod, &locked, NULL, 0));
	for (unsigned row = 0; row < dataSize.h; row++, data += dataSize.rowSize, (const uint8_t *&)locked.pBits += locked.Pitch)
		WriteRow(data, locked.pBits, dataSize.w);
	AssertHR(GetTex()->UnlockRect(lod));
}

void CCoreRendererDX9::CCoreTexture::_Reallocate(const uint8_t *data, unsigned int width, unsigned int height, unsigned int mipmaps, unsigned int alignment, D3DFORMAT format)
{
	if (width == 0 || height == 0 || !_parent.GetNSQTexSupport() && width != height)
		throw E_INVALIDARG;

	const bool non_power_of_two = __popcnt(width) != 1 || __popcnt(height) != 1;
	if (width > _parent.GetMaxTextureWidth() || height > _parent.GetMaxTextureHeight() || !_parent.GetNPOTTexSupport() && non_power_of_two)
		throw E_INVALIDARG;

	// DX does not support compressed textures with top mip level not multiple block size
	if (_Compressed() && width % 4 && height % 4)
		throw E_INVALIDARG;

	SetTex(_parent._texturePools[!IsDepth()][mipmaps != 1].GetTexture(_parent._device, { width, height, format }));

	if (!IsDepth() || this->format == TDF_DEPTH_COMPONENT32 && format == D3DFMT_D32F_LOCKABLE)
	{
		unsigned int cur_mip = 0;
		do
			_SetPixelData(data, _DataSize(cur_mip, alignment), cur_mip);
		while (++cur_mip < mipmaps);
	}
}

void CCoreRendererDX9::CCoreTexture::_Clear()
{
	if (_parent.DeviceLost() || IsDepth())
		_textureContainer.texture = NULL;
	else
	{
		try
		{
			SyncRT();
		}
		catch (const HRESULT hr)
		{
			LogWrite(_parent._engineCore, ("Failed to sync rendertarget texture while preparing to device reset (hr = " + to_string(hr) + ").").c_str(), LT_ERROR, ExtractFilename(__FILE__), __LINE__);
		}
	}
}

void CCoreRendererDX9::CCoreTexture::_Restore(const IDirect3DDevice9Ptr &device, unsigned int width, unsigned int height, bool mipmaps, D3DFORMAT format)
{
	try
	{
		_SetTex(_parent._texturePools[false][mipmaps != 1].GetTexture(device, { width, height, format }));
	}
	catch (const HRESULT hr)
	{
		LogWrite(_parent._engineCore, ("Failed to restore texture after device reset (hr = " + to_string(hr) + ").").c_str(), LT_ERROR, ExtractFilename(__FILE__), __LINE__);
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetSize(uint &width, uint &height)
{
	CHECK_DEVICE(_parent);

	D3DSURFACE_DESC desc;
	AssertHR(GetTex()->GetLevelDesc(0, &desc));
	width = desc.Width; height = desc.Height;

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetDepth(uint &depth)
{
	depth = 0;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetType(E_TEXTURE_TYPE &eType)
{
	eType = _type;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetFormat(E_TEXTURE_DATA_FORMAT &eFormat)
{
	eFormat = format;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetLoadFlags(E_TEXTURE_LOAD_FLAGS &eLoadFlags)
{
	eLoadFlags = _loadFlags;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetPixelData(uint8 *pData, uint &uiDataSize, uint uiLodLevel)
{
	try
	{
		CHECK_DEVICE(_parent);

		if (IsDepth() && (format != TDF_DEPTH_COMPONENT32 || [this]
		{
			D3DSURFACE_DESC desc;
			AssertHR(GetTex()->GetLevelDesc(0, &desc));
			return desc.Format != D3DFMT_D32F_LOCKABLE;
		}()))
			return S_FALSE;

		if (_parent._curRenderTarget == this)
			return E_ABORT;

		if (!_mipMaps && uiLodLevel != 0)
			return E_INVALIDARG;

		const auto data_size = _DataSize(uiLodLevel);

		const auto size = data_size.h * data_size.rowSize;
		if (!pData || size != uiDataSize)
		{
			uiDataSize = size;
			return S_FALSE;
		}

		SyncRT();

		const auto ReadRow = _RowConvertion(false);

		D3DLOCKED_RECT locked;
		AssertHR(GetTex()->LockRect(uiLodLevel, &locked, NULL, D3DLOCK_READONLY));
		for (unsigned row = 0; row < data_size.h; row++, pData += data_size.rowSize, (uint8_t *&)locked.pBits += locked.Pitch)
			ReadRow(locked.pBits, pData, data_size.w);
		AssertHR(GetTex()->UnlockRect(uiLodLevel));

		return S_OK;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::SetPixelData(const uint8 *pData, uint uiDataSize, uint uiLodLevel)
{
	try
	{
		CHECK_DEVICE(_parent);

		if (IsDepth() && (format != TDF_DEPTH_COMPONENT32 || [this]
		{
			D3DSURFACE_DESC desc;
			AssertHR(GetTex()->GetLevelDesc(0, &desc));
			return desc.Format != D3DFMT_D32F_LOCKABLE;
		}()))
			return S_FALSE;

		if (_parent._curRenderTarget == this)
			return E_ABORT;

		if (!_mipMaps && uiLodLevel != 0)
			return E_INVALIDARG;

		const auto data_size = _DataSize(uiLodLevel);

		if (data_size.h * data_size.rowSize != uiDataSize)
			return E_INVALIDARG;

		SyncRT();

		_SetPixelData(pData, data_size, uiLodLevel);

		return S_OK;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::Reallocate(const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipMaps, E_TEXTURE_DATA_FORMAT eDataFormat)
{
	CHECK_DEVICE(_parent);

	if (!pData || eDataFormat != format)
		return E_INVALIDARG;

	if (_parent._curRenderTarget == this)
		return E_ABORT;

	DGLE_RESULT ret = S_OK;

	if (!_parent.GetMipmapSupport() && bMipMaps)
	{
		bMipMaps = false;
		ret = S_FALSE;
	}

	unsigned long int mipmaps = 1;
	if (bMipMaps)
	{
		if (_parent._mipmapSupport && !IsDepth())
		{
			_BitScanReverse(&mipmaps, max(uiWidth, uiHeight));
			mipmaps++;
		}
		else
		{
			bMipMaps = false;
			ret = S_FALSE;
		}
	}
	else if (_mipMaps)
		mipmaps = 0;

	try
	{
		const auto old = GetTex();
		D3DSURFACE_DESC desc;
		AssertHR(old->GetLevelDesc(0, &desc));
		_Reallocate(pData, uiWidth, uiHeight, mipmaps, 0, desc.Format);
		for (unsigned stage = 0; stage < _parent._maxTexUnits; stage++)
		{
			IDirect3DBaseTexture9Ptr binded;
			AssertHR(_parent._device->GetTexture(stage, &binded));
			if (binded == old)
				AssertHR(_parent._device->SetTexture(stage, GetTex()));
		}
	}
	catch (const HRESULT hr)
	{
		return hr;
	}

	if (_mipMaps && !bMipMaps)
	{
		if (FAILED(D3DXFilterTexture(GetTex(), NULL, D3DX_DEFAULT, D3DX_DEFAULT)))
			ret = S_FALSE;
	}

	return ret;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::GetBaseObject(IBaseRenderObjectContainer *&prObj)
{
	prObj = &_textureContainer;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CCoreTexture::Free()
{
	delete this;
	return S_OK;
}
#pragma endregion
#pragma endregion

#pragma region CCoreRendererDX9
CCoreRendererDX9::CCoreRendererDX9(IEngineCore &engineCore) :
_engineCore(engineCore), _stInitResults(false)
{
	assert(d3d);
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
	if (FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, DEVTYPE, present_params.BackBufferFormat, present_params.Windowed, present_params.MultiSampleType, NULL)) ||
		FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, DEVTYPE, present_params.AutoDepthStencilFormat, present_params.Windowed, present_params.MultiSampleType, NULL)))
	{
		wnd.eMultisampling = MM_NONE;
		present_params.MultiSampleType = D3DMULTISAMPLE_NONE;
	}
	if (present_params.MultiSampleType == D3DMULTISAMPLE_NONE)
		present_params.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	return present_params;
}

void CCoreRendererDX9::_ConfigureWindow(const TEngineWindow &wnd, DGLE_RESULT &res)
{
	const DWORD style = wnd.bFullScreen ? WS_POPUP : wnd.uiFlags & EWF_ALLOW_SIZEING ? WS_OVERLAPPEDWINDOW : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	DWORD style_ex = WS_EX_APPWINDOW;

	if (wnd.uiFlags & EWF_TOPMOST)
		style_ex |= WS_EX_TOPMOST;

	TWindowHandle hwnd;
	AssertHR(_engineCore.GetWindowHandle(hwnd));

	if (SetWindowLong(hwnd, GWL_EXSTYLE, style_ex) == 0)
	{
		LOG("Can't change window styleEx.", LT_ERROR);
		res = S_FALSE;
	}

	if (SetWindowLong(hwnd, GWL_STYLE, style) == 0)
	{
		LOG("Can't change window style.", LT_ERROR);
		res = S_FALSE;
	}

	uint desktop_width = 0, desktop_height = 0;

	RECT rc = { 0, 0, wnd.uiWidth, wnd.uiHeight };
	int	 top_x = 0, top_y = 0;

	AdjustWindowRectEx(&rc, style, FALSE, style_ex);

	if (!wnd.bFullScreen)
	{
		{
			HDC desktop_dc = GetDC(GetDesktopWindow());
			desktop_width = GetDeviceCaps(desktop_dc, HORZRES);
			desktop_height = GetDeviceCaps(desktop_dc, VERTRES);
			ReleaseDC(GetDesktopWindow(), desktop_dc);
		}

		LOG("Desktop resolution: " + to_string(desktop_width) + 'X' + to_string(desktop_height), LT_INFO);

		if (IsIconic(hwnd) == FALSE && (desktop_width < (uint)(rc.right - rc.left) || desktop_height < (uint)(rc.bottom - rc.top)))
			LOG("Window rectangle is beyound screen.", LT_WARNING);

		top_x = (desktop_width - (rc.right - rc.left)) / 2,
			top_y = (desktop_height - (rc.bottom - rc.top)) / 2;

		if (top_x < 0) top_x = 0;
		if (top_y < 0) top_y = 0;
	}

	SetWindowPos(hwnd, HWND_TOP, top_x, top_y, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	//if (bSetFocus)
	{
		SetForegroundWindow(hwnd);
		SetCursorPos(top_x + (rc.right - rc.left) / 2, top_y + (rc.bottom - rc.top) / 2);
	}

#ifndef USE_CIRCULAR_BUFFER
	const auto max_graph_length = wnd.uiWidth + 1;
	if (_GPUTimeHistory.size() > max_graph_length)
		_GPUTimeHistory.erase(_GPUTimeHistory.begin(), next(_GPUTimeHistory.begin(), _GPUTimeHistory.size() - max_graph_length));
#endif
}

HRESULT CCoreRendererDX9::_BeginScene()
{
	HRESULT hr = _device->BeginScene();
	if (FAILED(hr)) return hr;
	
	_ProfilerStartFrame(hr);

	return hr;
}

void CCoreRendererDX9::_AbortProfiling()
{
	LOG("Fail to launch profiler queries. Disabling profiler...", LT_ERROR);
	AssertHR(_engineCore.ConsoleExecute(PROFILER_CMD_NAME" 0"));
	_startGPUTimeQuery = {};
	_GPUTimeFreqQuery = {};
	_ProfilerTasksApply([this](auto &task) { _ProfilerTaskAbort(task); }, _advancedProfilerTasks);
}

void CCoreRendererDX9::_AbortGPUTimeGraphProfiling()
{
	LOG("Fail to plot GPU time graph. Disabling graph...", LT_ERROR);
	AssertHR(_engineCore.ConsoleExecute((PROFILER_CMD_NAME + to_string(_profilerState - 1)).c_str()));
}

void CCoreRendererDX9::_ProfilerStartFrame(HRESULT &hr)
{
	try
	{
		if (_profilerState >= 1)
		{
			if (_eventQuerySupport)
			{
				auto query = _basicProfilerQueryPool.GetQuery<D3DQUERYTYPE_EVENT>(_device);
				query.Issue();
				_startFrameEventQueryQueue.Insert(move(query));
				_frameCPU++;

				while (_startFrameEventQueryQueue.Extract())
					_lastSec_CPU_GPU_delayHistory.push_back(_frameCPU - ++_frameGPU);
			}

			if (_GPUTimeQuerySupport)
			{
				_GetQuery<D3DQUERYTYPE_TIMESTAMPDISJOINT>(_GPUTimeFreqQuery) = _basicProfilerQueryPool.GetQuery<D3DQUERYTYPE_TIMESTAMPDISJOINT>(_device);
				_GetQuery<D3DQUERYTYPE_TIMESTAMPFREQ>(_GPUTimeFreqQuery) = _basicProfilerQueryPool.GetQuery<D3DQUERYTYPE_TIMESTAMPFREQ>(_device);
				_startGPUTimeQuery = _basicProfilerQueryPool.GetQuery<D3DQUERYTYPE_TIMESTAMP>(_device);

				_GetQuery<D3DQUERYTYPE_TIMESTAMPDISJOINT>(_GPUTimeFreqQuery).Start();
				_GetQuery<D3DQUERYTYPE_TIMESTAMPFREQ>(_GPUTimeFreqQuery).Issue();
				_startGPUTimeQuery.Issue();
			}
		}
		else
		{
			_basicProfilerQueryPool.Clear();

			_startFrameEventQueryQueue.Clear();
			_frameCPU = _frameGPU = 0;
			_lastSec_CPU_GPU_delayHistory.clear();
			_lastSec_CPU_GPU_delayHistory.shrink_to_fit();
			_CPU_GPU_delayRange.reset();

			_GPUTimeQueryQueue.Clear();
			_lastSecGPUTimeHistory.clear();
			_lastSecGPUTimeHistory.shrink_to_fit();
			_avgGPUTime.reset();
		}

		if (_profilerState > 0 && _profilerState % 2 == 0 && _GPUTimeQuerySupport)
		{
#ifdef USE_CIRCULAR_BUFFER
			TEngineWindow wnd;
			AssertHR(_engineCore.GetCurrentWindow(wnd));
			const auto max_graph_length = wnd.uiWidth + 1;
			_GPUTimeHistory.rset_capacity(max_graph_length);
#endif
		}
		else
		{
#ifdef USE_CIRCULAR_BUFFER
			_GPUTimeHistory.set_capacity(0);
#else
			_GPUTimeHistory.clear();
			_GPUTimeHistory.shrink_to_fit();
#endif
			_DestroyGPUTimeGraphVB();
			_GPUTimeGraphVBShadow.shrink_to_fit();
		}

		if (_profilerState >= 5)
			_ProfilerTasksApply([this](auto &task) { _ProfilerTaskStart(task); }, _advancedProfilerTasks);
		else
		{
			_advancedProfilerQueryPool.Clear();
			_ProfilerTasksApply([this](auto &task) { _ProfilerTaskDestroy(task); }, _advancedProfilerTasks);
		}
	}
	catch (...)
	{
		_AbortProfiling();
		hr = S_FALSE;
	}
}

void CCoreRendererDX9::_ProfilerStopFrame(HRESULT &hr)
{
	try
	{
		if (_startGPUTimeQuery)
		{
			auto stopGPUTimeQuery = _basicProfilerQueryPool.GetQuery<D3DQUERYTYPE_TIMESTAMP>(_device);
			stopGPUTimeQuery.Issue();
			_GetQuery<D3DQUERYTYPE_TIMESTAMPDISJOINT>(_GPUTimeFreqQuery).Stop();
			_GPUTimeQueryQueue.Insert(tuple_cat(make_tuple(move(_startGPUTimeQuery), move(stopGPUTimeQuery)), move(_GPUTimeFreqQuery)));
		}

		_ProfilerTasksApply([this](auto &task) { _ProfilerTaskStop(task); }, _advancedProfilerTasks);

		while (const auto GPU_time_data = _GPUTimeQueryQueue.Extract())
		{
			if (get<3>(*GPU_time_data))
				LOG("Disjoint timestamps across frame duration encountered. Skipping GPU time data for this frame.", LT_WARNING);
			else
			{
				_lastSecGPUTimeHistory.push_back((get<1>(*GPU_time_data) - get<0>(*GPU_time_data)) * 1000. / get<2>(*GPU_time_data));
				if (_profilerState > 0 && _profilerState % 2 == 0)
				{
#ifndef USE_CIRCULAR_BUFFER
					TEngineWindow wnd;
					AssertHR(_engineCore.GetCurrentWindow(wnd));
					if (_GPUTimeHistory.size() > wnd.uiWidth)
						_GPUTimeHistory.pop_front();
#endif
					_GPUTimeHistory.push_back(_lastSecGPUTimeHistory.back());
				}
			}
		}
	}
	catch (...)
	{
		_AbortProfiling();
		hr = S_FALSE;
	}
}

void CCoreRendererDX9::_DestroyGPUTimeGraphVB()
{
	if (_GPUTimeGraphVB)
	{
		AssertHR(_GPUTimeGraphVB->Free());
		_GPUTimeGraphVB = NULL;
	}
}

IRender2D &CCoreRendererDX9::_GetRender2D()
{
	IEngineSubSystem *render;
	IRender2D *render2D;
	AssertHR(_engineCore.GetSubSystem(ESS_RENDER, render));
	AssertHR(static_cast<IRender *>(render)->GetRender2D(render2D));
	return *render2D;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Initialize(TCrRndrInitResults &stResults, TEngineWindow &stWin, E_ENGINE_INIT_FLAGS &eInitFlags)
{
	if (_stInitResults)
		return E_ABORT;

	int cpu_info[4];
	__cpuid(cpu_info, 0x00000001);
	if (!(cpu_info[2] & 1 << 23))
	{
		LOG("POPCNT instruction is not supported", LT_FATAL);
		return E_FAIL;
	}

	_16bitColor = eInitFlags & EIF_FORCE_16_BIT_COLOR;

	LOG("Initializing Core Renderer...", LT_INFO);

	D3DPRESENT_PARAMETERS present_params = _GetPresentParams(stWin);
	if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, DEVTYPE, present_params.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_params, &_device)))
	{
		stResults = false;
		LOG("Can't create Direct3D Device.", LT_FATAL);
		return E_ABORT;
	}
	DGLE_RESULT res = S_OK;
#if 0
	_ConfigureWindow(stWin, res);
#elif !defined USE_CIRCULAR_BUFFER
	const auto max_graph_length = stWin.uiWidth + 1;
	if (_GPUTimeHistory.size() > max_graph_length)
		_GPUTimeHistory.erase(_GPUTimeHistory.begin(), next(_GPUTimeHistory.begin(), _GPUTimeHistory.size() - max_graph_length));
#endif
	const HRESULT hr = _BeginScene();
	AssertHR(hr);
	if (res != S_OK) res = hr;

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
	_eventQuerySupport = SUCCEEDED(_device->CreateQuery(D3DQUERYTYPE_EVENT, NULL));
	_GPUTimeQuerySupport = SUCCEEDED(_device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, NULL)) && SUCCEEDED(_device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, NULL)) && SUCCEEDED(_device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, NULL));
	_ProfilerTasksApply([this](auto &task) { _ProfilerTaskCheckSupport(task); }, _advancedProfilerTasks);

	_FFP = new CFixedFunctionPipelineDX9(*this, _device);

	try
	{
		_immediateVB = new CDynamicVB(*this, false);
		_immediatePointsVB = new CDynamicVB(*this, true);
		_immediateIB16 = new CDynamicIB(*this, false, false);
		_immediateIB32 = new CDynamicIB(*this, false, true);
		_immediatePointsIB16 = new CDynamicIB(*this, true, false);
		_immediatePointsIB32 = new CDynamicIB(*this, true, true);
	}
	catch (const HRESULT hr)
	{
		LOG("Core Renderer initialization error: fail to create dynamic vertex buffers.", LT_FATAL);
		return hr;
	}

	AssertHR(BindTexture(NULL, 0));
	AssertHR(SetBlendState(TBlendStateDesc()));
	AssertHR(SetDepthStencilState(TDepthStencilDesc()));
	AssertHR(SetRasterizerState(TRasterizerStateDesc()));

	AssertHR(SetMatrix(MatrixIdentity(), MT_TEXTURE));
	AssertHR(SetMatrix(MatrixIdentity(), MT_PROJECTION));
	AssertHR(SetMatrix(MatrixIdentity(), MT_MODELVIEW));

	AssertHR(_engineCore.AddEventListener(ET_ON_PER_SECOND_TIMER, _EventsHandler, this));
	AssertHR(_engineCore.AddEventListener(ET_ON_PROFILER_DRAW, _EventsHandler, this));

	static constexpr char help_text[] = R"(Displays Core Renderer DirectX 9 subsystems profiler. Valid values are:
- '0': disable profiler
- '1': basic statistics
- '2': basic statistics with graph
- '3': basic statistics, caches/pools statistics
- '4': basic statistics with graph, caches/pools statistics
- '5': basic statistics, caches/pools statistics, advanced statistics
- '6': basic statistics with graph, caches/pools statistics, advanced statistics)";
	AssertHR(_engineCore.ConsoleRegisterVariable(PROFILER_CMD_NAME, help_text, &_profilerState, 0, 6));
	AssertHR(_engineCore.ConsoleRegisterVariable("GPU_time_graph_alpha", "Sets GPU time graph opacity (0-255).", &_GPUTimeGraphAlpha, 0, 255));

	_stInitResults = stResults;

	LOG("Core Renderer initialized.", LT_INFO);

	return res;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Finalize()
{
	if (!_stInitResults)
		return E_ABORT;

	AssertHR(_engineCore.ConsoleUnregister(PROFILER_CMD_NAME));
	AssertHR(_engineCore.ConsoleUnregister("GPU_time_graph_alpha"));

	AssertHR(_engineCore.RemoveEventListener(ET_ON_PER_SECOND_TIMER, _EventsHandler, this));
	AssertHR(_engineCore.RemoveEventListener(ET_ON_PROFILER_DRAW, _EventsHandler, this));

	delete _FFP, _FFP = nullptr;

	delete _immediateVB, _immediateVB = nullptr;
	delete _immediatePointsVB, _immediatePointsVB = nullptr;
	delete _immediateIB16, _immediateIB16 = nullptr;
	delete _immediateIB32, _immediateIB32 = nullptr;
	delete _immediatePointsIB16, _immediatePointsIB16 = nullptr;
	delete _immediatePointsIB32, _immediatePointsIB32 = nullptr;

	_DestroyGPUTimeGraphVB();

	LOG("Core Renderer finalized.", LT_INFO);

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::AdjustMode(TEngineWindow &stNewWin)
{
	try
	{
		CHECK_DEVICE(*this);

		_clearBroadcast();
		_screenColorTarget = NULL;
		_screenDepthTarget = NULL;
		_PushStates();
		D3DPRESENT_PARAMETERS present_params = _GetPresentParams(stNewWin);
		DGLE_RESULT res = S_OK;
		if (stNewWin.bFullScreen)
			_ConfigureWindow(stNewWin, res);
		switch (_device->Reset(&present_params))
		{
		case S_OK:
			break;
		case D3DERR_DEVICELOST:
			_deviceLost = true;
			LOG("Device lost while trying to adjust mode.", LT_INFO);
			return LOST_DEVICE_RETURN_CODE;
		default:
			_deviceLost = true;
			throw E_FAIL;
		}
		if (!stNewWin.bFullScreen)
			_ConfigureWindow(stNewWin, res);
		_restoreBroadcast(_device);
		const HRESULT hr = _BeginScene();
		CheckHR(hr);
		if (res != S_OK) res = hr;
		_PopStates();
		CheckHR(_device->GetRenderTarget(0, &_screenColorTarget));
		CheckHR(_device->GetDepthStencilSurface(&_screenDepthTarget));
		return res;
	}
	catch (const HRESULT hr)
	{
		//LOG("Fail to adjust mode", LT_FATAL);
		return hr;
	}
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
	CHECK_DEVICE(*this);

	DWORD flags = 0;

	if (bColor)	flags |= D3DCLEAR_TARGET;
	if (bDepth)	flags |= D3DCLEAR_ZBUFFER;
	if (bStencil) flags |= D3DCLEAR_STENCIL;

	AssertHR(_device->Clear(0, NULL, flags, _clearColor, 1, 0));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetViewport(uint x, uint y, uint width, uint height)
{
	CHECK_DEVICE(*this);

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
	CHECK_DEVICE(*this);

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
	CHECK_DEVICE(*this);

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
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_POINTSIZE, (DWORD &)fSize));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetPointSize(float &fSize)
{
	CHECK_DEVICE(*this);

	AssertHR(_device->GetRenderState(D3DRS_POINTSIZE, (DWORD *)&fSize));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ReadFrameBuffer(uint uiX, uint uiY, uint uiWidth, uint uiHeight, uint8 *pData, uint uiDataSize, E_TEXTURE_DATA_FORMAT eDataFormat)
{
	try
	{
		CHECK_DEVICE(*this);

		IDirect3DSurface9Ptr frame_buffer;

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
		};

		if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
		{
			const auto &resolved = _rendertargetCache.GetRendertarget(_device, uiWidth, uiHeight, desc.Format);
			CheckHR(_device->StretchRect(frame_buffer, &rect, resolved, &rect, D3DTEXF_NONE));
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
			return E_INVALIDARG;
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

		AssertHR(frame_buffer->LockRect(&locked, &rect, D3DLOCK_READONLY));
		pData += (uiHeight - 1) * bytes;
		for (uint row = 0; row < uiHeight; row++, pData -= bytes, (uint8 *&)locked.pBits += locked.Pitch)
			ReadRow(locked.pBits, pData, row_length);
		AssertHR(frame_buffer->UnlockRect());

		return S_OK;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

#pragma region DynamicBuffer
#pragma region CDynamicBufferBase
inline void CCoreRendererDX9::CDynamicBufferBase::_CreateBuffer()
{
	_CreateBufferImpl();
	_offset = 0;
}

CCoreRendererDX9::CDynamicBufferBase::CDynamicBufferBase(CCoreRendererDX9 &parent, unsigned int sizeMultiplier,
	CBroadcast<>::CCallbackHandle &&clearCallbackHandle, CBroadcast<const IDirect3DDevice9Ptr &>::CCallbackHandle &&restoreCallbackHandle) :
_frameEndCallbackHandle(parent._frameEndBroadcast.AddCallback(std::bind(&CDynamicBufferBase::_OnFrameEnd, this))),
_clearCallbackHandle(move(clearCallbackHandle)), _restoreCallbackHandle(move(restoreCallbackHandle)),
_limit(_baseLimit * sizeMultiplier), _size(_baseStartSize * sizeMultiplier)
{}

CCoreRendererDX9::CDynamicBufferBase::~CDynamicBufferBase() = default;

unsigned int CCoreRendererDX9::CDynamicBufferBase::FillSegment(const void *data, unsigned int size)
{
	const auto old_offset = _offset;
	const auto lock_flags = _size - _offset >= size ? D3DLOCK_NOOVERWRITE : (_offset = 0, D3DLOCK_DISCARD);
	if (_size < size)
	{
		const auto old_size = _size;
		_size = size;
		try
		{
			_CreateBuffer();
		}
		catch (...)
		{
			_size = old_size;
			_offset = old_offset;
			throw;
		}
	}
	_FillSegmentImpl(data, size, lock_flags);
	_lastFrameSize += size;
	const unsigned int offset = _offset;
	_offset += size;
	return offset;
}

void CCoreRendererDX9::CDynamicBufferBase::_OnFrameEnd()
{
	const auto target_size = min(_lastFrameSize, _limit);
	_lastFrameSize = 0;
	if (_size < target_size)
	{
		const auto old_size = _size;
		_size = target_size;
		try
		{
			const auto old_buffer = _GetBuffer();
			const auto old_offset = _offset;
			_CreateBuffer();
			_OnGrow(old_buffer, old_offset);
		}
		catch (...)
		{
			_size = old_size;
			// do not rethrow, leave old buffer on fail
		}
	}
}

inline DWORD CCoreRendererDX9::CDynamicBufferBase::_Usage(bool points)
{
	DWORD usage = D3DUSAGE_DYNAMIC;
	if (points)
		usage |= D3DUSAGE_POINTS;
	return usage;
}
#pragma endregion

#pragma region CDynamicVB
inline void CCoreRendererDX9::CDynamicVB::_CreateBuffer(const IDirect3DDevice9Ptr &device, DWORD usage)
{
	decltype(_VB) VB;
	CheckHR(device->CreateVertexBuffer(_size, usage, 0, D3DPOOL_DEFAULT, &VB, NULL));
	_VB = VB;
}

inline void CCoreRendererDX9::CDynamicVB::_CreateBuffer(DWORD usage)
{
	assert(_VB);

	IDirect3DDevice9Ptr device;
	AssertHR(_VB->GetDevice(&device));

	_CreateBuffer(device, usage);
}

CCoreRendererDX9::CDynamicVB::CDynamicVB(CCoreRendererDX9 &parent, bool points) :
CDynamicBufferBase(parent, 4,
parent._clearBroadcast.AddCallback([this]{ _VB = NULL, _offset = 0; }),
parent._restoreBroadcast.AddCallback(bind(&CDynamicVB::_Restore, this, placeholders::_1, points)))
{
	_Restore(parent._device, points);
}

void CCoreRendererDX9::CDynamicVB::Reset(bool points)
{
	_restoreCallbackHandle = _restoreCallbackHandle.GetParent()->AddCallback(bind(&CDynamicVB::_Restore, this, placeholders::_1, points));
	_CreateBuffer(_Usage(points));
}

void CCoreRendererDX9::CDynamicVB::_Restore(const IDirect3DDevice9Ptr &device, bool points)
{
	_CreateBuffer(device, _Usage(points));
}

void CCoreRendererDX9::CDynamicVB::_CreateBufferImpl()
{
	assert(_VB);

	D3DVERTEXBUFFER_DESC desc;
	AssertHR(_VB->GetDesc(&desc));

	_CreateBuffer(desc.Usage);
}

void CCoreRendererDX9::CDynamicVB::_FillSegmentImpl(const void *data, unsigned int size, DWORD lockFlags)
{
	assert(_VB);

	void *mapped;
	AssertHR(_VB->Lock(_offset, size, &mapped, lockFlags));
	memcpy(mapped, data, size);
	AssertHR(_VB->Unlock());
}

inline auto CCoreRendererDX9::_GetImmediateVB(bool points) const -> CDynamicVB *
{
	return points ? _immediatePointsVB : _immediateVB;
}
#pragma endregion

#pragma region CDynamicIB
inline void CCoreRendererDX9::CDynamicIB::_CreateBuffer(const IDirect3DDevice9Ptr &device, DWORD usage, D3DFORMAT format)
{
	decltype(_IB) IB;
	CheckHR(device->CreateIndexBuffer(_size, usage, format, D3DPOOL_DEFAULT, &IB, NULL));
	_IB = IB;
}

inline void CCoreRendererDX9::CDynamicIB::_CreateBuffer(DWORD usage, D3DFORMAT format)
{
	assert(_IB);

	IDirect3DDevice9Ptr device;
	AssertHR(_IB->GetDevice(&device));

	_CreateBuffer(device, usage, format);
}

CCoreRendererDX9::CDynamicIB::CDynamicIB(CCoreRendererDX9 &parent, bool points, bool _32) :
CDynamicBufferBase(parent, 1,
parent._clearBroadcast.AddCallback([this]{ _IB = NULL, _offset = 0; }),
parent._restoreBroadcast.AddCallback(bind(&CDynamicIB::_Restore, this, placeholders::_1, points, _32)))
{
	_Restore(parent._device, points, _32);
}

void CCoreRendererDX9::CDynamicIB::Reset(bool points, bool _32)
{
	_restoreCallbackHandle = _restoreCallbackHandle.GetParent()->AddCallback(bind(&CDynamicIB::_Restore, this, placeholders::_1, points, _32));
	_CreateBuffer(_Usage(points), _32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16);
}

void CCoreRendererDX9::CDynamicIB::_Restore(const IDirect3DDevice9Ptr &device, bool points, bool _32)
{
	_CreateBuffer(device, _Usage(points), _32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16);
}

void CCoreRendererDX9::CDynamicIB::_CreateBufferImpl()
{
	assert(_IB);

	D3DINDEXBUFFER_DESC desc;
	AssertHR(_IB->GetDesc(&desc));

	_CreateBuffer(desc.Usage, desc.Format);
}

void CCoreRendererDX9::CDynamicIB::_FillSegmentImpl(const void *data, unsigned int size, DWORD lockFlags)
{
	assert(_IB);

	void *mapped;
	AssertHR(_IB->Lock(_offset, size, &mapped, lockFlags));
	memcpy(mapped, data, size);
	AssertHR(_IB->Unlock());
}

inline auto CCoreRendererDX9::_GetImmediateIB(bool points, bool _32) const -> CDynamicIB *
{
	return points ? _32 ? _immediatePointsIB32 : _immediatePointsIB16 : _32 ? _immediateIB32 : _immediateIB16;
}
#pragma endregion
#pragma endregion

namespace
{
	typedef D3DVERTEXELEMENT9 TVertexDecl[size(vertexElementLUT) + 2];	// +2 for position and D3DDECL_END()

	inline void SetVertexElement(TVertexDecl &elements, WORD stream, BYTE type, BYTE usage)
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
	inline void FillVertexDecl<size(vertexElementLUT)>(const TDrawDataDesc &drawDesc, TVertexDecl &elements, UINT stream)
	{
		SetVertexElement(elements, stream, drawDesc.bVertices2D ? D3DDECLTYPE_FLOAT2 : D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION);
		elements[++stream] = D3DDECL_END();
	}
}

#pragma region CVertexDeclarationCache
inline CCoreRendererDX9::CVertexDeclarationCache::tag::tag(const TDrawDataDesc &desc) : packed()
{
	_2D = desc.bVertices2D, normal = desc.uiNormalOffset != ~0, uv = desc.uiTextureVertexOffset != ~0, color = desc.uiColorOffset != ~0;
}

auto CCoreRendererDX9::CVertexDeclarationCache::GetDecl(IDirect3DDevice9 *device, const TDrawDataDesc &desc) -> const TCache::mapped_type &
{
	auto &cached = _cache[desc];
	if (!cached)
	{
		TVertexDecl elements;
		FillVertexDecl(desc, elements);
		CheckHR(device->CreateVertexDeclaration(elements, &cached));
	}
	return cached;
}
#pragma endregion

#pragma region CRendertargetCache
CCoreRendererDX9::CRendertargetCache::CRendertargetCache(CCoreRendererDX9 &parent) :
_clearCallbackHandle(parent._clearBroadcast.AddCallback([this]{ _cache.clear(); }))
{}

auto CCoreRendererDX9::CRendertargetCache::GetRendertarget(IDirect3DDevice9 *device, unsigned int width, unsigned int height, TCache::key_type format) -> const TCache::mapped_type &
{
	auto &cached = _cache[format];
	if (!cached || [=, &cached]
	{
		D3DSURFACE_DESC desc;
		AssertHR(cached->GetDesc(&desc));
		return desc.Width < width || desc.Height < height;
	}())
	{
		TCache::mapped_type created;
		CheckHR(device->CreateRenderTarget(width, height, format, D3DMULTISAMPLE_NONE, 0, TRUE, &created, NULL));
		cached = created;
	}
	return cached;
}
#pragma endregion

#pragma region image pool
#pragma region CImagePool
inline bool CCoreRendererDX9::CImagePool::TImageDesc::operator ==(const TImageDesc &src) const
{
	return width == src.width && height == src.height && format == src.format;
}

inline size_t CCoreRendererDX9::CImagePool::THash::operator ()(const TImageDesc &src) const
{
	return HashRange(src.width, src.height, src.format);
}

CCoreRendererDX9::CImagePool::CImagePool(CCoreRendererDX9 &parent, bool managed) :
_clearCallbackHandle(managed ? nullptr : parent._clearBroadcast.AddCallback([this]{ _pool.clear(); })),
_cleanCallbackHandle(parent._cleanBroadcast.AddCallback([this]
{
	auto cur_rt = _pool.begin();
	const auto end_rt = _pool.cend();
	while (cur_rt != end_rt)
	{
		if (Unused(cur_rt->second.image) && ++cur_rt->second.idleTime > _maxIdle && _pool.size() > _maxPoolSize)
			cur_rt = _pool.erase(cur_rt);
		else
			++cur_rt;
	}
}))
{}

const IUnknownPtr &CCoreRendererDX9::CImagePool::_GetImage(IDirect3DDevice9 *device, const TPool::key_type &desc)
{
	const auto range = _pool.equal_range(desc);

	// find unused
	const auto unused = find_if(range.first, range.second, [](TPool::const_reference rt)
	{
		return Unused(rt.second.image);
	});

	if (unused != range.second)
	{
		unused->second.idleTime = 0;
		return unused->second.image;
	}

	// unused not found, create new
	return _pool.emplace(desc, _CreateImage(device, desc))->second.image;
}
#pragma endregion

#pragma region CMSAARendertargetPool
inline CCoreRendererDX9::CMSAARendertargetPool::CMSAARendertargetPool(CCoreRendererDX9 &parent) : CImagePool(parent) {}

inline IDirect3DSurface9Ptr CCoreRendererDX9::CMSAARendertargetPool::GetRendertarget(IDirect3DDevice9 *device, const TPool::key_type &desc)
{
	return _GetImage(device, desc);
}

IUnknownPtr CCoreRendererDX9::CMSAARendertargetPool::_CreateImage(IDirect3DDevice9 *device, const TPool::key_type &desc) const
{
	D3DMULTISAMPLE_TYPE MSAA = D3DMULTISAMPLE_NONE;
	IDirect3DSwapChain9Ptr swap_chain;
	AssertHR(device->GetSwapChain(0, &swap_chain));
	D3DPRESENT_PARAMETERS params;
	AssertHR(swap_chain->GetPresentParameters(&params));
	MSAA = params.MultiSampleType;
	IDirect3DSurface9Ptr result;
	CheckHR(device->CreateRenderTarget(desc.width, desc.height, desc.format, MSAA, 0, FALSE, &result, NULL));
	return result;
}
#pragma endregion

#pragma region CTexturePool
inline CCoreRendererDX9::CTexturePool::CTexturePool(CCoreRendererDX9 &parent, bool managed, bool mipmaps) :
CImagePool(parent, managed), _managed(managed), _mipmaps(mipmaps)
{}

inline IDirect3DTexture9Ptr CCoreRendererDX9::CTexturePool::GetTexture(IDirect3DDevice9 *device, const TPool::key_type &desc)
{
	return _GetImage(device, desc);
}

IUnknownPtr CCoreRendererDX9::CTexturePool::_CreateImage(IDirect3DDevice9 *device, const TPool::key_type &desc) const
{
	DWORD usage = 0;
	if (!_managed)
	{
		switch (desc.format)
		{
		case D3DFMT_D15S1:
		case D3DFMT_D16:
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D24S8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D24X8:
		case D3DFMT_D32:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D32_LOCKABLE:
			usage |= D3DUSAGE_DEPTHSTENCIL;
			break;
		default:
			usage |= D3DUSAGE_RENDERTARGET;
			if (_mipmaps)
				usage |= D3DUSAGE_AUTOGENMIPMAP;
		}
	}
	IDirect3DTexture9Ptr result;
	CheckHR(device->CreateTexture(desc.width, desc.height, _mipmaps ? 0 : 1, usage, desc.format, _managed ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT, &result, NULL));
	return result;
}
#pragma endregion
#pragma endregion

#pragma region COffscreenDepth
CCoreRendererDX9::COffscreenDepth::COffscreenDepth(CCoreRendererDX9 &parent) :
_clearCallbackHandle(parent._clearBroadcast.AddCallback([this]{ _surface = NULL; }))
{}

IDirect3DSurface9Ptr CCoreRendererDX9::COffscreenDepth::Get(IDirect3DDevice9 *device, UINT width, UINT height, D3DMULTISAMPLE_TYPE MSAA)
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
		CheckHR(device->CreateDepthStencilSurface(width, height, _offscreenDepthFormat, MSAA, 0, TRUE, &_surface, NULL));

	return _surface;
}
#pragma endregion

#pragma region queries
#pragma region CQueryPool
IDirect3DQuery9Ptr CCoreRendererDX9::CQueryPool::_GetQuery(IDirect3DDevice9 *device, D3DQUERYTYPE type)
{
	// find unused
	const auto unused = find_if(_pool.cbegin(), _pool.cend(), [type](decltype(_pool)::const_reference query)
	{
		return Unused(query) && query->GetType() == type;
	});

	if (unused != _pool.cend())
		return *unused;

	// unused not found, create new
	IDirect3DQuery9Ptr query;
	CheckHR(device->CreateQuery(type, &query));
	_pool.push_back(query);
	return move(query);
}
#pragma endregion

#pragma region CQueryBase
inline void CCoreRendererDX9::CQueryBase::Start()
{
	CheckHR(_query->Issue(D3DISSUE_BEGIN));
}

inline void CCoreRendererDX9::CQueryBase::Stop()
{
	CheckHR(_query->Issue(D3DISSUE_END));
}

inline bool CCoreRendererDX9::CQueryBase::Ready() noexcept
{
	return _GetData(NULL, 0);
}

inline bool CCoreRendererDX9::CQueryBase::_GetData(void *dst, DWORD size) noexcept
{
	switch (_query->GetData(dst, size, 0))
	{
	case S_OK:		return true;
	case S_FALSE:	return false;
	default:
		// should not return error when D3DGETDATA_FLUSH is not specified according to DX9 documentation
		assert(false);
		__assume(false);
	}
}
#pragma endregion

#pragma region CQueryAccess
template<typename TResult>
inline optional<TResult> CCoreRendererDX9::CQueryAccess<TResult>::GetData() noexcept
{
	TResult result;
	const bool available = _GetData(&result, sizeof result);
	return available ? optional<TResult>(result) : nullopt;
}
#pragma endregion

#pragma region CQueryQueue
template<class Item>
void CCoreRendererDX9::CQueryQueueBase<Item>::Insert(Item &&item)
{
	_queue.push_back(move(item));
}

#ifdef MSVC_LIMITATIONS
template<typename ...Args>
static inline bool Conjunction(bool head, Args ...tail)
{
	return head && Conjunction(tail...);
}

// terminator
static inline bool Conjunction()
{
	return true;
}

template<D3DQUERYTYPE ...types>
template<size_t ...idx>
inline bool CCoreRendererDX9::CQueryQueue<types...>::_Ready(index_sequence<idx...>) noexcept
{
	return Conjunction(get<idx>(_queue.front()).Ready()...);
}
#else
template<D3DQUERYTYPE ...types>
template<size_t ...idx>
inline bool CCoreRendererDX9::CQueryQueue<types...>::_Ready(index_sequence<idx...>) noexcept
{
	return (get<idx>(_queue.front()).Ready() && ...);
}
#endif

#ifdef MSVC_LIMITATIONS
template<typename T>
static inline const T &PassThrough(const T &x)
{
	return x;
}

template<D3DQUERYTYPE ...types>
template<size_t ...idx>
inline auto CCoreRendererDX9::CQueryQueue<types...>::_GetData(index_sequence<idx...>)
{
	return make_tuple(PassThrough(get<idx>(_queue.front()).GetData()).value()...);
}
#else
template<D3DQUERYTYPE ...types>
template<size_t ...idx>
inline auto CCoreRendererDX9::CQueryQueue<types...>::_GetData(index_sequence<idx...>)
{
	return make_tuple(get<idx>(_queue.front()).GetData().value()...);
}
#endif

template<D3DQUERYTYPE ...types>
auto CCoreRendererDX9::CQueryQueue<types...>::Extract() -> optional<tuple<typename QueryTypeTraits<types>::TResult...>>
{
	if (_queue.empty())
		return nullopt;

#if WAIT_FOR_ALL_QUERIES
	const bool ready = _Ready(make_index_sequence<sizeof...(types)>());
#else
	const bool ready = get<sizeof...(types) - 1>(_queue.front()).Ready();
#endif

	if (!ready)
		return nullopt;

	const auto result = _GetData(make_index_sequence<sizeof...(types)>());
	_queue.pop_front();
	return result;
}

template<D3DQUERYTYPE type>
auto CCoreRendererDX9::CQueryQueue<type>::Extract() -> optional<typename QueryTypeTraits<type>::TResult>
{
	if (_queue.empty())
		return nullopt;

	const auto result = static_cast<CQuery<type> &>(_queue.front()).GetData();
	if (result/*.has_value()*/)
		_queue.pop_front();
	return result;
}
#pragma endregion

#ifdef MSVC_LIMITATIONS
#ifdef __cpp_lib_apply
template<typename F, D3DQUERYTYPE firstType, D3DQUERYTYPE ...restTypes>
inline void CCoreRendererDX9::_ProfilerTasksApplyImpl(F f, TProfilerTask<firstType> &head, TProfilerTask<restTypes> &...tail)
{
	f(head);
	_ProfilerTasksApplyImpl(f, tail...);
}

template<typename F, D3DQUERYTYPE ...types>
void CCoreRendererDX9::_ProfilerTasksApply(F f, ProfilerTasks<types...> &tasks)
{
	apply(_ProfilerTasksApplyImpl<F, types...>, tuple_cat(make_tuple(f), tasks));
}
#else
template<D3DQUERYTYPE firstType, D3DQUERYTYPE ...restTypes, typename F, class Tasks>
inline void CCoreRendererDX9::_ProfilerTasksApplyImpl(F f, Tasks &tasks)
{
	f(_GetProfilerTask<firstType>(tasks));
	_ProfilerTasksApplyImpl<restTypes...>(f, tasks);
}

template<typename F, D3DQUERYTYPE ...types>
void CCoreRendererDX9::_ProfilerTasksApply(F f, ProfilerTasks<types...> &tasks)
{
	_ProfilerTasksApplyImpl<types...>(f, tasks);
}
#endif
#else
template<typename F, D3DQUERYTYPE ...types>
void CCoreRendererDX9::_ProfilerTasksApply(F f, ProfilerTasks<types...> &tasks)
{
	(f(_GetProfilerTask<types>(tasks)), ...);
}
#endif

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskCheckSupport(TProfilerTask<type> &task)
{
	task.supported = SUCCEEDED(_device->CreateQuery(type, NULL));
}

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskStart(TProfilerTask<type> &task)
{
	if (task.supported)
		(task.query = _advancedProfilerQueryPool.GetQuery<type>(_device)).Start();
}

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskStop(TProfilerTask<type> &task)
{
	if (task.query)
	{
		task.query.Stop();
		task.queryQueue.Insert(move(task.query));
	}
}

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskCollectResults(TProfilerTask<type> &task)
{
	decltype(task.result)::value_type accumResults = {};
	unsigned int frames = 0;
	while (const auto task_sample = task.queryQueue.Extract())
	{
		accumResults += *task_sample;
		frames++;
	}
	if (frames)
		task.result = accumResults / frames;
}

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskDestroy(TProfilerTask<type> &task)
{
	task.queryQueue.Clear();
	task.result.reset();
}

template<D3DQUERYTYPE type>
inline void CCoreRendererDX9::_ProfilerTaskAbort(TProfilerTask<type> &task)
{
	task.query = {};
}
#pragma endregion

DGLE_RESULT DGLE_API CCoreRendererDX9::SetRenderTarget(ICoreTexture *pTexture)
{
	try
	{
		CHECK_DEVICE(*this);

		if (pTexture && _curRenderTarget && FAILED(SetRenderTarget(NULL)))
			return E_ABORT;

		if (!pTexture && _curRenderTarget)
		{
			if (!_curRenderTarget->IsDepth())
			{
				IDirect3DSurface9Ptr offscreen_target;
				AssertHR(_device->GetRenderTarget(0, &offscreen_target));
				if (offscreen_target)
				{
					D3DSURFACE_DESC offscreen_desc;
					AssertHR(offscreen_target->GetDesc(&offscreen_desc));
					if (offscreen_desc.MultiSampleType != D3DMULTISAMPLE_NONE)
					{
						D3DSURFACE_DESC desc;
						AssertHR(_curRenderTarget->GetTex()->GetLevelDesc(0, &desc));
						if (desc.Pool == D3DPOOL_MANAGED)
						{
							auto &texture_pool = _texturePools[false][_curRenderTarget->GetTex()->GetLevelCount() != 1];
							_curRenderTarget->SetTex(texture_pool.GetTexture(_device, { desc.Width, desc.Height, desc.Format }));
						}
						IDirect3DSurface9Ptr resolved_surface;
						AssertHR(_curRenderTarget->GetTex()->GetSurfaceLevel(0, &resolved_surface));
						CheckHR(_device->StretchRect(offscreen_target, NULL, resolved_surface, NULL, D3DTEXF_NONE));
					}
#					if !SYNC_RT_TEX_LAZY
						_curRenderTarget->SyncRT();
#					endif
				}
				else
					throw E_FAIL;
			}
			AssertHR(_device->SetRenderTarget(0, _screenColorTarget));
			AssertHR(_device->SetDepthStencilSurface(_screenDepthTarget));
			AssertHR(_device->SetViewport(&_screenViewport));
			_curRenderTarget = nullptr;
		}
		else if (pTexture && !_curRenderTarget)
		{
			auto &texture = *static_cast<CCoreTexture *>(pTexture);
			IDirect3DSurface9Ptr depth_target, color_target;
			if (texture.IsDepth())
			{
				AssertHR(texture.GetTex()->GetSurfaceLevel(0, &depth_target));
				AssertHR(_device->SetRenderState(D3DRS_COLORWRITEENABLE, 0));
			}
			else
			{
				D3DSURFACE_DESC dst_desc;
				AssertHR(texture.GetTex()->GetLevelDesc(0, &dst_desc));
				TEngineWindow wnd;
				AssertHR(_engineCore.GetCurrentWindow(wnd));
				dst_desc.MultiSampleType = Multisample_DGLE_2_D3D(wnd.eMultisampling);
				if (FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, DEVTYPE, dst_desc.Format, !wnd.bFullScreen, dst_desc.MultiSampleType, NULL)) ||
					FAILED(d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, DEVTYPE, _offscreenDepthFormat, !wnd.bFullScreen, dst_desc.MultiSampleType, NULL)))
					dst_desc.MultiSampleType = D3DMULTISAMPLE_NONE;
				AssertHR(_device->GetRenderTarget(0, &_screenColorTarget));
				AssertHR(_device->GetDepthStencilSurface(&_screenDepthTarget));
				AssertHR(_device->GetViewport(&_screenViewport));
				if (dst_desc.MultiSampleType == D3DMULTISAMPLE_NONE)
				{
					D3DSURFACE_DESC desc;
					AssertHR(texture.GetTex()->GetLevelDesc(0, &desc));
					if (desc.Pool == D3DPOOL_MANAGED)
					{
						auto &texture_pool = _texturePools[false][texture.GetTex()->GetLevelCount() != 1];
						texture.SetTex(texture_pool.GetTexture(_device, { dst_desc.Width, dst_desc.Height, dst_desc.Format }));
					}
					AssertHR(texture.GetTex()->GetSurfaceLevel(0, &color_target));
				}
				else
					color_target = _MSAARendertargetPool.GetRendertarget(_device, { dst_desc.Width, dst_desc.Height, dst_desc.Format });
				depth_target = _offscreenDepth.Get(_device, dst_desc.Width, dst_desc.Height, dst_desc.MultiSampleType);
			}
			CheckHR(_device->SetDepthStencilSurface(depth_target));
			CheckHR(_device->SetRenderTarget(0, color_target));
			_curRenderTarget = static_cast<CCoreTexture *>(pTexture);
		}
		else
			return S_FALSE;

		_SetProjXform();

		return S_OK;
	}
	catch (const HRESULT hr)
	{
		LOG("Fail to set rendertarget", LT_FATAL);
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetRenderTarget(ICoreTexture *&prTexture)
{
	prTexture = _curRenderTarget;
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CreateTexture(ICoreTexture *&prTex, const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipmapsPresented, E_CORE_RENDERER_DATA_ALIGNMENT eDataAlignment, E_TEXTURE_DATA_FORMAT eDataFormat, E_TEXTURE_LOAD_FLAGS eLoadFlags)
{
	typedef add_lvalue_reference_t<underlying_type_t<E_TEXTURE_LOAD_FLAGS>> TLoadFlags;
	try
	{
		CHECK_DEVICE(*this);

		DGLE_RESULT ret = S_OK;

		DWORD required_anisotropy = 4;

		const auto init = CCoreTexture::GetInit(*this, eDataFormat);

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
				(TLoadFlags)eLoadFlags &= ~TLF_FILTERING_ANISOTROPIC;
				(TLoadFlags)eLoadFlags |= init.is_depth ? TLF_FILTERING_BILINEAR : TLF_FILTERING_TRILINEAR;

				(TLoadFlags)eLoadFlags &= ~TLF_ANISOTROPY_2X;
				(TLoadFlags)eLoadFlags &= ~TLF_ANISOTROPY_4X;
				(TLoadFlags)eLoadFlags &= ~TLF_ANISOTROPY_8X;
				(TLoadFlags)eLoadFlags &= ~TLF_ANISOTROPY_16X;

				ret = S_FALSE;
			}
		}

		if (eLoadFlags & TLF_FILTERING_TRILINEAR && !(eLoadFlags & TLF_GENERATE_MIPMAPS) && !bMipmapsPresented)
			(TLoadFlags)eLoadFlags |= TLF_GENERATE_MIPMAPS;

		if (eLoadFlags & TLF_COMPRESS)
			ret = S_FALSE;

		if ((!_mipmapSupport || init.is_depth) && (eLoadFlags & TLF_GENERATE_MIPMAPS || bMipmapsPresented))
		{
			(TLoadFlags)eLoadFlags &= ~TLF_GENERATE_MIPMAPS;
			bMipmapsPresented = false;
			if (eLoadFlags & TLF_FILTERING_TRILINEAR)
			{
				(TLoadFlags)eLoadFlags &= ~TLF_FILTERING_TRILINEAR;
				(TLoadFlags)eLoadFlags |= TLF_FILTERING_BILINEAR;
			}
			ret = S_FALSE;
		}

		const auto texture = new CCoreTexture(init, TT_2D, pData, uiWidth, uiHeight, bMipmapsPresented, eDataAlignment, eLoadFlags, required_anisotropy, ret);
		prTex = texture;

		if (!bMipmapsPresented && eLoadFlags & TLF_GENERATE_MIPMAPS && FAILED(D3DXFilterTexture(texture->GetTex(), NULL, D3DX_DEFAULT, D3DX_DEFAULT)))
		{
			(TLoadFlags)eLoadFlags &= ~TLF_GENERATE_MIPMAPS;
			ret = S_FALSE;
		}

		return ret;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::CreateGeometryBuffer(ICoreGeometryBuffer *&prBuffer, const TDrawDataDesc &stDrawDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode, E_CORE_RENDERER_BUFFER_TYPE eType)
{
	CHECK_DEVICE(*this);

	if (!stDrawDesc.pData || uiVerticesCount == 0 || uiIndicesCount && eMode == CRDM_POINTS)
		return E_INVALIDARG;

	try
	{
		switch (eType)
		{
		case CRBT_SOFTWARE:
			prBuffer = new CCoreGeometryBufferSoftware(*this, stDrawDesc, uiVerticesCount, uiIndicesCount, eMode);
			break;
		case CRBT_HARDWARE_DYNAMIC:
			prBuffer = new CCoreGeometryBufferDynamic(*this, stDrawDesc, uiVerticesCount, uiIndicesCount, eMode);
			break;
		case CRBT_HARDWARE_STATIC:
			prBuffer = new CCoreGeometryBufferStatic(*this, stDrawDesc, uiVerticesCount, uiIndicesCount, eMode);
			break;
		default:
			return E_INVALIDARG;
		}
	}
	catch (const HRESULT hr)
	{
		return hr;
	}

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleStateFilter(bool bEnabled)
{
	return S_FALSE;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::InvalidateStateFilter()
{
	return S_FALSE;
}

void CCoreRendererDX9::_SetProjXform()
{
	D3DSURFACE_DESC rt_desc;
	IDirect3DSurface9Ptr cur_rt;
	AssertHR(_device->GetRenderTarget(0, &cur_rt));
	AssertHR(cur_rt->GetDesc(&rt_desc));

	TMatrix4x4 patched_xform = _projXform;
	for (int i = 0; i < 4; i++)
	{
		if (_curRenderTarget)
			patched_xform._2D[i][1] = -patched_xform._2D[i][1];
		patched_xform._2D[i][2] = (patched_xform._2D[i][2] + patched_xform._2D[i][3]) * .5f;
	}
	patched_xform._2D[3][0] -= 1.f / rt_desc.Width;
	patched_xform._2D[3][1] += 1.f / rt_desc.Height;
	AssertHR(_device->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX *>(patched_xform._2D)));
}

/*
	current push/pop implementation propably very slow
	possily not all states being saved/restored
*/

#if !defined _MSC_VER || __clang__	// workaround for VS 2015
constexpr D3DRENDERSTATETYPE CCoreRendererDX9::_rederStateTypes[];
constexpr D3DSAMPLERSTATETYPE CCoreRendererDX9::_samplerStateTypes[];
constexpr D3DTEXTURESTAGESTATETYPE CCoreRendererDX9::_stageStateTypes[];
#endif

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
	cur_state.textureStates = make_unique<TTextureStates::element_type[]>(_maxTexStages);
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

#if SAVE_ALL_STATES
	typedef decltype(cur_state.clipPlanes) TClipPlanes;
	cur_state.clipPlanes = make_unique<TClipPlanes::element_type []>(_maxClipPlanes);
	for (DWORD idx = 0; idx < _maxClipPlanes; idx++)
		AssertHR(_device->GetClipPlane(idx, cur_state.clipPlanes[idx]));
#endif
#if SAVE_ALL_STATES
	AssertHR(_device->GetFVF(&cur_state.FVF));
	cur_state.NPatchMode = _device->GetNPatchMode();

	AssertHR(_device->GetVertexShader(&cur_state.VS));
	AssertHR(_device->GetPixelShader(&cur_state.PS));

	typedef decltype(cur_state.VSFloatConsts) TVSFloatConsts;
	cur_state.VSFloatConsts = make_unique<TVSFloatConsts::element_type []>(_maxVSFloatConsts);
	AssertHR(_device->GetVertexShaderConstantF(0, (float *)cur_state.VSFloatConsts.get(), _maxVSFloatConsts));
	AssertHR(_device->GetPixelShaderConstantF(0, (float *)cur_state.PSFloatConsts, size(cur_state.PSFloatConsts)));
	AssertHR(_device->GetVertexShaderConstantI(0, (int *)cur_state.VSIntConsts, size(cur_state.VSIntConsts)));
	AssertHR(_device->GetPixelShaderConstantI(0, (int *)cur_state.PSIntConsts, size(cur_state.PSIntConsts)));
	AssertHR(_device->GetVertexShaderConstantB(0, (BOOL *)cur_state.VSBoolConsts, size(cur_state.VSBoolConsts)));
	AssertHR(_device->GetPixelShaderConstantB(0, (BOOL *)cur_state.PSIntConsts, size(cur_state.PSIntConsts)));
#endif

	AssertHR(_device->GetMaterial(&cur_state.material));

	cur_state.clearColor = _clearColor;
	cur_state.lineWidth = _lineWidth;

	cur_state.selectedTexLayer = _selectedTexLayer;

	_stateStack.push(move(cur_state));

	_FFP->PushLights();
}

void CCoreRendererDX9::_PopStates()
{
	const auto &saved_state = _stateStack.top();

	for (unsigned idx = 0; idx < saved_state.renderStates.size(); idx++)
		AssertHR(_device->SetRenderState(_rederStateTypes[idx], saved_state.renderStates[idx]));

	for (DWORD stage = 0; stage < _maxTexStages; stage++)
	{
		const auto &saved_stage_states = saved_state.textureStates[stage];
		AssertHR(_device->SetTexture(stage, saved_stage_states.texture));
		for (unsigned idx = 0; idx < saved_stage_states.samplerStates.size(); idx++)
			AssertHR(_device->SetSamplerState(stage, _samplerStateTypes[idx], saved_stage_states.samplerStates[idx]));
		for (unsigned idx = 0; idx < saved_stage_states.stageStates.size(); idx++)
			AssertHR(_device->SetTextureStageState(stage, _stageStateTypes[idx], saved_stage_states.stageStates[idx]));
	}

	AssertHR(_device->SetViewport(&saved_state.viewport));
	AssertHR(_device->SetScissorRect(&saved_state.scissorRect));

#if SAVE_ALL_STATES
	for (DWORD idx = 0; idx < _maxClipPlanes; idx++)
		AssertHR(_device->SetClipPlane(idx, saved_state.clipPlanes[idx]));
	AssertHR(_device->SetFVF(saved_state.FVF));
	AssertHR(_device->SetNPatchMode(saved_state.NPatchMode));

	AssertHR(_device->SetVertexShader(saved_state.VS));
	AssertHR(_device->SetPixelShader(saved_state.PS));

	AssertHR(_device->SetVertexShaderConstantF(0, (const float *)saved_state.VSFloatConsts.get(), _maxVSFloatConsts));
	AssertHR(_device->SetPixelShaderConstantF(0, (const float *)saved_state.PSFloatConsts, size(saved_state.PSFloatConsts)));
	AssertHR(_device->SetVertexShaderConstantI(0, (const int *)saved_state.VSIntConsts, size(saved_state.VSIntConsts)));
	AssertHR(_device->SetPixelShaderConstantI(0, (const int *)saved_state.PSIntConsts, size(saved_state.PSIntConsts)));
	AssertHR(_device->SetVertexShaderConstantB(0, (const BOOL *)saved_state.VSBoolConsts, size(saved_state.VSBoolConsts)));
	AssertHR(_device->SetPixelShaderConstantB(0, (const BOOL *)saved_state.PSIntConsts, size(saved_state.PSIntConsts)));
#endif

	AssertHR(_device->SetMaterial(&saved_state.material));

	_clearColor = saved_state.clearColor;
	_lineWidth = saved_state.lineWidth;

	_selectedTexLayer = saved_state.selectedTexLayer;

	_stateStack.pop();

	_FFP->PopLights();
}

void CCoreRendererDX9::_DiscardStates()
{
	_stateStack.pop();
}

DGLE_RESULT DGLE_API CCoreRendererDX9::PushStates()
{
	CHECK_DEVICE(*this);

	typedef decltype(_bindingsStack) TBindingsStack;
	TBindingsStack::value_type cur_bindings;

	typedef decltype(cur_bindings.rendertargets) TSurfaces;
	cur_bindings.rendertargets = make_unique<TSurfaces::element_type[]>(_maxRTs);
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

	AssertHR(_device->GetDepthStencilSurface(&cur_bindings.depthStencil));

#if SAVE_ALL_STATES
	AssertHR(_device->GetIndices(&cur_bindings.IB));

	typedef decltype(cur_bindings.vertexStreams) TVertexStreams;
	cur_bindings.vertexStreams = make_unique<TVertexStreams::element_type[]>(_maxVertexStreams);
	for (DWORD idx = 0; idx < _maxVertexStreams; idx++)
	{
		AssertHR(_device->GetStreamSource(idx, &cur_bindings.vertexStreams[idx].VB, &cur_bindings.vertexStreams[idx].offset, &cur_bindings.vertexStreams[idx].stride));
		AssertHR(_device->GetStreamSourceFreq(idx, &cur_bindings.vertexStreams[idx].freq));
	}

	AssertHR(_device->GetVertexDeclaration(&cur_bindings.VBDecl));
#endif

	_bindingsStack.push(move(cur_bindings));

	_PushStates();

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::PopStates()
{
	CHECK_DEVICE(*this);

	const auto &saved_bindings = _bindingsStack.top();

	for (DWORD idx = 0; idx < _maxRTs; idx++)
		AssertHR(_device->SetRenderTarget(idx, saved_bindings.rendertargets[idx]));

	AssertHR(_device->SetDepthStencilSurface(saved_bindings.depthStencil));

#if SAVE_ALL_STATES
	AssertHR(_device->SetIndices(saved_bindings.IB));

	for (DWORD idx = 0; idx < _maxVertexStreams; idx++)
	{
		AssertHR(_device->SetStreamSource(idx, saved_bindings.vertexStreams[idx].VB, saved_bindings.vertexStreams[idx].offset, saved_bindings.vertexStreams[idx].stride));
		AssertHR(_device->SetStreamSourceFreq(idx, saved_bindings.vertexStreams[idx].freq));
	}

	AssertHR(_device->SetVertexDeclaration(saved_bindings.VBDecl));
#endif

	_bindingsStack.pop();

	_PopStates();

	return S_OK;
}

inline D3DTRANSFORMSTATETYPE CCoreRendererDX9::_MatrixType_DGLE_2_D3D(E_MATRIX_TYPE dgleType) const
{
	switch (dgleType)
	{
	case MT_MODELVIEW:	return D3DTS_WORLD;
	case MT_PROJECTION:	return D3DTS_PROJECTION;
	case MT_TEXTURE:	return D3DTRANSFORMSTATETYPE(D3DTS_TEXTURE0 + _selectedTexLayer);
	default:
		assert(false);
		__assume(false);
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetMatrix(const TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType)
{
	CHECK_DEVICE(*this);

	TMatrix4x4 xform = stMatrix;
	if (eMatType == MT_PROJECTION)
	{
		_projXform = stMatrix;
		_SetProjXform();
	}
	else
		AssertHR(_device->SetTransform(_MatrixType_DGLE_2_D3D(eMatType), reinterpret_cast<const D3DMATRIX *>(xform._2D)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetMatrix(TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType)
{
	CHECK_DEVICE(*this);

	if (eMatType == MT_PROJECTION)
		stMatrix = _projXform;
	else
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
		static_assert(is_same_v<IdxType, uint16> || is_same_v<IdxType, uint32>, "invalid IB format");
		assert(iCount > 0);
		return *max_element(IB, IB + iCount) + 1;
	}
}

template<unsigned idx>
inline void CCoreRendererDX9::_BindVB(const TDrawDataDesc &drawDesc, const IDirect3DVertexBuffer9Ptr &VB, unsigned int baseOffset, UINT stream) const
{
	const auto offset = drawDesc.*vertexElementLUT[idx].offset;
	if (offset != -1)
	{
		const auto stride = drawDesc.*vertexElementLUT[idx].stride ? drawDesc.*vertexElementLUT[idx].stride : GetVertexElementStride(vertexElementLUT[idx].type);
		AssertHR(_device->SetStreamSource(stream++, VB, offset + baseOffset, stride));
	}
	_BindVB<idx + 1>(drawDesc, VB, baseOffset, stream);
}

template<>
inline void CCoreRendererDX9::_BindVB<size(vertexElementLUT)>(const TDrawDataDesc &drawDesc, const IDirect3DVertexBuffer9Ptr &VB, unsigned int baseOffset, UINT stream) const
{
	const auto stride = drawDesc.uiVertexStride ? drawDesc.uiVertexStride : GetVertexElementStride(drawDesc.bVertices2D ? D3DDECLTYPE_FLOAT2 : D3DDECLTYPE_FLOAT3);
	AssertHR(_device->SetStreamSource(stream, VB, baseOffset, stride));
}

void CCoreRendererDX9::_Draw(CGeometryProviderBase &geom)
{
	assert(_FFP);
	const DWORD arg = geom.GetDrawDesc().uiColorOffset == ~0 && !_FFP->IsGlobalLightingEnabled() ? D3DTA_TFACTOR : D3DTA_CURRENT;
	AssertHR(_device->SetTextureStageState(0, D3DTSS_COLORARG2, arg));
	AssertHR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, arg));

	AssertHR(_device->SetVertexDeclaration(geom.GetVBDecl()));
	const auto VB_offset = geom.SetupVB();
	_BindVB(geom.GetDrawDesc(), geom.GetVB(), VB_offset);

	const auto primitive_type = PrimitiveType_DGLE_2_D3D(geom.GetDrawMode());
	const auto primitive_count = PrimitiveCount(geom.GetDrawMode(), geom.GetIndicesCount() ? geom.GetIndicesCount() : geom.GetVerticesCount());

	if (geom.GetIndicesCount())
	{
		const auto IB_offset = geom.SetupIB();
		AssertHR(_device->SetIndices(geom.GetIB()));
		//                                                                                                      byte offset -> index offset
		//                                                                                               ___________________^__________________
		//                                                                                              |                                      |
		AssertHR(_device->DrawIndexedPrimitive(primitive_type, 0, 0, geom.GetVerticesCount(), IB_offset >> geom.GetDrawDesc().bIndexBuffer32 + 1, primitive_count));
	}
	else
		AssertHR(_device->DrawPrimitive(primitive_type, 0, primitive_count));
}

DGLE_RESULT DGLE_API CCoreRendererDX9::Draw(const TDrawDataDesc &stDrawDesc, E_CORE_RENDERER_DRAW_MODE eMode, uint uiCount)
{
	try
	{
		CHECK_DEVICE(*this);

		uint v_count = uiCount, i_count = 0;
		if (stDrawDesc.pIndexBuffer)
		{
			v_count = stDrawDesc.bIndexBuffer32 ? GetVCount((const uint32 *)stDrawDesc.pIndexBuffer, uiCount) : GetVCount((const uint32 *)stDrawDesc.pIndexBuffer, uiCount);
			i_count = uiCount;
		}

		const bool points = eMode == CRDM_POINTS;
		CGeometryProvider geom(*this, stDrawDesc, eMode, v_count, i_count, _GetImmediateVB(points), stDrawDesc.pIndexBuffer ? _GetImmediateIB(points, stDrawDesc.bIndexBuffer32) : nullptr);
		_Draw(geom);

		return S_OK;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::DrawBuffer(ICoreGeometryBuffer *pBuffer)
{
	try
	{
		CHECK_DEVICE(*this);

		if (const auto geom = dynamic_cast<CGeometryProviderBase *const>(pBuffer))
		{
			_Draw(*geom);
			return S_OK;
		}

		return S_FALSE;
	}
	catch (const HRESULT hr)
	{
		return hr;
	}
}

DGLE_RESULT DGLE_API CCoreRendererDX9::SetColor(const TColor4 &stColor)
{
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_TEXTUREFACTOR, SwapRB(stColor)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetColor(TColor4 &stColor)
{
	CHECK_DEVICE(*this);

	D3DCOLOR color;
	AssertHR(_device->GetRenderState(D3DRS_TEXTUREFACTOR, &color));
	stColor = SwapRB(color);
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleBlendState(bool bEnabled)
{
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_ALPHABLENDENABLE, bEnabled));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::ToggleAlphaTestState(bool bEnabled)
{
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_ALPHATESTENABLE, bEnabled));
	return S_OK;
}

void CCoreRendererDX9::_FlipRectY(uint &y, uint height) const
{
	// avoiding D3D Get* calls here and in FlipRectY may improve performance
	IDirect3DSurface9Ptr rt;
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
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_ALPHABLENDENABLE, stState.bEnabled));
	AssertHR(_device->SetRenderState(D3DRS_SRCBLEND, BlendFactor_DGLE_2_D3D(stState.eSrcFactor)));
	AssertHR(_device->SetRenderState(D3DRS_DESTBLEND, BlendFactor_DGLE_2_D3D(stState.eDstFactor)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetBlendState(TBlendStateDesc &stState)
{
	CHECK_DEVICE(*this);

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
	CHECK_DEVICE(*this);

	if (uiTextureLayer > _maxTexUnits)
		return E_INVALIDARG;

	_selectedTexLayer = uiTextureLayer;

	const auto tex = static_cast<CCoreTexture *>(pTex);
	AssertHR(_device->SetTexture(uiTextureLayer, tex ? tex->GetTex() : nullptr));
	D3DTEXTUREOP colorop, alphaop;
	if (tex)
	{
		colorop = tex->format == TDF_ALPHA8 ? D3DTOP_SELECTARG2 : D3DTOP_MODULATE;
		alphaop = D3DTOP_MODULATE;
	}
	else
		colorop = alphaop = uiTextureLayer == 0 ? D3DTOP_SELECTARG2 : D3DTOP_DISABLE;
	AssertHR(_device->SetTextureStageState(uiTextureLayer, D3DTSS_COLOROP, colorop));
	AssertHR(_device->SetTextureStageState(uiTextureLayer, D3DTSS_ALPHAOP, alphaop));

	if (tex)
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
	CHECK_DEVICE(*this);

	if (uiTextureLayer > _maxTexUnits)
		return E_INVALIDARG;

	_selectedTexLayer = uiTextureLayer;

	IDirect3DBaseTexture9Ptr d3dTex;
	AssertHR(_device->GetTexture(uiTextureLayer, &d3dTex));

	if (!d3dTex)
		return S_OK;

	DWORD data_size;
	if (FAILED(d3dTex->GetPrivateData(__uuidof(CCoreTexture), &prTex, &data_size)) || !prTex)
		return S_FALSE;
	assert(data_size == sizeof prTex);
	prTex = reinterpret_cast<CCoreTexture *&>(prTex);

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
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_ZENABLE, stState.bDepthTestEnabled));
	AssertHR(_device->SetRenderState(D3DRS_ZWRITEENABLE, stState.bWriteToDepthBuffer));
	AssertHR(_device->SetRenderState(D3DRS_ZFUNC, CmpFunc_DGLE_2_D3D(stState.eDepthFunc)));
	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetDepthStencilState(TDepthStencilDesc &stState)
{
	CHECK_DEVICE(*this);

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
	CHECK_DEVICE(*this);

	AssertHR(_device->SetRenderState(D3DRS_ALPHATESTENABLE, stState.bAlphaTestEnabled));
	AssertHR(_device->SetRenderState(D3DRS_ALPHAFUNC, CmpFunc_DGLE_2_D3D(stState.eAlphaTestFunc)));
	AssertHR(_device->SetRenderState(D3DRS_ALPHAREF, round(stState.fAlphaTestRefValue * 255)));
	AssertHR(_device->SetRenderState(D3DRS_SCISSORTESTENABLE, stState.bScissorEnabled));
	AssertHR(_device->SetRenderState(D3DRS_FILLMODE, stState.bWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID));
	AssertHR(_device->SetRenderState(D3DRS_CULLMODE, CullMode_DGLE_2_D3D(stState.eCullMode, stState.bFrontCounterClockwise)));

	return S_OK;
}

DGLE_RESULT DGLE_API CCoreRendererDX9::GetRasterizerState(TRasterizerStateDesc &stState)
{
	CHECK_DEVICE(*this);

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
	try
	{
		CHECK_DEVICE(*this);

		_frameEndBroadcast();
		_PushStates();
		HRESULT result;
		_ProfilerStopFrame(result);
		CheckHR(_device->EndScene());
		switch (_device->Present(NULL, NULL, NULL, NULL))
		{
		case S_OK:
			break;
		case D3DERR_DEVICELOST:
			_deviceLost = true;
			LOG("Device lost while trying to present frame.", LT_INFO);
			return LOST_DEVICE_RETURN_CODE;
		default:
			throw E_FAIL;
		}
		const HRESULT hr = _BeginScene();
		CheckHR(hr);
		if (result != S_OK) result = hr;
		_DiscardStates();

		return result;
	}
	catch (const HRESULT hr)
	{
		LOG("Fail to present frame", LT_FATAL);
		return hr;
	}
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
	CHECK_DEVICE(*this);

	switch (eFeature)
	{
	case CRFT_BUILTIN_FULLSCREEN_MODE:
		bIsSupported = true;
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
		bIsSupported = true;
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
		bIsSupported = _textureAddressCaps & D3DPTADDRESSCAPS_MIRRORONCE;
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

template<>
inline void CCoreRendererDX9::_HandleEvent<ET_ON_PER_SECOND_TIMER>(IBaseEvent *pEvent)
{
	if (_deviceLost)
	{
		bool fail = false;
		switch (_device->TestCooperativeLevel())
		{
		case D3DERR_DEVICELOST:
			break;
		case D3DERR_DEVICENOTRESET:
		{
			_clearBroadcast();
			_screenColorTarget = NULL;
			_screenDepthTarget = NULL;
			TEngineWindow wnd;
			AssertHR(_engineCore.GetCurrentWindow(wnd));
			D3DPRESENT_PARAMETERS present_params = _GetPresentParams(wnd);
			DGLE_RESULT res;
			if (wnd.bFullScreen)
				_ConfigureWindow(wnd, res);
			switch (_device->Reset(&present_params))
			{
			case S_OK:
				if (!wnd.bFullScreen)
					_ConfigureWindow(wnd, res);
				_restoreBroadcast(_device);
				fail |= FAILED(_BeginScene());
				_PopStates();
				fail |= FAILED(_device->GetRenderTarget(0, &_screenColorTarget));
				fail |= FAILED(_device->GetDepthStencilSurface(&_screenDepthTarget));
				_deviceLost = false;
				if (!fail)
					LOG("Device restored back from lost state to operational one.", LT_INFO);
				break;
			case D3DERR_DEVICELOST:
				break;
			default:
				fail = true;
			}
			break;
		}
		default:
			fail = true;
		}
		if (fail)
			LOG("Fail to reset device", LT_FATAL);
	}
	else
		_cleanBroadcast();

	// update profiler data

	if (!_lastSec_CPU_GPU_delayHistory.empty())
	{
		const auto delayRange = minmax_element(_lastSec_CPU_GPU_delayHistory.cbegin(), _lastSec_CPU_GPU_delayHistory.cend());
		_CPU_GPU_delayRange.emplace(*delayRange.first, *delayRange.second);
		// every item x in delay history estimates delay in range [x-1, x] => need to decrement min delay in order to get conservative range estimation
		if (_CPU_GPU_delayRange->first > 0)
			_CPU_GPU_delayRange->first--;
		_lastSec_CPU_GPU_delayHistory.clear();
	}

	if (!_lastSecGPUTimeHistory.empty())
	{
		// consider using potentially more efficient std::reduce
		_avgGPUTime = accumulate(_lastSecGPUTimeHistory.cbegin(), _lastSecGPUTimeHistory.cend(), 0.) / _lastSecGPUTimeHistory.size();
		_lastSecGPUTimeHistory.clear();
	}

	_ProfilerTasksApply([this](auto &task) { _ProfilerTaskCollectResults(task); }, _advancedProfilerTasks);
}

template<>
inline void CCoreRendererDX9::_HandleEvent<ET_ON_PROFILER_DRAW>(IBaseEvent *pEvent)
{
	if (_profilerState >= 1)
	{
		const auto unavailable_color = ColorGray(), unsupported_color = ColorBrown();

		AssertHR(_engineCore.RenderProfilerText("===Core Renderer Profiler==="));

		if (_GPUTimeQuerySupport)
		{
			if (_avgGPUTime)
			{
				const auto color = _avgGPUTime <= 1000.f / 60.f ? ColorGreen() : _avgGPUTime <= 1000.f / 30.f ? ColorYellow() : ColorRed();
				AssertHR(_engineCore.RenderProfilerText(("GPU frame time estimation (can be affected by CPU in CPU bound scenarios)..." + to_string(*_avgGPUTime) + " ms").c_str(), color));
			}
			else
				AssertHR(_engineCore.RenderProfilerText("GPU frame time data not yet available", unavailable_color));

			if (_CPU_GPU_delayRange)
			{
				string text = "CPU-GPU delay...............................................................";
				if (_CPU_GPU_delayRange->second > 0)
					text.append(to_string(_CPU_GPU_delayRange->first)).append(1, '-').append(to_string(_CPU_GPU_delayRange->second));
				else
					text += '0';
				text += " frames";
				AssertHR(_engineCore.RenderProfilerText(text.c_str()));
			}
			else
				AssertHR(_engineCore.RenderProfilerText("CPU-GPU delay data not yet available", unavailable_color));

			try
			{
				if (_profilerState % 2 == 0 && !_GPUTimeHistory.empty())
				{
					IRender2D &render2D = _GetRender2D();

					TEngineWindow wnd;
					AssertHR(_engineCore.GetCurrentWindow(wnd));

					const auto FlipY = [shift = wnd.uiHeight - 1](float y) { return shift - y; };

					const auto max_graph_length = wnd.uiWidth + 1;
					_GPUTimeGraphVBShadow.resize(max_graph_length);
					_GPUTimeGraphVBShadow.shrink_to_fit();
					transform(_GPUTimeHistory.begin(), _GPUTimeHistory.end(), _GPUTimeGraphVBShadow.begin(), [x = 0.f, FlipY, this](float y) mutable -> decltype(_GPUTimeGraphVBShadow)::value_type
					{
						constexpr float lo = 1000.f / 60.f, hi = 1000.f / 30.f;
						float lerp_factor = y;
						lerp_factor -= lo;
						lerp_factor /= hi - lo;
						
						// D3DX converts D3DXCOLOR to D3DCOLORVALUE via struct aliasing, this invokes UB thus problems possible on compilers aggressive concerning strict aliasing rules
						const D3DCOLORVALUE color = Math::lerp(D3DXCOLOR(ColorGreen(_GPUTimeGraphAlpha).rgba), D3DXCOLOR(ColorRed(_GPUTimeGraphAlpha).rgba), clamp(lerp_factor, 0.f, 1.f));
						
						return
						{
							make_pair(TPoint2(x, FlipY(y * GPUTimeGraphScale)), color),
							make_pair(TPoint2(x++, FlipY(0.f)), color)
						};
					});

					TDrawDataDesc VB_data_desc;
					VB_data_desc.pData = (uint8 *)_GPUTimeGraphVBShadow.data();
					VB_data_desc.bVertices2D = true;
					VB_data_desc.uiVertexStride = VB_data_desc.uiColorStride = sizeof(decltype(_GPUTimeGraphVBShadow)::value_type::value_type);
					VB_data_desc.uiColorOffset = sizeof(decltype(_GPUTimeGraphVBShadow)::value_type::value_type::first_type);
					const uint vcount = _GPUTimeHistory.size() * 2;
					if (_GPUTimeGraphVB)
						CheckHR(_GPUTimeGraphVB->Reallocate(VB_data_desc, vcount, 0, CRDM_TRIANGLE_STRIP));
					else
						CheckHR(CreateGeometryBuffer(_GPUTimeGraphVB, VB_data_desc, vcount, 0, CRDM_TRIANGLE_STRIP, CRBT_HARDWARE_DYNAMIC));
					_GPUTimeGraphVBShadow.clear();

					AssertHR(render2D.DrawBuffer(NULL, _GPUTimeGraphVB, {}, EF_BLEND));
				}
			}
			catch (...)
			{
				_AbortGPUTimeGraphProfiling();
			}
		}
		else
			AssertHR(_engineCore.RenderProfilerText("GPU frame time query is not supported by GPU/driver", unsupported_color));

		if (_profilerState >= 3)
		{
			AssertHR(_engineCore.RenderProfilerText("-------Caches / Pools-------"));

			auto RenderProfilerText = [this, text = string()](const char prefix[], auto x, const char postfix[]) mutable
			{
				text = prefix + to_string(x) + postfix;
				if (x != 1) text += 's';
				AssertHR(_engineCore.RenderProfilerText(text.c_str()));
			};

			RenderProfilerText("Vertex Declaration Cache...", _VBDeclCache.Size(), " declaration");
			RenderProfilerText("Render Traget Cache........", _rendertargetCache.Size(), " rendertarget");
			RenderProfilerText("MSAA Render Target Pool....", _MSAARendertargetPool.Size(), " rendertarget");
			AssertHR(_engineCore.RenderProfilerText("Texture Pools:"));
			RenderProfilerText("- Unmanaged................", _texturePools[false][false].Size(), " texture");
			RenderProfilerText("- Unmanaged Mipmapped......", _texturePools[false][true].Size(), " texture");
			RenderProfilerText("- Managed..................", _texturePools[true][false].Size(), " texture");
			RenderProfilerText("- Managed Mipmapped........", _texturePools[true][true].Size(), " texture");

			AssertHR(_engineCore.RenderProfilerText("----------------------------"));

			if (_profilerState >= 5)
			{
				const auto &interface_timings_task = _GetProfilerTask<D3DQUERYTYPE_INTERFACETIMINGS>(_advancedProfilerTasks);
				if (interface_timings_task.supported)
				{
					if (interface_timings_task.result)
					{
						AssertHR(_engineCore.RenderProfilerText("---Interface Timings---"));
						AssertHR(_engineCore.RenderProfilerText(("Waiting for locks....................." + to_string(interface_timings_task.result->WaitingForGPUToUseApplicationResourceTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Waiting for command buffer overflow..." + to_string(interface_timings_task.result->WaitingForGPUToAcceptMoreCommandsTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Waiting for frame latency recovery...." + to_string(interface_timings_task.result->WaitingForGPUToStayWithinLatencyTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Waiting for unpipelinable stuff......." + to_string(interface_timings_task.result->WaitingForGPUExclusiveResourceTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Waiting for other stuff..............." + to_string(interface_timings_task.result->WaitingForGPUOtherTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Interface timings data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Interface timings query is not supported by GPU/driver", unsupported_color));

				const auto &pipeline_timings_task = _GetProfilerTask<D3DQUERYTYPE_PIPELINETIMINGS>(_advancedProfilerTasks);
				if (pipeline_timings_task.supported)
				{
					if (pipeline_timings_task.result)
					{
						AssertHR(_engineCore.RenderProfilerText("---Pipeline  Timings---"));
						AssertHR(_engineCore.RenderProfilerText(("Vertex processing....................." + to_string(pipeline_timings_task.result->VertexProcessingTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Pixel processing......................" + to_string(pipeline_timings_task.result->PixelProcessingTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Other GPU processing.................." + to_string(pipeline_timings_task.result->OtherGPUProcessingTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("GPU idle.............................." + to_string(pipeline_timings_task.result->GPUIdleTimePercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Pipeline timings data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Pipeline timings query is not supported by GPU/driver", unsupported_color));

				const auto &bandwidth_timings_task = _GetProfilerTask<D3DQUERYTYPE_BANDWIDTHTIMINGS>(_advancedProfilerTasks);
				if (bandwidth_timings_task.supported)
				{
					if (bandwidth_timings_task.result)
					{
						AssertHR(_engineCore.RenderProfilerText("---Bandwidth Timings---"));
						AssertHR(_engineCore.RenderProfilerText(("CPU->GPU bandwidth used..............." + to_string(bandwidth_timings_task.result->MaxBandwidthUtilized)).c_str()));
						AssertHR(_engineCore.RenderProfilerText(("CPU->GPU upload memory used..........." + to_string(bandwidth_timings_task.result->FrontEndUploadMemoryUtilizedPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Vertex throughput used................" + to_string(bandwidth_timings_task.result->VertexRateUtilizedPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Triangle setup throughput used........" + to_string(bandwidth_timings_task.result->TriangleSetupRateUtilizedPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Fillrate used........................." + to_string(bandwidth_timings_task.result->FillRateUtilizedPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Bandwidth timings data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Bandwidth timings query is not supported by GPU/driver", unsupported_color));

				const auto &vertex_timings_task = _GetProfilerTask<D3DQUERYTYPE_VERTEXTIMINGS>(_advancedProfilerTasks);
				if (vertex_timings_task.supported)
				{
					if (vertex_timings_task.result)
					{
						AssertHR(_engineCore.RenderProfilerText("----Vertex  Timings----"));
						AssertHR(_engineCore.RenderProfilerText(("Memory accesses......................." + to_string(vertex_timings_task.result->MemoryProcessingPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Computations.........................." + to_string(vertex_timings_task.result->ComputationProcessingPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Vertex timings data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Vertex timings query is not supported by GPU/driver", unsupported_color));

				const auto &pixel_timings_task = _GetProfilerTask<D3DQUERYTYPE_PIXELTIMINGS>(_advancedProfilerTasks);
				if (pixel_timings_task.supported)
				{
					if (pixel_timings_task.result)
					{
						AssertHR(_engineCore.RenderProfilerText("-----Pixel Timings-----"));
						AssertHR(_engineCore.RenderProfilerText(("Memory accesses......................." + to_string(pixel_timings_task.result->MemoryProcessingPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText(("Computations.........................." + to_string(pixel_timings_task.result->ComputationProcessingPercent * 100.f) + '%').c_str()));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Pixel timings data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Pixel timings query is not supported by GPU/driver", unsupported_color));

				const auto &cache_utilization_task = _GetProfilerTask<D3DQUERYTYPE_CACHEUTILIZATION>(_advancedProfilerTasks);
				if (cache_utilization_task.supported)
				{
					if (cache_utilization_task.result)
					{
						const auto GetColor = [](float x)
						{
							return x >= .9f ? ColorGreen() : x >= .1f ? ColorYellow() : ColorRed();
						};

						AssertHR(_engineCore.RenderProfilerText("---Cache Utilization---"));
						AssertHR(_engineCore.RenderProfilerText(("Texture cache hit rate................" + to_string(cache_utilization_task.result->TextureCacheHitRate * 100.f) + '%').c_str(), GetColor(cache_utilization_task.result->TextureCacheHitRate)));
						AssertHR(_engineCore.RenderProfilerText(("Posttransform vertex cache hit rate..." + to_string(cache_utilization_task.result->PostTransformVertexCacheHitRate * 100.f) + '%').c_str(), GetColor(cache_utilization_task.result->PostTransformVertexCacheHitRate)));
						AssertHR(_engineCore.RenderProfilerText("-----------------------"));
					}
					else
						AssertHR(_engineCore.RenderProfilerText("Cache utilization data not yet available", unavailable_color));
				}
				else
					AssertHR(_engineCore.RenderProfilerText("Cache utilization query is not supported by GPU/driver", unsupported_color));
			}
		}
	}
}

void DGLE_API CCoreRendererDX9::_EventsHandler(void *pParameter, IBaseEvent *pEvent)
{
	E_EVENT_TYPE type;
	AssertHR(pEvent->GetEventType(type));
	const auto renderer = PTHIS(CCoreRendererDX9);
	assert(renderer);

	switch (type)
	{
	case ET_ON_PER_SECOND_TIMER:
		renderer->_HandleEvent<ET_ON_PER_SECOND_TIMER>(pEvent);
		break;
	case ET_ON_PROFILER_DRAW:
		renderer->_HandleEvent<ET_ON_PROFILER_DRAW>(pEvent);
		break;
	}
}
#pragma endregion