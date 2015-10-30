/**
\author		Alexey Shaydurov aka ASH
\date		4.10.2015 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"
#include "Renderer.h"

using RendererImpl::CRendererBase;
using RendererImpl::CRenderer;
using RendererImpl::Interface::IRenderer;
using Microsoft::WRL::ComPtr;

CRendererBase::CRendererBase(HWND hwnd, const DXGI_MODE_DESC &modeDesc, bool fullscreen, bool multithreaded)
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc =
	{
		modeDesc,
		1, 0,
		DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT, 2, hwnd,
		!fullscreen, DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	};
	UINT flags =
#ifdef NDEBUG
		0
#else
		D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
#endif
	;
	if (!multithreaded) flags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

	AssertHR(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
		&_swapChain, &_device, NULL, &_immediateContext));

	// get back buffer
	ComPtr<ID3D11Texture2D> rt;
	AssertHR(_swapChain->GetBuffer(0, __uuidof(rt), &rt));
	AssertHR(_device->CreateRenderTargetView(rt.Get(), NULL, &_rtView));

	// create Zbuffer
	const D3D11_TEXTURE2D_DESC zbuffer_desc =
	{
		swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height,
		1, 1, DXGI_FORMAT_D16_UNORM,
		swap_chain_desc.SampleDesc, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0
	};
	ComPtr<ID3D11Texture2D> zbuffer;
	AssertHR(_device->CreateTexture2D(&zbuffer_desc, NULL, &zbuffer));
	AssertHR(_device->CreateDepthStencilView(zbuffer.Get(), NULL, &_zbufferView));

	// setup viewport
	const D3D11_VIEWPORT viewport = {0, 0, modeDesc.Width, modeDesc.Height, 0, 1};
	_immediateContext->RSSetViewports(1, &viewport);

	// setup rendertargets
	_immediateContext->OMSetRenderTargets(1, _rtView.GetAddressOf(), _zbufferView.Get());
}

DXGI_MODE_DESC CRenderer::_CreateModeDesc(UINT width, UINT height, UINT refreshRate)
{
	const DXGI_MODE_DESC mode_desc =
	{
		width, height, {refreshRate, 1},
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
		DXGI_MODE_SCALING_UNSPECIFIED
	};
	return mode_desc;
}

CRenderer::CRenderer(HWND hwnd, const DXGI_MODE_DESC &modeDesc, bool fullscreen, bool multithreaded):
	CRendererBase(hwnd, modeDesc, fullscreen, multithreaded),
	C2D(modeDesc)
{
	_SetupFrame();

	// test
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = 32768*512*4;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	desc.StructureByteStride = 0;
	AssertHR(_device->CreateBuffer(&desc, NULL, &_buf));
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {DXGI_FORMAT_R32_TYPELESS, D3D11_SRV_DIMENSION_BUFFEREX};
	srv_desc.BufferEx.FirstElement = 0;
	srv_desc.BufferEx.NumElements = 32768*512;
	srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
	AssertHR(_device->CreateShaderResourceView(_buf.Get(), &srv_desc, &_srv));
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {DXGI_FORMAT_R32_TYPELESS, D3D11_UAV_DIMENSION_BUFFER};
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = 32768*512;
	uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	AssertHR(_device->CreateUnorderedAccessView(_buf.Get(), &uav_desc, &_uav));
	const D3D11_TEXTURE2D_DESC textureDesc =
	{
		4096, 4096, 1, 1,
		DXGI_FORMAT_R32G32B32_UINT,
		{1, 0},
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0, 0
	};
	ComPtr<ID3D11Texture2D> texture;
	AssertHR(_device->CreateTexture2D(&textureDesc, NULL, &texture));
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	AssertHR(_device->CreateShaderResourceView(texture.Get(), &srv_desc, &_textureView));
	_immediateContext->CSSetShaderResources(0, 1, _textureView.GetAddressOf());
}

void CRenderer::SetMode(unsigned int width, unsigned int height)
{
}

void CRenderer::SetMode(unsigned idx)
{
}

void CRenderer::ToggleFullscreen(bool fullscreen)
{
}

void CRenderer::NextFrame() const
{
	C2D::_NextFrame();

	// test
	//AssertHR(_effect2D->GetVariableByName("buff")->AsShaderResource()->SetResource(_srv))
	////AssertHR(_effect2D->GetVariableByName("rwbuff")->AsUnorderedAccessView()->SetUnorderedAccessView(_uav))
	//AssertHR(_effect2D->GetTechniqueByName("test")->GetPassByIndex(0)->Apply(0, _immediateContext))
	//_immediateContext->Dispatch(32768, 1, 1);
	//_immediateContext->Flush();

	AssertHR(_swapChain->Present(0, 0));
	_SetupFrame();
}

void CRenderer::_SetupFrame() const
{
	_immediateContext->ClearRenderTargetView(_rtView.Get(), DirectX::Colors::Black);
	_immediateContext->ClearDepthStencilView(_zbufferView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
}

extern "C" IRenderer *RendererImpl::Interface::CreateRenderer(HWND hwnd, unsigned int width, unsigned int height, bool fullscreen, unsigned int refreshRate, bool multithreaded)
{
	return new CRenderer(hwnd, width, height, fullscreen, refreshRate, multithreaded);
}