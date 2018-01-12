/**
\author		Alexey Shaydurov aka ASH
\date		12.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "terrain.hh"
#include "tracked resource.inl"
#include "world hierarchy.inl"
#include "GPU stream buffer allocator.inl"
#include "GPU work submission.h"

// !: need to investigate for deadlocks possibility due to MSVC's std::async threadpool overflow
/*
0 - disable
1 - async
2 - execution::par
*/
#define MULTITHREADED_QUADS_SHCEDULE 2


using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace GPUStreamBuffer = Impl::GPUStreamBuffer;
namespace OcclusionCulling = Impl::OcclusionCulling;
namespace RenderPipeline = Impl::RenderPipeline;

namespace
{
#	pragma region ObjIterator
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
		// replace with C++20 <=>
#if defined _MSC_VER && _MSC_VER <= 1912
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
#	pragma endregion

	template<bool src32bit, bool dst32bit>
	void CopyIB(const void *src, volatile void *&dst, unsigned long int count)
	{
		typedef conditional_t<src32bit, uint32_t, uint16_t> SrcType;
		typedef conditional_t<dst32bit, uint32_t, uint16_t> DstType;
		copy_n(reinterpret_cast<const SrcType *>(src), count, reinterpret_cast<volatile DstType *>(dst));
		reinterpret_cast<volatile DstType *&>(dst) += count;
	}
}

TerrainVectorLayer::CRenderStage::COcclusionQueryPass::COcclusionQueryPass(const WRL::ComPtr<ID3D12PipelineState> &PSO) : PSO(PSO)
{}

TerrainVectorLayer::CRenderStage::COcclusionQueryPass::~COcclusionQueryPass() = default;

