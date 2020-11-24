#include "stdafx.h"
#include "terrain.hh"
#include "terrain materials.hh"
#include "world.hh"	// for globalGPUBuffer
#include "tracked resource.inl"
#include "world hierarchy.inl"
#include "GPU stream buffer allocator.inl"
#include "cmdlist pool.inl"
#include "terrain render stages.h"
#include "GPU work submission.h"
#include "render stage.h"
#include "render passes.h"
#include "render pipeline.h"
#include "occlusion query batch.h"
#include "occlusion query visualization.h"
#include "global GPU buffer data.h"
#include "shader bytecode.h"
#include "config.h"
#include "PIX events.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

namespace Shaders
{
#	include "AABB_2D.csh"
#	include "AABB_2D_vis.csh"
}

// !: need to investigate for deadlocks possibility due to MSVC's std::async threadpool overflow
/*
0 - disable
1 - async
2 - execution::par
*/
#define MULTITHREADED_QUADS_SHCEDULE 2


using namespace std;
using namespace Renderer;
using namespace HLSL;
using pmr::polymorphic_allocator;
using WRL::ComPtr;
namespace OcclusionCulling = Impl::OcclusionCulling;
namespace CmdListPool = Impl::CmdListPool;
namespace RenderPipeline = Impl::RenderPipeline;
namespace RenderPasses = RenderPipeline::RenderPasses;

extern pmr::synchronized_pool_resource globalTransientRAM;
extern ComPtr<ID3D12Device4> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

namespace
{
#	pragma region ObjIterator
	// make it templated in order to avoid access to quad's private 'Object' and making it possible to put the definition to anonymous namespace
	template<class Object>
	class ObjIterator;

