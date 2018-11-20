#pragma once

#include <winerror.h>
#include <comdef.h>

inline void CheckHR(HRESULT hr)
{
	if (FAILED(hr))
		throw _com_error(hr);
}