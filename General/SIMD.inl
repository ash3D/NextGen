#pragma once

#include "SIMD.h"
#include <emmintrin.h>

#pragma region XMM
inline void Math::SIMD::XMM::Extract(float dst[4]) const noexcept
{
	_mm_storeu_ps(dst, xmm);
}

inline std::array<float, 4> Math::SIMD::XMM::Extract() const noexcept
{
	alignas(16) std::array<float, 4> result;
	_mm_store_ps(result.data(), xmm);
	return result;
}

inline auto __vectorcall Math::SIMD::operator +(XMM src) noexcept -> XMM
{
	return src;
}

inline auto __vectorcall Math::SIMD::operator -(XMM src) noexcept -> XMM
{
	return _mm_xor_ps(src.xmm, _mm_castsi128_ps(_mm_set1_epi32(0x80000000)));
}

inline auto __vectorcall Math::SIMD::operator +(XMM left, XMM right) noexcept -> XMM
{
	return left += right;
}

inline auto __vectorcall Math::SIMD::operator -(XMM left, XMM right) noexcept -> XMM
{
	return left -= right;
}

inline auto __vectorcall Math::SIMD::operator *(XMM left, XMM right) noexcept -> XMM
{
	return left *= right;
}

inline auto __vectorcall Math::SIMD::operator /(XMM left, XMM right) noexcept -> XMM
{
	return left /= right;
}

inline auto __vectorcall Math::SIMD::operator +=(XMM &left, XMM right) noexcept -> XMM &
{
	left.xmm = _mm_add_ps(left.xmm, right.xmm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator -=(XMM &left, XMM right) noexcept -> XMM &
{
	left.xmm = _mm_sub_ps(left.xmm, right.xmm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator *=(XMM &left, XMM right) noexcept -> XMM &
{
	left.xmm = _mm_mul_ps(left.xmm, right.xmm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator /=(XMM &left, XMM right) noexcept -> XMM &
{
	left.xmm = _mm_div_ps(left.xmm, right.xmm);
	return left;
}
#pragma endregion

#pragma region YMM
inline void Math::SIMD::YMM::Extract(float dst[8]) const noexcept
{
	_mm256_storeu_ps(dst, ymm);
}

inline std::array<float, 8> Math::SIMD::YMM::Extract() const noexcept
{
	alignas(16) std::array<float, 8> result;
	_mm256_store_ps(result.data(), ymm);
	return result;
}

inline auto __vectorcall Math::SIMD::operator +(YMM src) noexcept -> YMM
{
	return src;
}

inline auto __vectorcall Math::SIMD::operator -(YMM src) noexcept -> YMM
{
	return _mm256_xor_ps(src.ymm, _mm256_castsi256_ps(_mm256_set1_epi32(0x80000000)));
}

inline auto __vectorcall Math::SIMD::operator +(YMM left, YMM right) noexcept -> YMM
{
	return left += right;
}

inline auto __vectorcall Math::SIMD::operator -(YMM left, YMM right) noexcept -> YMM
{
	return left -= right;
}

inline auto __vectorcall Math::SIMD::operator *(YMM left, YMM right) noexcept -> YMM
{
	return left *= right;
}

inline auto __vectorcall Math::SIMD::operator /(YMM left, YMM right) noexcept -> YMM
{
	return left /= right;
}

inline auto __vectorcall Math::SIMD::operator +=(YMM &left, YMM right) noexcept -> YMM &
{
	left.ymm = _mm256_add_ps(left.ymm, right.ymm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator -=(YMM &left, YMM right) noexcept -> YMM &
{
	left.ymm = _mm256_sub_ps(left.ymm, right.ymm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator *=(YMM &left, YMM right) noexcept -> YMM &
{
	left.ymm = _mm256_mul_ps(left.ymm, right.ymm);
	return left;
}

inline auto __vectorcall Math::SIMD::operator /=(YMM &left, YMM right) noexcept -> YMM &
{
	left.ymm = _mm256_div_ps(left.ymm, right.ymm);
	return left;
}
#pragma endregion