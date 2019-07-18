#pragma once

#include <utility>
#include <list>
#include "tracked resource.h"

struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

namespace Renderer::Impl::Descriptors
{
	class TonemapResourceViewsStage;

	namespace GPUDescriptorHeap
	{
		namespace Impl
		{
			extern TrackedResource<ID3D12DescriptorHeap> heap, PreallocateHeap();
		}

		inline const auto &GetHeap() noexcept { return Impl::heap; }
		void OnFrameStart();
		D3D12_GPU_DESCRIPTOR_HANDLE SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &src);

		class AllocationClient
		{
			friend void OnFrameStart();

		private:
			static std::list<std::pair<const AllocationClient *, unsigned>> registeredClients;
			decltype(registeredClients)::const_iterator clientLocation;
			mutable UINT64 GPUDescriptorsAllocation;

		protected:
			AllocationClient(unsigned allocSize);
			~AllocationClient();

		public:
			AllocationClient(AllocationClient &) = delete;
			void operator =(AllocationClient &) = delete;

		protected:
			inline auto GetGPUDescriptorsAllocation() const noexcept { return GPUDescriptorsAllocation; }

		private:
			virtual void Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const = 0;
		};
	}
}