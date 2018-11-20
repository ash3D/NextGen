// vector math benchmark.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#ifndef NOMINMAX //GCC already defines this
#define NOMINMAX
#endif
#include "performance.h"

#define ENABLE_TEST 1

using std::cout;
using std::endl;
namespace VectorMath = Math::VectorMath;
using namespace VectorMath::HLSL;

constexpr auto iters = 2'000'000'000;

//extern template VectorMath::HLSL::int4;
//extern template VectorMath::GLSL::vec3;
//extern template class VectorMath::CInitListItem<short>;
//extern template class VectorMath::vector<short, 5>;
//extern template class VectorMath::matrix<long, 5, 4>;
//extern template VectorMath::HLSL::int2;

int main(int argc, _TCHAR *argv[])
{
#pragma region test
#if ENABLE_TEST || _DEBUG
	VectorMath::HLSL::int4 vec4(float2x1{}, 0, 1);
	VectorMath::GLSL::vec3 vec3(vec4);
	std::common_type_t<decltype(vec4), decltype(vec3)> vec;
	vec3.x + vec3.x;
	vec3.x + 42;
	vec3.apply(floor);
	min(vec4, 0.);
	min(vec4, vec3);
	min(vec4.x, vec4.y);
	VectorMath::max(0ul, 0l);
	vec4 < vec3;
	vec4 > 0;
	vec4 + vec3;
	vec4.wwww + vec3;
	vec4.zx += vec4;
	vec4.zx += vec4.aaa;
	vec3 += vec4;
	vec4.xy = vec3.yx;
	vec4.xy = vec4.yx;
	vec4.xy += vec4.yx;
	vec4.xy = vec4.y;
	vec4.xy += vec4.y;
	vec4.xy = float1x1();
	vec4.xy += float1x1();
	vec4 = vec4.xxzz;
	vec4 = 1;
	vec4.xy = 2;
	vec4 += 2;
	vec4 + 2;
	2 + vec4;
	vec4.x = int4(3).x;
	vec4.x = vec4.y;
	double d = vec4.x;
	d += vec4;
	+vec3;
	-vec3;
	//vec4.zz += vec4.aa;
	//vec4.zz = vec4.bb;
	//vec4.xx = vec3.xx;
	//(VectorMath::vector<short, 5>(vec4));
	VectorMath::vector<short, 5> vec5(vec4, 42);
	-vec5;
	vec5, vec3;
	//VectorMath::HLSL::int1x4 m1x4;
	//m1x4._m03_m02_m01_m00 = {0, 1, 2, 3};
	//m1x4[0];
	VectorMath::matrix<long, 5, 4> m5x4;
	min(m5x4, 0);
	max(0, m5x4);
	none(m5x4);
	m5x4 == 0;
	m5x4 != +m5x4 + -m5x4;
	vec3, m5x4;
	m5x4 += m5x4;
	m5x4 += 2;
	m5x4 + 2;
	2 + m5x4;

	// '_2x2 += vec4' triggers error on '_1x1 += vec3' below on MSVC (w/o workaround) if placed exactly here (before operations with 'M')
	float2x2 _2x2{};
	vec4 += _2x2;
	_2x2 += vec4;
	vec4 + _2x2;
	_2x2 + vec4;

	float2x3 M = float4x4(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	mul(M, M);
	vec3.x = M;
	vec3.x += M;

	float1x1 _1x1;
	vec3 = _1x1;
	vec3 += _1x1;
	vec3.x == _1x1;
	_1x1 = vec3;
	_1x1 += vec3;
	_1x1 == vec3.x;

	cout << vec4.x << '\n' << vec5[4] << "\nsizeof vec3 = " << sizeof vec3 << "\nfloat3 is standard layout: " << std::boolalpha << std::is_standard_layout_v<float3> << endl;

	vec4 += int4(4);
	vec4 += int4(4).xxxx;
	vec4 += vec4;
	vec4 += vec4.xxxx;
	vec4 += 42;

	vec4.xy += int4(4);
	vec4.xy += int4(4).xxx;
	vec4.xy += vec4;
	vec4.xy += vec4.xxx;
	vec4.xy += 42;

	vec4 = int4(4);
	vec4 = int4(4).xxxx;
	vec4 = vec4;
	vec4 = vec4.xxxx;
	vec4 = 42;

	vec4.xy = int4(4);
	vec4.xy = int4(4).xxx;
	vec4.xy = vec4;
	vec4.xy = vec4.xxx;
	vec4.xy = 42;
#endif
#pragma endregion

#pragma region benchmark
#if !_DEBUG
	int2 a(argc), b((size_t)argv);
	performance::Start();
	for (auto i = 0ull; i < iters; i++)
		a += a - b;
	performance::Stop();
	performance::Out(cout << a << '\n');
	cout << endl;
#endif
#pragma endregion

	return 0;
}