/*
	Hardware occlusion query technique used here.
	It has advantage over shader based batched occlusoin test in that it does not stresses UAV writes in PS for visible objects -
		hardware occlusion query does not incur any overhead in addition to regular depth/stencil test.
*/
void TerrainVectorLayer::CRenderStage::COcclusionQueryPass::operator()(const IRenderStage &parent, unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *cmdList) const
{
	const auto &occlusionQueryBatch = static_cast<const CRenderStage &>(parent).occlusionQueryBatch;

	if (rangeBegin < rangeEnd)
	{
		setupCallback(cmdList);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		ID3D12Resource *curVB = NULL;
		do
		{
			const auto &renderData = renderStream[rangeBegin];

			// setup VB if changed
			if (curVB != renderData.VB)
			{
				const D3D12_VERTEX_BUFFER_VIEW VB_view =
				{
					(curVB = renderData.VB)->GetGPUVirtualAddress(),
					curVB->GetDesc().Width,
					sizeof(AABB<2>)
				};
				cmdList->IASetVertexBuffers(0, 1, &VB_view);
			}

			occlusionQueryBatch.Start(rangeBegin, cmdList);
			cmdList->DrawInstanced(4, renderData.count, 0, renderData.startIdx);
			occlusionQueryBatch.Stop(rangeBegin, cmdList);

		} while (++rangeBegin < rangeEnd);
	}

	// on pass finish
	if (rangeEnd == renderStream.size())
		occlusionQueryBatch.Resolve(cmdList);
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::COcclusionQueryPass::Setup(function<void (ID3D12GraphicsCommandList1 *target)> &&setupCallback)
{
	this->setupCallback = move(setupCallback);
	renderStream.clear();
}

// 1 call site
inline void TerrainVectorLayer::CRenderStage::COcclusionQueryPass::IssueOcclusion(const OcclusionQueryGeometry &queryGeometry)
{
	renderStream.push_back(queryGeometry);
}

TerrainVectorLayer::CRenderStage::CMainPass::CMainPass(const float (&color)[3], const ComPtr<ID3D12PipelineState> &PSO) :
	color{ color[0], color[1], color[2] }, PSO(PSO)
{}

TerrainVectorLayer::CRenderStage::CMainPass::~CMainPass() = default;

void TerrainVectorLayer::CRenderStage::CMainPass::operator()(const IRenderStage &parent, unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *cmdList) const
{
	const auto &occlusionQueryBatch = static_cast<const CRenderStage &>(parent).occlusionQueryBatch;

	if (rangeBegin < rangeEnd)
	{
		setupCallback(cmdList);
		cmdList->SetGraphicsRoot32BitConstants(1, size(color), color, 0);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto curOcclusionQueryIdx = OcclusionCulling::QueryBatch::npos;
		do
		{
			const auto &quad = quads[renderStream[rangeBegin].startQuadIdx];

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

			const auto quadRangeEnd = min(rangeEnd, quad.streamEnd);
			do
			{
				const auto &renderData = renderStream[rangeBegin];
				if (curOcclusionQueryIdx != renderData.occlusion)
					occlusionQueryBatch.Set(curOcclusionQueryIdx = renderData.occlusion, cmdList);
				cmdList->DrawIndexedInstanced(renderData.triCount * 3, 1, renderData.startIdx, 0, 0);
			} while (++rangeBegin < quadRangeEnd);
		} while (rangeBegin < rangeEnd);
	}

	// on pass finish
	if (rangeEnd == renderStream.size())
		occlusionQueryBatch.Finish(cmdList);
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

void TerrainVectorLayer::CRenderStage::CMainPass::IssueCluster(unsigned long int startIdx, unsigned long int triCount, decltype(OcclusionCulling::QueryBatch::npos) occlusion)
{
	assert(triCount);
	renderStream.push_back({ startIdx, triCount, occlusion, quads.size() });
}

const RenderPipeline::IRenderPass &TerrainVectorLayer::CRenderStage::operator [](unsigned renderPassIdx) const
{
	const RenderPipeline::IRenderPass *const renderPasses[] = { &occlusionQueryPass, &mainPass };
	return *renderPasses[renderPassIdx];
}

void TerrainVectorLayer::CRenderStage::Setup(function<void (ID3D12GraphicsCommandList1 *target)> &&cullPassSetupCallback, function<void (ID3D12GraphicsCommandList1 *target)> &&mainPassSetupCallback)
{
	occlusionQueryPass.Setup(move(cullPassSetupCallback));
	mainPass.Setup(move(mainPassSetupCallback));
}

inline void TerrainVectorLayer::CRenderStage::SetupOcclusionQueryBatch(unsigned long queryCount)
{
	occlusionQueryBatch = OcclusionCulling::QueryBatch(queryCount);
}

void TerrainVectorLayer::CRenderStage::IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit)
{
	mainPass.IssueQuad(VIB, VB_size, IB_size, IB32bit);
}

template<class Node>
bool TerrainVectorLayer::CRenderStage::IssueNode(const Node &node, remove_const_t<decltype(OcclusionCulling::QueryBatch::npos)> &occlusionProvider, remove_const_t<decltype(OcclusionCulling::QueryBatch::npos)> &coarseOcclusion, remove_const_t<decltype(OcclusionCulling::QueryBatch::npos)> &fineOcclusion, decltype(node.GetOcclusionCullDomain()) &occlusionCullDomainOverriden)
{
	if (const auto &occlusionQueryGeometry = node.GetOcclusionQueryGeometry())
	{
		occlusionCullDomainOverriden = node.GetOcclusionCullDomain();
		occlusionQueryPass.IssueOcclusion({ occlusionQueryGeometry.VB, occlusionQueryGeometry.startIdx, occlusionQueryGeometry.count });
		fineOcclusion = ++occlusionProvider;
	}
	else if (fineOcclusion != OcclusionCulling::QueryBatch::npos)
		node.OverrideOcclusionCullDomain(occlusionCullDomainOverriden);
	if (occlusionCullDomainOverriden == decltype(node.GetOcclusionCullDomain())::WholeNode)
		coarseOcclusion = fineOcclusion;
	switch (node.GetVisibility(occlusionCullDomainOverriden))
	{
	case decltype(node.GetVisibility(occlusionCullDomainOverriden))::Composite:
		IssueExclusiveObjects(node, coarseOcclusion);
		return true;
	case decltype(node.GetVisibility(occlusionCullDomainOverriden))::Atomic:
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
void TerrainVectorLayer::CRenderStage::IssueExclusiveObjects(const Node &node, decltype(OcclusionCulling::QueryBatch::npos) occlusion)
{
	if (node.GetExclusiveTriCount())
		mainPass.IssueCluster(node.startIdx, node.GetExclusiveTriCount(), occlusion);
}

template<class Node>
void TerrainVectorLayer::CRenderStage::IssueChildren(const Node &node, decltype(OcclusionCulling::QueryBatch::npos) occlusion)
{
	mainPass.IssueCluster(node.startIdx + node.GetExclusiveTriCount() * 3, node.GetInclusiveTriCount() - node.GetExclusiveTriCount(), occlusion);
}

template<class Node>
void TerrainVectorLayer::CRenderStage::IssueWholeNode(const Node &node, decltype(OcclusionCulling::QueryBatch::npos) occlusion)
{
	mainPass.IssueCluster(node.startIdx, node.GetInclusiveTriCount(), occlusion);
}

void TerrainVectorLayer::QuadDeleter::operator()(TerrainVectorQuad *quadToRemove) const
{
	quadToRemove->layer->quads.erase(quadLocation);
}

TerrainVectorLayer::TerrainVectorLayer(shared_ptr<class World> world, unsigned int layerIdx, const float (&color)[3], const ComPtr<ID3D12PipelineState> &cullPSO, const ComPtr<ID3D12PipelineState> &mainPSO) :
	world(move(world)), layerIdx(layerIdx), renderStage(color, cullPSO, mainPSO)
{
}

TerrainVectorLayer::~TerrainVectorLayer() = default;

auto TerrainVectorLayer::AddQuad(unsigned long int vcount, const function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData) -> QuadPtr
{
	quads.emplace_back(shared_from_this(), vcount, fillVB, objCount, IB32bit, getObjectData);
	return { &quads.back(), { prev(quads.cend()) } };
}

const RenderPipeline::IRenderStage *TerrainVectorLayer::BuildRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList1 *target)> &cullPassSetupCallback, function<void (ID3D12GraphicsCommandList1 *target)> &mainPassSetupCallback) const
{
	using namespace placeholders;
	renderStage.Setup(move(cullPassSetupCallback), move(mainPassSetupCallback));
	// use C++17 template deduction
	GPUStreamBuffer::CountedAllocatorWrapper<sizeof AABB<2>> GPU_AABB_countedAllocator(*GPU_AABB_allocator);
#if MULTITHREADED_QUADS_SHCEDULE == 0
	for_each(quads.begin(), quads.end(), bind(&TerrainVectorQuad::Shcedule, _1, ref(GPU_AABB_countedAllocator), cref(frustumCuller), cref(frustumXform)));
#elif MULTITHREADED_QUADS_SHCEDULE == 1
	static thread_local vector<future<void>> pendingAsyncs;
	pendingAsyncs.clear();
	pendingAsyncs.reserve(quads.size());
	try
	{
		transform(quads.cbegin(), quads.cend(), back_inserter(pendingAsyncs), [&](decltype(quads)::const_reference quad)
		{
			return async(&TerrainVectorQuad::Shcedule, cref(quad), ref(GPU_AABB_countedAllocator), cref(frustumCuller), cref(frustumXform));
		});
		// wait for pending asyncs\
		use 'get' instead of 'wait' in order to propagate exceptions (first only)
		for_each(pendingAsyncs.begin(), pendingAsyncs.end(), mem_fn(&decltype(pendingAsyncs)::value_type::get));
	}
	catch (...)
	{
		// needed to prevent access to quads from worker threads after an exception was thrown
		for (const auto &pendingAsync : pendingAsyncs)
			if (pendingAsync.valid())
				pendingAsync.wait();
		throw;
	}
#elif MULTITHREADED_QUADS_SHCEDULE == 2
	// exceptions would currently lead to terminate()
	for_each(execution::par, quads.begin(), quads.end(), bind(&TerrainVectorQuad::Shcedule, _1, ref(GPU_AABB_countedAllocator), cref(frustumCuller), cref(frustumXform)));
#else
#error invalid MULTITHREADED_QUADS_SHCEDULE value
#endif
	renderStage.SetupOcclusionQueryBatch(GPU_AABB_countedAllocator.GetAllocatedItemCount());
	using namespace placeholders;
	for_each(quads.begin(), quads.end(), bind(&TerrainVectorQuad::Issue, _1, OcclusionCulling::QueryBatch::npos));
	return &renderStage;
}

void TerrainVectorLayer::ShceduleRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList1 *target)> cullPassSetupCallback, function<void (ID3D12GraphicsCommandList1 *target)> mainPassSetupCallback) const
{
	GPUWorkSubmission::AppendRenderStage(&TerrainVectorLayer::BuildRenderStage, this, /*cref*/(frustumCuller), /*cref*/(frustumXform), move(cullPassSetupCallback), move(mainPassSetupCallback));
}

