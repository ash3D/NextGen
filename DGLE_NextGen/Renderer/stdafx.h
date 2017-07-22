// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers



// TODO: reference additional headers your program requires here
#include <cassert>
#include <cstdint>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <utility>
#include <type_traits>
#include <limits>
#include <memory>
#include <iterator>
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <optional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "d3dx12.h"
#include "HRESULT.h"
#undef min
#undef max
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif