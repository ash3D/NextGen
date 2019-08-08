#include "stdafx.h"
#include "world.hh"
#include "viewport.hh"
#include "terrain.hh"
#include "instance.hh"
#include "tracked resource.inl"
#include "world hierarchy.inl"
#include "GPU stream buffer allocator.inl"
#include "cmdlist pool.inl"
#include "sun.h"
#include "world view context.h"
#include "frustum culling.h"
#include "occlusion query shceduling.h"
#include "occlusion query visualization.h"
#include "frame versioning.h"
#include "global GPU buffer data.h"
#include "static objects data.h"
#include "GPU work submission.h"
#include "shader bytecode.h"
#include "config.h"

namespace Shaders
{
#	include "AABB_3D_xform.csh"
#	include "AABB_3D.csh"
#	include "AABB_3D_vis.csh"
}

using namespace std;
using namespace Renderer;
using namespace HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

static void CheckSunZenithArg(float zenith)
{
	if (fabs(zenith) > M_PI_2)
		throw domain_error("Invalid sun zenith angle");
}

ComPtr<ID3D12Resource> Impl::World::CreateGlobalGPUBuffer()
{
	ComPtr<ID3D12Resource> buffer;
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(GlobalGPUBufferData)/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(buffer.GetAddressOf())));
	NameObject(buffer.Get(), L"global GPU buffer");
	
	// fill AABB vis colors and box IB
	{
		CD3DX12_RANGE range(offsetof(GlobalGPUBufferData, aabbVisColorsCB), offsetof(GlobalGPUBufferData, aabbVisColorsCB));
		volatile GlobalGPUBufferData *CPU_ptr;
		CheckHR(buffer->Map(0, &range, const_cast<void **>(reinterpret_cast<volatile void **>(&CPU_ptr))));

		namespace Colors = OcclusionCulling::DebugColors::Object3D;
		CPU_ptr->aabbVisColorsCB[false].culled		= Colors::culled;
		CPU_ptr->aabbVisColorsCB[false].rendered[0]	= Colors::hidden[0];
		CPU_ptr->aabbVisColorsCB[false].rendered[1]	= Colors::hidden[1];
		CPU_ptr->aabbVisColorsCB[true].culled		= Colors::culled;
		CPU_ptr->aabbVisColorsCB[true].rendered[0]	= Colors::visible[0];
		CPU_ptr->aabbVisColorsCB[true].rendered[1]	= Colors::visible[1];

		memcpy(const_cast<remove_volatile_t<decltype(CPU_ptr->boxIB)> &>(CPU_ptr->boxIB), CPU_ptr->boxIBInitData, sizeof CPU_ptr->boxIB);
		
		range.End += sizeof CPU_ptr->aabbVisColorsCB + sizeof CPU_ptr->boxIB;
		buffer->Unmap(0, &range);
	}

	return move(buffer);
}

auto Impl::World::MapGlobalGPUBuffer(const D3D12_RANGE *readRange) -> volatile GlobalGPUBufferData *
{
	volatile GlobalGPUBufferData *CPU_ptr;
	CheckHR(globalGPUBuffer->Map(0, readRange, const_cast<void **>(reinterpret_cast<volatile void **>(&CPU_ptr))));
	return CPU_ptr;
}

UINT64 Impl::World::GetCurFrameGPUDataPtr()
{
	return globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBufferData::PerFrameData::CurFrameCB_offset();
}

// defined here, not in class in order to eliminate dependency on "instance.hh" in "world.hh"
#if defined _MSC_VER && _MSC_VER <= 1922
inline const AABB<3> &Impl::World::BVHObject::GetAABB() const noexcept
#else
inline const auto &Impl::World::BVHObject::GetAABB() const noexcept
#endif
{
	return instance->GetWorldAABB();
}

inline unsigned long int Impl::World::BVHObject::GetTriCount() const noexcept
{
	return instance->GetObject3D().GetTriCount();
}

void Impl::World::InstanceDeleter::operator()(const Renderer::Instance *instanceToRemove) const
{
	instanceToRemove->GetWorld()->InvalidateStaticObjects();
	instanceToRemove->GetWorld()->staticObjects.erase(instanceLocation);
}

void Impl::World::InvalidateStaticObjects()
{
	staticObjectsCB.Reset();
	bvh.Reset();
	bvhView.Reset();
}

#pragma region occlusion query passes
ComPtr<ID3D12RootSignature> Impl::World::CreateXformAABB_RootSig()
{
	ComPtr<ID3D12VersionedRootSignatureDeserializer> rootSigProvider;
	CheckHR(D3D12CreateVersionedRootSignatureDeserializer(Shaders::AABB_3D_xform, sizeof Shaders::AABB_3D_xform, IID_PPV_ARGS(rootSigProvider.GetAddressOf())));
	return CreateRootSignature(*rootSigProvider->GetUnconvertedRootSignatureDesc(), L"Xform 3D AABB root signature");
}

