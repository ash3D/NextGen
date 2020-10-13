#pragma once

#include <wrl/client.h>

struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace Renderer::Impl
{
	class OffscreenBuffers;
}

namespace Renderer::Impl::Descriptors
{
	namespace WRL = Microsoft::WRL;

	class PostprocessDescriptorTableStore;

	namespace GPUDescriptorHeap
	{
		D3D12_GPU_DESCRIPTOR_HANDLE FillPostprocessGPUDescriptorTableStore(const PostprocessDescriptorTableStore &src);
	}

	class PostprocessDescriptorTableStore
	{
		friend D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::FillPostprocessGPUDescriptorTableStore(const PostprocessDescriptorTableStore &src);

	public:
		enum TableEntry
		{
			ZBufferSRV,
			SrcSRV,
			CompositeSRV,
			CompositeUAV,
			DstUAV,
			ReductionBufferUAV,
			ReductionBufferSRV,
			DOFOpacityBufferUAV,
			COCBufferUAV,
			DilatedCOCBufferUAV,
			HalfresDOFInputUAV,
			DOFLayersUAV,
			DOFOpacityBufferSRV,
			COCBufferSRV,
			DilatedCOCBufferSRV,
			HalfresDOFInputSRV,
			DOFLayersSRV,
			LensFlareSRV,
			BloomUpChainSRV,
			BloomDownChainSRV,
			BloomUpChainUAVFirst,
			BloomUpChainUAVLast = BloomUpChainUAVFirst + 4,		// inclusive, 5 mips
			BloomDownChainUAVFirst,
			BloomDownChainUAVLast = BloomDownChainUAVFirst + 5,	// inclusive, 6 mips
			TableSize
		};

	private:
		WRL::ComPtr<ID3D12DescriptorHeap> allocation;	// CPU heap does not require lifetime tracking

	public:
		PostprocessDescriptorTableStore();
		void Fill(const OffscreenBuffers &offscreenBuffers, UINT reductionBufferLength);

	public:
		// CPU handle
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptor(TableEntry entry) const;
	};
}