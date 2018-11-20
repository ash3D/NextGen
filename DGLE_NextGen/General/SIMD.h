/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <array>
#include <xmmintrin.h>
#include <intrin.h>

namespace Math::SIMD
{
	class XMM
	{
		__m128 xmm;

	public:
		inline XMM() noexcept : xmm(_mm_setzero_ps()) {}
		inline XMM(__m128 xmm) noexcept : xmm(xmm) {}
		inline XMM(float src) noexcept : xmm(_mm_set1_ps(src)) {}
		inline XMM(float x, float y, float z, float w) noexcept : xmm(_mm_set_ps(x, y, z, w)) {}
		inline XMM(const float src[4]) noexcept : xmm(_mm_loadu_ps(src)) {}

	public:
		inline operator __m128() const noexcept { return xmm; }

	public:
		inline void Extract(float dst[4]) const noexcept;
		inline std::array<float, 4> Extract() const noexcept;

		friend inline XMM __vectorcall operator +(XMM src) noexcept, __vectorcall operator -(XMM src) noexcept;
		friend inline XMM __vectorcall operator +(XMM left, XMM right) noexcept, __vectorcall operator -(XMM left, XMM right) noexcept, __vectorcall operator *(XMM left, XMM right) noexcept, __vectorcall operator /(XMM left, XMM right) noexcept;
		friend inline XMM &__vectorcall operator +=(XMM &left, XMM right) noexcept, &__vectorcall operator -=(XMM &left, XMM right) noexcept, &__vectorcall operator *=(XMM &left, XMM right) noexcept, &__vectorcall operator /=(XMM &left, XMM right) noexcept;
	};

	class YMM
	{
		__m256 ymm;

	public:
		inline YMM() noexcept : ymm(_mm256_setzero_ps()) {}
		inline YMM(__m256 ymm) noexcept : ymm(ymm) {}
		inline YMM(float src) noexcept : ymm(_mm256_set1_ps(src)) {}
		inline YMM(float _0, float _1, float _2, float _3, float _4, float _5, float _6, float _7) noexcept : ymm(_mm256_set_ps(_0, _1, _2, _3, _4, _5, _6, _7)) {}
		inline YMM(const float src[8]) noexcept : ymm(_mm256_loadu_ps(src)) {}

	public:
		inline operator __m256() const noexcept { return ymm; }

	public:
		inline void Extract(float dst[8]) const noexcept;
		inline std::array<float, 8> Extract() const noexcept;

		friend inline YMM __vectorcall operator +(YMM src) noexcept, __vectorcall operator -(YMM src) noexcept;
		friend inline YMM __vectorcall operator +(YMM left, YMM right) noexcept, __vectorcall operator -(YMM left, YMM right) noexcept, __vectorcall operator *(YMM left, YMM right) noexcept, __vectorcall operator /(YMM left, YMM right) noexcept;
		friend inline YMM &__vectorcall operator +=(YMM &left, YMM right) noexcept, &__vectorcall operator -=(YMM &left, YMM right) noexcept, &__vectorcall operator *=(YMM &left, YMM right) noexcept, &__vectorcall operator /=(YMM &left, YMM right) noexcept;
	};
}

#include "SIMD.inl"