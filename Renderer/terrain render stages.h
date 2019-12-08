#pragma once

#include "stdafx.h"
#include "terrain.hh"
#include "GPU stream buffer allocator.h"
#include "GPU work item.h"
#include "render stage.h"
#include "render passes.h"
#include "render pipeline.h"
#include "occlusion query batch.h"

extern std::pmr::synchronized_pool_resource globalTransientRAM;

struct Renderer::TerrainVectorQuad::OcclusionQueryPass final
{
	friend class MainRenderStage;
	friend class DebugRenderStage;

private:
	struct OcclusionQueryGeometry
	{
		ID3D12Resource *VB;
		unsigned long int startIdx;
		unsigned int count;
	};
	std::pmr::vector<OcclusionQueryGeometry> queryStream{ &globalTransientRAM };
	// order is essential (TRANSIENT, then PERSISTENT), index based access used
	std::variant<Impl::OcclusionCulling::QueryBatch<Impl::OcclusionCulling::TRANSIENT>, Impl::OcclusionCulling::QueryBatch<Impl::OcclusionCulling::PERSISTENT>> occlusionQueryBatch;
};

class Renderer::TerrainVectorQuad::MainRenderStage final : public Impl::RenderPipeline::IRenderStage, public std::enable_shared_from_this<MainRenderStage>
{
	friend extern void __cdecl ::InitRenderer();
	friend void TerrainVectorQuad::Issue(MainRenderStage &renderStage, std::remove_const_t<decltype(Impl::OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const;

private:
	const std::shared_ptr<const Renderer::TerrainVectorLayer> parent;
	const D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress;
	const Impl::RenderPipeline::RenderPasses::StageRTBinding stageRTBinding;
	const Impl::RenderPipeline::RenderPasses::StageZBinding stageZBinding;
	const Impl::RenderPipeline::RenderPasses::StageOutput stageOutput;
	std::promise<std::shared_ptr<const OcclusionQueryPass>> queryPassPromise{ std::allocator_arg, std::pmr::polymorphic_allocator<decltype(queryPassPromise)>(&globalTransientRAM) };

#pragma region occlusion query pass
private:
	static WRL::ComPtr<ID3D12RootSignature> cullPassRootSig, CreateCullPassRootSig();
	static WRL::ComPtr<ID3D12PipelineState> cullPassPSO, CreateCullPassPSO();

private:
	const std::shared_ptr<OcclusionQueryPass> queryPass;

private:
	inline void CullPassPre(Impl::CmdListPool::CmdList &target) const, CullPassPost(Impl::CmdListPool::CmdList &target) const;
	void CullPassRange(Impl::CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const Impl::RenderPipeline::RenderPasses::RenderPass &renderPass) const;

private:
	inline void SetupCullPass();
	void IssueOcclusion(ViewNode::OcclusionQueryGeometry occlusionQueryGeometry);
	inline void UpdateCullPassCache();
#pragma endregion

#pragma region main pass
private:
	struct RenderData
	{
		unsigned long int startIdx, triCount;
		decltype(Impl::OcclusionCulling::QueryBatchBase::npos) occlusion;
		unsigned long int startQuadIdx;
	};
	struct Quad
	{
		unsigned long int streamEnd;
		HLSL::float2 center;
		ID3D12Resource *VIB;	// no reference/lifetime tracking required, it would induce unnecessary overhead (lifetime tracking leads to mutex locks)
		unsigned long int VB_size, IB_size;
		bool IB32bit;
	};
	std::pmr::vector<RenderData> renderStream{ &globalTransientRAM };
	std::pmr::vector<Quad> quadStram{ &globalTransientRAM };

private:
	inline void MainPassPre(Impl::CmdListPool::CmdList &target) const, MainPassPost(Impl::CmdListPool::CmdList &target) const;
	void MainPassRange(Impl::CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const Impl::RenderPipeline::RenderPasses::RenderPass &renderPass) const;

private:
	inline void SetupMainPass();
	void IssueCluster(unsigned long int startIdx, unsigned long int triCount, decltype(Impl::OcclusionCulling::QueryBatchBase::npos) occlusion);
	void IssueExclusiveObjects(const TreeNode &node, decltype(Impl::OcclusionCulling::QueryBatchBase::npos) occlusion);
	void IssueChildren(const TreeNode &node, decltype(Impl::OcclusionCulling::QueryBatchBase::npos) occlusion);
	void IssueWholeNode(const TreeNode &node, decltype(Impl::OcclusionCulling::QueryBatchBase::npos) occlusion);
	bool IssueNodeObjects(const TreeNode &node, decltype(Impl::OcclusionCulling::QueryBatchBase::npos) coarseOcclusion,  decltype(Impl::OcclusionCulling::QueryBatchBase::npos) fineOcclusion, ViewNode::Visibility visibility);
	void IssueQuad(HLSL::float2 quadCenter, ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit);
	inline void UpdateMainPassCache();
#pragma endregion

private:
	void StagePre(Impl::CmdListPool::CmdList &target) const, StagePost(Impl::CmdListPool::CmdList &target) const;
	void CullPass2MainPass(Impl::CmdListPool::CmdList &target) const;

private:
	// Inherited via IRenderStage
	virtual Impl::RenderPipeline::PipelineItem (IRenderStage::*DoSync(void) const)(unsigned int &length) const override;

private:
	Impl::RenderPipeline::PipelineItem
		GetStagePre(unsigned int &length) const, GetStagePost(unsigned int &length) const,
		GetCullPassRange(unsigned int &length) const, GetMainPassRange(unsigned int &length) const,
		GetCullPass2MainPass(unsigned int &length) const;

private:
	inline void Setup(), SetupOcclusionQueryBatch(decltype(Impl::OcclusionCulling::QueryBatchBase::npos) maxOcclusion);
	inline void UpdateCaches();
	inline Impl::RenderPipeline::PipelineStage Build(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform);

private:
	static std::optional<Impl::GPUStreamBuffer::Allocator<sizeof(AABB<2>), TerrainVectorQuad::AABB_VB_name>> GPU_AABB_allocator;

private:
public:
	inline explicit MainRenderStage(std::shared_ptr<const Renderer::TerrainVectorLayer> &&parent,
		D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const Impl::RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &stageExchangeResult);

public:
	inline static StageExchange Schedule(std::shared_ptr<const Renderer::TerrainVectorLayer> parent, const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform,
		D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const Impl::RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets);
	inline static void OnFrameFinish();
};

class Renderer::TerrainVectorQuad::DebugRenderStage final : public Impl::RenderPipeline::IRenderStage, public std::enable_shared_from_this<DebugRenderStage>
{
	friend extern void __cdecl ::InitRenderer();

private:
	const D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress;
	const Impl::RenderPipeline::RenderPasses::StageRTBinding stageRTBinding;
	const Impl::RenderPipeline::RenderPasses::StageZBinding stageZBinding;
	const Impl::RenderPipeline::RenderPasses::StageOutput stageOutput;

#pragma region visualize occlusion pass
private:
	static WRL::ComPtr<ID3D12RootSignature> AABB_rootSig, CreateAABB_RootSig();
	static WRL::ComPtr<ID3D12PipelineState> AABB_PSO, CreateAABB_PSO();

private:
	std::shared_ptr<const OcclusionQueryPass> queryPass;

private:
	void AABBPassPre(Impl::CmdListPool::CmdList &target) const, AABBPassRange(Impl::CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const Impl::RenderPipeline::RenderPasses::RenderPass &renderPass, const float (&color)[3], bool visible) const;
#pragma endregion

private:
	// Inherited via IRenderStage
	virtual Impl::RenderPipeline::PipelineItem (IRenderStage::*DoSync(void) const)(unsigned int &length) const override;

private:
	Impl::RenderPipeline::PipelineItem
		GetAABBPassPre(unsigned int &length) const,
		GetVisiblePassRange(unsigned int &length) const, GetCulledPassRange(unsigned int &length) const;

private:
	inline Impl::RenderPipeline::PipelineStage Build(StageExchange &&stageExchange);

private:
public:
	inline explicit DebugRenderStage(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const Impl::RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets);

public:
	inline static void Schedule(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const Impl::RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange);
};