	template<class Object>
	typename ObjIterator<Object>::difference_type operator -(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	strong_ordering operator <=>(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	bool operator ==(ObjIterator<Object> left, ObjIterator<Object> right);

	template<class Object>
	class ObjIterator : public iterator<random_access_iterator_tag, Object, signed int>
	{
		// use reference_wrapper instead of plain ref in order to make iterator copy assignable (required by Iterator concept)
		reference_wrapper<const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)>> getObjectData;
		unsigned int objIdx;

	private:
		typedef iterator<random_access_iterator_tag, Object, signed int> iterator;

	public:
		using typename iterator::value_type;
		using typename iterator::difference_type;

	public:
		ObjIterator(const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData, unsigned int objIdx) :
			getObjectData(getObjectData), objIdx(objIdx) {}

	public:
		ObjIterator &operator ++(), operator ++(int), operator +(difference_type offset) const;
		friend difference_type operator -<>(ObjIterator left, ObjIterator right);

	public:
		value_type operator *() const;
		friend strong_ordering operator <=><>(ObjIterator left, ObjIterator right);
		friend bool operator ==<>(ObjIterator left, ObjIterator right);
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
	inline strong_ordering operator <=>(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		assert(&left.getObjectData.get() == &right.getObjectData.get());
		return left.objIdx <=> right.objIdx;
	}

	template<class Object>
	inline bool operator ==(ObjIterator<Object> left, ObjIterator<Object> right)
	{
		return left <=> right == 0;
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

#pragma region TerrainVectorQuad
#pragma region render stages
#pragma region occlusion query pass
ComPtr<ID3D12RootSignature> TerrainVectorQuad::MainRenderStage::CreateCullPassRootSig()
{
	CD3DX12_ROOT_PARAMETER1 CBV_param;
	CBV_param.InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);	// per-frame data
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(1, &CBV_param, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain occlusion query root signature");
}

ComPtr<ID3D12PipelineState> TerrainVectorQuad::MainRenderStage::CreateCullPassPSO()
{
	const D3D12_BLEND_DESC blendDesc
	{
		FALSE,								// alpha2covarage
		FALSE,								// independent blend
		{
			FALSE,							// blend enable
			FALSE,							// logic op enable
			D3D12_BLEND_ONE,				// src blend
			D3D12_BLEND_ZERO,				// dst blend
			D3D12_BLEND_OP_ADD,				// blend op
			D3D12_BLEND_ONE,				// src blend alpha
			D3D12_BLEND_ZERO,				// dst blend alpha
			D3D12_BLEND_OP_ADD,				// blend op alpha
			D3D12_LOGIC_OP_NOOP,			// logic op
			0								// write mask
		}
	};

	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
		FALSE,										// MSAA
		FALSE,										// AA line
		0,											// force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	);

	const CD3DX12_DEPTH_STENCIL_DESC dsDesc
	(
		FALSE,																									// depth
		D3D12_DEPTH_WRITE_MASK_ZERO,
		D3D12_COMPARISON_FUNC_ALWAYS,
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		0,																										// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "AABB_min", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_max", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= cullPassRootSig.Get(),
		.VS						= ShaderBytecode(Shaders::AABB_2D),
		.BlendState				= blendDesc,
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.DSVFormat				= Config::ZFormat::ROP,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain occlusion query PSO");
	return result;
}

void TerrainVectorQuad::MainRenderStage::CullPassPre(CmdListPool::CmdList &cmdList) const
{
	PIXBeginEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::TerrainOcclusionQueryPass), "occlusion query pass");
	cmdList.FlushBarriers();
}

/*
	Hardware occlusion query technique used here.
	It has advantage over shader based batched occlusion test in that it does not stresses UAV writes in PS for visible objects -
		hardware occlusion query does not incur any overhead in addition to regular depth/stencil test.
*/
void TerrainVectorQuad::MainRenderStage::CullPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(cullPassPSO.Get());

	cmdList->SetGraphicsRootSignature(cullPassRootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, World::GetCurFrameGPUDataPtr());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	RenderPasses::RangeRenderPassScope renderPassScope(cmdList, renderPass);

	const OcclusionCulling::QueryBatchBase &queryBatch = visit([](const OcclusionCulling::QueryBatchBase &queryBatch) noexcept -> const auto & { return queryBatch; }, queryPass->occlusionQueryBatch);
	ID3D12Resource *curVB = NULL;
	do
	{
		const auto &queryData = queryPass->queryStream[rangeBegin];

		// setup VB if changed
		if (curVB != queryData.VB)
		{
			const D3D12_VERTEX_BUFFER_VIEW VB_view =
			{
				(curVB = queryData.VB)->GetGPUVirtualAddress(),
				curVB->GetDesc().Width,
				sizeof(AABB<2>)
			};
			cmdList->IASetVertexBuffers(0, 1, &VB_view);
		}

		queryBatch.Start(cmdList, rangeBegin);
		cmdList->DrawInstanced(4, queryData.count, 0, queryData.startIdx);
		queryBatch.Stop(cmdList, rangeBegin);

	} while (++rangeBegin < rangeEnd);
}

void TerrainVectorQuad::MainRenderStage::CullPassPost(CmdListPool::CmdList &cmdList) const
{
	if (const auto preservingQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::PERSISTENT>>(&queryPass->occlusionQueryBatch))
		preservingQueryBatch->Resolve(cmdList/*, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
	else
		get<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(queryPass->occlusionQueryBatch).Resolve(cmdList);
	PIXEndEvent(cmdList);	// occlusion query pass
}

void TerrainVectorQuad::MainRenderStage::SetupCullPass()
{
	queryPass->queryStream.reserve(parent->queryStreamLenCache);
}

// 1 call site
inline void TerrainVectorQuad::MainRenderStage::IssueOcclusion(ViewNode::OcclusionQueryGeometry occlusionQueryGeometry)
{
	queryPass->queryStream.push_back({ occlusionQueryGeometry.VB, occlusionQueryGeometry.startIdx, occlusionQueryGeometry.count });
}

void TerrainVectorQuad::MainRenderStage::UpdateCullPassCache()
{
	parent->queryStreamLenCache = max(parent->queryStreamLenCache, queryPass->queryStream.size());
}
#pragma endregion

#pragma region main pass
void TerrainVectorQuad::MainRenderStage::MainPassPre(CmdListPool::CmdList &cmdList) const
{
	PIXBeginEvent(cmdList, parent->layerMaterial->color, "main pass");
	cmdList.FlushBarriers();
}

void TerrainVectorQuad::MainRenderStage::MainPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(parent->layerMaterial->PSO.Get());

	parent->layerMaterial->Setup(cmdList, World::GetCurFrameGPUDataPtr(), cameraSettingsGPUAddress);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RenderPasses::RangeRenderPassScope renderPassScope(cmdList, renderPass);

	auto curOcclusionQueryIdx = OcclusionCulling::QueryBatchBase::npos;
	do
	{
		const auto &quad = quadStram[renderStream[rangeBegin].startQuadIdx];

		parent->layerMaterial->SetupQuad(cmdList, quad.center);

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
				visit([&](const auto &queryBatch) { queryBatch.Set(cmdList, curOcclusionQueryIdx = renderData.occlusion); }, queryPass->occlusionQueryBatch);
			cmdList->DrawIndexedInstanced(renderData.triCount * 3, 1, renderData.startIdx, 0, 0);
		} while (++rangeBegin < quadRangeEnd);
	} while (rangeBegin < rangeEnd);
}

void TerrainVectorQuad::MainRenderStage::MainPassPost(CmdListPool::CmdList &cmdList) const
{
	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&queryPass->occlusionQueryBatch))
		transientQueryBatch->Finish(cmdList);
	PIXEndEvent(cmdList);	// main pass
}

void TerrainVectorQuad::MainRenderStage::SetupMainPass()
{
	renderStream.reserve(parent->renderStreamLenCache);
	quadStram.reserve(parent->quads.size());
}

void TerrainVectorQuad::MainRenderStage::IssueCluster(unsigned long int startIdx, unsigned long int triCount, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	assert(triCount);
	renderStream.push_back({ startIdx, triCount, occlusion, quadStram.size() });
}

// 1 call site
inline void TerrainVectorQuad::MainRenderStage::IssueExclusiveObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	if (node.GetExclusiveTriCount())
		IssueCluster(node.startIdx, node.GetExclusiveTriCount(), occlusion);
}

// 1 call site
inline void TerrainVectorQuad::MainRenderStage::IssueChildren(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	IssueCluster(node.startIdx + node.GetExclusiveTriCount() * 3, node.GetInclusiveTriCount() - node.GetExclusiveTriCount(), occlusion);
}

// 1 call site
inline void TerrainVectorQuad::MainRenderStage::IssueWholeNode(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	IssueCluster(node.startIdx, node.GetInclusiveTriCount(), occlusion);
}

bool TerrainVectorQuad::MainRenderStage::IssueNodeObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) coarseOcclusion,  decltype(OcclusionCulling::QueryBatchBase::npos) fineOcclusion, ViewNode::Visibility visibility)
{
	switch (visibility)
	{
	case ViewNode::Visibility::Composite:
		IssueExclusiveObjects(node, coarseOcclusion);
		return true;
	case ViewNode::Visibility::Atomic:
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

// 1 call site
inline void TerrainVectorQuad::MainRenderStage::IssueQuad(float2 quadCenter, ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit)
{
	quadStram.push_back({ renderStream.size(), quadCenter, VIB, VB_size, IB_size, IB32bit });
}

void TerrainVectorQuad::MainRenderStage::UpdateMainPassCache()
{
	parent->renderStreamLenCache = max(parent->renderStreamLenCache, renderStream.size());
}
#pragma endregion

void TerrainVectorQuad::MainRenderStage::StagePre(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	PIXBeginEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::TerrainLayer), "terrain layer [%u] \"%s\"", parent->layerIdx, parent->layerName.c_str());
	CullPassPre(cmdList);
}

void TerrainVectorQuad::MainRenderStage::CullPass2MainPass(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	CullPassPost(cmdList);
	MainPassPre(cmdList);
}

void TerrainVectorQuad::MainRenderStage::StagePost(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	MainPassPost(cmdList);
	PIXEndEvent(cmdList);	// stage
}

RenderPipeline::PipelineItem (RenderPipeline::IRenderStage::*TerrainVectorQuad::MainRenderStage::DoSync(void) const)(unsigned int &length) const
{
	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&queryPass->occlusionQueryBatch))
		transientQueryBatch->Sync();

	return static_cast<decltype(phaseSelector)>(&MainRenderStage::GetStagePre);
}

auto TerrainVectorQuad::MainRenderStage::GetStagePre(unsigned int &) const -> RenderPipeline::PipelineItem
{
	phaseSelector = static_cast<decltype(phaseSelector)>(&MainRenderStage::GetCullPassRange);
	return RenderPipeline::PipelineItem{ bind_front(&MainRenderStage::StagePre, shared_from_this()) };
}

auto TerrainVectorQuad::MainRenderStage::GetCullPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryPass->queryStream.size(), nullptr, { stageZBinding, true, false }, ROPOutput,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&MainRenderStage::GetCullPass2MainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) { return bind(&MainRenderStage::CullPassRange, shared_from_this(), _1, rangeBegin, rangeEnd, move(renderPass)); });
}

auto TerrainVectorQuad::MainRenderStage::GetCullPass2MainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	phaseSelector = static_cast<decltype(phaseSelector)>(&MainRenderStage::GetMainPassRange);
	return RenderPipeline::PipelineItem{ bind_front(&MainRenderStage::CullPass2MainPass, shared_from_this()) };
}

auto TerrainVectorQuad::MainRenderStage::GetMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::RenderStageRTBinding> RTBinding{ stageRTBinding, true, true };
	return IterateRenderPass(length, renderStream.size(), &RTBinding, { stageZBinding, false, true }, ROPOutput,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&MainRenderStage::GetStagePost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) { return bind(&MainRenderStage::MainPassRange, shared_from_this(), _1, rangeBegin, rangeEnd, move(renderPass)); });
}

auto TerrainVectorQuad::MainRenderStage::GetStagePost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	RenderPipeline::TerminateStageTraverse();
	return RenderPipeline::PipelineItem{ bind_front(&MainRenderStage::StagePost, shared_from_this()) };
}

void TerrainVectorQuad::MainRenderStage::Setup()
{
	SetupCullPass();
	SetupMainPass();
}

void TerrainVectorQuad::MainRenderStage::SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion)
{
	extern bool enableDebugDraw;
	if (enableDebugDraw != queryPass->occlusionQueryBatch.index())
	{
		if (enableDebugDraw)
			queryPass->occlusionQueryBatch.emplace<true>(parent->layerName);
		else
			queryPass->occlusionQueryBatch.emplace<false>();
	}
	visit([maxOcclusion](OcclusionCulling::QueryBatchBase &queryBatch) { queryBatch.Setup(maxOcclusion + 1); }, queryPass->occlusionQueryBatch);
}

void TerrainVectorQuad::MainRenderStage::UpdateCaches()
{
	UpdateCullPassCache();
	UpdateMainPassCache();
}

RenderPipeline::PipelineStage TerrainVectorQuad::MainRenderStage::Build(const Impl::FrustumCuller<2> & frustumCuller, const float4x4 &frustumXform)
{
	using namespace placeholders;

	PIXScopedEvent(PIX_COLOR_INDEX(PIXEvents::TerrainBuildRenderStage), "terrain layer [%u] build render stage", parent->layerIdx);
	Setup();

	// schedule
	{
		PIXScopedEvent(PIX_COLOR_INDEX(PIXEvents::TerrainSchedule), "schedule");
#if MULTITHREADED_QUADS_SHCEDULE == 0
		for_each(quads.begin(), quads.end(), bind(&TerrainVectorQuad::Schedule, _1, ref(*GPU_AABB_allocator), cref(frustumCuller), cref(frustumXform)));
#elif MULTITHREADED_QUADS_SHCEDULE == 1
		static thread_local vector<future<void>> pendingAsyncs;
		pendingAsyncs.clear();
		pendingAsyncs.reserve(quads.size());
		try
		{
			transform(quads.cbegin(), quads.cend(), back_inserter(pendingAsyncs), [&](decltype(quads)::const_reference quad)
				{
					return async(&TerrainVectorQuad::Schedule, cref(quad), ref(*GPU_AABB_allocator), cref(frustumCuller), cref(frustumXform));
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
		for_each(execution::par, parent->quads.begin(), parent->quads.end(), bind(&TerrainVectorQuad::Schedule, _1, ref(*GPU_AABB_allocator), cref(frustumCuller), cref(frustumXform)));
#else
#error invalid MULTITHREADED_QUADS_SHCEDULE value
#endif
	}

	auto occlusionProvider = OcclusionCulling::QueryBatchBase::npos;

	// issue
	{
		PIXScopedEvent(PIX_COLOR_INDEX(PIXEvents::TerrainIssue), "issue");
		for_each(parent->quads.begin(), parent->quads.end(), bind(&TerrainVectorQuad::Issue, _1, ref(*this), ref(occlusionProvider)));
	}

	SetupOcclusionQueryBatch(occlusionProvider);
	queryPassPromise.set_value(queryPass);
	UpdateCaches();

	return shared_from_this();
}

TerrainVectorQuad::MainRenderStage::MainRenderStage(shared_ptr<const Renderer::TerrainVectorLayer> &&parent,
	D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &stageExchangeResult) :
	parent(move(parent)),
	cameraSettingsGPUAddress(cameraSettingsGPUAddress),
	stageRTBinding(ROPTargets),
	stageZBinding(ROPTargets, true, true),
	ROPOutput(ROPTargets),
	queryPass(allocate_shared<OcclusionQueryPass>(polymorphic_allocator<OcclusionQueryPass>(&globalTransientRAM)))	// or do this in 'Build()' ?
{
	stageExchangeResult = queryPassPromise.get_future();
}

auto TerrainVectorQuad::MainRenderStage::Schedule(shared_ptr<const Renderer::TerrainVectorLayer> parent, const Impl::FrustumCuller<2> &frustumCuller, const float4x4 &frustumXform,
	D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets) -> StageExchange
{
	StageExchange stageExchange;
	auto renderStage = allocate_shared<MainRenderStage>(polymorphic_allocator<MainRenderStage>(&globalTransientRAM), move(parent), cameraSettingsGPUAddress, ROPTargets, stageExchange);
	GPUWorkSubmission::AppendPipelineStage<true>(&MainRenderStage::Build, move(renderStage), /*cref*/(frustumCuller), /*cref*/(frustumXform));
	return stageExchange;
}

inline void TerrainVectorQuad::MainRenderStage::OnFrameFinish()
{
	GPU_AABB_allocator->OnFrameFinish();
}

#pragma region visualize occlusion pass
enum
{
	AABB_PASS_ROOT_PARAM_PER_FRAME_DATA_CBV,
	AABB_PASS_ROOT_PARAM_CAMERA_SETTINGS_CBV,
	AABB_PASS_ROOT_PARAM_COLOR,
	AABB_PASS_ROOT_PARAM_COUNT
};

ComPtr<ID3D12RootSignature> TerrainVectorQuad::DebugRenderStage::CreateAABB_RootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[AABB_PASS_ROOT_PARAM_COUNT];
	rootParams[AABB_PASS_ROOT_PARAM_PER_FRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[AABB_PASS_ROOT_PARAM_CAMERA_SETTINGS_CBV].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[AABB_PASS_ROOT_PARAM_COLOR].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain AABB visualization root signature");
}

ComPtr<ID3D12PipelineState> TerrainVectorQuad::DebugRenderStage::CreateAABB_PSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_WIREFRAME,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
		TRUE,										// MSAA
		FALSE,										// AA line
		0,											// force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	);

	const CD3DX12_DEPTH_STENCIL_DESC dsDesc
	(
		FALSE,																									// depth
		D3D12_DEPTH_WRITE_MASK_ZERO,
		D3D12_COMPARISON_FUNC_ALWAYS,
		FALSE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,		// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS		// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "AABB_min", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_max", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= AABB_rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::AABB_2D),
		.PS						= ShaderBytecode(Shaders::AABB_2D_vis),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= DXGI_FORMAT_UNKNOWN,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain AABB visualization PSO");
	return result;
}

void TerrainVectorQuad::DebugRenderStage::AABBPassPre(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	cmdList.FlushBarriers();
}

void TerrainVectorQuad::DebugRenderStage::AABBPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass, const float (&color)[3], bool visible) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(AABB_PSO.Get());

