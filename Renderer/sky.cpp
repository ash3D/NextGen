#include "stdafx.h"
#include "sky.hh"
#include "world.hh"	// for globalGPUBuffer
#include "tracked resource.inl"
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "per-view cmd buffers.h"
#include "render passes.h"
#include "shader bytecode.h"
#include "config.h"
#include "PIX events.h"

namespace Shaders
{
#	include "skyboxVS.csh"
#	include "skyboxPS.csh"
}

using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace RenderPasses = Impl::RenderPipeline::RenderPasses;

extern pmr::synchronized_pool_resource globalTransientRAM;
extern ComPtr<ID3D12Device4> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

#pragma region root sig & PSO
ComPtr<ID3D12RootSignature> Impl::Sky::CreateRootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rootParams[ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_PERFRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	rootParams[ROOT_PARAM_CAM_SETTINGS_CBV].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_STATIC_SAMPLER_DESC cubemapSampler(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0, 1, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0, D3D12_FLOAT32_MAX, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 1, &cubemapSampler);
	return CreateRootSignature(sigDesc, L"skybox root signature");
}

ComPtr<ID3D12PipelineState> Impl::Sky::CreatePSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_FRONT,						// view from inside of box
		TRUE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		FALSE,										// depth clip
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

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = rootSig.Get(),
		.VS = ShaderBytecode(Shaders::skyboxVS),
		.PS = ShaderBytecode(Shaders::skyboxPS),
		.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask = UINT_MAX,
		.RasterizerState = rasterDesc,
		.DepthStencilState = dsDesc,
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats = { Config::HDRFormat },
		.DSVFormat = Config::ZFormat::ROP,
		.SampleDesc = Config::MSAA(),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"skybox PSO");
	return result;
}
#pragma endregion

#pragma region ROPBindings + deleter
struct Impl::Sky::ROPBindings final
{
	RenderPasses::PipelineStageRTBinding RTBinding;
	RenderPasses::PipelineStageZBinding ZBinding;
	RenderPasses::ROPOutput output;

public:
	explicit ROPBindings(const RenderPasses::PipelineROPTargets &ROPTargets);
};

// 1 call site
inline Impl::Sky::ROPBindings::ROPBindings(const RenderPasses::PipelineROPTargets &ROPTargets) :
	RTBinding(ROPTargets),
	ZBinding(ROPTargets, false/*useDepth*/, true/*useStencil*/),
	output(ROPTargets)
{
}

void Impl::Sky::ROPBindingsDeleter::operator()(ROPBindingsAllocatorTraits::pointer target)
{
	// TODO: replace with C++20 delete_object
	ROPBindingsAllocatorTraits::destroy(allocator, target);
	ROPBindingsAllocatorTraits::deallocate(allocator, target, 1);
}
#pragma endregion

auto Impl::Sky::RecordRenderCommands(UINT64 cameraSettingsGPUAddress, D3D12_GPU_DESCRIPTOR_HANDLE skyboxDescriptorTable,
	PerViewCmdBuffers::DeferredCmdBuffersProvider viewCmdBuffersProvider, unique_ptr<ROPBindingsAllocatorTraits::value_type, ROPBindingsDeleter> ROPBindings) -> RenderPipeline::PipelineStage
{
	const auto cmdList = viewCmdBuffersProvider.Acquire(PerViewCmdBuffers::SKYBOX, PSO.Get());

	{
		RenderPasses::LocalRenderPassScope renderPassScope(cmdList, ROPBindings->RTBinding, ROPBindings->ZBinding, ROPBindings->output);
		PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::Skybox), "skybox");

		cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		cmdList->SetGraphicsRootSignature(rootSig.Get());
		cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_DESC_TABLE, skyboxDescriptorTable);
		cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_PERFRAME_DATA_CBV, World::GetCurFrameGPUDataPtr());
		cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_CAM_SETTINGS_CBV, cameraSettingsGPUAddress);

		// setup box IB
		cmdList->IASetIndexBuffer(&World::GetBoxIBView());

		cmdList->DrawIndexedInstanced(14, 1, 0, 0, 0);
	}

	CheckHR(cmdList->Close());
	return cmdList;
}

Impl::Sky::Sky(const Renderer::Texture &cubemap, float lumScale) : cubemap(cubemap.Acquire()), lumScale(lumScale)
{
	// validation
	if (cubemap.Usage() != TextureUsage::Skybox)
		throw invalid_argument("Skybox: incompatible texture usage.");

	// create & fill SRV
	const D3D12_DESCRIPTOR_HEAP_DESC heapDesc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	// type
		1,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE			// CPU visible
	};
	CheckHR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(SRV.GetAddressOf())));
	const auto SRVHandle = SRV->GetCPUDescriptorHandleForHeapStart();
	NameObjectF(SRV.Get(), L"CPU descriptor stage for skybox cubemap (Sky object: %p, D3D object: %p, heap start CPU address: %p)", this, SRV.Get(), SRVHandle);
	const D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc
	{
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.TextureCube{.MipLevels = UINT_MAX}
	};
	device->CreateShaderResourceView(this->cubemap.Get(), &SRVDesc, SRVHandle);
}

Impl::Sky::Sky(const Sky &) = default;
Impl::Sky::Sky(Sky &&) = default;
Impl::Sky &Impl::Sky::operator =(const Sky &) = default;
Impl::Sky &Impl::Sky::operator =(Sky &&) = default;
Impl::Sky::~Sky() = default;

D3D12_CPU_DESCRIPTOR_HANDLE Impl::Sky::GetCubemapSRV() const
{
	return SRV->GetCPUDescriptorHandleForHeapStart();
}

void Impl::Sky::Render(UINT64 cameraSettingsGPUAddress, D3D12_GPU_DESCRIPTOR_HANDLE skyboxDescriptorTable,
	PerViewCmdBuffers::DeferredCmdBuffersProvider viewCmdBuffersProvider, const RenderPasses::PipelineROPTargets &ROPTargets) const
{
	// TODO: replace with C++20 new_object
	ROPBindingsAllocatorTraits::allocator_type allocator(&globalTransientRAM);
	const auto ptr = ROPBindingsAllocatorTraits::allocate(allocator, 1);
	try
	{
		ROPBindingsAllocatorTraits::construct(allocator, ptr, ROPTargets);
	}
	catch (...)
	{
		ROPBindingsAllocatorTraits::deallocate(allocator, ptr, 1);
		throw;
	}
	unique_ptr<ROPBindingsAllocatorTraits::value_type, ROPBindingsDeleter> ROPBindingsHandle(ptr, ROPBindingsDeleter(allocator));

	GPUWorkSubmission::AppendPipelineStage<false>(Sky::RecordRenderCommands, cameraSettingsGPUAddress, skyboxDescriptorTable, viewCmdBuffersProvider, move(ROPBindingsHandle));
}