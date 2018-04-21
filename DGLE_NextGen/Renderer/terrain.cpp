/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "terrain.hh"
#include "world.hh"	// for globalGPUBuffer
#include "tracked resource.inl"
#include "world hierarchy.inl"
#include "GPU stream buffer allocator.inl"
#include "cmdlist pool.inl"
#include "per-frame data.h"
#include "GPU work submission.h"
#include "occlusion query visualization.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

#include "AABB_2d.csh"
#include "vectorLayerVS.csh"
#include "vectorLayerPS.csh"

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

extern ComPtr<ID3D12Device2> device;
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
#if defined _MSC_VER && _MSC_VER <= 1913
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

#pragma region TerrainVectorQuad
TerrainVectorQuad::TerrainVectorQuad(shared_ptr<TerrainVectorLayer> &&layer, unsigned long int vcount, const function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool srcIB32bit, const function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData) :
	layer(move(layer)), subtree(ObjIterator<Object>(getObjectData, 0), ObjIterator<Object>(getObjectData, objCount), Impl::Hierarchy::SplitTechnique::MEAN, .5), subtreeView(subtree),
	IB32bit(vcount > UINT16_MAX), VB_size(vcount * sizeof(float [2])), IB_size(subtree.GetTriCount() * 3 * (IB32bit ? sizeof(uint32_t) : sizeof(uint16_t)))
{
	// create and fill VIB
	{
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(VB_size + IB_size),
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
inline void TerrainVectorQuad::Schedule(GPUStreamBuffer::Allocator<sizeof AABB<2>, AABB_VB_name> &GPU_AABB_allocator, const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const
{
	subtreeView.Schedule<true>(GPU_AABB_allocator, frustumCuller, frustumXform);
}

// 1 call site
inline void TerrainVectorQuad::Issue(remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const
{
	using namespace placeholders;

	subtreeView.Issue(bind(&TerrainVectorLayer::IssueOcclusion, layer.get(), _1), bind(&TerrainVectorLayer::IssueNodeObjects, layer.get(), _1, _2, _3, _4), occlusionProvider);
	layer->IssueQuad(VIB.Get(), VB_size, IB_size, IB32bit);
}
#pragma endregion

#pragma region TerrainVectorLayer
#pragma region occlusion query pass
ComPtr<ID3D12RootSignature> Impl::TerrainVectorLayer::CreateCullPassRootSig()
{
	CD3DX12_ROOT_PARAMETER1 CBV_param;
	CBV_param.InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(1, &CBV_param, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain occlusion query root signature");
}

ComPtr<ID3D12PipelineState> Impl::TerrainVectorLayer::CreateCullPassPSO()
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		cullPassRootSig.Get(),											// root signature
		CD3DX12_SHADER_BYTECODE(AABB_2D, sizeof AABB_2D),				// VS
		{},																// PS
		{},																// DS
		{},																// HS
		{},																// GS
		{},																// SO
		blendDesc,														// blend
		UINT_MAX,														// sample mask
		rasterDesc,														// rasterizer
		dsDesc,															// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
		0,																// render targets
		{},																// RT formats
		DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
		{1}																// MSAA
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain occlusion query PSO");
	return move(result);
}

void Impl::TerrainVectorLayer::CullPassPre(ID3D12GraphicsCommandList2 *cmdList) const
{
	PIXBeginEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::TerrainOcclusionQueryPass), "occlusion query pass");
}

/*
	Hardware occlusion query technique used here.
	It has advantage over shader based batched occlusoin test in that it does not stresses UAV writes in PS for visible objects -
		hardware occlusion query does not incur any overhead in addition to regular depth/stencil test.
*/
void Impl::TerrainVectorLayer::CullPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(cullPassPSO.Get());

	cullPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(cullPassRootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, World::globalGPUBuffer->GetGPUVirtualAddress() + World::PerFrameData::CurFrameCB_offset());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const OcclusionCulling::QueryBatchBase &queryBatch = occlusionQueryBatch.index() ? static_cast<OcclusionCulling::QueryBatchBase &>(get<true>(occlusionQueryBatch)) : get<false>(occlusionQueryBatch);
	ID3D12Resource *curVB = NULL;
	do
	{
		const auto &queryData = queryStream[rangeBegin];

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

void Impl::TerrainVectorLayer::CullPassPost(ID3D12GraphicsCommandList2 *cmdList) const
{
	if (const auto preservingQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::PERSISTENT>>(&occlusionQueryBatch))
		preservingQueryBatch->Resolve(cmdList/*, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
	else
		get<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(occlusionQueryBatch).Resolve(cmdList);
	PIXEndEvent(cmdList);	// occlusion query pass
}

// 1 call site
inline void Impl::TerrainVectorLayer::SetupCullPass(function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const
{
	cullPassSetupCallback = move(setupCallback);
	queryStream.clear();
}

// 1 call site
inline void Impl::TerrainVectorLayer::IssueOcclusion(ViewNode::OcclusionQueryGeometry occlusionQueryGeometry)
{
	queryStream.push_back({ occlusionQueryGeometry.VB, occlusionQueryGeometry.startIdx, occlusionQueryGeometry.count });
}
#pragma endregion

#pragma region main pass
ComPtr<ID3D12RootSignature> Impl::TerrainVectorLayer::CreateMainPassRootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[2];
	rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[1].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain main pass root signature");
}

ComPtr<ID3D12PipelineState> Impl::TerrainVectorLayer::CreateMainPassPSO()
{
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
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		mainPassRootSig.Get(),											// root signature
		CD3DX12_SHADER_BYTECODE(vectorLayerVS, sizeof vectorLayerVS),	// VS
		CD3DX12_SHADER_BYTECODE(vectorLayerPS, sizeof vectorLayerPS),	// PS
		{},																// DS
		{},																// HS
		{},																// GS
		{},																// SO
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),								// blend
		UINT_MAX,														// sample mask
		rasterDesc,														// rasterizer
		dsDesc,															// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
		1,																// render targets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },									// RT formats
		DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
		{1}																// MSAA
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain main pass PSO");
	return move(result);
}

void Impl::TerrainVectorLayer::MainPassPre(ID3D12GraphicsCommandList2 *cmdList) const
{
	const auto float2BYTE = [](float val) noexcept {return val * numeric_limits<BYTE>::max(); };
	PIXBeginEvent(cmdList, PIX_COLOR(float2BYTE(color[0]), float2BYTE(color[1]), float2BYTE(color[2])), "main pass");
}

void Impl::TerrainVectorLayer::MainPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(mainPassPSO.Get());

	mainPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(mainPassRootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, World::globalGPUBuffer->GetGPUVirtualAddress() + World::PerFrameData::CurFrameCB_offset());
	cmdList->SetGraphicsRoot32BitConstants(1, size(color), color, 0);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto curOcclusionQueryIdx = OcclusionCulling::QueryBatchBase::npos;
	do
	{
		const auto &quad = quadStram[renderStream[rangeBegin].startQuadIdx];

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
				visit([&](const auto &queryBatch) { queryBatch.Set(cmdList, curOcclusionQueryIdx = renderData.occlusion); }, occlusionQueryBatch);
			cmdList->DrawIndexedInstanced(renderData.triCount * 3, 1, renderData.startIdx, 0, 0);
		} while (++rangeBegin < quadRangeEnd);
	} while (rangeBegin < rangeEnd);
}

void Impl::TerrainVectorLayer::MainPassPost(ID3D12GraphicsCommandList2 *cmdList) const
{
	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
		transientQueryBatch->Finish(cmdList);
	PIXEndEvent(cmdList);	// main pass
}

// 1 call site
inline void Impl::TerrainVectorLayer::SetupMainPass(function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const
{
	mainPassSetupCallback = move(setupCallback);
	renderStream.clear();
	quadStram.clear();
}

void Impl::TerrainVectorLayer::IssueCluster(unsigned long int startIdx, unsigned long int triCount, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	assert(triCount);
	renderStream.push_back({ startIdx, triCount, occlusion, quadStram.size() });
}

// 1 call site
inline void Impl::TerrainVectorLayer::IssueExclusiveObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	if (node.GetExclusiveTriCount())
		IssueCluster(node.startIdx, node.GetExclusiveTriCount(), occlusion);
}

// 1 call site
inline void Impl::TerrainVectorLayer::IssueChildren(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	IssueCluster(node.startIdx + node.GetExclusiveTriCount() * 3, node.GetInclusiveTriCount() - node.GetExclusiveTriCount(), occlusion);
}

// 1 call site
inline void Impl::TerrainVectorLayer::IssueWholeNode(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion)
{
	IssueCluster(node.startIdx, node.GetInclusiveTriCount(), occlusion);
}

bool Impl::TerrainVectorLayer::IssueNodeObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) coarseOcclusion,  decltype(OcclusionCulling::QueryBatchBase::npos) fineOcclusion, ViewNode::Visibility visibility)
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
inline void Impl::TerrainVectorLayer::IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit)
{
	quadStram.push_back({ renderStream.size(), VIB, VB_size, IB_size, IB32bit });
}
#pragma endregion

#pragma region visualize occlusion pass
ComPtr<ID3D12PipelineState> Impl::TerrainVectorLayer::CreateAABB_PSO()
{
	const D3D12_BLEND_DESC blendDesc
	{
		FALSE,								// alpha2covarage
		FALSE,								// independent blend
		{
			TRUE,							// blend enable
			FALSE,							// logic op enable
			D3D12_BLEND_SRC_ALPHA,			// src blend
			D3D12_BLEND_INV_SRC_ALPHA,		// dst blend
			D3D12_BLEND_OP_ADD,				// blend op
			D3D12_BLEND_ONE,				// src blend alpha
			D3D12_BLEND_ZERO,				// dst blend alpha
			D3D12_BLEND_OP_ADD,				// blend op alpha
			D3D12_LOGIC_OP_NOOP,			// logic op
			D3D12_COLOR_WRITE_ENABLE_ALL	// write mask
		}
	};

	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_WIREFRAME,
		D3D12_CULL_MODE_NONE,
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		mainPassRootSig.Get(),											// root signature
		CD3DX12_SHADER_BYTECODE(AABB_2D, sizeof AABB_2D),				// VS
		CD3DX12_SHADER_BYTECODE(vectorLayerPS, sizeof vectorLayerPS),	// PS
		{},																// DS
		{},																// HS
		{},																// GS
		{},																// SO
		blendDesc,														// blend
		UINT_MAX,														// sample mask
		rasterDesc,														// rasterizer
		dsDesc,															// depth stencil
		{ VB_decl, size(VB_decl) },										// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
		1,																// render targets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },									// RT formats
		DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
		{1}																// MSAA
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain AABB visualization PSO");
	return move(result);
}

void Impl::TerrainVectorLayer::AABBPassRange(CmdListPool::CmdList &cmdList, unsigned long rangeBegin, unsigned long rangeEnd, const float (&color)[3], bool visible) const
{
	assert(rangeBegin < rangeEnd);

	cmdList.Setup(AABB_PSO.Get());

	mainPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(mainPassRootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(0, World::globalGPUBuffer->GetGPUVirtualAddress() + World::PerFrameData::CurFrameCB_offset());
	cmdList->SetGraphicsRoot32BitConstants(1, size(color), color, 0);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const auto &queryBatch = get<true>(occlusionQueryBatch);
	ID3D12Resource *curVB = NULL;
	do
	{
		const auto &queryData = queryStream[rangeBegin];

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

void Impl::TerrainVectorLayer::StagePre(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	PIXBeginEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::TerrainLayer), "terrain layer [%u] \"%s\"", layerIdx, layerName.c_str());
	CullPassPre(cmdList);
}

void Impl::TerrainVectorLayer::CullPass2MainPass(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	CullPassPost(cmdList);
	MainPassPre(cmdList);
}

void Impl::TerrainVectorLayer::StagePost(CmdListPool::CmdList &cmdList) const
{
	cmdList.Setup();

	MainPassPost(cmdList);
	PIXEndEvent(cmdList);	// stage
}
#pragma endregion

void Impl::TerrainVectorLayer::Sync() const
{
	if (const auto transientQueryBatch = get_if<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>>(&occlusionQueryBatch))
		transientQueryBatch->Sync();
}

auto Impl::TerrainVectorLayer::GetStagePre(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetCullPassRange);
	return bind(&TerrainVectorLayer::StagePre, this, _1);
}

auto  Impl::TerrainVectorLayer::GetCullPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetCullPass2MainPass); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&TerrainVectorLayer::CullPassRange, this, _1, rangeBegin, rangeEnd); });
}

auto Impl::TerrainVectorLayer::GetCullPass2MainPass(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	phaseSelector = static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetMainPassRange);
	return bind(&TerrainVectorLayer::CullPass2MainPass, this, _1);
}

auto Impl::TerrainVectorLayer::GetMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, renderStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetStagePost); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&TerrainVectorLayer::MainPassRange, this, _1, rangeBegin, rangeEnd); });
}

auto Impl::TerrainVectorLayer::GetStagePost(unsigned int &) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	RenderPipeline::TerminateStageTraverse();
	return bind(&TerrainVectorLayer::StagePost, this, _1);
}

auto Impl::TerrainVectorLayer::GetVisiblePassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { phaseSelector = static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetCulledPassRange); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&TerrainVectorLayer::AABBPassRange, this, _1, rangeBegin, rangeEnd, cref(OcclusionCulling::DebugColors::Terrain::visible), true); });
}

auto Impl::TerrainVectorLayer::GetCulledPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, queryStream.size(), [] { RenderPipeline::TerminateStageTraverse(); },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&TerrainVectorLayer::AABBPassRange, this, _1, rangeBegin, rangeEnd, cref(OcclusionCulling::DebugColors::Terrain::culled), false); });
}

// 1 call site
inline void Impl::TerrainVectorLayer::Setup(function<void (ID3D12GraphicsCommandList2 *target)> &&cullPassSetupCallback, function<void (ID3D12GraphicsCommandList2 *target)> &&mainPassSetupCallback) const
{
	SetupCullPass(move(cullPassSetupCallback));
	SetupMainPass(move(mainPassSetupCallback));
}

inline void Impl::TerrainVectorLayer::SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion) const
{
	extern bool enableDebugDraw;
	if (enableDebugDraw != occlusionQueryBatch.index())
	{
		if (enableDebugDraw)
			occlusionQueryBatch.emplace<true>(layerName);
		else
			occlusionQueryBatch.emplace<false>();
	}
	visit([maxOcclusion](auto &queryBatch) { queryBatch.Setup(maxOcclusion + 1); }, occlusionQueryBatch);
}

void TerrainVectorLayer::QuadDeleter::operator()(const TerrainVectorQuad *quadToRemove) const
{
	quadToRemove->layer->quads.erase(quadLocation);
}

Impl::TerrainVectorLayer::TerrainVectorLayer(shared_ptr<class Renderer::World> &&world, unsigned int layerIdx, const float (&color)[3], string &&layerName) :
	world(move(world)), layerIdx(layerIdx), color{ color[0], color[1], color[2] }, layerName(move(layerName))
{
}

Impl::TerrainVectorLayer::~TerrainVectorLayer() = default;

auto Impl::TerrainVectorLayer::AddQuad(unsigned long int vcount, const function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData) -> QuadPtr
{
	quads.emplace_back(shared_from_this(), vcount, fillVB, objCount, IB32bit, getObjectData);
	return { &quads.back(), QuadDeleter{ prev(quads.cend()) } };
}

auto Impl::TerrainVectorLayer::BuildRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList2 *target)> &cullPassSetupCallback, function<void (ID3D12GraphicsCommandList2 *target)> &mainPassSetupCallback) const -> RenderPipeline::RenderStage
{
	using namespace placeholders;

	PIXScopedEvent(PIX_COLOR_INDEX(PIXEvents::TerrainBuildRenderStage), "terrain layer [%u] build render stage", layerIdx);
	Setup(move(cullPassSetupCallback), move(mainPassSetupCallback));

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
		for_each(execution::par, quads.begin(), quads.end(), bind(&TerrainVectorQuad::Schedule, _1, ref(*GPU_AABB_allocator), cref(frustumCuller), cref(frustumXform)));
#else
#error invalid MULTITHREADED_QUADS_SHCEDULE value
#endif
	}

	auto occlusionProvider = OcclusionCulling::QueryBatchBase::npos;

	// issue
	{
		PIXScopedEvent(PIX_COLOR_INDEX(PIXEvents::TerrainIssue), "issue");
		for_each(quads.begin(), quads.end(), bind(&TerrainVectorQuad::Issue, _1, ref(occlusionProvider)));
	}

	SetupOcclusionQueryBatch(occlusionProvider);

	return { this, static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetStagePre) };
}

auto Impl::TerrainVectorLayer::GetDebugDrawRenderStage() const -> RenderPipeline::PipelineStage
{
	return RenderPipeline::PipelineStage(in_place_type<RenderPipeline::RenderStage>, static_cast<const RenderPipeline::IRenderStage *>(this), static_cast<decltype(phaseSelector)>(&TerrainVectorLayer::GetVisiblePassRange));
}

void Impl::TerrainVectorLayer::ScheduleRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, function<void (ID3D12GraphicsCommandList2 *target)> cullPassSetupCallback, function<void (ID3D12GraphicsCommandList2 *target)> mainPassSetupCallback) const
{
	GPUWorkSubmission::AppendPipelineStage<true>(&TerrainVectorLayer::BuildRenderStage, this, /*cref*/(frustumCuller), /*cref*/(frustumXform), move(cullPassSetupCallback), move(mainPassSetupCallback));
}

void Impl::TerrainVectorLayer::ScheduleDebugDrawRenderStage() const
{
	GPUWorkSubmission::AppendPipelineStage<false>(&TerrainVectorLayer::GetDebugDrawRenderStage, this);
}
#pragma endregion