	cmdList->SetGraphicsRootSignature(AABB_rootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(AABB_PASS_ROOT_PARAM_PER_FRAME_DATA_CBV, World::GetCurFrameGPUDataPtr());
	cmdList->SetGraphicsRootConstantBufferView(AABB_PASS_ROOT_PARAM_CAMERA_SETTINGS_CBV, cameraSettingsGPUAddress);
	cmdList->SetGraphicsRoot32BitConstants(AABB_PASS_ROOT_PARAM_COLOR, size(color), color, 0);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	RenderPasses::RangeRenderPassScope renderPassScope(cmdList, renderPass);

	const auto &queryBatch = get<true>(queryPass->occlusionQueryBatch);
	ID3D12Resource *curVB = NULL;
	do
	{
		const auto &queryData = queryPass->queryStream[rangeBegin];

		// setup VB if changed
		if (curVB != queryData.VB)
		{
			const D3D12_VERTEX_BUFFER_VIEW VB_view =
			{
				(curVB = queryData.VB)->GetGPUVirtualAddress(),
				curVB->GetDesc().Width,
				sizeof(AABB<2>)
			};
			cmdList->IASetVertexBuffers(0, 1, &VB_view);
		}

		queryBatch.Set(cmdList, rangeBegin, visible);
		cmdList->DrawInstanced(4, queryData.count, 0, queryData.startIdx);

	} while (++rangeBegin < rangeEnd);
}
#pragma endregion

Impl::RenderPipeline::PipelineItem (Impl::RenderPipeline::IRenderStage::*TerrainVectorQuad::DebugRenderStage::DoSync(void) const)(unsigned int &length) const
{
	return static_cast<decltype(phaseSelector)>(&DebugRenderStage::GetAABBPassPre);
}

auto TerrainVectorQuad::DebugRenderStage::GetAABBPassPre(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	phaseSelector = static_cast<decltype(phaseSelector)>(&DebugRenderStage::GetVisiblePassRange);
	return RenderPipeline::PipelineItem{ bind_front(&DebugRenderStage::AABBPassPre, shared_from_this()) };
}

auto TerrainVectorQuad::DebugRenderStage::GetVisiblePassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::RenderStageRTBinding> RTBinding{ stageRTBinding, true, false };
	return IterateRenderPass(length, queryPass->queryStream.size(), &RTBinding, { stageZBinding, true, false }, ROPOutput,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&DebugRenderStage::GetCulledPassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) { return bind(&DebugRenderStage::AABBPassRange, shared_from_this(), _1, rangeBegin, rangeEnd, move(renderPass), cref(OcclusionCulling::DebugColors::Terrain::visible), true); });
}

