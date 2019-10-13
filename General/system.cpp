#include <Windows.h>
//#include <ntddstor.h>
#include <ntddscsi.h>
#include <winerror.h>
#include <comdef.h>
#include "system.h"
#include <cstddef>	// for offsetof
#include <cwchar>
#include <utility>	// for as_const
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <exception>
#include <stdexcept>
#define NOMINMAX

using namespace std;

void __fastcall System::WideIOPrologue(FILE *stream, const char accessMode[])
{
	if (fwide(stream, 0) < 0)
	{
		[[maybe_unused]] const auto reopened = freopen(NULL, accessMode, stream);
#ifndef _WIN32
		assert(reopened);
#endif
	}
}

void __fastcall System::WideIOEpilogue(FILE *stream, const char accessMode[])
{
	// reopen again to reset orientation
	[[maybe_unused]] const auto reopened = freopen(NULL, accessMode, stream);
#ifndef _WIN32
	assert(reopened);
#endif
}

void __fastcall System::ValidateHandle(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
}

const char driveDetectionFailMsgPrefix[] = "Fail to detect drive type: ";

static void PrintError(const char *msg)
{
	cerr << driveDetectionFailMsgPrefix << msg << endl;
}

static void PrintError(const wchar_t *msg)
{
	System::WideIOGuard IOGuard(stderr);
	wcerr << driveDetectionFailMsgPrefix << msg << endl;
}

static constexpr const char driveHandleName[] = "drive";

// 1 call site
static inline bool DetectSDD(const filesystem::path &root)
{
	const auto driveName = L"\\\\.\\" + root.native();

#ifdef _MSC_VER
	const System::Handle<driveHandleName> driveHandle(CreateFileW(driveName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
	const auto StorageQuery = [&driveHandle]
#else
	const auto StorageQuery = [driveHandle = System::Handle<driveHandleName>(CreateFileW(driveName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL))]
#endif
	(DWORD controlCode, auto &in, auto &out)
	{
		DWORD bytesReturned;
		return DeviceIoControl(driveHandle, controlCode, &in, sizeof in, &out, sizeof out, &bytesReturned, NULL) && bytesReturned == sizeof out;
	};

	// check TRIM support
	{
		STORAGE_PROPERTY_QUERY trimQuery = { StorageDeviceTrimProperty, PropertyStandardQuery };
		if (DEVICE_TRIM_DESCRIPTOR trimDesc; StorageQuery(IOCTL_STORAGE_QUERY_PROPERTY, trimQuery, trimDesc))
			return trimDesc.TrimEnabled;
	}

	// check seek penalty
	{
		STORAGE_PROPERTY_QUERY seekQuery = { StorageDeviceSeekPenaltyProperty, PropertyStandardQuery };
		if (DEVICE_SEEK_PENALTY_DESCRIPTOR seekDesc; StorageQuery(IOCTL_STORAGE_QUERY_PROPERTY, seekQuery, seekDesc))
			return !seekDesc.IncursSeekPenalty;
	}

	// check RPM
	{
		// https://nyaruru.hatenablog.com/entry/2012/09/29/063829
		struct ATAIdentifyDeviceQuery
		{
			ATA_PASS_THROUGH_EX header;
			WORD data[256];
		} RPMQuery =
		{
			{
				.Length = sizeof RPMQuery.header,
				.AtaFlags = ATA_FLAGS_DATA_IN,
				.DataTransferLength = sizeof RPMQuery.data,
				.TimeOutValue = 1,   //Timeout in seconds
				.DataBufferOffset = offsetof(ATAIdentifyDeviceQuery, data)
			}
		};
		RPMQuery.header.CurrentTaskFile[6] = 0xe; // ATA IDENTIFY DEVICE
		if (StorageQuery(IOCTL_ATA_PASS_THROUGH, RPMQuery, RPMQuery))
		{
			/*
			Index of nominal media rotation rate
			SOURCE: http://www.t13.org/documents/UploadedDocuments/docs2009/d2015r1a-ATAATAPI_Command_Set_-_2_ACS-2.pdf
			7.18.7.81 Word 217
			QUOTE: Word 217 indicates the nominal media rotation rate of the device and is defined in table:
			Value           Description
			--------------------------------
			0000h           Rate not reported
			0001h           Non-rotating media (e.g., solid state device)
			0002h-0400h     Reserved
			0401h-FFFEh     Nominal media rotation rate in rotations per minute (rpm)
			(e.g., 7 200 rpm = 1C20h)
			FFFFh           Reserved
			*/
			constexpr auto kNominalMediaRotRateWordIndex = 217u;
			const auto RPM = RPMQuery.data[kNominalMediaRotRateWordIndex];
			if (RPM == 0x0001)
				return true;
			if (RPM >= 0X0401 && RPM <= 0XFFFE)
				return false;
		}
	}

	// fail to query anything, report error and conservatively assume HDD
	throw runtime_error("unable to query anything.");
}

// based on https://stackoverflow.com/a/33359142/7155994
bool __fastcall System::DetectSSD(const filesystem::path &location, bool optimizeForSMT) noexcept
{
	try
	{
		struct CacheCell
		{
			mutable shared_mutex mtx;
			bool payload = false/*this default value (conservative HDD assumption) will be left on fails (e.g. access denied) and will be returned on successive requests*/;
		};
		static unordered_map<filesystem::path::string_type, CacheCell> cache;
		static shared_mutex mtx;
		const auto root = (location.has_root_name() ? location : filesystem::current_path()).root_name();

		const auto FetchCacheCell = [](const CacheCell &cell)
		{
			// acquire read-only access
			shared_lock RO_cell_lck(cell.mtx);
			return cell.payload;
		};

		if (optimizeForSMT)
		{
			// acquire read-only access
			shared_lock RO_global_lck(mtx);
			if (const auto cached = as_const(cache).find(root); cached != cache.cend())
			{
				RO_global_lck.unlock();	// allow global RW access to other cells while possibly waiting for long cur sell fill
				return FetchCacheCell(cached->second);
			}
		}

		// acquire read-write access
		unique_lock RW_global_lck(mtx);
		const auto [cacheCell, cacheMiss] = cache.try_emplace(root);
		if (cacheMiss)
		{
			// have to first acquire cell RW access then release global access to force RO cell accesses to wait for cell payload fill
			lock_guard RW_cell_lck(cacheCell->second.mtx);
			RW_global_lck.unlock();
			return cacheCell->second.payload = ::DetectSDD(root);
		}
		RW_global_lck.unlock();			// safe to release global access before acquiring RO cell access in 'FetchCacheCell()' (which is for cell fill awaiting only)
		return FetchCacheCell(cacheCell->second);
	}
	catch (const std::exception &error)
	{
		PrintError(error.what());
	}
	catch (const _com_error &error)
	{
		PrintError(error.ErrorMessage());
	}
	catch (...)
	{
		PrintError("unknown error.");
	}

	// fail to query anything, go conservative (assume HDD)
	return false;
}