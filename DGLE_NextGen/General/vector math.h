/**
\author		Alexey Shaydurov aka ASH
\date		05.11.2016 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma region design considerations
/*
it is safe to use const_cast if const version returns 'const &', not value, and '*this' object is not const

functions like distance now receives CSwizzle arguments
function template with same name from other namespace can have higher priority when passing vector arguments
(because no conversion required in contrast to vector->CSwizzle conversion required for function in VectorMath)
consider overloads with vector arguments to eliminate this issue

apply now have overloads similar to valarray's apply (in order to handle functions having template overloads) and some other overloads in addition
but it still does not perform as desired (vector<int, ...>.apply(floor) for example)
consider using preprocessor instead of templates or overloading each target function (floor(const vector<T, ...> &)

'&& = ?' now forbidden
'&& op= ?' also forbidden but not with MSVC (it does not follows C++ standard in that regard)

vectors/matrices are currently forbidden to be an element type for other vector/matrix

scalar args passed by const reference and temporary copies are created if WAR hazard detected
for common types (float, double) pass by value can be more efficient but many of functions are inlined so there should not be performance difference
for more complicated scalar types (complex or user defined high precision types for example) pass by const reference can be more efficient
similar approach used in STL (functions like min/max always accepts refs regardless of type complexity)
aliasing can potentially hinder compiler to perform some optimizations, consider using '__restrict' to prevent this

similar to HLSL:
1D swizzle op 1x1 matrix -> 1D swizle
1x1 matrix op 1D swizzle -> 1x1 matrix
min/max does not treat 1D swizzles / 1x1 matrices as scalars
1xN matrices converted to vectors in some situations if DISABLE_MATRIX_DECAY is non specified to 1
matrix2x3 op matrix3x2 forbidden if ENABLE_UNMATCHED_MATRICES is not specified to 1
*/
#pragma endregion

#if USE_BOOST_PREPROCESSOR && BOOST_PP_IS_ITERATING
#if BOOST_PP_ITERATION_DEPTH() == 1
#	define ROWS BOOST_PP_FRAME_ITERATION(1)
#	define COLUMNS BOOST_PP_FRAME_ITERATION(2)
#	define XYZW (x)(y)(z)(w)
#	define RGBA (r)(g)(b)(a)
//#	define MATRIX_POSITION(z, i, data)
//#	define MATRIX_ZERO_BASED (BOOST_PP_REPEAT(4, , _m))
#	define MATRIX_ZERO_BASED \
	(_m00)(_m01)(_m02)(_m03)\
	(_m10)(_m11)(_m12)(_m13)\
	(_m20)(_m21)(_m22)(_m23)\
	(_m30)(_m31)(_m32)(_m33)
#	define MATRIX_ONE_BASED \
	(_11)(_12)(_13)(_14)\
	(_21)(_22)(_23)(_24)\
	(_31)(_32)(_33)(_34)\
	(_41)(_42)(_43)(_44)
#	define GENERATE_ZERO_SEQ_PRED(d, state) BOOST_PP_LESS_D(d, BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(2, 1, state)), BOOST_PP_TUPLE_ELEM(2, 0, state))
#	define GENERATE_ZERO_SEQ_OP(d, state) (BOOST_PP_TUPLE_ELEM(2, 0, state), BOOST_PP_SEQ_PUSH_BACK(BOOST_PP_TUPLE_ELEM(2, 1, state), 0))
#	define GENERATE_ZERO_SEQ(size) BOOST_PP_TUPLE_ELEM(2, 1, BOOST_PP_WHILE(GENERATE_ZERO_SEQ_PRED, GENERATE_ZERO_SEQ_OP, (size, )))
#
#	define IS_SEQ_ZERO_OP(s, state, elem) BOOST_PP_BITAND(state, BOOST_PP_EQUAL(elem, 0))
#	define IS_SEQ_ZERO(seq) BOOST_PP_SEQ_FOLD_LEFT(IS_SEQ_ZERO_OP, 1, seq)
#
#	define IS_SEQ_UNIQUE_PRED(d, state) BOOST_PP_LESS_D(d, BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(3, 2, state)))
#	define IS_SEQ_ELEM_UNIQUE(s, state, elem) (BOOST_PP_BITAND(BOOST_PP_TUPLE_ELEM(2, 0, state), BOOST_PP_NOT_EQUAL(elem, BOOST_PP_TUPLE_ELEM(2, 1, state))), BOOST_PP_TUPLE_ELEM(2, 1, state))
#	define IS_SEQ_UNIQUE_OP(d, state) (BOOST_PP_BITAND(BOOST_PP_TUPLE_ELEM(3, 0, state), BOOST_PP_TUPLE_ELEM(2, 0, BOOST_PP_SEQ_FOLD_LEFT(IS_SEQ_ELEM_UNIQUE, (1, BOOST_PP_SEQ_ELEM(BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_TUPLE_ELEM(3, 2, state))), BOOST_PP_SEQ_FIRST_N(BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_TUPLE_ELEM(3, 2, state))))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(3, 1, state)), BOOST_PP_TUPLE_ELEM(3, 2, state))
#	define IS_SEQ_UNIQUE(seq) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(IS_SEQ_UNIQUE_PRED, IS_SEQ_UNIQUE_OP, (1, 1, seq)))
#
#	define INC_MODULO(elem, modulo1, modulo2) BOOST_PP_MOD(BOOST_PP_IF(BOOST_PP_MOD(BOOST_PP_MOD(BOOST_PP_INC(elem), 4), modulo1), BOOST_PP_INC(elem), BOOST_PP_SUB(BOOST_PP_ADD(elem, 4), BOOST_PP_MOD(BOOST_PP_ADD(elem, 4), 4))), BOOST_PP_MUL(modulo2, 4))
#	// ?: BOOST_PP_BITAND does not work in VS 2010
#	define INC_SEQ_PRED(d, state) BOOST_PP_AND(BOOST_PP_LESS_D(d, BOOST_PP_TUPLE_ELEM(4, 3, state), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(4, 0, state))), BOOST_PP_BITOR(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(4, 3, state), 0), BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(BOOST_PP_DEC(BOOST_PP_TUPLE_ELEM(4, 3, state)), BOOST_PP_TUPLE_ELEM(4, 0, state)), 0)))
#	define INC_SEQ_OP(d, state) (BOOST_PP_SEQ_REPLACE(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 3, state), INC_MODULO(BOOST_PP_SEQ_ELEM(BOOST_PP_TUPLE_ELEM(4, 3, state), BOOST_PP_TUPLE_ELEM(4, 0, state)), BOOST_PP_TUPLE_ELEM(4, 1, state), BOOST_PP_TUPLE_ELEM(4, 2, state))), BOOST_PP_TUPLE_ELEM(4, 1, state), BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 3, state)))
#	//																															  cur idx
#	//																																 ^
#	//																																 |
#	define INC_SEQ(seq, modulo1, modulo2) BOOST_PP_TUPLE_ELEM(4, 0, BOOST_PP_WHILE(INC_SEQ_PRED, INC_SEQ_OP, (seq, modulo1, modulo2, 0)))
#
#	define IDX_2_SYMBOL(s, NAMING_SET, i) BOOST_PP_SEQ_ELEM(i, NAMING_SET)
#
#	define SWIZZLES_INNER_LOOP_PRED(r, state) BOOST_PP_BITAND(BOOST_PP_NOT(IS_SEQ_ZERO(BOOST_PP_TUPLE_ELEM(4, 1, state))), BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 3, state)))
#	define SWIZZLES_INNER_LOOP_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), INC_SEQ(BOOST_PP_TUPLE_ELEM(4, 1, state), COLUMNS, BOOST_PP_MAX(ROWS, 1)), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define TRANSFORM_SWIZZLE(NAMING_SET, swizzle_seq) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, NAMING_SET, swizzle_seq))
#	define SWIZZLES_INNER_LOOP_MACRO(r, state) BOOST_PP_APPLY(BOOST_PP_TUPLE_ELEM(4, 0, state))(BOOST_PP_TUPLE_ELEM(4, 1, state))
#	define SWIZZLES_INTERMIDIATE_LOOP_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_INTERMIDIATE_LOOP_MACRO(r, state) BOOST_PP_FOR_##r((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, SWIZZLES_INNER_LOOP_MACRO)
#	define SWIZZLES_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_MACRO(r, state) BOOST_PP_FOR_##r((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, SWIZZLES_INTERMIDIATE_LOOP_MACRO)
#	define SWIZZLES(z, i, callback)											\
		SWIZZLES_INNER_LOOP_MACRO(z, (callback, GENERATE_ZERO_SEQ(i), , ))	\
		/*																		 cur iteration		max iterations
																					   ^				  ^
																					   |_______	   _______|
																							   |  |	*/\
		BOOST_PP_FOR((callback, INC_SEQ(GENERATE_ZERO_SEQ(i), COLUMNS, BOOST_PP_MAX(ROWS, 1)), 0, 64), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_OP, SWIZZLES_MACRO)
#	define GENERATE_SWIZZLES(callback) BOOST_PP_REPEAT_FROM_TO(1, 5, SWIZZLES, callback)
#
#	define SWIZZLE_SEQ_2_DESC(seq) CSwizzleDesc<BOOST_PP_TUPLE_REM_CTOR(BOOST_PP_SEQ_SIZE(seq), BOOST_PP_SEQ_TO_TUPLE(seq))>

#	define BOOST_PP_ITERATION_LIMITS (1, 4)
#	define BOOST_PP_FILENAME_2 "vector math.h"
#	include BOOST_PP_ITERATE()
#else
	/*
	gcc does not allow explicit specialization in class scope => CSwizzle can not be inside CDataContainer
	ElementType needed in order to compiler can deduce template args for operators
	*/
#if !defined DISABLE_MATRIX_SWIZZLES || ROWS == 0
	namespace Impl
	{
#		define NAMING_SET_1 BOOST_PP_IF(ROWS, MATRIX_ZERO_BASED, XYZW)
#		define NAMING_SET_2 BOOST_PP_IF(ROWS, MATRIX_ONE_BASED, RGBA)

#		define SWIZZLE_OBJECT(swizzle_seq)											\
			CSwizzle<ElementType, ROWS, COLUMNS, SWIZZLE_SEQ_2_DESC(swizzle_seq)>	\
				TRANSFORM_SWIZZLE(NAMING_SET_1, swizzle_seq),						\
				TRANSFORM_SWIZZLE(NAMING_SET_2, swizzle_seq);

#if defined _MSC_VER && _MSC_VER <= 1900
#		define TRIVIAL_CTOR_FORWARD CDataContainer() = default; template<typename First, typename ...Rest> CDataContainer(const First &first, const Rest &...rest) : data(first, rest...) {}
#else
#		define TRIVIAL_CTOR_FORWARD CDataContainer() = default; NONTRIVIAL_CTOR_FORWARD
#endif
#		define NONTRIVIAL_CTOR_FORWARD template<typename ...Args> CDataContainer(const Args &...args) : data(args...) {}

		// specialization for graphics vectors/matrices
#		define DATA_CONTAINER_SPECIALIZATION(trivialCtor)																					\
			template<typename ElementType>																									\
			class CDataContainer<ElementType, ROWS, COLUMNS, enable_if_t<is_trivially_default_constructible_v<ElementType> == trivialCtor>>	\
			{																																\
			protected:																														\
				/*forward ctors/dtor/= to data*/																							\
				BOOST_PP_REMOVE_PARENS(BOOST_PP_IIF(trivialCtor, (TRIVIAL_CTOR_FORWARD), (NONTRIVIAL_CTOR_FORWARD)))						\
																																			\
				CDataContainer(const CDataContainer &src) : data(src.data) {}																\
																																			\
				CDataContainer(CDataContainer &&src) : data(move(src.data)) {}																\
																																			\
				~CDataContainer()																											\
				{																															\
					data.~CData<ElementType, ROWS, COLUMNS>();																				\
				}																															\
																																			\
				void operator =(const CDataContainer &src)																					\
				{																															\
					data = src.data;																										\
				}																															\
																																			\
				void operator =(CDataContainer &&src)																						\
				{																															\
					data = move(src.data);																									\
				}																															\
																																			\
			public:																															\
				union																														\
				{																															\
					CData<ElementType, ROWS, COLUMNS> data;																					\
					/*gcc does not allow class definition inside anonymous union*/															\
					GENERATE_SWIZZLES((SWIZZLE_OBJECT))																						\
				};																															\
			};

		DATA_CONTAINER_SPECIALIZATION(0)
		DATA_CONTAINER_SPECIALIZATION(1)

#		undef TRIVIAL_CTOR_FORWARD
#		undef NONTRIVIAL_CTOR_FORWARD
#		undef DATA_CONTAINER_SPECIALIZATION

#		undef NAMING_SET_1
#		undef NAMING_SET_2
#		undef SWIZZLE_OBJECT
	}
#endif

#	pragma region generate typedefs
		// tuple: (C++, HLSL, GLSL)
#		define SCALAR_TYPES_MAPPINGS					\
			((bool, bool, b))							\
			((int32_t, int, i))((uint32_t, uint, ui))	\
			((int64_t, long, l))((uint64_t, ulong, ul))	\
			((float, float, ))((double, double, d))

#		define CPP_SCALAR_TYPE(scalar_types_mapping) BOOST_PP_TUPLE_ELEM(3, 0, scalar_types_mapping)
#		define HLSL_SCALAR_TYPE(scalar_types_mapping) BOOST_PP_TUPLE_ELEM(3, 1, scalar_types_mapping)
#		define GLSL_SCALAR_TYPE(scalar_types_mapping) BOOST_PP_TUPLE_ELEM(3, 2, scalar_types_mapping)

#		if ROWS == 0
#			define CPP_CLASS(scalar_types_mapping) vector<CPP_SCALAR_TYPE(scalar_types_mapping), COLUMNS>
#			define HLSL_CLASS(scalar_types_mapping) BOOST_PP_CAT(HLSL_SCALAR_TYPE(scalar_types_mapping), COLUMNS)
#			define GLSL_CLASS(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(GLSL_SCALAR_TYPE(scalar_types_mapping), vec), COLUMNS)
#		else
#			define CPP_CLASS(scalar_types_mapping) matrix<CPP_SCALAR_TYPE(scalar_types_mapping), ROWS, COLUMNS>
#			define HLSL_CLASS(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(HLSL_SCALAR_TYPE(scalar_types_mapping), ROWS), x), COLUMNS)
#			define GLSL_CLASS1(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(GLSL_SCALAR_TYPE(scalar_types_mapping), mat), ROWS), x), COLUMNS)
#			if ROWS == COLUMNS
#				define GLSL_CLASS2(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(GLSL_SCALAR_TYPE(scalar_types_mapping), mat), ROWS)
#				define GLSL_CLASS(scalar_types_mapping) GLSL_CLASS1(scalar_types_mapping), GLSL_CLASS2(scalar_types_mapping)
#			else
#				define GLSL_CLASS(scalar_types_mapping) GLSL_CLASS1(scalar_types_mapping)
#			endif
#		endif

#		define GENERATE_TYPEDEF(r, graphics_class, elem) typedef CPP_CLASS(elem) graphics_class(elem);

		namespace HLSL
		{
			BOOST_PP_SEQ_FOR_EACH(GENERATE_TYPEDEF, HLSL_CLASS, SCALAR_TYPES_MAPPINGS)
		}

		namespace GLSL
		{
			BOOST_PP_SEQ_FOR_EACH(GENERATE_TYPEDEF, GLSL_CLASS, SCALAR_TYPES_MAPPINGS)
		}

#		undef SCALAR_TYPES_MAPPINGS
#		undef CPP_SCALAR_TYPE
#		undef HLSL_SCALAR_TYPE
#		undef GLSL_SCALAR_TYPE
#		undef CPP_CLASS
#		undef HLSL_CLASS
#		undef GLSL_CLASS1
#		undef GLSL_CLASS2
#		undef GLSL_CLASS
#		undef GENERATE_TYPEDEF
#	pragma endregion
#endif

#if BOOST_PP_ITERATION_DEPTH() == 1
#	undef ROWS
#	undef COLUMNS
#	undef XYZW
#	undef RGBA
#	undef MATRIX_ZERO_BASED
#	undef MATRIX_ONE_BASED
#	undef GENERATE_ZERO_SEQ_PRED
#	undef GENERATE_ZERO_SEQ_OP
#	undef GENERATE_ZERO_SEQ
#	undef IS_SEQ_ZERO_OP
#	undef IS_SEQ_ZERO
#	undef IS_SEQ_UNIQUE_PRED
#	undef IS_SEQ_ELEM_UNIQUE
#	undef IS_SEQ_UNIQUE_OP
#	undef IS_SEQ_UNIQUE
#	undef INC_MODULO
#	undef INC_SEQ_PRED
#	undef INC_SEQ_OP
#	undef INC_SEQ
#	undef IDX_2_SYMBOL
#	undef SWIZZLES_INNER_LOOP_PRED
#	undef SWIZZLES_INNER_LOOP_OP
#	undef TRANSFORM_SWIZZLE
#	undef SWIZZLES_INNER_LOOP_MACRO
#	undef SWIZZLES_INTERMIDIATE_LOOP_OP
#	undef SWIZZLES_INTERMIDIATE_LOOP_MACRO
#	undef SWIZZLES_OP
#	undef SWIZZLES_MACRO
#	undef SWIZZLES
#	undef GENERATE_SWIZZLES
#	undef SWIZZLE_SEQ_2_DESC
#endif
#else
#if !USE_BOOST_PREPROCESSOR
#	pragma once
#endif
#	ifndef __VECTOR_MATH_H__
#	define __VECTOR_MATH_H__

#	if !defined  __clang__  && defined _MSC_FULL_VER && _MSC_FULL_VER < 190024210
#	error Old MSVC compiler version. Visual Studio 2015 Update 3 or later required.
#	elif defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ >= 4 && __GNUC_MINOR__ < 7))
#	error Old GCC compiler version. GCC 4.7 or later required.	// need to be clarified
#	endif

#	pragma warning(push)
#	pragma warning(disable: 4003)

// ugly workaround for operations like 'vec4.xy += int4(4).xxx;'
#if defined _MSC_VER && _MSC_VER <= 1900
#	define MSVC_NAMESPACE_WORKAROUND
#endif

// it seems that MSVC violates C++ standard regarding temp objects lifetime with initializer lists\
further investigations needed, including other compilers

#if !defined INIT_LIST_ITEM_COPY && defined _MSC_VER
#define INIT_LIST_ITEM_COPY 1
#endif

#ifdef _MSC_VER
#	define EBCO __declspec(empty_bases)
#else
#	define EBCO
#endif

#	define INIT_LIST_ITEM_OVERFLOW_MSG "too large item encountered in sequence"

