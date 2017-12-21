/**
\author		Alexey Shaydurov aka ASH
\date		21.12.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer::QueryPool
{
	namespace WRL = Microsoft::WRL;

	class OcclusionQuery
	{
		std::pair<WRL::ComPtr<ID3D12QueryHeap>, WRL::ComPtr<ID3D12Resource>> chunk;
		UINT idx;

	public:
		void Start(ID3D12GraphicsCommandList1 *target), Stop(ID3D12GraphicsCommandList1 *target);
		void Set(ID3D12GraphicsCommandList1 *target);
	};

	class OcclusionQueryBatch
	{
		static std::shared_mutex mtx;
		static std::deque<std::pair<WRL::ComPtr<ID3D12Heap>, WRL::ComPtr<ID3D12Resource>>> chunks;
		static unsigned long capacity;

	private:
		unsigned long count;

	public:
		explicit OcclusionQueryBatch(unsigned long count);

	public:
		OcclusionQuery GetNext();
		void Resolve(ID3D12GraphicsCommandList1 *target), Finish(ID3D12GraphicsCommandList1 *target);
	};
}