ComPtr<ID3D12PipelineState> Impl::World::CreateXformAABB_PSO()
{
	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "AABB_min", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AABB_max", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	const D3D12_SO_DECLARATION_ENTRY soEntries[] =
	{
		{0, "AABB_corner", 0, 0, 4, 0},
		{0, "AABB_extent", 0, 0, 4, 0},
		{0, "AABB_extent", 1, 0, 4, 0},
		{0, "AABB_extent", 2, 0, 4, 0}
	};

	const D3D12_STREAM_OUTPUT_DESC soDesc =
	{
		soEntries, size(soEntries),
		&xformedAABBSize, 1,
		D3D12_SO_NO_RASTERIZED_STREAM
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= xformAABB_rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::AABB_3D_xform),
		.StreamOutput			= soDesc,
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		.DepthStencilState		= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		.DSVFormat				= DXGI_FORMAT_UNKNOWN,
		.SampleDesc				= {1},
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"Xform 3D AABB PSO");
	return move(result);
}

ComPtr<ID3D12RootSignature> Impl::World::CreateCullPassRootSig()
{
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(0, (const D3D12_ROOT_PARAMETER *)NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"world objects occlusion query root signature");
}

auto Impl::World::CreateCullPassPSOs() -> decltype(cullPassPSOs)
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
#if 1
		D3D12_CULL_MODE_BACK,
#else
		D3D12_CULL_MODE_NONE,
#endif
		TRUE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
		FALSE,										// MSAA
		TRUE,										// AA line
		0,											// force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	);

	const CD3DX12_DEPTH_STENCIL_DESC dsDescs[2] =
	{
		CD3DX12_DEPTH_STENCIL_DESC
		{
			TRUE,																									// depth
			D3D12_DEPTH_WRITE_MASK_ZERO,
			D3D12_COMPARISON_FUNC_LESS,
			TRUE,																									// stencil
			D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
			0xef,																									// stencil write mask
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
		},
		CD3DX12_DEPTH_STENCIL_DESC
		{
			TRUE,																									// depth
			D3D12_DEPTH_WRITE_MASK_ZERO,
			D3D12_COMPARISON_FUNC_LESS,
			FALSE,																									// stencil
			D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
			D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,		// front
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS		// back
		}
	};

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "AABB_corner", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= cullPassRootSig.Get(),
		.VS						= ShaderBytecode(Shaders::AABB_3D),
		.BlendState				= blendDesc,
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDescs[0],
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	decltype(cullPassPSOs) result;
	
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[0].GetAddressOf())));
	NameObject(result[0].Get(), L"world objects first occlusion query pass PSO");

	PSO_desc.DepthStencilState = dsDescs[1];

	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[1].GetAddressOf())));
	NameObject(result[1].Get(), L"world objects second occlusion query pass PSO");

	return move(result);
}

void Impl::World::XformAABBPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(xformAABB_PSO.Get());

	cmdList->SetGraphicsRootSignature(xformAABB_rootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, GetCurFrameGPUDataPtr());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

	// set SO
	cmdList->SOSetTargets(0, 1, &xformedAABBs.GetSOView());

	ID3D12Resource *curSrcVB = NULL;
	do
	{
		const auto &queryData = queryStream[rangeBegin];

		// setup VB if changed
		if (curSrcVB != queryData.VB)
		{
			const D3D12_VERTEX_BUFFER_VIEW VB_view =
			{
				(curSrcVB = queryData.VB)->GetGPUVirtualAddress(),
				curSrcVB->GetDesc().Width,
				sizeof(AABB<3>)
			};
			cmdList->IASetVertexBuffers(0, 1, &VB_view);
		}

		cmdList->DrawInstanced(queryData.count, 1, queryData.startIdx, 0);
	} while (++rangeBegin < rangeEnd);
}

