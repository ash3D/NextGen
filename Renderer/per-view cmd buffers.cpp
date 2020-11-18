#include "stdafx.h"
#include "per-view cmd buffers.h"

using namespace Renderer::Impl::PerViewCmdBuffers;
using WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;
void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

ID3D12GraphicsCommandList4 *DeferredCmdBuffersProvider::Acquire(CmdListID listID, ID3D12PipelineState *PSO)
{
	assert(listID >= 0 && listID < CMDLIST_COUNT);
	const auto &list = cmdBuffers.lists[listID];

	// reset list
	CheckHR(list->Reset(cmdBuffers.allocator.Get(), PSO));

	return list.Get();
}

// result valid until call to 'OnFrameFinish()'
auto CmdBuffersManager::OnFrameStart() -> DeferredCmdBuffersProvider
{
	FrameVersioning::OnFrameStart();
	CmdBuffers &cmdBuffers = GetCurFrameDataVersion();

	if (cmdBuffers.allocator && cmdBuffers.lists[VIEWPORT_PRE] && cmdBuffers.lists[VIEWPORT_POST] && cmdBuffers.lists[SKYBOX])
		// reset allocator
		CheckHR(cmdBuffers.allocator->Reset());
	else
	{
		const unsigned short createVersion = GetFrameLatency() - 1;

		// create allocator
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.allocator)));
		NameObjectF(cmdBuffers.allocator.Get(), L"per-view command allocator [%hu] (cmd buffers manager %p)", createVersion, this);

		// create lists
		CheckHR(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cmdBuffers.lists[VIEWPORT_PRE])));
		NameObjectF(cmdBuffers.lists[VIEWPORT_PRE].Get(), L"viewport pre command list [%hu] (cmd buffers manager %p)", createVersion, this);
		CheckHR(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cmdBuffers.lists[VIEWPORT_POST])));
		NameObjectF(cmdBuffers.lists[VIEWPORT_POST].Get(), L"viewport post command list [%hu] (cmd buffers manager %p)", createVersion, this);
		CheckHR(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cmdBuffers.lists[SKYBOX])));
		NameObjectF(cmdBuffers.lists[SKYBOX].Get(), L"skybox command list [%hu] (cmd buffers manager %p)", createVersion, this);
	}

	return DeferredCmdBuffersProvider{ cmdBuffers };
}