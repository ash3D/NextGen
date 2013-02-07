/**
\author		Alexey Shaydurov aka ASH
\date		7.2.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface\Renderer.h"
#include "RendererBase.h"
#include "2D.h"

class RendererImpl::CRenderer: public C2D
{
	static DXGI_MODE_DESC _CreateModeDesc(UINT width, UINT height, UINT refreshRate);
#ifdef MSVC_LIMITATIONS
	void _Init();
public:
	CRenderer(HWND hwnd, DGLE2::uint modeIdx, bool fullscreen, bool multithreaded):
		CRendererBase(hwnd, DisplayModesImpl::displayModes.GetDX11Mode(modeIdx), fullscreen, multithreaded),
		C2D(DisplayModesImpl::displayModes.GetDX11Mode(modeIdx), multithreaded)
	{
		_Init();
	}
	CRenderer(HWND hwnd, DGLE2::uint width, DGLE2::uint height, bool fullscreen, DGLE2::uint refreshRate, bool multithreaded/*, format*/):
		CRendererBase(hwnd, _CreateModeDesc(width, height, refreshRate), fullscreen, multithreaded),
		C2D(_CreateModeDesc(width, height, refreshRate), multithreaded)
	{
		_Init();
	}
#else
	CRenderer(const DXGI_MODE_DESC &modeDesc);
public:
	CRenderer(HWND hwnd, DGLE2::uint modeIdx, bool fullscreen, bool multithreaded):
		CRenderer(DisplayModesImpl::displayModes.GetDX11Mode(modeIdx))
	{
	}
	CRenderer(HWND hwnd, DGLE2::uint width, DGLE2::uint height, bool fullscreen, DGLE2::uint refreshRate, bool multithreaded/*, format*/):
		CRenderer(_CreateModeDesc(width, height, refreshRate))
	{
	}
#endif
private:
#ifdef MSVC_LIMITATIONS
	~CRenderer() {}
#else
	~CRenderer() = default;
#endif

public:
	virtual void SetMode(DGLE2::uint width, DGLE2::uint height) override;
	virtual void SetMode(DGLE2::uint idx) override;
	virtual void ToggleFullscreen(bool fullscreen) override;
	virtual void NextFrame() const override;

private:
	// test
	DirectX::ComPtrs::ID3D11BufferPtr _buf;
	DirectX::ComPtrs::ID3D11ShaderResourceViewPtr _srv, _textureView;
	DirectX::ComPtrs::ID3D11UnorderedAccessViewPtr _uav;
};