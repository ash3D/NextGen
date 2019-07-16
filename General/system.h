#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winerror.h>
#include <filesystem>
#include <iostream>

namespace System
{
	extern void __fastcall ValidateHandle(HANDLE handle);

	template<const char name[]>
	class Handle
	{
		const HANDLE handle;

	public:
		Handle(HANDLE handle);
		~Handle();
		Handle(Handle &) = delete;
		void operator =(Handle &) = delete;

	public:
		operator HANDLE() const noexcept { return handle; }
	};

	extern bool __fastcall DetectSSD(const std::filesystem::path &location) noexcept;
}

#pragma region inline/template impl
template<const char name[]>
inline System::Handle<name>::Handle(HANDLE handle) : handle(handle)
{
	ValidateHandle(handle);
}

template<const char name[]>
System::Handle<name>::~Handle()
{
	if (!::CloseHandle(handle))
		std::cerr << "Fail to close " << name << " handle (hr=" << HRESULT_FROM_WIN32(GetLastError()) << ")." << std::endl;
}
#pragma endregion