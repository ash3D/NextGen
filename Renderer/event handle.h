#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include <Windows.h>

namespace Renderer::Impl
{
	class EventHandle
	{
		const HANDLE handle;

	public:
		EventHandle();
		~EventHandle();
		EventHandle(EventHandle &) = delete;
		void operator =(EventHandle &) = delete;

	public:
		operator HANDLE () const noexcept { return handle; }
	};
}