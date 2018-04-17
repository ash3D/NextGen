/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <shared_mutex>
#include "../tracked resource.h"

struct ID3D12Resource;
struct ID3D12GraphicsCommandList2;
struct D3D12_STREAM_OUTPUT_BUFFER_VIEW;

namespace Renderer::Impl::SOBuffer
{
	class Handle
	{
		friend class AllocatorBase;

	private:
		ID3D12Resource *buffer;
		unsigned long size = 0;
		long int curUsage;	// consider moving it to template param (it would make usage static)
		mutable long int prevUsage;

	public:
		Handle() = default;
		inline Handle(Handle &&src) noexcept;
		inline Handle &operator =(Handle &&srv) noexcept;

	private:
		Handle(ID3D12Resource *buffer, unsigned long size, long int usage) noexcept : buffer(buffer), size(size), curUsage(usage) {}

	public:
		void Sync() const;

	public:
		void StartSO(ID3D12GraphicsCommandList2 *target) const, UseSOResults(ID3D12GraphicsCommandList2 *target) const, Finish(ID3D12GraphicsCommandList2 *target) const;

	public:
		const unsigned long GetSize() const noexcept { return size; }
		const D3D12_STREAM_OUTPUT_BUFFER_VIEW GetSOView() const;	// valid after StartSO()
		const UINT64 GetGPUPtr() const;								// valid after UseSOResults() with usage specified at creation
	};

	class AllocatorBase
	{
		TrackedResource<ID3D12Resource> buffer;
		std::shared_mutex mtx;
		unsigned long version = 0;

	protected:
		AllocatorBase() = default;
		AllocatorBase(AllocatorBase &) = delete;
		AllocatorBase &operator =(AllocatorBase &) = delete;
		~AllocatorBase();

	protected:
		Handle Allocate(unsigned long size, long int usage, LPCWSTR resourceName);
	};

	template<LPCWSTR resourceName>
	class Allocator : public AllocatorBase
	{
	public:
		Handle Allocate(unsigned long size, long int usage) { return AllocatorBase::Allocate(size, usage, resourceName); }
	};

#pragma region inline impl
	inline Handle::Handle(Handle &&src) noexcept : buffer(src.buffer), size(src.size), curUsage(src.curUsage), prevUsage(src.prevUsage)
	{
		src.size = 0;
	}

	inline Handle &Handle::operator =(Handle &&src) noexcept
	{
		buffer = src.buffer;
		size = src.size;
		curUsage = src.curUsage;
		prevUsage = src.prevUsage;
		src.size = 0;
		return *this;
	}
#pragma endregion
}