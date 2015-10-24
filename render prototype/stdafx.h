// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#include <limits>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <utility>
#include <type_traits>
#include <iterator>
#include <memory>
#include <new>			// for bad_alloc
#include <cassert>
#include <crtdbg.h>		// for _CrtSetDbgFlag
#include "Winerror.h"	// for SUCCEEDED

#include <D3D11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include "D3dx11effect.h"

//#define DISABLE_MATRIX_SWIZZLES
//#include "vector math.h"

inline void AssertHR(const HRESULT hr)
{
	assert(SUCCEEDED(hr));
}