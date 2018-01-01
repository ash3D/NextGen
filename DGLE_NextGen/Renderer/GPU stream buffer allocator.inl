/**
\author		Alexey Shaydurov aka ASH
\date		01.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "GPU stream buffer allocator.h"

inline Renderer::Impl::GPUStreamBufferAllocatorBase::GPUStreamBufferAllocatorBase(unsigned long allocGranularity)
{
	AllocateChunk(CD3DX12_RESOURCE_DESC::Buffer(allocGranularity));
}

template<unsigned itemSize>
inline const unsigned long Renderer::Impl::GPUStreamBufferAllocator<itemSize>::allocGranularity = std::lcm(itemSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

template<unsigned itemSize>
inline Renderer::Impl::GPUStreamBufferAllocator<itemSize>::GPUStreamBufferAllocator() : GPUStreamBufferAllocatorBase(allocGranularity)
{}

template<unsigned itemSize>
inline std::pair<ID3D12Resource *, unsigned long> Renderer::Impl::GPUStreamBufferAllocator<itemSize>::Allocate(unsigned long count)
{
	return GPUStreamBufferAllocatorBase::Allocate(count, itemSize, allocGranularity);
}