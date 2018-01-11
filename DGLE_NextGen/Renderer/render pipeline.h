/**
\author		Alexey Shaydurov aka ASH
\date		11.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <cassert>
#include <future>
#include <variant>

struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList1;

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	struct IRenderStage
	{
		virtual void Sync() const = 0;
		virtual unsigned int RenderPassCount() const noexcept = 0;
		virtual const struct IRenderPass &operator [](unsigned renderPassIdx) const = 0;

	protected:
		IRenderStage() = default;
		~IRenderStage() = default;
		IRenderStage(IRenderStage &) = delete;
		void operator =(IRenderStage &) = delete;
	};

	struct IRenderPass
	{
		virtual unsigned long int Length() const noexcept = 0;
		virtual ID3D12PipelineState *GetPSO(unsigned long int i) const = 0;
		virtual void operator ()(const IRenderStage &parent, unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *target) const = 0;

	protected:
		IRenderPass() = default;
		~IRenderPass() = default;
		IRenderPass(IRenderPass &) = delete;
		void operator =(IRenderPass &) = delete;
	};

	class RenderRange
	{
		unsigned long int rangeBegin, rangeEnd;
		const IRenderStage *renderStage;
		unsigned renderPassIdx;

		// accessed by std::variant ctor => public (it may be accessed in base/helper classes/functions => adding std::variant to friend is not sufficient)
	public:
		RenderRange(unsigned long int rangeBegin, unsigned long int rangeEnd, const IRenderStage *renderStage, unsigned renderPassIdx) noexcept;

	public:
		explicit operator bool() const noexcept { return renderStage; }
		void operator ()(CmdListPool::CmdList &target) const;
	};

	typedef std::variant<ID3D12GraphicsCommandList1 *, const IRenderStage *> PipelineStage;

	void AppendStage(std::future<PipelineStage> &&stage);
	std::variant<ID3D12GraphicsCommandList1 *, RenderRange> GetNext(unsigned int &length);
	bool Empty();
}