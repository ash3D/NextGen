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

inline std::pair<ID3D12Resource *, unsigned long> Renderer::Impl::GPUStreamBufferCountedAllocator::Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity)
{
	const auto result = GPUStreamBufferAllocatorBase::Allocate(count, itemSize, allocGranularity);
	// place after Allocate() for exception safety (fetch_add is noexcept => if Allocate() succeeds it will be guaranteed reflected in allocatedItemsCount)
	allocatedItemsCount.fetch_add(count, std::memory_order_relaxed);
	return result;
}

template<unsigned itemSize, bool counted>
inline const unsigned long Renderer::Impl::GPUStreamBufferAllocator<itemSize, counted>::allocGranularity = std::lcm(itemSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

template<unsigned itemSize, bool counted>
inline Renderer::Impl::GPUStreamBufferAllocator<itemSize, counted>::GPUStreamBufferAllocator() : Base(allocGranularity)
{}

template<unsigned itemSize, bool counted>
inline std::pair<ID3D12Resource *, unsigned long> Renderer::Impl::GPUStreamBufferAllocator<itemSize, counted>::Allocate(unsigned long count)
{
	return Base::Allocate(count, itemSize, allocGranularity);
}