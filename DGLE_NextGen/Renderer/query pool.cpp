/**
\author		Alexey Shaydurov aka ASH
\date		21.12.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "query pool.h"
#include "align.h"

using namespace std;
using namespace Renderer::QueryPool;
using WRL::ComPtr;

// OcclusionQueryBatch's statics
shared_mutex OcclusionQueryBatch::mtx;
decltype(OcclusionQueryBatch::chunks) OcclusionQueryBatch::chunks;
unsigned long OcclusionQueryBatch::capacity;

OcclusionQueryBatch::OcclusionQueryBatch(unsigned long count) : count(count)
{
	extern ComPtr<ID3D12Device2> device;

	shared_lock<decltype(mtx)> sharedLock(mtx);
	if (count > capacity)
	{
		sharedLock.unlock();
		lock_guard<decltype(mtx)> exclusiveLock(mtx);
		// other thread can have time to replenish the pool, check again
		if (count > capacity)
		{
			static_assert(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT % sizeof(UINT64) == 0);
			const auto resolvedSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>((count - capacity) * sizeof(UINT64));

			decltype(chunks)::value_type newChunk;
			const D3D12_QUERY_HEAP_DESC heapDesk = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, resolvedSize / sizeof(UINT64), 0 };
			const CD3DX12_RESOURCE_DESC resolveBufferDesc(D3D12_RESOURCE_DIMENSION_BUFFER, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, resolvedSize, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE);
			CheckHR(device->CreateQueryHeap(&heapDesk, IID_PPV_ARGS(newChunk.first.GetAddressOf())));
			CheckHR(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, &resolveBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(newChunk.second.GetAddressOf())));
			chunks.push_back(move(newChunk));
			capacity += heapDesk.Count;
		}
	}
}