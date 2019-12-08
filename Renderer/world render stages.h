#pragma once

#include "stdafx.h"
#include "world.hh"
#include "GPU stream buffer allocator.h"
#include "GPU work item.h"
#include "render stage.h"
#include "render passes.h"
#include "render pipeline.h"
#include "SO buffer.h"
#include "occlusion query batch.h"

extern std::pmr::synchronized_pool_resource globalTransientRAM;

struct Renderer::Impl::World::OcclusionQueryPasses final
{
	friend class MainRenderStage;
	friend class DebugRenderStage;

private:
	struct OcclusionQueryGeometry
	{
		ID3D12Resource *VB;
		unsigned long int startIdx, xformedStartIdx;
		unsigned int count;
	};
	std::pmr::vector<OcclusionQueryGeometry> queryStream{ &globalTransientRAM };
	// order is essential (TRANSIENT, then DUAL), index based access used
	std::variant<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>, OcclusionCulling::QueryBatch<OcclusionCulling::DUAL>> occlusionQueryBatch;
	SOBuffer::Handle xformedAABBs;
	static constexpr UINT xformedAABBSize = sizeof(float[4])/*corner*/ + sizeof(float[3][4])/*extents*/;
};

class Renderer::Impl::World::MainRenderStage final : public RenderPipeline::IRenderStage, public std::enable_shared_from_this<MainRenderStage>
{
	friend extern void __cdecl ::InitRenderer();

private:
	const std::shared_ptr<const Renderer::World> parent;
	WorldViewContext &viewCtx;
	const D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress;
	const RenderPasses::StageRTBinding stageRTBinding;
	const RenderPasses::StageZBinding stageZPrecullBinding, stageZBinding;
	const RenderPasses::StageOutput stageOutput;
	std::promise<std::shared_ptr<const OcclusionQueryPasses>> queryPassesPromise{ std::allocator_arg, std::pmr::polymorphic_allocator<decltype(queryPassesPromise)>(&globalTransientRAM) };

#pragma region occlusion query passes
private:
	static WRL::ComPtr<ID3D12RootSignature> xformAABB_rootSig, CreateXformAABB_RootSig();
	static WRL::ComPtr<ID3D12PipelineState> xformAABB_PSO, CreateXformAABB_PSO();
	static WRL::ComPtr<ID3D12RootSignature> cullPassRootSig, CreateCullPassRootSig();
	static std::array<WRL::ComPtr<ID3D12PipelineState>, 2> cullPassPSOs, CreateCullPassPSOs();

private:
	const std::shared_ptr<OcclusionQueryPasses> queryPasses;

private:
	void XformAABBPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd) const;
	void CullPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool final) const;

private:
	inline void SetupCullPass();
	void IssueOcclusion(decltype(bvhView)::Node::OcclusionQueryGeometry occlusionQueryGeometry, unsigned long int &counter);
	inline void UpdateCullPassCache();
#pragma endregion

#pragma region main passes
private:
	struct RenderData
	{
		const Renderer::Instance *instance;
		decltype(OcclusionCulling::QueryBatchBase::npos) occlusion;
	};
	std::pmr::vector<RenderData> renderStreams[2]{ std::remove_extent_t<decltype(renderStreams)>{&globalTransientRAM}, std::remove_extent_t<decltype(renderStreams)>{&globalTransientRAM} };

private:
	void MainPassPre(CmdListPool::CmdList &target) const, MainPassPost(CmdListPool::CmdList &target) const;
	void MainPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool final) const;

private:
	inline void SetupMainPass();
	void IssueObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion);
	bool IssueNodeObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion, decltype(OcclusionCulling::QueryBatchBase::npos), decltype(bvhView)::Node::Visibility visibility);
	inline void UpdateMainPassCache();
#pragma endregion

private:
	void StagePre(CmdListPool::CmdList &target) const, StagePost(CmdListPool::CmdList &target) const;
	void XformAABBPass2CullPass(CmdListPool::CmdList &target) const, CullPass2MainPass(CmdListPool::CmdList &target, bool final) const, MainPass2CullPass(CmdListPool::CmdList &target) const;

