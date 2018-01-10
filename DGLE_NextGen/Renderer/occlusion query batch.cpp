/**
\author		Alexey Shaydurov aka ASH
\date		10.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "occlusion query batch.h"
#include "tracked resource.inl"
#include "align.h"

using namespace std;
using namespace Renderer::Impl::OcclusionCulling;
using Microsoft::WRL::ComPtr;

// not thread-safe, must be called sequentially in render stage order
QueryBatch::QueryBatch(unsigned long count) : count(count)
{
	extern ComPtr<ID3D12Device2> device;

	if (count)
	{
		if (const UINT64 requiredSize = count * sizeof(UINT64); fresh = !resultsPool || requiredSize > resultsPool->GetDesc().Width)
		{
			static_assert(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT % sizeof(UINT64) == 0);
			const auto newResultsSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>(requiredSize);

			const D3D12_QUERY_HEAP_DESC heapDesk = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, newResultsSize / sizeof(UINT64), 0 };
			const CD3DX12_RESOURCE_DESC resultsDesc(D3D12_RESOURCE_DIMENSION_BUFFER, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, newResultsSize, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE);
			CheckHR(device->CreateQueryHeap(&heapDesk, IID_PPV_ARGS(heapPool.ReleaseAndGetAddressOf())));
			CheckHR(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &resultsDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(resultsPool.ReleaseAndGetAddressOf())));
		}

		// tracked->untracked copy here is safe since it holds for single frame only\
		untracked used to eliminate unnecessary overhead of retirement every frame
		batchHeap = heapPool.Get();
		batchResults = resultsPool.Get();
	}
}

void QueryBatch::Start(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx < count);
	cmdList->BeginQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatch::Stop(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx < count);
	cmdList->EndQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatch::Set(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx == npos || queryIdx < count);
	cmdList->SetPredication(queryIdx == npos ? NULL : batchResults, queryIdx == npos ? 0 : queryIdx * sizeof(UINT64), D3D12_PREDICATION_OP_EQUAL_ZERO);
}

void QueryBatch::Resolve(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
	{
		if (!fresh)
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY));
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults, 0);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
	}
}

void QueryBatch::Finish(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
}