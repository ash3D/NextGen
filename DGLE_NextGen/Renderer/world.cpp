/**
\author		Alexey Shaydurov aka ASH
\date		22.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "world.hh"
#include "viewport.hh"
#include "terrain.hh"
#include "instance.hh"
#include "tracked resource.inl"
#include "world hierarchy.inl"
#include "GPU stream buffer allocator.inl"
#include "cmdlist pool.inl"
#include "world view context.h"
#include "frustum culling.h"
#include "occlusion query shceduling.h"
#include "occlusion query visualization.h"
#include "frame versioning.h"
#include "global GPU buffer.h"
#include "per-frame data.h"
#include "static objects data.h"
#include "GPU work submission.h"

#include "AABB_3D_xform.csh"
#include "AABB_3D.csh"
#include "AABB_3D_vis.csh"

using namespace std;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

// LH, CW
static constexpr array<uint16_t, 14> boxIB = { 2, 3, 0, 1, 5, 3, 7, 2, 6, 0, 4, 5, 6, 7 };

ComPtr<ID3D12Resource> Impl::World::CreateGlobalGPUBuffer()
{
	ComPtr<ID3D12Resource> buffer;
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(GlobalGPUBuffer::BoxIB_offset() + sizeof boxIB/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(buffer.GetAddressOf())));
	NameObject(buffer.Get(), L"per-frame CB + box IB");
	
	// fill AABB vis colors and box IB
	{
		CD3DX12_RANGE range(PerFrameData::CB_footprint(), PerFrameData::CB_footprint());
		PerFrameData *CPU_ptr;
		CheckHR(buffer->Map(0, &range, reinterpret_cast<void **>(&CPU_ptr)));
		CPU_ptr += maxFrameLatency;

		auto &visColorsCPU_ptr = reinterpret_cast<GlobalGPUBuffer::AABB_3D_VisColors *&>(CPU_ptr);
		visColorsCPU_ptr->culled = OcclusionCulling::DebugColors::Object3D::culled;
		visColorsCPU_ptr->rendered[0] = OcclusionCulling::DebugColors::Object3D::hidden[0];
		visColorsCPU_ptr->rendered[1] = OcclusionCulling::DebugColors::Object3D::hidden[1];
		visColorsCPU_ptr++;
		visColorsCPU_ptr->culled = OcclusionCulling::DebugColors::Object3D::culled;
		visColorsCPU_ptr->rendered[0] = OcclusionCulling::DebugColors::Object3D::visible[0];
		visColorsCPU_ptr->rendered[1] = OcclusionCulling::DebugColors::Object3D::visible[1];

		*reinterpret_cast<remove_const_t<decltype(boxIB)> *>(++visColorsCPU_ptr) = boxIB;
		
		range.End += GlobalGPUBuffer::AABB_3D_VisColors::CB_footprint() + sizeof boxIB;
		buffer->Unmap(0, &range);
	}

	return move(buffer);
}

auto Impl::World::MapPerFrameCB(const D3D12_RANGE *readRange) -> volatile PerFrameData *
{
	volatile PerFrameData *CPU_ptr;
	CheckHR(globalGPUBuffer->Map(0, readRange, const_cast<void **>(reinterpret_cast<volatile void **>(&CPU_ptr))));
	return CPU_ptr;
}

// defined here, not in class in order to eliminate dependency on "instance.hh" in "world.hh"
#if defined _MSC_VER && _MSC_VER <= 1913
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
	instanceToRemove->GetWorld()->staticObjects.erase(instsnceLocation);
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
	CheckHR(D3D12CreateVersionedRootSignatureDeserializer(AABB_3D_xform, sizeof AABB_3D_xform, IID_PPV_ARGS(rootSigProvider.GetAddressOf())));
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		xformAABB_RootSig.Get(),										// root signature
		CD3DX12_SHADER_BYTECODE(AABB_3D_xform, sizeof AABB_3D_xform),	// VS
		{},																// PS
		{},																// DS
		{},																// HS
		{},																// GS
		soDesc,															// SO
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),								// blend
		UINT_MAX,														// sample mask
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),							// rasterizer
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),						// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,							// primitive topology
		0,																// render targets
		{},																// RT formats
		DXGI_FORMAT_UNKNOWN,											// depth stencil format
		{1}																// MSAA
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
		cullPassRootSig.Get(),											// root signature
		CD3DX12_SHADER_BYTECODE(AABB_3D, sizeof AABB_3D),				// VS
		{},																// PS
		{},																// DS
		{},																// HS
		{},																// GS
		{},																// SO
		blendDesc,														// blend
		UINT_MAX,														// sample mask
#if 0
		CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),							// rasterizer
#else
		CD3DX12_RASTERIZER_DESC
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
		),																// rasterizer
#endif
		dsDescs[0],														// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
		0,																// render targets
		{},																// RT formats
		DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
		{1}																// MSAA
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

	cmdList->SetGraphicsRootSignature(xformAABB_RootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, globalGPUBuffer->GetGPUVirtualAddress() + PerFrameData::CurFrameCB_offset());
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

void Impl::World::CullPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, bool final) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(cullPassPSOs[final].Get());

	cullPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(cullPassRootSig.Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// setup IB/VB
	{
		const D3D12_INDEX_BUFFER_VIEW IB_view =
		{
			globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBuffer::BoxIB_offset(),
			sizeof boxIB,
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

	const OcclusionCulling::QueryBatchBase &queryBatch = visit([](const auto &queryBatch) noexcept -> const OcclusionCulling::QueryBatchBase & { return queryBatch; }, occlusionQueryBatch);
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
inline void Impl::World::SetupCullPass(function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const
{
	cullPassSetupCallback = move(setupCallback);
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

void Impl::World::MainPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, bool final) const
{
	assert(rangeBegin < rangeEnd);

	const auto &renderStream = renderStreams[final];

	cmdList.Setup(renderStream[rangeBegin].instance->GetStartPSO().Get());

	mainPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(decltype(staticObjects)::value_type::GetRootSignature().Get());
	cmdList->SetGraphicsRootConstantBufferView(0, globalGPUBuffer->GetGPUVirtualAddress() + PerFrameData::CurFrameCB_offset());

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
inline void Impl::World::SetupMainPass(function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const
{
	mainPassSetupCallback = move(setupCallback);
	for (auto &stream : renderStreams) stream.clear();
}

// 1 call site
inline void Impl::World::IssueObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion) const
{
	const auto issue2stream = [range = node.GetExclusiveObjectsRange(), occlusion](remove_extent_t<decltype(renderStreams)> &renderStream)
	{
		transform(range.first, range.second, back_inserter(renderStream), [occlusion](const Renderer::Instance *instance) noexcept -> remove_reference_t<decltype(renderStream)>::value_type { return { instance, occlusion }; });
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
ComPtr<ID3D12RootSignature> Impl::World::CreateAABB_RootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[4];
	rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[1].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[3].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
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
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
		FALSE,										// MSAA
		TRUE,										// AA line
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
		AABB_RootSig.Get(),												// root signature
		CD3DX12_SHADER_BYTECODE(AABB_3D, sizeof AABB_3D),				// VS
		CD3DX12_SHADER_BYTECODE(AABB_3D_vis, sizeof AABB_3D_vis),		// PS
		{},																// DS
		{},																// HS
		{},																// GS
		{},																// SO
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),								// blend
		UINT_MAX,														// sample mask
		rasterDesc,														// rasterizer
		dsDescHidden,													// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
		1,																// render targets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },									// RT formats
		DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
		{1}																// MSAA
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

void Impl::World::AABBPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, bool visible) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(AABB_PSOs[visible].Get());

	const auto &queryBatch = get<true>(occlusionQueryBatch);

	mainPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(AABB_RootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, World::globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBuffer::AABB_3D_VisColors::CB_offset(visible));
	cmdList->SetGraphicsRootShaderResourceView(1, queryBatch.GetGPUPtr(false));
	cmdList->SetGraphicsRootShaderResourceView(2, queryBatch.GetGPUPtr(true));
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// setup IB/VB (consider sharing this with cull pass)
	{
		const D3D12_INDEX_BUFFER_VIEW IB_view =
		{
			globalGPUBuffer->GetGPUVirtualAddress() + GlobalGPUBuffer::BoxIB_offset(),
			sizeof boxIB,
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

	do
	{
		const auto &queryData = queryStream[rangeBegin];

		cmdList->SetGraphicsRoot32BitConstant(3, rangeBegin * sizeof(UINT64), 0);
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
	cmdList.Setup();

	// just copy for now, do reprojection in future
	if (viewCtx->ZBufferHistory)
	{
		if (const auto targetZDesc = ZBuffer->GetDesc(), historyZDesc = viewCtx->ZBufferHistory->GetDesc(); targetZDesc.Width == historyZDesc.Width && targetZDesc.Height == historyZDesc.Height)
		{
			{
				const D3D12_RESOURCE_BARRIER barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ZBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_DEST),
					CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
				};
				cmdList->ResourceBarrier(size(barriers), barriers);
			}
			cmdList->CopyResource(ZBuffer, viewCtx->ZBufferHistory.Get());
			{
				const D3D12_RESOURCE_BARRIER barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(ZBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_DEPTH_WRITE),
					CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
				};
				cmdList->ResourceBarrier(size(barriers), barriers);
				cmdList->ClearDepthStencilView({ dsv }, D3D12_CLEAR_FLAG_STENCIL, 1.f, UINT8_MAX, 0, NULL);
			}
		}
	}

	xformedAABBs.StartSO(cmdList);
}

void Impl::World::XformAABBPass2CullPass(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	xformedAABBs.UseSOResults(cmdList);
}

void Impl::World::CullPass2MainPass(CmdListPool::CmdList &cmdList, bool final) const
{
	cmdList.Setup();

	if (final)
	{
		// xformedAABBs used for debug draw -> finish it later if debug draw enabled
		extern bool enableDebugDraw;
		if (!enableDebugDraw)
			xformedAABBs.Finish(cmdList);
	}
	else
		cmdList->ClearDepthStencilView({ dsv }, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, UINT8_MAX, 0, NULL);

	visit([&cmdList, final](const auto &queryBatch) { queryBatch.Resolve(cmdList, final); }, occlusionQueryBatch);
}

//void Impl::World::MainPass2CullPass(CmdListPool::CmdList &cmdList) const
//{
//	cmdList.Setup();
//
//}

void Impl::World::StagePost(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
		transientQueryBatch->Finish(cmdList);

#if 1
	const D3D12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(ZBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
	};
	UINT barrierCount = size(barriers);

	const auto targetZDesc = ZBuffer->GetDesc();
	if (viewCtx->ZBufferHistory)
		if (const auto historyZDesc = viewCtx->ZBufferHistory->GetDesc(); targetZDesc.Width != historyZDesc.Width || targetZDesc.Height != historyZDesc.Height)
			viewCtx->ZBufferHistory.Reset();
	if (!viewCtx->ZBufferHistory)
	{
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&targetZDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.f, UINT8_MAX),
			IID_PPV_ARGS(viewCtx->ZBufferHistory.ReleaseAndGetAddressOf())
		));
		NameObjectF(viewCtx->ZBufferHistory.Get(), L"Z buffer history (world view context %p) [%lu]", viewCtx, viewCtx->ZBufferHistoryVersion++);
		barrierCount--;
	}

	cmdList->ResourceBarrier(barrierCount, barriers);
	cmdList->CopyResource(viewCtx->ZBufferHistory.Get(), ZBuffer);
	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(ZBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE),
			CD3DX12_RESOURCE_BARRIER::Transition(viewCtx->ZBufferHistory.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
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
	return bind(&World::StagePre, this, _1);
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
	return bind(&World::XformAABBPass2CullPass, this, _1);
}

auto Impl::World::GetFirstCullPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetFirstCullPass2FirstMainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::CullPassRange, this, _1, rangeBegin, rangeEnd, false); });
}

auto Impl::World::GetFirstCullPass2FirstMainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetFirstMainPassRange);
	return bind(&World::CullPass2MainPass, this, _1, false);
}

auto Impl::World::GetFirstMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, renderStreams[0].size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::/*GetFirstMainPass2SecondCullPass*/GetSecondCullPassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::MainPassRange, this, _1, rangeBegin, rangeEnd, false); });
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
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetSecondCullPass2SecondMainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::CullPassRange, this, _1, rangeBegin, rangeEnd, true); });
}

