/**
\author		Alexey Shaydurov aka ASH
\date		10.01.2016 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "Interface/Renderer.h"
#include "RendererBase.h"
#include "2D.h"

class RendererImpl::CRenderer final: public C2D
{
	static DXGI_MODE_DESC _CreateModeDesc(UINT width, UINT height, UINT refreshRate);
	CRenderer(HWND hwnd, const DXGI_MODE_DESC &modeDesc, bool fullscreen, bool multithreaded);
public:
	CRenderer(HWND hwnd, unsigned modeIdx, bool fullscreen, bool multithreaded):
		CRenderer(hwnd, DisplayModesImpl::displayModes.GetDX11Mode(modeIdx), fullscreen, multithreaded)
	{
	}
	CRenderer(HWND hwnd, unsigned int width, unsigned int height, bool fullscreen, unsigned int refreshRate, bool multithreaded/*, format*/):
		CRenderer(hwnd, _CreateModeDesc(width, height, refreshRate), fullscreen, multithreaded)
	{
	}
private:
	~CRenderer() = default;

public:
	virtual void SetMode(unsigned int width, unsigned int height) override;
	virtual void SetMode(unsigned idx) override;
	virtual void ToggleFullscreen(bool fullscreen) override;
	virtual void NextFrame() const override;

private:
	void _SetupFrame() const;

private:
	// test
	Microsoft::WRL::ComPtr<ID3D11Buffer> _buf;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _srv, _textureView;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> _uav;
};