void Impl::World::CullPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool final) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(cullPassPSOs[final].Get());

	cmdList->SetGraphicsRootSignature(cullPassRootSig.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// setup IB/VB
	{
		const D3D12_INDEX_BUFFER_VIEW IB_view =
		{
			globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBufferData::BoxIB_offset(),
			sizeof GlobalGPUBufferData::boxIB,
			DXGI_FORMAT_R16_UINT
		};
		const D3D12_VERTEX_BUFFER_VIEW VB_view =
		{
			xformedAABBs.GetGPUPtr(),
			xformedAABBs.GetSize(),
			xformedAABBSize
		};
		cmdList->IASetIndexBuffer(&IB_view);
		cmdList->IASetVertexBuffers(0, 1, &VB_view);
	}

	RenderPasses::RenderPassScope renderPassScope(cmdList, renderPass);

	const OcclusionCulling::QueryBatchBase &queryBatch = visit([](const OcclusionCulling::QueryBatchBase &queryBatch) noexcept -> const auto & { return queryBatch; }, occlusionQueryBatch);
	do
	{
		const auto &queryData = queryStream[rangeBegin];

		queryBatch.Start(cmdList, rangeBegin);
		if (final)
		{
			// eliminate rendering already rendered objects again
			if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
				transientQueryBatch->Set(cmdList, rangeBegin, false);
			else
				get<OcclusionCulling::QueryBatch<OcclusionCulling::DUAL>>(occlusionQueryBatch).Set(cmdList, rangeBegin, false, false);
		}
		cmdList->DrawIndexedInstanced(14, queryData.count, 0, 0, queryData.xformedStartIdx);
		queryBatch.Stop(cmdList, rangeBegin);

	} while (++rangeBegin < rangeEnd);
}

// 1 call site
inline void Impl::World::SetupCullPass() const
{
	queryStream.clear();
}

// 1 call site
void Impl::World::IssueOcclusion(decltype(bvhView)::Node::OcclusionQueryGeometry occlusionQueryGeometry, unsigned long int &counter) const
{
	queryStream.push_back({ occlusionQueryGeometry.VB, occlusionQueryGeometry.startIdx, counter, occlusionQueryGeometry.count });
	counter += occlusionQueryGeometry.count;
}
#pragma endregion

#pragma region main passes
//void Impl::World::MainPassPre(CmdListPool::CmdList &cmdList) const
//{
//}
//
//void Impl::World::MainPassPost(CmdListPool::CmdList &cmdList) const
//{
//}

void Impl::World::MainPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool final) const
{
	assert(rangeBegin < rangeEnd);

	const auto &renderStream = renderStreams[final];

	cmdList.Setup(renderStream[rangeBegin].instance->GetStartPSO());

	decltype(staticObjects)::value_type::Setup(cmdList, GetCurFrameGPUDataPtr(), tonemapParamsGPUAddress);

	RenderPasses::RenderPassScope renderPassScope(cmdList, renderPass);

	for_each(next(renderStream.cbegin(), rangeBegin), next(renderStream.cbegin(), rangeEnd), [&, curOcclusionQueryIdx = OcclusionCulling::QueryBatchBase::npos, final](remove_reference_t<decltype(renderStream)>::value_type renderData) mutable
	{
		if (curOcclusionQueryIdx != renderData.occlusion)
		{
			curOcclusionQueryIdx = renderData.occlusion;
			if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
				transientQueryBatch->Set(cmdList, curOcclusionQueryIdx);
			else
				get<OcclusionCulling::QueryBatch<OcclusionCulling::DUAL>>(occlusionQueryBatch).Set(cmdList, curOcclusionQueryIdx, final);
		}
		renderData.instance->Render(cmdList);
	});
}

// 1 call site
inline void Impl::World::SetupMainPass() const
{
	for (auto &stream : renderStreams) stream.clear();
}

// 1 call site
inline void Impl::World::IssueObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion) const
{
	const auto issue2stream = [range = node.GetExclusiveObjectsRange(), occlusion](remove_extent_t<decltype(renderStreams)> &renderStream)
	{
		transform(range.first, range.second, back_inserter(renderStream), [occlusion](const Renderer::Instance *instance) noexcept -> typename/*MSVC 1921/1922*/ remove_reference_t<decltype(renderStream)>::value_type { return { instance, occlusion }; });
	};

	issue2stream(renderStreams[0]);
	
	// do not render unconditional occluders again on second pass
	if (occlusion != OcclusionCulling::QueryBatchBase::npos)
		issue2stream(renderStreams[1]);
}

bool Impl::World::IssueNodeObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion, decltype(OcclusionCulling::QueryBatchBase::npos), decltype(bvhView)::Node::Visibility visibility) const
{
	if (visibility != decltype(visibility)::Culled)
	{
		IssueObjects(node, occlusion);
		return true;
	}
	return false;
}
#pragma endregion

#pragma region visualize occlusion pass
enum
{
	AABB_PASS_ROOT_PARAM_COLOR_CBV,
	AABB_PASS_ROOT_PARAM_TONEMAP_PARAMS_CBV,
	AABB_PASS_ROOT_PARAM_VISIBILITY1_SRV,
	AABB_PASS_ROOT_PARAM_VISIBILITY2_SRV,
	AABB_PASS_ROOT_PARAM_VISIBLILITY_OFFSET,
	AABB_PASS_ROOT_PARAM_COUNT
};

