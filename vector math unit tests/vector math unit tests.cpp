#include "stdafx.h"
#include "CppUnitTest.h"

//#include <iostream>
//#define DISABLE_MATRIX_SWIZZLES
//#include "vector math.h"
#ifndef NOMINMAX //GCC already defines this
#define NOMINMAX
#endif
//#include "performance.h"

using std::cout;
using std::endl;
namespace VectorMath = Math::VectorMath;
using namespace VectorMath::HLSL;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace vectormathunittests
{		
	template <typename T1, typename T2>
	void AreEqual(const T1 &expected, const T2 &actual, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Assert::AreEqual(expected, actual[i]);
		}
	}

	template <typename T1, typename T2>
	void AreEqual(const T1 &expected, const T2 &actual)
	{
		AreEqual(expected, actual, actual.dimension);
	}

	TEST_CLASS(VectorMathUnitTests)
	{
	public:

		TEST_METHOD(CtorsBasic)
		{
			VectorMath::HLSL::int4 vec4;
			VectorMath::GLSL::vec3 vec3(vec4);
			Assert::AreEqual(true, all(vec3 == vec4));

			VectorMath::vector<int, 5> vec5(vec4, 42);
			Assert::AreEqual(true, all(vec4 == vec5));
			Assert::AreEqual(42, vec5[4]);
		}

		TEST_METHOD(Assignment)
		{
			VectorMath::vector<int, 4> vec4(1);
			AreEqual(1, vec4);

			vec4 = int4(4);
			AreEqual(4, vec4);

			vec4 = int4(3).xxxx;
			AreEqual(3, vec4);

			vec4 = vec4;
			AreEqual(3, vec4);

			vec4 = vec4.xxxx;
			AreEqual(3, vec4);

			vec4 = 42;
			AreEqual(42, vec4);

			//
			vec4.xy = int4(4);
			AreEqual(4, vec4, 2);

			vec4.xy = int4(4).xxx;
			AreEqual(4, vec4, 2);

			vec4.xy = vec4;
			AreEqual(4, vec4, 2);

			vec4.xy = vec4.xxx;
			AreEqual(4, vec4, 2);

			Assert::AreEqual(42, vec4[2]);

			vec4.xy = 42;
			AreEqual(42, vec4);

			vec4.xy = int4(4);
			vec4.yz = vec4.zy;
			Assert::AreEqual(4, vec4[0]);
			Assert::AreEqual(42, vec4[1]);
			Assert::AreEqual(4, vec4[2]);
			Assert::AreEqual(42, vec4[3]);

			vec4.xyz = { 0, 1, 2 };
			Assert::AreEqual(0, vec4[0]);
			Assert::AreEqual(1, vec4[1]);
			Assert::AreEqual(2, vec4[2]);
			vec4.xyz = vec4.xxy;
			Assert::AreEqual(0, vec4[0]);
			Assert::AreEqual(0, vec4[1]);
			Assert::AreEqual(1, vec4[2]);
		}

		TEST_METHOD(AdditionAssignment)
		{
			VectorMath::vector<int, 4> vec4(1);

			vec4 += int4(4);
			AreEqual(5, vec4);

			vec4 += int4(4).xxxx;
			AreEqual(9, vec4);

			vec4 += vec4;
			AreEqual(18, vec4);

			vec4 += vec4.xxxx;
			AreEqual(36, vec4);

			vec4 += 42;
			AreEqual(78, vec4);

			//
			vec4.xy += int4(4);
			AreEqual(82, vec4, 2);

			vec4.xy += int4(4).xxx;
			AreEqual(86, vec4, 2);

			vec4.xy += vec4;
			AreEqual(172, vec4, 2);

			vec4.xy += vec4.xxx;
			AreEqual(344, vec4, 2);

			vec4.xy += 42;
			AreEqual(386, vec4, 2);

			//
			vec4.xy = vec4.zw;
			AreEqual(78, vec4);
		}

		TEST_METHOD(Addition)
		{
			VectorMath::vector<int, 4> vec4(1);
			VectorMath::vector<int, 3> vec3(2);

			AreEqual(2, vec4 + 1);
			AreEqual(3, vec4 + vec3, 3);
			AreEqual(3, vec4.xyz + vec3);
			Assert::AreEqual(3, (vec4 + vec3)[2]);

			vec4.xz = 1 + vec3.xx;
			Assert::AreEqual(3, vec4[0]);
			Assert::AreEqual(1, vec4[1]);
			Assert::AreEqual(3, vec4[2]);
			Assert::AreEqual(1, vec4[3]);

			vec4.xy = vec4.xy + vec4.yx;
			AreEqual(4, vec4, 2);
			Assert::AreEqual(3, vec4[2]);
			Assert::AreEqual(1, vec4[3]);
		}

		TEST_METHOD(Comparison)
		{
			int4 vec4(1);
			VectorMath::vector<long, 3> vec3(2);

			Assert::AreEqual(true, all(vec4 < vec3));
			//Assert::AreEqual(int4(0), min(vec4, 0));
			AreEqual(0L, min(vec4, 0));
			Assert::AreEqual(true, all(int4(0) == min(vec4, 0)));
		}

		TEST_METHOD(ScalarTest)
		{
			VectorMath::vector<int, 4> vec4(1);
			VectorMath::vector<int, 1> vec1(vec4);
			Assert::AreEqual(1, vec1[0]);

			int i = vec4.x;
			Assert::AreEqual(1, i);

			i += vec4;
			Assert::AreEqual(2, i);

			vec1 = i;
			vec4 = vec1.xxxx;
			AreEqual(2, vec4);
		}

		TEST_METHOD(Performance)
		{
			constexpr auto iters = 8ull * 1024 * 1024;

			int2 a(1), b(1);

			for (auto i = 0ull; i < iters; i++)
				a += a - b;
			AreEqual(1L, a);
		}
	};
}