auto Impl::World::GetSecondCullPass2SecondMainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetSecondMainPassRange);
	return bind(&World::CullPass2MainPass, this, _1, true);
}

auto Impl::World::GetSecondMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, renderStreams[1].size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetStagePost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::MainPassRange, this, _1, rangeBegin, rangeEnd, true); });
}

auto Impl::World::GetStagePost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPipeline::TerminateStageTraverse();
	return bind(&World::StagePost, this, _1);
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

auto Impl::World::GetHiddenPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetVisiblePassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::AABBPassRange, this, _1, rangeBegin, rangeEnd, false); });
}

auto Impl::World::GetVisiblePassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&World::GetAABBPassPost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::AABBPassRange, this, _1, rangeBegin, rangeEnd, true); });
}

auto Renderer::Impl::World::GetAABBPassPost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPipeline::TerminateStageTraverse();
	return bind(&World::AABBPassPost, this, _1);
}

// 1 call site
inline void Impl::World::Setup(WorldViewContext &viewCtx, ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, function<void (ID3D12GraphicsCommandList2 *target)> &&cullPassSetupCallback, function<void (ID3D12GraphicsCommandList2 *target)> &&mainPassSetupCallback) const
{
	this->viewCtx = &viewCtx;
	this->ZBuffer = ZBuffer;
	this->dsv = dsv.ptr;
	SetupCullPass(move(cullPassSetupCallback));
	SetupMainPass(move(mainPassSetupCallback));
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
	visit([maxOcclusion](auto &queryBatch) { queryBatch.Setup(maxOcclusion + 1); }, occlusionQueryBatch);
}

