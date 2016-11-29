#pragma once

#include <windows.h>
#include <ostream>

#if defined _M_X64 || __clang__
#	define DISABLE_ASM
#endif

namespace performance {
	using namespace std;

	DWORD tick_count;
#ifndef DISABLE_ASM
	unsigned __int64 cycles_count;
#endif

	inline void Start(void) {
		tick_count = GetTickCount();
#ifndef DISABLE_ASM
		__asm {
			rdtsc
			mov dword ptr cycles_count, eax
			mov dword ptr cycles_count + 4, edx
		}
#endif
	}

	inline void Stop(void) {
#ifndef DISABLE_ASM
		__asm {
			rdtsc
			sub eax, dword ptr cycles_count
			sbb edx, dword ptr cycles_count + 4
			mov dword ptr cycles_count, eax
			mov dword ptr cycles_count + 4, edx
		}
#endif
		tick_count = GetTickCount() - tick_count;
	}

	inline void Out(ostream &out) {
		out << tick_count << " ms";
#ifndef DISABLE_ASM
		out << '\n' << cycles_count << " CPU cycles";
#endif
	}
}

#undef DISABLE_ASM