private:
	// Inherited via IRenderStage
	virtual RenderPipeline::PipelineItem (IRenderStage::*DoSync(void) const)(unsigned int &length) const override;

private:
	RenderPipeline::PipelineItem
		GetStagePre(unsigned int &length) const, GetStagePost(unsigned int &length) const,
		GetXformAABBPassRange(unsigned int &length) const,
		GetFirstCullPassRange(unsigned int &length) const, GetSecondCullPassRange(unsigned int &length) const,
		GetFirstMainPassRange(unsigned int &length) const, GetSecondMainPassRange(unsigned int &length) const,
		GetXformAABBPass2FirstCullPass(unsigned int &length) const, GetFirstCullPass2FirstMainPass(unsigned int &length) const, GetFirstMainPass2SecondCullPass(unsigned int &length) const, GetSecondCullPass2SecondMainPass(unsigned int &length) const,
		GetMainPassPre(unsigned int &length) const, GetMainPassRange(unsigned int &length) const, GetMainPassPost(unsigned int &length) const;

private:
	inline void Setup(), SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion);
	inline void UpdateCaches();
	inline RenderPipeline::PipelineStage Build(const HLSL::float4x4 &frustumXform, const HLSL::float4x3 &viewXform);

private:
	static constexpr const WCHAR AABB_VB_name[] = L"3D objects occlusion query boxes", xformedAABB_SO_name[] = L"3D objects xformed occlusion query boxes";
	static std::optional<GPUStreamBuffer::Allocator<sizeof(AABB<3>), AABB_VB_name>> GPU_AABB_allocator;
	static SOBuffer::Allocator<xformedAABB_SO_name> xformedAABBsStorage;

private:
	inline static RenderPasses::StageZBinding MakeZPrecullBinding(WorldViewContext &viewCtx, const RenderPasses::PipelineROPTargets &ROPTargets);

private:
public:
	inline explicit MainRenderStage(std::shared_ptr<const Renderer::World> parent, WorldViewContext &viewCtx,
		D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &stageExchangeResult);

public:
	inline static StageExchange Schedule(std::shared_ptr<const Renderer::World> parent, WorldViewContext &viewCtx, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 &viewXform,
		D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets);
	inline static void OnFrameFinish();
};

class Renderer::Impl::World::DebugRenderStage final : public RenderPipeline::IRenderStage, public std::enable_shared_from_this<DebugRenderStage>
{
	friend extern void __cdecl ::InitRenderer();

private:
	const D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress;
	const RenderPasses::StageRTBinding stageRTBinding;
	const RenderPasses::StageZBinding stageZBinding;
	const RenderPasses::StageOutput stageOutput;

#pragma region visualize occlusion pass
private:
	static WRL::ComPtr<ID3D12RootSignature> AABB_rootSig, CreateAABB_RootSig();
	static std::array<WRL::ComPtr<ID3D12PipelineState>, 2> AABB_PSOs, CreateAABB_PSOs();

private:
	std::shared_ptr<const OcclusionQueryPasses> queryPasses;

private:
	void AABBPassPre(CmdListPool::CmdList &target) const, AABBPassPost(CmdListPool::CmdList &target) const;
	void AABBPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool visible) const;
#pragma endregion

private:
	// Inherited via IRenderStage
	virtual RenderPipeline::PipelineItem (IRenderStage::*DoSync(void) const)(unsigned int &length) const override;

private:
	RenderPipeline::PipelineItem
		GetAABBPassPre(unsigned int &length) const, GetAABBPassPost(unsigned int &length) const,
		GetHiddenPassRange(unsigned int &length) const, GetVisiblePassRange(unsigned int &length) const;

private:
	inline RenderPipeline::PipelineStage Build(StageExchange &&stageExchange);

public:
	inline explicit DebugRenderStage(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets);

public:
	inline static void Schedule(D3D12_GPU_VIRTUAL_ADDRESS cameraSettingsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange);
};