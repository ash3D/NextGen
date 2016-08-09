/**
\author		Alexey Shaydurov aka ASH
\date		09.08.2016 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "CompilerCheck.h"

namespace Math::Hermite
{
	#pragma region Hermite polynomials
	template<typename T>
	extern inline T H0(T x) { return (1 + 2 * x) * (1 - x) * (1 - x); }

	template<typename T>
	extern inline T H1(T x) { return x * (1 - x) * (1 - x); }

	template<typename T>
	extern inline T H2(T x) { return -x * x * (1 - x); }

	template<typename T>
	extern inline T H3(T x) { return x * x * (3 - 2 * x); }
	#pragma endregion

	#pragma region Hermite polynomials derivatives
	template<typename T>
	extern inline T dH0dx(T x) { return 6 * x * (x - 1); }

	template<typename T>
	extern inline T dH1dx(T x) { return (1 - x) * (1 - 3 * x); }

	template<typename T>
	extern inline T dH2dx(T x) { return x * (3 * x - 2); }

	template<typename T>
	extern inline T dH3dx(T x) { return 6 * x * (1 - x); }
	#pragma endregion
}