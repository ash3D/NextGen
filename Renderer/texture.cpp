#include "stdafx.h"
#include "texture.hh"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

Impl::Texture::Texture(const wchar_t *fileName)
{
	extern ComPtr<ID3D12Device2> device;
	unique_ptr<uint8_t []> data;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	CheckHR(DirectX::LoadDDSTextureFromFile(device.Get(), fileName, tex.GetAddressOf(), data, subresources));
}