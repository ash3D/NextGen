/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "GPU work submission.h"
#include "render pipeline.h"
#include "cmdlist pool.h"

using namespace std;
using namespace Renderer;
using namespace GPUWorkSubmission;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12CommandQueue> cmdQueue;

/*
Current tasks wait implementation based on 'condition_variable' notification mechanism.

'launch::async' policy required in order to guarantee deadlocks elimination that are possible if stdlib select deferred launch policy.
It forces stdlib to launch dedicated thread for task but MSVC implementaion is non-conforming in that regard and uses thread pool.
Such behavior (thread pool) is desired here. It would be better if there was additional bitmask for thred pool launh policy as an extension.

Need to keep track the status of Concurrency TS and reimplement waiting via 'wait_all'.
*/

#ifdef _MSC_VER
#define WRAP_CMD_LIST 1
#endif

namespace
{
	constexpr unsigned int targetCmdListWorkSize = 1'000u, GPUSubmitWorkSizeThreshold = 3'000u;

	mutex mtx;
	condition_variable workReadyEvent;
	vector<RenderPipeline::RenderRange> workAccumulator;
	const unsigned int targetTaskCount = max(thread::hardware_concurrency(), 1u);
	unsigned int accumulatedWorkFreeSpace = targetCmdListWorkSize, runningTaskCount;

	struct PendingWork
	{
#if WRAP_CMD_LIST
		future<optional<CmdListPool::CmdList>> list;
#else
		future<CmdListPool::CmdList> list;
#endif
		unsigned int size;
	};
	struct GPUWorkItem : variant<ID3D12GraphicsCommandList1 *, PendingWork>
	{
		// TODO: use C++17 base class aggregate init instead of inheriting ctor
#if 1
		using variant::variant;
#endif
		// 1 call site
		inline operator ID3D12GraphicsCommandList1 *();
	};
	deque<GPUWorkItem> ROB;

	GPUWorkItem::operator ID3D12GraphicsCommandList1 *()
	{
		// convert to cmd list ptr
		const struct Conveter
		{
			ID3D12GraphicsCommandList1 *operator ()(ID3D12GraphicsCommandList1 *src) const noexcept
			{
				return src;
			}

			ID3D12GraphicsCommandList1 *operator ()(PendingWork &src) const
			{
#if WRAP_CMD_LIST
				return *src.list.get();
#else
				return src.list.get();
#endif
			}
		} converter;
		return visit(converter, static_cast<variant &>(*this));
	}

	inline CmdListPool::CmdList RecordCmdList(decltype(workAccumulator) &&work, CmdListPool::CmdList &&target)
	{
		for (const auto &range : work)
			range(target);
		CheckHR(target->Close());
		return move(target);
	}

#if WRAP_CMD_LIST
	// MSVC perform default construction for return value in shared state
	typedef packaged_task<optional<CmdListPool::CmdList> (decltype(workAccumulator) &&work, CmdListPool::CmdList &&target)> RecordCmdListTask;
#else
	typedef packaged_task<decltype(RecordCmdList)> RecordCmdListTask;
#endif

	inline void LaunchRecordCmdList(RecordCmdListTask &&task, decltype(workAccumulator) &&work, CmdListPool::CmdList &&target)
	{
		task(move(work), move(target));
		{
			lock_guard<decltype(mtx)> lck(mtx);
			runningTaskCount--;
		}
		workReadyEvent.notify_one();
	}

	struct PipelineStageVisitor
	{
		// 1 call site
		inline void operator ()(ID3D12GraphicsCommandList1 *cmdList) const, operator ()(RenderPipeline::RenderRange &&renderRange) const;
	};

	void PipelineStageVisitor::operator ()(ID3D12GraphicsCommandList1 *cmdList) const
	{
	}

	void PipelineStageVisitor::operator ()(RenderPipeline::RenderRange &&range) const
	{
	}
}

void GPUWorkSubmission::Prepare()
{
	mtx.lock();
}

namespace Renderer::GPUWorkSubmission
{
	void LaunchBuildRenderStage(packaged_task<RenderPipeline::PipelineStage ()> &&buildRenderStage)
	{
		buildRenderStage();
		mtx.lock();
		mtx.unlock();
		workReadyEvent.notify_one();
	}

	vector<future<void>> pendingAsyncRefs;
}

void GPUWorkSubmission::Run()
{
	{
		unique_lock<decltype(mtx)> lck(mtx, adopt_lock);

		do
		{
			workReadyEvent.wait(lck);

			// launch command lists recording
			while (workAccumulator.empty() || runningTaskCount < targetTaskCount)
			{
				const auto item = RenderPipeline::GetNext(accumulatedWorkFreeSpace);
				if (const auto cmdList = get_if<ID3D12GraphicsCommandList1 *>(&item))
				{
					// flush accumulated work if needed
					const bool flushAccumulatedWork = accumulatedWorkFreeSpace == 0 || !workAccumulator.empty() && (*cmdList || RenderPipeline::Empty());
					if (flushAccumulatedWork)
					{
						RecordCmdListTask task(RecordCmdList);
						const auto reserved = workAccumulator.capacity();
						ROB.emplace_back(PendingWork{ task.get_future(), targetCmdListWorkSize - accumulatedWorkFreeSpace });
						pendingAsyncRefs.push_back(async(launch::async, LaunchRecordCmdList, move(task), move(workAccumulator), CmdListPool::CmdList()));
						accumulatedWorkFreeSpace = targetCmdListWorkSize;
						runningTaskCount++;
						workAccumulator.reserve(reserved);
					}

					if (*cmdList)
						ROB.emplace_back(*cmdList);
					else if (!flushAccumulatedWork)
						break;
				}
				else // item is render range
					workAccumulator.push_back(get<RenderPipeline::RenderRange>(move(item)));
			}

			// submit command list batch if ready
			auto readyWorkEnd = ROB.begin();
			unsigned int readyWorkSize = 0;
			for (const auto workEnd = ROB.end(); readyWorkEnd != workEnd; ++readyWorkEnd)
			{
				if (const PendingWork *const pendingWork = get_if<PendingWork>(&*readyWorkEnd))
				{
					if (pendingWork->list.wait_for(0s) != future_status::timeout)
						readyWorkSize += pendingWork->size;
					else
						break;
				}
			}
			if (readyWorkSize >= GPUSubmitWorkSizeThreshold || RenderPipeline::Empty() && readyWorkEnd == ROB.end())
			{
				const vector<ID3D12CommandList *> listsToExequte(ROB.begin(), readyWorkEnd);
				cmdQueue->ExecuteCommandLists(listsToExequte.size(), listsToExequte.data());
				ROB.erase(ROB.begin(), readyWorkEnd);
			}
		} while (!ROB.empty() || !RenderPipeline::Empty());
	}

	pendingAsyncRefs.clear();
}