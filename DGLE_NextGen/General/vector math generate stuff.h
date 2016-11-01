/**
\author		Alexey Shaydurov aka ASH
\date		01.11.2016 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#ifndef ROWS
#	if defined _MSC_VER && _MSC_VER <= 1900
#		define TRIVIAL_CTOR_FORWARD CDataContainer() = default; template<typename First, typename ...Rest> CDataContainer(const First &first, const Rest &...rest) : data(first, rest...) {}
#	else
#		define TRIVIAL_CTOR_FORWARD CDataContainer() = default; NONTRIVIAL_CTOR_FORWARD
#	endif
#		define NONTRIVIAL_CTOR_FORWARD template<typename ...Args> CDataContainer(const Args &...args) : data(args...) {}
#
#	define ROWS 0
#	include "vector math generate stuff.h"
#
#	define ROWS 1
#	include "vector math generate stuff.h"
#
#	define ROWS 2
#	include "vector math generate stuff.h"
#
#	define ROWS 3
#	include "vector math generate stuff.h"
#
#	define ROWS 4
#	include "vector math generate stuff.h"
#
#	undef ROWS
#
#	undef TRIVIAL_CTOR_FORWARD
#	undef NONTRIVIAL_CTOR_FORWARD
#elif !defined COLUMNS
#	define COLUMNS 1
#	include "vector math generate stuff.h"
#
#	define COLUMNS 2
#	include "vector math generate stuff.h"
#
#	define COLUMNS 3
#	include "vector math generate stuff.h"
#
#	define COLUMNS 4
#	include "vector math generate stuff.h"
#
#	undef COLUMNS
#elif !defined TRIVIAL_CTOR
#if !defined DISABLE_MATRIX_SWIZZLES || ROWS == 0
#	define TRIVIAL_CTOR true
#	define CTOR_FORWARD TRIVIAL_CTOR_FORWARD
#	include "vector math generate stuff.h"
#
#	define TRIVIAL_CTOR false
#	define CTOR_FORWARD NONTRIVIAL_CTOR_FORWARD
#	include "vector math generate stuff.h"
#
#	undef TRIVIAL_CTOR
#	undef CTOR_FORWARD
#endif
#pragma region generate typedefs
#ifdef _MSC_VER
#	define PASS_THROUGH(x) x
#endif
#
#	define LOOKUP_CPP_TYPE(cpp, hlsl, glsl) cpp
#	define LOOKUP_HLSL_TYPE(cpp, hlsl, glsl) hlsl
#	define LOOKUP_GLSL_TYPE(cpp, hlsl, glsl) glsl
#
#	// scalar types mapping expected at '...'
#	if ROWS == 0
#ifdef _MSC_VER
#		define CPP_CLASS(...) vector<PASS_THROUGH(LOOKUP_CPP_TYPE(__VA_ARGS__)), COLUMNS>
#else
#		define CPP_CLASS(...) vector<LOOKUP_CPP_TYPE(__VA_ARGS__), COLUMNS>
#endif
#		define HLSL_CLASS(...) CONCAT(LOOKUP_HLSL_TYPE(__VA_ARGS__), COLUMNS)
#		define GLSL_CLASS(...) CONCAT(CONCAT(LOOKUP_GLSL_TYPE(__VA_ARGS__), vec), COLUMNS)
#	else
#ifdef _MSC_VER
#		define CPP_CLASS(...) matrix<PASS_THROUGH(LOOKUP_CPP_TYPE(__VA_ARGS__)), ROWS, COLUMNS>
#else
#		define CPP_CLASS(...) matrix<LOOKUP_CPP_TYPE(__VA_ARGS__), ROWS, COLUMNS>
#endif
#		define HLSL_CLASS(...) CONCAT(CONCAT(CONCAT(LOOKUP_HLSL_TYPE(__VA_ARGS__), ROWS), x), COLUMNS)
#		define GLSL_CLASS1(...) CONCAT(CONCAT(CONCAT(CONCAT(LOOKUP_GLSL_TYPE(__VA_ARGS__), mat), ROWS), x), COLUMNS)
#		if ROWS == COLUMNS
#			define GLSL_CLASS2(...) CONCAT(CONCAT(LOOKUP_GLSL_TYPE(__VA_ARGS__), mat), ROWS)
#			define GLSL_CLASS(...) GLSL_CLASS1(__VA_ARGS__), GLSL_CLASS2(__VA_ARGS__)
#		else
#			define GLSL_CLASS GLSL_CLASS1
#		endif
#	endif
#
#	define GENERATE_TYPEDEF(graphicsClass, ...) typedef CPP_CLASS(__VA_ARGS__) graphicsClass(__VA_ARGS__);
#	define GENERATE_TYPEDEFS(graphicsClass)						\
		GENERATE_TYPEDEF(graphicsClass, bool, bool, b)			\
		GENERATE_TYPEDEF(graphicsClass, int32_t, int, i)		\
		GENERATE_TYPEDEF(graphicsClass, uint32_t, uint, ui)		\
		GENERATE_TYPEDEF(graphicsClass, int64_t, long, l)		\
		GENERATE_TYPEDEF(graphicsClass, uint64_t, ulong, ul)	\
		GENERATE_TYPEDEF(graphicsClass, float, float, )			\
		GENERATE_TYPEDEF(graphicsClass, double, double, d)

	namespace HLSL
	{
		GENERATE_TYPEDEFS(HLSL_CLASS)
	}

	namespace GLSL
	{
		GENERATE_TYPEDEFS(GLSL_CLASS)
	}

#pragma region cleanup
#ifdef _MSC_VER
#	undef PASS_THROUGH
#endif
#	undef LOOKUP_CPP_TYPE
#	undef LOOKUP_HLSL_TYPE
#	undef LOOKUP_GLSL_TYPE
#	undef CPP_CLASS
#	undef HLSL_CLASS
#	undef GLSL_CLASS1
#	undef GLSL_CLASS2
#	undef GLSL_CLASS
#	undef GENERATE_TYPEDEF
#	undef GENERATE_TYPEDEFS
#pragma endregion
#pragma endregion
#else
// specialization for graphics vectors/matrices
namespace Impl
{
	template<typename ElementType>
	class CDataContainer<ElementType, ROWS, COLUMNS, enable_if_t<is_trivially_default_constructible_v<ElementType> == TRIVIAL_CTOR>>
	{
	protected:
		// forward ctors/dtor/= to data
		CTOR_FORWARD

		CDataContainer(const CDataContainer &src) : data(src.data) {}

		CDataContainer(CDataContainer &&src) : data(move(src.data)) {}

		~CDataContainer()
		{
			data.~CData<ElementType, ROWS, COLUMNS>();
		}

		void operator =(const CDataContainer &src)
		{
			data = src.data;
		}

		void operator =(CDataContainer &&src)
		{
			data = move(src.data);
		}

	public:
		union
		{
			CData<ElementType, ROWS, COLUMNS> data;
			// gcc does not allow class definition inside anonymous union
			GENERATE_SWIZZLES(ROWS, COLUMNS)
		};
	};
}
#endif