auto TerrainVectorQuad::DebugRenderStage::GetCulledPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::RenderStageRTBinding> RTBinding{ stageRTBinding, false, true };
	return IterateRenderPass(length, queryPass->queryStream.size(), &RTBinding, { stageZBinding, false, true }, ROPOutput,
		[] { RenderPipeline::TerminateStageTraverse(); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RangeRenderPass &renderPass) { return bind(&DebugRenderStage::AABBPassRange, shared_from_this(), _1, rangeBegin, rangeEnd, move(renderPass), cref(OcclusionCulling::DebugColors::Terrain::culled), false); });
}

TerrainVectorQuad::DebugRenderStage::DebugRenderStage(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets) :
	cameraSettingsGPUAddress(cameraSettingsGPUAddress),
	stageRTBinding(ROPTargets),
	stageZBinding(ROPTargets, true, false),
	ROPOutput(ROPTargets)
{
}

RenderPipeline::PipelineStage TerrainVectorQuad::DebugRenderStage::Build(StageExchange &&stageExchange)
{
	queryPass = stageExchange.get();
	return shared_from_this();
}

void TerrainVectorQuad::DebugRenderStage::Schedule(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange)
{
	auto renderStage = allocate_shared<DebugRenderStage>(polymorphic_allocator<DebugRenderStage>(&globalTransientRAM), cameraSettingsGPUAddress, ROPTargets);
	GPUWorkSubmission::AppendPipelineStage<false>(&DebugRenderStage::Build, move(renderStage), move(stageExchange));
}
#pragma endregion