ComPtr<ID3D12RootSignature> Impl::World::CreateAABB_RootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[5];
	rootParams[AABB_PASS_ROOT_PARAM_COLOR_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[AABB_PASS_ROOT_PARAM_TONEMAP_PARAMS_CBV].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[AABB_PASS_ROOT_PARAM_VISIBILITY1_SRV].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[AABB_PASS_ROOT_PARAM_VISIBILITY2_SRV].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[4].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);
	return CreateRootSignature(sigDesc, L"world 3D objects AABB visualization root signature");
}

auto Impl::World::CreateAABB_PSOs() -> decltype(AABB_PSOs)
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_WIREFRAME,
#if 0
		D3D12_CULL_MODE_BACK,
#else
		D3D12_CULL_MODE_NONE,
#endif
		TRUE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
		TRUE,										// MSAA
		FALSE,										// AA line
		0,											// force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	);

	const CD3DX12_DEPTH_STENCIL_DESC dsDescHidden
	(
		TRUE,																									// depth
		D3D12_DEPTH_WRITE_MASK_ZERO,
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		FALSE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,		// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS		// back
	);

	// TODO: make it global and share with cull pass PSO
	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "AABB_corner", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "AABB_extent", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= AABB_rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::AABB_3D),
		.PS						= ShaderBytecode(Shaders::AABB_3D_vis),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDescHidden,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	decltype(cullPassPSOs) result;
	
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[0].GetAddressOf())));
	NameObject(result[0].Get(), L"world 3D objects hidden AABB visualization PSO");

	// patch hidden -> visible
	PSO_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[1].GetAddressOf())));
	NameObject(result[1].Get(), L"world 3D objects visible AABB visualization PSO");

	return move(result);
}

void Impl::World::AABBPassPre(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	// before AABB pass "action"
	cmdList.FlushBarriers();
}

void Impl::World::AABBPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass, bool visible) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(AABB_PSOs[visible].Get());

	const auto &queryBatch = get<true>(occlusionQueryBatch);

	cmdList->SetGraphicsRootSignature(AABB_rootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(AABB_PASS_ROOT_PARAM_COLOR_CBV, globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBufferData::AABB_3D_VisColors::CB_offset(visible));
	cmdList->SetGraphicsRootConstantBufferView(AABB_PASS_ROOT_PARAM_TONEMAP_PARAMS_CBV, tonemapParamsGPUAddress);
	cmdList->SetGraphicsRootShaderResourceView(AABB_PASS_ROOT_PARAM_VISIBILITY1_SRV, queryBatch.GetGPUPtr(false));
	cmdList->SetGraphicsRootShaderResourceView(AABB_PASS_ROOT_PARAM_VISIBILITY2_SRV, queryBatch.GetGPUPtr(true));
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// setup IB/VB (consider sharing this with cull pass)
	{
		const D3D12_INDEX_BUFFER_VIEW IB_view =
		{
			globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBufferData::BoxIB_offset(),
			sizeof GlobalGPUBufferData::boxIB,
			DXGI_FORMAT_R16_UINT
		};
		const D3D12_VERTEX_BUFFER_VIEW VB_view =
		{
			xformedAABBs.GetGPUPtr(),
			xformedAABBs.GetSize(),
			xformedAABBSize
		};
		cmdList->IASetIndexBuffer(&IB_view);
		cmdList->IASetVertexBuffers(0, 1, &VB_view);
	}

	RenderPasses::RenderPassScope renderPassScope(cmdList, renderPass);

	do
	{
		const auto &queryData = queryStream[rangeBegin];

		cmdList->SetGraphicsRoot32BitConstant(AABB_PASS_ROOT_PARAM_VISIBLILITY_OFFSET, rangeBegin * sizeof(UINT64), 0);
		cmdList->DrawIndexedInstanced(14, queryData.count, 0, 0, queryData.xformedStartIdx);

	} while (++rangeBegin < rangeEnd);
}

void Impl::World::AABBPassPost(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	xformedAABBs.Finish(cmdList);
}
#pragma endregion

void Impl::World::StagePre(CmdListPool::CmdList &cmdList) const
{
	// specify NULL to reset possible predication
	cmdList.Setup(NULL);

	// just copy for now, do reprojection in future
	if (viewCtx->ZBufferHistory)
	{
		if (const auto targetZDesc = ZBindingPrecull->GetZBuffer()->GetDesc(), historyZDesc = viewCtx->ZBufferHistory->GetDesc(); targetZDesc.Width == historyZDesc.Width && targetZDesc.Height == historyZDesc.Height)
		{
			{
				initializer_list<D3D12_RESOURCE_BARRIER> barriers
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ZBindingPrecull->GetZBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_DEST, 0),
					CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
				};
				cmdList.ResourceBarrier(barriers);
				cmdList.FlushBarriers<true>();
			}
			cmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(ZBindingPrecull->GetZBuffer(), 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(viewCtx->ZBufferHistory.Get(), 0), NULL);
			{
				initializer_list<D3D12_RESOURCE_BARRIER> barriers
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ZBindingPrecull->GetZBuffer(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0),
					CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
				};
				cmdList.ResourceBarrier(barriers);
				cmdList.FlushBarriers<true>();
			}
		}
	}

	xformedAABBs.StartSO(cmdList);

	// before xform AABB pass "action"
	cmdList.FlushBarriers();
}

