/**
\author		Alexey Shaydurov aka ASH
\date		4.10.2015 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "Interface/Renderer.h"
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

class RendererImpl::CRendererBase: public DtorImpl::CRef, public Interface::IRenderer
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
	Microsoft::WRL::ComPtr<ID3D11Device>			_device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>		_immediateContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>			_swapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	_rtView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	_zbufferView;
};