Impl::World::World(const float (&terrainXform)[4][3])
{
	memcpy(this->terrainXform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

template<unsigned int rows, unsigned int columns>
static inline void CopyMatrix2CB(const float (&src)[rows][columns], volatile Impl::CBRegister::AlignedRow<columns> (&dst)[rows]) noexcept
{
	copy_n(src, rows, dst);
}

void Impl::World::Render(WorldViewContext &viewCtx, const float (&viewXform)[4][3], const float (&projXform)[4][4], ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const function<void (ID3D12GraphicsCommandList2 *target, bool enableRT)> &setupRenderOutputCallback) const
{
	using namespace placeholders;

	FlushUpdates();

	const float4x3 terrainTransform(terrainXform), viewTransform(viewXform),
		worldViewTransform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), viewTransform);
	const float4x4 frustumTransform = mul(float4x4(worldViewTransform[0], 0.f, worldViewTransform[1], 0.f, worldViewTransform[2], 0.f, worldViewTransform[3], 1.f), float4x4(projXform));

	// update per-frame CB
	{
#if !PERSISTENT_MAPS
		const auto curFrameCB_offset = PerFrameData::CurFrameCB_offset();
		CD3DX12_RANGE range(curFrameCB_offset, curFrameCB_offset);
		volatile PerFrameData *const perFrameCB_CPU_ptr = MapPerFrameCB(&range);
#endif
		auto &curFrameCB_region = perFrameCB_CPU_ptr[globalFrameVersioning->GetContinuousRingIdx()];
		CopyMatrix2CB(projXform, curFrameCB_region.projXform);
		CopyMatrix2CB(viewXform, curFrameCB_region.viewXform);
		CopyMatrix2CB(terrainXform, curFrameCB_region.terrainXform);
#if !PERSISTENT_MAPS
		range.End += sizeof(PerFrameData);
		globalGPUBuffer->Unmap(0, &range);
#endif
	}

	const function<void (ID3D12GraphicsCommandList2 *target)> cullPassSetupCallback = bind(setupRenderOutputCallback, _1, false), mainPassSetupCallback = bind(setupRenderOutputCallback, _1, true);
	
	GPUWorkSubmission::AppendPipelineStage<true>(&World::BuildRenderStage, this, ref(viewCtx), frustumTransform, worldViewTransform, ZBuffer, dsv, cullPassSetupCallback, mainPassSetupCallback);

	for_each(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), bind(&decltype(terrainVectorLayers)::value_type::ScheduleRenderStage,
		_1, FrustumCuller<2>(frustumTransform), cref(frustumTransform), cref(cullPassSetupCallback), cref(mainPassSetupCallback)));

	extern bool enableDebugDraw;
	if (enableDebugDraw)
	{
		for_each(terrainVectorLayers.crbegin(), terrainVectorLayers.crend(), mem_fn(&decltype(terrainVectorLayers)::value_type::ScheduleDebugDrawRenderStage));
		GPUWorkSubmission::AppendPipelineStage<false>(&World::GetDebugDrawRenderStage, this);
	}
}

