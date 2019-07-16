#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winerror.h>
#include <comdef.h>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace System
{
#pragma region wide IO
	constexpr const char defaulStreamAccessMode[] = "w";
	extern void __fastcall WideIOPrologue(FILE *stream, const char accessMode[] = defaulStreamAccessMode), __fastcall WideIOEpilogue(FILE *stream, const char accessMode[] = defaulStreamAccessMode);

	template<const char accessMode[] = defaulStreamAccessMode>
	class WideIOGuard
	{
		FILE *const stream;

	public:
		explicit WideIOGuard(FILE *stream) : stream(stream) { WideIOPrologue(stream, accessMode); }
		~WideIOGuard() { WideIOEpilogue(stream, accessMode); }
		WideIOGuard(WideIOGuard &) = delete;
		void operator =(WideIOGuard &) = delete;
	};
#pragma endregion


#pragma region HANDLE
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
#pragma endregion


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
	{
		System::WideIOGuard IOGuard(stderr);
		std::wcerr << "Fail to close " << name << " handle: " << _com_error(HRESULT_FROM_WIN32(GetLastError())).ErrorMessage() << std::endl;
	}
}
#pragma endregion