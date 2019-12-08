#pragma region design considerations
/*
it is safe to use const_cast if const version returns 'const &', not value, and '*this' object is not const

functions like distance now receives Swizzle arguments
function template with same name from other namespace can have higher priority when passing vector arguments
(because no conversion required in contrast to vector->Swizzle conversion required for function in VectorMath)
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
1D swizzle op 1x1 matrix -> 1D swizzle
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
#	define SWIZZLE_SEQ_2_DESC(seq) SwizzleDesc<BOOST_PP_TUPLE_REM_CTOR(BOOST_PP_SEQ_SIZE(seq), BOOST_PP_SEQ_TO_TUPLE(seq))>

#	define BOOST_PP_ITERATION_LIMITS (1, 4)
#	define BOOST_PP_FILENAME_2 "vector math.h"
#	include BOOST_PP_ITERATE()
#else
	/*
	gcc does not allow explicit specialization in class scope => Swizzle can not be inside DataContainer
	ElementType needed in order to compiler can deduce template args for operators
	*/
#if !defined DISABLE_MATRIX_SWIZZLES || ROWS == 0
	namespace Impl
	{
#		define NAMING_SET_1 BOOST_PP_IF(ROWS, MATRIX_ZERO_BASED, XYZW)
#		define NAMING_SET_2 BOOST_PP_IF(ROWS, MATRIX_ONE_BASED, RGBA)

#		define SWIZZLE_OBJECT(swizzle_seq)											\
			Swizzle<ElementType, ROWS, COLUMNS, SWIZZLE_SEQ_2_DESC(swizzle_seq)>	\
				TRANSFORM_SWIZZLE(NAMING_SET_1, swizzle_seq),						\
				TRANSFORM_SWIZZLE(NAMING_SET_2, swizzle_seq);

#		define TRIVIAL_CTOR_FORWARD DataContainer() = default; NONTRIVIAL_CTOR_FORWARD
#		define NONTRIVIAL_CTOR_FORWARD template<typename ...Args> DataContainer(const Args &...args) : data(args...) {}

		// specialization for graphics vectors/matrices
#		define DATA_CONTAINER_SPECIALIZATION(trivialCtor)																\
			template<typename ElementType>																				\
			requires (is_trivially_default_constructible_v<ElementType> == trivialCtor)									\
			struct DataContainer<ElementType, ROWS, COLUMNS>															\
			{																											\
				union																									\
				{																										\
					Data<ElementType, ROWS, COLUMNS> data;																\
					/*gcc does not allow class definition inside anonymous union*/										\
					GENERATE_SWIZZLES((SWIZZLE_OBJECT))																	\
				};																										\
																														\
			protected:																									\
				/*forward ctors/dtor/= to data*/																		\
				BOOST_PP_REMOVE_PARENS(BOOST_PP_IIF(trivialCtor, (TRIVIAL_CTOR_FORWARD), (NONTRIVIAL_CTOR_FORWARD)))	\
																														\
				DataContainer(const DataContainer &src) : data(src.data) {}												\
																														\
				DataContainer(DataContainer &&src) : data(move(src.data)) {}											\
																														\
				~DataContainer()																						\
				{																										\
					data.~Data<ElementType, ROWS, COLUMNS>();															\
				}																										\
																														\
				void operator =(const DataContainer &src)																\
				{																										\
					data = src.data;																					\
				}																										\
																														\
				void operator =(DataContainer &&src)																	\
				{																										\
					data = move(src.data);																				\
				}																										\
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

#	if defined _MSC_VER && _MSC_VER < 1923 && !defined  __clang__
#	error Old MSVC compiler version. Visual Studio 2019 16.3 or later required.
#	elif defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)) && 0 && !defined __clang__
#	error Old GCC compiler version. GCC ?.? or later required.	// need to be clarified
#	endif

#	pragma warning(push)
#	pragma warning(disable: 4003)

#if defined _MSC_VER && _MSC_VER <= 1924 && !defined __clang__
#	define MSVC_LIMITATIONS
#endif

// ugly workaround for operations like 'vec4.xy += int4(4).xxx;'
#ifdef MSVC_LIMITATIONS
#	define MSVC_NAMESPACE_WORKAROUND
#endif

#if !defined _MSVC_TRADITIONAL || _MSVC_TRADITIONAL
#	define MSVC_PREPROCESSOR_WORKAROUND
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
#	include <algorithm>
#if INIT_LIST_SUPPORT_TIER > 0
#	include <initializer_list>
#endif
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
//#	define GET_SWIZZLE_ELEMENT(vectorDimension, idx, cv) (reinterpret_cast<cv Data<ElementType, vectorDimension> &>(*this).data[(idx)])
//#	define GET_SWIZZLE_ELEMENT_PACKED(vectorDimension, packedSwizzle, idx, cv) (GET_SWIZZLE_ELEMENT(vectorDimension, packedSwizzle >> ((idx) << 1) & 3u, cv))
#endif

	namespace Math::VectorMath
	{
#		define OP_plus			+
#		define OP_minus			-
#		define OP_multiplies	*
#		define OP_divides		/
#		define OP_equal_to		==
#		define OP_not_equal_to	!=
#		define OP_greater		>
#		define OP_less			<
#		define OP_greater_equal	>=
#		define OP_less_equal	<=
#		define OP_logical_and	&&
#		define OP_logical_or	||
#		define F_2_OP(F) OP_ ## F
#		define F_2_PAIR(F) F_2_OP(F), F

#ifdef MSVC_PREPROCESSOR_WORKAROUND
#		define GENERATE_OPERATOR_IMPL(operatorTemplate, args) operatorTemplate args
#		define GENERATE_OPERATOR(operatorTemplate, ...) GENERATE_OPERATOR_IMPL(operatorTemplate, (__VA_ARGS__))
#else
#		define GENERATE_OPERATOR(operatorTemplate, ...) operatorTemplate(__VA_ARGS__)
#endif

#if USE_BOOST_PREPROCESSOR
#		define ARITHMETIC_OPS (plus)(minus)(multiplies)(divides)
#		define MASK_OPS (equal_to)(not_equal_to)(greater)(less)(greater_equal)(less_equal)(logical_and)(logical_or)
#		define OPERATOR_GENERATOR(r, callback, op) GENERATE_OPERATOR(BOOST_PP_TUPLE_ELEM(2, 0, callback), BOOST_PP_TUPLE_ELEM(2, 1, callback)(op))
#		define GENERATE_OPERATORS(operatorTemplate, adaptor, ops) BOOST_PP_SEQ_FOR_EACH(OPERATOR_GENERATOR, (operatorTemplate, adaptor), ops)
#		define GENERATE_ARITHMETIC_OPERATORS(operatorTemplate, adaptor) GENERATE_OPERATORS(operatorTemplate, adaptor, ARITHMETIC_OPS)
#		define GENERATE_MASK_OPERATORS(operatorTemplate, adaptor) GENERATE_OPERATORS(operatorTemplate, adaptor, MASK_OPS)
#else
#		define GENERATE_ARITHMETIC_OPERATORS(operatorTemplate, adaptor)	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(plus))			\
			GENERATE_OPERATOR(operatorTemplate, adaptor(minus))			\
			GENERATE_OPERATOR(operatorTemplate, adaptor(multiplies))	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(divides))

#		define GENERATE_MASK_OPERATORS(operatorTemplate, adaptor)		\
			GENERATE_OPERATOR(operatorTemplate, adaptor(equal_to))		\
			GENERATE_OPERATOR(operatorTemplate, adaptor(not_equal_to))	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(greater))		\
			GENERATE_OPERATOR(operatorTemplate, adaptor(less))			\
			GENERATE_OPERATOR(operatorTemplate, adaptor(greater_equal))	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(less_equal))	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(logical_and))	\
			GENERATE_OPERATOR(operatorTemplate, adaptor(logical_or))
#endif

		namespace Impl
		{
#if USE_BOOST_MPL
			namespace mpl = boost::mpl;
			using namespace mpl::placeholders;
#endif

			using std::move;
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
			using std::type_identity;
			using std::conditional_t;
			using std::result_of_t;
			using std::plus;
			using std::minus;
			using std::multiplies;
			using std::divides;
			using std::equal_to;
			using std::not_equal_to;
			using std::greater;
			using std::less;
			using std::greater_equal;
			using std::less_equal;
			using std::logical_and;
			using std::logical_or;
			using std::numeric_limits;
#if INIT_LIST_SUPPORT_TIER > 0
			using std::initializer_list;
#endif

			enum class TagName
			{
				Swizzle,
				Vector,
				Matrix,
				Wrapped,
			};

			template<TagName name, bool scalar>
			class Tag
			{
			protected:
				Tag() = default;
				Tag(const Tag &) = default;
				Tag &operator =(const Tag &) & = default;
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
			static constexpr bool IsWrappedAsScalar = CheckTag::Check<Src, TagName::Wrapped, true>;

			template<typename Src>
			concept _1D = (CheckTag::Check<Src, TagName::Swizzle, true> || CheckTag::Check<Src, TagName::Vector, true>) && !IsWrappedAsScalar<Src>;

			template<typename Src>
			concept _1x1 = CheckTag::Check<Src, TagName::Matrix, true> && !IsWrappedAsScalar<Src>;

			template<typename Src>
			concept PackedScalar = _1D<Src> || _1x1<Src>;

			template<typename Src>
			concept Scalar = !CheckTag::Check<Src, TagName::Swizzle, false> && !CheckTag::Check<Src, TagName::Vector, false> && !CheckTag::Check<Src, TagName::Matrix, false> || IsWrappedAsScalar<Src>;

			template<typename Src>
			concept PureScalar = Scalar<Src> && !PackedScalar<Src>;

			template<typename ElementType>
			static constexpr bool IsElementTypeValid = (is_union_v<ElementType> || is_class_v<ElementType> || is_arithmetic_v<ElementType>) && !is_const_v<ElementType> && !IsWrappedAsScalar<ElementType>;

			template<class F>
			static constexpr bool IsMaskOp = false;

#			define IS_MASK_OP_SPEC(F)	\
				template<typename T>	\
				static constexpr bool IsMaskOp<F<T>> = true;
#			define PASS_THROUGH(x) x
			GENERATE_MASK_OPERATORS(IS_MASK_OP_SPEC, PASS_THROUGH)
#			undef IS_MASK_OP_SPEC
#			undef PASS_THROUGH

			struct positive
			{
				template<typename Src>
				constexpr auto operator ()(const Src &src) const { return +src; }
			};

			struct plus_assign
			{
				template<typename Dst, typename Src>
				constexpr void operator ()(Dst &dst, const Src &src) const { dst += src; }
			};

			struct minus_assign
			{
				template<typename Dst, typename Src>
				constexpr void operator ()(Dst &dst, const Src &src) const { dst -= src; }
			};

			struct multiplies_assign
			{
				template<typename Dst, typename Src>
				constexpr void operator ()(Dst &dst, const Src &src) const { dst *= src; }
			};

			struct divides_assign
			{
				template<typename Dst, typename Src>
				constexpr void operator ()(Dst &dst, const Src &src) const { dst /= src; }
			};

#if USE_BOOST_MPL
			template<class Seq, class Iter>
			using DistanceFromBegin = typename mpl::distance<typename mpl::begin<Seq>::type, Iter>::type;

			template<class SwizzleVector>
			class SwizzleTable
			{
			public:
				static constexpr unsigned int dimension = mpl::size<SwizzleVector>::type::value;

			private:
				static constexpr unsigned int bitsRequired = dimension * 4;
				using PackedSwizzle =
					conditional_t<bitsRequired <= numeric_limits<unsigned int>::digits, unsigned int,
					conditional_t<bitsRequired <= numeric_limits<unsigned long int>::digits, unsigned long int,
					unsigned long long int>>;

			private:
				template<class Iter>
				struct PackSwizzleElement : mpl::shift_left<typename mpl::deref<Iter>::type, typename mpl::times<DistanceFromBegin<SwizzleVector, Iter>, mpl::integral_c<unsigned int, 4u>>> {};
				typedef mpl::iter_fold<SwizzleVector, mpl::integral_c<PackedSwizzle, 0u>, mpl::bitor_<_1, PackSwizzleElement<_2>>> EvaluatePackedSwizzle;

			private:
				static constexpr PackedSwizzle packedSwizzle = EvaluatePackedSwizzle::type::value;

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return packedSwizzle >> idx * 4u & 0xFu;
				}
			};

			template<unsigned int ...swizzleSeq>
			class SwizzleDesc : public SwizzleTable<mpl::vector_c<unsigned int, swizzleSeq...>>
			{
			public:
				typedef mpl::vector_c<unsigned int, swizzleSeq...> SwizzleVector;

			private:
				typedef typename mpl::sort<SwizzleVector>::type SortedSwizzleVector;
				typedef typename mpl::unique<SortedSwizzleVector, is_same<_, _>>::type UniqueSwizzleVector;
				typedef typename mpl::equal_to<mpl::size<UniqueSwizzleVector>, mpl::size<SwizzleVector>> IsWriteMaskValid;

			public:
				static constexpr typename IsWriteMaskValid::value_type isWriteMaskValid = IsWriteMaskValid::value;
			};

			template<unsigned int rows, unsigned int columns>
			class SequencingSwizzleVector
			{
				typedef mpl::range_c<unsigned int, 0, rows * columns> LinearSequence;
				typedef mpl::integral_c<unsigned int, columns> Columns;
				typedef mpl::bitor_<mpl::modulus<_, Columns>, mpl::shift_left<mpl::divides<_, Columns>, mpl::integral_c<unsigned int, 2u>>> Xform;

			public:
				// SwizzleVector here not actually vector but rather range
				typedef mpl::transform_view<LinearSequence, Xform> SwizzleVector;
			};

			template<unsigned int rows, unsigned int columns>
			struct SequencingSwizzleDesc : SequencingSwizzleVector<rows, columns>, SwizzleTable<typename SequencingSwizzleVector<rows, columns>::SwizzleVector>
			{
				static constexpr bool isWriteMaskValid = true;
			};
#else
			template<unsigned int ...swizzleSeq>
			class SwizzleTable
			{
			public:
				static constexpr unsigned int dimension = sizeof...(swizzleSeq);

			private:
				static constexpr unsigned int bitsRequired = dimension * 4;
				using PackedSwizzle =
					conditional_t<bitsRequired <= numeric_limits<unsigned int>::digits, unsigned int,
					conditional_t<bitsRequired <= numeric_limits<unsigned long int>::digits, unsigned long int,
					unsigned long long int>>;

			private:
#ifdef MSVC_LIMITATIONS
				template<unsigned int shift, PackedSwizzle swizzleHead, PackedSwizzle ...swizzleTail>
				static constexpr PackedSwizzle PackSwizzleSeq = swizzleHead << shift | PackSwizzleSeq<shift + 4u, swizzleTail...>;

				template<unsigned int shift, PackedSwizzle swizzleLast>
				static constexpr PackedSwizzle PackSwizzleSeq<shift, swizzleLast> = swizzleLast << shift;
#else
				template<typename IdxSeq>
				static constexpr PackedSwizzle PackSwizzleSeq = 0;

				template<unsigned int ...idxSeq>
				static constexpr PackedSwizzle PackSwizzleSeq<integer_sequence<unsigned int, idxSeq...>> = ((PackedSwizzle(swizzleSeq) << idxSeq * 4u) | ...);
#endif

			private:
#ifdef MSVC_LIMITATIONS
				static constexpr PackedSwizzle packedSwizzle = PackSwizzleSeq<0u, swizzleSeq...>;
#else
				static constexpr PackedSwizzle packedSwizzle = PackSwizzleSeq<make_integer_sequence<unsigned int, dimension>>;
#endif

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return packedSwizzle >> idx * 4u & 0xFu;
				}
			};

			template<unsigned int ...swizzleSeq>
			class SwizzleDesc : public SwizzleTable<swizzleSeq...>
			{
			public:
				typedef SwizzleTable<swizzleSeq...> SwizzleTable;

			public:
				using SwizzleTable::dimension;
				using SwizzleTable::FetchIdx;

			private:
				static constexpr bool IsWriteMaskValid()
				{
					for (unsigned i = 0; i < dimension; i++)
						for (unsigned j = i + 1; j < dimension; j++)
							if (FetchIdx(i) == FetchIdx(j))
								return false;
					return true;
				}

			public:
				static constexpr bool isWriteMaskValid = IsWriteMaskValid();
			};

			template<unsigned int rows, unsigned int columns>
			class MakeSequencingSwizzleTable
			{
				typedef make_integer_sequence<unsigned int, rows * columns> LinearSequence;

				template<typename LinearSequence>
				struct MakeSwizzleTable;

				template<unsigned int ...seq>
				struct MakeSwizzleTable<integer_sequence<unsigned int, seq...>>
				{
					typedef SwizzleTable<seq / columns << 2u | seq % columns ...> type;
				};

				public:
					typedef typename MakeSwizzleTable<LinearSequence>::type type;
			};

			template<unsigned int rows, unsigned int columns>
			struct SequencingStorelessSwizzle
			{
				static constexpr unsigned int dimension = rows * columns;

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return idx / columns << 2u | idx % columns;
				}
			};

			template<unsigned int rows, unsigned int columns>
			struct SequencingSwizzleDesc : conditional_t<rows * columns <= 16, typename MakeSequencingSwizzleTable<rows, columns>::type, SequencingStorelessSwizzle<rows, columns>>
			{
				static constexpr bool isWriteMaskValid = true;
			};
