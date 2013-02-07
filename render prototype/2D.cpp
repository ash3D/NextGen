/**
\author		Alexey Shaydurov aka ASH
\date		7.2.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"
#include "2D.h"

using namespace std;
using namespace DirectX::ComPtrs;
using RendererImpl::C2D;
namespace Interface = RendererImpl::Interface;
using namespace Interface::Instances::_2D;
using DGLE2::uint;
using DGLE2::uint16;
using DGLE2::uint32;

C2D::C2D(const DXGI_MODE_DESC &modeDesc, bool multithreaded):
	_dynamicRectsAllocator(_dedicatedHeap),
	//_staticRectsAllocator(0), _dynamicRectsAllocator(0),
	//_staticEllipsesAllocator(0), _dynamicEllipsesAllocator(0),
	//_staticEllipsesAAAllocator(0), _dynamicEllipsesAAAllocator(0),
	_staticRects(_staticRectsAllocator), _dynamicRects(_dynamicRectsAllocator),
	_staticEllipses(_staticEllipsesAllocator), _dynamicEllipses(_dynamicEllipsesAllocator),
	_staticEllipsesAA(_staticEllipsesAAAllocator), _dynamicEllipsesAA(_dynamicEllipsesAAAllocator),
	_dynamic2DVBSize(0), _static2DDirty(false),
	_VBSize(64), _VBStart(0), _VCount(0), _count(0), _curLayer(~0)
{
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
	ASSERT_HR(_device->CreateInputLayout(quadDesc, extent<decltype(quadDesc)>::value, pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &_quadLayout))
	//ASSERT_HR(_device->CreateInputLayout(quadDesc, extent<decltype(quadDesc)>::value, _effect2D->GetVariableByName("Quad_VS")->AsShader()->))

	// immediate 2D
	new(&_2DVB) CDynamicVB(_device, sizeof(TQuad) * 64);
	D3D11_BUFFER_DESC desc = {sizeof(TQuad) * _VBSize, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
	ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
	D3D11_MAPPED_SUBRESOURCE mapped;
	_immediateContext->Map(_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	_mappedVB = reinterpret_cast<TQuad *>(mapped.pData);
}

#pragma region(immediate)
void C2D::DrawPoint(float x, float y, uint32 color, float size) const
{
}

void C2D::DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width) const
{
}

void C2D::DrawRect(float x, float y, float width, float height, uint32 color, Interface::Textures::ITexture2D *texture, float angle) const
{
	if (_VCount == _VBSize)
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

void C2D::DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], Interface::Textures::ITexture2D *texture, float angle) const
{
}

void C2D::DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color) const
{
}

void C2D::DrawCircle(float x, float y, float r, uint32 color) const
{
}

void C2D::DrawEllipse(float x, float y, float rx, float ry, uint32 color, bool AA, float angle) const
{
	ASSERT_HR((AA ? _ellipseAAPass : _ellipsePass)->Apply(0, _immediateContext))
	_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	_immediateContext->IASetInputLayout(_quadLayout);
	_2DVB.Draw(_immediateContext, sizeof(TQuad), 1, CQuadFiller(x, y, rx, ry, 0, angle, color));
}
#pragma endregion

#pragma region(scene)
C2D::CQuadHandle::CQuadHandle(shared_ptr<C2D> &&parent, TQuads C2D::*const container, TQuad &&quad, bool dynamic):
	_parent(move(parent)), _container(container),
	_quad(((*_parent.*_container).push_front(move(quad)), (*_parent.*_container).begin()))
{
	if (!dynamic)
		_parent->_static2DDirty = true;
}

C2D::CQuadHandle::~CQuadHandle()
{
	(*_parent.*_container).erase(_quad);
}

// 1 call site (C2D::AddRect)
inline C2D::CRectHandle::CRectHandle(
	shared_ptr<C2D> &&parent, bool dynamic, uint16 layer,
	float x, float y, float width, float height, uint32 color, float angle):
	CQuadHandle(
		move(parent),
		dynamic ? &C2D::_dynamicRects : &C2D::_staticRects,
		TQuad(x, y, width, height, layer, angle, color),
		dynamic)
{
}

// 1 call site (C2D::AddEllipse)
inline C2D::CEllipseHandle::CEllipseHandle(
	shared_ptr<C2D> &&parent, bool dynamic, uint16 layer,
	float x, float y, float rx, float ry, uint32 color, bool AA, float angle):
	CQuadHandle(
		move(parent),
		dynamic ? AA ? &C2D::_dynamicEllipsesAA : &C2D::_dynamicEllipses : AA? &C2D::_staticEllipsesAA : &C2D::_staticEllipses,
		TQuad(x, y, rx, ry, layer, angle, color),
		dynamic)
{
}

IRect *C2D::AddRect(bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle)
{
	return new CRectHandle(GetRef<C2D>(), dynamic, layer, x, y, width, height, color, angle);
}

IEllipse *C2D::AddEllipse(bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle)
{
	return new CEllipseHandle(GetRef<C2D>(), dynamic, layer, x, y, rx, ry, color, AA, angle);
}

void C2D::_DrawScene() const
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
		ASSERT_HR(_device->CreateBuffer(&VB_desc, &init_data, &_static2DVB.GetInterfacePtr()))
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
			ASSERT_HR(_device->CreateBuffer(&VB_desc, NULL, &_dynamic2DVB.GetInterfacePtr()))
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
#pragma endregion

void C2D::_NextFrame() const
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
	if (_VBSize < _count)
	{
		D3D11_BUFFER_DESC desc = {sizeof(TQuad) * (_VBSize = _count), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
		ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB.GetInterfacePtr()))
	}
	D3D11_MAPPED_SUBRESOURCE mapped;
	ASSERT_HR(_immediateContext->Map(_VB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))
	_mappedVB = reinterpret_cast<TQuad *>(mapped.pData);
	_count = 0;
	_curLayer = ~0;

	_DrawScene();
}