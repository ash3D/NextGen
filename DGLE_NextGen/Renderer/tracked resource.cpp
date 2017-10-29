/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "tracked resource.h"

Renderer::Impl::TrackedResource::~TrackedResource()
{
	extern void RetireResource(WRL::ComPtr<ID3D12Pageable> resource);
	if (*this)
		RetireResource(std::move(*this));
}