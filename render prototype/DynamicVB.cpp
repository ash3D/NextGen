/**
\author		Alexey Shaydurov aka ASH
\date		11.3.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"
#include "DynamicVB.h"

using std::function;
using Microsoft::WRL::ComPtr;
using RendererImpl::CDynamicVB;

CDynamicVB::CDynamicVB(ComPtr<ID3D11Device> device, UINT minSize): _device(device), _size(minSize), _offset(0)
{
	D3D11_BUFFER_DESC desc = {_size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
	ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
}

void CDynamicVB::Draw(ComPtr<ID3D11DeviceContext> context, UINT stride, UINT vcount, function<void (void *)> fillCallback)
{
	_ASSERT(fillCallback);
	const UINT size = stride * vcount;
	if (_size < size)
	{
		D3D11_BUFFER_DESC desc = {_size = size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
		ASSERT_HR(_device->CreateBuffer(&desc, NULL, &_VB))
		_offset = 0;
	}
	const D3D11_MAP map_type = _offset + size <= _size ? D3D11_MAP_WRITE_NO_OVERWRITE : (_offset = 0, D3D11_MAP_WRITE_DISCARD);
	D3D11_MAPPED_SUBRESOURCE mapped;
	ASSERT_HR(context->Map(_VB.Get(), 0, map_type, 0, &mapped))
	fillCallback(reinterpret_cast<unsigned char *>(mapped.pData) + _offset);
	context->Unmap(_VB.Get(), 0);
	context->IASetVertexBuffers(0, 1, _VB.GetAddressOf(), &stride, &_offset);
	context->Draw(vcount, 0);
	_offset += size;
}