#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "render passes.h"
#include "shader bytecode.h"
#include "CB register.h"
#include "DMA engine.h"
#include "config.h"
#include "PIX events.h"
#include "CS config.h"

namespace Shaders
{
#	include "luminanceTextureReduction.csh"
#	include "luminanceBufferReduction.csh"
#	include "decode2halfres.csh"
#	include "brightPass.csh"
#	include "downsample.csh"
#	include "upsampleBlur.csh"
#	include "postprocessFinalComposite.csh"
}

// offsetof is conditionally supported for non-standard layout types since C++17
#define ENABLE_NONSTDLAYOUT_OFFSETOF 1

using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace RenderPipeline = Impl::RenderPipeline;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);
	
extern const float backgroundColor[4] = { .0f, .2f, .4f, 1.f };

static inline float CalculateLumAdaptationLerpFactor(float delta)
{
	static constexpr float adaptationSpeed = 1;

	return exp(-adaptationSpeed * delta);
}

// result valid until call to 'OnFrameFinish()'
auto Impl::Viewport::CmdListsManager::OnFrameStart() -> PrePostCmds<ID3D12GraphicsCommandList4 *>
{
	FrameVersioning::OnFrameStart();
	auto &cmdBuffers = GetCurFrameDataVersion();

	if (cmdBuffers.pre.allocator && cmdBuffers.post.allocator && cmdBuffers.pre.list && cmdBuffers.post.list)
	{
		// reset allocators
		CheckHR(cmdBuffers.pre.allocator->Reset());
		CheckHR(cmdBuffers.post.allocator->Reset());

		// reset lists
		CheckHR(cmdBuffers.pre.list->Reset(cmdBuffers.pre.allocator.Get(), NULL));
		CheckHR(cmdBuffers.post.list->Reset(cmdBuffers.post.allocator.Get(), luminanceTextureReductionPSO.Get()));
	}
	else
	{
#if ENABLE_NONSTDLAYOUT_OFFSETOF
		const void *const ptr = reinterpret_cast<const std::byte *>(this) - offsetof(Viewport, cmdListsManager);
#else
		const void *const ptr = this;
#endif
		const unsigned short version = GetFrameLatency() - 1;

		// create allocators
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.pre.allocator)));
		NameObjectF(cmdBuffers.pre.allocator.Get(), L"viewport %p pre command allocator [%hu]", ptr, version);
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.post.allocator)));
		NameObjectF(cmdBuffers.post.allocator.Get(), L"viewport %p post command allocator [%hu]", ptr, version);

		// create lists
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.pre.allocator.Get(), NULL, IID_PPV_ARGS(&cmdBuffers.pre.list)));
		NameObjectF(cmdBuffers.pre.list.Get(), L"viewport %p pre command list [%hu]", ptr, version);
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.post.allocator.Get(), luminanceTextureReductionPSO.Get(), IID_PPV_ARGS(&cmdBuffers.post.list)));
		NameObjectF(cmdBuffers.post.list.Get(), L"viewport %p post command list [%hu]", ptr, version);
	}

	return { cmdBuffers.pre.list.Get(), cmdBuffers.post.list.Get() };
}

#pragma region postprocess root sig & PSOs
ComPtr<ID3D12RootSignature> Impl::Viewport::CreatePostprocessRootSig()
{
	// consider encapsulating desc table in PostprocessDescriptorTableStore
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	const D3D12_DESCRIPTOR_RANGE1 descTable[] =
	{
		/*
			descriptor volatile flags below are for hardware out-of-bounds access checking for reduction buffer SRV/UAV
			?: there is D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS flag which seemed as exactly what is needed
				but it fails with message "Unsupported bit-flag set (descriptor range flags 10000)."
				no mention for it in docs other than just presence in https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ne-d3d12-d3d12_descriptor_range_flags without any description
		*/
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 11, 0, 1)
	};
	rootParams[ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(descTable), descTable);
	rootParams[ROOT_PARAM_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);	// alternatively can place into desc table with desc static flag
	rootParams[ROOT_PARAM_UAV].InitAsUnorderedAccessView(2);
	rootParams[ROOT_PARAM_PUSH_CONST].InitAsConstants(1, 1);
	const CD3DX12_STATIC_SAMPLER_DESC resampleTapFilter(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 1, &resampleTapFilter);
	return CreateRootSignature(sigDesc, L"postprocess root signature");
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLuminanceTextureReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::luminanceTextureReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"luminance texture reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLuminanceBufferReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::luminanceBufferReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"luminance buffer reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateDecode2HalfresPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::decode2halfres),			// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"postprocess decode 2 halfres PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBrightPassPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::brightPass),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom bright pass PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBloomDownsmplePSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::downsample),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom downsample PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBloomUpsmpleBlurPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::upsampleBlur),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom upsample blur PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreatePostprocessFinalCompositePSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSig.Get(),							// root signature
		ShaderBytecode(Shaders::postprocessFinalComposite),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"postprocess final composite PSO");
	return PSO;
}
#pragma endregion