TerrainVectorQuad::TerrainVectorQuad(shared_ptr<TerrainVectorLayer> layer, unsigned long int vcount, const function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool srcIB32bit, const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData) :
	layer(move(layer)), subtree(ObjIterator<Object>(getObjectData, 0), ObjIterator<Object>(getObjectData, objCount), Impl::Hierarchy::SplitTechnique::MEAN, .5),
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

// 1 call site
inline void TerrainVectorQuad::Shcedule(GPUStreamBuffer::CountedAllocatorWrapper<sizeof AABB<2>> &GPU_AABB_allocator, const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const
{
	subtree.Shcedule(GPU_AABB_allocator, frustumCuller, frustumXform);
}

// 1 call site
inline void TerrainVectorQuad::Issue(remove_const_t<decltype(OcclusionCulling::QueryBatch::npos)> &occlusionProvider) const
{
	using namespace placeholders;

	const auto issueNode = bind(&TerrainVectorLayer::CRenderStage::IssueNode<decltype(subtree)::Node>, ref(layer->renderStage), _1, ref(occlusionProvider), _2, _3, _4);
	subtree.Traverse(issueNode, OcclusionCulling::QueryBatch::npos, OcclusionCulling::QueryBatch::npos, decltype(declval<decltype(subtree)::Node>().GetOcclusionCullDomain())::ChildrenOnly);
	layer->renderStage.IssueQuad(VIB.Get(), VB_size, IB_size, IB32bit);
}