#endif

			template<unsigned int vectorDimension>
			using VectorSwizzleDesc = SequencingSwizzleDesc<1, vectorDimension>;

			template<unsigned int rows, unsigned c>
			struct ColumnSwizzleDesc
			{
				static constexpr unsigned int dimension = rows;
				static constexpr bool isWriteMaskValid = true;

			public:
				static constexpr unsigned int FetchIdx(unsigned int idx)
				{
					return idx << 2u | c;
				}
			};

#if USE_BOOST_MPL
			// contains SwizzleVector only, has no dimension nor isWriteMaskValid (it is trivial to evaluate but it is not needed for WAR hazard detector)
			template<unsigned int scalarSwizzle, unsigned int dimension>
			class BroadcastScalarSwizzleDesc
			{
				template<typename IntSeq>
				struct IntSeq2SwizzleVec;

				template<unsigned int ...seq>
				struct IntSeq2SwizzleVec<integer_sequence<unsigned int, seq...>>
				{
					typedef mpl::vector_c<unsigned int, (seq, scalarSwizzle)...> type;
				};

			public:
				typedef typename IntSeq2SwizzleVec<make_integer_sequence<unsigned int, dimension>>::type SwizzleVector;
			};
#else
			template<unsigned int scalarSwizzle, unsigned int dimension>
			class MakeBroadcastScalarSwizzleDesc
			{
				template<typename Rep>
				struct MakeSwizzleTable;

				template<unsigned int ...rep>
				struct MakeSwizzleTable<integer_sequence<unsigned int, rep...>>
				{
					typedef SwizzleTable<(rep, scalarSwizzle)...> type;
				};

			public:
				typedef typename MakeSwizzleTable<make_integer_sequence<unsigned int, dimension>>::type type;
			};

			// contains SwizzleTable only, has no isWriteMaskValid (it is trivial to evaluate but it is not needed for WAR hazard detector)
			template<unsigned int scalarSwizzle, unsigned int dimension>
			using BroadcastScalarSwizzleDesc = typename MakeBroadcastScalarSwizzleDesc<scalarSwizzle, dimension>::type;
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = VectorSwizzleDesc<columns>>
			class SwizzleDataAccess;

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = VectorSwizzleDesc<columns>, typename isWriteMaskValid = bool_constant<SwizzleDesc::isWriteMaskValid>>
			class SwizzleAssign;

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
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = Impl::VectorSwizzleDesc<columns>, typename = Impl::TouchSwizzleDesc<SwizzleDesc>>
			class Swizzle;
