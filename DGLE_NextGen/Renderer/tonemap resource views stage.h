/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <wrl/client.h>

struct ID3D12DescriptorHeap;
struct ID3D12Resource;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace Renderer::Impl::Descriptors
{
	namespace WRL = Microsoft::WRL;

	class TonemapResourceViewsStage;

	namespace GPUDescriptorHeap
	{
		WRL::ComPtr<ID3D12DescriptorHeap> CreateHeap();
		D3D12_GPU_DESCRIPTOR_HANDLE SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &src);
	}

	class TonemapResourceViewsStage
	{
		friend WRL::ComPtr<ID3D12DescriptorHeap> GPUDescriptorHeap::CreateHeap();
		friend D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &src);

		enum ViewIDs
		{
			TextureSRV,
			ReductionBufferUAV,
			ViewCount
		};
		WRL::ComPtr<ID3D12DescriptorHeap> allocation;	// CPU heap does not require lifetime tracking

	public:
		TonemapResourceViewsStage();
		void Fill(ID3D12Resource *texture, ID3D12Resource *reductionBuffer);
	};
}