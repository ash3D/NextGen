#pragma once

#include <wrl/client.h>

struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace Renderer
{
	class Sky;

	namespace Impl
	{
		class OffscreenBuffers;
	}
}

namespace Renderer::Impl::Descriptors
{
	namespace WRL = Microsoft::WRL;

	class PostprocessDescriptorTableStore;

	namespace GPUDescriptorHeap
	{
		struct PerFrameDescriptorTables StreamPerFrameDescriptorTables(const Renderer::Sky &sky, const PostprocessDescriptorTableStore &postprocess);
	}

	class PostprocessDescriptorTableStore
	{
		friend auto GPUDescriptorHeap::StreamPerFrameDescriptorTables(const Renderer::Sky &sky, const PostprocessDescriptorTableStore &postprocess) -> PerFrameDescriptorTables;

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
		WRL::ComPtr<ID3D12DescriptorHeap> CPUStore;	// CPU heap does not require lifetime tracking

	public:
		PostprocessDescriptorTableStore();
		void Fill(const OffscreenBuffers &offscreenBuffers, UINT reductionBufferLength);

	public:
		// CPU handle
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptor(TableEntry entry) const;
	};
}