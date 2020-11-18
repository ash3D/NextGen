#pragma once

#define NOMINMAX

#include <wrl/client.h>
#include "../frame versioning.h"

struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList4;
struct ID3D12PipelineState;

namespace Renderer::Impl::PerViewCmdBuffers
{
	namespace WRL = Microsoft::WRL;

	enum CmdListID
	{
		VIEWPORT_PRE,
		VIEWPORT_POST,
		SKYBOX,
		CMDLIST_COUNT
	};

	struct CmdBuffers final
	{
		WRL::ComPtr<ID3D12CommandAllocator> allocator;
		WRL::ComPtr<ID3D12GraphicsCommandList4> lists[CMDLIST_COUNT];
	};

	class CmdBuffersManager;

	class DeferredCmdBuffersProvider final
	{
		friend class CmdBuffersManager;

	private:
		const CmdBuffers &cmdBuffers;

	private:
		explicit DeferredCmdBuffersProvider(const CmdBuffers &cmdBuffers) : cmdBuffers(cmdBuffers)
		{}

	public:
		ID3D12GraphicsCommandList4 *Acquire(CmdListID listID, ID3D12PipelineState *PSO = NULL);
	};

	static constexpr const WCHAR viewportName[] = L"viewport";
	class CmdBuffersManager final : FrameVersioning<CmdBuffers, viewportName>
	{
	public:
		DeferredCmdBuffersProvider OnFrameStart();
		using FrameVersioning::OnFrameFinish;
	};
}