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
#define _SECURE_SCL 0
#include <limits>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <memory>
#include <new>			// for bad_alloc
#include <malloc.h>		// for _get_heap_handle
#include <crtdbg.h>

#include <D3D11.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include "D3dx11effect.h"
#include "com ptr defs.h"

//#define DISABLE_MATRIX_SWIZZLES
//#include "vector math.h"

#include "C++11 stub.h"

#define ASSERT_HR(...) {const HRESULT hr = __VA_ARGS__; _ASSERT(SUCCEEDED(hr));}