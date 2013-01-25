/**
\author		Alexey Shaydurov aka ASH
\date		25.1.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"

namespace Win32Heap
{
	class CWin32Heap
	{
	public:
		CWin32Heap(): _handle(HeapCreate(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE, 0, 0))
		{
			if (!_handle) throw "fail to create heap";
		}
		CWin32Heap(CWin32Heap &&src) noexcept: _handle(src._handle)
		{
			src._handle = NULL;
		}
		CWin32Heap &operator =(CWin32Heap &&src) noexcept
		{
			_ASSERT(this != &src);
			_handle = src._handle;
			src._handle = NULL;
			return *this;
		}
		~CWin32Heap()
		{
			if (_handle)
				if (!HeapDestroy(_handle)) throw "fail to destroy heap";
		}
	public:
		operator HANDLE() const noexcept
		{
			return _handle;
		}
	private:
		HANDLE _handle;
	};

	template<typename T>
	class CWin32HeapAllocator: public std::allocator<T>
	{
		template<typename T>
		friend class CWin32HeapAllocator;
	public:
		template<typename Other>
		struct rebind
		{
			typedef CWin32HeapAllocator<Other> other;
		};
		CWin32HeapAllocator(HANDLE heap = HANDLE(_get_heap_handle())) noexcept: _heap(heap) {}
		template<typename Other>
		CWin32HeapAllocator(const CWin32HeapAllocator<Other> &src) noexcept: _heap(src._heap) {}
		pointer allocate(size_type count, const void * = NULL)
		{
			try
			{
				return pointer(HeapAlloc(_heap, 0, count * sizeof(T)));
			}
			catch (...)
			{
				throw bad_alloc();
			}
		}
		void deallocate(pointer ptr, size_type)
		{
			if (!HeapFree(_heap, 0, ptr)) throw "fail to free heap block";
		}
	private:
		HANDLE _heap;
	};
}