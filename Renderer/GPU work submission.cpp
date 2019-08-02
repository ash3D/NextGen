#include "stdafx.h"
#include "GPU work submission.h"
#include "render pipeline.h"
#include "cmdlist pool.inl"

using namespace std;
using namespace Renderer;
using namespace GPUWorkSubmission;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12CommandQueue> gfxQueue;

/*
Current tasks wait implementation based on 'condition_variable' notification mechanism.

'launch::async' policy required in order to guarantee deadlocks elimination that are possible if stdlib select deferred launch policy.
It forces stdlib to launch dedicated thread for task but MSVC implementation is non-conforming in that regard and uses thread pool.
Such behavior (thread pool) is desired here. It would be better if there was additional bitmask for thread pool launch policy as an extension.

Need to keep track the status of Concurrency TS and reimplement waiting via 'wait_all'.
*/

#ifdef _MSC_VER
#define WRAP_CMD_LIST 1
#endif

// prevents TDR when predication (e.g. occlusion culling) used in combination with Z buffer without miplevels (as required for MSAA)\
seems as a driver bug
#define KEPLER_WORKAROUND 1

namespace
{
	constexpr unsigned int targetCmdListWorkSize = 1'00u, GPUSubmitWorkSizeThreshold = 1'000u;

	mutex mtx;
	condition_variable workReadyEvent;
	vector<RenderPipeline::RenderStageItem> workBatch;
	vector<future<void>> pendingAsyncRefs;
	const unsigned int targetTaskCount = []
	{
		const unsigned int maxThreads = thread::hardware_concurrency();
		return maxThreads ? maxThreads : UINT_MAX;
	}();
	unsigned int workBatchFreeSpace = targetCmdListWorkSize, runningTaskCount;

	struct PendingWork
	{
#if WRAP_CMD_LIST
		future<optional<CmdListPool::CmdList>> list;
#else
		future<CmdListPool::CmdList> list;
#endif
		unsigned int size;
		bool suspended;
	};
	struct GPUWorkItem : variant<ID3D12GraphicsCommandList4 *, PendingWork>
	{
		// TODO: use C++17 base class aggregate init instead of inheriting ctor
#if 1
		using variant::variant;
#endif
		// 1 call site
		inline operator ID3D12GraphicsCommandList4 *();
	};
	deque<GPUWorkItem> ROB;

