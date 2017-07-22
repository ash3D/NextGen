/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "terrain.hh"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

static constexpr unsigned int clusterTriThreshold = 256;

void TerrainVectorLayer::QuadDeleter::operator()(TerrainVectorQuad *quadToRemove) const
{
	quadToRemove->layer->quads.erase(quadLocation);
}

TerrainVectorLayer::TerrainVectorLayer(shared_ptr<class World> world, unsigned int layerIdx, const float (&color)[3]) :
	world(move(world)), layerIdx(layerIdx), color{ color[0], color[1], color[2] }
{
}

auto TerrainVectorLayer::AddQuad(unsigned long int vcount, function<void __cdecl(volatile float verts[][2])> fillVB, unsigned int objCount, bool IB32bit, function<ObjectData __cdecl(unsigned int objIdx)> getObjectData) -> QuadPtr
{
	quads.emplace_back(shared_from_this(), vcount, fillVB, objCount, IB32bit, getObjectData);
	return { &quads.back(), { prev(quads.cend()) } };
}

void TerrainVectorLayer::Render(ID3D12GraphicsCommandList1 *cmdList) const
{
	cmdList->SetGraphicsRoot32BitConstants(1, size(color), color, 0);
	for (const auto &quad : quads)
		quad.Render(cmdList);
}

template<bool src32bit, bool dst32bit>
static void CopyIB(const void *src, volatile void *&dst, unsigned long int count)
{
	typedef conditional_t<src32bit, uint32_t, uint16_t> SrcType;
	typedef conditional_t<dst32bit, uint32_t, uint16_t> DstType;
	copy_n(reinterpret_cast<const SrcType *>(src), count, reinterpret_cast<volatile DstType *>(dst));
	reinterpret_cast<volatile DstType *&>(dst) += count;
}

TerrainVectorQuad::TerrainVectorQuad(shared_ptr<TerrainVectorLayer> layer, unsigned long int vcount, function<void (volatile float verts[][2])> fillVB, unsigned int objCount, bool srcIB32bit, function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> getObjectData) :
	layer(move(layer)), objIdxCounts(objCount), VB_size(vcount * sizeof(float [2])), IB32bit(vcount > UINT16_MAX)	// consider using allocator with default init for vector in place of value one
{
	extern ComPtr<ID3D12Device2> device;

	//decltype(objIdxCounts)::value_type idxCount = 0;
	/*
	Perform linear objects clusterization in order to fill GPU vertex shader SIMD.
	It works reasonably well assuming spatial objects positions coherency (needed for efficient culling).
	Otherwise consider to perform BVH partitioning with objects reordering in order to attain spatial coherency.
	*/
	Cluster curCluster = {};
	for (unsigned i = 0; i < objCount; i++)
	{
		const auto curObjData = getObjectData(i);
		idxCount += objIdxCounts[i] = curObjData.triCount * 3;
		curCluster.aabb.Refit(curObjData.aabb);
		if ((curCluster.idxCount += objIdxCounts[i]) >= clusterTriThreshold * 3)
		{
			clusters.push_back(curCluster);
			curCluster.aabb = {};
			curCluster.startIdx = idxCount;
			curCluster.idxCount = 0;
		}
	}
	if (clusters.empty())
		clusters.push_back(curCluster);
	else // tail
	{
		clusters.back().aabb.Refit(curCluster.aabb);
		clusters.back().idxCount += curCluster.idxCount;
	}
	clusters.shrink_to_fit();
	IB_size = idxCount * (IB32bit ? sizeof(uint32_t) : sizeof(uint16_t));

	// create and fill VIB
	{
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(VB_size + IB_size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,	// clear value
			IID_PPV_ARGS(&VIB)));

		volatile void *writePtr;
		CheckHR(VIB->Map(0, &CD3DX12_RANGE(0, 0), const_cast<void **>(&writePtr)));

		// VB
		fillVB(reinterpret_cast<volatile float (*)[2]>(writePtr));
		reinterpret_cast<volatile unsigned char *&>(writePtr) += VB_size;

		// IB
		const auto CopyIB_ptr = srcIB32bit ? IB32bit ? CopyIB<true, true> : CopyIB<true, false> : IB32bit ? CopyIB<false, true> : CopyIB<false, false>;
		for (unsigned i = 0; i < objCount; i++)
		{
			const auto curObjData = getObjectData(i);
			const auto curObjIdxCount = curObjData.triCount * 3;
			CopyIB_ptr(curObjData.tris, writePtr, curObjIdxCount);
		}
		
		VIB->Unmap(0, NULL);
	}
}

TerrainVectorQuad::~TerrainVectorQuad() = default;

void TerrainVectorQuad::Render(ID3D12GraphicsCommandList1 *cmdList) const
{
	const D3D12_VERTEX_BUFFER_VIEW VB_view =
	{
		VIB->GetGPUVirtualAddress(),
		VB_size,
		sizeof(float [2])
	};
	const D3D12_INDEX_BUFFER_VIEW IB_view =
	{
		VB_view.BufferLocation + VB_view.SizeInBytes,
		IB_size,
		IB32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
	};
	cmdList->IASetVertexBuffers(0, 1, &VB_view);
	cmdList->IASetIndexBuffer(&IB_view);

#if 0
	cmdList->DrawIndexedInstanced(idxCount, 1, 0, 0, 0);
#elif 1
	for (const auto &curCluster : clusters)
		cmdList->DrawIndexedInstanced(curCluster.idxCount, 1, curCluster.startIdx, 0, 0);
#else
	decltype(objIdxCounts)::value_type curIB_offset = 0;
	for (const auto curObjIdxCount : objIdxCounts)
	{
		cmdList->DrawIndexedInstanced(curObjIdxCount, 1, curIB_offset, 0, 0);
		curIB_offset += curObjIdxCount;
	}
#endif
}