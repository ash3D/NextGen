/**
\author		Alexey Shaydurov aka ASH
\date		15.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "tracked resource.h"

using Renderer::Impl::TrackedResource;

template<bool move>
void TrackedResource::Retire()
{
	extern void RetireResource(WRL::ComPtr<ID3D12Pageable> resource);
	typedef Resource &Copy, &&Move;
	if (*this)
		RetireResource(static_cast<std::conditional_t<move, Move, Copy>>(*this));
}

template void TrackedResource::Retire<false>();
template void TrackedResource::Retire<true>();