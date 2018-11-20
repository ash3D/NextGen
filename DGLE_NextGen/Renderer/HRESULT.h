/**
\author		Alexey Shaydurov aka ASH
\date		04.11.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <winerror.h>
#include <comdef.h>

inline void CheckHR(HRESULT hr)
{
	if (FAILED(hr))
		throw _com_error(hr);
}