#	include <cassert>
#	include <cstdint>
#	include <utility>
#	include <type_traits>
#	include <functional>
#	include <limits>
#	include <iterator>
#	include <algorithm>
#	include <initializer_list>
#if USE_BOOST_PREPROCESSOR
#	include <boost/preprocessor/cat.hpp>
#	include <boost/preprocessor/facilities/apply.hpp>
#	include <boost/preprocessor/iteration/iterate.hpp>
//#	include <boost/preprocessor/repetition/repeat.hpp>
#	include <boost/preprocessor/repetition/repeat_from_to.hpp>
#	include <boost/preprocessor/repetition/for.hpp>
#	include <boost/preprocessor/control/while.hpp>
#	include <boost/preprocessor/control/if.hpp>
#	include <boost/preprocessor/control/iif.hpp>
#	include <boost/preprocessor/arithmetic/inc.hpp>
#	include <boost/preprocessor/arithmetic/dec.hpp>
#	include <boost/preprocessor/arithmetic/add.hpp>
#	include <boost/preprocessor/arithmetic/sub.hpp>
#	include <boost/preprocessor/arithmetic/mul.hpp>
#	include <boost/preprocessor/arithmetic/mod.hpp>
#	include <boost/preprocessor/comparison/equal.hpp>
#	include <boost/preprocessor/comparison/not_equal.hpp>
#	include <boost/preprocessor/comparison/less.hpp>
#	include <boost/preprocessor/logical/and.hpp>
#	include <boost/preprocessor/logical/bitand.hpp>
#	include <boost/preprocessor/logical/bitor.hpp>
#	include <boost/preprocessor/logical/not.hpp>
#	include <boost/preprocessor/selection/max.hpp>
#	include <boost/preprocessor/tuple/elem.hpp>
#	include <boost/preprocessor/tuple/to_seq.hpp>
#	include <boost/preprocessor/tuple/rem.hpp>
#	include <boost/preprocessor/seq/seq.hpp>
#	include <boost/preprocessor/seq/size.hpp>
#	include <boost/preprocessor/seq/elem.hpp>
#	include <boost/preprocessor/seq/first_n.hpp>
#	include <boost/preprocessor/seq/push_back.hpp>
#	include <boost/preprocessor/seq/pop_front.hpp>
#	include <boost/preprocessor/seq/replace.hpp>
#	include <boost/preprocessor/seq/fold_left.hpp>
#	include <boost/preprocessor/seq/transform.hpp>
#	include <boost/preprocessor/seq/for_each.hpp>
#	include <boost/preprocessor/seq/cat.hpp>
#	include <boost/preprocessor/seq/to_tuple.hpp>
#	include <boost/preprocessor/punctuation/remove_parens.hpp>
#endif
#if USE_BOOST_MPL
#	include <boost/mpl/apply.hpp>
#	include <boost/mpl/placeholders.hpp>
#	include <boost/mpl/lambda.hpp>
#	include <boost/mpl/bind.hpp>
#	include <boost/mpl/vector_c.hpp>
#	include <boost/mpl/range_c.hpp>
#	include <boost/mpl/front.hpp>
#	include <boost/mpl/size.hpp>
#	include <boost/mpl/min_max.hpp>
#	include <boost/mpl/sort.hpp>
#	include <boost/mpl/unique.hpp>
#	include <boost/mpl/comparison.hpp>
#	include <boost/mpl/or.hpp>
#	include <boost/mpl/bitor.hpp>
#	include <boost/mpl/shift_left.hpp>
#	include <boost/mpl/times.hpp>
#	include <boost/mpl/divides.hpp>
#	include <boost/mpl/modulus.hpp>
#	include <boost/mpl/integral_c.hpp>
#	include <boost/mpl/iterator_range.hpp>
#	include <boost/mpl/deref.hpp>
#	include <boost/mpl/begin.hpp>
#	include <boost/mpl/advance.hpp>
#	include <boost/mpl/distance.hpp>
#	include <boost/mpl/iter_fold.hpp>
#	include <boost/mpl/find.hpp>
#	include <boost/mpl/transform_view.hpp>
#endif

#if USE_BOOST_PREPROCESSOR
//#	define GET_SWIZZLE_ELEMENT(vectorDimension, idx, cv) (reinterpret_cast<cv CData<ElementType, vectorDimension> &>(*this).data[(idx)])
//#	define GET_SWIZZLE_ELEMENT_PACKED(vectorDimension, packedSwizzle, idx, cv) (GET_SWIZZLE_ELEMENT(vectorDimension, packedSwizzle >> ((idx) << 1) & 3u, cv))
#endif

	namespace Math::VectorMath
	{
#if USE_BOOST_PREPROCESSOR
#		define ARITHMETIC_OPS (+)(-)(*)(/)
#		define REL_OPS (==)(!=)(<)(<=)(>)(>=)
#		define GENERATE_OPERATOR(r, operatorTemplate, op) operatorTemplate(op)
#		define GENERATE_OPERATORS(operatorTemplate, ops) BOOST_PP_SEQ_FOR_EACH(GENERATE_OPERATOR, operatorTemplate, ops)
#		define GENERATE_ARITHMETIC_OPERATORS(operatorTemplate) GENERATE_OPERATORS(operatorTemplate, ARITHMETIC_OPS)
#		define GENERATE_REL_OPERATORS(operatorTemplate) GENERATE_OPERATORS(operatorTemplate, REL_OPS)
#else
#		define GENERATE_ARITHMETIC_OPERATORS(operatorTemplate)	\
			operatorTemplate(+)									\
			operatorTemplate(-)									\
			operatorTemplate(*)									\
			operatorTemplate(/)

#		define GENERATE_REL_OPERATORS(operatorTemplate)			\
			operatorTemplate(==)								\
			operatorTemplate(!=)								\
			operatorTemplate(<)									\
			operatorTemplate(<=)								\
			operatorTemplate(>)									\
			operatorTemplate(>=)
#endif

		namespace Impl
		{
#if USE_BOOST_MPL
			namespace mpl = boost::mpl;
			using namespace mpl::placeholders;
#endif

			using std::declval;
			using std::integer_sequence;
			using std::index_sequence;
			using std::make_integer_sequence;
			using std::make_index_sequence;
			using std::integral_constant;
			using std::bool_constant;
			using std::false_type;
			using std::true_type;
			using std::is_union_v;
			using std::is_class_v;
			using std::is_arithmetic_v;
			using std::is_const_v;
			using std::is_trivially_default_constructible_v;
			using std::is_same;
			using std::is_same_v;
			using std::is_convertible;
			using std::remove_cv_t;
			using std::remove_const_t;
			using std::remove_volatile_t;
			using std::remove_reference_t;
			using std::decay_t;
			using std::enable_if;
			using std::enable_if_t;
			using std::conditional_t;
			using std::result_of_t;
			using std::numeric_limits;
			using std::iterator;
			using std::forward_iterator_tag;
			using std::initializer_list;

			enum class TagName
			{
				Swizzle,
				Vector,
				Matrix,
			};

			template<TagName name, bool scalar>
			class Tag
			{
			protected:
				Tag() = default;
				Tag(const Tag &) = default;
#ifdef __GNUC__
				Tag &operator =(const Tag &) = default;
#else
				Tag &operator =(const Tag &) & = default;
#endif
				~Tag() = default;
			};

			namespace CheckTag
			{
				template<template<typename, typename> class F, typename Var, typename Fixed>
				using Iterate = bool_constant<F<Var &, Fixed>::value || F<const Var &, Fixed>::value || F<Var &&, Fixed>::value || F<const Var &&, Fixed>::value>;

				template<typename FixedTag, typename VarSrc>
				using IterateSrc = Iterate<is_convertible, VarSrc, FixedTag>;

				template<typename Src, TagName name, bool scalar>
				static constexpr bool Check = Iterate<IterateSrc, Tag<name, scalar>, Src>::value;

				template<typename Src, bool scalar>
				static constexpr bool CheckScalar = Check<Src, TagName::Swizzle, scalar> || Check<Src, TagName::Vector, scalar> || Check<Src, TagName::Matrix, scalar>;
			};

			template<typename Src>
			static constexpr bool Is1D = CheckTag::Check<Src, TagName::Swizzle, true> || CheckTag::Check<Src, TagName::Vector, true>;

			template<typename Src>
			static constexpr bool Is1x1 = CheckTag::Check<Src, TagName::Matrix, true>;

			template<typename Src>
			static constexpr bool IsPackedScalar = Is1D<Src> || Is1x1<Src>;

			template<typename Src>
			static constexpr bool IsScalar = !CheckTag::Check<Src, TagName::Swizzle, false> && !CheckTag::Check<Src, TagName::Vector, false> && !CheckTag::Check<Src, TagName::Matrix, false>;

			template<typename Src>
			static constexpr bool IsPureScalar = IsScalar<Src> && !IsPackedScalar<Src>;

			template<typename ElementType>
			static constexpr bool IsElementTypeValid = (is_union_v<ElementType> || is_class_v<ElementType> || is_arithmetic_v<ElementType>) && !is_const_v<ElementType> && IsPureScalar<ElementType>;

#if USE_BOOST_MPL
			template<class Seq, class Iter>
			using DistanceFromBegin = typename mpl::distance<typename mpl::begin<Seq>::type, Iter>::type;

			template<class CSwizzleVector>
			class CPackedSwizzle
			{
			public:
				static constexpr unsigned int dimension = mpl::size<CSwizzleVector>::type::value;

			private:
				static constexpr unsigned int bitsRequired = dimension * 4;
				using TPackedSwizzle =
					conditional_t<bitsRequired <= numeric_limits<unsigned int>::digits, unsigned int,
					conditional_t<bitsRequired <= numeric_limits<unsigned long int>::digits, unsigned long int,
					unsigned long long int>>;

			private:
				template<class Iter>
				struct PackSwizzleElement : mpl::shift_left<typename mpl::deref<Iter>::type, typename mpl::times<DistanceFromBegin<CSwizzleVector, Iter>, mpl::integral_c<unsigned int, 4u>>> {};
				typedef mpl::iter_fold<CSwizzleVector, mpl::integral_c<TPackedSwizzle, 0u>, mpl::bitor_<_1, PackSwizzleElement<_2>>> PackedSwizzle;

			private:
				static constexpr TPackedSwizzle packedSwizzle = PackedSwizzle::type::value;

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return packedSwizzle >> idx * 4u & 0xFu;
				}
			};

			template<unsigned int ...swizzleSeq>
			class CSwizzleDesc : public CPackedSwizzle<mpl::vector_c<unsigned int, swizzleSeq...>>
			{
			public:
				typedef mpl::vector_c<unsigned int, swizzleSeq...> CSwizzleVector;

			private:
				typedef typename mpl::sort<CSwizzleVector>::type CSortedSwizzleVector;
				typedef typename mpl::unique<CSortedSwizzleVector, is_same<_, _>>::type CUniqueSwizzleVector;
				typedef typename mpl::equal_to<mpl::size<CUniqueSwizzleVector>, mpl::size<CSwizzleVector>> IsWriteMaskValid;

			public:
				static constexpr typename IsWriteMaskValid::value_type isWriteMaskValid = IsWriteMaskValid::value;
			};

			template<unsigned int rows, unsigned int columns>
			class CSequencingSwizzleVector
			{
				typedef mpl::range_c<unsigned int, 0, rows * columns> LinearSequence;
				typedef mpl::integral_c<unsigned int, columns> Columns;
				typedef mpl::bitor_<mpl::modulus<_, Columns>, mpl::shift_left<mpl::divides<_, Columns>, mpl::integral_c<unsigned int, 2u>>> Xform;

			public:
				// CSwizzleVector here not actually vector but rather range
				typedef mpl::transform_view<LinearSequence, Xform> CSwizzleVector;
			};

			template<unsigned int rows, unsigned int columns>
			struct CSequencingSwizzleDesc : CSequencingSwizzleVector<rows, columns>, CPackedSwizzle<typename CSequencingSwizzleVector<rows, columns>::CSwizzleVector>
			{
				static constexpr bool isWriteMaskValid = true;
			};
#else
			template<unsigned int ...swizzleSeq>
			class CPackedSwizzle
			{
			public:
				static constexpr unsigned int dimension = sizeof...(swizzleSeq);

			private:
				static constexpr unsigned int bitsRequired = dimension * 4;
#if defined _MSC_VER && _MSC_VER <= 1900
				using TPackedSwizzle = unsigned long long int;
#else
				// ICE on VS 2015
				using TPackedSwizzle =
					conditional_t<bitsRequired <= numeric_limits<unsigned int>::digits, unsigned int,
					conditional_t<bitsRequired <= numeric_limits<unsigned long int>::digits, unsigned long int,
					unsigned long long int>>;
#endif

			private:
#if defined _MSC_VER && _MSC_VER <= 1900
				template<TPackedSwizzle shiftedHead, TPackedSwizzle ...shiftedTail>
				static constexpr TPackedSwizzle Packer = shiftedHead | Packer<shiftedTail...>;

				// terminator
				template<TPackedSwizzle shiftedLast>
				static constexpr TPackedSwizzle Packer<shiftedLast> = shiftedLast;
#else
				template<TPackedSwizzle ...shiftedSeq>
				static constexpr TPackedSwizzle Packer = (shiftedSeq | ...);
#endif

#if defined _MSC_VER && _MSC_VER <= 1900
				template<unsigned int shift, TPackedSwizzle swizzleHead, TPackedSwizzle ...swizzleTail>
				static constexpr TPackedSwizzle MakePackedSwizzle = swizzleHead << shift | MakePackedSwizzle<shift + 4u, swizzleTail...>;

				template<unsigned int shift, TPackedSwizzle swizzleLast>
				static constexpr TPackedSwizzle MakePackedSwizzle<shift, swizzleLast> = swizzleLast << shift;
#else
				template<typename IdxSeq>
				static constexpr TPackedSwizzle MakePackedSwizzle = 0;

				template<unsigned int ...idxSeq>
				static constexpr TPackedSwizzle MakePackedSwizzle<integer_sequence<unsigned int, idxSeq...>> = Packer<TPackedSwizzle(swizzleSeq) << idxSeq * 4u ...>;
#endif

			private:
#if defined _MSC_VER && _MSC_VER <= 1900
				static constexpr TPackedSwizzle packedSwizzle = MakePackedSwizzle<0u, swizzleSeq...>;
#else
				static constexpr TPackedSwizzle packedSwizzle = MakePackedSwizzle<make_integer_sequence<unsigned int, dimension>>;
#endif

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return packedSwizzle >> idx * 4u & 0xFu;
				}

			public:
				typedef CPackedSwizzle PackedSwizzle;
			};

			template<unsigned int ...swizzleSeq>
			class CSwizzleDesc : public CPackedSwizzle<swizzleSeq...>
			{
#if defined _MSC_VER && _MSC_VER <= 1900
				static constexpr bool IsWriteMaskValid(unsigned i = 0, unsigned j = 1)
				{
					return i < dimension ? j < dimension ? FetchIdx(i) != FetchIdx(j) && IsWriteMaskValid(i, j + 1) : IsWriteMaskValid(i + 1, i + 2) : true;
				}
#else
				static constexpr bool IsWriteMaskValid()
				{
					for (unsigned i = 0; i < dimension; i++)
						for (unsigned j = i + 1; j < dimension; j++)
							if (FetchIdx(i) == FetchIdx(j))
								return false;
					return true;
				}
#endif

			public:
				static constexpr bool isWriteMaskValid = IsWriteMaskValid();
			};

			template<unsigned int rows, unsigned int columns>
			class MakeSequencingPackedSwizzle
			{
				typedef make_integer_sequence<unsigned int, rows * columns> LinearSequence;

				template<typename LinearSequence>
				struct PackedSwizzle;

				template<unsigned int ...seq>
				struct PackedSwizzle<integer_sequence<unsigned int, seq...>>
				{
					typedef CPackedSwizzle<seq % columns | seq / columns << 2u ...> type;
				};

				public:
					typedef typename PackedSwizzle<LinearSequence>::type type;
			};

			template<unsigned int rows, unsigned int columns>
			struct CSequencingSwizzleDesc : MakeSequencingPackedSwizzle<rows, columns>::type
			{
				static constexpr bool isWriteMaskValid = true;
			};
#endif

			template<unsigned int vectorDimension>
			using CVectorSwizzleDesc = CSequencingSwizzleDesc<1, vectorDimension>;

#if USE_BOOST_MPL
			// contains CSwizzleVector only, has no dimension nor isWriteMaskValid (it is trivial to evaluate but it is not needed for WAR hazard detector)
			template<unsigned int scalarSwizzle, unsigned int dimension>
			class CBroadcastScalarSwizzleDesc
			{
				template<typename IntSeq>
				struct IntSeq2SwizzleVec;

				template<unsigned int ...seq>
				struct IntSeq2SwizzleVec<integer_sequence<unsigned int, seq...>>
				{
#if defined _MSC_VER && _MSC_VER <= 1900
					typedef mpl::vector_c<unsigned int, seq * 0 + scalarSwizzle ...> type;
#else
					typedef mpl::vector_c<unsigned int, (seq, scalarSwizzle)...> type;
#endif
				};

			public:
				typedef typename IntSeq2SwizzleVec<make_integer_sequence<unsigned int, dimension>>::type CSwizzleVector;
			};
#else
			template<unsigned int scalarSwizzle, unsigned int dimension>
			class MakeBroadcastScalarSwizzleDesc
			{
				template<typename Rep>
				struct PackedSwizzle;

				template<unsigned int ...rep>
				struct PackedSwizzle<integer_sequence<unsigned int, rep...>>
				{
#if defined _MSC_VER && _MSC_VER <= 1900
					typedef CPackedSwizzle<rep * 0 + scalarSwizzle ...> type;
#else
					typedef CPackedSwizzle<(rep, scalarSwizzle)...> type;
#endif
				};

			public:
				typedef typename PackedSwizzle<make_integer_sequence<unsigned int, dimension>>::type type;
			};

			// contains CPackedSwizzle only, has no isWriteMaskValid (it is trivial to evaluate but it is not needed for WAR hazard detector)
			template<unsigned int scalarSwizzle, unsigned int dimension>
			using CBroadcastScalarSwizzleDesc = typename MakeBroadcastScalarSwizzleDesc<scalarSwizzle, dimension>::type;
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleDataAccess;

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, typename isWriteMaskValid = bool_constant<SwizzleDesc::isWriteMaskValid>>
			class CSwizzleAssign;

#if OPTIMIZE_FOR_PCH
			template<class SwizzleDesc>
			using TouchSwizzleDesc = std::void_t<bool_constant<SwizzleDesc::isWriteMaskValid>>;
#else
			template<class SwizzleDesc>
			using TouchSwizzleDesc = void;
#endif

#ifdef MSVC_NAMESPACE_WORKAROUND
		}
#endif
			// rows = 0 for vectors
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, typename = Impl::TouchSwizzleDesc<SwizzleDesc>>
			class CSwizzle;
