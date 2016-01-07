/**
\author		Alexey Shaydurov aka ASH
\date		07.01.2016 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface/Renderer.h"
#include "RendererBase.h"
#include "Dtor.h"
#include "Win32Heap.h"
#include "DynamicVB.h"

//#define _2D_FLOAT32_TO_FLOAT16

class RendererImpl::C2D: virtual public CRendererBase
{
#pragma pack(push, 4)
	struct TQuad
	{
		TQuad() = default;
		TQuad(float x, float y, float width, float height, uint16_t layer, float angle, uint32_t color):
			pos(x, y), extents(width, height), layer(layer),
#ifdef _2D_FLOAT32_TO_FLOAT16
			angle(DirectX::PackedVector::XMConvertFloatToHalf(angle)),
#else
			angle(angle),
#endif
			color(color)
		{
		}
		bool operator <(const TQuad &quad) const noexcept
		{
			return layer < quad.layer;
		}
#ifdef _2D_FLOAT32_TO_FLOAT16
		DirectX::PackedVector::XMHALF2 pos, extents;
		uint16_t layer;
		DirectX::PackedVector::HALF angle;
		uint32_t color;
#else
		DirectX::XMFLOAT2 pos, extents;
		uint16_t layer;
		float angle;
		uint32_t color;
#endif
	};
#pragma pack(pop)

	class CQuadFiller;

protected:
	C2D(const DXGI_MODE_DESC &modeDesc);
	~C2D() = default;

public:
	virtual void DrawPoint(float x, float y, uint32_t color, float size) const override;
	virtual void DrawLine(unsigned int vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32_t colors[], bool closed, float width) const override;
	virtual void DrawRect(float x, float y, float width, float height, uint32_t color, Interface::Textures::ITexture2D *texture, float angle) const override;
	virtual void DrawRect(float x, float y, float width, float height, uint32_t (&colors)[4], Interface::Textures::ITexture2D *texture, float angle) const override;
	virtual void DrawPolygon(unsigned int vcount, _In_count_(vcount) const float coords[][2], uint32_t color) const override;
	virtual void DrawCircle(float x, float y, float r, uint32_t color) const override;
	virtual void DrawEllipse(float x, float y, float rx, float ry, uint32_t color, bool AA, float angle) const override;

	virtual Interface::Instances::_2D::IRect *AddRect(bool dynamic, uint16_t layer, float x, float y, float width, float height, uint32_t color, float angle) override;
	virtual Interface::Instances::_2D::IEllipse *AddEllipse(bool dynamic, uint16_t layer, float x, float y, float rx, float ry, uint32_t color, bool AA, float angle) override;

protected:
	void _NextFrame() const;

private:
	void _DrawScene() const;

private:
	Win32Heap::CWin32Heap _dedicatedHeap;
	Win32Heap::CWin32HeapAllocator<TQuad> _staticRectsAllocator, _staticEllipsesAllocator, _staticEllipsesAAAllocator;
	Win32Heap::CWin32HeapAllocator<TQuad> _dynamicRectsAllocator, _dynamicEllipsesAllocator, _dynamicEllipsesAAAllocator;
	//class CRect;
	//class CEllipse;
	//typedef list<CRect> TRects;
	//typedef list<CEllipse> TEllipses;

	// 2D state objects
	// temp for test
protected:
	Microsoft::WRL::ComPtr<ID3DX11Effect> _effect2D;
private:
	Microsoft::WRL::ComPtr<ID3D11InputLayout> _quadLayout;
	ID3DX11EffectPass *_rectPass, *_ellipsePass, *_ellipseAAPass;

	// immediate 2D
	mutable CDynamicVB _2DVB;
	mutable Microsoft::WRL::ComPtr<ID3D11Buffer> _VB;
	mutable TQuad *_mappedVB;
	mutable UINT _VBSize, _VBStart, _VCount;
	mutable unsigned int _count;
	mutable unsigned short _curLayer;

	// 2D scene
	typedef std::list<TQuad, Win32Heap::CWin32HeapAllocator<TQuad>> TQuads;
	class CQuadHandle;
	class CRectHandle;
	class CEllipseHandle;
	mutable TQuads _staticRects, _staticEllipses, _staticEllipsesAA;
	TQuads _dynamicRects, _dynamicEllipses, _dynamicEllipsesAA;
	mutable Microsoft::WRL::ComPtr<ID3D11Buffer> _static2DVB, _dynamic2DVB;
	mutable UINT _dynamic2DVBSize;
	mutable bool _static2DDirty;
};

class RendererImpl::C2D::CQuadFiller
{
public:
	CQuadFiller(float x, float y, float width, float height, uint16_t layer, float angle, uint32_t color):
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

class RendererImpl::C2D::CQuadHandle: DtorImpl::CDtor
{
protected:
	CQuadHandle(std::shared_ptr<C2D> &&parent, TQuads C2D::*const container, TQuad &&quad, bool dynamic);
	virtual ~CQuadHandle();
protected:
	const std::shared_ptr<C2D> _parent;
private:
	TQuads C2D::*const _container;
protected:
	const TQuads::iterator _quad;
};

class RendererImpl::C2D::CRectHandle: private CQuadHandle, public Interface::Instances::_2D::IRect
{
public:
	CRectHandle(
		std::shared_ptr<C2D> &&parent, bool dynamic, uint16_t layer,
		float x, float y, float width, float height, uint32_t color, float angle);
private:
	virtual ~CRectHandle() = default;

public:
	virtual void SetPos(float x, float y) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->pos = DirectX::PackedVector::XMHALF2(x, y);
#else
		_quad->pos.x = x;
		_quad->pos.y = y;
#endif
	}

	virtual void SetExtents(float x, float y) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->extents = DirectX::PackedVector::XMHALF2(x, y);
#else
		_quad->extents.x = x;
		_quad->extents.y = y;
#endif
	}

	virtual void SetColor(uint32_t color) override
	{
		_quad->color = color;
	}

	virtual void SetAngle(float angle) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->angle = DirectX::PackedVector::XMConvertFloatToHalf(angle);
#else
		_quad->angle = angle;
#endif
	}
};

class RendererImpl::C2D::CEllipseHandle: private CQuadHandle, public Interface::Instances::_2D::IEllipse
{
public:
	CEllipseHandle(
		std::shared_ptr<C2D> &&parent, bool dynamic, uint16_t layer,
		float x, float y, float rx, float ry, uint32_t color, bool AA, float angle);
private:
	virtual ~CEllipseHandle() = default;

public:
	virtual void SetPos(float x, float y) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->pos = DirectX::PackedVector::XMHALF2(x, y);
#else
		_quad->pos.x = x;
		_quad->pos.y = y;
#endif
	}

	virtual void SetRadii(float rx, float ry) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->extents = DirectX::PackedVector::XMHALF2(rx, ry);
#else
		_quad->extents.x = rx;
		_quad->extents.y = ry;
#endif
	}

	virtual void SetColor(uint32_t color) override
	{
		_quad->color = color;
	}

	virtual void SetAngle(float angle) override
	{
#ifdef _2D_FLOAT32_TO_FLOAT16
		_quad->angle = DirectX::PackedVector::XMConvertFloatToHalf(angle);
#else
		_quad->angle = angle;
#endif
	}
};