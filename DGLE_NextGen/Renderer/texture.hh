/**
\author		Alexey Shaydurov aka ASH
\date		06.11.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <wrl/client.h>
#include "../tracked resource.h"

struct ID3D12Resource;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		/*
			store textures in upload heap for now which means accessing it over PCIe on discrete GPUs, thouugh it is optimal for iGPUs (no redundant copy needed)
			need to develop [batched] copy (upload -> default) mechanism on dedicated DMA cmd queue (enable it for discrete GPUs only)
		*/
		class Texture
		{
			WRL::ComPtr<ID3D12Resource> tex;	// in upload heap

		protected:
			// acquire GPU lifetime tracked resource
			operator TrackedResource<ID3D12Resource>() const { return tex; }

		public:
			Texture(const wchar_t *fileName);
		};
	}

	class Texture : public Impl::Texture
	{
	public:
		using Impl::Texture::Texture;

		// hide from protected
	private:
		using Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>;
	};
}