/**
\author		Alexey Shaydurov aka ASH
\date		31.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "terrain.hh"
#include "world hierarchy.inl"
#include "GPU work submission.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace RenderPipeline = Impl::RenderPipeline;

namespace
{
	// make it templated in order to avoid access to quad's private 'Object' and making it possible to put the definition to anonymous namespace
	template<class Object>
	class ObjIterator;

	template<class Object>
	typename ObjIterator<Object>::difference_type operator -(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator ==(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator !=(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator <(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator <=(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator >(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator >=(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	class ObjIterator : public iterator<random_access_iterator_tag, Object, signed int>
	{
		// use reference_wrapper instead of plain ref in order to make iterator copy assignable (required by Iterator concept)
		reference_wrapper<const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)>> getObjectData;
		unsigned int objIdx;

	private:
		typedef iterator<random_access_iterator_tag, Object, signed int> iterator;

	public:
		using iterator::value_type;
		using iterator::difference_type;

	public:
		ObjIterator(const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData, unsigned int objIdx) :
			getObjectData(getObjectData), objIdx(objIdx) {}

	public:
		ObjIterator &operator ++(), operator ++(int), operator +(difference_type offset) const;
		friend difference_type operator -<>(ObjIterator left, ObjIterator right);

	public:
		value_type operator *() const;
#if defined _MSC_VER && _MSC_VER <= 1911
		friend bool operator ==<>(ObjIterator left, ObjIterator right);
		friend bool operator !=<>(ObjIterator left, ObjIterator right);
		friend bool operator < <>(ObjIterator left, ObjIterator right);
		friend bool operator <=<>(ObjIterator left, ObjIterator right);
		friend bool operator > <>(ObjIterator left, ObjIterator right);
		friend bool operator >=<>(ObjIterator left, ObjIterator right);
#else
		friend bool operator ==<>(ObjIterator left, ObjIterator right), operator !=<>(ObjIterator left, ObjIterator right),
			operator < <>(ObjIterator left, ObjIterator right), operator <= <>(ObjIterator left, ObjIterator right),
			operator > <>(ObjIterator left, ObjIterator right), operator >= <>(ObjIterator left, ObjIterator right);
#endif
	};

	template<class Object>
	inline auto ObjIterator<Object>::operator ++() -> ObjIterator &
	{
		++objIdx;
		return *this;
	}

	template<class Object>
	inline auto ObjIterator<Object>::operator ++(int) -> ObjIterator
	{
		return { getObjectData, objIdx++ };
	}

	template<class Object>
	inline auto ObjIterator<Object>::operator +(difference_type offset) const -> ObjIterator
	{
		return { getObjectData, objIdx + offset };
	}

	template<class Object>
	inline typename ObjIterator<Object>::difference_type operator -(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx - right.objIdx;
	}

	template<class Object>
	auto ObjIterator<Object>::operator *() const -> value_type
	{
		const auto objData = getObjectData(objIdx);
		return { objData.aabb, objData.triCount, objIdx };
	}

	template<class Object>
	inline bool operator ==(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx == right.objIdx;
	}

	template<class Object>
	inline bool operator !=(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx != right.objIdx;
	}

	template<class Object>
	inline bool operator <(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx < right.objIdx;
	}

	template<class Object>
	inline bool operator <=(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx <= right.objIdx;
	}

	template<class Object>
	inline bool operator >(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx > right.objIdx;
	}

	template<class Object>
	inline bool operator >=(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx >= right.objIdx;
	}

	template<bool src32bit, bool dst32bit>
	void CopyIB(const void *src, volatile void *&dst, unsigned long int count)
	{
		typedef conditional_t<src32bit, uint32_t, uint16_t> SrcType;
		typedef conditional_t<dst32bit, uint32_t, uint16_t> DstType;
		copy_n(reinterpret_cast<const SrcType *>(src), count, reinterpret_cast<volatile DstType *>(dst));
		reinterpret_cast<volatile DstType *&>(dst) += count;
	}
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::COcclusionQueryPass::Setup()
{
	renderStream.clear();
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::COcclusionQueryPass::IssueOcclusion(void *occlusion)
{
}

TerrainVectorLayer::CRenderStage::CMainPass::CMainPass(const float (&color)[3], const ComPtr<ID3D12PipelineState> &PSO) :
	color{ color[0], color[1], color[2] }, PSO(PSO)
{}

TerrainVectorLayer::CRenderStage::CMainPass::~CMainPass() = default;

void TerrainVectorLayer::CRenderStage::CMainPass::operator()(unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *cmdList) const
{
	if (rangeBegin < rangeEnd)
	{
		setupCallback(cmdList);
		cmdList->SetGraphicsRoot32BitConstants(1, size(color), color, 0);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto quadIdx = renderStream[rangeBegin].startQuadIdx;
		do
		{
			const auto &quad = quads[quadIdx++];

			// setup VB/IB
			{
				const D3D12_VERTEX_BUFFER_VIEW VB_view =
				{
					quad.VIB->GetGPUVirtualAddress(),
					quad.VB_size,
					sizeof(float [2])
				};
				const D3D12_INDEX_BUFFER_VIEW IB_view =
				{
					VB_view.BufferLocation + VB_view.SizeInBytes,
					quad.IB_size,
					quad.IB32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
				};
				cmdList->IASetVertexBuffers(0, 1, &VB_view);
				cmdList->IASetIndexBuffer(&IB_view);
			}

			for (const auto quadRangeEnd = min(rangeEnd, quad.streamEnd); rangeBegin < quadRangeEnd; rangeBegin++)
			{
				const auto &renderData = renderStream[rangeBegin];
				cmdList->DrawIndexedInstanced(renderData.triCount * 3, 1, renderData.startIdx, 0, 0);
			}
		} while (rangeBegin < rangeEnd);
	}
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::CMainPass::Setup(function<void (ID3D12GraphicsCommandList1 *target)> &&setupCallback)
{
	this->setupCallback = move(setupCallback);
	renderStream.clear();
	quads.clear();
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::CMainPass::IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit)
{
	quads.push_back({ renderStream.size(), VIB, VB_size, IB_size, IB32bit });
}

void TerrainVectorLayer::CRenderStage::CMainPass::IssueCluster(unsigned long int startIdx, unsigned long int triCount, void *occlusion)
{
	assert(triCount);
	renderStream.push_back({ startIdx, triCount, occlusion, quads.size() });
}

const RenderPipeline::IRenderPass &TerrainVectorLayer::CRenderStage::operator [](unsigned renderPassIdx) const
{
	const RenderPipeline::IRenderPass *const renderPasses[] = { &occlusionQueryPass, &mainPass };
	return *renderPasses[renderPassIdx];
}

void TerrainVectorLayer::CRenderStage::Setup(function<void (ID3D12GraphicsCommandList1 *target)> &&mainPassSetupCallback)
{
	occlusionQueryPass.Setup();
	mainPass.Setup(move(mainPassSetupCallback));
}

void TerrainVectorLayer::CRenderStage::IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit)
{
	mainPass.IssueQuad(VIB, VB_size, IB_size, IB32bit);
}

template<class Node>
bool TerrainVectorLayer::CRenderStage::IssueNode(const Node &node, void *&coarseOcclusion, void *&fineOcclusion, bool &cullWholeNodeOverriden)
{
	if (node.OcclusionQueryShceduled())
	{
		cullWholeNodeOverriden = node.CullWholeNode();
		occlusionQueryPass.IssueOcclusion(fineOcclusion = this);
	}
	else if (fineOcclusion)
		cullWholeNodeOverriden = cullWholeNodeOverriden || node.CullWholeNode();	// |= may lead to UB due to uninit read
	if (cullWholeNodeOverriden)
		coarseOcclusion = fineOcclusion;
	switch (node.GetVisibility())
	{
	case decltype(node.GetVisibility())::Composite:
		IssueExclusiveObjects(node, coarseOcclusion);
		return true;
	case decltype(node.GetVisibility())::Atomic:
		if (coarseOcclusion == fineOcclusion)
			IssueWholeNode(node, coarseOcclusion);
		else
		{
			IssueExclusiveObjects(node, coarseOcclusion);
			IssueChildren(node, fineOcclusion);
		}
		break;
	}
	return false;
}

template<class Node>
void TerrainVectorLayer::CRenderStage::IssueExclusiveObjects(const Node &node, void *occlusion)
{
	if (node.GetExclusiveTriCount())
		mainPass.IssueCluster(node.startIdx, node.GetExclusiveTriCount(), occlusion);
}

template<class Node>
void TerrainVectorLayer::CRenderStage::IssueChildren(const Node &node, void *occlusion)
{
	mainPass.IssueCluster(node.startIdx + node.GetExclusiveTriCount() * 3, node.GetInclusiveTriCount() - node.GetExclusiveTriCount(), occlusion);
}

template<class Node>
void TerrainVectorLayer::CRenderStage::IssueWholeNode(const Node &node, void *occlusion)
{
	mainPass.IssueCluster(node.startIdx, node.GetInclusiveTriCount(), occlusion);
}

void TerrainVectorLayer::QuadDeleter::operator()(TerrainVectorQuad *quadToRemove) const
{
	quadToRemove->layer->quads.erase(quadLocation);
}

TerrainVectorLayer::TerrainVectorLayer(shared_ptr<class World> world, unsigned int layerIdx, const float (&color)[3], ComPtr<ID3D12PipelineState> &mainPSO) :
	world(move(world)), layerIdx(layerIdx), renderStage(color, mainPSO)
{
}

TerrainVectorLayer::~TerrainVectorLayer() = default;

auto TerrainVectorLayer::AddQuad(unsigned long int vcount, const function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData) -> QuadPtr
{
	quads.emplace_back(shared_from_this(), vcount, fillVB, objCount, IB32bit, getObjectData);
	return { &quads.back(), { prev(quads.cend()) } };
}

const RenderPipeline::IRenderStage *TerrainVectorLayer::BuildRenderStage(const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList1 *target)> &mainPassSetupCallback) const
{
	using namespace placeholders;
	renderStage.Setup(move(mainPassSetupCallback));
	for_each(quads.begin(), quads.end(), bind(&TerrainVectorQuad::Dispatch, _1, cref(frustumXform)));
	return &renderStage;
}

void TerrainVectorLayer::ShceduleRenderStage(const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList1 *target)> mainPassSetupCallback) const
{
	GPUWorkSubmission::AppendRenderStage(&TerrainVectorLayer::BuildRenderStage, this, /*cref*/(frustumXform), move(mainPassSetupCallback));
}