TerrainVectorQuad::TerrainVectorQuad(shared_ptr<TerrainVectorLayer> &&layer, unsigned long int vcount, const function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool srcIB32bit, const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData) :
	layer(move(layer)), subtree(ObjIterator<Object>(getObjectData, 0), ObjIterator<Object>(getObjectData, objCount), Impl::Hierarchy::SplitTechnique::MEAN, .5), subtreeView(subtree),
	IB32bit(vcount > UINT16_MAX), VB_size(vcount * sizeof(float [2])), IB_size(subtree.GetTriCount() * 3 * (IB32bit ? sizeof(uint32_t) : sizeof(uint16_t)))
{
	// create and fill VIB
	{
		const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VB_size + IB_size);
		CheckHR(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,	// clear value
			IID_PPV_ARGS(&VIB)));
		const auto &aabb = subtree.GetAABB();
		// explicitly convert to floats since .x/.y are swizzles which can not be passed to variadic function
#ifdef _MSC_VER
		// it seems that Dinkumware treats "%s" as "%ls" for wide format string
		wstring_convert<codecvt_utf8<WCHAR>> converter;
		NameObjectF(VIB.Get(), L"terrain layer[%u] \"%ls\" quad[<%.f:%.f>-<%.f:%.f>]", this->layer->layerIdx, converter.from_bytes(this->layer->layerName).c_str(), float(aabb.min.x), float(aabb.min.y), float(aabb.max.x), float(aabb.max.y));
