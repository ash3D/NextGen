#pragma once

#include <windows.h>
#include <ostream>

namespace performance {
	using namespace std;

	DWORD tick_count;
#ifndef _M_X64
	unsigned __int64 cycles_count;
#endif

	inline void Start(void) {
		tick_count = GetTickCount();
#ifndef _M_X64
		__asm {
			rdtsc
			mov dword ptr cycles_count, eax
			mov dword ptr cycles_count + 4, edx
		}
#endif
	}

	inline void Stop(void) {
#ifndef _M_X64
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
#ifndef _M_X64
		out << '\n' << cycles_count << " CPU cycles";
#endif
	}
}