// "world.hh" currently does not #include "terrain.hh" (TerrainVectorLayer forward declared) => out-of-line
void Impl::World::OnFrameFinish() const
{
	TerrainVectorLayer::OnFrameFinish();
}

shared_ptr<Renderer::Viewport> Impl::World::CreateViewport() const
{
	return make_shared<Renderer::Viewport>(shared_from_this());
}

shared_ptr<Renderer::TerrainVectorLayer> Impl::World::AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3], string layerName)
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
	const auto inserted = terrainVectorLayers.emplace(insertLocation, shared_from_this(), layerIdx, color, move(layerName));
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
		struct AdressIterator : enable_if_t<true, decltype(staticObjects)::const_iterator>
		{
			AdressIterator(decltype(staticObjects)::const_iterator src) : enable_if_t<true, decltype(staticObjects)::const_iterator>(src) {}	// replace with C++17 aggregate base class init
			auto operator *() const noexcept { return &decltype(staticObjects)::const_iterator::operator *(); }
		};

		// rebuild BVH
		if (!bvh)
		{
			bvh = { AdressIterator{staticObjects.cbegin()}, AdressIterator{staticObjects.cend()}, Hierarchy::SplitTechnique::MEAN };
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

auto Impl::World::BuildRenderStage(WorldViewContext &viewCtx, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 &viewXform, ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, function<void (ID3D12GraphicsCommandList2 *target)> &cullPassSetupCallback, function<void (ID3D12GraphicsCommandList2 *target)> &mainPassSetupCallback) const -> RenderPipeline::RenderStage
{
	Setup(viewCtx, ZBuffer, dsv, move(cullPassSetupCallback), move(mainPassSetupCallback));

	// schedule
	bvhView.Schedule<false>(*GPU_AABB_allocator, FrustumCuller<3>(frustumXform), frustumXform, &viewXform);

	auto occlusionProvider = OcclusionCulling::QueryBatchBase::npos;
	unsigned long int AABBCount = 0;

	// issue
	{
		using namespace placeholders;

		bvhView.Issue(bind(&World::IssueOcclusion, this, _1, ref(AABBCount)), bind(&World::IssueNodeObjects, this, _1, _2, _3, _4), occlusionProvider);
	}

	SetupOcclusionQueryBatch(occlusionProvider);
	xformedAABBs = xformedAABBsStorage.Allocate(AABBCount * xformedAABBSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	return { this, static_cast<decltype(phaseSelector)>(&World::GetStagePre) };
}

auto Impl::World::GetDebugDrawRenderStage() const -> RenderPipeline::PipelineStage
{
	return RenderPipeline::PipelineStage(in_place_type<RenderPipeline::RenderStage>, static_cast<const RenderPipeline::IRenderStage *>(this), static_cast<decltype(phaseSelector)>(&World::GetHiddenPassRange));
}

/*
	VS 2017 STL uses allocator's construct() to construct combined object (main object + shared ptr data)
	but constructs containing object directly via placement new which does not have access to private members.
	GCC meanwhile compiles it fine.
*/
#if defined _MSC_VER && _MSC_VER <= 1913
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return make_shared<World>(World::tag(), terrainXform);
}
#else
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return allocate_shared<World>(World::Allocator<World>(), terrainXform);
}
#endif