void Impl::World::XformAABBPass2CullPass(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	xformedAABBs.UseSOResults(cmdList);

	// before cull pass "action"
	cmdList.FlushBarriers();
}

void Impl::World::CullPass2MainPass(CmdListPool::CmdList &cmdList, bool final) const
{
	// specify NULL to reset possible predication
	cmdList.Setup(NULL);

	if (final)
	{
		// xformedAABBs used for debug draw -> finish it later if debug draw enabled
		extern bool enableDebugDraw;
		if (!enableDebugDraw)
			xformedAABBs.Finish(cmdList);
	}

	visit([&cmdList, final](const auto &queryBatch) { queryBatch.Resolve(cmdList, final); }, occlusionQueryBatch);

	// before main pass "action"
	cmdList.FlushBarriers();
}

//void Impl::World::MainPass2CullPass(CmdListPool::CmdList &cmdList) const
//{
//	cmdList.Setup();
//
//}

void Impl::World::StagePost(CmdListPool::CmdList &cmdList) const
{
	// specify NULL to reset possible predication
	cmdList.Setup(NULL);

	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
		transientQueryBatch->Finish(cmdList);

#if 1
	// enables 'force' for FlushBarriers() below
	cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(ZBindingMain->GetZBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE));

	const auto targetZDesc = ZBindingMain->GetZBuffer()->GetDesc();
	if (viewCtx->ZBufferHistory)
		if (const auto historyZDesc = viewCtx->ZBufferHistory->GetDesc(); targetZDesc.Width != historyZDesc.Width || targetZDesc.Height != historyZDesc.Height)
			viewCtx->ZBufferHistory.Reset();
	if (viewCtx->ZBufferHistory)
		cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY));
	else
	{
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&targetZDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,
			IID_PPV_ARGS(viewCtx->ZBufferHistory.ReleaseAndGetAddressOf())
		));
		NameObjectF(viewCtx->ZBufferHistory.Get(), L"Z buffer history (world view context %p) [%lu]", viewCtx, viewCtx->ZBufferHistoryVersion++);
	}

	cmdList.FlushBarriers<true>();
	cmdList->CopyResource(viewCtx->ZBufferHistory.Get(), ZBindingMain->GetZBuffer());
	{
		initializer_list<D3D12_RESOURCE_BARRIER> barriers
		{
			CD3DX12_RESOURCE_BARRIER::Transition(ZBindingMain->GetZBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE),
			CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
		};
		cmdList.ResourceBarrier(barriers);
	}
#endif
}

void Impl::World::Sync() const
{
	xformedAABBs.Sync();
	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
		transientQueryBatch->Sync();
}

auto Impl::World::GetStagePre(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetXformAABBPassRange);
	return RenderPipeline::RenderStageItem{ bind(&World::StagePre, this, _1) };
}

auto Impl::World::GetXformAABBPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetXformAABBPass2FirstCullPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::XformAABBPassRange, this, _1, rangeBegin, rangeEnd); });
}

auto Impl::World::GetXformAABBPass2FirstCullPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetFirstCullPassRange);
	return RenderPipeline::RenderStageItem{ bind(&World::XformAABBPass2CullPass, this, _1) };
}

auto Impl::World::GetFirstCullPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), nullptr, { *ZBindingPrecull, true, true }, *outputMain,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetFirstCullPass2FirstMainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::CullPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), false); });
}

auto Impl::World::GetFirstCullPass2FirstMainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetFirstMainPassRange);
	return RenderPipeline::RenderStageItem{ bind(&World::CullPass2MainPass, this, _1, false) };
}

auto Impl::World::GetFirstMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::StageRTBinding> RTBinding{ *RTBindinMain, true, false };
	return IterateRenderPass(length, renderStreams[0].size(), &RTBinding, { *ZBindingMain, true, false }, *outputMain,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::/*GetFirstMainPass2SecondCullPass*/GetSecondCullPassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::MainPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), false); });
}

//auto Impl::World::GetFirstMainPass2SecondCullPass(unsigned int &) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetSecondCullPassRange);
//	return bind(&World::MainPass2CullPass, this, _1);
//}

