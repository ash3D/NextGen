//--------------------------------------------------------------------------------------
// File: DDSTextureLoader12.h
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#include <d3d12.h>

#include <memory>
#include <vector>
#include <stdint.h>
#include <cassert>
#include <type_traits>


namespace DirectX
{
    enum DDS_ALPHA_MODE
    {
        DDS_ALPHA_MODE_UNKNOWN       = 0,
        DDS_ALPHA_MODE_STRAIGHT      = 1,
        DDS_ALPHA_MODE_PREMULTIPLIED = 2,
        DDS_ALPHA_MODE_OPAQUE        = 3,
        DDS_ALPHA_MODE_CUSTOM        = 4,
    };

    enum DDS_LOADER_FLAGS
    {
        DDS_LOADER_DEFAULT = 0,
        DDS_LOADER_FORCE_SRGB = 0x1,
        DDS_LOADER_MIP_RESERVE = 0x8,
    };

	enum DDS_CPU_ACCESS_FLAGS
	{
		DDS_CPU_ACCESS_DENY			= 001,
		DDS_CPU_ACCESS_INDIRECT		= 002,
		DDS_CPU_ACCESS_DIRECT		= 003,
		DDS_CPU_ACCESS_ALLOW_READS	= 010,
	};

	namespace Impl
	{
		constexpr bool ValidateIntermediateCPUAccessFlags(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> flags) noexcept
		{
			return flags & 3 && !(flags & ~013);
		}

		constexpr bool ValidateSimmHelper(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> a, std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> b)
		{
			return a == DDS_CPU_ACCESS_ALLOW_READS && b == DDS_CPU_ACCESS_ALLOW_READS || a == ~DDS_CPU_ACCESS_ALLOW_READS && b == ~DDS_CPU_ACCESS_ALLOW_READS;
		}

		constexpr bool ValidateOR(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> a, std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> b) noexcept
		{
			return ValidateIntermediateCPUAccessFlags(a) && b == DDS_CPU_ACCESS_ALLOW_READS;
		}

		constexpr bool ValidateORSimm(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> a, std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> b) noexcept
		{
			return ValidateOR(a, b) || ValidateOR(b, a) || ValidateSimmHelper(a, b);
		}

		constexpr bool ValidateAND(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> a, std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> b) noexcept
		{
			return ValidateIntermediateCPUAccessFlags(a) && b == ~DDS_CPU_ACCESS_ALLOW_READS;
		}

		constexpr bool ValidateANDSimm(std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> a, std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> b) noexcept
		{
			return ValidateAND(a, b) || ValidateAND(b, a) || ValidateSimmHelper(a, b);
		}
	}

	constexpr DDS_CPU_ACCESS_FLAGS operator ~(DDS_CPU_ACCESS_FLAGS a) noexcept
	{
		typedef std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> int_t;
		assert(a == DDS_CPU_ACCESS_ALLOW_READS || a == ~int_t(DDS_CPU_ACCESS_ALLOW_READS));
		return DDS_CPU_ACCESS_FLAGS(~int_t(a));
	}

	constexpr DDS_CPU_ACCESS_FLAGS operator |(DDS_CPU_ACCESS_FLAGS a, DDS_CPU_ACCESS_FLAGS b) noexcept
	{
		typedef std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> int_t;
		assert(Impl::ValidateORSimm(a, b));
		return DDS_CPU_ACCESS_FLAGS(int_t(a) | int_t(b));
	}

#ifndef _MSC_VER
	constexpr
#endif
	inline DDS_CPU_ACCESS_FLAGS &operator |=(DDS_CPU_ACCESS_FLAGS &a, DDS_CPU_ACCESS_FLAGS b) noexcept
	{
		typedef std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> int_t;
		assert(Impl::ValidateOR(a, b));
		return reinterpret_cast<DDS_CPU_ACCESS_FLAGS &>(reinterpret_cast<int_t &>(a) |= int_t(b));
	}

 	constexpr DDS_CPU_ACCESS_FLAGS operator &(DDS_CPU_ACCESS_FLAGS a, DDS_CPU_ACCESS_FLAGS b) noexcept
	{
		typedef std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> int_t;
		assert(Impl::ValidateANDSimm(a, b));
		return DDS_CPU_ACCESS_FLAGS(int_t(a) & int_t(b));
	}

#ifndef _MSC_VER
	constexpr
#endif
	inline DDS_CPU_ACCESS_FLAGS &operator &=(DDS_CPU_ACCESS_FLAGS &a, DDS_CPU_ACCESS_FLAGS b) noexcept
	{
		typedef std::underlying_type_t<DDS_CPU_ACCESS_FLAGS> int_t;
		assert(Impl::ValidateAND(a, b));
		return reinterpret_cast<DDS_CPU_ACCESS_FLAGS &>(reinterpret_cast<int_t &>(a) &= int_t(b));
	}

	void operator ^(DDS_CPU_ACCESS_FLAGS, DDS_CPU_ACCESS_FLAGS) = delete;
	void operator ^=(DDS_CPU_ACCESS_FLAGS, DDS_CPU_ACCESS_FLAGS) = delete;
	// ... and other operators (arithmetic, shift etc)\
	not actually required since built-in returns int and it isn't accepted anyway

   // Standard version
    HRESULT __cdecl LoadDDSTextureFromMemory(
        _In_ ID3D12Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        _Outptr_ ID3D12Resource** texture,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    HRESULT __cdecl LoadDDSTextureFromFile(
        _In_ ID3D12Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        _Outptr_ ID3D12Resource** texture,
        std::unique_ptr<uint8_t[]>& ddsData,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        size_t maxsize = 0,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    // Extended version
    HRESULT __cdecl LoadDDSTextureFromMemoryEx(
        _In_ ID3D12Device* d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        size_t maxsize,
        D3D12_RESOURCE_FLAGS resFlags,
        unsigned int loadFlags,
		DDS_CPU_ACCESS_FLAGS CPUAccessFlags,
        _Outptr_ ID3D12Resource** texture,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);

    HRESULT __cdecl LoadDDSTextureFromFileEx(
        _In_ ID3D12Device* d3dDevice,
        _In_z_ const wchar_t* szFileName,
        size_t maxsize,
        D3D12_RESOURCE_FLAGS resFlags,
        unsigned int loadFlags,
 		DDS_CPU_ACCESS_FLAGS CPUAccessFlags,
       _Outptr_ ID3D12Resource** texture,
        std::unique_ptr<uint8_t[]>& ddsData,
        std::vector<D3D12_SUBRESOURCE_DATA>& subresources,
        _Out_opt_ DDS_ALPHA_MODE* alphaMode = nullptr,
        _Out_opt_ bool* isCubeMap = nullptr);
}