inline RenderPipeline::PipelineStage Impl::Viewport::Pre(ID3D12GraphicsCommandList4 *cmdList, ID3D12Resource *output, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) const
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPre), "viewport pre");
	if (fresh)
	{
		// ClearUnorderedAccessViewFloat() can be used instead but it requires CPU & GPU view handles
		constexpr float initVal = 1;
		const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER initParams[] =
		{
			{cameraSettingsBuffer->GetGPUVirtualAddress(), reinterpret_cast<const UINT &>(initVal)/*strict aliasing rule violation, use C++20 bit_cast instead*/},
			{initParams[0].Dest + sizeof(float), initParams[0].Value}
		};
		cmdList->WriteBufferImmediate(size(initParams), initParams, NULL);
	}
	{
		/*
			could potentially use split barrier for cameraSettingsBuffer to overlap with occlusion culling but it would require conditional finish barrier in world render
			anyway barrier happens only once at viewport creation
		*/
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(1 + fresh, barriers);
	}
	CheckHR(cmdList->Close());
	return cmdList;
}

inline RenderPipeline::PipelineStage Impl::Viewport::Post(ID3D12GraphicsCommandList4 *cmdList, ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface,
	ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer,
	D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, float lumAdaptationLerpFactor, UINT width, UINT height) const
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPost), "viewport post");

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bind postprocess resources
	{
		const auto cameraSettingsBufferGPUAddress = cameraSettingsBuffer->GetGPUVirtualAddress();
		cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());
		cmdList->SetComputeRootSignature(postprocessRootSig.Get());
		cmdList->SetComputeRootDescriptorTable(ROOT_PARAM_DESC_TABLE, postprocessDescriptorTable);
		cmdList->SetComputeRootConstantBufferView(ROOT_PARAM_CBV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRootUnorderedAccessView(ROOT_PARAM_UAV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRoot32BitConstant(ROOT_PARAM_PUSH_CONST, reinterpret_cast<const UINT &>(lumAdaptationLerpFactor)/*use C++20 bit_cast instead*/, 0);
	}

	// initial texture reduction (PSO set during cmd list creation/reset)
	const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
	cmdList->Dispatch(luminanceReductionTexDispatchSize.x, luminanceReductionTexDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// final buffer reduction
	cmdList->SetPipelineState(luminanceBufferReductionPSO.Get());
	cmdList->Dispatch(1, 1, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	const auto ImageDispatchSize = [width, height](UINT lod) noexcept
	{
		return (Math::VectorMath::HLSL::uint2(width >> lod, height >> lod) + CSConfig::ImageProcessing::blockSize - 1) / CSConfig::ImageProcessing::blockSize;
	};

	const auto halfResDispatchSize = ImageDispatchSize(1);

	// decode 2 halfres
	cmdList->SetPipelineState(decode2halfresPSO.Get());
	cmdList->Dispatch(halfResDispatchSize.x, halfResDispatchSize.y, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0));

	// bloom bright pass
	cmdList->SetPipelineState(brightPassPSO.Get());
	cmdList->Dispatch(halfResDispatchSize.x, halfResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	const UINT bloomUpChainLen = bloomUpChain->GetDesc().MipLevels, bloomDownChainLen = bloomDownChain->GetDesc().MipLevels;
	UINT lod = 0;

	// bloom downsample chain
	for (cmdList->SetPipelineState(bloomDownsamplePSO.Get()); lod < bloomDownChainLen; lod++)
	{
		cmdList->SetComputeRoot32BitConstant(ROOT_PARAM_PUSH_CONST, lod, 0);
		const auto dispatchSize = ImageDispatchSize(lod + 2/*halfres + dest offset*/);
		cmdList->Dispatch(dispatchSize.x, dispatchSize.y, 1);

		{
			const D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, lod, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
				CD3DX12_RESOURCE_BARRIER::Transition(lod + 1 < bloomDownChainLen ? bloomDownChain : bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, lod + 1)
			};
			cmdList->ResourceBarrier(size(barriers), barriers);
		}
	}

	// bloom blur + upsample chain
	for (cmdList->SetPipelineState(bloomUpsampleBlurPSO.Get()); lod > 0; lod--)
	{
		cmdList->SetComputeRoot32BitConstant(ROOT_PARAM_PUSH_CONST, lod - 1, 0);
		const auto dispatchSize = ImageDispatchSize(lod/*halfres - dest offset*/);
		cmdList->Dispatch(dispatchSize.x, dispatchSize.y, 1);

		{
			const D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, lod, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, lod - 1),
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
			};
			cmdList->ResourceBarrier(2 + (lod == 2), barriers);
		}
	}

	// postprocess main final composite pass
	cmdList->SetPipelineState(postrpocessFinalCompositePSO.Get());
	const auto fullResDispatchSize = ImageDispatchSize(0);
	cmdList->Dispatch(fullResDispatchSize.x, fullResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// copy to output
	cmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(output, 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(LDRSurface, 0), NULL);

	{
		/*
			finish all barriers here as last action
			it can be delayed even further - until next frame(potentially more efficient - less chance of GPU idle)
			but resource creation tracking needed for it (do not issue barrier for fresh resource)
		*/
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	CheckHR(cmdList->Close());
	return cmdList;
}

Impl::Viewport::Viewport(shared_ptr<const Renderer::World> world) : world(move(world)), viewXform(), projXform()
{
	assert(this->world);

	viewXform[0][0] = viewXform[1][1] = viewXform[2][2] = projXform[0][0] = projXform[1][1] = projXform[2][2] = projXform[3][3] = 1.f;

	/*
		create camera settings buffer
		very small size - consider sharing it with other viewport persistent data
	*/
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Impl::CBRegister::AlignedRow<3>/*or just 'float [3]'?*/), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(cameraSettingsBuffer.GetAddressOf())
	));
	NameObjectF(cameraSettingsBuffer.Get(), L"camera settings buffer for viewport %p", static_cast<Renderer::Viewport *>(this));
}

