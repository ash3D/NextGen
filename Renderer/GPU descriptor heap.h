#pragma once

#include <utility>
#include <list>
#include "tracked resource.h"

struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace Renderer
{
	class Sky;

	namespace Impl::Descriptors
	{
		class PostprocessDescriptorTableStore;
	}
}

namespace Renderer::Impl::Descriptors::GPUDescriptorHeap
{
	namespace Impl
	{
		extern TrackedResource<ID3D12DescriptorHeap> heap, PreallocateHeap();
	}

	inline const auto &GetHeap() noexcept { return Impl::heap; }
	void OnFrameStart();

	struct PerFrameDescriptorTables
	{
		// UINT64 instead of D3D12_GPU_DESCRIPTOR_HANDLE in order to eliminate dependency on d3d12.h
		UINT64 skybox, postprocess;
	};
	PerFrameDescriptorTables StreamPerFrameDescriptorTables(const Renderer::Sky &sky, const PostprocessDescriptorTableStore &postprocess);

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