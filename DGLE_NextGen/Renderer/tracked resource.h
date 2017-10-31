/**
\author		Alexey Shaydurov aka ASH
\date		31.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <wrl/client.h>

struct ID3D12Resource;

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	class TrackedResource : public WRL::ComPtr<ID3D12Resource>
	{
	public:
		~TrackedResource();
	};
}