auto Impl::World::GetSecondCullPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), nullptr, { *ZBindingMain, false, false }, *outputMain,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetSecondCullPass2SecondMainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::CullPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), true); });
}

auto Impl::World::GetSecondCullPass2SecondMainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetSecondMainPassRange);
	return RenderPipeline::RenderStageItem{ bind(&World::CullPass2MainPass, this, _1, true) };
}

auto Impl::World::GetSecondMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::StageRTBinding> RTBinding{ *RTBindinMain, false, true };
	return IterateRenderPass(length, renderStreams[1].size(), &RTBinding, { *ZBindingMain, false, true }, *outputMain,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetStagePost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::MainPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), true); });
}

auto Impl::World::GetStagePost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPipeline::TerminateStageTraverse();
	return RenderPipeline::RenderStageItem{ bind(&World::StagePost, this, _1) };
}

//auto Impl::World::GetMainPassPre(unsigned int &) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	phaseSelector = &World::GetMainPassRange;
//	return bind(&World::MainPassPre, this, _1);
//}

//auto Impl::World::GetMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	return IterateRenderPass(length, renderStream.size(), [] { RenderPipeline::TerminateStageTraverse();/*phaseSelector = &World::GetMainPassPost;*/ },
//		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::MainPassRange, this, _1, rangeBegin, rangeEnd); });
//}

//auto Impl::World::GetMainPassPost(unsigned int &) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	RenderPipeline::TerminateStageTraverse();
//	return bind(&World::MainPassPost, this, _1);
//}

auto Impl::World::GetAABBPassPre(unsigned int & length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetHiddenPassRange);
	return RenderPipeline::RenderStageItem{ bind(&World::AABBPassPre, this, _1) };
}

auto Impl::World::GetHiddenPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::StageRTBinding> RTBinding{ *RTBBindingDebug, true, false };
	return IterateRenderPass(length, queryStream.size(), &RTBinding, { *ZBindingDebug, true, false }, *outputDebug,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetVisiblePassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::AABBPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), false); });
}

auto Impl::World::GetVisiblePassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPasses::PassROPBinding<RenderPasses::StageRTBinding> RTBinding{ *RTBBindingDebug, false, true };
	return IterateRenderPass(length, queryStream.size(), &RTBinding, { *ZBindingDebug, false, true }, *outputDebug,
		[] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetAABBPassPost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass) { return bind(&World::AABBPassRange, this, _1, rangeBegin, rangeEnd, move(renderPass), true); });
}

auto Impl::World::GetAABBPassPost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPipeline::TerminateStageTraverse();
	return RenderPipeline::RenderStageItem{ bind(&World::AABBPassPost, this, _1) };
}

// 1 call site
inline void Impl::World::Setup(WorldViewContext &viewCtx, UINT64 tonemapParamsGPUAddress) const
{
	this->viewCtx = &viewCtx;
	this->tonemapParamsGPUAddress = tonemapParamsGPUAddress;
	SetupCullPass();
	SetupMainPass();
}

inline void Impl::World::SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion) const
{
	extern bool enableDebugDraw;
	if (enableDebugDraw != occlusionQueryBatch.index())
	{
		if (enableDebugDraw)
			occlusionQueryBatch.emplace<true>(L"world 3D objects", 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);
		else
			occlusionQueryBatch.emplace<false>();
	}
	visit([maxOcclusion](OcclusionCulling::QueryBatchBase &queryBatch) { queryBatch.Setup(maxOcclusion + 1); }, occlusionQueryBatch);
}

