/**
\author		Alexey Shaydurov aka ASH
\date		11.3.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace RendererImpl
{
	class CDynamicVB
	{
		Microsoft::WRL::ComPtr<ID3D11Device> _device;
		Microsoft::WRL::ComPtr<ID3D11Buffer> _VB;
		UINT _size, _offset;
	public:
		// TODO: remove default ctor and use C++11 unrestricted union in CDynamicVB users
		CDynamicVB() noexcept {}
		CDynamicVB(Microsoft::WRL::ComPtr<ID3D11Device> device, UINT minSize);
	public:
		void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, UINT stride, UINT vcount, std::function<void (void *VB)> fillCallback);
	};
}