#ifdef MSVC_NAMESPACE_WORKAROUND
		namespace Impl
		{
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>>
			using CSwizzle = VectorMath::CSwizzle<ElementType, rows, columns, SwizzleDesc>;
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			using CSequencingSwizzle = CSwizzle<ElementType, rows, columns, CSequencingSwizzleDesc<rows, columns>>;

			template<typename ElementType, unsigned int rows, unsigned int columns, typename = void>
			class CDataContainer;
		}

		template<typename ElementType, unsigned int dimension>
		class EBCO vector;

		template<typename ElementType, unsigned int rows, unsigned int columns>
		class EBCO matrix;

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool all(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool any(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool none(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool all(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool any(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool none(const matrix<ElementType, rows, columns> &src);

		namespace Impl
		{
			template<typename SrcType>
			inline enable_if_t<IsPureScalar<SrcType>, const SrcType &> ExtractScalar(const SrcType &src) noexcept
			{
				return src;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline const ElementType &ExtractScalar(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src) noexcept
			{
				return src;
			}

#if defined _MSC_VER && _MSC_VER <= 1900
			// workaround for lack of expression SFINAE support in VS 2015 for MatrixOpScalarResult/ScalarOpMatrixResult
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline const ElementType &ExtractScalar(const matrix<ElementType, rows, columns> &src) noexcept
#else
			template<typename ElementType>
			inline const ElementType &ExtractScalar(const matrix<ElementType, 1, 1> &src) noexcept
#endif
			{
				return src;
			}

#			pragma region WAR hazard
#				pragma region detector
#if USE_BOOST_MPL
					template<class CDstSwizzleVector, class CSrcSwizzleVector, bool assign>
					class SwizzleWARHazardDetectHelper
					{
						// cut CSrcSwizzleVector off
						typedef typename mpl::min<typename mpl::size<CSrcSwizzleVector>::type, typename mpl::size<CDstSwizzleVector>::type>::type MinSwizzleSize;
						typedef typename mpl::begin<CSrcSwizzleVector>::type SrcSwizzleBegin;
						typedef typename mpl::iterator_range<SrcSwizzleBegin, typename mpl::advance<SrcSwizzleBegin, MinSwizzleSize>::type>::type CCuttedSrcSwizzleVector;

					private:
						template<class SrcIter>
						class Pred
						{
							template<class F>
							struct invoke
							{
								typedef typename F::type type;
							};
							using DstIter = typename mpl::find<CDstSwizzleVector, typename mpl::deref<SrcIter>::type>::type;
							using DstWasAlreadyWritten = typename mpl::less<DistanceFromBegin<CDstSwizzleVector, DstIter>, DistanceFromBegin<CCuttedSrcSwizzleVector, SrcIter>>::type;

						private:
							using FindSrcWrittenToDstIter = mpl::advance<typename mpl::begin<CCuttedSrcSwizzleVector>::type, DistanceFromBegin<CDstSwizzleVector, DstIter>>;
							using DstWasModified = mpl::bind<typename mpl::lambda<mpl::not_equal_to<invoke<_1>, invoke<_2>>>::type, mpl::deref<DstIter>, mpl::deref<typename FindSrcWrittenToDstIter::type>>;

						public:
							typedef typename mpl::apply<conditional_t<assign && DstWasAlreadyWritten::value, DstWasModified, mpl::bind<mpl::lambda<_>::type, DstWasAlreadyWritten>>>::type type;
						};
						typedef typename mpl::iter_fold<CCuttedSrcSwizzleVector, false_type, mpl::or_<_1, Pred<_2>>>::type Result;

					public:
						static constexpr typename Result::value_type value = Result::value;
					};

					// mpl::transform does not work with ranges (bug in mpl?) => use mpl::transform_view (even if it can ponentially be less efficient)
					template<class SwizzleDesc, unsigned int rowIdx>
					using RowSwizzleDesc_2_MatrixSwizzleVector = mpl::transform_view<typename SwizzleDesc::CSwizzleVector, mpl::bitor_<_, mpl::integral_c<unsigned, rowIdx << 2>>>;

					template<class DstSwizzleDesc, bool dstIsMatrix, class SrcSwizzleDesc, bool srcIsMatrix, unsigned int rowIdx, bool assign>
					using DetectRowVsMatrixWARHaard = SwizzleWARHazardDetectHelper<
						conditional_t<dstIsMatrix, typename DstSwizzleDesc::CSwizzleVector, RowSwizzleDesc_2_MatrixSwizzleVector<DstSwizzleDesc, rowIdx>>,
						conditional_t<srcIsMatrix, typename SrcSwizzleDesc::CSwizzleVector, RowSwizzleDesc_2_MatrixSwizzleVector<SrcSwizzleDesc, rowIdx>>,
						assign>;
#else
					template<class PackedDstSwizzle, class PackedSrcSwizzle, bool assign>
					class SwizzleWARHazardDetectHelper
					{
						// cut SrcSwizzle off
						static constexpr unsigned int cuttedSrcDimension = std::min(PackedSrcSwizzle::dimension, PackedDstSwizzle::dimension);

					private:
						// searches until srcSwizzleIdx, returns srcSwizzleIdx if not found
#if defined _MSC_VER && _MSC_VER <= 1900
						static constexpr unsigned FindDstSwizzleIdx(unsigned srcSwizzleIdx, unsigned dstSwizzleIdx = 0)
						{
							return
								dstSwizzleIdx < srcSwizzleIdx ?
									PackedDstSwizzle::FetchIdx(dstSwizzleIdx) == PackedSrcSwizzle::FetchIdx(srcSwizzleIdx) ?
										dstSwizzleIdx
									:
										FindDstSwizzleIdx(srcSwizzleIdx, dstSwizzleIdx + 1)
								:
									srcSwizzleIdx;
						}
#else
						static constexpr unsigned FindDstSwizzleIdx(unsigned srcSwizzleIdx)
						{
							unsigned dstSwizzleIdx = 0;
							const auto idx2Find = PackedSrcSwizzle::FetchIdx(srcSwizzleIdx);
							while (dstSwizzleIdx < srcSwizzleIdx && PackedDstSwizzle::FetchIdx(dstSwizzleIdx) != idx2Find)
								dstSwizzleIdx++;
							return dstSwizzleIdx;
						}
#endif

						static constexpr bool DstWasAlreadyWritten(unsigned dstSwizzleIdx, unsigned srcSwizzleIdx)
						{
							return dstSwizzleIdx < srcSwizzleIdx;
						}

						static constexpr bool DstWasModified(unsigned dstSwizzleIdx)
						{
							return PackedDstSwizzle::FetchIdx(dstSwizzleIdx) != PackedSrcSwizzle::FetchIdx(dstSwizzleIdx);
						}

#if defined _MSC_VER && _MSC_VER <= 1900
						static constexpr bool Iterate(unsigned dstSwizzleIdx, unsigned srcSwizzleIdx, bool dstWasAlreadyWritten)
						{
							return (assign && dstWasAlreadyWritten ? DstWasModified(dstSwizzleIdx) : dstWasAlreadyWritten) || Result(srcSwizzleIdx + 1);
						}

						static constexpr bool Iterate(unsigned dstSwizzleIdx, unsigned srcSwizzleIdx)
						{
							return Iterate(dstSwizzleIdx, srcSwizzleIdx, DstWasAlreadyWritten(dstSwizzleIdx, srcSwizzleIdx));
						}

						static constexpr bool Result(unsigned srcSwizzleIdx = 0)
						{
							return srcSwizzleIdx < cuttedSrcDimension ? Iterate(FindDstSwizzleIdx(srcSwizzleIdx), srcSwizzleIdx) : false;
						}
#else
						static constexpr bool Result()
						{
							for (unsigned srcSwizzleIdx = 0; srcSwizzleIdx < cuttedSrcDimension; srcSwizzleIdx++)
							{
								unsigned dstSwizzleIdx = FindDstSwizzleIdx(srcSwizzleIdx);
								const bool dstWasAlreadyWritten = DstWasAlreadyWritten(dstSwizzleIdx, srcSwizzleIdx);
								if (assign && dstWasAlreadyWritten ? DstWasModified(dstSwizzleIdx) : dstWasAlreadyWritten)
									return true;
							}
							return false;
						}
#endif

					public:
						static constexpr bool value = Result();
					};

					template<class PackedSwizzle, unsigned int rowIdx>
					struct RowSwizzle_2_MatrixSwizzle;

					template<unsigned int ...swizzleSeq, unsigned int rowIdx>
					struct RowSwizzle_2_MatrixSwizzle<CPackedSwizzle<swizzleSeq...>, rowIdx>
					{
						typedef CPackedSwizzle<swizzleSeq | rowIdx << 2u ...> type;
					};

					template<class SwizzleDesc, unsigned int rowIdx>
					using RowSwizzleDesc_2_PackedMatrixSwizzle = RowSwizzle_2_MatrixSwizzle<typename SwizzleDesc::PackedSwizzle, rowIdx>;

					template<class DstSwizzleDesc, bool dstIsMatrix, class SrcSwizzleDesc, bool srcIsMatrix, unsigned int rowIdx, bool assign>
					using DetectRowVsMatrixWARHaard = SwizzleWARHazardDetectHelper<
						typename conditional_t<dstIsMatrix, enable_if<true, DstSwizzleDesc>, RowSwizzleDesc_2_PackedMatrixSwizzle<DstSwizzleDesc, rowIdx>>::type,
						typename conditional_t<srcIsMatrix, enable_if<true, SrcSwizzleDesc>, RowSwizzleDesc_2_PackedMatrixSwizzle<SrcSwizzleDesc, rowIdx>>::type,
						assign>;
#endif

					template
					<
						typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc,
						typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc,
						bool assign, typename Void = void
					>
					struct DetectSwizzleWARHazard : false_type
					{
						// C++17\
						static_assert(is_same_v<Void, void>);
					};

					template
					<
						typename ElementType, unsigned int rows, unsigned int columns,
						class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
					>
					struct DetectSwizzleWARHazard<ElementType, rows, columns, DstSwizzleDesc, ElementType, rows, columns, SrcSwizzleDesc, assign> :
#if USE_BOOST_MPL
						SwizzleWARHazardDetectHelper<typename DstSwizzleDesc::CSwizzleVector, typename SrcSwizzleDesc::CSwizzleVector, assign> {};
#else
						SwizzleWARHazardDetectHelper<DstSwizzleDesc, SrcSwizzleDesc, assign> {};
#endif

					// mixed vector/matrix swizzles
					template
					<
						typename ElementType, unsigned int dstRows, unsigned int srcRows, unsigned int columns,
						class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
					>
					class DetectSwizzleWARHazard<ElementType, dstRows, columns, DstSwizzleDesc, ElementType, srcRows, columns, SrcSwizzleDesc, assign, enable_if_t<bool(dstRows) != bool(srcRows)>>
					{
						template<unsigned int rows, unsigned rowIdx = 0>
						static constexpr auto rowsFold = DetectRowVsMatrixWARHaard<DstSwizzleDesc, dstRows, SrcSwizzleDesc, srcRows, rowIdx, assign>::value || rowsFold<rows, rowIdx + 1>;

						// terminator
						template<unsigned int rows>
						static constexpr auto rowsFold<rows, rows> = false;

					public:
						static constexpr auto value = rowsFold<dstRows ? dstRows : srcRows>;
					};

#					pragma region scalar
						template<typename DstType, typename SrcType, typename Void = void>
						struct DetectScalarWARHazard : false_type
						{
							// C++17\
							static_assert(!IsPureScalar<DstType> && Is1x1<SrcType> && is_same_v<Void, void>);
						};

						// pure scalar

						template<typename DstElementType, unsigned int dstElementsCount, typename SrcType>
						using PureScalarWARHazardDetectHelper = bool_constant<(dstElementsCount > 1 &&
							is_same_v<remove_volatile_t<DstElementType>, remove_cv_t<remove_reference_t<decltype(ExtractScalar(declval<SrcType>()))>>>)>;

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc, typename SrcType>
						struct DetectScalarWARHazard<CSwizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc>, SrcType, enable_if_t<IsPureScalar<SrcType>>> :
							PureScalarWARHazardDetectHelper<DstElementType, DstSwizzleDesc::dimension, SrcType> {};

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, typename SrcType>
						struct DetectScalarWARHazard<matrix<DstElementType, dstRows, dstColumns>, SrcType, enable_if_t<IsPureScalar<SrcType>>> :
							PureScalarWARHazardDetectHelper<DstElementType, dstRows * dstColumns, SrcType> {};

						// 1D swizzle/vector

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static integral_constant<unsigned int, rows> GetSwizzleRows(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static integral_constant<unsigned int, columns> GetSwizzleColumns(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static SwizzleDesc GetSwizzleDesc(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc, typename SrcType>
						struct DetectScalarWARHazard<CSwizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc>, SrcType, enable_if_t<Is1D<SrcType>>> :
							DetectSwizzleWARHazard
							<
								DstElementType, dstRows, dstColumns, DstSwizzleDesc,
								remove_const_t<remove_reference_t<decltype(ExtractScalar(declval<SrcType>()))>>,
								enable_if_t<true, decltype(GetSwizzleRows(declval<SrcType>()))>::value,
								enable_if_t<true, decltype(GetSwizzleColumns(declval<SrcType>()))>::value,
#if USE_BOOST_MPL
								CBroadcastScalarSwizzleDesc<mpl::front<typename enable_if_t<true, decltype(GetSwizzleDesc(declval<SrcType>()))>::CSwizzleVector>::type::value, DstSwizzleDesc::dimension>,
#else
								CBroadcastScalarSwizzleDesc<enable_if_t<true, decltype(GetSwizzleDesc(declval<SrcType>()))>::FetchIdx(0), DstSwizzleDesc::dimension>,
#endif
								false
							> {};

						template
						<
							typename DstElementType, unsigned int dstRows, unsigned int dstColumns,
							typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc
						>
						static false_type MatrixVs1D_WARHazardDetectHelper(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SrcSwizzleDesc>
#if USE_BOOST_MPL
						static bool_constant<mpl::front<typename SrcSwizzleDesc::CSwizzleVector>::type::value != (columns - 1 | rows - 1 << 2)>
#else
						static bool_constant<SrcSwizzleDesc::FetchIdx(0) != (columns - 1 | rows - 1 << 2)>
#endif
							MatrixVs1D_WARHazardDetectHelper(const CSwizzle<ElementType, rows, columns, SrcSwizzleDesc> &);

						template<typename ElementType, unsigned int dstRows, unsigned int columns, class SrcSwizzleDesc>
#if USE_BOOST_MPL
						static bool_constant<(mpl::front<typename SrcSwizzleDesc::CSwizzleVector>::type::value != columns - 1 || dstRows > 1)>
#else
						static bool_constant<(SrcSwizzleDesc::FetchIdx(0) != columns - 1 || dstRows > 1)>
#endif
							MatrixVs1D_WARHazardDetectHelper(const CSwizzle<ElementType, 0, columns, SrcSwizzleDesc> &);

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, typename SrcType>
						struct DetectScalarWARHazard<matrix<DstElementType, dstRows, dstColumns>, SrcType, enable_if_t<Is1D<SrcType>>> :
							decltype(MatrixVs1D_WARHazardDetectHelper<DstElementType, dstRows, dstColumns>(declval<SrcType>())) {};
#					pragma endregion
#				pragma endregion

#				pragma region trigger
#					ifndef NDEBUG
						// vector
						template<unsigned rowIdx = 0, typename ElementType, unsigned int columns, class SwizzleDesc>
						static inline const void *GetRowAddress(const CSwizzleDataAccess<ElementType, 0, columns, SwizzleDesc> &swizzle)
						{
							return swizzle.Data();
						}

						// matrix
						template<unsigned rowIdx = 0, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static inline const void *GetRowAddress(const CSwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> &swizzle)
						{
							return swizzle.Data()[rowIdx].Data();
						}

						template
						<
							bool assign, class DstSwizzleDesc, class SrcSwizzleDesc,
							typename ElementType, unsigned int rows, unsigned int columns
						>
						static inline bool TriggerWARHazard(CSwizzleDataAccess<ElementType, rows, columns, DstSwizzleDesc> &dst, const CSwizzleDataAccess<ElementType, rows, columns, SrcSwizzleDesc> &src)
						{
#if USE_BOOST_MPL
							return SwizzleWARHazardDetectHelper<typename DstSwizzleDesc::CSwizzleVector, typename SrcSwizzleDesc::CSwizzleVector, assign>::value && GetRowAddress(dst) == GetRowAddress(src);
#else
							return SwizzleWARHazardDetectHelper<DstSwizzleDesc, SrcSwizzleDesc, assign>::value && GetRowAddress(dst) == GetRowAddress(src);
#endif
						}

						// mixed vector/matrix swizzles
						template
						<
							bool assign, unsigned rowIdx = 0, class DstSwizzleDesc, class SrcSwizzleDesc,
							typename ElementType, unsigned int dstRows, unsigned int srcRows, unsigned int columns
						>
						static inline enable_if_t<bool(dstRows) != bool(srcRows) && rowIdx < std::max(dstRows, srcRows), bool>
						TriggerWARHazard(CSwizzleDataAccess<ElementType, dstRows, columns, DstSwizzleDesc> &dst, const CSwizzleDataAccess<ElementType, srcRows, columns, SrcSwizzleDesc> &src)
						{
							return (DetectRowVsMatrixWARHaard<DstSwizzleDesc, dstRows, SrcSwizzleDesc, srcRows, rowIdx, assign>::value
								&& GetRowAddress<rowIdx>(dst) == GetRowAddress<rowIdx>(src)) | TriggerWARHazard<assign, rowIdx + 1>(dst, src);
						}

						// served both for unmatched swizzles and as terminator for mixed vector/matrix swizzles
						template
						<
							bool assign, unsigned rowIdx = 0,
							typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc,
							typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc
						>
						static inline bool TriggerWARHazard(CSwizzleDataAccess<DstElementType, dstRows, dstColumns, DstSwizzleDesc> &, const CSwizzleDataAccess<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &)
						{
							return false;
						}

#						pragma region scalar
							// TODO: use C++17 fold expression in place of recursive inline function calls\
							it can improve performance in debug builds where it is common for compiler to not inline functions

							// swizzle
							template<unsigned idx = 0, typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc>
							static inline enable_if_t<(idx < DstSwizzleDesc::dimension - 1), bool> TriggerScalarWARHazard(CSwizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc> &dst, const void *src)
							{
								return &dst[idx] == src | TriggerScalarWARHazard<idx + 1>(dst, src);
							}

							// matrix
							template<unsigned rowIdx = 0, typename DstElementType, unsigned int dstRows, unsigned int dstColumns>
							static inline enable_if_t<(rowIdx < dstRows - 1), bool> TriggerScalarWARHazard(matrix<DstElementType, dstRows, dstColumns> &dst, const void *src)
							{
								return &dst[rowIdx][dstColumns - 1] == src | TriggerScalarWARHazard<rowIdx + 1>(dst, src);
							}

							// terminator for both swizzles and matrices
							template<unsigned = 0, class DstType>
							static inline bool TriggerScalarWARHazard(DstType &, const void *)
							{
								return false;
							}
#						pragma endregion NOTE: extracted scalar expected as src
#					endif
#				pragma endregion NOTE: assert should not be triggered if WAR hazard is not detected except for scalars.
#			pragma endregion

#if defined _MSC_VER && _MSC_VER <= 1900 && !_DEBUG
			namespace ElementsCountHelpers
			{
				// empty
				static integral_constant<unsigned int, 0u> ElementsCountHelper();

				// scalar
				template<typename Type>
				static enable_if_t<IsPureScalar<Type>, integral_constant<unsigned int, 1u>> ElementsCountHelper(const Type &);

				// swizzle
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				static integral_constant<unsigned int, SwizzleDesc::dimension> ElementsCountHelper(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

				// matrix
				template<typename ElementType, unsigned int rows, unsigned int columns>
				static integral_constant<unsigned int, rows * columns> ElementsCountHelper(const matrix<ElementType, rows, columns> &);

				template<typename First, typename Second, typename ...Rest>
				static auto ElementsCountHelper(const First &first, const Second &second, const Rest &...rest)	// enaable_if_t - workaround for VS 2015
					-> integral_constant<unsigned int, enable_if_t<true, decltype(ElementsCountHelper(first))>::value + enable_if_t<true, decltype(ElementsCountHelper(second, rest...))>::value>;
			}

			template<typename ...Args>
			static constexpr unsigned int ElementsCount = decltype(ElementsCountHelpers::ElementsCountHelper(declval<Args>()...))::value;
#endif

			class CDataCommon
			{
			protected:
				enum class InitType
				{
					Copy,
					Scalar,
					Array,
					Sequencing,
				};

				template<InitType, class IdxSeq, unsigned offset = 0>
				class InitTag {};

				// unresolved external symbols on VS 2015 under whole program optimizations
#if !(defined _MSC_VER && _MSC_VER <= 1900) || _DEBUG
			private:
				// empty
				static integral_constant<unsigned int, 0u> ElementsCountHelper();

				// scalar
				template<typename Type>
				static enable_if_t<IsPureScalar<Type>, integral_constant<unsigned int, 1u>> ElementsCountHelper(const Type &);

				// swizzle
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				static integral_constant<unsigned int, SwizzleDesc::dimension> ElementsCountHelper(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

				// matrix
				template<typename ElementType, unsigned int rows, unsigned int columns>
				static integral_constant<unsigned int, rows * columns> ElementsCountHelper(const matrix<ElementType, rows, columns> &);

				template<typename First, typename Second, typename ...Rest>
				static auto ElementsCountHelper(const First &first, const Second &second, const Rest &...rest)	// enaable_if_t - workaround for VS 2015
					-> integral_constant<unsigned int, enable_if_t<true, decltype(ElementsCountHelper(first))>::value + enable_if_t<true, decltype(ElementsCountHelper(second, rest...))>::value>;

			protected:
				template<typename ...Args>
				static constexpr unsigned int ElementsCount = decltype(ElementsCountHelper(declval<Args>()...))::value;
#endif

#if defined _MSC_VER && _MSC_VER <= 1900
				// SFINAE leads to internal comiler error on VS 2015, use tagging as workaround
			private:
				enum class Dispatch
				{
					Skip,
					Scalar,
					Other,
				};

				template<Dispatch dispatch>
				using DispatchTag = integral_constant<Dispatch, dispatch>;

				// next
				template<unsigned idx, typename First, typename ...Rest>
				static inline decltype(auto) GetElementImpl(DispatchTag<Dispatch::Skip>, const First &, const Rest &...rest) noexcept
				{
					return GetElement<idx - ElementsCount<const First &>>(rest...);
				}

				// scalar
				template<unsigned idx, typename SrcType, typename ...Rest>
				static inline decltype(auto) GetElementImpl(DispatchTag<Dispatch::Scalar>, const SrcType &scalar, const Rest &...rest) noexcept
				{
					return scalar;
				}

				// swizzle
				template<unsigned idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, typename ...Rest>
				static inline decltype(auto) GetElementImpl(DispatchTag<Dispatch::Other>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src, const Rest &...rest) noexcept
				{
					return src[idx];
				}

				// matrix
				template<unsigned idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, typename ...Rest>
				static inline decltype(auto) GetElementImpl(DispatchTag<Dispatch::Other>, const matrix<SrcElementType, srcRows, srcColumns> &src, const Rest &...rest) noexcept
				{
					return src[idx / srcColumns][idx % srcColumns];
				}

			protected:
				// clamp
				template<unsigned idx, typename First, typename ...Rest>
				static inline decltype(auto) GetElement(const First &first, const Rest &...rest) noexcept
				{
					constexpr auto count = ElementsCount<decltype(first)> + ElementsCount<decltype(rest)...>;
					constexpr auto clampedIdx = idx < count ? idx : count - 1u;
					return GetElementImpl<clampedIdx>(DispatchTag<clampedIdx < ElementsCount<decltype(first)> ? IsPureScalar<First> ? Dispatch::Scalar : Dispatch::Other : Dispatch::Skip>(), first, rest...);
				}
#else
			private:
				// scalar
				template<unsigned idx, typename SrcType>
				static inline auto GetElementImpl(const SrcType &scalar) noexcept -> enable_if_t<IsPureScalar<SrcType>, decltype(scalar)>
				{
					return scalar;
				}

				// swizzle
				template<unsigned idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				static inline decltype(auto) GetElementImpl(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) noexcept
				{
					return src[idx];
				}

				// matrix
				template<unsigned idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				static inline decltype(auto) GetElementImpl(const matrix<SrcElementType, srcRows, srcColumns> &src) noexcept
				{
					return src[idx / srcColumns][idx % srcColumns];
				}

				// dispatch
				template<unsigned idx, typename First, typename ...Rest>
				static inline auto GetElementFind(const First &first, const Rest &...) noexcept
					-> enable_if_t<idx < ElementsCount<const First &>, decltype(GetElementImpl<idx>(first))>
				{
					return GetElementImpl<idx>(first);
				}

				// next
				template<unsigned idx, typename First, typename ...Rest>
				static inline auto GetElementFind(const First &, const Rest &...rest) noexcept
					-> enable_if_t<idx >= ElementsCount<const First &>, decltype(GetElementFind<idx - ElementsCount<const First &>>(rest...))>
				{
					return GetElementFind<idx - ElementsCount<const First &>>(rest...);
				}

			protected:
				// clamp
				template<unsigned idx, typename ...Args>
				static inline decltype(auto) GetElement(const Args &...args) noexcept
				{
					constexpr auto count = ElementsCount<const Args &...>;
					return GetElementFind<idx < count ? idx : count - 1u>(args...);
				}
#endif
			};

			// generic for matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CData final : private CDataCommon
			{
				friend class matrix<ElementType, rows, columns>;
				friend bool VectorMath::all<>(const matrix<ElementType, rows, columns> &);
				friend bool VectorMath::any<>(const matrix<ElementType, rows, columns> &);
				friend bool VectorMath::none<>(const matrix<ElementType, rows, columns> &);

				template<typename, unsigned int, unsigned int, class>
				friend class CSwizzleDataAccess;

				friend class CDataContainer<ElementType, rows, columns>;

			private:
				CData() = default;
				CData(const CData &) = default;
				CData(CData &&) = default;
				~CData() = default;
#ifdef __GNUC__
				CData &operator =(const CData &) = default;
				CData &operator =(CData &&) = default;
#else
				CData &operator =(const CData &) & = default;
				CData &operator =(CData &&) & = default;
#endif

			private:	// matrix specific ctors
				template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				CData(InitTag<InitType::Copy, index_sequence<rowIdx...>>, const matrix<SrcElementType, srcRows, srcColumns> &src);

				template<size_t ...rowIdx, typename SrcElementType>
				CData(InitTag<InitType::Scalar, index_sequence<rowIdx...>>, const SrcElementType &scalar);

				template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				CData(InitTag<InitType::Array, index_sequence<rowIdx...>>, const SrcElementType (&src)[srcRows][srcColumns]);

				template<size_t ...rowIdx, typename ...Args>
				CData(InitTag<InitType::Sequencing, index_sequence<rowIdx...>>, const Args &...args);

			private:
				vector<ElementType, columns> rowsData[rows];
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline CData<ElementType, rows, columns>::CData(InitTag<InitType::Copy, index_sequence<rowIdx...>>, const matrix<SrcElementType, srcRows, srcColumns> &src) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Copy, make_index_sequence<columns>>(), src[rowIdx])... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType>
			inline CData<ElementType, rows, columns>::CData(InitTag<InitType::Scalar, index_sequence<rowIdx...>>, const SrcElementType &scalar) :
				rowsData{ (rowIdx, scalar)... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline CData<ElementType, rows, columns>::CData(InitTag<InitType::Array, index_sequence<rowIdx...>>, const SrcElementType (&src)[srcRows][srcColumns]) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Array, make_index_sequence<columns>>(), src[rowIdx])... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename ...Args>
			inline CData<ElementType, rows, columns>::CData(InitTag<InitType::Sequencing, index_sequence<rowIdx...>>, const Args &...args) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Sequencing, make_index_sequence<columns>, rowIdx * columns>(), args...)... } {}

			// specialization for vector
			template<typename ElementType, unsigned int dimension>
			class CData<ElementType, 0, dimension> final : private CDataCommon
			{
				friend class vector<ElementType, dimension>;

				template<typename, unsigned int, unsigned int, class>
				friend class CSwizzleDataAccess;

				friend class CDataContainer<ElementType, 0, dimension>;

			private:
				CData() = default;
				CData(const CData &) = default;
				CData(CData &&) = default;
				~CData() = default;
#ifdef __GNUC__
				CData &operator =(const CData &) = default;
				CData &operator =(CData &&) = default;
#else
				CData &operator =(const CData &) & = default;
				CData &operator =(CData &&) & = default;
#endif

			private:	// vector specific ctors
				template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				CData(InitTag<InitType::Copy, index_sequence<idx...>>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

				template<size_t ...idx, typename SrcElementType>
				CData(InitTag<InitType::Scalar, index_sequence<idx...>>, const SrcElementType &scalar);

				template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
				CData(InitTag<InitType::Array, index_sequence<idx...>>, const SrcElementType (&src)[srcDimension]);

				template<size_t ...idx, unsigned offset, typename ...Args>
				CData(InitTag<InitType::Sequencing, index_sequence<idx...>, offset>, const Args &...args);

			private:
				ElementType data[dimension];
			};

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline CData<ElementType, 0, dimension>::CData(InitTag<InitType::Copy, index_sequence<idx...>>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				data{ static_cast<const ElementType &>(src[idx])... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType>
			inline CData<ElementType, 0, dimension>::CData(InitTag<InitType::Scalar, index_sequence<idx...>>, const SrcElementType &scalar) :
				data{ (idx, static_cast<const ElementType &>(scalar))... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
			inline CData<ElementType, 0, dimension>::CData(InitTag<InitType::Array, index_sequence<idx...>>, const SrcElementType (&src)[srcDimension]) :
				data{ static_cast<const ElementType &>(src[idx])... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, unsigned offset, typename ...Args>
			inline CData<ElementType, 0, dimension>::CData(InitTag<InitType::Sequencing, index_sequence<idx...>, offset>, const Args &...args) :
				data{ static_cast<const ElementType &>(GetElement<idx + offset>(args...))... } {}

			// generic vector/matrix
			template<typename ElementType, unsigned int rows, unsigned int columns, typename Void>
			class CDataContainer
			{
				// C++17\
				static_assert(is_same_v<Void, void>);

			public:
				CData<ElementType, rows, columns> data;

			protected:
				CDataContainer() = default;
				CDataContainer(const CDataContainer &) = default;
				CDataContainer(CDataContainer &&) = default;
#if defined _MSC_VER && _MSC_VER <= 1900
				template<typename First, typename ...Rest> CDataContainer(const First &first, const Rest &...rest) : data(first, rest...) {}
#else
				template<typename ...Args> CDataContainer(const Args &...args) : data(args...) {}
#endif
#ifdef __GNUC__
				CDataContainer &operator =(const CDataContainer &) = default;
				CDataContainer &operator =(CDataContainer &&) = default;
#else
				CDataContainer &operator =(const CDataContainer &) & = default;
				CDataContainer &operator =(CDataContainer &&) & = default;
#endif
			};

#			pragma region Initializer list
#if INIT_LIST_ITEM_COPY
				template<typename ElementType, unsigned int capacity>
				class CInitListItem final
				{
					CInitListItem() = delete;
					CInitListItem(const CInitListItem &) = delete;
					CInitListItem &operator =(const CInitListItem &) = delete;

				private:
					template<size_t ...idx, typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr CInitListItem(index_sequence<idx...>, const CSwizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						itemStore{ static_cast<const ElementType &>(item[idx])... },
						itemSize(sizeof...(idx))
					{}

					template<size_t ...idx, typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr CInitListItem(index_sequence<idx...>, const matrix<ItemElementType, itemRows, itemColumns> &item) :
						item{ static_cast<const ElementType &>(item[idx / itemColumns][idx % itemColumns])... },
						itemSize(sizeof...(idx))
					{}

				public:
					template<typename ItemElementType, typename = enable_if_t<IsPureScalar<ItemElementType>>>
					constexpr CInitListItem(const ItemElementType &item) :
						itemStore{ static_cast<const ElementType &>(item) },
						itemSize(1)
					{}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr CInitListItem(const CSwizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						CInitListItem(make_index_sequence<std::min(ItemSwizzleDesc::dimension, capacity)>, item)
					{
						static_assert(ItemSwizzleDesc::dimension <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr CInitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) :
						CInitListItem(make_index_sequence<std::min(itemRows * itemColumns, capacity)>, item)
					{
						static_assert(itemRows * itemColumns <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					constexpr const ElementType &operator [](unsigned idx) const noexcept
					{
						return itemStore[idx];
					}

					constexpr unsigned int GetItemSize() const noexcept
					{
						return itemSize;
					}

				private:
					const ElementType itemStore[capacity];
					const unsigned int itemSize;
				};
#else
				template<typename ElementType, unsigned int capacity>
				class CInitListItem final
				{
					CInitListItem() = delete;
					CInitListItem(const CInitListItem &) = delete;
					CInitListItem &operator =(const CInitListItem &) = delete;

				private:
					template<typename ItemElementType>
					static const ElementType &GetItemElement(const void *item, unsigned) noexcept
					{
						return *reinterpret_cast<const ItemElementType *>(item);
					}

					template<typename ItemElementType, unsigned int dimension>
					static const ElementType &GetItemElement(const void *item, unsigned idx) noexcept
					{
						const auto &v = *reinterpret_cast<const vector<ItemElementType, dimension> *>(item);
						return v[idx];
					}

					template<typename ItemElementType, unsigned int rows, unsigned int columns>
					static const ElementType &GetItemElement(const void *item, unsigned idx) noexcept
					{
						const auto &m = *reinterpret_cast<const matrix<ItemElementType, rows, columns> *>(item);
						return m[idx / columns][idx % columns];
					}

				public:
					template<typename ItemElementType, typename = enable_if_t<IsPureScalar<ItemElementType>>>
					constexpr CInitListItem(const ItemElementType &item) :
						getItemElement(GetItemElement<ItemElementType>),
						item(&item),
						itemSize(1)
					{}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr CInitListItem(const CSwizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						getItemElement(GetItemElement<ItemElementType, ItemSwizzleDesc::dimension>),
						item(&item),
						itemSize(ItemSwizzleDesc::dimension)
					{
						static_assert(ItemSwizzleDesc::dimension <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr CInitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) :
						getItemElement(GetItemElement<ItemElementType, itemRows, itemColumns>),
						item(&item),
						itemSize(itemRows * itemColumns)
					{
						static_assert(itemRows * itemColumns <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					constexpr const ElementType &operator [](unsigned idx) const noexcept
					{
						return getItemElement(item, idx);
					}

					constexpr unsigned int GetItemSize() const noexcept
					{
						return itemSize;
					}

				private:
					const ElementType &(&getItemElement)(const void *, unsigned) noexcept;
					const void *const item;
					const unsigned int itemSize;
				};
#endif
#			pragma endregion TODO: consider to remove it and rely on potentially more efficient variadic template technique for sequencing ctors, or limit its usage for assignment operators only
		}

#pragma region specializations for graphics vectors/matrices
#if USE_BOOST_PREPROCESSOR
#		define BOOST_PP_ITERATION_LIMITS (0, 4)
#		define BOOST_PP_FILENAME_1 "vector math.h"
#		include BOOST_PP_ITERATE()
#else
#pragma region swizzles generator
#ifdef _MSC_VER
#		define CONCAT_IMPL_1(a, b) a ## b
#		define CONCAT_IMPL_0(a, b) CONCAT_IMPL_1(a, b)
#		define CONCAT_IMPL(a, b) CONCAT_IMPL_0(a, b)
#else
#		define CONCAT_IMPL(a, b) a ## b
#endif
#		define CONCAT(a, b) CONCAT_IMPL(a, b)
#
#		define LOOKUP_SYMBOL_0_0 x
#		define LOOKUP_SYMBOL_0_1 y
#		define LOOKUP_SYMBOL_0_2 z
#		define LOOKUP_SYMBOL_0_3 w
#
#		define LOOKUP_SYMBOL_1_0 r
#		define LOOKUP_SYMBOL_1_1 g
#		define LOOKUP_SYMBOL_1_2 b
#		define LOOKUP_SYMBOL_1_3 a
#
#		define LOOKUP_SYMBOL_0_00 _m00
#		define LOOKUP_SYMBOL_0_01 _m01
#		define LOOKUP_SYMBOL_0_02 _m02
#		define LOOKUP_SYMBOL_0_03 _m03
#		define LOOKUP_SYMBOL_0_10 _m10
#		define LOOKUP_SYMBOL_0_11 _m11
#		define LOOKUP_SYMBOL_0_12 _m12
#		define LOOKUP_SYMBOL_0_13 _m13
#		define LOOKUP_SYMBOL_0_20 _m20
#		define LOOKUP_SYMBOL_0_21 _m21
#		define LOOKUP_SYMBOL_0_22 _m22
#		define LOOKUP_SYMBOL_0_23 _m23
#		define LOOKUP_SYMBOL_0_30 _m30
#		define LOOKUP_SYMBOL_0_31 _m31
#		define LOOKUP_SYMBOL_0_32 _m32
#		define LOOKUP_SYMBOL_0_33 _m33
#
#		define LOOKUP_SYMBOL_1_00 _11
#		define LOOKUP_SYMBOL_1_01 _12
#		define LOOKUP_SYMBOL_1_02 _13
#		define LOOKUP_SYMBOL_1_03 _14
#		define LOOKUP_SYMBOL_1_10 _21
#		define LOOKUP_SYMBOL_1_11 _22
#		define LOOKUP_SYMBOL_1_12 _23
#		define LOOKUP_SYMBOL_1_13 _24
#		define LOOKUP_SYMBOL_1_20 _31
#		define LOOKUP_SYMBOL_1_21 _32
#		define LOOKUP_SYMBOL_1_22 _33
#		define LOOKUP_SYMBOL_1_23 _34
#		define LOOKUP_SYMBOL_1_30 _41
#		define LOOKUP_SYMBOL_1_31 _42
#		define LOOKUP_SYMBOL_1_32 _43
#		define LOOKUP_SYMBOL_1_33 _44
#
#		define LOOKUP_SYMBOL_0(idx, ...) LOOKUP_SYMBOL_0_##idx
#		define LOOKUP_SYMBOL_1(idx, ...) LOOKUP_SYMBOL_1_##idx
#if defined _MSC_VER && _MSC_VER <= 1900
		constexpr unsigned int ExtractSwizzleIdx(unsigned int dec, unsigned int bcd)
		{
			return bcd;
		}
#		define LOOKUP_SYMBOL_0_ExtractSwizzleIdx(idx, ...) LOOKUP_SYMBOL_0_##idx
#		define LOOKUP_SYMBOL_1_ExtractSwizzleIdx(idx, ...) LOOKUP_SYMBOL_1_##idx
#endif
#
#		define IDX_SEQ_2_SYMBOLS_1(xform, i0) xform i0
#		define IDX_SEQ_2_SYMBOLS_2(xform, i0, i1) CONCAT(IDX_SEQ_2_SYMBOLS_1(xform, i0), xform i1)
#		define IDX_SEQ_2_SYMBOLS_3(xform, i0, i1, i2) CONCAT(IDX_SEQ_2_SYMBOLS_2(xform, i0, i1), xform i2)
#		define IDX_SEQ_2_SYMBOLS_4(xform, i0, i1, i2, i3) CONCAT(IDX_SEQ_2_SYMBOLS_3(xform, i0, i1, i2), xform i3)
#ifdef _MSC_VER
#		define IDX_SEQ_2_SYMBOLS(swizDim, xform, ...) CONCAT(IDX_SEQ_2_SYMBOLS_, swizDim(xform, __VA_ARGS__))
#else
#		define IDX_SEQ_2_SYMBOLS(swizDim, xform, ...) IDX_SEQ_2_SYMBOLS_##swizDim(xform, __VA_ARGS__)
#endif
#		define GENERATE_SWIZZLE(rows, columns, swizDim, ...)				\
			CSwizzle<ElementType, rows, columns, CSwizzleDesc<__VA_ARGS__>>	\
				IDX_SEQ_2_SYMBOLS(swizDim, LOOKUP_SYMBOL_0, __VA_ARGS__),	\
				IDX_SEQ_2_SYMBOLS(swizDim, LOOKUP_SYMBOL_1, __VA_ARGS__);
#
#		define DEC_2_BIN_
#		define DEC_2_BIN_0 00
#		define DEC_2_BIN_1 01
#		define DEC_2_BIN_2 10
#		define DEC_2_BIN_3 11
#if defined _MSC_VER && _MSC_VER <= 1900
#		define ENCODE_RC_IDX(r, c) (ExtractSwizzleIdx(r ## c, CONCAT(0b, CONCAT(DEC_2_BIN_##r, DEC_2_BIN_##c))))
#else
#		define ENCODE_RC_IDX(r, c) (r ## c, CONCAT(0b, CONCAT(DEC_2_BIN_##r, DEC_2_BIN_##c)))
#endif
#
#		define SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, c4) GENERATE_SWIZZLE(rows, columns, 4, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1), ENCODE_RC_IDX(r2, c2), ENCODE_RC_IDX(r3, c4))
#		define SWIZZLE_3_ITERATE_COLUMN_1(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 0)
#		define SWIZZLE_3_ITERATE_COLUMN_2(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_1(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 1)
#		define SWIZZLE_3_ITERATE_COLUMN_3(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_2(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 2)
#		define SWIZZLE_3_ITERATE_COLUMN_4(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_3(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 3)
#		define SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_##columns(rows, columns, r0, c0, r1, c1, r2, c2, r3)
#		define SWIZZLE_3_ITERATE_ROW_0(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, )
#		define SWIZZLE_3_ITERATE_ROW_1(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 0)
#		define SWIZZLE_3_ITERATE_ROW_2(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_1(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 1)
#		define SWIZZLE_3_ITERATE_ROW_3(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_2(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 2)
#		define SWIZZLE_3_ITERATE_ROW_4(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_3(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 3)
#		define SWIZZLE_3_SELECTOR_3(rows, columns, r0, c0, r1, c1, r2, c2) GENERATE_SWIZZLE(rows, columns, 3, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1), ENCODE_RC_IDX(r2, c2))
#		define SWIZZLE_3_SELECTOR_4(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_##rows(rows, columns, r0, c0, r1, c1, r2, c2)
#		define SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, c2) SWIZZLE_3_SELECTOR_##swizDim(rows, columns, r0, c0, r1, c1, r2, c2)
#
#		define SWIZZLE_2_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 0)
#		define SWIZZLE_2_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 1)
#		define SWIZZLE_2_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 2)
#		define SWIZZLE_2_ITERATE_COLUMN_4(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 3)
#		define SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0, c0, r1, c1, r2)
#		define SWIZZLE_2_ITERATE_ROW_0(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, )
#		define SWIZZLE_2_ITERATE_ROW_1(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 0)
#		define SWIZZLE_2_ITERATE_ROW_2(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_1(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 1)
#		define SWIZZLE_2_ITERATE_ROW_3(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_2(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 2)
#		define SWIZZLE_2_ITERATE_ROW_4(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_3(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 3)
#		define SWIZZLE_2_SELECTOR_2(rows, columns, r0, c0, r1, c1) GENERATE_SWIZZLE(rows, columns, 2, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1))
#		define SWIZZLE_2_SELECTOR_3(rows, columns, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_##rows(rows, columns, 3, r0, c0, r1, c1)
#		define SWIZZLE_2_SELECTOR_4(rows, columns, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_##rows(rows, columns, 4, r0, c0, r1, c1)
#		define SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_SELECTOR_##swizDim(rows, columns, r0, c0, r1, c1)
#
#		define SWIZZLE_1_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 0)
#		define SWIZZLE_1_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 1)
#		define SWIZZLE_1_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 2)
#		define SWIZZLE_1_ITERATE_COLUMN_4(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 3)
#		define SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0, c0, r1)
#		define SWIZZLE_1_ITERATE_ROW_0(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, )
#		define SWIZZLE_1_ITERATE_ROW_1(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 0)
#		define SWIZZLE_1_ITERATE_ROW_2(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_1(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 1)
#		define SWIZZLE_1_ITERATE_ROW_3(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_2(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 2)
#		define SWIZZLE_1_ITERATE_ROW_4(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_3(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 3)
#		define SWIZZLE_1_SELECTOR_1(rows, columns, r0, c0) GENERATE_SWIZZLE(rows, columns, 1, ENCODE_RC_IDX(r0, c0))
#		define SWIZZLE_1_SELECTOR_2(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 2, r0, c0)
#		define SWIZZLE_1_SELECTOR_3(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 3, r0, c0)
#		define SWIZZLE_1_SELECTOR_4(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 4, r0, c0)
#		define SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, c0) SWIZZLE_1_SELECTOR_##swizDim(rows, columns, r0, c0)
#
#		define SWIZZLE_0_ITERATE_COLUMN_1(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 0)
#		define SWIZZLE_0_ITERATE_COLUMN_2(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_1(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 1)
#		define SWIZZLE_0_ITERATE_COLUMN_3(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_2(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 2)
#		define SWIZZLE_0_ITERATE_COLUMN_4(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_3(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 3)
#		define SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0)
#		define SWIZZLE_0_ITERATE_ROW_0(rows, columns, swizDim) SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, )
#		define SWIZZLE_0_ITERATE_ROW_1(rows, columns, swizDim) SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, 0)
#		define SWIZZLE_0_ITERATE_ROW_2(rows, columns, swizDim) SWIZZLE_0_ITERATE_ROW_1(rows, columns, swizDim) SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, 1)
#		define SWIZZLE_0_ITERATE_ROW_3(rows, columns, swizDim) SWIZZLE_0_ITERATE_ROW_2(rows, columns, swizDim) SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, 2)
#		define SWIZZLE_0_ITERATE_ROW_4(rows, columns, swizDim) SWIZZLE_0_ITERATE_ROW_3(rows, columns, swizDim) SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, 3)
#		define SWIZZLE_ITERATE(rows, columns, swizDim) SWIZZLE_0_ITERATE_ROW_##rows(rows, columns, swizDim)
#
#		define GENERATE_SWIZZLES(rows, columns)	\
			SWIZZLE_ITERATE(rows, columns, 1)	\
			SWIZZLE_ITERATE(rows, columns, 2)	\
			SWIZZLE_ITERATE(rows, columns, 3)	\
			SWIZZLE_ITERATE(rows, columns, 4)
#pragma endregion

		// undefs to be on the safe side
#		undef ROWS
#		undef COULMNS
#		undef TRIVIAL_CTOR
#		include "vector math generate stuff.h"

#pragma region cleanup
#ifdef _MSC_VER
#		undef CONCAT_IMPL_1
#		undef CONCAT_IMPL_0
#endif
#		undef CONCAT_IMPL
#		undef CONCAT
#
#		undef LOOKUP_SYMBOL_0_0
#		undef LOOKUP_SYMBOL_0_1
#		undef LOOKUP_SYMBOL_0_2
#		undef LOOKUP_SYMBOL_0_3
#
#		undef LOOKUP_SYMBOL_1_0
#		undef LOOKUP_SYMBOL_1_1
#		undef LOOKUP_SYMBOL_1_2
#		undef LOOKUP_SYMBOL_1_3
#
#		undef LOOKUP_SYMBOL_0_00
#		undef LOOKUP_SYMBOL_0_01
#		undef LOOKUP_SYMBOL_0_02
#		undef LOOKUP_SYMBOL_0_03
#		undef LOOKUP_SYMBOL_0_10
#		undef LOOKUP_SYMBOL_0_11
#		undef LOOKUP_SYMBOL_0_12
#		undef LOOKUP_SYMBOL_0_13
#		undef LOOKUP_SYMBOL_0_20
#		undef LOOKUP_SYMBOL_0_21
#		undef LOOKUP_SYMBOL_0_22
#		undef LOOKUP_SYMBOL_0_23
#		undef LOOKUP_SYMBOL_0_30
#		undef LOOKUP_SYMBOL_0_31
#		undef LOOKUP_SYMBOL_0_32
#		undef LOOKUP_SYMBOL_0_33
#
#		undef LOOKUP_SYMBOL_1_00
#		undef LOOKUP_SYMBOL_1_01
#		undef LOOKUP_SYMBOL_1_02
#		undef LOOKUP_SYMBOL_1_03
#		undef LOOKUP_SYMBOL_1_10
#		undef LOOKUP_SYMBOL_1_11
#		undef LOOKUP_SYMBOL_1_12
#		undef LOOKUP_SYMBOL_1_13
#		undef LOOKUP_SYMBOL_1_20
#		undef LOOKUP_SYMBOL_1_21
#		undef LOOKUP_SYMBOL_1_22
#		undef LOOKUP_SYMBOL_1_23
#		undef LOOKUP_SYMBOL_1_30
#		undef LOOKUP_SYMBOL_1_31
#		undef LOOKUP_SYMBOL_1_32
#		undef LOOKUP_SYMBOL_1_33
#
#		undef LOOKUP_SYMBOL_0
#		undef LOOKUP_SYMBOL_1
#if defined _MSC_VER && _MSC_VER <= 1900
#		undef LOOKUP_SYMBOL_0_ExtractSwizzleIdx
#		undef LOOKUP_SYMBOL_1_ExtractSwizzleIdx
#endif
#
#		undef IDX_SEQ_2_SYMBOLS_1
#		undef IDX_SEQ_2_SYMBOLS_2
#		undef IDX_SEQ_2_SYMBOLS_3
#		undef IDX_SEQ_2_SYMBOLS_4
#		undef IDX_SEQ_2_SYMBOLS
#		undef GENERATE_SWIZZLE
#
#		undef DEC_2_BIN_
#		undef DEC_2_BIN_0
#		undef DEC_2_BIN_1
#		undef DEC_2_BIN_2
#		undef DEC_2_BIN_3
#		undef ENCODE_RC_IDX
#
#		undef SWIZZLE_4_GENERATE
#		undef SWIZZLE_3_ITERATE_COLUMN_1
#		undef SWIZZLE_3_ITERATE_COLUMN_2
#		undef SWIZZLE_3_ITERATE_COLUMN_3
#		undef SWIZZLE_3_ITERATE_COLUMN_4
#		undef SWIZZLE_3_ITERATE_COLUMNS
#		undef SWIZZLE_3_ITERATE_ROW_0
#		undef SWIZZLE_3_ITERATE_ROW_1
#		undef SWIZZLE_3_ITERATE_ROW_2
#		undef SWIZZLE_3_ITERATE_ROW_3
#		undef SWIZZLE_3_ITERATE_ROW_4
#		undef SWIZZLE_3_SELECTOR_3
#		undef SWIZZLE_3_SELECTOR_4
#		undef SWIZZLE_3_SELECTOR
#
#		undef SWIZZLE_2_ITERATE_COLUMN_1
#		undef SWIZZLE_2_ITERATE_COLUMN_2
#		undef SWIZZLE_2_ITERATE_COLUMN_3
#		undef SWIZZLE_2_ITERATE_COLUMN_4
#		undef SWIZZLE_2_ITERATE_COLUMNS
#		undef SWIZZLE_2_ITERATE_ROW_0
#		undef SWIZZLE_2_ITERATE_ROW_1
#		undef SWIZZLE_2_ITERATE_ROW_2
#		undef SWIZZLE_2_ITERATE_ROW_3
#		undef SWIZZLE_2_ITERATE_ROW_4
#		undef SWIZZLE_2_SELECTOR_2
#		undef SWIZZLE_2_SELECTOR_3
#		undef SWIZZLE_2_SELECTOR_4
#		undef SWIZZLE_2_SELECTOR
#
#		undef SWIZZLE_1_ITERATE_COLUMN_1
#		undef SWIZZLE_1_ITERATE_COLUMN_2
#		undef SWIZZLE_1_ITERATE_COLUMN_3
#		undef SWIZZLE_1_ITERATE_COLUMN_4
#		undef SWIZZLE_1_ITERATE_COLUMNS
#		undef SWIZZLE_1_ITERATE_ROW_0
#		undef SWIZZLE_1_ITERATE_ROW_1
#		undef SWIZZLE_1_ITERATE_ROW_2
#		undef SWIZZLE_1_ITERATE_ROW_3
#		undef SWIZZLE_1_ITERATE_ROW_4
#		undef SWIZZLE_1_SELECTOR_1
#		undef SWIZZLE_1_SELECTOR_2
#		undef SWIZZLE_1_SELECTOR_3
#		undef SWIZZLE_1_SELECTOR_4
#		undef SWIZZLE_1_SELECTOR
#
#		undef SWIZZLE_0_ITERATE_COLUMN_1
#		undef SWIZZLE_0_ITERATE_COLUMN_2
#		undef SWIZZLE_0_ITERATE_COLUMN_3
#		undef SWIZZLE_0_ITERATE_COLUMN_4
#		undef SWIZZLE_0_ITERATE_COLUMNS
#		undef SWIZZLE_0_ITERATE_ROW_0
#		undef SWIZZLE_0_ITERATE_ROW_1
#		undef SWIZZLE_0_ITERATE_ROW_2
#		undef SWIZZLE_0_ITERATE_ROW_3
#		undef SWIZZLE_0_ITERATE_ROW_4
#		undef SWIZZLE_ITERATE
#
#		undef GENERATE_SWIZZLES
#pragma endregion
#endif
#pragma endregion

		namespace Impl
		{
#			ifdef NDEBUG
#				define FRIEND_DECLARATIONS
#			else
#				define FRIEND_DECLARATIONS																										\
					template<unsigned rowIdx = 0, typename ElementType, unsigned int columns, class SwizzleDesc>								\
					friend static inline const void *GetRowAddress(const CSwizzleDataAccess<ElementType, 0, columns, SwizzleDesc> &swizzle);	\
																																				\
					template<unsigned rowIdx = 0, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>				\
					friend static inline const void *GetRowAddress(const CSwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> &swizzle);
#			endif

			// CSwizzle inherits from this to reduce preprocessor generated code for faster compiling
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleDataAccess
			{
				typedef CSwizzle<ElementType, rows, columns, SwizzleDesc> TSwizzle;

			protected:
				CSwizzleDataAccess() = default;
				CSwizzleDataAccess(const CSwizzleDataAccess &) = delete;
				~CSwizzleDataAccess() = default;
				CSwizzleDataAccess &operator =(const CSwizzleDataAccess &) = delete;

			public:
				typedef TSwizzle TOperationResult;

			protected:
#ifdef __GNUC__
				operator TSwizzle &() noexcept
#else
				operator TSwizzle &() & noexcept
#endif
				{
					return static_cast<TSwizzle &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &Data() const noexcept
				{
					/*
									static	  reinterpret
									   ^		   ^
									   |		   |
					CSwizzleDataAccess -> CSwizzle -> CData
					*/
					typedef CData<ElementType, rows, columns> CData;
					return reinterpret_cast<const CData *>(static_cast<const TSwizzle *>(this))->rowsData;
				}

			public:
#ifdef __GNUC__
				const ElementType &operator [](unsigned int idx) const noexcept
#else
				const ElementType &operator [](unsigned int idx) const & noexcept
#endif
				{
					assert(idx < SwizzleDesc::dimension);
					idx = SwizzleDesc::FetchIdx(idx);
					const auto row = idx >> 2u & 3u, column = idx & 3u;
					return Data()[row][column];
				}

#ifndef __GNUC__
				const ElementType &operator [](unsigned int idx) const && noexcept
				{
					return operator [](idx);
				}
#endif

#ifdef __GNUC__
				ElementType &operator [](unsigned int idx) noexcept
#else
				ElementType &operator [](unsigned int idx) & noexcept
#endif
				{
					return const_cast<ElementType &>(static_cast<const CSwizzleDataAccess &>(*this)[idx]);
				}
			};

			// specialization for vectors
			template<typename ElementType, unsigned int vectorDimension, class SwizzleDesc>
			class CSwizzleDataAccess<ElementType, 0, vectorDimension, SwizzleDesc>
			{
				/*
				?

				does not work with gcc:
				typedef CSwizzle<...> CSwizzle;
				typedef CDataContainer<...> CDataContainer;

				does not work with VS and gcc:
				typedef vector<...> vector;

				works with both VS and gcc (differs from cases above in that it is in function scope, not in class one):
				typedef TSwizzleTraits<...> TSwizzleTraits;
				*/
				typedef CSwizzle<ElementType, 0, vectorDimension, SwizzleDesc> TSwizzle;

			protected:
				CSwizzleDataAccess() = default;
				CSwizzleDataAccess(const CSwizzleDataAccess &) = delete;
				~CSwizzleDataAccess() = default;
				CSwizzleDataAccess &operator =(const CSwizzleDataAccess &) = delete;

			public:
				typedef TSwizzle TOperationResult;

			protected:
#ifdef __GNUC__
				operator TSwizzle &() noexcept
#else
				operator TSwizzle &() & noexcept
#endif
				{
					return static_cast<TSwizzle &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &Data() const noexcept
				{
					/*
									static	  reinterpret
									   ^		   ^
									   |		   |
					CSwizzleDataAccess -> CSwizzle -> CData
					*/
					typedef CData<ElementType, 0, vectorDimension> CData;
					return reinterpret_cast<const CData *>(static_cast<const TSwizzle *>(this))->data;
				}

			public:
#ifdef __GNUC__
				const ElementType &operator [](unsigned int idx) const noexcept
#else
				const ElementType &operator [](unsigned int idx) const & noexcept
#endif
				{
					assert(idx < SwizzleDesc::dimension);
					idx = SwizzleDesc::FetchIdx(idx);
					return Data()[idx];
				}

#ifndef __GNUC__
				const ElementType &operator [](unsigned int idx) const && noexcept
				{
					return operator [](idx);
				}
#endif

#ifdef __GNUC__
				ElementType &operator [](unsigned int idx) noexcept
#else
				ElementType &operator [](unsigned int idx) & noexcept
#endif
				{
					return const_cast<ElementType &>(static_cast<const CSwizzleDataAccess &>(*this)[idx]);
				}
			};

			/*
			CVectorSwizzleDesc<vectorDimension> required for VS 2013/2015
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzleDataAccess<ElementType, 0, vectorDimension, CVectorSwizzleDesc<vectorDimension>>
			{
				/*
								static
								   ^
								   |
				CSwizzleDataAccess -> vector
				*/
				typedef vector<ElementType, vectorDimension> Tvector;

			protected:
				CSwizzleDataAccess() = default;
				CSwizzleDataAccess(const CSwizzleDataAccess &) = default;
				~CSwizzleDataAccess() = default;
#ifdef __GNUC__
				CSwizzleDataAccess &operator =(const CSwizzleDataAccess &) = default;
#else
				CSwizzleDataAccess &operator =(const CSwizzleDataAccess &) & = default;
#endif

				// TODO: consider adding 'op=' operators to friends and making some stuff below protected/private (and TOperationResult for other CSwizzleDataAccess above)
			public:
				typedef Tvector TOperationResult;

#ifdef __GNUC__
				operator Tvector &() noexcept
#else
				operator Tvector &() & noexcept
#endif
				{
					return static_cast<Tvector &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &Data() const noexcept
				{
					return static_cast<const Tvector *>(this)->data.data;
				}

				auto &Data() noexcept
				{
					return static_cast<Tvector *>(this)->data.data;
				}

			public:
#ifdef __GNUC__
				const ElementType &operator [](unsigned int idx) const noexcept
#else
				const ElementType &operator [](unsigned int idx) const & noexcept
#endif
				{
					assert(idx < vectorDimension);
					return Data()[idx];
				}

#ifndef __GNUC__
				const ElementType &operator [](unsigned int idx) const && noexcept
				{
					return operator [](idx);
				}
#endif

#ifdef __GNUC__
				ElementType &operator [](unsigned int idx) noexcept
#else
				ElementType &operator [](unsigned int idx) & noexcept
#endif
				{
					assert(idx < vectorDimension);
					return Data()[idx];
				}
			};

#			undef FRIEND_DECLARATIONS

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, false_type> : public CSwizzleDataAccess<ElementType, rows, columns, SwizzleDesc>
			{
			protected:
				CSwizzleAssign() = default;
				CSwizzleAssign(const CSwizzleAssign &) = delete;
				~CSwizzleAssign() = default;

			public:
				void operator =(const CSwizzleAssign &) = delete;
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type> : public CSwizzleDataAccess<ElementType, rows, columns, SwizzleDesc>
			{
				typedef CSwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> TSwizzleDataAccess;
				typedef CSwizzle<ElementType, rows, columns, SwizzleDesc> TSwizzle;

				// TODO: remove 'public'
			public:
				using typename TSwizzleDataAccess::TOperationResult;

			protected:
				CSwizzleAssign() = default;
				CSwizzleAssign(const CSwizzleAssign &) = default;
				~CSwizzleAssign() = default;

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			public:
#ifdef __GNUC__
				operator ElementType &() noexcept
#else
				operator ElementType &() & noexcept
#endif
				{
					return (*this)[0];
				}
#endif

			public:
#ifdef __GNUC__
				inline TOperationResult &operator =(const CSwizzleAssign &src)
#else
				inline TOperationResult &operator =(const CSwizzleAssign &src) &
#endif
				{
					return operator =(std::move(static_cast<const TSwizzle &>(src)));
				}

				// currently public to allow user specify WAR hazard explicitly if needed

				template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
				enable_if_t<(!WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);
#else
				enable_if_t<(!WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;
#endif

				template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
				enable_if_t<(WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);
#else
				enable_if_t<(WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;
#endif

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
				enable_if_t<(SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src)
#else
				enable_if_t<(SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
#endif
				{
					static constexpr auto WARHazard = DetectSwizzleWARHazard
					<
						ElementType, rows, columns, SwizzleDesc,
						SrcElementType, srcRows, srcColumns, SrcSwizzleDesc,
						true
					>::value;
					return operator =<WARHazard>(src);
				}

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
				enable_if_t<(SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src)
#else
				enable_if_t<(SrcSwizzleDesc::dimension > 1), TOperationResult &> operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &
#endif
				{
					return operator =<false>(src);
				}

				template<typename SrcType>
#ifdef __GNUC__
				inline enable_if_t<IsScalar<SrcType>, TOperationResult &> operator =(const SrcType &scalar);
#elif defined _MSC_VER && _MSC_VER <= 1900
				inline enable_if_t<IsScalar<SrcType>, TOperationResult &> operator =(const SrcType &src) &
				{
					const auto &scalar = ExtractScalar(src);
					for (unsigned idx = 0; idx < SwizzleDesc::dimension; idx++)
						(*this)[idx] = scalar;
					return *this;
				}
#else
				inline enable_if_t<IsScalar<SrcType>, TOperationResult &> operator =(const SrcType &scalar) &;
#endif

				template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
#ifdef __GNUC__
				enable_if_t<(srcRows > 1 || srcColumns > 1), TOperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src);
#else
				enable_if_t<(srcRows > 1 || srcColumns > 1), TOperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &;
#endif

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
#ifdef __GNUC__
				enable_if_t<(srcRows > 1 || srcColumns > 1), TOperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &&src)
#else
				enable_if_t<(srcRows > 1 || srcColumns > 1), TOperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &&src) &
				{
					return operator =<false>(src);
				}
#endif

#ifdef __GNUC__
				inline TOperationResult &operator =(initializer_list<CInitListItem<ElementType, SwizzleDesc::dimension>> initList);
#else
				inline TOperationResult &operator =(initializer_list<CInitListItem<ElementType, SwizzleDesc::dimension>> initList) &;
#endif

			public:
				using TSwizzleDataAccess::operator [];
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src)
#else
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
#endif
				-> enable_if_t<(!WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &>
			{
				static_assert(SwizzleDesc::dimension <= SrcSwizzleDesc::dimension, "'vector = vector': too small src dimension");
				assert(!TriggerWARHazard<true>(*this, src));
				for (unsigned idx = 0; idx < SwizzleDesc::dimension; idx++)
					(*this)[idx] = src[idx];
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src)
#else
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
#endif
				-> enable_if_t<(WARHazard && SrcSwizzleDesc::dimension > 1), TOperationResult &>
			{
				// make copy and call direct assignment
				return operator =<false>(vector<SrcElementType, SrcSwizzleDesc::dimension>(src));
			}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<typename SrcType>
#ifdef __GNUC__
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const SrcType &src) -> enable_if_t<IsScalar<SrcType>, TOperationResult &>
#else
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const SrcType &src) & -> enable_if_t<IsScalar<SrcType>, TOperationResult &>
#endif
			{
				const auto &scalar = ExtractScalar(src);
				for (unsigned idx = 0; idx < SwizzleDesc::dimension; idx++)
					(*this)[idx] = scalar;
				return *this;
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
#ifdef __GNUC__
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src)
#else
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &
#endif
				-> enable_if_t<(srcRows > 1 || srcColumns > 1), TOperationResult &>
			{
				// C++17\
				static_assert(sizeof...(WARHazard) <= 1);
				constexpr static const bool underflow = SwizzleDesc::dimension > srcRows * srcColumns, overflow = SwizzleDesc::dimension < srcRows * srcColumns;
				static_assert(!(underflow || overflow && SwizzleDesc::dimension > 1), "'vector = matrix': unmatched sequencing");
				const auto &seq = reinterpret_cast<const CSequencingSwizzle<SrcElementType, srcRows, srcColumns> &>(src.data);
				return operator =<WARHazard...>(seq);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
#ifdef __GNUC__
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(initializer_list<CInitListItem<ElementType, SwizzleDesc::dimension>> initList) -> TOperationResult &
#else
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(initializer_list<CInitListItem<ElementType, SwizzleDesc::dimension>> initList) & -> TOperationResult &
#endif
			{
				unsigned dstIdx = 0;
				for (const auto &item : initList)
					for (unsigned itemEementIdx = 0; itemEementIdx < item.GetItemSize(); itemEementIdx++)
						(*this)[dstIdx++] = item[itemEementIdx];
				assert(dstIdx == SwizzleDesc::dimension);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>>
			class EBCO CSwizzleCommon :
				public CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>,
				public Tag<is_same_v<SwizzleDesc, CVectorSwizzleDesc<columns>> ? TagName::Vector : TagName::Swizzle, SwizzleDesc::dimension == 1>
			{
			protected:
				CSwizzleCommon() = default;
				CSwizzleCommon(const CSwizzleCommon &) = default;
				~CSwizzleCommon() = default;
#ifdef __GNUC__
				CSwizzleCommon &operator =(const CSwizzleCommon &) = default;
#else
				CSwizzleCommon &operator =(const CSwizzleCommon &) & = default;
#endif

			public:
#if defined _MSC_VER && _MSC_VER <= 1900
				operator const ElementType &() const & noexcept
				{
					return (*this)[0];
				}

				operator const ElementType &() const && noexcept
				{
					return *this;
				}

				operator conditional_t<SwizzleDesc::isWriteMaskValid, ElementType, const ElementType> &() & noexcept
				{
					return (*this)[0];
				}
#else
				operator const ElementType &() const noexcept
				{
					return (*this)[0];
				}
#endif

				//operator const ElementType &() noexcept
				//{
				//	return operator ElementType &();
				//}

			private:
				template<size_t ...idx>
				inline auto Pos(index_sequence<idx...>) const
				{
					return vector<decay_t<decltype(+declval<ElementType>())>, SwizzleDesc::dimension>(+(*this)[idx]...);
				}

				template<size_t ...idx>
				inline auto Neg(index_sequence<idx...>) const
				{
					return vector<decay_t<decltype(-declval<ElementType>())>, SwizzleDesc::dimension>(-(*this)[idx]...);
				}

			public:
				auto operator +() const;
				auto operator -() const;

			public:
				template<typename F>
				vector<result_of_t<F &(ElementType)>, SwizzleDesc::dimension> apply(F f) const;

				template<typename TResult>
				vector<TResult, SwizzleDesc::dimension> apply(TResult f(ElementType)) const
				{
					return apply<TResult(ElementType)>(f);
				}

				template<typename TResult>
				vector<TResult, SwizzleDesc::dimension> apply(TResult f(const ElementType &)) const
				{
					return apply<TResult(const ElementType &)>(f);
				}

				vector<ElementType, SwizzleDesc::dimension> apply(ElementType f(ElementType)) const
				{
					return apply<ElementType>(f);
				}

				vector<ElementType, SwizzleDesc::dimension> apply(ElementType f(const ElementType &)) const
				{
					return apply<ElementType>(f);
				}
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator +() const
			{
				return Pos(std::make_index_sequence<SwizzleDesc::dimension>());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator -() const
			{
				return Neg(std::make_index_sequence<SwizzleDesc::dimension>());
			}

#ifdef MSVC_NAMESPACE_WORKAROUND
		}
#define NAMESPACE_PREFIX Impl::
#else
#define NAMESPACE_PREFIX
#endif
			// this specialization used as base class for CDataContainer to eliminate need for various overloads
			/*
			CVectorSwizzleDesc<vectorDimension> required for VS 2013/2015
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzle<ElementType, 0, vectorDimension, NAMESPACE_PREFIX CVectorSwizzleDesc<vectorDimension>> : public NAMESPACE_PREFIX CSwizzleCommon<ElementType, 0, vectorDimension>
			{
			protected:
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = default;
				~CSwizzle() = default;
#ifdef __GNUC__
				CSwizzle &operator =(const CSwizzle &) = default;
#else
				CSwizzle &operator =(const CSwizzle &) & = default;
#endif
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, typename>
			class CSwizzle final : public NAMESPACE_PREFIX CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>
			{
				friend class NAMESPACE_PREFIX CDataContainer<ElementType, rows, columns>;

			public:
#ifdef __GNUC__
				CSwizzle &operator =(const CSwizzle &) = default;
#else
				CSwizzle &operator =(const CSwizzle &) & = default;
#endif
				using NAMESPACE_PREFIX CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =;

			private:
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = delete;
				~CSwizzle() = default;
			};

#undef NAMESPACE_PREFIX
#ifdef MSVC_NAMESPACE_WORKAROUND
		namespace Impl
		{
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleIteratorImpl : public iterator<forward_iterator_tag, const ElementType>
			{
				const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle;
				unsigned i;

			protected:
				CSwizzleIteratorImpl(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle, unsigned i) :
					swizzle(swizzle), i(i) {}

				~CSwizzleIteratorImpl() = default;

			public:	// required by stl => public
				conditional_t
				<
					sizeof(typename CSwizzleIteratorImpl::value_type) <= sizeof(void *),
					typename CSwizzleIteratorImpl::value_type,
					typename CSwizzleIteratorImpl::reference
				> operator *() const
				{
					return swizzle[i];
				}

				typename CSwizzleIteratorImpl::pointer operator ->() const
				{
					return &swizzle[i];
				}

				bool operator ==(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc> src) const noexcept
				{
					assert(&swizzle == &src.swizzle);
					return i == src.i;
				}

				bool operator !=(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc> src) const noexcept
				{
					assert(&swizzle == &src.swizzle);
					return i != src.i;
				}
#ifdef __GNUC__
				CSwizzleIteratorImpl &operator ++()
#else
				CSwizzleIteratorImpl &operator ++() &
#endif
				{
					++i;
					return *this;
				}

#ifdef __GNUC__
				CSwizzleIteratorImpl operator ++(int)
#else
				CSwizzleIteratorImpl operator ++(int) &
#endif
				{
					CSwizzleIteratorImpl old(*this);
					operator ++();
					return old;
				}
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class CSwizzleIterator final : public CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc>
			{
				friend bool VectorMath::all<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);
				friend bool VectorMath::any<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);
				friend bool VectorMath::none<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

				using CSwizzleIteratorImpl::CSwizzleIteratorImpl;
				// copy ctor required by stl => public
				~CSwizzleIterator() = default;
			};

			template<class Src>
			struct _1xN_2_vec_impl
			{
				typedef Src type;
			};

#if !DISABLE_MATRIX_DECAY
			template<typename ElementType, unsigned int dimension>
			struct _1xN_2_vec_impl<matrix<ElementType, 1, dimension>>
			{
				typedef vector<ElementType, dimension> type;
			};
#endif

			template<class Src>
			struct vec_2_1xN_impl
			{
				typedef Src type;
			};

			template<typename ElementType, unsigned int dimension>
			struct vec_2_1xN_impl<vector<ElementType, dimension>>
			{
				typedef matrix<ElementType, 1, dimension> type;
			};

			template<class Src, typename LeftElementType, typename RightElementType, bool force>
			using _1xN_2_vec = conditional_t<force || !is_same_v<decay_t<LeftElementType>, decay_t<RightElementType>>, typename _1xN_2_vec_impl<Src>::type, Src>;

			template<class Src>
			using vec_2_1xN = typename vec_2_1xN_impl<Src>::type;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec
			>
			class MatrixOpMatrixResultImpl
			{
#if defined _MSC_VER && _MSC_VER <= 1900
				constexpr static const unsigned int lr = leftRows, lc = leftColumns, rr = rightRows, rc = rightColumns;
#endif
				constexpr static const bool dimensionalMismatch =
#if ENABLE_UNMATCHED_MATRICES
					false;
#else
#if defined _MSC_VER && _MSC_VER <= 1900
#else
					!(leftRows <= rightRows && leftColumns <= rightColumns || leftRows >= rightRows && leftColumns >= rightColumns);
#endif
					!(lr <= rr && lc <= rc || lr >= rr && lc >= rc);
#endif
#if defined _MSC_VER && _MSC_VER <= 1900
				typedef matrix<TargetElementType, std::min(lr, rr), std::min(lc, rc)> ResultMatrix;
#else
				typedef matrix<TargetElementType, std::min(leftRows, rightRows), std::min(leftColumns, rightColumns)> ResultMatrix;
#endif
				typedef vector<typename ResultMatrix::ElementType, ResultMatrix::columns> ResultVector;
				typedef _1xN_2_vec<ResultMatrix, LeftElementType, RightElementType, force_1xN_2_vec> Result;
				constexpr static const bool vecMatMismatch = is_same_v<_1xN_2_vec<ResultMatrix, LeftElementType, RightElementType, false>, ResultVector> && (leftRows > 1 || rightRows > 1);

			public:
				typedef conditional_t<dimensionalMismatch, nullptr_t, conditional_t<vecMatMismatch, void, Result>> type;
			};

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec = false
			>
			using MatrixOpMatrixResult = typename MatrixOpMatrixResultImpl<LeftElementType, leftRows, leftColumns, RightElementType, rightRows, rightColumns, TargetElementType, force_1xN_2_vec>::type;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightType,
				typename TargetElementType, bool force_1xN_2_vec = false
			>
			using MatrixOpScalarResult = _1xN_2_vec<matrix<TargetElementType, leftRows, leftColumns>, LeftElementType, decltype(ExtractScalar(declval<RightType>())), force_1xN_2_vec>;

			template
			<
				typename LeftType,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec = false
			>
			using ScalarOpMatrixResult = conditional_t<rightRows * rightColumns == 1,
				vector<decay_t<decltype(ExtractScalar(declval<LeftType>()))>, 1>,
				_1xN_2_vec<matrix<TargetElementType, rightRows, rightColumns>, decltype(ExtractScalar(declval<LeftType>())), RightElementType, force_1xN_2_vec>>;

			template
			<
				typename LeftElementType, unsigned int leftDimension,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec = false
			>
			using SwizzleOpMatrixResult = conditional_t<leftDimension <= rightRows * rightColumns,
				vector<TargetElementType, leftDimension>,
				_1xN_2_vec<matrix<TargetElementType, rightRows, rightColumns>, LeftElementType, RightElementType, force_1xN_2_vec>>;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightDimension,
				typename TargetElementType, bool force_1xN_2_vec = false
			>
			using MatrixOpSwizzleResult = conditional_t<leftRows * leftColumns <= rightDimension,
				_1xN_2_vec<matrix<TargetElementType, leftRows, leftColumns>, LeftElementType, RightElementType, force_1xN_2_vec>,
				vector<TargetElementType, rightDimension>>;
		}

#		pragma region generate operators
#			pragma region swizzle
				// swizzle / 1D swizzle op=<!WARHazard> swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<!WARHazard && (RightSwizzleDesc::dimension > 1),															\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						static_assert(LeftSwizzleDesc::isWriteMaskValid, "'vector "#op"= vector': invalid write mask");									\
						static_assert(LeftSwizzleDesc::dimension <= RightSwizzleDesc::dimension, "'vector "#op"=' vector: too small src dimension");	\
						assert(!Impl::TriggerWARHazard<false>(left, right));																			\
						for (unsigned i = 0; i < LeftSwizzleDesc::dimension; i++)																		\
							left[i] op##= right[i];																										\
						return left;																													\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op=<WARHazard> swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<WARHazard && (RightSwizzleDesc::dimension > 1),																\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						return operator op##=<false>(left, vector<RightElementType, RightSwizzleDesc::dimension>(right));								\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1),																			\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						static constexpr auto WARHazard = Impl::DetectSwizzleWARHazard																	\
						<																																\
							LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																	\
							RightElementType, rightRows, rightColumns, RightSwizzleDesc,																\
							false																														\
						>::value;																														\
						return operator op##=<WARHazard>(left, right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1),																			\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)											\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				namespace Impl::ScalarOps
				{
					// swizzle / 1D swizzle op=<!WARHazard, !extractScalar> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar,																							\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<!WARHazard && !extractScalar/* && IsScalar<RightType>*/,														\
						typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(					\
						CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
						const RightType &right)																											\
						{																																\
							static_assert(LeftSwizzleDesc::isWriteMaskValid, "'vector "#op"= scalar': invalid write mask");								\
							assert(!TriggerScalarWARHazard(left, &right));																				\
							for (unsigned i = 0; i < LeftSwizzleDesc::dimension; i++)																	\
								left[i] op##= right;																									\
							return left;																												\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#					undef OPERATOR_DEFINITION

					// swizzle / 1D swizzle op=<WARHazard, !extractScalar> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar,																							\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<WARHazard && !extractScalar/* && IsScalar<RightType>*/,														\
						typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(					\
						CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
						const RightType &right)																											\
						{																																\
							return operator op##=<false, false>(left, RightType(right));																\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#					undef OPERATOR_DEFINITION

					// swizzle / 1D swizzle op=<?WARHazard, extractScalar> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar = true,																					\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<extractScalar/* && IsScalar<RightType>*/,																	\
						typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(					\
						CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
						const RightType &right)																											\
						{																																\
							return operator op##=<WARHazard, false>(left, ExtractScalar(right));														\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#					undef OPERATOR_DEFINITION
				}

#if defined _MSC_VER && _MSC_VER <= 1900
				//	workaround for ICE on VS 2015\
					this implementation does not allow users to explicitly specify WAR hazard\
					TODO: try with newer version or x64
				namespace Workaround = Impl::ScalarOps;
#else
				namespace Workaround = VectorMath;
				// swizzle / 1D swizzle op=<?WARHazard> scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<Impl::IsScalar<RightType>,																					\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const RightType &right)																												\
					{																																	\
						return Impl::ScalarOps::operator op##=<WARHazard>(left, right);																	\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION
#endif

				// swizzle / 1D swizzle op= scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<Impl::IsScalar<RightType>,																					\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const RightType &right)																												\
					{																																	\
						static constexpr bool WARHazard =																								\
							Impl::DetectScalarWARHazard<Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>, RightType>::value;		\
						return Workaround::operator op##=<WARHazard>(left, right);																		\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<Impl::IsScalar<RightType>,																					\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const RightType &&right)																											\
					{																																	\
						return Workaround::operator op##=<false>(left, right);																			\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op swizzle / 1D swizzle op 1D swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 == RightSwizzleDesc::dimension > 1),										\
					vector																																\
					<																																	\
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>,									\
						std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension)																\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						decltype(left op right) result(left);																							\
						return result op##= std::move(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op swizzle / 1D swizzle op 1D swizzle -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 == RightSwizzleDesc::dimension > 1),										\
					vector																																\
					<																																	\
						bool,																															\
						std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension)																\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						decltype(left op right) result;																									\
						for (unsigned i = 0; i < decltype(result)::dimension; i++)																		\
							result[i] = left[i] op right[i];																							\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op scalar / 1D swizle op pure scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 ? Impl::IsScalar<RightType> : Impl::IsPureScalar<RightType>),				\
					vector																																\
					<																																	\
						std::decay_t<decltype(std::declval<LeftElementType>() op Impl::ExtractScalar(std::declval<RightType>()))>,						\
						LeftSwizzleDesc::dimension																										\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const RightType &right)																												\
					{																																	\
						decltype(left op right) result(left);																							\
						return Workaround::operator op##=<false>(result, right);																		\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op scalar / 1D swizle op pure scalar -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 ? Impl::IsScalar<RightType> : Impl::IsPureScalar<RightType>),				\
					vector																																\
					<																																	\
						bool,																															\
						LeftSwizzleDesc::dimension																										\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const RightType &right)																												\
					{																																	\
						decltype(left op right) result;																									\
						const auto &scalar = Impl::ExtractScalar(right);																				\
						for (unsigned i = 0; i < decltype(result)::dimension; i++)																		\
							result[i] = left[i] op scalar;																								\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// scalar op swizzle / pure scalar op 1D swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1 ? Impl::IsScalar<LeftType> : Impl::IsPureScalar<LeftType>),				\
					vector																																\
					<																																	\
						std::decay_t<decltype(Impl::ExtractScalar(std::declval<LeftType>()) op std::declval<RightElementType>())>,						\
						RightSwizzleDesc::dimension																										\
					>> operator op(																														\
					const LeftType &left,																												\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						decltype(left op right) result(left);																							\
						return operator op##=<false>(result, right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// scalar op swizzle / pure scalar op 1D swizzle -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1 ? Impl::IsScalar<LeftType> : Impl::IsPureScalar<LeftType>),				\
					vector																																\
					<																																	\
						bool,																															\
						RightSwizzleDesc::dimension																										\
					>> operator op(																														\
					const LeftType &left,																												\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						decltype(left op right) result;																									\
						const auto &scalar = Impl::ExtractScalar(left);																					\
						for (unsigned i = 0; i < decltype(result)::dimension; i++)																		\
							result[i] = scalar op right[i];																								\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION
#			pragma endregion

#			pragma region matrix
				// matrix / 1x1 matrix op= matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline auto operator op##=(																											\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					-> std::enable_if_t<(rightRows > 1 || rightColumns > 1), decltype(left)>															\
					{																																	\
						static_assert(leftRows <= rightRows, "'matrix "#op"= matrix': too few rows in src");											\
						static_assert(leftColumns <= rightColumns, "'matrix "#op"= matrix': too few columns in src");									\
						constexpr static const bool vecMatMismatch = std::is_void_v<Impl::MatrixOpMatrixResult<											\
							LeftElementType, leftRows, leftColumns,																						\
							RightElementType, rightRows, rightColumns,																					\
							LeftElementType>>;																											\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op"= matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op"= matrix1xN -> vectorN': cannot convert matrix to vector");	\
						for (unsigned r = 0; r < leftRows; r++)																							\
							operator op##=<false>(left[r], right[r]);																					\
						return left;																													\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

#if defined _MSC_VER && _MSC_VER <= 1900
				namespace Impl::ScalarOps
				{
					// matrix / 1x1 matrix op=<?WARHazard> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard,																												\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,													\
							typename RightType																											\
						>																																\
						inline decltype(auto) operator op##=(																							\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const RightType &right)																											\
						{																																\
							return left.operator op##=<WARHazard>(right);																				\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#					undef OPERATOR_DEFINITION
				}
#else
				// matrix / 1x1 matrix op=<?WARHazard> scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					friend inline auto Workaround::operator op##=(																						\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const RightType &right)																												\
					-> std::enable_if_t<Impl::IsScalar<RightType>, decltype(left)>																		\
					{																																	\
						return left.operator op##=<WARHazard>(right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION
#endif

				// matrix / 1x1 matrix op= scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					inline auto operator op##=(																											\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const RightType &right)																												\
					-> std::enable_if_t<Impl::IsScalar<RightType>, decltype(left)>																		\
					{																																	\
						static constexpr bool WARHazard =																								\
							Impl::DetectScalarWARHazard<matrix<LeftElementType, leftRows, leftColumns>, RightType>::value;								\
						return Workaround::operator op##=<WARHazard>(left, right);																		\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= temp scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					inline auto operator op##=(																											\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const RightType &&right)																											\
					-> std::enable_if_t<Impl::IsScalar<RightType>, decltype(left)>																		\
					{																																	\
						return Workaround::operator op##=<false>(left, right);																			\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op matrix / 1x1 matrix op 1x1 matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(leftRows > 1 || leftColumns > 1) == (rightRows > 1 || rightColumns > 1),									\
					Impl::MatrixOpMatrixResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightElementType, rightRows, rightColumns,																						\
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>										\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						typedef decltype(left op right) Result;																							\
						constexpr static const bool vecMatMismatch = std::is_void_v<Result>;															\
						static_assert(!std::is_null_pointer_v<Result>, "'matrix "#op" matrix': mismatched matrix dimensions");							\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op" matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op" matrix1xN -> vectorN': cannot convert matrix to vector");		\
						Result result(left);																											\
						return result op##= std::move(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op matrix / 1x1 matrix op 1x1 matrix -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(leftRows > 1 || leftColumns > 1) == (rightRows > 1 || rightColumns > 1),									\
					Impl::MatrixOpMatrixResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightElementType, rightRows, rightColumns,																						\
						bool, true																														\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						typedef Impl::vec_2_1xN<decltype(left op right)> MatrixResult;																	\
						constexpr static const bool vecMatMismatch = std::is_void_v<MatrixResult>;														\
						static_assert(!std::is_null_pointer_v<MatrixResult>, "'matrix "#op" matrix': mismatched matrix dimensions");					\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op" matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op" matrix1xN -> vectorN': cannot convert matrix to vector");		\
						MatrixResult result;																											\
						for (unsigned r = 0; r < MatrixResult::rows; r++)																				\
							result[r] = left[r] op right[r];																							\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op scalar / 1x1 matrix op pure scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<(leftRows > 1 || leftColumns > 1 ? Impl::IsScalar<RightType> : Impl::IsPureScalar<RightType>),				\
					Impl::MatrixOpScalarResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightType,																														\
						std::decay_t<decltype(std::declval<LeftElementType>() op Impl::ExtractScalar(std::declval<RightType>()))>						\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const RightType &right)																												\
					{																																	\
						decltype(left op right) result(left);																							\
						return result op##= std::move(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op scalar / 1x1 matrix op pure scalar -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					inline std::enable_if_t<(leftRows > 1 || leftColumns > 1 ? Impl::IsScalar<RightType> : Impl::IsPureScalar<RightType>),				\
					Impl::MatrixOpScalarResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightType,																														\
						bool, true																														\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const RightType &right)																												\
					{																																	\
						typedef Impl::vec_2_1xN<decltype(left op right)> MatrixResult;																	\
						MatrixResult result;																											\
						for (unsigned r = 0; r < MatrixResult::rows; r++)																				\
							result[r] = left[r] op right;																								\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// scalar op matrix / pure scalar op 1x1 matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(rightRows > 1 || rightColumns > 1 ? Impl::IsScalar<LeftType> : Impl::IsPureScalar<LeftType>),				\
					Impl::ScalarOpMatrixResult																											\
					<																																	\
						LeftType,																														\
						RightElementType, rightRows, rightColumns,																						\
						std::decay_t<decltype(Impl::ExtractScalar(std::declval<LeftType>()) op std::declval<RightElementType>())>						\
					>> operator op(																														\
					const LeftType &left,																												\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						decltype(left op right) result(left);																							\
						return result op##= right;																										\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// scalar op matrix / pure scalar op 1x1 matrix -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(rightRows > 1 || rightColumns > 1 ? Impl::IsScalar<LeftType> : Impl::IsPureScalar<LeftType>),				\
					Impl::ScalarOpMatrixResult																											\
					<																																	\
						LeftType,																														\
						RightElementType, rightRows, rightColumns,																						\
						bool, true																														\
					>> operator op(																														\
					const LeftType &left,																												\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						typedef Impl::vec_2_1xN<decltype(left op right)> MatrixResult;																	\
						MatrixResult result;																											\
						for (unsigned r = 0; r < MatrixResult::rows; r++)																				\
							result[r] = left op right[r];																								\
						return result;																													\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#			undef OPERATOR_DEFINITION
#			pragma endregion

#			pragma region sequencing
				// swizzle / 1D swizzle op= matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool ...WARHazard,																												\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(rightRows > 1 || rightColumns > 1),																		\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						/* C++17 static_assert(sizeof...(WARHazard) <= 1);*/																			\
						constexpr static const bool																										\
							underflow = LeftSwizzleDesc::dimension > rightRows * rightColumns,															\
							overflow = LeftSwizzleDesc::dimension < rightRows * rightColumns;															\
						static_assert(!(underflow || overflow && LeftSwizzleDesc::dimension > 1), "'vector "#op"= matrix': unmatched sequencing");		\
						const auto &seq = reinterpret_cast<const Impl::CSequencingSwizzle<RightElementType, rightRows, rightColumns> &>(right.data);	\
						return left.operator =<WARHazard...>(seq);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(rightRows > 1 || rightColumns > 1),																		\
					typename Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(				\
					Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
					const matrix<RightElementType, rightRows, rightColumns> &&right)																	\
					{																																	\
						return left.operator =<false>(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool ...WARHazard,																												\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline auto operator op##=(																											\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					-> std::enable_if_t<(RightSwizzleDesc::dimension > 1), decltype(left)>																\
					{																																	\
						/* C++17 static_assert(sizeof...(WARHazard) <= 1);*/																			\
						constexpr static const auto leftDimension = leftRows * leftColumns;																\
						constexpr static const bool																										\
							underflow = leftDimension > RightSwizzleDesc::dimension,																	\
							overflow = leftDimension < RightSwizzleDesc::dimension;																		\
						static_assert(!(underflow || overflow && leftDimension > 1), "'matrix "#op"= vector': unmatched sequencing");					\
						auto &seq = reinterpret_cast<Impl::CSequencingSwizzle<LeftElementType, leftRows, leftColumns> &>(left.data);					\
						seq.operator =<WARHazard...>(right);																							\
						return left;																													\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= temp swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline auto operator op##=(																											\
					matrix<LeftElementType, leftRows, leftColumns> &left,																				\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)											\
					-> std::enable_if_t<(RightSwizzleDesc::dimension > 1), decltype(left)>																\
					{																																	\
						return left.operator =<false>(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op matrix / 1D swizzle op 1x1 matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 == (rightRows > 1 || rightColumns > 1)),									\
					Impl::SwizzleOpMatrixResult																											\
					<																																	\
						LeftElementType, LeftSwizzleDesc::dimension,																					\
						RightElementType, rightRows, rightColumns,																						\
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>										\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						constexpr static const bool matched = LeftSwizzleDesc::dimension == rightRows * rightColumns;									\
						static_assert(matched || rightRows == 1 || rightColumns == 1, "'vector "#op" matrix': unmatched sequencing");					\
						decltype(left op right) result(left);																							\
						return result op##= std::move(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// swizzle op matrix / 1D swizzle op 1x1 matrix -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 == (rightRows > 1 || rightColumns > 1)),									\
					Impl::SwizzleOpMatrixResult																											\
					<																																	\
						LeftElementType, LeftSwizzleDesc::dimension,																					\
						RightElementType, rightRows, rightColumns,																						\
						bool, true																														\
					>> operator op(																														\
					const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
					const matrix<RightElementType, rightRows, rightColumns> &right)																		\
					{																																	\
						constexpr static const bool matched = LeftSwizzleDesc::dimension == rightRows * rightColumns;									\
						static_assert(matched || rightRows == 1 || rightColumns == 1, "'vector "#op" matrix': unmatched sequencing");					\
						const auto &seq = reinterpret_cast<const Impl::CSequencingSwizzle<RightElementType, rightRows, rightColumns> &>(right.data);	\
						return left op seq;																												\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op swizzle / 1x1 matrix op 1D swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<((leftRows > 1 || leftColumns > 1) == RightSwizzleDesc::dimension > 1),										\
					Impl::MatrixOpSwizzleResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightElementType, RightSwizzleDesc::dimension,																					\
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>										\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						constexpr static const bool matched = leftRows * leftColumns == RightSwizzleDesc::dimension;									\
						static_assert(matched || leftRows == 1 || leftColumns == 1, "'matrix "#op" vector': unmatched sequencing");						\
						decltype(left op right) result(left);																							\
						return result op##= std::move(right);																							\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION

				// matrix op swizzle / 1x1 matrix op 1D swizzle -> bool
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<((leftRows > 1 || leftColumns > 1) == RightSwizzleDesc::dimension > 1),										\
					Impl::MatrixOpSwizzleResult																											\
					<																																	\
						LeftElementType, leftRows, leftColumns,																							\
						RightElementType, RightSwizzleDesc::dimension,																					\
						bool, true																														\
					>> operator op(																														\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																			\
					const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)											\
					{																																	\
						constexpr static const bool matched = leftRows * leftColumns == RightSwizzleDesc::dimension;									\
						static_assert(matched || leftRows == 1 || leftColumns == 1, "'matrix "#op" vector': unmatched sequencing");						\
						const auto &seq = reinterpret_cast<const Impl::CSequencingSwizzle<LeftElementType, leftRows, leftColumns> &>(left.data);		\
						return seq op right;																											\
					}
				GENERATE_REL_OPERATORS(OPERATOR_DEFINITION)
#				undef OPERATOR_DEFINITION
#			pragma endregion
#		pragma endregion

		template<typename ElementType_, unsigned int dimension_>
		class EBCO vector : public Impl::CDataContainer<ElementType_, 0, dimension_>, public Impl::CSwizzle<ElementType_, 0, dimension_>
		{
			static_assert(dimension_ > 0, "vector dimension should be positive");
			static_assert(Impl::IsElementTypeValid<ElementType_>, "invalid vector element type");
			typedef Impl::CDataContainer<ElementType_, 0, dimension_> DataContainer;
			typedef Impl::CData<ElementType_, 0, dimension_> Data;
			//using CDataContainer<ElementType_, 0, dimension_>::data;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int dimension = dimension_;

			vector() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			vector(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<typename SrcType, typename = std::enable_if_t<Impl::IsScalar<SrcType>>>
			vector(const SrcType &scalar);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, typename = std::enable_if_t<(srcRows > 1 || srcColumns > 1)>>
			vector(const matrix<SrcElementType, srcRows, srcColumns> &src);

#if defined _MSC_VER && _MSC_VER <= 1900
			template<typename First, typename Second, typename ...Rest>
			vector(const First &first, const Second &second, const Rest &...rest);
#else
			template<typename ...Args, typename = std::enable_if_t<(sizeof...(Args) > 1)>>
			vector(const Args &...args);
#endif

			vector(std::initializer_list<Impl::CInitListItem<ElementType, dimension>> initList);

			//template<typename TIterator>
			//explicit vector(TIterator src);

			template<typename SrcElementType, unsigned int srcDimension>
			vector(const SrcElementType (&src)[srcDimension]);

#ifdef __GNUC__
			vector &operator =(const vector &) = default;
#else
			vector &operator =(const vector &) & = default;
#endif
			using Impl::CSwizzleAssign<ElementType, 0, dimension>::operator =;

		private:
			template<typename ElementType, unsigned int rows, unsigned int columns>
			friend class Impl::CData;

			// workaround for bug in VS 2015 Update 2
			typedef std::make_index_sequence<dimension> IdxSeq;

			template<typename Data::InitType initType, class IdxSeq, unsigned offset, typename ...Args>
#if defined _MSC_VER && _MSC_VER <= 1900
			inline vector(typename Data::template InitTag<initType, IdxSeq, offset> tag, const Args &...args) :
				DataContainer(tag, args...) {}
#else
			vector(typename Data::template InitTag<initType, IdxSeq, offset>, const Args &...args);
#endif
		};

		template<typename ElementType_, unsigned int rows_, unsigned int columns_>
		class EBCO matrix : public Impl::CDataContainer<ElementType_, rows_, columns_>, public Impl::Tag<Impl::TagName::Matrix, rows_ == 1 && columns_ == 1>
		{
			static_assert(rows_ > 0, "matrix should contain at leat 1 row");
			static_assert(columns_ > 0, "matrix should contain at leat 1 column");
			static_assert(Impl::IsElementTypeValid<ElementType_>, "invalid matrix element type");
			friend bool VectorMath::all<>(const matrix<ElementType_, rows_, columns_> &);
			friend bool VectorMath::any<>(const matrix<ElementType_, rows_, columns_> &);
			friend bool VectorMath::none<>(const matrix<ElementType_, rows_, columns_> &);
			typedef vector<ElementType_, columns_> TRow;
			typedef Impl::CDataContainer<ElementType_, rows_, columns_> DataContainer;
			typedef Impl::CData<ElementType_, rows_, columns_> Data;
			typedef std::make_index_sequence<rows_> IdxSeq;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int rows = rows_, columns = columns_;

			matrix() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const matrix<SrcElementType, srcRows, srcColumns> &src);

			template<typename SrcType, typename = std::enable_if_t<Impl::IsScalar<SrcType>>>
			matrix(const SrcType &scalar);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, typename = std::enable_if_t<(SrcSwizzleDesc::dimension > 1)>>
			matrix(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

#if defined _MSC_VER && _MSC_VER <= 1900
			template<typename First, typename Second, typename ...Rest>
			matrix(const First &first, const Second &second, const Rest &...rest);
#else
			template<typename ...Args, typename = std::enable_if_t<(sizeof...(Args) > 1)>>
			matrix(const Args &...args);
#endif

			matrix(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList);

			//template<typename TIterator>
			//explicit matrix(TIterator src);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const SrcElementType (&src)[srcRows][srcColumns]);

#ifdef __GNUC__
			matrix &operator =(const matrix &) = default;
#else
			matrix &operator =(const matrix &) & = default;
#endif

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
#ifdef __GNUC__
			std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src);
#else
			std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &;
#endif

			template<typename SrcType>
#ifdef __GNUC__
			std::enable_if_t<Impl::IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar);
#elif defined _MSC_VER && _MSC_VER <= 1900
			std::enable_if_t<Impl::IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar) &
			{
				for (unsigned r = 0; r < rows; r++)
					operator [](r) = scalar;
				return *this;
			}
#else
			std::enable_if_t<Impl::IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar) &;
#endif

			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);
#else
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;
#endif

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src);
#else
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &;
#endif

#ifdef __GNUC__
			matrix &operator =(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList);
#else
			matrix &operator =(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList) &;
#endif

			auto operator +() const;
			auto operator -() const;

		private:
			// matrix / 1x1 matrix op=<!WARHazard, !extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar, typename SrcType>							\
				std::enable_if_t<!WARHazard && !extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>	\
				operator op##=(const SrcType &scalar);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op=<WARHazard, !extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar, typename SrcType>							\
				std::enable_if_t<WARHazard && !extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>	\
				operator op##=(const SrcType &scalar);
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op=<?WARHazard, extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar = true, typename SrcType>					\
				std::enable_if_t<extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>				\
				operator op##=(const SrcType &scalar);
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION)
#			undef OPERATOR_DECLARATION

#if defined _MSC_VER && _MSC_VER <= 1900
			// matrix / 1x1 matrix op=<?WARHazard> scalar
#			define OPERATOR_DECLARATION(op)																\
				template																				\
				<																						\
					bool WARHazard,																		\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,			\
					typename RightType																	\
				>																						\
				friend inline decltype(auto) Impl::ScalarOps::operator op##=(							\
				matrix<LeftElementType, leftRows, leftColumns> &left,									\
				const RightType &right);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION)
#			undef OPERATOR_DECLARATION
#else
			// matrix / 1x1 matrix op=<?WARHazard> scalar
#			define OPERATOR_DECLARATION(op)																\
				template																				\
				<																						\
					bool WARHazard,																		\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,			\
					typename RightType																	\
				>																						\
				friend inline auto Workaround::operator op##=(											\
				matrix<LeftElementType, leftRows, leftColumns> &left,									\
				const RightType &right)																	\
				-> std::enable_if_t<Impl::IsScalar<RightType>, decltype(left)>;
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION)
#			undef OPERATOR_DECLARATION
#endif

		public:
#ifdef __GNUC__
			const TRow &operator [](unsigned int idx) const noexcept;
#else
			const TRow &operator [](unsigned int idx) const & noexcept, &operator [](unsigned int idx) const && noexcept;
#endif
#ifdef __GNUC__
			TRow &operator [](unsigned int idx) noexcept;
#else
			TRow &operator [](unsigned int idx) & noexcept;
#endif

			operator const ElementType &() const noexcept;
#ifdef __GNUC__
			operator ElementType &() noexcept;
#else
			operator ElementType &() & noexcept;
#endif

		public:
			template<typename F>
			matrix<std::result_of_t<F &(ElementType)>, rows, columns> apply(F f) const;

			template<typename TResult>
			matrix<TResult, rows, columns> apply(TResult f(ElementType)) const
			{
				return apply<TResult (ElementType)>(f);
			}

			template<typename TResult>
			matrix<TResult, rows, columns> apply(TResult f(const ElementType &)) const
			{
				return apply<TResult (const ElementType &)>(f);
			}

			matrix<ElementType, rows, columns> apply(ElementType f(ElementType)) const
			{
				return apply<ElementType>(f);
			}

			matrix<ElementType, rows, columns> apply(ElementType f(const ElementType &)) const
			{
				return apply<ElementType>(f);
			}

		private:
			template<size_t ...idx>
			inline auto Pos(std::index_sequence<idx...>) const;

			template<size_t ...idx>
			inline auto Neg(std::index_sequence<idx...>) const;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename F>
		inline vector<std::result_of_t<F &(ElementType)>, SwizzleDesc::dimension>
		Impl::CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>::apply(F f) const
		{
			constexpr unsigned int dimension = SwizzleDesc::dimension;
			vector<std::result_of_t<F &(ElementType)>, dimension> result;
			for (unsigned i = 0; i < dimension; i++)
				result[i] = f((*this)[i]);
			return result;
		}

#if defined _MSC_VER && _MSC_VER <= 1900 && !_DEBUG
#	define ELEMENTS_COUNT_PREFIX Impl
#else
#	define ELEMENTS_COUNT_PREFIX Data
#endif

#		pragma region vector impl
			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline vector<ElementType, dimension>::vector(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(Data::InitTag<Data::InitType::Copy, IdxSeq>(), src)
			{
				static_assert(dimension <= SrcSwizzleDesc::dimension, "\"copy\" ctor: too small src dimension");
			}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcType, typename = std::enable_if_t<Impl::IsScalar<SrcType>>>
			inline vector<ElementType, dimension>::vector(const SrcType &scalar) :
				DataContainer(Data::InitTag<Data::InitType::Scalar, IdxSeq>(), Impl::ExtractScalar(scalar)) {}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, typename = std::enable_if_t<(srcRows > 1 || srcColumns > 1)>>
			inline vector<ElementType, dimension>::vector(const matrix<SrcElementType, srcRows, srcColumns> &src) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), src)
			{
				constexpr bool checkOverflow = dimension > 1 && srcRows > 1 && srcColumns > 1;
				static_assert(srcRows * srcColumns >= dimension, "sequencing ctor: too few src elements");
				static_assert(srcRows * srcColumns <= dimension || !checkOverflow, "sequencing ctor: too many src elements");
			}

#if defined _MSC_VER && _MSC_VER <= 1900
			template<typename ElementType, unsigned int dimension>
			template<typename First, typename Second, typename ...Rest>
			inline vector<ElementType, dimension>::vector(const First &first, const Second &second, const Rest &...rest) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), first, second, rest...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX::ElementsCount<const First &, const Second, const Rest &...>;
				static_assert(srcElements >= dimension, "sequencing ctor: too few src elements");
				static_assert(srcElements <= dimension, "sequencing ctor: too many src elements");
			}
#else
			template<typename ElementType, unsigned int dimension>
			template<typename ...Args, typename = std::enable_if_t<(sizeof...(Args) > 1)>>
			inline vector<ElementType, dimension>::vector(const Args &...args) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), args...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX::ElementsCount<const Args &...>;
				static_assert(srcElements >= dimension, "sequencing ctor: too few src elements");
				static_assert(srcElements <= dimension, "sequencing ctor: too many src elements");
			}
#endif

			//template<typename ElementType, unsigned int dimension>
			//template<typename TIterator>
			//inline vector<ElementType, dimension>::vector(TIterator src)
			//{
			//	std::copy_n(src, dimension, DataContainer::data.data);
			//}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcDimension>
			inline vector<ElementType, dimension>::vector(const SrcElementType (&src)[srcDimension]) :
				DataContainer(Data::InitTag<Data::InitType::Array, IdxSeq>(), src)
			{
				static_assert(dimension <= srcDimension, "array ctor: too small src dimension");
			}

			template<typename ElementType, unsigned int dimension>
			inline vector<ElementType, dimension>::vector(std::initializer_list<Impl::CInitListItem<ElementType, dimension>> initList)
			{
				operator =(initList);
			}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			template<typename ElementType, unsigned int dimension>
			template<typename vector<ElementType, dimension>::Data::InitType initType, class IdxSeq, unsigned offset, typename ...Args>
			inline vector<ElementType, dimension>::vector(typename Data::template InitTag<initType, IdxSeq, offset> tag, const Args &...args) :
				DataContainer(tag, args...) {}
#endif
#		pragma endregion

#		pragma region matrix impl
			template<typename ElementType, unsigned int rows, unsigned int columns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const noexcept -> const typename matrix::TRow &
#else
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const & noexcept -> const typename matrix::TRow &
#endif
			{
				assert(idx < rows);
				return DataContainer::data.rowsData[idx];
			}

#ifndef __GNUC__
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const && noexcept -> const typename matrix::TRow &
			{
				return operator [](idx);
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) noexcept -> typename matrix::TRow &
#else
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) & noexcept -> typename matrix::TRow &
#endif
			{
				assert(idx < rows);
				return DataContainer::data.rowsData[idx];
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline matrix<ElementType, rows, columns>::operator const ElementType &() const noexcept
			{
				return operator [](0);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
#ifdef __GNUC__
			inline matrix<ElementType, rows, columns>::operator ElementType &() noexcept
#else
			inline matrix<ElementType, rows, columns>::operator ElementType &() & noexcept
#endif
			{
				return operator [](0);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const matrix<SrcElementType, srcRows, srcColumns> &src) :
				DataContainer(Data::InitTag<Data::InitType::Copy, IdxSeq>(), src)
			{
				static_assert(rows <= srcRows, "\"copy\" ctor: too few rows in src");
				static_assert(columns <= srcColumns, "\"copy\" ctor: too few columns in src");
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcType, typename = std::enable_if_t<Impl::IsScalar<SrcType>>>
			inline matrix<ElementType, rows, columns>::matrix(const SrcType &scalar) :
				DataContainer(Data::InitTag<Data::InitType::Scalar, IdxSeq>(), scalar) {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, typename = std::enable_if_t<(SrcSwizzleDesc::dimension > 1)>>
			inline matrix<ElementType, rows, columns>::matrix(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), src)
			{
				constexpr bool checkOverflow = rows > 1 && columns > 1;
				constexpr auto srcElements = SrcSwizzleDesc::dimension;
				static_assert(srcElements >= rows * columns, "sequencing ctor: too few src elements");
				static_assert(srcElements <= rows * columns || !checkOverflow, "sequencing ctor: too many src elements");
			}

#if defined _MSC_VER && _MSC_VER <= 1900
			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename First, typename Second, typename ...Rest>
			inline matrix<ElementType, rows, columns>::matrix(const First &first, const Second &second, const Rest &...rest) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), first, second, rest...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX::ElementsCount<const First &, const Second &, const Rest &...>;
				static_assert(srcElements >= rows * columns, "sequencing ctor: too few src elements");
				static_assert(srcElements <= rows * columns, "sequencing ctor: too many src elements");
			}
#else
			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename ...Args, typename = std::enable_if_t<(sizeof...(Args) > 1)>>
			inline matrix<ElementType, rows, columns>::matrix(const Args &...args) :
				DataContainer(Data::InitTag<Data::InitType::Sequencing, IdxSeq>(), args...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX::ElementsCount<const Args &...>;
				static_assert(srcElements >= rows * columns, "sequencing ctor: too few src elements");
				static_assert(srcElements <= rows * columns, "sequencing ctor: too many src elements");
			}
#endif

			//template<typename ElementType, unsigned int rows, unsigned int columns>
			//template<typename TIterator>
			//inline matrix<ElementType, rows, columns>::matrix(TIterator src)
			//{
			//	for (unsigned r = 0; r < rows; r++, src += columns)
			//		operator [](r) = src;
			//}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const SrcElementType (&src)[srcRows][srcColumns]) :
				DataContainer(Data::InitTag<Data::InitType::Array, IdxSeq>(), src)
			{
				static_assert(rows <= srcRows, "array ctor: too few rows in src");
				static_assert(columns <= srcColumns, "array ctor: too few columns in src");
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline matrix<ElementType, rows, columns>::matrix(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList)
			{
				operator =(initList);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) -> std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &>
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) & -> std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &>
#endif
			{
				static_assert(rows <= srcRows, "'matrix = matrix': too few rows in src");
				static_assert(columns <= srcColumns, "'matrix = matrix': too few columns in src");
				for (unsigned r = 0; r < rows; r++)
					operator [](r) = src[r];
				return *this;
			}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcType>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const SrcType &scalar) -> std::enable_if_t<Impl::IsScalar<SrcType>, matrix &>
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const SrcType &scalar) & -> std::enable_if_t<Impl::IsScalar<SrcType>, matrix &>
#endif
			{
				for (unsigned r = 0; r < rows; r++)
					operator [](r) = scalar;
				return *this;
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src)
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
#endif
				-> std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &>
			{
				// C++17\
				static_assert(sizeof...(WARHazard) <= 1);
				constexpr static const auto dstDimension = rows * columns;
				constexpr static const bool underflow = dstDimension > SrcSwizzleDesc::dimension, overflow = dstDimension < SrcSwizzleDesc::dimension;
				static_assert(!(underflow || overflow && dstDimension > 1), "'matrix = vector': unmatched sequencing");
				auto &seq = reinterpret_cast<Impl::CSequencingSwizzle<ElementType, rows, columns> &>(data);
				seq.operator =<WARHazard...>(src);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src)
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &
#endif
				-> std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &>
			{
				return operator =<false>(src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList) -> matrix &
#else
			inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<Impl::CInitListItem<ElementType, rows * columns>> initList) & -> matrix &
#endif
			{
				unsigned dstIdx = 0;
				for (const auto &item : initList)
					for (unsigned itemEementIdx = 0; itemEementIdx < item.GetItemSize(); itemEementIdx++, dstIdx++)
						(*this)[dstIdx / columns][dstIdx % columns] = item[itemEementIdx];
				assert(dstIdx == rows * columns);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...idx>
			inline auto matrix<ElementType, rows, columns>::Pos(std::index_sequence<idx...>) const
			{
				return matrix<std::decay_t<decltype(+std::declval<ElementType>())>, rows, columns>(+operator [](idx)...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...idx>
			inline auto matrix<ElementType, rows, columns>::Neg(std::index_sequence<idx...>) const
			{
				return matrix<std::decay_t<decltype(-std::declval<ElementType>())>, rows, columns>(-operator [](idx)...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator +() const
			{
				return Pos(IdxSeq());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator -() const
			{
				return Neg(IdxSeq());
			}

			// matrix / 1x1 matrix op=<!WARHazard, !extractScalar> scalar
#			define OPERATOR_DEFINITION(op)																					\
				template<typename ElementType, unsigned int rows, unsigned int columns>										\
				template<bool WARHazard, bool extractScalar, typename SrcType>												\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)						\
				-> std::enable_if_t<!WARHazard && !extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>					\
				{																											\
					assert(!TriggerScalarWARHazard(*this, &scalar));														\
					for (unsigned r = 0; r < rows; r++)																		\
						Impl::ScalarOps::operator op##=<false, false>(operator [](r), scalar);								\
					return *this;																							\
				}
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#			undef OPERATOR_DEFINITION

			// matrix / 1x1 matrix op=<WARHazard, !extractScalar> scalar
#			define OPERATOR_DEFINITION(op)																					\
				template<typename ElementType, unsigned int rows, unsigned int columns>										\
				template<bool WARHazard, bool extractScalar, typename SrcType>												\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)						\
				-> std::enable_if_t<WARHazard && !extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>					\
				{																											\
					return operator op=<false, false>(SrcType(scalar));														\
				}
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#			undef OPERATOR_DEFINITION

			// matrix / 1x1 matrix op=<?WARHazard, extractScalar> scalar
#			define OPERATOR_DEFINITION(op)																					\
				template<typename ElementType, unsigned int rows, unsigned int columns>										\
				template<bool WARHazard, bool extractScalar, typename SrcType>												\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)						\
				-> std::enable_if_t<extractScalar/* && Impl::IsScalar<SrcType>*/, matrix &>									\
				{																											\
					return operator op=<WARHazard, false>(Impl::ExtractScalar(scalar));										\
				}
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION)
#			undef OPERATOR_DEFINITION

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename F>
			inline auto matrix<ElementType, rows, columns>::apply(F f) const -> matrix<std::result_of_t<F &(ElementType)>, rows, columns>
			{
				matrix<std::result_of_t<F &(ElementType)>, rows, columns> result;
				for (unsigned r = 0; r < rows; r++)
					result[r] = (*this)[r].apply(f);
				return result;
			}
#		pragma endregion

#	undef ELEMENTS_COUNT_PREFIX

#		pragma region min/max functions
			// std::min/max requires explicit template param if used for different types => provide scalar version\
			this version also has option to return copy or reference
#			define FUNCTION_DEFINITION(f)																					\
				template<bool copy = false, typename LeftType, typename RightType>											\
				inline auto f(const LeftType &left, const RightType &right)													\
				-> std::enable_if_t<Impl::IsPureScalar<LeftType> && Impl::IsPureScalar<RightType>, std::conditional_t<copy,	\
					std::common_type_t<LeftType, RightType>, const std::common_type_t<LeftType, RightType> &>>				\
				{																											\
					return std::f<std::common_type_t<LeftType, RightType>>(left, right);									\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,		\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc	\
				>																											\
				inline auto f(																								\
				const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,						\
				const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)					\
				{																											\
					typedef std::common_type_t																				\
					<																										\
						vector<LeftElementType, LeftSwizzleDesc::dimension>,												\
						vector<RightElementType, RightSwizzleDesc::dimension>												\
					> TResult;																								\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::dimension; i++)														\
						result[i] = std::f<typename TResult::ElementType>(left[i], right[i]);								\
					return result;																							\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,		\
					typename RightType																						\
				>																											\
				inline auto f(																								\
				const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,						\
				const RightType &right)																						\
				-> std::enable_if_t<Impl::IsPureScalar<RightType>, std::common_type_t										\
				<																											\
					vector<LeftElementType, LeftSwizzleDesc::dimension>,													\
					RightType																								\
				>>																											\
				{																											\
					typedef decltype(f(left, right)) TResult;																\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::dimension; i++)														\
						result[i] = std::f<typename TResult::ElementType>(left[i], right);									\
					return result;																							\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftType,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc	\
				>																											\
				inline auto f(																								\
				const LeftType &left,																						\
				const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)					\
				-> std::enable_if_t<Impl::IsPureScalar<LeftType>, std::common_type_t										\
				<																											\
					LeftType,																								\
					vector<RightElementType, RightSwizzleDesc::dimension>													\
				>>																											\
				{																											\
					typedef decltype(f(left, right)) TResult;																\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::dimension; i++)														\
						result[i] = std::f<typename TResult::ElementType>(left, right[i]);									\
					return result;																							\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,								\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns							\
				>																											\
				auto f(																										\
				const matrix<LeftElementType, leftRows, leftColumns> &left,													\
				const matrix<RightElementType, rightRows, rightColumns> &right)												\
				{																											\
					typedef std::common_type_t																				\
					<																										\
						matrix<LeftElementType, leftRows, leftColumns>,														\
						matrix<RightElementType, rightRows, rightColumns>													\
					> TResult;																								\
					TResult result;																							\
					for (unsigned r = 0; r < TResult::rows; r++)															\
						result[r] = VectorMath::f(left[r], right[r]);														\
					return result;																							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,								\
					typename RightType																						\
				>																											\
				auto f(																										\
				const matrix<LeftElementType, leftRows, leftColumns> &left,													\
				const RightType &right)																						\
				-> std::enable_if_t<Impl::IsPureScalar<RightType>, std::common_type_t										\
				<																											\
					matrix<LeftElementType, leftRows, leftColumns>,															\
					RightType																								\
				>>																											\
				{																											\
					typedef decltype(f(left, right)) TResult;																\
					TResult result;																							\
					for (unsigned r = 0; r < TResult::rows; r++)															\
						result[r] = VectorMath::f(left[r], right);															\
					return result;																							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename LeftType,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns							\
				>																											\
				auto f(																										\
				const LeftType &left,																						\
				const matrix<RightElementType, rightRows, rightColumns> &right)												\
				-> std::enable_if_t<Impl::IsPureScalar<LeftType>, std::common_type_t										\
				<																											\
					LeftType,																								\
					matrix<RightElementType, rightRows, rightColumns>														\
				>>																											\
				{																											\
					typedef decltype(f(left, right)) TResult;																\
					TResult result;																							\
					for (unsigned r = 0; r < TResult::rows; r++)															\
						result[r] = VectorMath::f(left, right[r]);															\
					return result;																							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION
#		pragma endregion

#		pragma region all/any/none functions
#			define FUNCTION_DEFINITION(f)																												\
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>												\
				inline bool f(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &src)														\
				{																																		\
					typedef Impl::CSwizzleIterator<ElementType, rows, columns, SwizzleDesc> TSwizzleIterator;											\
					return std::f##_of(TSwizzleIterator(src, 0), TSwizzleIterator(src, SwizzleDesc::dimension), [](std::conditional_t					\
					<																																	\
						sizeof(typename TSwizzleIterator::value_type) <= sizeof(void *),																\
						typename TSwizzleIterator::value_type,																							\
						typename TSwizzleIterator::reference																							\
					> element) -> bool {return element;});																								\
				};
			FUNCTION_DEFINITION(all)
			FUNCTION_DEFINITION(any)
			FUNCTION_DEFINITION(none)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION1(f) FUNCTION_DEFINITION2(f, f)
#			define FUNCTION_DEFINITION2(f1, f2)																											\
				template<typename ElementType, unsigned int rows, unsigned int columns>																	\
				bool f1(const matrix<ElementType, rows, columns> &src)																					\
				{																																		\
					return std::f2##_of(src.data.rowsData, src.data.rowsData + rows, f1<ElementType, 0, columns, Impl::CVectorSwizzleDesc<columns>>);	\
				};
			FUNCTION_DEFINITION1(all)
			FUNCTION_DEFINITION1(any)
			FUNCTION_DEFINITION2(none, all)	// none for matrix requires special handling
#			undef FUNCTION_DEFINITION1
#			undef FUNCTION_DEFINITION2
#		pragma endregion TODO: consider for packed specializations for bool vectors/matrices and replace std::all/any/none functions with bit operations

#		pragma region mul functions
			// note: most of these functions are not inline

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto mul(
			const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				constexpr unsigned int rowXcolumnDimension = std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension);
				typedef std::decay_t<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>())> ElementType;

				ElementType result = ElementType();

				for (unsigned i = 0; i < rowXcolumnDimension; i++)
					result += left[i] * right[i];

				return result;
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns
			>
			auto mul(
			const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const matrix<RightElementType, rightRows, rightColumns> &right)
			{
				constexpr unsigned int
					resultColumns = rightColumns,
					rowXcolumnDimension = std::min(LeftSwizzleDesc::dimension, rightRows);
				typedef std::decay_t<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>())> ElementType;

				vector<ElementType, resultColumns> result = ElementType();

				for (unsigned c = 0; c < resultColumns; c++)
					for (unsigned i = 0; i < rowXcolumnDimension; i++)
						result[c] += left[i] * right[i][c];

				return result;
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			auto mul(
			const matrix<LeftElementType, leftRows, leftColumns> &left,
			const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				constexpr unsigned int
					resultRows = leftRows,
					rowXcolumnDimension = std::min(leftColumns, RightSwizzleDesc::dimension);
				typedef std::decay_t<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>())> ElementType;

				vector<ElementType, resultRows> result = ElementType();

				for (unsigned r = 0; r < resultRows; r++)
					for (unsigned i = 0; i < rowXcolumnDimension; i++)
						result[r] += left[r][i] * right[i];

				return result;
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns
			>
			auto mul(
			const matrix<LeftElementType, leftRows, leftColumns> &left,
			const matrix<RightElementType, rightRows, rightColumns> &right)
			{
				constexpr unsigned int
					resultRows = leftRows,
					resultColumns = rightColumns,
					rowXcolumnDimension = std::min(leftColumns, rightRows);
				typedef std::decay_t<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>())> ElementType;

				matrix<ElementType, resultRows, resultColumns> result = ElementType();

				for (unsigned r = 0; r < resultRows; r++)
					for (unsigned c = 0; c < resultColumns; c++)
						for (unsigned i = 0; i < rowXcolumnDimension; i++)
							result[r][c] += left[r][i] * right[i][c];

				return result;
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto dot(
			const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				return mul(left, right);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto length(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			{
				return sqrt(dot(swizzle, swizzle));
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto distance(
			const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				return length(right - left);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto normalize(const Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			{
				return swizzle / length(swizzle);
			}

#			pragma region "series of matrices delimitted by ',' interpreted as series of successive transforms; inspirited by boost's function superposition"
				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
				>
				inline auto operator ,(
				const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				{
					return mul(left, right);
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto operator ,(
				const Impl::CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
				{
					return mul(left, right);
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
				>
				inline auto operator ,(
				const matrix<LeftElementType, leftRows, leftColumns> &left,
				const Impl::CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				{
					return mul(left, right);
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto operator ,(
				const matrix<LeftElementType, leftRows, leftColumns> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
				{
					return mul(left, right);
				}
#			pragma endregion
#		pragma endregion

#if USE_BOOST_PREPROCESSOR
#		undef ARITHMETIC_OPS
#		undef REL_OPS
#		undef GENERATE_OPERATOR
#		undef GENERATE_OPERATORS
#endif
#		undef GENERATE_ARITHMETIC_OPERATORS
#		undef GENERATE_REL_OPERATORS
	}

	// std specializations\
	NOTE: cv/ref specializations not provided, std::decay_t should be applied first for such types
	namespace std
	{
		// swizzle -> vector
		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		struct decay<Math::VectorMath::Impl::CSwizzle<ElementType, rows, columns, SwizzleDesc>>
		{
			typedef Math::VectorMath::vector<ElementType, SwizzleDesc::dimension> type;
		};

		// vector/vector
#if defined _MSC_VER && _MSC_VER <= 1900
		template<typename LeftElementType, unsigned int leftDimension, typename RightElementType, unsigned int rightDimension>
		class common_type<Math::VectorMath::vector<LeftElementType, leftDimension>, Math::VectorMath::vector<RightElementType, rightDimension>>
		{
			static constexpr unsigned int dimension = min(leftDimension, rightDimension);

		public:
			typedef Math::VectorMath::vector<common_type_t<LeftElementType, RightElementType>, dimension> type;
		};
#else
		template<typename LeftElementType, unsigned int leftDimension, typename RightElementType, unsigned int rightDimension>
		struct common_type<Math::VectorMath::vector<LeftElementType, leftDimension>, Math::VectorMath::vector<RightElementType, rightDimension>>
		{
			typedef Math::VectorMath::vector<common_type_t<LeftElementType, RightElementType>, min(leftDimension, rightDimension)> type;
		};
#endif

		// vector/scalar
		template<typename LeftElementType, unsigned int leftDimension, typename RightType>
		struct common_type<Math::VectorMath::vector<LeftElementType, leftDimension>, RightType>
		{
			typedef Math::VectorMath::vector<common_type_t<LeftElementType, RightType>, leftDimension> type;
		};

		// scalar/vector
		template<typename LeftType, typename RightElementType, unsigned int rightDimension>
		struct common_type<LeftType, Math::VectorMath::vector<RightElementType, rightDimension>>
		{
			typedef Math::VectorMath::vector<common_type_t<LeftType, RightElementType>, rightDimension> type;
		};

		// matrix/matrix
#if defined _MSC_VER && _MSC_VER <= 1900
		template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
		class common_type<Math::VectorMath::matrix<LeftElementType, leftRows, leftColumns>, Math::VectorMath::matrix<RightElementType, rightRows, rightColumns>>
		{
			static constexpr unsigned int rows = min(leftRows, rightRows), columns = min(leftColumns, rightColumns);

		public:
			typedef Math::VectorMath::matrix<common_type_t<LeftElementType, RightElementType>, rows, columns> type;
		};
#else
		template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
		struct common_type<Math::VectorMath::matrix<LeftElementType, leftRows, leftColumns>, Math::VectorMath::matrix<RightElementType, rightRows, rightColumns>>
		{
			typedef Math::VectorMath::matrix<common_type_t<LeftElementType, RightElementType>, min(leftRows, rightRows), min(leftColumns, rightColumns)> type;
		};
#endif

		// matrix/scalar
		template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, typename RightType>
		struct common_type<Math::VectorMath::matrix<LeftElementType, leftRows, leftColumns>, RightType>
		{
			typedef Math::VectorMath::matrix<common_type_t<LeftElementType, RightType>, leftRows, leftColumns> type;
		};

		// scalar/matrix
		template<typename LeftType, typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
		struct common_type<LeftType, Math::VectorMath::matrix<RightElementType, rightRows, rightColumns>>
		{
			typedef Math::VectorMath::matrix<common_type_t<LeftType, RightElementType>, rightRows, rightColumns> type;
		};
	}

#	undef MSVC_NAMESPACE_WORKAROUND

#	undef EBCO

#	undef INIT_LIST_ITEM_OVERFLOW_MSG

#if USE_BOOST_PREPROCESSOR
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED
#endif

#	pragma warning(pop)

#endif//__VECTOR_MATH_H__
#endif