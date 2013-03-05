/**
\author		Alexey Shaydurov aka ASH
\date		5.3.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include <comdef.h>
#include <D3D11.h>
#include "D3dx11effect.h"

#define COM_PTR(Interface) _COM_SMARTPTR_TYPEDEF(Interface, __uuidof(Interface))

namespace DirectX
{
	namespace ComPtrs
	{
		// D3Dcommon
		COM_PTR(ID3DBlob);

		// D3D11
		COM_PTR(ID3D11DeviceChild);
		COM_PTR(ID3D11DepthStencilState);
		COM_PTR(ID3D11BlendState);
		COM_PTR(ID3D11RasterizerState);
		COM_PTR(ID3D11Resource);
		COM_PTR(ID3D11Buffer);
		COM_PTR(ID3D11Texture1D);
		COM_PTR(ID3D11Texture2D);
		COM_PTR(ID3D11Texture3D);
		COM_PTR(ID3D11View);
		COM_PTR(ID3D11ShaderResourceView);
		COM_PTR(ID3D11RenderTargetView);
		COM_PTR(ID3D11DepthStencilView);
		COM_PTR(ID3D11UnorderedAccessView);
		COM_PTR(ID3D11VertexShader);
		COM_PTR(ID3D11HullShader);
		COM_PTR(ID3D11DomainShader);
		COM_PTR(ID3D11GeometryShader);
		COM_PTR(ID3D11PixelShader);
		COM_PTR(ID3D11ComputeShader);
		COM_PTR(ID3D11InputLayout);
		COM_PTR(ID3D11SamplerState);
		COM_PTR(ID3D11Asynchronous);
		COM_PTR(ID3D11Query);
		COM_PTR(ID3D11Predicate);
		COM_PTR(ID3D11Counter);
		COM_PTR(ID3D11ClassInstance);
		COM_PTR(ID3D11ClassLinkage);
		COM_PTR(ID3D11CommandList);
		COM_PTR(ID3D11DeviceContext);
		COM_PTR(ID3D11Device);

		// DXGI
		COM_PTR(IDXGIObject);
		COM_PTR(IDXGIDeviceSubObject);
		COM_PTR(IDXGIResource);
		COM_PTR(IDXGIKeyedMutex);
		COM_PTR(IDXGISurface);
		COM_PTR(IDXGISurface1);
		COM_PTR(IDXGIAdapter);
		COM_PTR(IDXGIOutput);
		COM_PTR(IDXGISwapChain);
		COM_PTR(IDXGIFactory);
		COM_PTR(IDXGIDevice);
		COM_PTR(IDXGIFactory1);
		COM_PTR(IDXGIAdapter1);
		COM_PTR(IDXGIDevice1);

		// Effects
		_COM_SMARTPTR_TYPEDEF(ID3DX11Effect, IID_ID3DX11Effect);
	}
}