	GPUWorkItem::operator ID3D12GraphicsCommandList4 *()
	{
		// convert to cmd list ptr
		const struct Conveter
		{
			ID3D12GraphicsCommandList4 *operator ()(ID3D12GraphicsCommandList4 *src) const noexcept
			{
				return src;
			}

			ID3D12GraphicsCommandList4 *operator ()(PendingWork &src) const
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

	inline CmdListPool::CmdList RecordCmdList(decltype(workBatch) &&work, CmdListPool::CmdList &&target)
	{
		for (const auto &item : work)
			item.work(target);

#if KEPLER_WORKAROUND
#if 0
		target->ClearState(NULL);
#else
		target->SetPredication(NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
#endif
#endif

		/*
		NOTE: Current command list distribution approach doesn't account "transition" (pre/post) phases (where transition barriers occurs) and only breaks command lists on "range" phases.
		GPU work submission mechanism waits for next render stage even if work batch is saturated in order to butch up all "transition" phases into current batch (greedy approach).
		Thus currently command list breaking should not left any pending barriers and FlushBarriers() call below may only take effect on pipeline finish
		(e.g. World::AABBPassPost() when debug draw enabled) and so it should not lead to any GPU inefficiency due to limited barrier batching.

		It is however possible to implement more generic solution which is capable to batch up inter-stage barriers regardless of
		command list distribution and GPU work submission details - instead of FlushBarriers() here acquire barriers from next cmd list via std::future.
		First FlushBarriers() on cmd list would fire promise and switch function pointer to normal flushing behavior (pass it to D3D12 ResourceBarrier()).
		Synchronization implied by waiting for std::future is not harmful here as it occurs at 'current cmd list finish - next cmd list start'.
		Care should be taken to avoid deadlocks due to threadpool implementation details (e.g. perform final barriers flush on main thread).
		*/
		target.FlushBarriers();

		CheckHR(target->Close());
		target.MarkSuspended(work.back().suspended);

		return move(target);
	}

#if WRAP_CMD_LIST
	// MSVC perform default construction for return value in shared state
	typedef packaged_task<optional<CmdListPool::CmdList> (decltype(workBatch) &&work, CmdListPool::CmdList &&target)> RecordCmdListTask;
#else
	typedef packaged_task<decltype(RecordCmdList)> RecordCmdListTask;
#endif

	inline void LaunchRecordCmdList(RecordCmdListTask &&task, decltype(workBatch) &&work, CmdListPool::CmdList &&target)
	{
		task(move(work), move(target));
		{
			lock_guard lck(mtx);
			runningTaskCount--;
		}
		workReadyEvent.notify_one();
	}

	inline void LaunchBuildRenderStage(packaged_task<RenderPipeline::PipelineStage ()> &&buildRenderStage)
	{
		buildRenderStage();
		mtx.lock();
		mtx.unlock();
		workReadyEvent.notify_one();
	}

	void FlushWorkBatch(unique_lock<decltype(mtx)> &lck)
	{
		assert(!workBatch.empty());
		RecordCmdListTask task(RecordCmdList);
		const auto reserved = workBatch.capacity();
		ROB.emplace_back(PendingWork{ task.get_future(), targetCmdListWorkSize - workBatchFreeSpace, workBatch.back().suspended });

		auto asyncRef = async(launch::async, LaunchRecordCmdList, move(task), move(workBatch), CmdListPool::CmdList());
		auto lckSentry(move(lck));
		pendingAsyncRefs.push_back(move(asyncRef));
		lckSentry.swap(lck);

		workBatchFreeSpace = targetCmdListWorkSize;
		runningTaskCount++;
		workBatch.reserve(reserved);
	}
}

void GPUWorkSubmission::Prepare()
{
	/*
	This makes renderer non-reenterable after exception thrown during pipeline construction.
	But currently there is no recovery mechanism so renderer left in invalid state anyway, application should be aborted on exception.
	In the future however when recovery will be implemented std::unique_lock or similar should be used to provide exception guarantee.
	*/
	mtx.lock();
}

namespace Renderer::GPUWorkSubmission
{
	void AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()> &&buildRenderStage)
	{
		RenderPipeline::AppendStage(buildRenderStage.get_future());
		auto asyncRef = async(launch::async, LaunchBuildRenderStage, move(buildRenderStage));
		unique_lock lckSentry(mtx, adopt_lock);
		pendingAsyncRefs.push_back(move(asyncRef));
		lckSentry.release();
	}
}

void GPUWorkSubmission::Run()
{
	{
		unique_lock lck(mtx, adopt_lock);

		do
		{
			workReadyEvent.wait(lck);

			// launch command lists recording
			while (workBatch.empty() || runningTaskCount < targetTaskCount)
			{
				const auto item = RenderPipeline::GetNext(workBatchFreeSpace);
				if (const auto cmdList = get_if<ID3D12GraphicsCommandList4 *>(&item))
				{
					// flush work batch if needed
					if (!workBatch.empty())
						FlushWorkBatch(lck);

					assert(*cmdList);
					ROB.emplace_back(*cmdList);
				}
				else if (const auto stageItem = get_if<RenderPipeline::RenderStageItem>(&item))
				{
					if (stageItem->work)
						workBatch.push_back(move(*stageItem));
					else // batch overflow
						FlushWorkBatch(lck);
				}
				else // stage waiting/pipeline finish
				{
					if (!workBatch.empty() && RenderPipeline::Empty())
						FlushWorkBatch(lck);
					break;
				}
			}

			// submit command list batch if ready
			auto doneWorkEnd = ROB.begin(), readyWorkEnd = doneWorkEnd;
			unsigned int doneWorkSize = 0, readyWorkSize = 0;
			for (const auto workEnd = ROB.end(); doneWorkEnd != workEnd; ++doneWorkEnd)
			{
				if (const PendingWork *const pendingWork = get_if<PendingWork>(&*doneWorkEnd))
				{
					if (pendingWork->list.wait_for(0s) == future_status::timeout)
						break;
					doneWorkSize += pendingWork->size;
					if (pendingWork->suspended)
						continue;
				}
				++(readyWorkEnd = doneWorkEnd);
				readyWorkSize = doneWorkSize;
			}
			if (readyWorkSize >= GPUSubmitWorkSizeThreshold || RenderPipeline::Empty() && readyWorkEnd == ROB.end())
			{
				static vector<ID3D12CommandList *> listsToExequte;
				listsToExequte.assign(ROB.begin(), readyWorkEnd);
				gfxQueue->ExecuteCommandLists(listsToExequte.size(), listsToExequte.data());
				ROB.erase(ROB.begin(), readyWorkEnd);
			}
		} while (!ROB.empty() || !RenderPipeline::Empty());
	}

	pendingAsyncRefs.clear();
}