#ifdef MSVC_NAMESPACE_WORKAROUND
		namespace Impl
		{
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = VectorSwizzleDesc<columns>>
			using Swizzle = VectorMath::Swizzle<ElementType, rows, columns, SwizzleDesc>;
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			using SequencingSwizzle = Swizzle<ElementType, rows, columns, SequencingSwizzleDesc<rows, columns>>;

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned c>
			using ColumnSwizzle = Swizzle<ElementType, rows, columns, ColumnSwizzleDesc<rows, c>>;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			struct DataContainer;
		}

		template<class Wrapped/*vector or matrix*/>
		class AsScalar final : public Impl::Tag<Impl::TagName::Wrapped, true>
		{
			const Wrapped &ref;

		public:
			explicit AsScalar(const Wrapped &src) noexcept : ref(src) {}
			operator const Wrapped &() const noexcept { return ref; }
		};

		template<typename ElementType, unsigned int dimension>
		class EBCO vector;

		template<typename ElementType, unsigned int rows, unsigned int columns>
		class EBCO matrix;

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool all(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool any(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool none(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool all(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool any(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool none(const matrix<ElementType, rows, columns> &src);

		namespace Impl
		{
			template<PureScalar SrcType>
			inline const SrcType &ExtractScalar(const SrcType &src) noexcept
			{
				return src;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline const ElementType &ExtractScalar(const Swizzle<ElementType, rows, columns, SwizzleDesc> &src) noexcept
			{
				return src;
			}

			template<typename ElementType>
			inline const ElementType &ExtractScalar(const matrix<ElementType, 1, 1> &src) noexcept
			{
				return src;
			}

#			pragma region WAR hazard
#				pragma region detector
#if USE_BOOST_MPL
					template<class DstSwizzleVector, class SrcSwizzleVector, bool assign>
					class SwizzleWARHazardDetectHelper
					{
						// cut SrcSwizzleVector off
						typedef typename mpl::min<typename mpl::size<SrcSwizzleVector>::type, typename mpl::size<DstSwizzleVector>::type>::type MinSwizzleSize;
						typedef typename mpl::begin<SrcSwizzleVector>::type SrcSwizzleBegin;
						typedef typename mpl::iterator_range<SrcSwizzleBegin, typename mpl::advance<SrcSwizzleBegin, MinSwizzleSize>::type>::type CuttedSrcSwizzleVector;

					private:
						template<class SrcIter>
						class Pred
						{
							template<class F>
							struct invoke
							{
								typedef typename F::type type;
							};
							using DstIter = typename mpl::find<DstSwizzleVector, typename mpl::deref<SrcIter>::type>::type;
							using DstWasAlreadyWritten = typename mpl::less<DistanceFromBegin<DstSwizzleVector, DstIter>, DistanceFromBegin<CuttedSrcSwizzleVector, SrcIter>>::type;

						private:
							using FindSrcWrittenToDstIter = mpl::advance<typename mpl::begin<CuttedSrcSwizzleVector>::type, DistanceFromBegin<DstSwizzleVector, DstIter>>;
							using DstWasModified = mpl::bind<typename mpl::lambda<mpl::not_equal_to<invoke<_1>, invoke<_2>>>::type, mpl::deref<DstIter>, mpl::deref<typename FindSrcWrittenToDstIter::type>>;

						public:
							typedef typename mpl::apply<conditional_t<assign && DstWasAlreadyWritten::value, DstWasModified, mpl::bind<mpl::lambda<_>::type, DstWasAlreadyWritten>>>::type type;
						};
						typedef typename mpl::iter_fold<CuttedSrcSwizzleVector, false_type, mpl::or_<_1, Pred<_2>>>::type Result;

					public:
						static constexpr typename Result::value_type value = Result::value;
					};

					// mpl::transform does not work with ranges (bug in mpl?) => use mpl::transform_view (even if it can potentially be less efficient)
					template<class SwizzleDesc, unsigned int rowIdx>
					using RowSwizzleDesc_2_MatrixSwizzleVector = mpl::transform_view<typename SwizzleDesc::SwizzleVector, mpl::bitor_<_, mpl::integral_c<unsigned, rowIdx << 2>>>;

					template<class DstSwizzleDesc, bool dstIsMatrix, class SrcSwizzleDesc, bool srcIsMatrix, unsigned int rowIdx, bool assign>
					using DetectRowVsMatrixWARHazard = SwizzleWARHazardDetectHelper<
						conditional_t<dstIsMatrix, typename DstSwizzleDesc::SwizzleVector, RowSwizzleDesc_2_MatrixSwizzleVector<DstSwizzleDesc, rowIdx>>,
						conditional_t<srcIsMatrix, typename SrcSwizzleDesc::SwizzleVector, RowSwizzleDesc_2_MatrixSwizzleVector<SrcSwizzleDesc, rowIdx>>,
						assign>;
#else
					template<class DstSwizzleTable, class SrcSwizzleTable, bool assign>
					class SwizzleWARHazardDetectHelper
					{
						// cut SrcSwizzle off
						static constexpr unsigned int cuttedSrcDimension = std::min(SrcSwizzleTable::dimension, DstSwizzleTable::dimension);

					private:
						// searches until srcSwizzleIdx, returns srcSwizzleIdx if not found
						static constexpr unsigned FindDstSwizzleIdx(unsigned srcSwizzleIdx)
						{
							unsigned dstSwizzleIdx = 0;
							const auto idx2Find = SrcSwizzleTable::FetchIdx(srcSwizzleIdx);
							while (dstSwizzleIdx < srcSwizzleIdx && DstSwizzleTable::FetchIdx(dstSwizzleIdx) != idx2Find)
								dstSwizzleIdx++;
							return dstSwizzleIdx;
						}

						static constexpr bool DstWasAlreadyWritten(unsigned dstSwizzleIdx, unsigned srcSwizzleIdx)
						{
							return dstSwizzleIdx < srcSwizzleIdx;
						}

						static constexpr bool DstWasModified(unsigned dstSwizzleIdx)
						{
							return DstSwizzleTable::FetchIdx(dstSwizzleIdx) != SrcSwizzleTable::FetchIdx(dstSwizzleIdx);
						}

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

					public:
						static constexpr bool value = Result();
					};

					template<class SwizzleTable, unsigned int rowIdx>
					struct RowSwizzle_2_MatrixSwizzle;

					template<unsigned int ...swizzleSeq, unsigned int rowIdx>
					struct RowSwizzle_2_MatrixSwizzle<SwizzleTable<swizzleSeq...>, rowIdx>
					{
						typedef SwizzleTable<swizzleSeq | rowIdx << 2u ...> type;
					};

					template<class SwizzleDesc, unsigned int rowIdx>
					using RowSwizzleDesc_2_MatrixSwizzleTable = RowSwizzle_2_MatrixSwizzle<typename SwizzleDesc::SwizzleTable, rowIdx>;

					template<class DstSwizzleDesc, bool dstIsMatrix, class SrcSwizzleDesc, bool srcIsMatrix, unsigned int rowIdx, bool assign>
					using DetectRowVsMatrixWARHazard = SwizzleWARHazardDetectHelper<
						typename conditional_t<dstIsMatrix, type_identity<DstSwizzleDesc>, RowSwizzleDesc_2_MatrixSwizzleTable<DstSwizzleDesc, rowIdx>>::type,
						typename conditional_t<srcIsMatrix, type_identity<SrcSwizzleDesc>, RowSwizzleDesc_2_MatrixSwizzleTable<SrcSwizzleDesc, rowIdx>>::type,
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
						static_assert(is_same_v<Void, void>);
					};

					template
					<
						typename ElementType, unsigned int rows, unsigned int columns,
						class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
					>
					struct DetectSwizzleWARHazard<ElementType, rows, columns, DstSwizzleDesc, ElementType, rows, columns, SrcSwizzleDesc, assign> :
#if USE_BOOST_MPL
						SwizzleWARHazardDetectHelper<typename DstSwizzleDesc::SwizzleVector, typename SrcSwizzleDesc::SwizzleVector, assign> {};
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
#ifdef MSVC_LIMITATIONS
						template<unsigned int rows, unsigned rowIdx = 0>
						static constexpr auto FoldRows = DetectRowVsMatrixWARHazard<DstSwizzleDesc, bool(dstRows), SrcSwizzleDesc, bool(srcRows), rowIdx, assign>::value || FoldRows<rows, rowIdx + 1>;

						// terminator
						template<unsigned int rows>
						static constexpr auto FoldRows<rows, rows> = false;

					public:
						static constexpr auto value = FoldRows<dstRows ? dstRows : srcRows>;
#else
						template<typename Seq>
						static constexpr bool FoldRows = false;

						template<unsigned ...rowIdx>
						static constexpr bool FoldRows<integer_sequence<unsigned, rowIdx...>> = (DetectRowVsMatrixWARHazard<DstSwizzleDesc, bool(dstRows), SrcSwizzleDesc, bool(srcRows), rowIdx, assign>::value || ...);

					public:
						static constexpr bool value = FoldRows<make_integer_sequence<unsigned, dstRows ? dstRows : srcRows>>;
#endif
					};

#					pragma region scalar
						template<typename DstType, typename SrcType, typename Void = void>
						struct DetectScalarWARHazard : false_type
						{
							static_assert(!PureScalar<DstType> && _1x1<SrcType> && is_same_v<Void, void>);
						};

						// pure scalar

						template<typename DstElementType, unsigned int dstElementsCount, typename SrcType>
						using PureScalarWARHazardDetectHelper = bool_constant<(dstElementsCount > 1 &&
							is_same_v<remove_volatile_t<DstElementType>, remove_cv_t<remove_reference_t<decltype(ExtractScalar(declval<SrcType>()))>>>)>;

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc, typename SrcType>
						struct DetectScalarWARHazard<Swizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc>, SrcType, enable_if_t<PureScalar<SrcType>>> :
							PureScalarWARHazardDetectHelper<DstElementType, DstSwizzleDesc::dimension, SrcType> {};

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, typename SrcType>
						struct DetectScalarWARHazard<matrix<DstElementType, dstRows, dstColumns>, SrcType, enable_if_t<PureScalar<SrcType>>> :
							PureScalarWARHazardDetectHelper<DstElementType, dstRows * dstColumns, SrcType> {};

						// 1D swizzle/vector

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static integral_constant<unsigned int, rows> GetSwizzleRows(const Swizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static integral_constant<unsigned int, columns> GetSwizzleColumns(const Swizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static SwizzleDesc GetSwizzleDesc(const Swizzle<ElementType, rows, columns, SwizzleDesc> &);

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc, typename SrcType>
						struct DetectScalarWARHazard<Swizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc>, SrcType, enable_if_t<_1D<SrcType>>> :
							DetectSwizzleWARHazard
							<
								DstElementType, dstRows, dstColumns, DstSwizzleDesc,
								remove_const_t<remove_reference_t<decltype(ExtractScalar(declval<SrcType>()))>>,
								decltype(GetSwizzleRows(declval<SrcType>()))::value,
								decltype(GetSwizzleColumns(declval<SrcType>()))::value,
#if USE_BOOST_MPL
								BroadcastScalarSwizzleDesc<mpl::front<typename decltype(GetSwizzleDesc(declval<SrcType>()))::SwizzleVector>::type::value, DstSwizzleDesc::dimension>,
#else
								BroadcastScalarSwizzleDesc<decltype(GetSwizzleDesc(declval<SrcType>()))::FetchIdx(0), DstSwizzleDesc::dimension>,
#endif
								false
							> {};

						template
						<
							typename DstElementType, unsigned int dstRows, unsigned int dstColumns,
							typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc
						>
						static false_type MatrixVs1D_WARHazardDetectHelper(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &);

						template<typename ElementType, unsigned int rows, unsigned int columns, class SrcSwizzleDesc>
#if USE_BOOST_MPL
						static bool_constant<mpl::front<typename SrcSwizzleDesc::SwizzleVector>::type::value != (columns - 1 | rows - 1 << 2)>
#else
						static bool_constant<SrcSwizzleDesc::FetchIdx(0) != (columns - 1 | rows - 1 << 2)>
#endif
							MatrixVs1D_WARHazardDetectHelper(const Swizzle<ElementType, rows, columns, SrcSwizzleDesc> &);

						template<typename ElementType, unsigned int dstRows, unsigned int columns, class SrcSwizzleDesc>
#if USE_BOOST_MPL
						static bool_constant<(mpl::front<typename SrcSwizzleDesc::SwizzleVector>::type::value != columns - 1 || dstRows > 1)>
#else
						static bool_constant<(SrcSwizzleDesc::FetchIdx(0) != columns - 1 || dstRows > 1)>
#endif
							MatrixVs1D_WARHazardDetectHelper(const Swizzle<ElementType, 0, columns, SrcSwizzleDesc> &);

						template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, typename SrcType>
						struct DetectScalarWARHazard<matrix<DstElementType, dstRows, dstColumns>, SrcType, enable_if_t<_1D<SrcType>>> :
							decltype(MatrixVs1D_WARHazardDetectHelper<DstElementType, dstRows, dstColumns>(declval<SrcType>())) {};
#					pragma endregion
#				pragma endregion

#				pragma region trigger
#					ifndef NDEBUG
						// vector
						template<unsigned rowIdx = 0, typename ElementType, unsigned int dimension, class SwizzleDesc>
						static inline const void *GetRowAddress(const SwizzleDataAccess<ElementType, 0, dimension, SwizzleDesc> &swizzle)
						{
							return swizzle.AccessData();
						}

						// matrix
						template<unsigned rowIdx = 0, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
						static inline const void *GetRowAddress(const SwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> &swizzle)
						{
							return swizzle.AccessData()[rowIdx].SwizzleDataAccess::AccessData();
						}

						template
						<
							bool assign, class DstSwizzleDesc, class SrcSwizzleDesc,
							typename ElementType, unsigned int rows, unsigned int columns
						>
						static inline bool TriggerWARHazard(SwizzleDataAccess<ElementType, rows, columns, DstSwizzleDesc> &dst, const SwizzleDataAccess<ElementType, rows, columns, SrcSwizzleDesc> &src)
						{
#if USE_BOOST_MPL
#if __clang__
							return SwizzleWARHazardDetectHelper<typename DstSwizzleDesc::SwizzleVector, typename SrcSwizzleDesc::SwizzleVector, assign>::value && GetRowAddress<0>(dst) == GetRowAddress<0>(src);
#else
							return SwizzleWARHazardDetectHelper<typename DstSwizzleDesc::SwizzleVector, typename SrcSwizzleDesc::SwizzleVector, assign>::value && GetRowAddress(dst) == GetRowAddress(src);
#endif
#else
#if __clang__
							return SwizzleWARHazardDetectHelper<DstSwizzleDesc, SrcSwizzleDesc, assign>::value && GetRowAddress<0>(dst) == GetRowAddress<0>(src);
#else
							return SwizzleWARHazardDetectHelper<DstSwizzleDesc, SrcSwizzleDesc, assign>::value && GetRowAddress(dst) == GetRowAddress(src);
#endif
#endif
						}

						// mixed vector/matrix swizzles
						template
						<
							bool assign, unsigned rowIdx = 0, class DstSwizzleDesc, class SrcSwizzleDesc,
							typename ElementType, unsigned int dstRows, unsigned int srcRows, unsigned int columns
						>
						static inline enable_if_t<bool(dstRows) != bool(srcRows) && rowIdx < std::max(dstRows, srcRows), bool>
						TriggerWARHazard(SwizzleDataAccess<ElementType, dstRows, columns, DstSwizzleDesc> &dst, const SwizzleDataAccess<ElementType, srcRows, columns, SrcSwizzleDesc> &src)
						{
							return (DetectRowVsMatrixWARHazard<DstSwizzleDesc, bool(dstRows), SrcSwizzleDesc, bool(srcRows), rowIdx, assign>::value
								&& GetRowAddress<rowIdx>(dst) == GetRowAddress<rowIdx>(src)) | TriggerWARHazard<assign, rowIdx + 1>(dst, src);
						}

						// served both for unmatched swizzles and as terminator for mixed vector/matrix swizzles
						template
						<
							bool assign, unsigned rowIdx = 0,
							typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc,
							typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc
						>
						static inline bool TriggerWARHazard(SwizzleDataAccess<DstElementType, dstRows, dstColumns, DstSwizzleDesc> &, const SwizzleDataAccess<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &)
						{
							return false;
						}

#						pragma region scalar
							// TODO: use C++17 fold expression in place of recursive inline function calls\
							it can improve performance in debug builds where it is common for compiler to not inline functions

							// swizzle
							template<unsigned idx = 0, typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc>
							static inline enable_if_t<(idx < DstSwizzleDesc::dimension - 1), bool> TriggerScalarWARHazard(Swizzle<DstElementType, dstRows, dstColumns, DstSwizzleDesc> &dst, const void *src)
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

#if !defined _MSC_VER || defined __clang__
			namespace ElementsCountHelpers
			{
				// scalar
				template<PureScalar Type>
				static integral_constant<unsigned int, 1u> ElementsCountHelper(const Type &);

				// swizzle
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				static integral_constant<unsigned int, SwizzleDesc::dimension> ElementsCountHelper(const Swizzle<ElementType, rows, columns, SwizzleDesc> &);

				// matrix
				template<typename ElementType, unsigned int rows, unsigned int columns>
				static integral_constant<unsigned int, rows * columns> ElementsCountHelper(const matrix<ElementType, rows, columns> &);
			}

			template<typename ...Args>
			static constexpr unsigned int ElementsCount = (decltype(ElementsCountHelpers::ElementsCountHelper(declval<Args>()))::value + ...);
#endif

			struct DataCommon
			{
				enum class InitType
				{
					Copy,
					Scalar,
					Array,
					Sequencing,
				};

				template<InitType, class IdxSeq, unsigned offset = 0>
				class InitTag {};

#if defined _MSC_VER && !defined __clang__
			private:
				// scalar
				template<PureScalar Type>
				static integral_constant<unsigned int, 1u> ElementsCountHelper(const Type &);

				// swizzle
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				static integral_constant<unsigned int, SwizzleDesc::dimension> ElementsCountHelper(const Swizzle<ElementType, rows, columns, SwizzleDesc> &);

				// matrix
				template<typename ElementType, unsigned int rows, unsigned int columns>
				static integral_constant<unsigned int, rows * columns> ElementsCountHelper(const matrix<ElementType, rows, columns> &);

			public:
				template<typename ...Args>
				static constexpr unsigned int ElementsCount = (decltype(ElementsCountHelper(declval<Args>()))::value + ...);
#endif

			private:
				// scalar
				template<unsigned idx, PureScalar SrcType>
				static inline decltype(auto) GetElementImpl(const SrcType &scalar) noexcept
				{
					return scalar;
				}

				// swizzle
				template<unsigned idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				static inline decltype(auto) GetElementImpl(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) noexcept
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
				static inline decltype(auto) GetElementFind(const First &first, const Rest &...) noexcept
					requires (idx < ElementsCount<const First &>)
				{
					return GetElementImpl<idx>(first);
				}

				// next
				template<unsigned idx, typename First, typename ...Rest>
				static inline decltype(auto) GetElementFind(const First &, const Rest &...rest) noexcept
					requires (idx >= ElementsCount<const First &>)
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
			};

			// generic for matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			struct Data final : DataCommon
			{
				vector<ElementType, columns> rowsData[rows];

			public:
				Data() = default;

			public:	// matrix specific ctors
				template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				Data(InitTag<InitType::Copy, index_sequence<rowIdx...>>, const matrix<SrcElementType, srcRows, srcColumns> &src);

				template<size_t ...rowIdx, typename SrcElementType>
				Data(InitTag<InitType::Scalar, index_sequence<rowIdx...>>, const SrcElementType &scalar);

				template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				Data(InitTag<InitType::Array, index_sequence<rowIdx...>>, const SrcElementType (&src)[srcRows][srcColumns]);

				template<size_t ...rowIdx, typename ...Args>
				Data(InitTag<InitType::Sequencing, index_sequence<rowIdx...>>, const Args &...args);
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline Data<ElementType, rows, columns>::Data(InitTag<InitType::Copy, index_sequence<rowIdx...>>, const matrix<SrcElementType, srcRows, srcColumns> &src) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Copy, make_index_sequence<columns>>(), src[rowIdx])... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType>
			inline Data<ElementType, rows, columns>::Data(InitTag<InitType::Scalar, index_sequence<rowIdx...>>, const SrcElementType &scalar) :
				rowsData{ (rowIdx, scalar)... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline Data<ElementType, rows, columns>::Data(InitTag<InitType::Array, index_sequence<rowIdx...>>, const SrcElementType (&src)[srcRows][srcColumns]) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Array, make_index_sequence<columns>>(), src[rowIdx])... } {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename ...Args>
			inline Data<ElementType, rows, columns>::Data(InitTag<InitType::Sequencing, index_sequence<rowIdx...>>, const Args &...args) :
				rowsData{ vector<ElementType, columns>(InitTag<InitType::Sequencing, make_index_sequence<columns>, rowIdx * columns>(), args...)... } {}

			// specialization for vector
			template<typename ElementType, unsigned int dimension>
			struct Data<ElementType, 0, dimension> final : DataCommon
			{
				ElementType data[dimension];

			public:
				Data() = default;

			public:	// vector specific ctors
				template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				Data(InitTag<InitType::Copy, index_sequence<idx...>>, const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

				template<size_t ...idx, typename SrcElementType>
				Data(InitTag<InitType::Scalar, index_sequence<idx...>>, const SrcElementType &scalar);

				template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
				Data(InitTag<InitType::Array, index_sequence<idx...>>, const SrcElementType (&src)[srcDimension]);

				template<size_t ...idx, unsigned offset, typename ...Args>
				Data(InitTag<InitType::Sequencing, index_sequence<idx...>, offset>, const Args &...args);
			};

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline Data<ElementType, 0, dimension>::Data(InitTag<InitType::Copy, index_sequence<idx...>>, const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				data{ static_cast<const ElementType &>(src[idx])... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType>
			inline Data<ElementType, 0, dimension>::Data(InitTag<InitType::Scalar, index_sequence<idx...>>, const SrcElementType &scalar) :
				data{ (idx, static_cast<const ElementType &>(scalar))... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
			inline Data<ElementType, 0, dimension>::Data(InitTag<InitType::Array, index_sequence<idx...>>, const SrcElementType (&src)[srcDimension]) :
				data{ static_cast<const ElementType &>(src[idx])... } {}

			template<typename ElementType, unsigned int dimension>
			template<size_t ...idx, unsigned offset, typename ...Args>
			inline Data<ElementType, 0, dimension>::Data(InitTag<InitType::Sequencing, index_sequence<idx...>, offset>, const Args &...args) :
				data{ static_cast<const ElementType &>(GetElement<idx + offset>(args...))... } {}

			// generic vector/matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			struct DataContainer
			{
				Data<ElementType, rows, columns> data;

			protected:
				DataContainer() = default;
				DataContainer(const DataContainer &) = default;
				DataContainer(DataContainer &&) = default;
				template<typename ...Args> DataContainer(const Args &...args) : data(args...) {}
				DataContainer &operator =(const DataContainer &) & = default;
				DataContainer &operator =(DataContainer &&) & = default;
			};

#			pragma region Initializer list
#if INIT_LIST_SUPPORT_TIER > 0
#if INIT_LIST_ITEM_COPY
				template<typename ElementType, unsigned int capacity>
				class InitListItem final
				{
					InitListItem() = delete;
					InitListItem(const InitListItem &) = delete;
					InitListItem &operator =(const InitListItem &) = delete;

				private:
					template<size_t ...idx, typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr InitListItem(index_sequence<idx...>, const Swizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						itemStore{ static_cast<const ElementType &>(item[idx])... },
						itemSize(sizeof...(idx))
					{}

					template<size_t ...idx, typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr InitListItem(index_sequence<idx...>, const matrix<ItemElementType, itemRows, itemColumns> &item) :
						itemStore{ static_cast<const ElementType &>(item[idx / itemColumns][idx % itemColumns])... },
						itemSize(sizeof...(idx))
					{}

				public:
					template<PureScalar ItemElementType>
					constexpr InitListItem(const ItemElementType &item) :
						itemStore{ static_cast<const ElementType &>(item) },
						itemSize(1)
					{}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr InitListItem(const Swizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						InitListItem(make_index_sequence<std::min(ItemSwizzleDesc::dimension, capacity)>(), item)
					{
						static_assert(ItemSwizzleDesc::dimension <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr InitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) :
						InitListItem(make_index_sequence<std::min(itemRows * itemColumns, capacity)>(), item)
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
				class InitListItem final
				{
					InitListItem() = delete;
					InitListItem(const InitListItem &) = delete;
					InitListItem &operator =(const InitListItem &) = delete;

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
					template<PureScalar ItemElementType>
					constexpr InitListItem(const ItemElementType &item) :
						getItemElement(GetItemElement<ItemElementType>),
						item(&item),
						itemSize(1)
					{}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
					constexpr InitListItem(const Swizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
						getItemElement(GetItemElement<ItemElementType, ItemSwizzleDesc::dimension>),
						item(&item),
						itemSize(ItemSwizzleDesc::dimension)
					{
						static_assert(ItemSwizzleDesc::dimension <= capacity, INIT_LIST_ITEM_OVERFLOW_MSG);
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					constexpr InitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) :
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
#ifdef MSVC_PREPROCESSOR_WORKAROUND
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
#
#		define IDX_SEQ_2_SYMBOLS_1(xform, i0) xform i0
#		define IDX_SEQ_2_SYMBOLS_2(xform, i0, i1) CONCAT(IDX_SEQ_2_SYMBOLS_1(xform, i0), xform i1)
#		define IDX_SEQ_2_SYMBOLS_3(xform, i0, i1, i2) CONCAT(IDX_SEQ_2_SYMBOLS_2(xform, i0, i1), xform i2)
#		define IDX_SEQ_2_SYMBOLS_4(xform, i0, i1, i2, i3) CONCAT(IDX_SEQ_2_SYMBOLS_3(xform, i0, i1, i2), xform i3)
#ifdef MSVC_PREPROCESSOR_WORKAROUND
#		define IDX_SEQ_2_SYMBOLS(swizDim, xform, ...) CONCAT(IDX_SEQ_2_SYMBOLS_, swizDim(xform, __VA_ARGS__))
#else
#		define IDX_SEQ_2_SYMBOLS(swizDim, xform, ...) IDX_SEQ_2_SYMBOLS_##swizDim(xform, __VA_ARGS__)
#endif
#		define GENERATE_SWIZZLE(rows, columns, swizDim, ...)				\
			Swizzle<ElementType, rows, columns, SwizzleDesc<__VA_ARGS__>>	\
				IDX_SEQ_2_SYMBOLS(swizDim, LOOKUP_SYMBOL_0, __VA_ARGS__),	\
				IDX_SEQ_2_SYMBOLS(swizDim, LOOKUP_SYMBOL_1, __VA_ARGS__);
#
#		define DEC_2_BIN_
#		define DEC_2_BIN_0 00
#		define DEC_2_BIN_1 01
#		define DEC_2_BIN_2 10
#		define DEC_2_BIN_3 11
#		define ENCODE_RC_IDX(r, c) (r ## c, CONCAT(0b, CONCAT(DEC_2_BIN_##r, DEC_2_BIN_##c)))
#
#		define SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, c4) GENERATE_SWIZZLE(rows, columns, 4, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1), ENCODE_RC_IDX(r2, c2), ENCODE_RC_IDX(r3, c4))
#		define SWIZZLE_3_ITERATE_COLUMN_1(rows, columns, r0, c0, r1, c1, r2, c2, r3) /*-----------------------terminate iteration-----------------------*/ SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 0)
#		define SWIZZLE_3_ITERATE_COLUMN_2(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_1(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 1)
#		define SWIZZLE_3_ITERATE_COLUMN_3(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_2(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 2)
#		define SWIZZLE_3_ITERATE_COLUMN_4(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_3(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_4_GENERATE(rows, columns, r0, c0, r1, c1, r2, c2, r3, 3)
#		define SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, r3) SWIZZLE_3_ITERATE_COLUMN_##columns(rows, columns, r0, c0, r1, c1, r2, c2, r3)
#		define SWIZZLE_3_ITERATE_ROW_0(rows, columns, r0, c0, r1, c1, r2, c2) /*-------------------terminate  iteration-------------------*/ SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, )
#		define SWIZZLE_3_ITERATE_ROW_1(rows, columns, r0, c0, r1, c1, r2, c2) /*-------------------terminate  iteration-------------------*/ SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 0)
#		define SWIZZLE_3_ITERATE_ROW_2(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_1(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 1)
#		define SWIZZLE_3_ITERATE_ROW_3(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_2(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 2)
#		define SWIZZLE_3_ITERATE_ROW_4(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_3(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_COLUMNS(rows, columns, r0, c0, r1, c1, r2, c2, 3)
#		define SWIZZLE_3_SELECTOR_3(rows, columns, r0, c0, r1, c1, r2, c2) GENERATE_SWIZZLE(rows, columns, 3, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1), ENCODE_RC_IDX(r2, c2))
#		define SWIZZLE_3_SELECTOR_4(rows, columns, r0, c0, r1, c1, r2, c2) SWIZZLE_3_ITERATE_ROW_##rows(rows, columns, r0, c0, r1, c1, r2, c2)
#		define SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, c2) SWIZZLE_3_SELECTOR_##swizDim(rows, columns, r0, c0, r1, c1, r2, c2)
#
#		define SWIZZLE_2_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1, c1, r2) /*-----------------------terminate  iteration-----------------------*/ SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 0)
#		define SWIZZLE_2_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 1)
#		define SWIZZLE_2_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 2)
#		define SWIZZLE_2_ITERATE_COLUMN_4(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_3_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1, r2, 3)
#		define SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, r2) SWIZZLE_2_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0, c0, r1, c1, r2)
#		define SWIZZLE_2_ITERATE_ROW_0(rows, columns, swizDim, r0, c0, r1, c1) /*--------------------terminate iteration--------------------*/ SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, )
#		define SWIZZLE_2_ITERATE_ROW_1(rows, columns, swizDim, r0, c0, r1, c1) /*--------------------terminate iteration--------------------*/ SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 0)
#		define SWIZZLE_2_ITERATE_ROW_2(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_1(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 1)
#		define SWIZZLE_2_ITERATE_ROW_3(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_2(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 2)
#		define SWIZZLE_2_ITERATE_ROW_4(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_3(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1, c1, 3)
#		define SWIZZLE_2_SELECTOR_2(rows, columns, r0, c0, r1, c1) GENERATE_SWIZZLE(rows, columns, 2, ENCODE_RC_IDX(r0, c0), ENCODE_RC_IDX(r1, c1))
#		define SWIZZLE_2_SELECTOR_3(rows, columns, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_##rows(rows, columns, 3, r0, c0, r1, c1)
#		define SWIZZLE_2_SELECTOR_4(rows, columns, r0, c0, r1, c1) SWIZZLE_2_ITERATE_ROW_##rows(rows, columns, 4, r0, c0, r1, c1)
#		define SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, c1) SWIZZLE_2_SELECTOR_##swizDim(rows, columns, r0, c0, r1, c1)
#
#		define SWIZZLE_1_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1) /*-------------------terminate  iteration-------------------*/ SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 0)
#		define SWIZZLE_1_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_1(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 1)
#		define SWIZZLE_1_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_2(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 2)
#		define SWIZZLE_1_ITERATE_COLUMN_4(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_3(rows, columns, swizDim, r0, c0, r1) SWIZZLE_2_SELECTOR(rows, columns, swizDim, r0, c0, r1, 3)
#		define SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, r1) SWIZZLE_1_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0, c0, r1)
#		define SWIZZLE_1_ITERATE_ROW_0(rows, columns, swizDim, r0, c0) /*----------------terminate iteration----------------*/ SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, )
#		define SWIZZLE_1_ITERATE_ROW_1(rows, columns, swizDim, r0, c0) /*----------------terminate iteration----------------*/ SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 0)
#		define SWIZZLE_1_ITERATE_ROW_2(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_1(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 1)
#		define SWIZZLE_1_ITERATE_ROW_3(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_2(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 2)
#		define SWIZZLE_1_ITERATE_ROW_4(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_ROW_3(rows, columns, swizDim, r0, c0) SWIZZLE_1_ITERATE_COLUMNS(rows, columns, swizDim, r0, c0, 3)
#		define SWIZZLE_1_SELECTOR_1(rows, columns, r0, c0) GENERATE_SWIZZLE(rows, columns, 1, ENCODE_RC_IDX(r0, c0))
#		define SWIZZLE_1_SELECTOR_2(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 2, r0, c0)
#		define SWIZZLE_1_SELECTOR_3(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 3, r0, c0)
#		define SWIZZLE_1_SELECTOR_4(rows, columns, r0, c0) SWIZZLE_1_ITERATE_ROW_##rows(rows, columns, 4, r0, c0)
#		define SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, c0) SWIZZLE_1_SELECTOR_##swizDim(rows, columns, r0, c0)
#
#		define SWIZZLE_0_ITERATE_COLUMN_1(rows, columns, swizDim, r0) /*---------------terminate  iteration---------------*/ SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 0)
#		define SWIZZLE_0_ITERATE_COLUMN_2(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_1(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 1)
#		define SWIZZLE_0_ITERATE_COLUMN_3(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_2(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 2)
#		define SWIZZLE_0_ITERATE_COLUMN_4(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_3(rows, columns, swizDim, r0) SWIZZLE_1_SELECTOR(rows, columns, swizDim, r0, 3)
#		define SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, r0) SWIZZLE_0_ITERATE_COLUMN_##columns(rows, columns, swizDim, r0)
#		define SWIZZLE_0_ITERATE_ROW_0(rows, columns, swizDim) /*------------terminate iteration------------*/ SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, )
#		define SWIZZLE_0_ITERATE_ROW_1(rows, columns, swizDim) /*------------terminate iteration------------*/ SWIZZLE_0_ITERATE_COLUMNS(rows, columns, swizDim, 0)
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
#ifdef MSVC_PREPROCESSOR_WORKAROUND
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
#				define FRIEND_DECLARATIONS																								\
					template<unsigned rowIdx, typename ElementType, unsigned int columns, class SwizzleDesc>							\
					friend inline const void *GetRowAddress(const SwizzleDataAccess<ElementType, 0, columns, SwizzleDesc> &swizzle);	\
																																		\
					template<unsigned rowIdx, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>			\
					friend inline const void *GetRowAddress(const SwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> &swizzle);
#			endif

			// Swizzle inherits from this to reduce preprocessor generated code for faster compiling
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class SwizzleDataAccess
			{
				typedef Swizzle<ElementType, rows, columns, SwizzleDesc> SwizzleAlias;

			protected:
				SwizzleDataAccess() = default;
				SwizzleDataAccess(const SwizzleDataAccess &) = delete;
				~SwizzleDataAccess() = default;
				SwizzleDataAccess &operator =(const SwizzleDataAccess &) = delete;

			public:
				typedef SwizzleAlias OperationResult;

			protected:
				operator SwizzleAlias &() & noexcept
				{
					return static_cast<SwizzleAlias &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &AccessData() const noexcept
				{
					/*
									static	reinterpret
									  ^			 ^
									  |			 |
					SwizzleDataAccess -> Swizzle -> Data
					*/
					typedef Data<ElementType, rows, columns> Data;
					return reinterpret_cast<const Data *>(static_cast<const SwizzleAlias *>(this))->rowsData;
				}

			public:
				const ElementType &operator [](unsigned int idx) const & noexcept
				{
					assert(idx < SwizzleDesc::dimension);
					idx = SwizzleDesc::FetchIdx(idx);
					const auto row = idx >> 2u & 3u, column = idx & 3u;
					return AccessData()[row][column];
				}

				ElementType &operator [](unsigned int idx) & noexcept
				{
					return const_cast<ElementType &>(static_cast<const SwizzleDataAccess &>(*this)[idx]);
				}
			};

			// specialization for vectors
			template<typename ElementType, unsigned int vectorDimension, class SwizzleDesc>
			class SwizzleDataAccess<ElementType, 0, vectorDimension, SwizzleDesc>
			{
				/*
				?

				does not work with gcc:
				typedef Swizzle<...> Swizzle;
				typedef DataContainer<...> DataContainer;

				does not work with VS and gcc:
				typedef vector<...> vector;

				works with both VS and gcc (differs from cases above in that it is in function scope, not in class one):
				typedef SwizzleTraits<...> SwizzleTraits;
				*/
				typedef Swizzle<ElementType, 0, vectorDimension, SwizzleDesc> SwizzleAlias;

			protected:
				SwizzleDataAccess() = default;
				SwizzleDataAccess(const SwizzleDataAccess &) = delete;
				~SwizzleDataAccess() = default;
				SwizzleDataAccess &operator =(const SwizzleDataAccess &) = delete;

			public:
				typedef SwizzleAlias OperationResult;

			protected:
				operator SwizzleAlias &() & noexcept
				{
					return static_cast<SwizzleAlias &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &AccessData() const noexcept
				{
					/*
									static	reinterpret
									  ^			 ^
									  |			 |
					SwizzleDataAccess -> Swizzle -> Data
					*/
					typedef Data<ElementType, 0, vectorDimension> Data;
					return reinterpret_cast<const Data *>(static_cast<const SwizzleAlias *>(this))->data;
				}

			public:
				const ElementType &operator [](unsigned int idx) const & noexcept
				{
					assert(idx < SwizzleDesc::dimension);
					idx = SwizzleDesc::FetchIdx(idx);
					return AccessData()[idx];
				}

				ElementType &operator [](unsigned int idx) & noexcept
				{
					return const_cast<ElementType &>(static_cast<const SwizzleDataAccess &>(*this)[idx]);
				}
			};

			/*
			VectorSwizzleDesc<vectorDimension> required for VS 2013/2015/2017
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class SwizzleDataAccess<ElementType, 0, vectorDimension, VectorSwizzleDesc<vectorDimension>>
			{
				/*
								static
								  ^
								  |
				SwizzleDataAccess -> vector
				*/
				typedef vector<ElementType, vectorDimension> Vector;

			protected:
				SwizzleDataAccess() = default;
				SwizzleDataAccess(const SwizzleDataAccess &) = default;
				~SwizzleDataAccess() = default;
				SwizzleDataAccess &operator =(const SwizzleDataAccess &) & = default;

				// TODO: consider adding 'op=' operators to friends and making some stuff below protected/private (and OperationResult for other SwizzleDataAccess above)
			public:
				typedef Vector OperationResult;

				operator Vector &() & noexcept
				{
					return static_cast<Vector &>(*this);
				}

			private:
				FRIEND_DECLARATIONS

				const auto &AccessData() const noexcept
				{
					return static_cast<const Vector *>(this)->data.data;
				}

				auto &AccessData() noexcept
				{
					return static_cast<Vector *>(this)->data.data;
				}

			public:
				const ElementType &operator [](unsigned int idx) const & noexcept
				{
					assert(idx < vectorDimension);
					return AccessData()[idx];
				}

				ElementType &operator [](unsigned int idx) & noexcept
				{
					assert(idx < vectorDimension);
					return AccessData()[idx];
				}
			};

#			undef FRIEND_DECLARATIONS

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class SwizzleAssign<ElementType, rows, columns, SwizzleDesc, false_type> : public SwizzleDataAccess<ElementType, rows, columns, SwizzleDesc>
			{
			protected:
				SwizzleAssign() = default;
				SwizzleAssign(const SwizzleAssign &) = delete;
				~SwizzleAssign() = default;

			public:
				void operator =(const SwizzleAssign &) = delete;
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			class SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type> : public SwizzleDataAccess<ElementType, rows, columns, SwizzleDesc>
			{
				typedef SwizzleDataAccess<ElementType, rows, columns, SwizzleDesc> SwizzleDataAccess;
				typedef Swizzle<ElementType, rows, columns, SwizzleDesc> SwizzleAlias;

				// TODO: remove 'public'
			public:
				using typename SwizzleDataAccess::OperationResult;

			protected:
				SwizzleAssign() = default;
				SwizzleAssign(const SwizzleAssign &) = default;
				~SwizzleAssign() = default;

#ifndef MSVC_LIMITATIONS
			public:
				operator ElementType &() & noexcept
				{
					return (*this)[0];
				}
#endif

				template<size_t ...idx, typename SrcType>
				void AssignScalar(index_sequence<idx...>, const SrcType &scalar) &;

			public:
				inline OperationResult &operator =(const SwizzleAssign &src) &
				{
					return operator =(move(static_cast<const SwizzleAlias &>(src)));
				}

				// currently public to allow user specify WAR hazard explicitly if needed

				template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				enable_if_t<(!WARHazard && SrcSwizzleDesc::dimension > 1), OperationResult &> operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;

				template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				enable_if_t<(WARHazard && SrcSwizzleDesc::dimension > 1), OperationResult &> operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				enable_if_t<(SrcSwizzleDesc::dimension > 1), OperationResult &> operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
				{
					constexpr auto WARHazard = DetectSwizzleWARHazard
					<
						ElementType, rows, columns, SwizzleDesc,
						SrcElementType, srcRows, srcColumns, SrcSwizzleDesc,
						true
					>::value;
					return operator =<WARHazard>(src);
				}

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				enable_if_t<(SrcSwizzleDesc::dimension > 1), OperationResult &> operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &
				{
					return operator =<false>(src);
				}

				template<Scalar SrcType>
				inline OperationResult &operator =(const SrcType &scalar) &;

				template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				enable_if_t<(srcRows > 1 || srcColumns > 1), OperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &;

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				enable_if_t<(srcRows > 1 || srcColumns > 1), OperationResult &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &&src) &
				{
					return operator =<false>(src);
				}

#if INIT_LIST_SUPPORT_TIER >= 1
				inline OperationResult &operator =(initializer_list<InitListItem<ElementType, SwizzleDesc::dimension>> initList) &;
#endif

			private:
				template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
				void AssignSwizzle(index_sequence<idx...>, const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;

			public:
				using SwizzleDataAccess::operator [];
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline auto SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
				-> enable_if_t<(!WARHazard && SrcSwizzleDesc::dimension > 1), OperationResult &>
			{
				static_assert(SwizzleDesc::dimension <= SrcSwizzleDesc::dimension, "'vector = vector': too small src dimension");
				assert(!TriggerWARHazard<true>(*this, src));
				AssignSwizzle(make_index_sequence<SwizzleDesc::dimension>(), src);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline auto SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
				-> enable_if_t<(WARHazard && SrcSwizzleDesc::dimension > 1), OperationResult &>
			{
				// make copy and call direct assignment
				return operator =<false>(vector<SrcElementType, SwizzleDesc::dimension>(src));
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<Scalar SrcType>
			inline auto SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const SrcType &scalar) & -> OperationResult &
			{
				AssignScalar(make_index_sequence<SwizzleDesc::dimension>(), ExtractScalar(scalar));
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline auto SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &
				-> enable_if_t<(srcRows > 1 || srcColumns > 1), OperationResult &>
			{
				static_assert(sizeof...(WARHazard) <= 1);
				constexpr static const bool underflow = SwizzleDesc::dimension > srcRows * srcColumns, overflow = SwizzleDesc::dimension < srcRows * srcColumns;
				static_assert(!(underflow || overflow && SwizzleDesc::dimension > 1), "'vector = matrix': unmatched sequencing");
				const auto &seq = reinterpret_cast<const SequencingSwizzle<SrcElementType, srcRows, srcColumns> &>(src.data);
				return operator =<WARHazard...>(seq);
			}

#if INIT_LIST_SUPPORT_TIER >= 1
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::operator =(initializer_list<InitListItem<ElementType, SwizzleDesc::dimension>> initList) & -> OperationResult &
			{
				unsigned dstIdx = 0;
				for (const auto &item : initList)
					for (unsigned itemEementIdx = 0; itemEementIdx < item.GetItemSize(); itemEementIdx++)
						(*this)[dstIdx++] = item[itemEementIdx];
				assert(dstIdx == SwizzleDesc::dimension);
				return *this;
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline void SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::AssignSwizzle(index_sequence<idx...>, const Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
			{
				(((*this)[idx] = src[idx]), ...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			template<size_t ...idx, typename SrcType>
			inline void SwizzleAssign<ElementType, rows, columns, SwizzleDesc, true_type>::AssignScalar(index_sequence<idx...>, const SrcType &scalar) &
			{
				(((*this)[idx] = scalar), ...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = VectorSwizzleDesc<columns>>
			class EBCO SwizzleCommon :
				public SwizzleAssign<ElementType, rows, columns, SwizzleDesc>,
				public Tag<is_same_v<SwizzleDesc, VectorSwizzleDesc<columns>> ? TagName::Vector : TagName::Swizzle, SwizzleDesc::dimension == 1>
			{
			protected:
				SwizzleCommon() = default;
				SwizzleCommon(const SwizzleCommon &) = default;
				~SwizzleCommon() = default;
				SwizzleCommon &operator =(const SwizzleCommon &) & = default;

			public:
#ifdef MSVC_LIMITATIONS
				operator const ElementType &() const & noexcept
				{
					return (*this)[0];
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
				template<size_t ...idx, class F>
				inline auto Op(index_sequence<idx...>, F f) const
				{
					return vector<decay_t<decltype(f(declval<ElementType>()))>, SwizzleDesc::dimension>(f((*this)[idx])...);
				}

			public:
				auto operator +() const;
				auto operator -() const;
				auto operator !() const;

			public:
				template<typename F>
				vector<result_of_t<F &(ElementType)>, SwizzleDesc::dimension> apply(F f) const;

				template<typename Result>
				vector<Result, SwizzleDesc::dimension> apply(Result __cdecl f(ElementType)) const
				{
					return apply<Result __cdecl(ElementType)>(f);
				}

				template<typename Result>
				vector<Result, SwizzleDesc::dimension> apply(Result __cdecl f(const ElementType &)) const
				{
					return apply<Result __cdecl(const ElementType &)>(f);
				}

				vector<ElementType, SwizzleDesc::dimension> apply(ElementType __cdecl f(ElementType)) const
				{
					return apply<ElementType>(f);
				}

				vector<ElementType, SwizzleDesc::dimension> apply(ElementType __cdecl f(const ElementType &)) const
				{
					return apply<ElementType>(f);
				}

			private:
				template<typename F, size_t ...idx>
				inline vector<result_of_t<F &(ElementType)>, SwizzleDesc::dimension> apply(F f, index_sequence<idx...>) const;
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto SwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator +() const
			{
				return Op(make_index_sequence<SwizzleDesc::dimension>(), positive());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto SwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator -() const
			{
				return Op(make_index_sequence<SwizzleDesc::dimension>(), std::negate());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto SwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator !() const
			{
				return Op(make_index_sequence<SwizzleDesc::dimension>(), std::logical_not());
			}

#ifdef MSVC_NAMESPACE_WORKAROUND
		}
#define NAMESPACE_PREFIX Impl::
#else
#define NAMESPACE_PREFIX
#endif
			// this specialization used as base class for DataContainer to eliminate need for various overloads
			/*
			VectorSwizzleDesc<vectorDimension> required for VS 2013/2015/2017
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class Swizzle<ElementType, 0, vectorDimension, NAMESPACE_PREFIX VectorSwizzleDesc<vectorDimension>> : public NAMESPACE_PREFIX SwizzleCommon<ElementType, 0, vectorDimension>
			{
			protected:
				Swizzle() = default;
				Swizzle(const Swizzle &) = default;
				~Swizzle() = default;
				Swizzle &operator =(const Swizzle &) & = default;
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, typename>
			class Swizzle final : public NAMESPACE_PREFIX SwizzleCommon<ElementType, rows, columns, SwizzleDesc>
			{
				friend struct NAMESPACE_PREFIX DataContainer<ElementType, rows, columns>;

			public:
				Swizzle &operator =(const Swizzle &) & = default;
				using NAMESPACE_PREFIX SwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =;

			private:
				Swizzle() = default;
				Swizzle(const Swizzle &) = delete;
				~Swizzle() = default;
			};

#undef NAMESPACE_PREFIX
#ifdef MSVC_NAMESPACE_WORKAROUND
		namespace Impl
		{
#endif

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

			template<class Src, typename LeftElementType, typename RightElementType, bool force>
			using _1xN_2_vec = conditional_t<force || !is_same_v<decay_t<LeftElementType>, decay_t<RightElementType>>, typename _1xN_2_vec_impl<Src>::type, Src>;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec
			>
			class MatrixOpMatrixResultImpl
			{
				constexpr static const bool dimensionalMismatch =
#if ENABLE_UNMATCHED_MATRICES
					false;
#else
					!(leftRows <= rightRows && leftColumns <= rightColumns || leftRows >= rightRows && leftColumns >= rightColumns);
#endif
				typedef matrix<TargetElementType, std::min(leftRows, rightRows), std::min(leftColumns, rightColumns)> ResultMatrix;
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
				typename TargetElementType, bool force_1xN_2_vec
			>
			using MatrixOpMatrixResult = typename MatrixOpMatrixResultImpl<LeftElementType, leftRows, leftColumns, RightElementType, rightRows, rightColumns, TargetElementType, force_1xN_2_vec>::type;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightType,
				typename TargetElementType, bool force_1xN_2_vec
			>
			using MatrixOpScalarResult = _1xN_2_vec<matrix<TargetElementType, leftRows, leftColumns>, LeftElementType, decltype(ExtractScalar(declval<RightType>())), force_1xN_2_vec>;

			template
			<
				typename LeftType,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec
			>
			using ScalarOpMatrixResult = conditional_t<rightRows * rightColumns == 1,
				vector<decay_t<decltype(ExtractScalar(declval<LeftType>()))>, 1>,
				_1xN_2_vec<matrix<TargetElementType, rightRows, rightColumns>, decltype(ExtractScalar(declval<LeftType>())), RightElementType, force_1xN_2_vec>>;

			template
			<
				typename LeftElementType, unsigned int leftDimension,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns,
				typename TargetElementType, bool force_1xN_2_vec
			>
			using SwizzleOpMatrixResult = conditional_t<leftDimension <= rightRows * rightColumns,
				vector<TargetElementType, leftDimension>,
				_1xN_2_vec<matrix<TargetElementType, rightRows, rightColumns>, LeftElementType, RightElementType, force_1xN_2_vec>>;

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightDimension,
				typename TargetElementType, bool force_1xN_2_vec
			>
			using MatrixOpSwizzleResult = conditional_t<leftRows * leftColumns <= rightDimension,
				_1xN_2_vec<matrix<TargetElementType, leftRows, leftColumns>, LeftElementType, RightElementType, force_1xN_2_vec>,
				vector<TargetElementType, rightDimension>>;
		}

#		pragma region generate operators
#			pragma region swizzle

				namespace Impl
				{
					template
					<
						size_t ...idx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
					>
					inline void SwizzleOpAssignSwizzle(
						index_sequence<idx...>, F f,
						Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
						const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
					{
						assert(!TriggerWARHazard<false>(left, right));
						(f(left[idx], right[idx]), ...);
					}
				}

				// swizzle / 1D swizzle op=<!WARHazard> swizzle
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<!WARHazard && (RightSwizzleDesc::dimension > 1),															\
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
					{																																	\
						static_assert(LeftSwizzleDesc::isWriteMaskValid, "'vector "#op"= vector': invalid write mask");									\
						static_assert(LeftSwizzleDesc::dimension <= RightSwizzleDesc::dimension, "'vector "#op"= vector': too small src dimension");	\
						Impl::SwizzleOpAssignSwizzle(std::make_index_sequence<LeftSwizzleDesc::dimension>(), Impl::F##_assign(), left, right);			\
						return left;																													\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
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
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
					{																																	\
						return operator op##=<false>(left, vector<RightElementType, LeftSwizzleDesc::dimension>(right));								\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1),																			\
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
					{																																	\
						constexpr auto WARHazard = Impl::DetectSwizzleWARHazard																			\
						<																																\
							LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																	\
							RightElementType, rightRows, rightColumns, RightSwizzleDesc,																\
							false																														\
						>::value;																														\
						return operator op##=<WARHazard>(left, right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline std::enable_if_t<(RightSwizzleDesc::dimension > 1),																			\
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)										\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				namespace Impl::ScalarOps
				{
					template
					<
						size_t ...idx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
						typename RightType
					>
					inline void SwizzleOpAssignScalar(
						index_sequence<idx...>, F f,
						Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
						const RightType &right)
					{
						assert(!TriggerScalarWARHazard(left, &right));
						(f(left[idx], right), ...);
					}

					// swizzle / 1D swizzle op=<!WARHazard, !extractScalar> scalar
#					define OPERATOR_DEFINITION(op, F)																									\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar,																							\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<!WARHazard && !extractScalar/* && Scalar<RightType>*/,														\
						typename Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
							Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
							const RightType &right)																										\
						{																																\
							static_assert(LeftSwizzleDesc::isWriteMaskValid, "'vector "#op"= scalar': invalid write mask");								\
							SwizzleOpAssignScalar(make_index_sequence<LeftSwizzleDesc::dimension>(), F##_assign(), left, right);						\
							return left;																												\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#					undef OPERATOR_DEFINITION

					// swizzle / 1D swizzle op=<WARHazard, !extractScalar> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar,																							\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<WARHazard && !extractScalar/* && Scalar<RightType>*/,														\
						typename Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
							Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
							const RightType &right)																										\
						{																																\
							return operator op##=<false, false>(left, RightType(right));																\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#					undef OPERATOR_DEFINITION

					// swizzle / 1D swizzle op=<?WARHazard, extractScalar> scalar
#					define OPERATOR_DEFINITION(op)																										\
						template																														\
						<																																\
							bool WARHazard, bool extractScalar = true,																					\
							typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,							\
							typename RightType																											\
						>																																\
						inline enable_if_t<extractScalar/* && Scalar<RightType>*/,																		\
						typename Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
							Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,														\
							const RightType &right)																										\
						{																																\
							return operator op##=<WARHazard, false>(left, ExtractScalar(right));														\
						}
					GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#					undef OPERATOR_DEFINITION
				}

				// swizzle / 1D swizzle op=<?WARHazard> scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						Impl::Scalar RightType																											\
					>																																	\
					inline typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &operator op##=(			\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const RightType &right)																											\
					{																																	\
						return Impl::ScalarOps::operator op##=<WARHazard>(left, right);																	\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						Impl::Scalar RightType																											\
					>																																	\
					inline typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &operator op##=(			\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const RightType &right)																											\
					{																																	\
						constexpr bool WARHazard =																										\
							Impl::DetectScalarWARHazard<Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>, RightType>::value;		\
						return operator op##=<WARHazard>(left, right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						Impl::Scalar RightType																											\
					>																																	\
					inline typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &operator op##=(			\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const RightType &&right)																										\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...idx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
					>
					inline auto SwizzleOpSwizzle(
						index_sequence<idx...>, F f,
						const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
						const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
						-> vector<decay_t<decltype(f(std::declval<LeftElementType>(), declval<RightElementType>()))>, sizeof...(idx)>
					{
						return{ AsScalar(f(left[idx], right[idx]))... };
					}
				}

				// swizzle op swizzle / 1D swizzle op 1D swizzle
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline auto operator op(																											\
						const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
						requires (LeftSwizzleDesc::dimension > 1 == RightSwizzleDesc::dimension > 1)													\
					{																																	\
						constexpr unsigned int dimension = std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension);							\
						return Impl::SwizzleOpSwizzle(std::make_index_sequence<dimension>(), std::F(), left, right);									\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...idx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
						typename RightType
					>
					inline auto SwizzleOpScalar(
						index_sequence<idx...>, F f,
						const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
						const RightType &right)
						-> vector<decay_t<decltype(f(std::declval<LeftElementType>(), declval<RightType>()))>, sizeof...(idx)>
					{
						return{ AsScalar(f(left[idx], right))... };
					}
				}

				// swizzle op scalar / 1D swizzle op pure scalar
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightType																												\
					>																																	\
					inline auto operator op(																											\
						const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
						const RightType &right)																											\
						requires (LeftSwizzleDesc::dimension > 1 ? Impl::Scalar<RightType> : Impl::PureScalar<RightType>)								\
					{																																	\
						using namespace Impl;																											\
						return SwizzleOpScalar(make_index_sequence<LeftSwizzleDesc::dimension>(), F(), left, ExtractScalar(right));						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...idx, class F,
						typename LeftType,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
					>
					inline auto ScalarOpSwizzle(
						index_sequence<idx...>, F f,
						const LeftType &left,
						const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
						-> vector<decay_t<decltype(f(std::declval<LeftType>(), declval<RightElementType>()))>, sizeof...(idx)>
					{
						return{ AsScalar(f(left, right[idx]))... };
					}
				}

				// scalar op swizzle / pure scalar op 1D swizzle
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline auto operator op(																											\
						const LeftType &left,																											\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
						requires (RightSwizzleDesc::dimension > 1 ? Impl::Scalar<LeftType> : Impl::PureScalar<LeftType>)								\
					{																																	\
						using namespace Impl;																											\
						return ScalarOpSwizzle(make_index_sequence<RightSwizzleDesc::dimension>(), F(), ExtractScalar(left), right);					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION
#			pragma endregion

#			pragma region matrix
				// matrix / 1x1 matrix op= matrix
#ifdef MSVC_LIMITATIONS
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
						requires (rightRows > 1 || rightColumns > 1)																					\
					{																																	\
						static_assert(leftRows <= rightRows, "'matrix "#op"= matrix': too few rows in src");											\
						static_assert(leftColumns <= rightColumns, "'matrix "#op"= matrix': too few columns in src");									\
						constexpr static const bool vecMatMismatch = std::is_void_v<Impl::MatrixOpMatrixResult<											\
							LeftElementType, leftRows, leftColumns,																						\
							RightElementType, rightRows, rightColumns,																					\
							LeftElementType, false>>;																									\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op"= matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op"= matrix1xN -> vectorN': cannot convert matrix to vector");	\
						left.OpAssignMatrix(std::make_index_sequence<leftRows>(), operator op##=, right);												\
						return left;																													\
					}
#else
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
						requires (rightRows > 1 || rightColumns > 1)																					\
					{																																	\
						static_assert(leftRows <= rightRows, "'matrix "#op"= matrix': too few rows in src");											\
						static_assert(leftColumns <= rightColumns, "'matrix "#op"= matrix': too few columns in src");									\
						constexpr static const bool vecMatMismatch = std::is_void_v<Impl::MatrixOpMatrixResult<											\
							LeftElementType, leftRows, leftColumns,																						\
							RightElementType, rightRows, rightColumns,																					\
							LeftElementType, false>>;																									\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op"= matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op"= matrix1xN -> vectorN': cannot convert matrix to vector");	\
						left.OpAssignMatrix(std::make_index_sequence<leftRows>(), operator op##=<false>, right);										\
						return left;																													\
					}
#endif
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op=<?WARHazard> scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool WARHazard,																													\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						Impl::Scalar RightType																											\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const RightType &right)																											\
					{																																	\
						return left.template operator op##=<WARHazard>(right);																			\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						Impl::Scalar RightType																											\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const RightType &right)																											\
					{																																	\
						constexpr bool WARHazard = Impl::DetectScalarWARHazard<matrix<LeftElementType, leftRows, leftColumns>, RightType>::value;		\
						return operator op##=<WARHazard>(left, right);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= temp scalar
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						Impl::Scalar RightType																											\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const RightType &&right)																										\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...rowIdx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns
					>
					inline auto MatrixOpMatrix(
						index_sequence<rowIdx...>, F f,
						const matrix<LeftElementType, leftRows, leftColumns> &left,
						const matrix<RightElementType, rightRows, rightColumns> &right)
						-> MatrixOpMatrixResult
						<
							LeftElementType, leftRows, leftColumns,
							RightElementType, rightRows, rightColumns,
							decay_t<decltype(f(declval<LeftElementType>(), declval<RightElementType>()))>, IsMaskOp<F>
						>
					{
						return{ f(left[rowIdx], right[rowIdx])... };
					}
				}

				// matrix op matrix / 1x1 matrix op 1x1 matrix
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline auto operator op(																											\
						const matrix<LeftElementType, leftRows, leftColumns> &left,																		\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
						-> std::enable_if_t<(leftRows > 1 || leftColumns > 1) == (rightRows > 1 || rightColumns > 1),									\
						decltype(Impl::MatrixOpMatrix(std::make_index_sequence<std::min(leftRows, rightRows)>(), std::F(), left, right))>				\
					{																																	\
						typedef decltype(left op right) Result;																							\
						constexpr static const bool vecMatMismatch = std::is_void_v<Result>;															\
						static_assert(!std::is_null_pointer_v<Result>, "'matrix "#op" matrix': mismatched matrix dimensions");							\
						static_assert(!(vecMatMismatch && leftRows == 1), "'matrix1xN -> vectorN "#op" matrix': cannot convert matrix to vector");		\
						static_assert(!(vecMatMismatch && rightRows == 1), "'matrix "#op" matrix1xN -> vectorN': cannot convert matrix to vector");		\
						return Impl::MatrixOpMatrix(std::make_index_sequence<std::min(leftRows, rightRows)>(), std::F(), left, right);					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...rowIdx, class F,
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
						typename RightType
					>
					inline auto MatrixOpScalar(
						index_sequence<rowIdx...>, F f,
						const matrix<LeftElementType, leftRows, leftColumns> &left,
						const RightType &right)
						-> MatrixOpScalarResult
						<
							LeftElementType, leftRows, leftColumns,
							RightType,
							decay_t<decltype(f(declval<LeftElementType>(), declval<RightType>()))>, IsMaskOp<F>
						>
					{
						return{ f(left[rowIdx], right)... };
					}
				}

				// matrix op scalar / 1x1 matrix op pure scalar
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightType																												\
					>																																	\
					inline auto operator op(																											\
						const matrix<LeftElementType, leftRows, leftColumns> &left,																		\
						const RightType &right)																											\
						requires (leftRows > 1 || leftColumns > 1 ? Impl::Scalar<RightType> : Impl::PureScalar<RightType>)								\
					{																																	\
						return Impl::MatrixOpScalar(std::make_index_sequence<leftRows>(), std::F(), left, Impl::ExtractScalar(right));					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION

				namespace Impl
				{
					template
					<
						size_t ...rowIdx, class F,
						typename LeftType,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns
					>
					inline auto ScalarOpMatrix(
						index_sequence<rowIdx...>, F f,
						const LeftType &left,
						const matrix<RightElementType, rightRows, rightColumns> &right)
						-> ScalarOpMatrixResult
						<
							LeftType,
							RightElementType, rightRows, rightColumns,
							decay_t<decltype(f(declval<LeftType>(), declval<RightElementType>()))>, IsMaskOp<F>
						>
					{
						return{ f(left, right[rowIdx])... };
					}
				}

				// scalar op matrix / pure scalar op 1x1 matrix
#				define OPERATOR_DEFINITION(op, F)																										\
					template																															\
					<																																	\
						typename LeftType,																												\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline auto operator op(																											\
						const LeftType &left,																											\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
						requires (rightRows > 1 || rightColumns > 1 ? Impl::Scalar<LeftType> : Impl::PureScalar<LeftType>)								\
					{																																	\
						return Impl::ScalarOpMatrix(std::make_index_sequence<rightRows>(), std::F(), Impl::ExtractScalar(left), right);					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION
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
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
					{																																	\
						static_assert(sizeof...(WARHazard) <= 1);																						\
						constexpr static const bool																										\
							underflow = LeftSwizzleDesc::dimension > rightRows * rightColumns,															\
							overflow = LeftSwizzleDesc::dimension < rightRows * rightColumns;															\
						static_assert(!(underflow || overflow && LeftSwizzleDesc::dimension > 1), "'vector "#op"= matrix': unmatched sequencing");		\
						const auto &seq = reinterpret_cast<const Impl::SequencingSwizzle<RightElementType, rightRows, rightColumns> &>(right.data);		\
						return operator op##=<WARHazard...>(left, seq);																					\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle / 1D swizzle op= temp matrix
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,								\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns													\
					>																																	\
					inline std::enable_if_t<(rightRows > 1 || rightColumns > 1),																		\
					typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(					\
						Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,													\
						const matrix<RightElementType, rightRows, rightColumns> &&right)																\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						bool ...WARHazard,																												\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
						requires (RightSwizzleDesc::dimension > 1)																						\
					{																																	\
						static_assert(sizeof...(WARHazard) <= 1);																						\
						constexpr static const auto leftDimension = leftRows * leftColumns;																\
						constexpr static const bool																										\
							underflow = leftDimension > RightSwizzleDesc::dimension,																	\
							overflow = leftDimension < RightSwizzleDesc::dimension;																		\
						static_assert(!(underflow || overflow && leftDimension > 1), "'matrix "#op"= vector': unmatched sequencing");					\
						auto &seq = reinterpret_cast<Impl::SequencingSwizzle<LeftElementType, leftRows, leftColumns> &>(left.data);						\
						operator op##=<WARHazard...>(seq, right);																						\
						return left;																													\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// matrix / 1x1 matrix op= temp swizzle
#				define OPERATOR_DEFINITION(op)																											\
					template																															\
					<																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc							\
					>																																	\
					inline decltype(auto) operator op##=(																								\
						matrix<LeftElementType, leftRows, leftColumns> &left,																			\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)										\
						requires (RightSwizzleDesc::dimension > 1)																						\
					{																																	\
						return operator op##=<false>(left, right);																						\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#				undef OPERATOR_DEFINITION

				// swizzle op matrix / 1D swizzle op 1x1 matrix
#				define OPERATOR_DEFINITION(op, F)																										\
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
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>, Impl::IsMaskOp<std::F<>>			\
					>> operator op(																														\
						const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,												\
						const matrix<RightElementType, rightRows, rightColumns> &right)																	\
					{																																	\
						constexpr static const bool matched = LeftSwizzleDesc::dimension == rightRows * rightColumns;									\
						static_assert(matched || rightRows == 1 || rightColumns == 1, "'vector "#op" matrix': unmatched sequencing");					\
						const auto &seq = reinterpret_cast<const Impl::SequencingSwizzle<RightElementType, rightRows, rightColumns> &>(right.data);		\
						return left op seq;																												\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION

				// matrix op swizzle / 1x1 matrix op 1D swizzle
#				define OPERATOR_DEFINITION(op, F)																										\
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
						std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>, Impl::IsMaskOp<std::F<>>			\
					>> operator op(																														\
						const matrix<LeftElementType, leftRows, leftColumns> &left,																		\
						const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)										\
					{																																	\
						constexpr static const bool matched = leftRows * leftColumns == RightSwizzleDesc::dimension;									\
						static_assert(matched || leftRows == 1 || leftColumns == 1, "'matrix "#op" vector': unmatched sequencing");						\
						const auto &seq = reinterpret_cast<const Impl::SequencingSwizzle<LeftElementType, leftRows, leftColumns> &>(left.data);			\
						return seq op right;																											\
					}
				GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
				GENERATE_MASK_OPERATORS(OPERATOR_DEFINITION, F_2_PAIR)
#				undef OPERATOR_DEFINITION
#			pragma endregion
#		pragma endregion

#		pragma region min/max functions
			// std::min/max requires explicit template param if used for different types => provide scalar version\
			this version also has option to return copy or reference
#			define FUNCTION_DEFINITION(f)																							\
				template<bool copy = false, Impl::PureScalar LeftType, Impl::PureScalar RightType>									\
				inline std::conditional_t<copy,																						\
					std::common_type_t<LeftType, RightType>,																		\
					const std::common_type_t<LeftType, RightType> &>																\
				f(const LeftType &left, const RightType &right)																		\
				{																													\
					return std::f<std::common_type_t<LeftType, RightType>>(left, right);											\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...idx,																								\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc		\
					>																												\
					inline vector<decay_t<decltype(f(declval<LeftElementType>(), declval<RightElementType>()))>, sizeof...(idx)>	\
					f(index_sequence<idx...>,																						\
						const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
						const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)							\
					{																												\
						return{ f(left[idx], right[idx])... };																		\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,				\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc			\
				>																													\
				inline auto f(																										\
					const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)						\
				{																													\
					constexpr unsigned int dimension = std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension);			\
					return Impl::f(std::make_index_sequence<dimension>(), left, right);												\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...idx,																								\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,			\
						PureScalar RightType																						\
					>																												\
					inline vector<decay_t<decltype(f(declval<LeftElementType>(), declval<RightType>()))>, sizeof...(idx)>			\
					f(index_sequence<idx...>,																						\
						const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
						const RightType &right)																						\
					{																												\
						return{ f(left[idx], right)... };																			\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,				\
					Impl::PureScalar RightType																						\
				>																													\
				inline auto f(																										\
					const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
					const RightType &right)																							\
				{																													\
					return Impl::f(std::make_index_sequence<LeftSwizzleDesc::dimension>(), left, right);							\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...idx,																								\
						PureScalar LeftType,																						\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc		\
					>																												\
					inline vector<decay_t<decltype(f(declval<LeftType>(), declval<RightElementType>()))>, sizeof...(idx)>			\
					f(index_sequence<idx...>,																						\
						const LeftType &left,																						\
						const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)							\
					{																												\
						return{ f(left, right[idx])... };																			\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					Impl::PureScalar LeftType,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc			\
				>																													\
				inline auto f(																										\
					const LeftType &left,																							\
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)						\
				{																													\
					return Impl::f(std::make_index_sequence<RightSwizzleDesc::dimension>(), left, right);							\
				};
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...rowIdx,																							\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,									\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns								\
					>																												\
					inline matrix																									\
					<																												\
						decay_t<decltype(f(declval<LeftElementType>(), declval<RightElementType>()))>,								\
						std::min(leftRows, rightRows), std::min(leftColumns, rightColumns)											\
					>																												\
					f(index_sequence<rowIdx...>,																					\
						const matrix<LeftElementType, leftRows, leftColumns> &left,													\
						const matrix<RightElementType, rightRows, rightColumns> &right)												\
					{																												\
						return{ f(left[rowIdx], right[rowIdx])... };																\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,										\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns									\
				>																													\
				auto f(																												\
					const matrix<LeftElementType, leftRows, leftColumns> &left,														\
					const matrix<RightElementType, rightRows, rightColumns> &right)													\
				{																													\
					return Impl::f(std::make_index_sequence<std::min(leftRows, rightRows)>(), left, right);							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...rowIdx,																							\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,									\
						PureScalar RightType																						\
					>																												\
					inline matrix<decay_t<decltype(f(declval<LeftElementType>(), declval<RightType>()))>, leftRows, leftColumns>	\
					f(index_sequence<rowIdx...>,																					\
						const matrix<LeftElementType, leftRows, leftColumns> &left,													\
						const RightType &right)																						\
					{																												\
						return{ f(left[rowIdx], right)... };																		\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,										\
					Impl::PureScalar RightType																						\
				>																													\
				auto f(																												\
					const matrix<LeftElementType, leftRows, leftColumns> &left,														\
					const RightType &right)																							\
				{																													\
					return Impl::f(std::make_index_sequence<leftRows>(), left, right);												\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

			namespace Impl
			{
				using VectorMath::min;
				using VectorMath::max;

#				define FUNCTION_DEFINITION(f)																						\
					template																										\
					<																												\
						size_t ...rowIdx,																							\
						PureScalar LeftType,																						\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns								\
					>																												\
					inline matrix<decay_t<decltype(f(declval<LeftType>(), declval<RightElementType>()))>, rightRows, rightColumns>	\
					f(index_sequence<rowIdx...>,																					\
						const LeftType &left,																						\
						const matrix<RightElementType, rightRows, rightColumns> &right)												\
					{																												\
						return{ f(left, right[rowIdx])... };																		\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
			}

#			define FUNCTION_DEFINITION(f)																							\
				template																											\
				<																													\
					Impl::PureScalar LeftType,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns									\
				>																													\
				auto f(																												\
					const LeftType &left,																							\
					const matrix<RightElementType, rightRows, rightColumns> &right)													\
				{																													\
					return Impl::f(std::make_index_sequence<rightRows>(), left, right);												\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION
#		pragma endregion

#		pragma region all/any/none functions
			namespace Impl
			{
				template<size_t ...idx, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				inline bool all(index_sequence<idx...>, const Swizzle<ElementType, rows, columns, SwizzleDesc> &src)
				{
					return (src[idx] && ...);
				}

				template<size_t ...idx, typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
				inline bool any(index_sequence<idx...>, const Swizzle<ElementType, rows, columns, SwizzleDesc> &src)
				{
					return (src[idx] || ...);
				}

				template<size_t ...rowIdx, typename ElementType, unsigned int rows, unsigned int columns>
				inline bool all(index_sequence<rowIdx...>, const matrix<ElementType, rows, columns> &src)
				{
					return (all(src[rowIdx]) && ...);
				}

				template<size_t ...rowIdx, typename ElementType, unsigned int rows, unsigned int columns>
				inline bool any(index_sequence<rowIdx...>, const matrix<ElementType, rows, columns> &src)
				{
					return (any(src[rowIdx]) || ...);
				}
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline bool all(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src)
			{
				return Impl::all(std::make_index_sequence<SwizzleDesc::dimension>(), src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline bool any(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src)
			{
				return Impl::any(std::make_index_sequence<SwizzleDesc::dimension>(), src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline bool none(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &src)
			{
				return !any(src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline bool all(const matrix<ElementType, rows, columns> &src)
			{
				return Impl::all(std::make_index_sequence<rows>(), src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline bool any(const matrix<ElementType, rows, columns> &src)
			{
				return Impl::any(std::make_index_sequence<rows>(), src);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline bool none(const matrix<ElementType, rows, columns> &src)
			{
				return !any(src);
			}
#		pragma endregion TODO: consider bool SWAR (packed specializations for bool vectors/matrices and replace std::all/any/none functions/fold expressions with bit operations)

#		pragma region mul functions
			namespace Impl
			{
				template
				<
					size_t ...idx,
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
				>
				inline auto mul(index_sequence<idx...>,
					const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
					const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				{
					return ((left[idx] * right[idx]) + ...);
				}
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto mul(
				const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				constexpr unsigned int rowXcolumnDimension = std::min(LeftSwizzleDesc::dimension, RightSwizzleDesc::dimension);
				return Impl::mul(std::make_index_sequence<rowXcolumnDimension>(), left, right);
			}

			namespace Impl
			{
				using VectorMath::mul;

#ifdef MSVC_LIMITATIONS
				template<unsigned c, typename ElementType, unsigned int rows, unsigned int columns>
				inline decltype(auto) MatrixColumn(const matrix<ElementType, rows, columns> &src)
				{
					return src.template Column<c>();
				}

				template
				<
					size_t ...columnIdx,
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto mul(index_sequence<columnIdx...>,
					const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
					const matrix<RightElementType, rightRows, rightColumns> &right)
					-> vector<decltype(mul(left, MatrixColumn<0>(right))), sizeof...(columnIdx)>
				{
					return{ mul(left, MatrixColumn<columnIdx>(right))... };
				}
#else
				template
				<
					size_t ...columnIdx,
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto mul(index_sequence<columnIdx...>,
					const Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
					const matrix<RightElementType, rightRows, rightColumns> &right)
					-> vector<decltype(mul(left, right.template Column<0>())), sizeof...(columnIdx)>
				{
					return{ mul(left, right.template Column<columnIdx>())... };
				}
#endif
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns
			>
			auto mul(
				const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
			{
				return Impl::mul(std::make_index_sequence<rightColumns>(), left, right);
			}

			namespace Impl
			{
				using VectorMath::mul;

				template
				<
					size_t ...rowIdx,
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
				>
				inline auto mul(index_sequence<rowIdx...>,
					const matrix<LeftElementType, leftRows, leftColumns> &left,
					const Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
					-> vector<decltype(mul(left[0], right)), sizeof...(rowIdx)>
				{
					return{ mul(left[rowIdx], right)... };
				}
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			auto mul(
				const matrix<LeftElementType, leftRows, leftColumns> &left,
				const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				return Impl::mul(std::make_index_sequence<leftRows>(), left, right);
			}

			namespace Impl
			{
				using VectorMath::mul;

				template
				<
					size_t ...rowIdx,
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto mul(index_sequence<rowIdx...>,
					const matrix<LeftElementType, leftRows, leftColumns> &left,
					const matrix<RightElementType, rightRows, rightColumns> &right)
					-> matrix<typename decltype(mul(left[0], right))::ElementType, leftRows, rightColumns>
				{
					return{ mul(left[rowIdx], right)... };
				}
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
				return Impl::mul(std::make_index_sequence<leftRows>(), left, right);
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto dot(
				const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				return mul(left, right);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto length(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			{
				return sqrt(dot(swizzle, swizzle));
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto distance(
				const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				return length(right - left);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto normalize(const Impl::Swizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			{
				return swizzle / length(swizzle);
			}

#			pragma region "series of matrices delimited by ',' interpreted as series of successive transforms; inspirited by boost's function superposition"
				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
				>
				inline auto operator ,(
					const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				{
					return mul(left, right);
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto operator ,(
					const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
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
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
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
#		pragma endregion note: most of these functions are not inline

		template<typename ElementType_, unsigned int dimension_>
		class EBCO vector : public Impl::DataContainer<ElementType_, 0, dimension_>, public Impl::Swizzle<ElementType_, 0, dimension_>
		{
			static_assert(dimension_ > 0, "vector dimension should be positive");
			static_assert(Impl::IsElementTypeValid<ElementType_>, "invalid vector element type");
			typedef Impl::DataContainer<ElementType_, 0, dimension_> DataContainer;
			typedef Impl::Data<ElementType_, 0, dimension_> Data;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int dimension = dimension_;

			vector() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			vector(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<Impl::Scalar SrcType>
			vector(const SrcType &scalar);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			requires (srcRows > 1 || srcColumns > 1)
			vector(const matrix<SrcElementType, srcRows, srcColumns> &src);

			template<typename ...Args>
			requires (sizeof...(Args) > 1)
			vector(const Args &...args);

#if INIT_LIST_SUPPORT_TIER >= 2
			vector(std::initializer_list<Impl::InitListItem<ElementType, dimension>> initList);
#endif

			//template<typename Iterator>
			//explicit vector(Iterator src);

			template<typename SrcElementType, unsigned int srcDimension>
			vector(const SrcElementType (&src)[srcDimension]);

			vector &operator =(const vector &) & = default;
			using Impl::SwizzleAssign<ElementType, 0, dimension>::operator =;

		private:
			template<typename ElementType, unsigned int rows, unsigned int columns>
			friend struct Impl::Data;

			// workaround for bug in VS 2015 Update 2
			typedef std::make_index_sequence<dimension> IdxSeq;

			template<typename Data::InitType initType, class IdxSeq, unsigned offset, typename ...Args>
#if defined MSVC_LIMITATIONS || defined __clang__
			inline vector(typename Data::template InitTag<initType, IdxSeq, offset> tag, const Args &...args) :
				DataContainer(tag, args...) {}
#else
			vector(typename Data::template InitTag<initType, IdxSeq, offset>, const Args &...args);
#endif

			// hide data in private
		private:
			friend class Impl::SwizzleDataAccess<ElementType, 0, dimension>;
			using DataContainer::data;
		};

		template<typename ElementType_, unsigned int rows_, unsigned int columns_>
		class EBCO matrix : public Impl::DataContainer<ElementType_, rows_, columns_>, public Impl::Tag<Impl::TagName::Matrix, rows_ == 1 && columns_ == 1>
		{
			static_assert(rows_ > 0, "matrix should contain at least 1 row");
			static_assert(columns_ > 0, "matrix should contain at least 1 column");
			static_assert(Impl::IsElementTypeValid<ElementType_>, "invalid matrix element type");
			typedef vector<ElementType_, columns_> Row;
			typedef Impl::DataContainer<ElementType_, rows_, columns_> DataContainer;
			typedef Impl::Data<ElementType_, rows_, columns_> Data;
			typedef std::make_index_sequence<rows_> IdxSeq;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int rows = rows_, columns = columns_;

			matrix() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const matrix<SrcElementType, srcRows, srcColumns> &src);

			template<Impl::Scalar SrcType>
			matrix(const SrcType &scalar);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			requires (SrcSwizzleDesc::dimension > 1)
			matrix(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<typename ...Args>
			requires (sizeof...(Args) > 1)
			matrix(const Args &...args);

#if INIT_LIST_SUPPORT_TIER >= 2
			matrix(std::initializer_list<Impl::InitListItem<ElementType, rows * columns>> initList);
#endif

			//template<typename Iterator>
			//explicit matrix(Iterator src);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const SrcElementType (&src)[srcRows][srcColumns]);

			matrix &operator =(const matrix &) & = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &> operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &;

			template<Impl::Scalar SrcType>
			matrix &operator =(const SrcType &scalar) &;

			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &> operator =(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &;

#if INIT_LIST_SUPPORT_TIER >= 1
			matrix &operator =(std::initializer_list<Impl::InitListItem<ElementType, rows * columns>> initList) &;
#endif

		private:
			template<size_t ...idx, class F>
			inline auto Op(std::index_sequence<idx...>, F f) const;

		public:
			auto operator +() const;
			auto operator -() const;
			auto operator !() const;

		private:
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			void AssignSwizzle(std::index_sequence<rowIdx...>, const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &;

			template<size_t ...rowIdx, typename SrcType>
			void AssignScalar(std::index_sequence<rowIdx...>, const SrcType &scalar) &;

			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			void OpAssignMatrix(
				std::index_sequence<rowIdx...>,
				typename Impl::Swizzle<ElementType, 0, columns>::OperationResult &f(
					Impl::Swizzle<ElementType, 0, columns> &, const Impl::Swizzle<SrcElementType, 0, srcColumns> &),
				const matrix<SrcElementType, srcRows, srcColumns> &src) &;

			template<size_t ...rowIdx, typename SrcType>
			void OpAssignScalar(
				std::index_sequence<rowIdx...>,
				typename Impl::Swizzle<ElementType, 0, columns>::OperationResult &f(Impl::Swizzle<ElementType, 0, columns> &, const SrcType &),
				const SrcType &scalar) &;

		private:
#pragma region generate operators
			// matrix / 1x1 matrix op=<!WARHazard, !extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar, typename SrcType>							\
				std::enable_if_t<!WARHazard && !extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>	\
				operator op##=(const SrcType &scalar);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op=<WARHazard, !extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar, typename SrcType>							\
				std::enable_if_t<WARHazard && !extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>	\
				operator op##=(const SrcType &scalar);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op=<?WARHazard, extractScalar> scalar
#			define OPERATOR_DECLARATION(op)																\
				template<bool WARHazard, bool extractScalar = true, typename SrcType>					\
				std::enable_if_t<extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>					\
				operator op##=(const SrcType &scalar);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op=<?WARHazard> scalar
#			define OPERATOR_DECLARATION(op)																\
				template																				\
				<																						\
					bool WARHazard,																		\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,			\
					Impl::Scalar RightType																\
				>																						\
				friend inline decltype(auto) operator op##=(											\
					matrix<LeftElementType, leftRows, leftColumns> &left,								\
					const RightType &right);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION
#pragma endregion

		public:
			const Row &operator [](unsigned int idx) const & noexcept;
			Row &operator [](unsigned int idx) & noexcept;

			operator const ElementType &() const noexcept;
			operator ElementType &() & noexcept;

		private:
#ifdef MSVC_LIMITATIONS
			template<unsigned c, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			friend inline decltype(auto) Impl::MatrixColumn(const matrix<SrcElementType, srcRows, srcColumns> &src);
#else
			template
			<
				size_t ...columnIdx,
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns
			>
			friend inline auto Impl::mul(std::index_sequence<columnIdx...>,
				const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
				-> vector<decltype(mul(left, right.template Column<0>())), sizeof...(columnIdx)>;
#endif

			template<unsigned c>
			decltype(auto) Column() const noexcept;

		public:
			template<typename F>
			matrix<std::result_of_t<F &(ElementType)>, rows, columns> apply(F f) const;

			template<typename Result>
			matrix<Result, rows, columns> apply(Result __cdecl f(ElementType)) const
			{
				return apply<Result __cdecl(ElementType)>(f);
			}

			template<typename Result>
			matrix<Result, rows, columns> apply(Result __cdecl f(const ElementType &)) const
			{
				return apply<Result __cdecl(const ElementType &)>(f);
			}

			matrix<ElementType, rows, columns> apply(ElementType __cdecl f(ElementType)) const
			{
				return apply<ElementType>(f);
			}

			matrix<ElementType, rows, columns> apply(ElementType __cdecl f(const ElementType &)) const
			{
				return apply<ElementType>(f);
			}

		private:
			template<typename F, size_t ...rowIdx>
			inline matrix<std::result_of_t<F &(ElementType)>, rows, columns> apply(F f, std::index_sequence<rowIdx...>) const;

			// hide data in private and expose it from dependent base to allow for unqualified lookup
		private:
#if defined MSVC_LIMITATIONS || defined __clang__
			template<typename, unsigned int, unsigned int, class, typename>
			friend class Impl::SwizzleAssign;
#else
			// ICE on VS 2015, has no effect on clang
			template<typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc>
			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			friend inline auto Impl::SwizzleAssign<DstElementType, dstRows, dstColumns, DstSwizzleDesc, std::true_type>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) &
				-> std::enable_if_t<(srcRows > 1 || srcColumns > 1), typename Impl::SwizzleAssign<DstElementType, dstRows, dstColumns, DstSwizzleDesc, std::true_type>::OperationResult &>;
#endif

#pragma region generate operators
			// swizzle / 1D swizzle op= matrix
#			define OPERATOR_DECLARATION(op)																									\
				template																													\
				<																															\
					bool ...WARHazard,																										\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns											\
				>																															\
				friend inline std::enable_if_t<(rightRows > 1 || rightColumns > 1),															\
				typename Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::OperationResult &> operator op##=(			\
					Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,											\
					const matrix<RightElementType, rightRows, rightColumns> &right);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op= matrix
#			define OPERATOR_DECLARATION(op)																									\
				template																													\
				<																															\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,												\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns											\
				>																															\
				friend inline decltype(auto) operator op##=(																				\
					matrix<LeftElementType, leftRows, leftColumns> &left,																	\
					const matrix<RightElementType, rightRows, rightColumns> &right)															\
					requires (rightRows > 1 || rightColumns > 1);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// matrix / 1x1 matrix op= swizzle
#			define OPERATOR_DECLARATION(op)																									\
				template																													\
				<																															\
					bool ...WARHazard,																										\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,												\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc					\
				>																															\
				friend inline decltype(auto) operator op##=(																				\
					matrix<LeftElementType, leftRows, leftColumns> &left,																	\
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)								\
					requires (RightSwizzleDesc::dimension > 1);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_OP)
#			undef OPERATOR_DECLARATION

			// swizzle op matrix / 1D swizzle op 1x1 matrix
#			define OPERATOR_DECLARATION(op, F)																								\
				template																													\
				<																															\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns											\
				>																															\
				friend inline std::enable_if_t<(LeftSwizzleDesc::dimension > 1 == (rightRows > 1 || rightColumns > 1)),						\
				Impl::SwizzleOpMatrixResult																									\
				<																															\
					LeftElementType, LeftSwizzleDesc::dimension,																			\
					RightElementType, rightRows, rightColumns,																				\
					std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>, Impl::IsMaskOp<std::F<>>	\
				>> operator op(																												\
					const Impl::Swizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,										\
					const matrix<RightElementType, rightRows, rightColumns> &right);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_PAIR)
			GENERATE_MASK_OPERATORS(OPERATOR_DECLARATION, F_2_PAIR)
#			undef OPERATOR_DECLARATION

			// matrix op swizzle / 1x1 matrix op 1D swizzle
#			define OPERATOR_DECLARATION(op, F)																								\
				template																													\
				<																															\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,												\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc					\
				>																															\
				friend inline std::enable_if_t<((leftRows > 1 || leftColumns > 1) == RightSwizzleDesc::dimension > 1),						\
				Impl::MatrixOpSwizzleResult																									\
				<																															\
					LeftElementType, leftRows, leftColumns,																					\
					RightElementType, RightSwizzleDesc::dimension,																			\
					std::decay_t<decltype(std::declval<LeftElementType>() op std::declval<RightElementType>())>, Impl::IsMaskOp<std::F<>>	\
				>> operator op(																												\
					const matrix<LeftElementType, leftRows, leftColumns> &left,																\
					const Impl::Swizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right);
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DECLARATION, F_2_PAIR)
			GENERATE_MASK_OPERATORS(OPERATOR_DECLARATION, F_2_PAIR)
#			undef OPERATOR_DECLARATION
#pragma endregion

			using DataContainer::data;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename F>
		inline vector<std::result_of_t<F &(ElementType)>, SwizzleDesc::dimension>
		Impl::SwizzleCommon<ElementType, rows, columns, SwizzleDesc>::apply(F f) const
		{
			return apply(f, make_index_sequence<SwizzleDesc::dimension>());
		}

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename F, size_t ...idx>
		inline vector<std::result_of_t<F &(ElementType)>, SwizzleDesc::dimension>
		Impl::SwizzleCommon<ElementType, rows, columns, SwizzleDesc>::apply(F f, index_sequence<idx...>) const
		{
			return{ f((*this)[idx])... };
		}

#if !defined _MSC_VER || defined __clang__
#	define ELEMENTS_COUNT_PREFIX Impl::
#elif 0
#	define ELEMENTS_COUNT_PREFIX Data::
#else
	// C++ standard core language defect?
#	define ELEMENTS_COUNT_PREFIX Data::template
#endif

#		pragma region vector impl
			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline vector<ElementType, dimension>::vector(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(typename Data::template InitTag<Data::InitType::Copy, IdxSeq>(), src)
			{
				static_assert(dimension <= SrcSwizzleDesc::dimension, "\"copy\" ctor: too small src dimension");
			}

			template<typename ElementType, unsigned int dimension>
			template<Impl::Scalar SrcType>
			inline vector<ElementType, dimension>::vector(const SrcType &scalar) :
				DataContainer(typename Data::template InitTag<Data::InitType::Scalar, IdxSeq>(), Impl::ExtractScalar(scalar)) {}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			requires (srcRows > 1 || srcColumns > 1)
			inline vector<ElementType, dimension>::vector(const matrix<SrcElementType, srcRows, srcColumns> &src) :
				DataContainer(typename Data::template InitTag<Data::InitType::Sequencing, IdxSeq>(), src)
			{
				constexpr bool checkOverflow = dimension > 1 && srcRows > 1 && srcColumns > 1;
				static_assert(srcRows * srcColumns >= dimension, "sequencing ctor: too few src elements");
				static_assert(srcRows * srcColumns <= dimension || !checkOverflow, "sequencing ctor: too many src elements");
			}

			template<typename ElementType, unsigned int dimension>
			template<typename ...Args>
			requires (sizeof...(Args) > 1)
			inline vector<ElementType, dimension>::vector(const Args &...args) :
				DataContainer(typename Data::template InitTag<Data::InitType::Sequencing, IdxSeq>(), args...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX ElementsCount<const Args &...>;
				static_assert(srcElements >= dimension, "sequencing ctor: too few src elements");
				static_assert(srcElements <= dimension, "sequencing ctor: too many src elements");
			}

			//template<typename ElementType, unsigned int dimension>
			//template<typename Iterator>
			//inline vector<ElementType, dimension>::vector(Iterator src)
			//{
			//	std::copy_n(src, dimension, DataContainer::data.data);
			//}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcDimension>
			inline vector<ElementType, dimension>::vector(const SrcElementType (&src)[srcDimension]) :
				DataContainer(typename Data::template InitTag<Data::InitType::Array, IdxSeq>(), src)
			{
				static_assert(dimension <= srcDimension, "array ctor: too small src dimension");
			}

#if INIT_LIST_SUPPORT_TIER >= 2
			template<typename ElementType, unsigned int dimension>
			inline vector<ElementType, dimension>::vector(std::initializer_list<Impl::InitListItem<ElementType, dimension>> initList)
			{
				operator =(initList);
			}
#endif

#if !(defined MSVC_LIMITATIONS || defined __clang__)
			template<typename ElementType, unsigned int dimension>
			template<typename vector<ElementType, dimension>::Data::InitType initType, class IdxSeq, unsigned offset, typename ...Args>
			inline vector<ElementType, dimension>::vector(typename Data::template InitTag<initType, IdxSeq, offset> tag, const Args &...args) :
				DataContainer(tag, args...) {}
#endif
#		pragma endregion

#		pragma region matrix impl
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const & noexcept -> const typename matrix::Row &
			{
				assert(idx < rows);
				return DataContainer::data.rowsData[idx];
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) & noexcept -> typename matrix::Row &
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
			inline matrix<ElementType, rows, columns>::operator ElementType &() & noexcept
			{
				return operator [](0);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<unsigned c>
			inline decltype(auto) matrix<ElementType, rows, columns>::Column() const noexcept
			{
				return reinterpret_cast<const Impl::ColumnSwizzle<ElementType, rows, columns, c> &>(data);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const matrix<SrcElementType, srcRows, srcColumns> &src) :
				DataContainer(typename Data::template InitTag<Data::InitType::Copy, IdxSeq>(), src)
			{
				static_assert(rows <= srcRows, "\"copy\" ctor: too few rows in src");
				static_assert(columns <= srcColumns, "\"copy\" ctor: too few columns in src");
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<Impl::Scalar SrcType>
			inline matrix<ElementType, rows, columns>::matrix(const SrcType &scalar) :
				DataContainer(typename Data::template InitTag<Data::InitType::Scalar, IdxSeq>(), scalar) {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			requires (SrcSwizzleDesc::dimension > 1)
			inline matrix<ElementType, rows, columns>::matrix(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(typename Data::template InitTag<Data::InitType::Sequencing, IdxSeq>(), src)
			{
				constexpr bool checkOverflow = rows > 1 && columns > 1;
				constexpr auto srcElements = SrcSwizzleDesc::dimension;
				static_assert(srcElements >= rows * columns, "sequencing ctor: too few src elements");
				static_assert(srcElements <= rows * columns || !checkOverflow, "sequencing ctor: too many src elements");
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename ...Args>
			requires (sizeof...(Args) > 1)
			inline matrix<ElementType, rows, columns>::matrix(const Args &...args) :
				DataContainer(typename Data::template InitTag<Data::InitType::Sequencing, IdxSeq>(), args...)
			{
				constexpr auto srcElements = ELEMENTS_COUNT_PREFIX ElementsCount<const Args &...>;
				static_assert(srcElements >= rows * columns, "sequencing ctor: too few src elements");
				static_assert(srcElements <= rows * columns, "sequencing ctor: too many src elements");
			}

			//template<typename ElementType, unsigned int rows, unsigned int columns>
			//template<typename Iterator>
			//inline matrix<ElementType, rows, columns>::matrix(Iterator src)
			//{
			//	for (unsigned r = 0; r < rows; r++, src += columns)
			//		operator [](r) = src;
			//}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const SrcElementType (&src)[srcRows][srcColumns]) :
				DataContainer(typename Data::template InitTag<Data::InitType::Array, IdxSeq>(), src)
			{
				static_assert(rows <= srcRows, "array ctor: too few rows in src");
				static_assert(columns <= srcColumns, "array ctor: too few columns in src");
			}

#if INIT_LIST_SUPPORT_TIER >= 2
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline matrix<ElementType, rows, columns>::matrix(std::initializer_list<Impl::InitListItem<ElementType, rows * columns>> initList)
			{
				operator =(initList);
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline auto matrix<ElementType, rows, columns>::operator =(const matrix<SrcElementType, srcRows, srcColumns> &src) & -> std::enable_if_t<(srcRows > 1 || srcColumns > 1), matrix &>
			{
				static_assert(rows <= srcRows, "'matrix = matrix': too few rows in src");
				static_assert(columns <= srcColumns, "'matrix = matrix': too few columns in src");
				AssignSwizzle(std::make_index_sequence<rows>(), src);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<Impl::Scalar SrcType>
			inline auto matrix<ElementType, rows, columns>::operator =(const SrcType &scalar) & -> matrix &
			{
				AssignScalar(std::make_index_sequence<rows>(), scalar);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<bool ...WARHazard, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
				-> std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &>
			{
				static_assert(sizeof...(WARHazard) <= 1);
				constexpr static const auto dstDimension = rows * columns;
				constexpr static const bool underflow = dstDimension > SrcSwizzleDesc::dimension, overflow = dstDimension < SrcSwizzleDesc::dimension;
				static_assert(!(underflow || overflow && dstDimension > 1), "'matrix = vector': unmatched sequencing");
				auto &seq = reinterpret_cast<Impl::SequencingSwizzle<ElementType, rows, columns> &>(data);
				seq.template operator =<WARHazard...>(src);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline auto matrix<ElementType, rows, columns>::operator =(const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &&src) &
				-> std::enable_if_t<(SrcSwizzleDesc::dimension > 1), matrix &>
			{
				return operator =<false>(src);
			}

#if INIT_LIST_SUPPORT_TIER >= 1
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<Impl::InitListItem<ElementType, rows * columns>> initList) & -> matrix &
			{
				unsigned dstIdx = 0;
				for (const auto &item : initList)
					for (unsigned itemEementIdx = 0; itemEementIdx < item.GetItemSize(); itemEementIdx++, dstIdx++)
						(*this)[dstIdx / columns][dstIdx % columns] = item[itemEementIdx];
				assert(dstIdx == rows * columns);
				return *this;
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...idx, class F>
			inline auto matrix<ElementType, rows, columns>::Op(std::index_sequence<idx...>, F f) const
			{
				return matrix<std::decay_t<decltype(f(std::declval<ElementType>()))>, rows, columns>(f(operator [](idx))...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator +() const
			{
				return Op(IdxSeq(), Impl::positive());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator -() const
			{
				return Op(IdxSeq(), std::negate());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator !() const
			{
				return Op(IdxSeq(), std::logical_not());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline void matrix<ElementType, rows, columns>::AssignSwizzle(std::index_sequence<rowIdx...>, const Impl::Swizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) &
			{
				((operator [](rowIdx) = src[rowIdx]), ...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcType>
			inline void matrix<ElementType, rows, columns>::AssignScalar(std::index_sequence<rowIdx...>, const SrcType &scalar) &
			{
				((operator [](rowIdx) = scalar), ...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline void matrix<ElementType, rows, columns>::OpAssignMatrix(
				std::index_sequence<rowIdx...>,
				typename Impl::Swizzle<ElementType, 0, columns>::OperationResult &f(
					Impl::Swizzle<ElementType, 0, columns> &, const Impl::Swizzle<SrcElementType, 0, srcColumns> &),
				const matrix<SrcElementType, srcRows, srcColumns> &src) &
			{
				(f(operator [](rowIdx), src[rowIdx]), ...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...rowIdx, typename SrcType>
			inline void matrix<ElementType, rows, columns>::OpAssignScalar(
				std::index_sequence<rowIdx...>,
				typename Impl::Swizzle<ElementType, 0, columns>::OperationResult &f(Impl::Swizzle<ElementType, 0, columns> &, const SrcType &),
				const SrcType &scalar) &
			{
				assert(!TriggerScalarWARHazard(*this, &scalar));
				(f(operator [](rowIdx), scalar), ...);
			}

			// matrix / 1x1 matrix op=<!WARHazard, !extractScalar> scalar
#ifdef MSVC_LIMITATIONS
#			define OPERATOR_DEFINITION(op)																							\
				template<typename ElementType, unsigned int rows, unsigned int columns>												\
				template<bool WARHazard, bool extractScalar, typename SrcType>														\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)								\
				-> std::enable_if_t<!WARHazard && !extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>							\
				{																													\
					OpAssignScalar(std::make_index_sequence<rows>(), Impl::ScalarOps::operator op##=								\
						<false, false, ElementType, 0, columns, Impl::VectorSwizzleDesc<columns>, SrcType>, scalar);				\
					return *this;																									\
				}
#else
#			define OPERATOR_DEFINITION(op)																							\
				template<typename ElementType, unsigned int rows, unsigned int columns>												\
				template<bool WARHazard, bool extractScalar, typename SrcType>														\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)								\
				-> std::enable_if_t<!WARHazard && !extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>							\
				{																													\
					using namespace Impl;																							\
					OpAssignScalar(make_index_sequence<rows>(), ScalarOps::operator op##=<false, false>, scalar);					\
					return *this;																									\
				}
#endif
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#			undef OPERATOR_DEFINITION

			// matrix / 1x1 matrix op=<WARHazard, !extractScalar> scalar
#			define OPERATOR_DEFINITION(op)																							\
				template<typename ElementType, unsigned int rows, unsigned int columns>												\
				template<bool WARHazard, bool extractScalar, typename SrcType>														\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)								\
				-> std::enable_if_t<WARHazard && !extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>								\
				{																													\
					return operator op##=<false, false>(SrcType(scalar));															\
				}
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#			undef OPERATOR_DEFINITION

			// matrix / 1x1 matrix op=<?WARHazard, extractScalar> scalar
#			define OPERATOR_DEFINITION(op)																							\
				template<typename ElementType, unsigned int rows, unsigned int columns>												\
				template<bool WARHazard, bool extractScalar, typename SrcType>														\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const SrcType &scalar)								\
				-> std::enable_if_t<extractScalar/* && Impl::Scalar<SrcType>*/, matrix &>											\
				{																													\
					return operator op##=<WARHazard, false>(Impl::ExtractScalar(scalar));											\
				}
			GENERATE_ARITHMETIC_OPERATORS(OPERATOR_DEFINITION, F_2_OP)
#			undef OPERATOR_DEFINITION

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename F>
			inline auto matrix<ElementType, rows, columns>::apply(F f) const -> matrix<std::result_of_t<F &(ElementType)>, rows, columns>
			{
				return apply(f, std::make_index_sequence<rows>());
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename F, size_t ...rowIdx>
			inline auto matrix<ElementType, rows, columns>::apply(F f, std::index_sequence<rowIdx...>) const -> matrix<std::result_of_t<F &(ElementType)>, rows, columns>
			{
				return{ (*this)[rowIdx].apply(f)... };
			}
#		pragma endregion

#	undef ELEMENTS_COUNT_PREFIX

#		undef OP_plus
#		undef OP_minus
#		undef OP_multiplies
#		undef OP_divides
#		undef OP_equal_to
#		undef OP_not_equal_to
#		undef OP_greater
#		undef OP_less
#		undef OP_greater_equal
#		undef OP_less_equal
#		undef OP_logical_and
#		undef OP_logical_or
#		undef F_2_OP
#		undef F_2_PAIR

#ifdef MSVC_PREPROCESSOR_WORKAROUND
#		undef GENERATE_OPERATOR_IMPL
#endif
#		undef GENERATE_OPERATOR

#if USE_BOOST_PREPROCESSOR
#		undef ARITHMETIC_OPS
#		undef MASK_OPS
#		undef OPERATOR_GENERATOR
#		undef GENERATE_OPERATORS
#endif
#		undef GENERATE_ARITHMETIC_OPERATORS
#		undef GENERATE_MASK_OPERATORS
	}

	// std specializations\
	NOTE: cv/ref specializations not provided, std::decay_t should be applied first for such types
	namespace std
	{
		// swizzle -> vector
		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		struct decay<Math::VectorMath::Impl::Swizzle<ElementType, rows, columns, SwizzleDesc>>
		{
			typedef Math::VectorMath::vector<ElementType, SwizzleDesc::dimension> type;
		};

		// vector/vector
		template<typename LeftElementType, unsigned int leftDimension, typename RightElementType, unsigned int rightDimension>
		struct common_type<Math::VectorMath::vector<LeftElementType, leftDimension>, Math::VectorMath::vector<RightElementType, rightDimension>>
		{
			typedef Math::VectorMath::vector<common_type_t<LeftElementType, RightElementType>, min(leftDimension, rightDimension)> type;
		};

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
		template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
		struct common_type<Math::VectorMath::matrix<LeftElementType, leftRows, leftColumns>, Math::VectorMath::matrix<RightElementType, rightRows, rightColumns>>
		{
			typedef Math::VectorMath::matrix<common_type_t<LeftElementType, RightElementType>, min(leftRows, rightRows), min(leftColumns, rightColumns)> type;
		};

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
#	undef MSVC_PREPROCESSOR_WORKAROUND
#	undef EBCO
#	undef INIT_LIST_ITEM_OVERFLOW_MSG

#if USE_BOOST_PREPROCESSOR
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED
#endif

#	pragma warning(pop)

#endif//__VECTOR_MATH_H__
#endif