void Impl::Viewport::SetViewTransform(const float (&matrix)[4][3])
{
	memcpy(viewXform, matrix, sizeof viewXform);
}

void Impl::Viewport::SetProjectionTransform(double fovy, double zn, double zf)
{
	assert(isgreater(fovy, 0.));
	assert(isgreater(zn, 0.));
	assert(isgreater(zf, 0.));
	assert(isless(zn, zf));

	const double yScale = 1. / tan(fovy / 2.), invAspect = double(projXform[0][0]) / double(projXform[1][1]), zScale = isfinite(zf) ? zf / (zf - zn) : 1.;
	projXform[0][0] = yScale * invAspect;
	projXform[1][1] = yScale;
	projXform[2][2] = zScale;
	projXform[3][2] = -zn * zScale;
	projXform[2][3] = 1;
	projXform[3][3] = 0;
}

void Impl::Viewport::UpdateAspect(double invAspect)
{
	projXform[0][0] = projXform[1][1] * invAspect;
}

void Impl::Viewport::Render(ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *ZBuffer, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface,
	ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, UINT width, UINT height) const
{
	// time
	using namespace chrono;
	const Clock::time_point curTime = Clock::now();
	const float delta = duration_cast<duration<float>>(curTime - time).count();
	time = curTime;

	auto cmdLists = cmdListsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Pre, this, cmdLists.pre, output, rtv, dsv);

	const RenderPipeline::RenderPasses::PipelineROPTargets ROPTargets(rendertarget, rtv, backgroundColor, ZBuffer, dsv, 1.f, 0xef, HDRSurface, width, height);
	world->Render(ctx, viewXform, projXform, cameraSettingsBuffer->GetGPUVirtualAddress(), ROPTargets);

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Post, this, cmdLists.post, output, rendertarget, HDRSurface, LDRSurface, bloomUpChain, bloomDownChain, luminanceReductionBuffer,
		postprocessDescriptorTable, CalculateLumAdaptationLerpFactor(delta), width, height);

	// defer as much as possible in order to reduce chances waiting to be inserted in GFX queue (DMA queue can progress enough by this point)
	DMA::Sync();

	GPUWorkSubmission::Run();
	fresh = false;
	cmdListsManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	world->OnFrameFinish();
}