TerrainVectorQuad::TerrainVectorQuad(shared_ptr<TerrainVectorLayer> layer, unsigned long int vcount, const function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool srcIB32bit, const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData) :
	layer(move(layer)), subtree(ObjIterator<Object>(getObjectData, 0), ObjIterator<Object>(getObjectData, objCount), Impl::Hierarchy::SplitTechnique::MEAN, .5f),
	IB32bit(vcount > UINT16_MAX), VB_size(vcount * sizeof(float [2])), IB_size(subtree.GetTriCount() * 3 * (IB32bit ? sizeof(uint32_t) : sizeof(uint16_t)))
{
	extern ComPtr<ID3D12Device2> device;

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
		auto reorderedIBFiller = [startIdx = 0ul, CopyIB_ptr, &getObjectData, &writePtr](decltype(subtree)::Node &node) mutable
		{
			node.startIdx = startIdx;
			const auto objRange = node.GetExclusiveObjectsRange();
			for_each(objRange.first, objRange.second, [&startIdx, &CopyIB_ptr, &getObjectData, &writePtr](const Object &obj)
			{
				const auto curObjData = getObjectData(obj.idx);
				const auto curObjIdxCount = curObjData.triCount * 3;
				CopyIB_ptr(curObjData.tris, writePtr, curObjIdxCount);
				startIdx += curObjIdxCount;
			});
			return true;
		};
		subtree.Traverse(reorderedIBFiller);
		subtree.FreeObjects();
		
		VIB->Unmap(0, NULL);
	}
}

TerrainVectorQuad::~TerrainVectorQuad() = default;

void TerrainVectorQuad::Dispatch(const HLSL::float4x4 &frustumXform) const
{
	using namespace placeholders;

	subtree.Shcedule(frustumXform);
	const auto issueNode = bind(&TerrainVectorLayer::CRenderStage::IssueNode<decltype(subtree)::Node>, ref(layer->renderStage), _1, _2, _3, _4);
	subtree.Traverse<void *, void *>(issueNode, nullptr, nullptr, false);
	layer->renderStage.IssueQuad(VIB.Get(), VB_size, IB_size, IB32bit);
}