Impl::World::World(const float(&terrainXform)[4][3], float zenith, float azimuth) : sunDir{ zenith, azimuth }
{
	CheckSunZenithArg(zenith);
	memcpy(this->terrainXform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

template<unsigned int rows, unsigned int columns>
static inline void CopyMatrix2CB(const float (&src)[rows][columns], volatile Impl::CBRegister::AlignedRow<columns> (&dst)[rows]) noexcept
{
	copy_n(src, rows, dst);
}

void Impl::World::Render(WorldViewContext &viewCtx, const float (&viewXform)[4][3], const float (&projXform)[4][4], UINT64 tonemapParamsGPUAddress, const RenderPasses::PipelineOutputTargets &outputTargets) const
{
	using namespace placeholders;

	FlushUpdates();

	const float4x3 terrainTransform(terrainXform), viewTransform(viewXform),
		worldViewTransform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), viewTransform);
	const float4x4 frustumTransform = mul(float4x4(worldViewTransform[0], 0.f, worldViewTransform[1], 0.f, worldViewTransform[2], 0.f, worldViewTransform[3], 1.f), float4x4(projXform));

	// update per-frame CB
	{
#if !PERSISTENT_MAPS
		const auto curFrameCB_offset = GlobalGPUBufferData::PerFrameData::CurFrameCB_offset();
		CD3DX12_RANGE range(curFrameCB_offset, curFrameCB_offset);
		volatile GlobalGPUBufferData *const globalGPUBuffer_CPU_ptr = MapGlobalGPUBuffer(&range);
#endif
		auto &curFrameCB_region = globalGPUBuffer_CPU_ptr->perFrameDataCB[globalFrameVersioning->GetContinuousRingIdx()];
		CopyMatrix2CB(projXform, curFrameCB_region.projXform);
		CopyMatrix2CB(viewXform, curFrameCB_region.viewXform);
		CopyMatrix2CB(terrainXform, curFrameCB_region.terrainXform);
		const float3 sunDir = Sun::Dir(this->sunDir.zenith, this->sunDir.azimuth);
		curFrameCB_region.sun.dir = reinterpret_cast<const float (&)[3]>(mul(sunDir, viewTransform));
		curFrameCB_region.sun.irradiance = reinterpret_cast<const float (&)[3]>(Sun::Irradiance(this->sunDir.zenith, sunDir.z));
#if !PERSISTENT_MAPS
		range.End += sizeof(GlobalGPUBufferData::PerFrameData);
		globalGPUBuffer->Unmap(0, &range);
#endif
	}

	ScheduleRenderStage(viewCtx, frustumTransform, worldViewTransform, tonemapParamsGPUAddress, outputTargets);

	for_each(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), bind(&decltype(terrainVectorLayers)::value_type::ScheduleRenderStage,
		_1, FrustumCuller<2>(frustumTransform), cref(frustumTransform), tonemapParamsGPUAddress, cref(outputTargets)));

	extern bool enableDebugDraw;
	if (enableDebugDraw)
	{
		for_each(terrainVectorLayers.crbegin(), terrainVectorLayers.crend(), bind(&decltype(terrainVectorLayers)::value_type::ScheduleDebugDrawRenderStage, _1, cref(outputTargets)));
		ScheduleDebugDrawRenderStage(outputTargets);
	}
}

// "world.hh" currently does not #include "terrain.hh" (TerrainVectorLayer forward declared) => out-of-line
void Impl::World::OnFrameFinish() const
{
	TerrainVectorLayer::OnFrameFinish();
}

inline void Impl::World::SetSunDir(float zenith, float azimuth)
{
	CheckSunZenithArg(zenith);
	sunDir.zenith = zenith;
	sunDir.azimuth = azimuth;
}

shared_ptr<Renderer::Viewport> Impl::World::CreateViewport() const
{
	return make_shared<Renderer::Viewport>(shared_from_this());
}

shared_ptr<Renderer::TerrainVectorLayer> Impl::World::AddTerrainVectorLayer(shared_ptr<TerrainMaterials::Interface> layerMaterial, unsigned int layerIdx, string layerName)
{
	// keep layers list sorted by idx
	class Idx
	{
		unsigned int idx;

	public:
		Idx(unsigned int idx) noexcept : idx(idx) {}
		Idx(const ::TerrainVectorLayer &layer) noexcept : idx(layer.layerIdx) {}

	public:
		operator unsigned int () const noexcept { return idx; }
	};
	const auto insertLocation = lower_bound(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), layerIdx, greater<Idx>());
	const auto inserted = terrainVectorLayers.emplace(insertLocation, shared_from_this(), move(layerMaterial), layerIdx, move(layerName));
	// consider using custom allocator for shared_ptr's internal data in order to improve memory management
	return { &*inserted, [inserted](::TerrainVectorLayer *layerToRemove) { layerToRemove->world->terrainVectorLayers.erase(inserted); } };
}

auto Impl::World::AddStaticObject(Renderer::Object3D object, const float (&xform)[4][3], const AABB<3> &worldAABB) -> InstancePtr
{
	if (!object)
		throw logic_error("Attempt to add empty static object");
	InvalidateStaticObjects();
	const auto &inserted = staticObjects.emplace_back(shared_from_this(), move(object), xform, worldAABB);
	return { &inserted, InstanceDeleter{ prev(staticObjects.cend()) } };
}

