// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#define _USE_MATH_DEFINES



// TODO: reference additional headers your program requires here
#include <cassert>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <utility>
#include <type_traits>
#include <compare>
#include <tuple>
#include <limits>
#include <memory>
#include <memory_resource>
#include <iterator>
#include <initializer_list>
#include <string>
#include <array>
#include <vector>
#include <queue>
#include <deque>
#include <list>
#include <forward_list>
#include <algorithm>
#include <numeric>
#include <execution>
#include <functional>
#include <optional>
#include <variant>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <exception>
#include <stdexcept>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "d3dx12.h"
#include "pix3.h"
#include "HRESULT.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif