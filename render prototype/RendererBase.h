/**
\author		Alexey Shaydurov aka ASH
\date		25.1.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "Interface\Renderer.h"
#include "Ref.h"
#include "DisplayModes.h"

namespace RendererImpl
{
	// forward declarations
	class CRendererBase;
	class C2D;
	class CRenderer;

	namespace Interface = DGLE2::Renderer::HighLevel;
}

class RendererImpl::CRendererBase: protected DtorImpl::CRef, public Interface::IRenderer
{
protected:
	// default ctor for virtual inheritance
#ifdef MSVC_LIMITATIONS
	CRendererBase() {}
#else
	CRendererBase() = default;
#endif
	CRendererBase(HWND hwnd, const DXGI_MODE_DESC &modeDesc, bool fullscreen, bool multithreaded);
	~CRendererBase()
	{
		ASSERT_HR(_swapChain->SetFullscreenState(FALSE, NULL))
	}
protected:
	DX11::ComPtrs::ID3D11DevicePtr				_device;
	DX11::ComPtrs::ID3D11DeviceContextPtr		_immediateContext;
	DX11::ComPtrs::IDXGISwapChainPtr			_swapChain;
	DX11::ComPtrs::ID3D11Texture2DPtr			_zbuffer;
	DX11::ComPtrs::ID3D11DepthStencilViewPtr	_zbufferView;
};