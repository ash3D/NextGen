#pragma once

#include <wrl/client.h>

struct ID3D12DescriptorHeap;
struct ID3D12Resource;
struct IDXGISwapChain4;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

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
			COCBufferUAV,
			DilatedCOCBufferUAV,
			HalfresDOFInputUAV,
			DOFLayersUAV,
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
		void Fill(ID3D12Resource *ZBuffer, ID3D12Resource *src, ID3D12Resource *composite, ID3D12Resource *dst,
			ID3D12Resource *COCBuffer, ID3D12Resource *dilatedCOCBuffer, ID3D12Resource *halferDOFSurface, ID3D12Resource *DOFLayers, ID3D12Resource *lensFlareSurface,
			ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *reductionBuffer, UINT reductionBufferLength);

	public:
		// CPU handle
		D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptor(TableEntry entry) const;
	};
}