#else
		NameObjectF(VIB.Get(), L"terrain layer[%u] \"%s\" quad[<%.f:%.f>-<%.f:%.f>]", this->layer->layerIdx, this->layer->layerName.c_str(), float(aabb.min.x), float(aabb.min.y), float(aabb.max.x), float(aabb.max.y));
#endif

		const CD3DX12_RANGE emptyReadRange(0, 0);
		volatile void *writePtr;
		CheckHR(VIB->Map(0, &emptyReadRange, const_cast<void **>(&writePtr)));

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
inline void TerrainVectorQuad::Schedule(Impl::GPUStreamBuffer::Allocator<sizeof AABB<2>, AABB_VB_name> &GPU_AABB_allocator, const Impl::FrustumCuller<2> &frustumCuller, const float4x4 &frustumXform) const
{
	subtreeView.Schedule<true>(GPU_AABB_allocator, frustumCuller, frustumXform);
}

// 1 call site
inline void TerrainVectorQuad::Issue(MainRenderStage &renderStage, remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const
{
	/*
		note on why copy quad`s data to layer`s quad stream rather than just put quad ptr there:
		'subtreeView' touched here so neighbor quad data is probably in cache line now anyway so accessing it now is cheap
		later during cmd list recording trying to access quad data via ptr would cause ptr chasing and cache pollution
		storing copy of quad data instead of ptr eliminate this performance pitfall
	*/
	subtreeView.Issue(bind_front(&MainRenderStage::IssueOcclusion, ref(renderStage)), bind_front(&MainRenderStage::IssueNodeObjects, ref(renderStage)), occlusionProvider);
	renderStage.IssueQuad(subtree.GetAABB().Center(), VIB.Get(), VB_size, IB_size, IB32bit);
}
#pragma endregion

#pragma region TerrainVectorLayer
void TerrainVectorLayer::QuadDeleter::operator()(const TerrainVectorQuad *quadToRemove) const
{
	quadToRemove->layer->quads.erase(quadLocation);
}

Impl::TerrainVectorLayer::TerrainVectorLayer(shared_ptr<class Renderer::World> &&world, shared_ptr<TerrainMaterials::Interface> &&layerMaterial, unsigned int layerIdx, string &&layerName) :
	world(move(world)), layerIdx(layerIdx), layerName(move(layerName)), layerMaterial(move(layerMaterial))
{
	if (!this->layerMaterial)
		throw logic_error("Attempt to create terrain vector layer with empty material.");
}

Impl::TerrainVectorLayer::~TerrainVectorLayer() = default;

auto Impl::TerrainVectorLayer::AddQuad(unsigned long int vcount, const function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData) -> QuadPtr
{
	quads.emplace_back(shared_from_this(), vcount, fillVB, objCount, IB32bit, getObjectData);
	return { &quads.back(), QuadDeleter{ prev(quads.cend()) } };
}

auto Impl::TerrainVectorLayer::ScheduleRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const float4x4 &frustumXform, UINT64 cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets) const -> StageExchange
{
	return MainRenderStage::Schedule(shared_from_this(), frustumCuller, frustumXform, cameraSettingsGPUAddress, ROPTargets);
}

void Impl::TerrainVectorLayer::ScheduleDebugDrawRenderStage(UINT64 cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange)
{
	DebugRenderStage::Schedule(cameraSettingsGPUAddress, ROPTargets, move(stageExchange));
}

void Renderer::Impl::TerrainVectorLayer::OnFrameFinish()
{
	MainRenderStage::OnFrameFinish();
}
#pragma endregion