void Impl::World::FlushUpdates() const
{
	if (!staticObjects.empty())
	{
		// rebuild BVH
		if (!bvh)
		{
			bvh = { staticObjects.cbegin(), staticObjects.cend(), Hierarchy::SplitTechnique::MEAN };
			bvhView = { bvh };
		}

		// recreate static objects CB
		if (!staticObjectsCB)
		{
			// create
			CheckHR(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeof(StaticObjectData) * staticObjects.size()/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				NULL,	// clear value
				IID_PPV_ARGS(staticObjectsCB.GetAddressOf())));
			NameObjectF(staticObjectsCB.Get(), L"static objects CB for world %p (%zu instances)", static_cast<const ::World *>(this), staticObjects.size());

			// fill
			volatile StaticObjectData *mapped;
			CheckHR(staticObjectsCB->Map(0, &CD3DX12_RANGE(0, 0), const_cast<void **>(reinterpret_cast<volatile void **>(&mapped))));
			// TODO: use C++20 initializer in range-based for
			auto CB_GPU_ptr = staticObjectsCB->GetGPUVirtualAddress();
			for (auto &instance : staticObjects)
			{
				CopyMatrix2CB(instance.GetWorldXform(), mapped++->worldXform);
				instance.CB_GPU_ptr = CB_GPU_ptr;
				CB_GPU_ptr += sizeof(StaticObjectData);
			}
			staticObjectsCB->Unmap(0, NULL);
		}
	}
}

auto Impl::World::BuildRenderStage(WorldViewContext &viewCtx, const float4x4 &frustumXform, const float4x3 &viewXform, UINT64 tonemapParamsGPUAddress) const -> RenderPipeline::RenderStage
{
	Setup(viewCtx, tonemapParamsGPUAddress);

	auto occlusionProvider = OcclusionCulling::QueryBatchBase::npos;
	unsigned long int AABBCount = 0;

	if (bvh)
	{
		// schedule
		bvhView.Schedule<false>(*GPU_AABB_allocator, FrustumCuller<3>(frustumXform), frustumXform, &viewXform);

		// issue
		{
			using namespace placeholders;

			bvhView.Issue(bind(&World::IssueOcclusion, this, _1, ref(AABBCount)), bind(&World::IssueNodeObjects, this, _1, _2, _3, _4), occlusionProvider);
		}
	}

	SetupOcclusionQueryBatch(occlusionProvider);
	xformedAABBs = xformedAABBsStorage.Allocate(AABBCount * xformedAABBSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	return { this, static_cast<decltype(phaseSelector)>(&World::GetStagePre) };
}

auto Impl::World::GetDebugDrawRenderStage() const -> RenderPipeline::PipelineStage
{
	return RenderPipeline::PipelineStage(in_place_type<RenderPipeline::RenderStage>, static_cast<const RenderPipeline::IRenderStage *>(this), static_cast<decltype(phaseSelector)>(&World::GetAABBPassPre));
}

void Impl::World::ScheduleRenderStage(WorldViewContext &viewCtx, const float4x4 &frustumTransform, const float4x3 &worldViewTransform, UINT64 tonemapParamsGPUAddress, const RenderPasses::PipelineOutputTargets &outputTargets) const
{
	RTBindinMain.emplace(outputTargets);
	bool ZHistoryValid = false;
	if (viewCtx.ZBufferHistory)
	{
		const auto targetZDesc = outputTargets.GetZBuffer()->GetDesc(), historyZDesc = viewCtx.ZBufferHistory->GetDesc();
		ZHistoryValid = targetZDesc.Width == historyZDesc.Width && targetZDesc.Height == historyZDesc.Height;
	}
	if (ZHistoryValid)
		ZBindingPrecull.emplace(outputTargets, D3D12_CLEAR_FLAG_STENCIL, 1.f, UINT8_MAX, false, false);
	else
		ZBindingPrecull.emplace(outputTargets, true, true);
	ZBindingMain.emplace(outputTargets, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, UINT8_MAX, true/*preserve for copy to Z history*/, false);
	outputMain.emplace(outputTargets);
	GPUWorkSubmission::AppendPipelineStage<true>(&World::BuildRenderStage, this, ref(viewCtx), frustumTransform, worldViewTransform, tonemapParamsGPUAddress);
}

void Impl::World::ScheduleDebugDrawRenderStage(const RenderPasses::PipelineOutputTargets &outputTargets) const
{
	RTBBindingDebug.emplace(outputTargets);
	ZBindingDebug.emplace(outputTargets, true, false);
	outputDebug.emplace(outputTargets);
	GPUWorkSubmission::AppendPipelineStage<false>(&World::GetDebugDrawRenderStage, this);
}

/*
	VS 2017/2019 STL uses allocator's construct() to construct combined object (main object + shared ptr data)
	but constructs containing object directly via placement new which does not have access to private members.
	GCC meanwhile compiles it fine.
*/
#if defined _MSC_VER && _MSC_VER <= 1922
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3], float zenith, float azimuth)
{
	return make_shared<World>(World::tag(), terrainXform, zenith, azimuth);
}
#else
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3], float zenith, float azimuth)
{
	return allocate_shared<World>(Misc::AllocatorProxy<World>(), terrainXform, zenith, azimuth);
}
#endif