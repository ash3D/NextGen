/**
\author		Alexey Shaydurov aka ASH
\date		25.1.2013 (c)Korotkov Andrey

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
		DX11::ComPtrs::ID3D11DevicePtr _device;
		DX11::ComPtrs::ID3D11BufferPtr _VB;
		UINT _size, _offset;
	public:
		// TODO: remove default ctor and use C++11 unrestricted union in CDynamicVB users
		CDynamicVB() noexcept {}
		CDynamicVB(DX11::ComPtrs::ID3D11DevicePtr device, UINT minSize);
	public:
		void Draw(DX11::ComPtrs::ID3D11DeviceContextPtr context, UINT stride, UINT vcount, std::function<void (void *VB)> fillCallback);
	};
}