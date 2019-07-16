#include <Windows.h>
//#include <ntddstor.h>
#include <ntddscsi.h>
#include <winerror.h>
#include <comdef.h>
#include "system.h"
#include <cstddef>	// for offsetof
#include <cwchar>
#include <iostream>
#include <string>
#define NOMINMAX

using namespace std;

void __fastcall System::WideIOPrologue(FILE *stream, const char accessMode[])
{
	if (fwide(stream, 0) < 0)
	{
		[[maybe_unused]] const auto reopened = freopen(NULL, accessMode, stream);
		assert(reopened);
	}
}

void __fastcall System::WideIOEpilogue(FILE *stream, const char accessMode[])
{
	// reopen again to reset orientation
	[[maybe_unused]] const auto reopened = freopen(NULL, accessMode, stream);
	assert(reopened);
}

void __fastcall System::ValidateHandle(HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE)
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
}

template<typename Char>
static void PrintError(const Char *msg)
{
	cerr << "Fail to detect drive type" << msg << ")." << endl;
}

static constexpr const char driveHandleName[] = "drive";

// based on https://stackoverflow.com/a/33359142/7155994
bool __fastcall System::DetectSSD(const filesystem::path &location) noexcept
{
	try
	{
		const auto driveName = L"\\\\.\\" + (location.has_root_name() ? location : filesystem::current_path()).root_name().native();

#ifdef _MSC_VER
		const Handle<driveHandleName> driveHandle(CreateFileW(driveName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
		const auto StorageQuery = [&driveHandle]
#else
		const auto StorageQuery = [driveHandle = Handle<driveHandleName>(CreateFileW(driveName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL))]
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
		PrintError("unknown error");
	}

	// fail to query anything, go conservative (assume HDD)
	return false;
}