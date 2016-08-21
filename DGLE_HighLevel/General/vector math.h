/**
\author		Alexey Shaydurov aka ASH
\date		21.08.2016 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma region limitations due to lack of C++11 support
/*
VS 2013 does not catch errors like vec.xx = vec.xx and does nothing for things like vec.x = vec.x

different versions of CDataContainer now inherited from specialized CSwizzle
sizeof(vector<float, 3>) in this case is 12 for both gcc and VS2010
if vector inherited from specialized CSwizzle instead of CDataContainer then sizeof(...) is 12 for gcc and 16 for VS2010
TODO: try inherit vector from CSwizzle for future versions of VS
*/
#pragma endregion

#pragma region design considerations
/*
it is safe to use const_cast if const version returns (const &), not value, and *this object is not const

functions like distance now receives CSwizzle arguments
function template with same name from other namespace can have higher priority when passing vector arguments
(because no conversion required in contrast to vector->CSwizzle conversion required for function in VectorMath)
consider overloads with vector arguments to eliminate this issue

apply now have overloads similar to valarray's apply (in order to handle functions having template overloads) and some other overloads in addition
but it still does not perform as desired (vector<int, ...>.apply(floor) for example)
consider using preprocessor instead of templates or overloading each target function (floor(const vector<T, ...> &)

'&& = ?' now forbidden
'&& op= ?' also forbidden but not with MSVC (it does not follows C++ standard in that regard)
*/
#pragma endregion

#if BOOST_PP_IS_ITERATING
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
#	define SWIZZLE_SEQ_2_VECTOR(seq) mpl::vector_c<unsigned int, BOOST_PP_TUPLE_REM_CTOR(BOOST_PP_SEQ_SIZE(seq), BOOST_PP_SEQ_TO_TUPLE(seq))>

#	define BOOST_PP_ITERATION_LIMITS (1, 4)
#	define BOOST_PP_FILENAME_2 "vector math.h"
#	include BOOST_PP_ITERATE()
#else
	/*
	gcc does not allow explicit specialization in class scope => CSwizzle can not be inside CDataContainer
	ElementType needed in order to compiler can deduce template args for operators
	*/
#if !defined DISABLE_MATRIX_SWIZZLES || ROWS == 0
#	define NAMING_SET_1 BOOST_PP_IF(ROWS, MATRIX_ZERO_BASED, XYZW)
#	define NAMING_SET_2 BOOST_PP_IF(ROWS, MATRIX_ONE_BASED, RGBA)

#	define SWIZZLE_OBJECT(swizzle_seq)															\
		CSwizzle<ElementType, ROWS, COLUMNS, CSwizzleDesc<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>>	\
			TRANSFORM_SWIZZLE(NAMING_SET_1, swizzle_seq),										\
			TRANSFORM_SWIZZLE(NAMING_SET_2, swizzle_seq);

#if defined _MSC_VER && _MSC_VER <= 1900
#	define TRIVIAL_CTOR_FORWARD CDataContainerImpl() = default; template<typename TCurSrc, typename ...TRestSrc> CDataContainerImpl(const TCurSrc &curSrc, const TRestSrc &...restSrc): data(curSrc, restSrc...) {}
#else
#	define TRIVIAL_CTOR_FORWARD CDataContainerImpl() = default; NONTRIVIAL_CTOR_FORWARD
#endif
#	define NONTRIVIAL_CTOR_FORWARD template<typename ...TSrc> CDataContainerImpl(const TSrc &...src) : data(src...) {}
#	define DATA_CONTAINER_IMPL_SPECIALIZATION(trivialCtor)																										\
		template<typename ElementType>																															\
		class CDataContainerImpl<ElementType, ROWS, COLUMNS, trivialCtor> : public std::conditional_t<ROWS == 0, CSwizzle<ElementType, 0, COLUMNS>, MatrixTag>	\
		{																																						\
		protected:																																				\
			/*forward ctors/dtor/= to data*/																													\
			BOOST_PP_REMOVE_PARENS(BOOST_PP_IIF(trivialCtor, (TRIVIAL_CTOR_FORWARD), (NONTRIVIAL_CTOR_FORWARD)))												\
																																								\
			CDataContainerImpl(const CDataContainerImpl &src) : data(src.data)																					\
			{																																					\
			}																																					\
																																								\
			CDataContainerImpl(CDataContainerImpl &&src) : data(std::move(src.data))																			\
			{																																					\
			}																																					\
																																								\
			~CDataContainerImpl()																																\
			{																																					\
				data.~CData<ElementType, ROWS, COLUMNS>();																										\
			}																																					\
																																								\
			void operator =(const CDataContainerImpl &right)																									\
			{																																					\
				data = right.data;																																\
			}																																					\
																																								\
			void operator =(CDataContainerImpl &&right)																											\
			{																																					\
				data = std::move(right.data);																													\
			}																																					\
																																								\
		public:																																					\
			union																																				\
			{																																					\
				CData<ElementType, ROWS, COLUMNS> data;																											\
				/*gcc does not allow class definition inside anonymous union*/																					\
				GENERATE_SWIZZLES((SWIZZLE_OBJECT))																												\
			};																																					\
		};
	DATA_CONTAINER_IMPL_SPECIALIZATION(0)
	DATA_CONTAINER_IMPL_SPECIALIZATION(1)
#	undef TRIVIAL_CTOR_FORWARD
#	undef NONTRIVIAL_CTOR_FORWARD
#	undef DATA_CONTAINER_IMPL_SPECIALIZATION

	// specialization for graphics vectors/matrices
	template<typename ElementType>
	class CDataContainer<ElementType, ROWS, COLUMNS> : public CDataContainerImpl<ElementType, ROWS, COLUMNS>
	{
	protected:
		using CDataContainerImpl<ElementType, ROWS, COLUMNS>::CDataContainerImpl;
		CDataContainer() = default;
		CDataContainer(const CDataContainer &) = default;
		CDataContainer(CDataContainer &&) = default;
		~CDataContainer() = default;
		CDataContainer &operator =(const CDataContainer &) = default;
		CDataContainer &operator =(CDataContainer &&) = default;
	};

#	undef NAMING_SET_1
#	undef NAMING_SET_2
#	undef SWIZZLE_OBJECT
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
#	undef SWIZZLE_SEQ_2_VECTOR
#endif
#else
#	ifndef __VECTOR_MATH_H__
#	define __VECTOR_MATH_H__

#	if !defined  __clang__  && defined _MSC_FULL_VER && _MSC_FULL_VER < 190024210
#	error Old MSVC compiler version. Visual Studio 2015 Update 3 or later required.
#	elif defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ >= 4 && __GNUC_MINOR__ < 7))
#	error Old GCC compiler version. GCC 4.7 or later required.	// need to be clarified
#	endif

#	pragma warning(push)
#	pragma warning(disable: 4003)

#	include <cassert>
#	include <cstdint>
#	include <utility>
#	include <type_traits>
#	include <functional>
#	include <iterator>
#	include <algorithm>
#	include <initializer_list>
#	include <memory>	// temp for unique_ptr
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
#	include <boost/mpl/placeholders.hpp>
#	include <boost/mpl/vector_c.hpp>
#	include <boost/mpl/range_c.hpp>
#	include <boost/mpl/at.hpp>
#	include <boost/mpl/min_max.hpp>
#	include <boost/mpl/sort.hpp>
#	include <boost/mpl/size.hpp>
#	include <boost/mpl/unique.hpp>
#	include <boost/mpl/comparison.hpp>
#	include <boost/mpl/or.hpp>
#	include <boost/mpl/bitor.hpp>
#	include <boost/mpl/shift_left.hpp>
#	include <boost/mpl/times.hpp>
#	include <boost/mpl/integral_c.hpp>
#	include <boost/mpl/iterator_range.hpp>
#	include <boost/mpl/deref.hpp>
#	include <boost/mpl/begin.hpp>
#	include <boost/mpl/advance.hpp>
#	include <boost/mpl/distance.hpp>
#	include <boost/mpl/iter_fold.hpp>
#	include <boost/mpl/find.hpp>

//#	define GET_SWIZZLE_ELEMENT(vectorDimension, idx, cv) (reinterpret_cast<cv CData<ElementType, vectorDimension> &>(*this).data[(idx)])
//#	define GET_SWIZZLE_ELEMENT_PACKED(vectorDimension, packedSwizzle, idx, cv) (GET_SWIZZLE_ELEMENT(vectorDimension, packedSwizzle >> ((idx) << 1) & 3u, cv))

	namespace Math::VectorMath
	{
#		define ARITHMETIC_OPS (+)(-)(*)(/)
#		define REL_OPS (==)(!=)(<)(<=)(>)(>=)
#		define GENERATE_OPERATORS_MACRO(r, callback, op) callback(op)
#		define GENERATE_OPERATORS(callback, ops) BOOST_PP_SEQ_FOR_EACH(GENERATE_OPERATORS_MACRO, callback, ops)

		namespace mpl = boost::mpl;

		enum class TagName
		{
			Swizzle,
			Matrix,
		};

		template<TagName name>
		class Tag
		{
		protected:
			Tag() = default;
			Tag(const Tag &) = default;
			Tag &operator =(const Tag &) = default;
			~Tag() = default;
		};

		typedef Tag<TagName::Swizzle> SwizzleTag;
		typedef Tag<TagName::Matrix> MatrixTag;

		template<typename Src, typename Tag>
		class CheckTag
		{
			template<template<typename, typename> class F, typename Var, typename Fixed>
			using Iterate = std::bool_constant<F<Var &, Fixed>::value || F<const Var &, Fixed>::value || F<Var &&, Fixed>::value || F<const Var &&, Fixed>::value>;

			template<typename FixedTag, typename VarSrc>
			using IterateSrc = Iterate<std::is_convertible, VarSrc, FixedTag>;

		public:
			static constexpr bool value = Iterate<IterateSrc, Tag, Src>::value;
		};

		template<typename Src>
		static constexpr bool IsSwizzle = CheckTag<Src, SwizzleTag>::value;

		template<typename Src>
		static constexpr bool IsMatrix = CheckTag<Src, MatrixTag>::value;

		template<typename Src>
		static constexpr bool IsScalar = !(IsSwizzle<Src> || IsMatrix<Src>);

		template<class CSwizzleVector_>
		class CSwizzleDesc
		{
			template<class Iter>
			struct PackSwizzleElement : mpl::shift_left<typename mpl::deref<Iter>::type, typename mpl::times<typename Iter::pos, mpl::integral_c<unsigned short, 4u>::type>> {};
			typedef mpl::iter_fold<CSwizzleVector_, mpl::integral_c<unsigned short, 0u>, mpl::bitor_<mpl::_1, PackSwizzleElement<mpl::_2>>> PackedSwizzle;

		private:
			typedef typename mpl::sort<CSwizzleVector_>::type CSortedSwizzleVector;
			typedef typename mpl::unique<CSortedSwizzleVector, std::is_same<mpl::_, mpl::_>>::type CUniqueSwizzleVector;
			typedef typename mpl::equal_to<mpl::size<CUniqueSwizzleVector>, mpl::size<CSwizzleVector_>> IsWriteMaskValid;

		public:
			// unique for CSwizzleDesc (not present in CVectorSwizzleDesc)
			static constexpr unsigned short packedSwizzle = PackedSwizzle::type::value;

		public:
			typedef CSwizzleVector_ CSwizzleVector;

		public:
			typedef typename mpl::size<CSwizzleVector_>::type TDimension;
			static constexpr typename IsWriteMaskValid::value_type isWriteMaskValid = IsWriteMaskValid::value;
		};

		template<unsigned int vectorDimension>
		class CVectorSwizzleDesc
		{
		public:
			// CSwizzleVector here not actually vector but rather range
			typedef mpl::range_c<unsigned int, 0, vectorDimension> CSwizzleVector;

		public:
			typedef typename mpl::integral_c<unsigned, vectorDimension> TDimension;
			static constexpr bool isWriteMaskValid = true;
		};

		template
		<
			typename DstElementType, unsigned int dstRows, unsigned int dstColumns, class DstSwizzleDesc,
			typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc,
			bool assign
		>
		struct DetectSwizzleWARHazard : std::false_type {};

		template
		<
			typename ElementType, unsigned int rows, unsigned int columns,
			class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
		>
		class DetectSwizzleWARHazard<ElementType, rows, columns, DstSwizzleDesc, ElementType, rows, columns, SrcSwizzleDesc, assign>
		{
			typedef typename DstSwizzleDesc::CSwizzleVector CDstSwizzleVector;
			typedef typename SrcSwizzleDesc::CSwizzleVector CSrcSwizzleVector;
			// cut off CSrcSwizzleVector
			typedef typename mpl::min<typename mpl::size<CSrcSwizzleVector>::type, typename mpl::size<CDstSwizzleVector>::type>::type MinSwizzleSize;
			typedef typename mpl::begin<CSrcSwizzleVector>::type SrcSwizzleBegin;
			typedef typename mpl::iterator_range<SrcSwizzleBegin, typename mpl::advance<SrcSwizzleBegin, MinSwizzleSize>::type>::type CCuttedSrcSwizzleVector;

		private:
			template<class SrcIter>
			class Pred
			{
				template<class Seq, class Iter>
				struct DistanceFromBegin : mpl::distance<typename mpl::begin<Seq>::type, Iter> {};
				typedef typename mpl::find<CDstSwizzleVector, typename mpl::deref<SrcIter>::type>::type DstIter;
				typedef typename mpl::less<typename DistanceFromBegin<CDstSwizzleVector, DstIter>::type, typename DistanceFromBegin<CCuttedSrcSwizzleVector, SrcIter>::type>::type DstWasAlreadyWritten;

			private:
				// use metafunctions to perform lazy evaluation
				template<class DstIter = DstIter>
				struct FindSrcWrittenToDstIter : mpl::advance<typename mpl::begin<CCuttedSrcSwizzleVector>::type, typename DistanceFromBegin<CDstSwizzleVector, DstIter>::type> {};
				template<class DstIter = DstIter>
				struct DstWasModified : mpl::not_equal_to<typename mpl::deref<DstIter>::type, typename mpl::deref<typename FindSrcWrittenToDstIter<>::type>::type> {};

			public:
				typedef std::conditional_t<assign && DstWasAlreadyWritten::value, DstWasModified<>, DstWasAlreadyWritten> type;
			};
			typedef typename mpl::iter_fold<CCuttedSrcSwizzleVector, std::false_type, mpl::or_<mpl::_1, Pred<mpl::_2>>>::type Result;

		public:
			static constexpr typename Result::value_type value = Result::value;
		};

		template<typename ElementType, unsigned int dimension>
		class vector;

		template<typename ElementType, unsigned int rows, unsigned int columns>
		class matrix;

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>>
		class CSwizzleCommon;

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>>
		class CSwizzleAssign;

		// rows = 0 for vectors
		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, typename isWriteMaskValid = std::bool_constant<SwizzleDesc::isWriteMaskValid>>
		class CSwizzle;

		template<typename ElementType, unsigned int rows, unsigned int columns, bool trivialCtor = std::is_trivially_default_constructible_v<ElementType>>
		class CDataContainerImpl;

		template<typename ElementType, unsigned int rows, unsigned int columns>
		class CDataContainer;

		template<typename ElementType>
		class CInitListItem;

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool all(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool any(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		bool none(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool all(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool any(const matrix<ElementType, rows, columns> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns>
		bool none(const matrix<ElementType, rows, columns> &src);

		class CDataCommon
		{
		protected:
			template<class IdxSeq, bool checkLength = true, unsigned offset = 0>
			class HeterogeneousInitTag {};

		private:
			// empty
			static std::integral_constant<unsigned int, 0u> ElementsCountHelper();

			// scalar
			template<typename Type>
			static std::enable_if_t<IsScalar<Type>, std::integral_constant<unsigned int, 1u>> ElementsCountHelper(const Type &);

			// swizzle
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			static typename SwizzleDesc::TDimension ElementsCountHelper(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &);

			// matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			static std::integral_constant<unsigned int, rows * columns> ElementsCountHelper(const matrix<ElementType, rows, columns> &);

			template<typename First, typename Second, typename ...Rest>
			static auto ElementsCountHelper(const First &first, const Second &second, const Rest &...rest)	// enaable_if_t - workaround for VS 2015
				-> std::integral_constant<unsigned int, std::enable_if_t<true, decltype(ElementsCountHelper(first))>::value + std::enable_if_t<true, decltype(ElementsCountHelper(second, rest...))>::value>;

		protected:
			template<typename ...Args>
			static constexpr unsigned int elementsCount = decltype(ElementsCountHelper(std::declval<Args>()...))::value;

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
			using DispatchTag = std::integral_constant<Dispatch, dispatch>;

			// next
			template<unsigned idx, typename First, typename ...Rest>
			static inline decltype(auto) GetElementImpl(DispatchTag<Dispatch::Skip>, const First &, const Rest &...rest) noexcept
			{
				return GetElement<idx - elementsCount<const First &>>(rest...);
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
				constexpr auto count = elementsCount<decltype(first)> + elementsCount<decltype(rest)...>;
				constexpr auto clampedIdx = idx < count ? idx : count - 1u;
				return GetElementImpl<clampedIdx>(DispatchTag<clampedIdx < elementsCount<decltype(first)> ? IsScalar<First> ? Dispatch::Scalar : Dispatch::Other : Dispatch::Skip>(), first, rest...);
			}
#else
		private:
			// scalar
			template<unsigned idx, typename SrcType>
			static inline auto GetElementImpl(const SrcType &scalar) noexcept -> std::enable_if_t<IsScalar<SrcType>, decltype(scalar)>
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
				-> std::enable_if_t<idx < elementsCount<const First &>, decltype(GetElementImpl<idx>(first))>
			{
				return GetElementImpl<idx>(first);
			}

			// next
			template<unsigned idx, typename First, typename ...Rest>
			static inline auto GetElementFind(const First &, const Rest &...rest) noexcept
				-> std::enable_if_t<idx >= elementsCount<const First &>, decltype(GetElementFind<idx - elementsCount<const First &>>(rest...))>
			{
				return GetElementFind<idx - elementsCount<const First &>>(rest...);
			}

		protected:
			// clamp
			template<unsigned idx, typename ...Args>
			static inline decltype(auto) GetElement(const Args &...args) noexcept
			{
				constexpr auto count = elementsCount<const Args &...>;
				return GetElementFind<idx < count ? idx : count - 1u>(args...);
			}
#endif
		};

		// generic for matrix
		template<typename ElementType, unsigned int rows, unsigned int columns>
		class CData final : private CDataCommon
		{
			friend class matrix<ElementType, rows, columns>;
			friend bool all<>(const matrix<ElementType, rows, columns> &);
			friend bool any<>(const matrix<ElementType, rows, columns> &);
			friend bool none<>(const matrix<ElementType, rows, columns> &);

			template<typename, unsigned int, unsigned int, class>
			friend class CSwizzleCommon;

			friend class CDataContainer<ElementType, rows, columns>;
			friend class CDataContainerImpl<ElementType, rows, columns, false>;
			friend class CDataContainerImpl<ElementType, rows, columns, true>;

		private:
			CData() = default;
			CData(const CData &) = default;
			CData(CData &&) = default;
			~CData() = default;
			CData &operator =(const CData &) = default;
			CData &operator =(CData &&) = default;

		private:	// matrix specific ctors
			template<size_t ...row, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			CData(std::index_sequence<row...>, const matrix<SrcElementType, srcRows, srcColumns> &src);

			template<size_t ...row, typename SrcElementType>
			CData(std::index_sequence<row...>, const SrcElementType &scalar);

			template<size_t ...row, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			CData(std::index_sequence<row...>, const SrcElementType (&src)[srcRows][srcColumns]);

			template<size_t ...row, typename First, typename ...Rest>
			CData(HeterogeneousInitTag<std::index_sequence<row...>>, const First &first, const Rest &...rest);

		private:
			vector<ElementType, columns> _rows[rows];
		};

		template<typename ElementType, unsigned int rows, unsigned int columns>
		template<size_t ...row, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
		inline CData<ElementType, rows, columns>::CData(std::index_sequence<row...>, const matrix<SrcElementType, srcRows, srcColumns> &src) :
			_rows{ vector<ElementType, columns>(HeterogeneousInitTag<std::make_index_sequence<columns>, false>(), static_cast<const CSwizzle<SrcElementType, 0, srcColumns> &>(src[row]))... }
		{
			static_assert(rows <= srcRows, "\"copy\" ctor: too few rows in src");
			static_assert(columns <= srcColumns, "\"copy\" ctor: too few columns in src");
		}

		template<typename ElementType, unsigned int rows, unsigned int columns>
		template<size_t ...row, typename SrcElementType>
		inline CData<ElementType, rows, columns>::CData(std::index_sequence<row...>, const SrcElementType &scalar) :
			_rows{ (row, static_cast<const ElementType &>(scalar))... } {}

		template<typename ElementType, unsigned int rows, unsigned int columns>
		template<size_t ...row, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
		inline CData<ElementType, rows, columns>::CData(std::index_sequence<row...>, const SrcElementType (&src)[srcRows][srcColumns]) :
			_rows{ vector<ElementType, columns>(HeterogeneousInitTag<std::make_index_sequence<columns>, false>(), static_cast<const CSwizzle<SrcElementType, 0, srcColumns> &>(src[row]))... }
		{
			static_assert(rows <= srcRows, "array ctor: too few rows in src");
			static_assert(columns <= srcColumns, "array ctor: too few columns in src");
		}

		template<typename ElementType, unsigned int rows, unsigned int columns>
		template<size_t ...row, typename First, typename ...Rest>
		inline CData<ElementType, rows, columns>::CData(HeterogeneousInitTag<std::index_sequence<row...>>, const First &first, const Rest &...rest) :
			_rows{ vector<ElementType, columns>(HeterogeneousInitTag<std::make_index_sequence<columns>, false, row * columns>(), first, rest...)... }
		{
			constexpr auto srcElements = elementsCount<const First &, const Rest &...>;
			static_assert(srcElements >= rows * columns, "heterogeneous ctor: too few src elements");
			static_assert(srcElements <= rows * columns, "heterogeneous ctor: too many src elements");
		}

		// specialization for vector
		template<typename ElementType, unsigned int dimension>
		class CData<ElementType, 0, dimension> final : private CDataCommon
		{
			friend class vector<ElementType, dimension>;

			template<typename, unsigned int, unsigned int, class>
			friend class CSwizzleCommon;

			friend class CDataContainer<ElementType, 0, dimension>;
			friend class CDataContainerImpl<ElementType, 0, dimension, false>;
			friend class CDataContainerImpl<ElementType, 0, dimension, true>;

		private:
			CData() = default;
			CData(const CData &) = default;
			CData(CData &&) = default;
			~CData() = default;
			CData &operator =(const CData &) = default;
			CData &operator =(CData &&) = default;

		private:	// vector specific ctors
			template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<size_t ...idx, typename SrcElementType>
			CData(std::index_sequence<idx...>, const SrcElementType &scalar);

			template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>, const SrcElementType (&src)[srcDimension]);

			template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
			inline CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const SrcElementType (&src)[srcDimension]);

			template<size_t ...idx, unsigned offset, typename First, typename ...Rest>
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false, offset>, const First &first, const Rest &...rest);

			template<size_t ...idx, typename First, typename ...Rest>
			inline CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const First &first, const Rest &...rest);

		private:
			ElementType data[dimension];
		};

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
			data{ static_cast<const ElementType &>(src[idx])... } {}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>(), src)
		{
			static_assert(dimension <= SrcSwizzleDesc::TDimension::value, "\"copy\" ctor: too small src dimension");
		}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename SrcElementType>
		inline CData<ElementType, 0, dimension>::CData(std::index_sequence<idx...>, const SrcElementType &scalar) :
			data{ (idx, static_cast<const ElementType &>(scalar))... } {}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>, const SrcElementType (&src)[srcDimension]) :
			data{ static_cast<const ElementType &>(src[idx])... } {}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename SrcElementType, unsigned int srcDimension>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const SrcElementType (&src)[srcDimension]) :
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>(), src)
		{
			static_assert(dimension <= srcDimension, "array ctor: too small src dimension");
		}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, unsigned offset, typename First, typename ...Rest>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, false, offset>, const First &first, const Rest &...rest) :
			data{ static_cast<const ElementType &>(GetElement<idx + offset>(first, rest...))... } {}

		template<typename ElementType, unsigned int dimension>
		template<size_t ...idx, typename First, typename ...Rest>
		inline CData<ElementType, 0, dimension>::CData(HeterogeneousInitTag<std::index_sequence<idx...>, true>, const First &first, const Rest &...rest) :
			CData(HeterogeneousInitTag<std::index_sequence<idx...>, false>(), first, rest...)
		{
			constexpr auto srcElements = elementsCount<const First &, const Rest &...>;
			static_assert(srcElements >= dimension, "heterogeneous ctor: too few src elements");
			static_assert(srcElements <= dimension, "heterogeneous ctor: too many src elements");
		}

		// generic vector/matrix
		template<typename ElementType, unsigned int rows, unsigned int columns>
		class CDataContainer : public std::conditional_t<rows == 0, CSwizzle<ElementType, 0, columns>, MatrixTag>
		{
		protected:
			CData<ElementType, rows, columns> data;
			CDataContainer() = default;
			CDataContainer(const CDataContainer &) = default;
			CDataContainer(CDataContainer &&) = default;
#if defined _MSC_VER && _MSC_VER <= 1900
			template<typename TCurSrc, typename ...TRestSrc> CDataContainer(const TCurSrc &curSrc, const TRestSrc &...restSrc) : data(curSrc, restSrc...) {}
#else
			template<typename ...TSrc> CDataContainer(const TSrc &...src) : data(src...) {}
#endif
			CDataContainer &operator =(const CDataContainer &) = default;
			CDataContainer &operator =(CDataContainer &&) = default;
		};

#		pragma region Initializer list
		template<typename ElementType>
		class CInitListItem final
		{
			CInitListItem() = delete;
			CInitListItem(const CInitListItem &) = delete;
			CInitListItem &operator =(const CInitListItem &) = delete;

		private:
			static ElementType _GetItemElement(const void *item, unsigned)
			{
				return *reinterpret_cast<const ElementType *>(item);
			}

			template<unsigned int dimension>
			static ElementType _GetItemElement(const void *item, unsigned idx)
			{
				const auto &v = *reinterpret_cast<const vector<ElementType, dimension> *>(item);
				return v[idx];
			}

			template<unsigned int rows, unsigned int columns>
			static ElementType _GetItemElement(const void *item, unsigned idx)
			{
				const auto &m = *reinterpret_cast<const matrix<ElementType, rows, columns> *>(item);
				return m[idx / columns][idx % columns];
			}

			static void _Delete(const void *ptr)
			{
				delete (const ElementType *)ptr;
			}

			template<unsigned int dimension>
			static void _Delete(const void *ptr)
			{
				delete (const vector<ElementType, dimension> *)ptr;
			}

			template<unsigned int rows, unsigned int columns>
			static void _Delete(const void *ptr)
			{
				delete (const matrix<ElementType, rows, columns> *)ptr;
			}

			class CDeleter
			{
				typedef void(*TImpl)(const void *ptr);
				TImpl _impl;

			public:
				CDeleter(TImpl impl) : _impl(impl) {}

			public:
				void operator ()(const void *ptr) const
				{
					_impl(ptr);
				}
			};

		public:
			CInitListItem(const ElementType &item) :
				_getItemElement(_GetItemElement),
				_item(new ElementType(item), CDeleter(_Delete)),
				_itemSize(1)
			{
			}

			template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, class ItemSwizzleDesc>
			CInitListItem(const CSwizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) :
			_getItemElement(_GetItemElement<ItemSwizzleDesc::TDimension::value>),
				_item(new vector<ElementType, ItemSwizzleDesc::TDimension::value>(item), CDeleter(_Delete<ItemSwizzleDesc::TDimension::value>)),
				_itemSize(ItemSwizzleDesc::TDimension::value)
			{
			}

			template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
			CInitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) :
			_getItemElement(_GetItemElement<itemRows, itemColumns>),
				_item(new matrix<ElementType, itemRows, itemColumns>(item), CDeleter(_Delete<itemRows, itemColumns>)),
				_itemSize(itemRows * itemColumns)
			{
			}

			ElementType operator [](unsigned idx) const
			{
				return _getItemElement(_item.get(), idx);
			}

			unsigned GetItemSize() const noexcept
			{
				return _itemSize;
			}

		private:
			ElementType(&_getItemElement)(const void *, unsigned);
			const std::unique_ptr<const void, CDeleter> _item;
			const unsigned _itemSize;
		};
#		pragma endregion TODO: consider to remove it and rely on more efficient variadic template technique for heterogeneous ctors, or limit it usage for assignment operators only, or disable/specialize temp overloads to get rid of mem alloc

		// specializations for graphics vectors/matrices
#		define BOOST_PP_ITERATION_LIMITS (0, 4)
#		define BOOST_PP_FILENAME_1 "vector math.h"
#		include BOOST_PP_ITERATE()

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		vector<ElementType, SwizzleDesc::TDimension::value> operator -(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>>
		class CSwizzleBase : public SwizzleTag
		{
			/*
					  static
						 ^
						 |
			CSwizzleBase -> CSwizzle
			*/
			typedef CSwizzle<ElementType, rows, columns, SwizzleDesc> TSwizzle;

		protected:
			CSwizzleBase() = default;
			CSwizzleBase(const CSwizzleBase &) = default;
			~CSwizzleBase() = default;
			CSwizzleBase &operator =(const CSwizzleBase &) = default;

		public:
			operator const ElementType &() const noexcept
			{
				return static_cast<const TSwizzle &>(*this)[0];
			}
			operator ElementType &() noexcept
			{
				return static_cast<TSwizzle &>(*this)[0];
			}
			//operator const ElementType &() noexcept
			//{
			//	return operator ElementType &();
			//}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			// ICE on VS 2015
		private:
			friend vector<ElementType, SwizzleDesc::TDimension::value> operator -<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);
#endif

			template<size_t ...idx>
			inline auto Neg(std::index_sequence<idx...>) const
			{
				return vector<ElementType, SwizzleDesc::TDimension::value>(-static_cast<const TSwizzle &>(*this)[idx]...);
			}

		public:
			template<typename F>
			vector<std::result_of_t<F &(ElementType)>, SwizzleDesc::TDimension::value> apply(F f) const;

			template<typename TResult>
			vector<TResult, SwizzleDesc::TDimension::value> apply(TResult f(ElementType)) const
			{
				return apply<TResult (ElementType)>(f);
			}

			template<typename TResult>
			vector<TResult, SwizzleDesc::TDimension::value> apply(TResult f(const ElementType &)) const
			{
				return apply<TResult (const ElementType &)>(f);
			}

			vector<ElementType, SwizzleDesc::TDimension::value> apply(ElementType f(ElementType)) const
			{
				return apply<ElementType>(f);
			}

			vector<ElementType, SwizzleDesc::TDimension::value> apply(ElementType f(const ElementType &)) const
			{
				return apply<ElementType>(f);
			}
		};

		// CSwizzle inherits from this to reduce preprocessor generated code for faster compiling
		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzleCommon : public CSwizzleBase<ElementType, rows, columns, SwizzleDesc>
		{
			typedef CSwizzle<ElementType, rows, columns, SwizzleDesc> TSwizzle;
			typedef matrix<ElementType, rows, columns> Tmatrix;

		protected:
			CSwizzleCommon() = default;
			CSwizzleCommon(const CSwizzleCommon &) = delete;
			~CSwizzleCommon() = default;
			CSwizzleCommon &operator =(const CSwizzleCommon &) = delete;

		public:
			typedef TSwizzle TOperationResult;

		protected:
			operator TSwizzle &()
			{
				return static_cast<TSwizzle &>(*this);
			}

		public:
			const ElementType &operator [](unsigned int idx) const noexcept
			{
				assert((idx < SwizzleDesc::TDimension::value));
				idx = SwizzleDesc::packedSwizzle >> idx * 4u & (1u << 4u) - 1u;
				const auto row = idx >> 2 & 3u, column = idx & 3u;
				/*
							static	  reinterpret
							   ^		   ^
							   |		   |
				CSwizzleCommon -> CSwizzle -> CData
				*/
				typedef CData<ElementType, rows, columns> CData;
				return reinterpret_cast<const CData &>(static_cast<const TSwizzle &>(*this))._rows[row][column];
			}
			ElementType &operator [](unsigned int idx) noexcept
			{
				return const_cast<ElementType &>(static_cast<const CSwizzleCommon &>(*this)[idx]);
			}
		};

		// specialization for vectors
		template<typename ElementType, unsigned int vectorDimension, class SwizzleDesc>
		class CSwizzleCommon<ElementType, 0, vectorDimension, SwizzleDesc> : public CSwizzleBase<ElementType, 0, vectorDimension, SwizzleDesc>
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
			typedef vector<ElementType, vectorDimension> Tvector;

		protected:
			CSwizzleCommon() = default;
			CSwizzleCommon(const CSwizzleCommon &) = delete;
			~CSwizzleCommon() = default;
			CSwizzleCommon &operator =(const CSwizzleCommon &) = delete;

		public:
			typedef TSwizzle TOperationResult;

		protected:
			operator TSwizzle &()
			{
				return static_cast<TSwizzle &>(*this);
			}

		public:
			const ElementType &operator [](unsigned int idx) const noexcept
			{
				assert((idx < SwizzleDesc::TDimension::value));
				idx = SwizzleDesc::packedSwizzle >> idx * 4u & (1u << 4u) - 1u;
				/*
							static	  reinterpret
							   ^		   ^
							   |		   |
				CSwizzleCommon -> CSwizzle -> CData
				*/
				typedef CData<ElementType, 0, vectorDimension> CData;
				return reinterpret_cast<const CData &>(static_cast<const TSwizzle &>(*this)).data[idx];
			}
			ElementType &operator [](unsigned int idx) noexcept
			{
				return const_cast<ElementType &>(static_cast<const CSwizzleCommon &>(*this)[idx]);
			}
		};

		/*
		CVectorSwizzleDesc<vectorDimension> required for VS 2013/2015
		TODO: try with newer version
		*/
		template<typename ElementType, unsigned int vectorDimension>
		class CSwizzleCommon<ElementType, 0, vectorDimension, CVectorSwizzleDesc<vectorDimension>> : public CSwizzleBase<ElementType, 0, vectorDimension>
		{
			typedef vector<ElementType, vectorDimension> Tvector;

		protected:
			CSwizzleCommon() = default;
			CSwizzleCommon(const CSwizzleCommon &) = default;
			~CSwizzleCommon() = default;
			CSwizzleCommon &operator =(const CSwizzleCommon &) = default;
			// TODO: consider adding 'op=' operators to friends and making some stuff below protected/private (and TOperationResult for other CSwizzleCommon above)

		public:
			typedef Tvector TOperationResult;

		private:
			operator const Tvector &() const noexcept
			{
				/*
							static
							   ^
							   |
				CSwizzleCommon -> vector
				*/
				return static_cast<const Tvector &>(*this);
			}

			operator const Tvector &() noexcept
			{
				return static_cast<const CSwizzleCommon *>(this)->operator const Tvector &();
			}

		public:
			operator Tvector &() noexcept
			{
				return const_cast<Tvector &>(operator const Tvector &());
			}

		public:
			const ElementType &operator [](unsigned int idx) const noexcept
			{
				return operator const Tvector &()[idx];
			}

			ElementType &operator [](unsigned int idx) noexcept
			{
				return operator Tvector &()[idx];
			}
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzleAssign : public CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>
		{
			typedef CSwizzle<ElementType, rows, columns, SwizzleDesc> TSwizzle;
			// TODO: remove 'public'

		public:
			using typename CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>::TOperationResult;

		protected:
			CSwizzleAssign() = default;
			CSwizzleAssign(const CSwizzleAssign &) = default;
			~CSwizzleAssign() = default;

		private:
			template<typename Arg, bool WARHazard>
			class SelectAssign
			{
				typedef void (CSwizzleAssign::*const TAssign)(Arg);
			public:
				static constexpr TAssign Assign = WARHazard ? TAssign(&CSwizzleAssign::AssignCopy) : TAssign(&CSwizzleAssign::AssignDirect);
			};

		private:
			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
			inline void AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right);

			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
			inline void AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right);

		public:
#ifdef __GNUC__
			inline TOperationResult &operator =(const CSwizzleAssign &right)
#else
			inline TOperationResult &operator =(const CSwizzleAssign &right) &
#endif
			{
				return operator =<false>(static_cast<const TSwizzle &>(right));
			}

			// currently public to allow user specify WAR hazard explixitly if needed
			template<bool WARHazard, typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
#ifdef __GNUC__
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
#else
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right) &
#endif
			{
				constexpr auto Assign = SelectAssign<const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &, WARHazard>::Assign;
				(this->*Assign)(right);
				return *this;
			}

			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
#ifdef __GNUC__
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
#else
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right) &
#endif
			{
				static constexpr auto WARHazard = DetectSwizzleWARHazard
				<
					ElementType, rows, columns, SwizzleDesc,
					RightElementType, rightRows, rightColumns, RightSwizzleDesc,
					true
				>::value;
				return operator =<WARHazard>(right);
			}

			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
#ifdef __GNUC__
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)
#else
			TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right) &
#endif
			{
				return operator =<false>(right);
			}

			template<typename RightType>
#ifdef __GNUC__
			inline std::enable_if_t<IsScalar<RightType>, TOperationResult &> operator =(const RightType &scalar);
#elif defined _MSC_VER && _MSC_VER <= 1900
			inline std::enable_if_t<IsScalar<RightType>, TOperationResult &> operator =(const RightType &scalar) &
			{
				for (unsigned idx = 0; idx < SwizzleDesc::TDimension::value; idx++)
					(*this)[idx] = scalar;
				return *this;
			}
#else
			inline std::enable_if_t<IsScalar<RightType>, TOperationResult &> operator =(const RightType &scalar) &;
#endif

#ifdef __GNUC__
			inline TOperationResult &operator =(std::initializer_list<CInitListItem<ElementType>> initList);
#else
			inline TOperationResult &operator =(std::initializer_list<CInitListItem<ElementType>> initList) &;
#endif

		public:
			using CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>::operator [];
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
		inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
		{
			/*
			NOTE: assert should not be triggered if WAR hazard not detected.
			C-style casts to CDataContainer below performed as static_cast if corresponding swizzle is
			CDataContainer's base and as reinterpret_cast if swizzle is part of CDataContainer's union.
			*/
			assert((
				!DetectSwizzleWARHazard
				<
					ElementType, rows, columns, SwizzleDesc,
					RightElementType, rightRows, rightColumns, RightSwizzleDesc,
					true
				>::value ||
				(const CDataContainer<ElementType, rows, columns> *)static_cast<const CSwizzle<ElementType, rows, columns, SwizzleDesc> *>(this) !=
				(const void *)(const CDataContainer<RightElementType, rightRows, rightColumns> *)&right));
			static_assert(SwizzleDesc::TDimension::value <= RightSwizzleDesc::TDimension::value, "operator =: too small src dimension");
			for (unsigned idx = 0; idx < SwizzleDesc::TDimension::value; idx++)
				(*this)[idx] = right[idx];
		}

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc>
		inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
		{
			// make copy and call AssignDirect()
			AssignDirect(vector<RightElementType, RightSwizzleDesc::TDimension::value>(right));
		}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename RightType>
#ifdef __GNUC__
		inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(const RightType &scalar) -> std::enable_if_t<IsScalar<RightType>, TOperationResult &>
#else
		inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(const RightType &scalar) & -> std::enable_if_t<IsScalar<RightType>, TOperationResult &>
#endif
		{
			for (unsigned idx = 0; idx < SwizzleDesc::TDimension::value; idx++)
				(*this)[idx] = scalar;
			return *this;
		}
#endif

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
#ifdef __GNUC__
		inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> TOperationResult &
#else
		inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) & -> TOperationResult &
#endif
		{
			unsigned dst_idx = 0;
			for (const auto &item : initList)
				for (unsigned item_element_idx = 0; item_element_idx < item.GetItemSize(); item_element_idx++)
					(*this)[dst_idx++] = item[item_element_idx];
			assert(dst_idx == SwizzleDesc::TDimension::value);
			return *this;
		}

		// this specialization used as base class for CDataContainer to eliminate need for various overloads
		/*
		CVectorSwizzleDesc<vectorDimension> required for VS 2013/2015
		TODO: try with newer version
		*/
		template<typename ElementType, unsigned int vectorDimension>
		class CSwizzle<ElementType, 0, vectorDimension, CVectorSwizzleDesc<vectorDimension>, std::true_type> : public CSwizzleAssign<ElementType, 0, vectorDimension>
		{
		protected:
			CSwizzle() = default;
			CSwizzle(const CSwizzle &) = default;
			~CSwizzle() = default;
			CSwizzle &operator =(const CSwizzle &) = default;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzle<ElementType, rows, columns, SwizzleDesc, std::false_type> : public CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>
		{
			friend class CDataContainerImpl<ElementType, rows, columns, false>;
			friend class CDataContainerImpl<ElementType, rows, columns, true>;

		public:
			CSwizzle &operator =(const CSwizzle &) = delete;

		private:
			CSwizzle() = default;
			CSwizzle(const CSwizzle &) = delete;
			~CSwizzle() = default;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzle<ElementType, rows, columns, SwizzleDesc, std::true_type> : public CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>
		{
			friend class CDataContainerImpl<ElementType, rows, columns, false>;
			friend class CDataContainerImpl<ElementType, rows, columns, true>;

		public:
#ifdef __GNUC__
			CSwizzle &operator =(const CSwizzle &) = default;
#else
			CSwizzle &operator =(const CSwizzle &) & = default;
#endif
			using CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =;

		private:
			CSwizzle() = default;
			CSwizzle(const CSwizzle &) = delete;
			~CSwizzle() = default;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzleIteratorImpl : public std::iterator<std::forward_iterator_tag, const ElementType>
		{
			const CSwizzle<ElementType, rows, columns, SwizzleDesc> &_swizzle;
			unsigned _i;

		protected:
			CSwizzleIteratorImpl(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle, unsigned i):
			_swizzle(swizzle), _i(i) {}
			~CSwizzleIteratorImpl() = default;

		public:	// required by stl => public
			std::conditional_t
			<
				sizeof(typename CSwizzleIteratorImpl::value_type) <= sizeof(void *),
				typename CSwizzleIteratorImpl::value_type,
				typename CSwizzleIteratorImpl::reference
			> operator *() const
			{
				return _swizzle[_i];
			}
			typename CSwizzleIteratorImpl::pointer operator ->() const
			{
				return &_swizzle[_i];
			}
			bool operator ==(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc> src) const noexcept
			{
				assert(&_swizzle == &src._swizzle);
				return _i == src._i;
			}
			bool operator !=(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc> src) const noexcept
			{
				assert(&_swizzle == &src._swizzle);
				return _i != src._i;
			}
			CSwizzleIteratorImpl &operator ++()
			{
				++_i;
				return *this;
			}
			CSwizzleIteratorImpl operator ++(int)
			{
				CSwizzleIteratorImpl old(*this);
				operator ++();
				return old;
			}
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		class CSwizzleIterator : public CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc>
		{
			friend bool all<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);
			friend bool any<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);
			friend bool none<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src);

			// use C++11 inheriting ctor
			CSwizzleIterator(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle, unsigned i):
			CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc>(swizzle, i) {}
			// copy ctor required by stl => public
			~CSwizzleIterator() = default;
		};

#		pragma region generate operators
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline vector<ElementType, SwizzleDesc::TDimension::value> operator +(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src)
			{
				return src;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline vector<ElementType, SwizzleDesc::TDimension::value> operator -(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src)
			{
				return src.Neg(std::make_index_sequence<SwizzleDesc::TDimension::value>());
			}

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					bool WARHazard,																																\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &operator op##=(							\
				CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																		\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					if (WARHazard)																																\
						return operator op##=<false>(left, vector<RightElementType, RightSwizzleDesc::TDimension::value>(right));								\
					else																																		\
					{																																			\
						assert((																																\
						!DetectSwizzleWARHazard																													\
							<																																	\
								LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																		\
								RightElementType, rightRows, rightColumns, RightSwizzleDesc,																	\
								false																															\
							>::value ||																															\
							(const CDataContainer<LeftElementType, leftRows, leftColumns> *)(&left) !=															\
							(const void *)(const CDataContainer<RightElementType, rightRows, rightColumns> *)&right));											\
						static_assert(LeftSwizzleDesc::isWriteMaskValid, "operator "#op"=: invalid write mask");												\
						static_assert(LeftSwizzleDesc::TDimension::value <= RightSwizzleDesc::TDimension::value, "operator "#op"=: too small src dimension");	\
						for (typename LeftSwizzleDesc::TDimension::value_type i = 0; i < LeftSwizzleDesc::TDimension::value; i++)								\
							left[i] op##= right[i];																												\
						return left;																															\
					}																																			\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &operator op##=(							\
				CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																		\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					static constexpr auto WARHazard = DetectSwizzleWARHazard																					\
					<																																			\
						LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																				\
						RightElementType, rightRows, rightColumns, RightSwizzleDesc,																			\
						false																																	\
					>::value;																																	\
					return operator op##=<WARHazard>(left, right);																								\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &operator op##=(							\
				CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																		\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &&right)															\
				{																																				\
					return operator op##=<false>(left, right);																									\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightType																															\
				>																																				\
				inline std::enable_if_t<IsScalar<RightType>,																									\
				typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc>::TOperationResult &> operator op##=(									\
				CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																		\
				RightType right)																																\
				{																																				\
					static_assert(LeftSwizzleDesc::isWriteMaskValid, "operator "#op"=: invalid write mask");													\
					for (typename LeftSwizzleDesc::TDimension::value_type i = 0; i < LeftSwizzleDesc::TDimension::value; i++)									\
						left[i] op##= right;																													\
					return left;																																\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline vector																																	\
				<																																				\
					decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																\
					mpl::min																																	\
					<																																			\
						typename LeftSwizzleDesc::TDimension,																									\
						typename RightSwizzleDesc::TDimension																									\
					>::type::value																																\
				> operator op(																																	\
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																	\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					vector																																		\
					<																																			\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),															\
						mpl::min																																\
						<																																		\
							typename LeftSwizzleDesc::TDimension,																								\
							typename RightSwizzleDesc::TDimension																								\
						>::type::value																															\
					> result(left);																																\
					return operator op##=<false>(result, right);																								\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline vector																																	\
				<																																				\
					bool,																																		\
					mpl::min																																	\
					<																																			\
						typename LeftSwizzleDesc::TDimension,																									\
						typename RightSwizzleDesc::TDimension																									\
					>::type::value																																\
				> operator op(																																	\
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																	\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					constexpr unsigned int dimension = mpl::min																									\
					<																																			\
						typename LeftSwizzleDesc::TDimension,																									\
						typename RightSwizzleDesc::TDimension																									\
					>::type::value;																																\
					vector<bool, dimension> result;																												\
					for (unsigned i = 0; i < dimension; i++)																									\
						result[i] = left[i] op right[i];																										\
					return result;																																\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightType																															\
				>																																				\
				inline std::enable_if_t<IsScalar<RightType>, vector																								\
				<																																				\
					decltype(std::declval<LeftElementType>() op std::declval<RightType>()),																		\
					LeftSwizzleDesc::TDimension::value																											\
				>> operator op(																																	\
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																	\
				const RightType &right)																															\
				{																																				\
					vector																																		\
					<																																			\
						decltype(std::declval<LeftElementType>() op std::declval<RightType>()),																	\
						LeftSwizzleDesc::TDimension::value																										\
					> result(left);																																\
					return result op##= right;																													\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,											\
					typename RightType																															\
				>																																				\
				inline std::enable_if_t<IsScalar<RightType>, vector																								\
				<																																				\
					bool,																																		\
					LeftSwizzleDesc::TDimension::value																											\
				>> operator op(																																	\
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,																	\
				const RightType &right)																															\
				{																																				\
					constexpr unsigned int dimension = LeftSwizzleDesc::TDimension::value;																		\
					vector<bool, dimension> result;																												\
					for (unsigned i = 0; i < dimension; i++)																									\
						result[i] = left[i] op right;																											\
					return result;																																\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftType,																															\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline std::enable_if_t<IsScalar<LeftType>, vector																								\
				<																																				\
					decltype(std::declval<LeftType>() op std::declval<RightElementType>()),																		\
					RightSwizzleDesc::TDimension::value																											\
				>> operator op(																																	\
				const LeftType &left,																															\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					vector																																		\
					<																																			\
						decltype(std::declval<LeftType>() op std::declval<RightElementType>()),																	\
						RightSwizzleDesc::TDimension::value																										\
					> result(left);																																\
					return operator op##=<false>(result, right);																								\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																														\
				template																																		\
				<																																				\
					typename LeftType,																															\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc										\
				>																																				\
				inline std::enable_if_t<IsScalar<LeftType>, vector																								\
				<																																				\
					bool,																																		\
					RightSwizzleDesc::TDimension::value																											\
				>> operator op(																																	\
				const LeftType &left,																															\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)																\
				{																																				\
					constexpr unsigned int dimension = RightSwizzleDesc::TDimension::value;																		\
					vector<bool, dimension> result;																												\
					for (unsigned i = 0; i < dimension; i++)																									\
						result[i] = left op right[i];																											\
					return result;																																\
				};
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION
#		pragma endregion

		template<typename ElementType_, unsigned int dimension_>
		class vector : public CDataContainer<ElementType_, 0, dimension_>
		{
			static_assert(dimension_ > 0, "vector dimension should be positive");
			typedef CDataContainer<ElementType_, 0, dimension_> DataContainer;
			//using CDataContainer<ElementType_, 0, dimension_>::data;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int dimension = dimension_;

			vector() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<typename SrcType, typename = std::enable_if_t<IsScalar<SrcType>>>
			vector(const SrcType &scalar);

			template<typename First, typename ...Rest, typename = std::enable_if_t<(sizeof...(Rest) > 0) || IsMatrix<First>>>
			vector(const First &first, const Rest &...rest);

			vector(std::initializer_list<CInitListItem<ElementType>> initList);

			//template<typename TIterator>
			//explicit vector(TIterator src);

			template<typename SrcElementType, unsigned int srcDimension>
			vector(const SrcElementType (&src)[srcDimension]);

#ifdef __GNUC__
			vector &operator =(const vector &) = default;
#else
			vector &operator =(const vector &) & = default;
#endif
			using CSwizzleAssign<ElementType, 0, dimension>::operator =;

			const ElementType &operator [](unsigned int idx) const noexcept;

			ElementType &operator [](unsigned int idx) noexcept;

		private:
			template<typename ElementType, unsigned int rows, unsigned int columns>
			friend class CData;

			// workaround for bug in VS 2015 Update 2
			typedef std::make_index_sequence<dimension> IdxSeq;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false>, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src);

			template<unsigned offset, typename First, typename ...Rest>
			vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false, offset>, const First &first, const Rest &...rest);

			template<typename SrcElementType, unsigned int srcDimension>
			vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false>, const SrcElementType (&src)[srcDimension]);
		};

		template<typename ElementType_, unsigned int rows_, unsigned int columns_>
		class matrix : public CDataContainer<ElementType_, rows_, columns_>
		{
			static_assert(rows_ > 0, "matrix should contain at leat 1 row");
			static_assert(columns_ > 0, "matrix should contain at leat 1 column");
			friend bool all<>(const matrix<ElementType_, rows_, columns_> &);
			friend bool any<>(const matrix<ElementType_, rows_, columns_> &);
			friend bool none<>(const matrix<ElementType_, rows_, columns_> &);
			typedef vector<ElementType_, columns_> TRow;
			typedef CDataContainer<ElementType_, rows_, columns_> DataContainer;

		public:
			typedef ElementType_ ElementType;
			static constexpr unsigned int rows = rows_, columns = columns_;

			matrix() = default;

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const matrix<SrcElementType, srcRows, srcColumns> &src);

			template<typename SrcType, typename = std::enable_if_t<IsScalar<SrcType>>>
			matrix(const SrcType &scalar);

			template<typename First, typename ...Rest, typename = std::enable_if_t<(sizeof...(Rest) > 0) || IsSwizzle<First>>>
			matrix(const First &first, const Rest &...rest);

			matrix(std::initializer_list<CInitListItem<ElementType>> initList);

			//template<typename TIterator>
			//explicit matrix(TIterator src);

			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			matrix(const SrcElementType (&src)[srcRows][srcColumns]);

#ifdef __GNUC__
			matrix &operator =(const matrix &) = default;
#else
			matrix &operator =(const matrix &) & = default;
#endif

			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
#ifdef __GNUC__
			matrix &operator =(const matrix<RightElementType, rightRows, rightColumns> &right);
#else
			matrix &operator =(const matrix<RightElementType, rightRows, rightColumns> &right) &;
#endif

			template<typename SrcType>
#ifdef __GNUC__
			std::enable_if_t<IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar);
#elif defined _MSC_VER && _MSC_VER <= 1900
			std::enable_if_t<IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar) &
			{
				for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					operator [](rowIdx) = scalar;
				return *this;
			}
#else
			std::enable_if_t<IsScalar<SrcType>, matrix &> operator =(const SrcType &scalar) &;
#endif

#ifdef __GNUC__
			matrix &operator =(std::initializer_list<CInitListItem<ElementType>> initList);
#else
			matrix &operator =(std::initializer_list<CInitListItem<ElementType>> initList) &;
#endif

			matrix operator +() const
			{
				return *this;
			}

			matrix operator -() const;

#			define OPERATOR_DECLARATION(op)																\
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>	\
				matrix &operator op##=(const matrix<RightElementType, rightRows, rightColumns> &right);
			GENERATE_OPERATORS(OPERATOR_DECLARATION, ARITHMETIC_OPS)
#			undef OPERATOR_DECLARATION

#if defined _MSC_VER && _MSC_VER <= 1900
#			define OPERATOR_DEFINITION(op)																\
				template<typename RightType>															\
				inline std::enable_if_t<IsScalar<RightType>, matrix &> operator op##=(RightType right)	\
				{																						\
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)									\
						operator [](rowIdx) op##= right;												\
					return *this;																		\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION
#else
#			define OPERATOR_DECLARATION(op)																\
				template<typename RightType>															\
				std::enable_if_t<IsScalar<RightType>, matrix &> operator op##=(RightType right);
			GENERATE_OPERATORS(OPERATOR_DECLARATION, ARITHMETIC_OPS)
#			undef OPERATOR_DECLARATION
#endif

			const TRow &operator [](unsigned int idx) const noexcept;

			TRow &operator [](unsigned int idx) noexcept;

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
			inline auto Neg(std::index_sequence<idx...>) const;
		};

		template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
		template<typename F>
		inline vector<std::result_of_t<F &(ElementType)>, SwizzleDesc::TDimension::value>
		CSwizzleBase<ElementType, rows, columns, SwizzleDesc>::apply(F f) const
		{
			constexpr unsigned int dimension = SwizzleDesc::TDimension::value;
			vector<std::result_of_t<F &(ElementType)>, dimension> result;
			for (unsigned i = 0; i < dimension; i++)
				result[i] = f(static_cast<const TSwizzle &>(*this)[i]);
			return result;
		}

#		pragma region vector impl
			template<typename ElementType, unsigned int dimension>
			inline const ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) const noexcept
			{
				assert(idx < dimension);
				return DataContainer::data.data[idx];
			}

			template<typename ElementType, unsigned int dimension>
			inline ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) noexcept
			{
				assert(idx < dimension);
				return DataContainer::data.data[idx];
			}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline vector<ElementType, dimension>::vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(CData<ElementType, 0, dimension>::HeterogeneousInitTag<IdxSeq>(), src) {}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcType, typename = std::enable_if_t<IsScalar<SrcType>>>
			inline vector<ElementType, dimension>::vector(const SrcType &scalar) :
				DataContainer(IdxSeq(), scalar) {}

			template<typename ElementType, unsigned int dimension>
			template<typename First, typename ...Rest, typename = std::enable_if_t<(sizeof...(Rest) > 0) || IsMatrix<First>>>
			inline vector<ElementType, dimension>::vector(const First &first, const Rest &...rest) :
				DataContainer(CData<ElementType, 0, dimension>::HeterogeneousInitTag<IdxSeq>(), first, rest...) {}

			//template<typename ElementType, unsigned int dimension>
			//template<typename TIterator>
			//inline vector<ElementType, dimension>::vector(TIterator src)
			//{
			//	std::copy_n(src, dimension, DataContainer::data.data);
			//}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcDimension>
			inline vector<ElementType, dimension>::vector(const SrcElementType (&src)[srcDimension]) :
				DataContainer(CData<ElementType, 0, dimension>::HeterogeneousInitTag<IdxSeq>(), src) {}

			template<typename ElementType, unsigned int dimension>
			inline vector<ElementType, dimension>::vector(std::initializer_list<CInitListItem<ElementType>> initList)
			{
				operator =(initList);
			}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc>
			inline vector<ElementType, dimension>::vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false> tag, const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc> &src) :
				DataContainer(tag, src) {}

			template<typename ElementType, unsigned int dimension>
			template<unsigned offset, typename First, typename ...Rest>
			inline vector<ElementType, dimension>::vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false, offset> tag, const First &first, const Rest &...rest) :
				DataContainer(tag, first, rest...) {}

			template<typename ElementType, unsigned int dimension>
			template<typename SrcElementType, unsigned int srcDimension>
			inline vector<ElementType, dimension>::vector(typename CData<ElementType, 0, dimension>::template HeterogeneousInitTag<IdxSeq, false> tag, const SrcElementType (&src)[srcDimension]) :
				DataContainer(tag, src) {}
#		pragma endregion

#		pragma region matrix impl
			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const noexcept -> const typename matrix::TRow &
			{
				assert(idx < rows);
				return DataContainer::data._rows[idx];
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) noexcept -> typename matrix::TRow &
			{
				assert(idx < rows);
				return DataContainer::data._rows[idx];
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const matrix<SrcElementType, srcRows, srcColumns> &src) :
				DataContainer(std::make_index_sequence<rows>(), src) {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcType, typename = std::enable_if_t<IsScalar<SrcType>>>
			inline matrix<ElementType, rows, columns>::matrix(const SrcType &scalar) :
				DataContainer(std::make_index_sequence<rows>(), scalar) {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename First, typename ...Rest, typename = std::enable_if_t<(sizeof...(Rest) > 0) || IsSwizzle<First>>>
			inline matrix<ElementType, rows, columns>::matrix(const First &first, const Rest &...rest) :
				DataContainer(CData<ElementType, rows, columns>::HeterogeneousInitTag<std::make_index_sequence<rows>>(), first, rest...) {}

			//template<typename ElementType, unsigned int rows, unsigned int columns>
			//template<typename TIterator>
			//inline matrix<ElementType, rows, columns>::matrix(TIterator src)
			//{
			//	for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++, src += columns)
			//		operator [](rowIdx) = src;
			//}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
			inline matrix<ElementType, rows, columns>::matrix(const SrcElementType (&src)[srcRows][srcColumns]) :
				DataContainer(std::make_index_sequence<rows>(), src) {}

			template<typename ElementType, unsigned int rows, unsigned int columns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> matrix &
#else
			inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) & -> matrix &
#endif
			{
				unsigned dst_idx = 0;
				for (const auto &item : initList)
					for (unsigned item_element_idx = 0; item_element_idx < item.GetItemSize(); item_element_idx++, dst_idx++)
						(*this)[dst_idx / columns][dst_idx % columns] = item[item_element_idx];
				assert(dst_idx == rows * columns);
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline matrix<ElementType, rows, columns>::matrix(std::initializer_list<CInitListItem<ElementType>> initList)
			{
				operator =(initList);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const matrix<RightElementType, rightRows, rightColumns> &right) -> matrix &
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const matrix<RightElementType, rightRows, rightColumns> &right) & -> matrix &
#endif
			{
				static_assert(rows <= rightRows, "operator =: too few rows in src");
				static_assert(columns <= rightColumns, "operator =: too few columns in src");
				for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					operator [](rowIdx) = right[rowIdx];
				return *this;
			}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename SrcType>
#ifdef __GNUC__
			inline auto matrix<ElementType, rows, columns>::operator =(const SrcType &scalar) -> std::enable_if_t<IsScalar<SrcType>, matrix &>
#else
			inline auto matrix<ElementType, rows, columns>::operator =(const SrcType &scalar) & -> std::enable_if_t<IsScalar<SrcType>, matrix &>
#endif
			{
				for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					operator [](rowIdx) = scalar;
				return *this;
			}
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<size_t ...idx>
			inline auto matrix<ElementType, rows, columns>::Neg(std::index_sequence<idx...>) const
			{
				return matrix(-operator [](idx)...);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns>
			inline auto matrix<ElementType, rows, columns>::operator -() const -> matrix
			{
				return Neg(std::make_index_sequence<rows>());
			}

#			define OPERATOR_DEFINITION(op)																													\
				template<typename ElementType, unsigned int rows, unsigned int columns>																		\
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>														\
				inline auto matrix<ElementType, rows, columns>::operator op##=(const matrix<RightElementType, rightRows, rightColumns> &right) -> matrix &	\
				{																																			\
					static_assert(rows <= rightRows, "operator "#op"=: too few rows in src");																\
					static_assert(columns <= rightColumns, "operator "#op"=: too few columns in src");														\
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)																						\
						operator [](rowIdx) op##= right[rowIdx];																							\
					return *this;																															\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#if !(defined _MSC_VER && _MSC_VER <= 1900)
#			define OPERATOR_DEFINITION(op)																											\
				template<typename ElementType, unsigned int rows, unsigned int columns>																\
				template<typename RightType>																										\
				inline auto matrix<ElementType, rows, columns>::operator op##=(RightType right) -> std::enable_if_t<IsScalar<RightType>, matrix &>	\
				{																																	\
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)																				\
						operator [](rowIdx) op##= right;																							\
					return *this;																													\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION
#endif

#			define OPERATOR_DEFINITION(op)																						\
				template																										\
				<																												\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,									\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns								\
				>																												\
				inline matrix																									\
				<																												\
					decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),								\
					mpl::min<mpl::integral_c<unsigned, leftRows>, mpl::integral_c<unsigned, rightRows>>::type::value,			\
					mpl::min<mpl::integral_c<unsigned, leftColumns>, mpl::integral_c<unsigned, rightColumns>>::type::value		\
				> operator op(																									\
				const matrix<LeftElementType, leftRows, leftColumns> &left,														\
				const matrix<RightElementType, rightRows, rightColumns> &right)													\
				{																												\
					matrix																										\
					<																											\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),							\
						mpl::min<mpl::integral_c<unsigned, leftRows>, mpl::integral_c<unsigned, rightRows>>::type::value,		\
						mpl::min<mpl::integral_c<unsigned, leftColumns>, mpl::integral_c<unsigned, rightColumns>>::type::value	\
					>																											\
					result(left);																								\
					return result op##= right;																					\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)																									\
				template																													\
				<																															\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,												\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns											\
				>																															\
				inline matrix																												\
				<																															\
					bool,																													\
					mpl::min<mpl::integral_c<unsigned, leftRows>, mpl::integral_c<unsigned, rightRows>>::type::value,						\
					mpl::min<mpl::integral_c<unsigned, leftColumns>, mpl::integral_c<unsigned, rightColumns>>::type::value					\
				> operator op(																												\
				const matrix<LeftElementType, leftRows, leftColumns> &left,																	\
				const matrix<RightElementType, rightRows, rightColumns> &right)																\
				{																															\
					constexpr unsigned int																									\
						rows = mpl::min<mpl::integral_c<unsigned, leftRows>, mpl::integral_c<unsigned, rightRows>>::type::value,			\
						columns = mpl::min<mpl::integral_c<unsigned, leftColumns>, mpl::integral_c<unsigned, rightColumns>>::type::value;	\
					matrix<bool, rows, columns> result;																						\
					for (unsigned i = 0; i < rows; i++)																						\
						result[i] = left[i] op right[i];																					\
					return result;																											\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)														\
				template																		\
				<																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,	\
					typename RightType															\
				>																				\
				inline std::enable_if_t<IsScalar<RightType>, matrix								\
				<																				\
					decltype(std::declval<LeftElementType>() op std::declval<RightType>()),		\
					leftRows, leftColumns														\
				>> operator op(																	\
				const matrix<LeftElementType, leftRows, leftColumns> &left,						\
				const RightType &right)															\
				{																				\
					matrix																		\
					<																			\
						decltype(std::declval<LeftElementType>() op std::declval<RightType>()),	\
						leftRows, leftColumns													\
					>																			\
					result(left);																\
					return result op##= right;													\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)														\
				template																		\
				<																				\
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,	\
					typename RightType															\
				>																				\
				inline std::enable_if_t<IsScalar<RightType>, matrix								\
				<																				\
					bool,																		\
					leftRows, leftColumns														\
				>> operator op(																	\
				const matrix<LeftElementType, leftRows, leftColumns> &left,						\
				const RightType &right)															\
				{																				\
					matrix<bool, leftRows, leftColumns> result;									\
					for (unsigned i = 0; i < leftRows; i++)										\
						result[i] = left[i] op right;											\
					return result;																\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)															\
				template																			\
				<																					\
					typename LeftType,																\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns	\
				>																					\
				inline std::enable_if_t<IsScalar<LeftType>, matrix									\
				<																					\
					decltype(std::declval<LeftType>() op std::declval<RightElementType>()),			\
					rightRows, rightColumns															\
				>> operator op(																		\
				const LeftType &left,																\
				const matrix<RightElementType, rightRows, rightColumns> &right)						\
				{																					\
					matrix																			\
					<																				\
						decltype(std::declval<LeftType>() op std::declval<RightElementType>()),		\
						rightRows, rightColumns														\
					>																				\
					result(left);																	\
					return result op##= right;														\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#			undef OPERATOR_DEFINITION

#			define OPERATOR_DEFINITION(op)															\
				template																			\
				<																					\
					typename LeftType,																\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns	\
				>																					\
				inline std::enable_if_t<IsScalar<LeftType>, matrix									\
				<																					\
					bool,																			\
					rightRows, rightColumns															\
				>> operator op(																		\
				const LeftType &left,																\
				const matrix<RightElementType, rightRows, rightColumns> &right)						\
				{																					\
					matrix<bool, rightRows, rightColumns> result;									\
					for (unsigned i = 0; i < rightRows; i++)										\
						result[i] = left op right[i];												\
					return result;																	\
				}
			GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#			undef OPERATOR_DEFINITION

			template<typename ElementType, unsigned int rows, unsigned int columns>
			template<typename F>
			inline auto matrix<ElementType, rows, columns>::apply(F f) const -> matrix<std::result_of_t<F &(ElementType)>, rows, columns>
			{
				matrix<std::result_of_t<F &(ElementType)>, rows, columns> result;
				for (unsigned i = 0; i < rows; i++)
					result[i] = (*this)[i].apply(f);
				return result;
			}
#		pragma endregion

#		pragma region min/max functions
			// std::min/max requires explicit template param if used for different types => provide scalar version
#			define FUNCTION_DEFINITION(f)																					\
				template<typename LeftElementType, typename RightElementType>												\
				inline auto f(const LeftElementType &left, const RightElementType &right) -> decltype(left - right)			\
				{																											\
					return std::f<decltype(left - right)>(left, right);														\
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
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)							\
				-> decltype(left - right)																					\
				{																											\
					typedef decltype(left - right) TResult;																	\
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
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,								\
				const RightType &right)																						\
				-> std::enable_if_t<IsScalar<RightType>, decltype(left - right)>											\
				{																											\
					typedef decltype(left - right) TResult;																	\
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
					typename Leftype,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc	\
				>																											\
				inline auto f(																								\
				const Leftype &left,																						\
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)							\
				-> std::enable_if_t<IsScalar<Leftype>, decltype(left - right)>												\
				{																											\
					typedef decltype(left - right) TResult;																	\
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
				-> decltype(left - right)																					\
				{																											\
					typedef decltype(left - right) TResult;																	\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::rows; i++)															\
						result[i] = VectorMath::f(left[i], right[i]);														\
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
				-> std::enable_if_t<IsScalar<RightType>, decltype(left - right)>											\
				{																											\
					typedef decltype(left - right) TResult;																	\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::rows; i++)															\
						result[i] = VectorMath::f(left[i], right);															\
					return result;																							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION(f)																					\
				template																									\
				<																											\
					typename Leftype,																						\
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns							\
				>																											\
				auto f(																										\
				const Leftype &left,																						\
				const matrix<RightElementType, rightRows, rightColumns> &right)												\
				-> std::enable_if_t<IsScalar<Leftype>, decltype(left - right)>												\
				{																											\
					typedef decltype(left - right) TResult;																	\
					TResult result;																							\
					for (unsigned i = 0; i < TResult::rows; i++)															\
						result[i] = VectorMath::f(left, right[i]);															\
					return result;																							\
				}
			FUNCTION_DEFINITION(min)
			FUNCTION_DEFINITION(max)
#			undef FUNCTION_DEFINITION
#		pragma endregion

#		pragma region all/any/none functions
#			define FUNCTION_DEFINITION(f)																										\
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>										\
				inline bool f(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &src)														\
				{																																\
					typedef CSwizzleIterator<ElementType, rows, columns, SwizzleDesc> TSwizzleIterator;											\
					return std::f##_of(TSwizzleIterator(src, 0), TSwizzleIterator(src, SwizzleDesc::TDimension::value), [](std::conditional_t	\
					<																															\
						sizeof(typename TSwizzleIterator::value_type) <= sizeof(void *),														\
						typename TSwizzleIterator::value_type,																					\
						typename TSwizzleIterator::reference																					\
					> element) -> bool {return element;});																						\
				};
			FUNCTION_DEFINITION(all)
			FUNCTION_DEFINITION(any)
			FUNCTION_DEFINITION(none)
#			undef FUNCTION_DEFINITION

#			define FUNCTION_DEFINITION1(f) FUNCTION_DEFINITION2(f, f)
#			define FUNCTION_DEFINITION2(f1, f2)																									\
				template<typename ElementType, unsigned int rows, unsigned int columns>															\
				bool f1(const matrix<ElementType, rows, columns> &src)																			\
				{																																\
					return std::f2##_of(src.data._rows, src.data._rows + rows, f1<ElementType, 0, columns, CVectorSwizzleDesc<columns>>);		\
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
			inline decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()) mul(
			const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				constexpr unsigned rowXcolumnDimension = mpl::min
				<
					typename LeftSwizzleDesc::TDimension,
					typename RightSwizzleDesc::TDimension
				>::type::value;
				typedef decltype(std::declval<LeftElementType>() + std::declval<RightElementType>()) ElementType;

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
			vector<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()), rightColumns> mul(
			const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const matrix<RightElementType, rightRows, rightColumns> &right)
			{
				constexpr unsigned
					resultColumns = rightColumns,
					rowXcolumnDimension = mpl::min
					<
						typename LeftSwizzleDesc::TDimension,
						mpl::integral_c<unsigned, rightRows>
					>::type::value;
				typedef decltype(std::declval<LeftElementType>() + std::declval<RightElementType>()) ElementType;

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
			vector<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()), leftRows> mul(
			const matrix<LeftElementType, leftRows, leftColumns> &left,
			const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			{
				constexpr unsigned
					resultRows = leftRows,
					rowXcolumnDimension = mpl::min
					<
						mpl::integral_c<unsigned, leftColumns>,
						typename RightSwizzleDesc::TDimension
					>::type::value;
				typedef decltype(std::declval<LeftElementType>() + std::declval<RightElementType>()) ElementType;

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
			matrix<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()), leftRows, rightColumns> mul(
			const matrix<LeftElementType, leftRows, leftColumns> &left,
			const matrix<RightElementType, rightRows, rightColumns> &right)
			{
				constexpr unsigned
					resultRows = leftRows,
					resultColumns = rightColumns,
					rowXcolumnDimension = mpl::min
					<
						mpl::integral_c<unsigned, leftColumns>,
						mpl::integral_c<unsigned, rightRows>
					>::type::value;
				typedef decltype(std::declval<LeftElementType>() + std::declval<RightElementType>()) ElementType;

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
			const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			-> decltype(mul(left, right))
			{
				return mul(left, right);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto length(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			-> decltype(sqrt(dot(swizzle, swizzle)))
			{
				return sqrt(dot(swizzle, swizzle));
			}

			template
			<
				typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
				typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc
			>
			inline auto distance(
			const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
			const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
			-> decltype(length(right - left))
			{
				return length(right - left);
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc>
			inline auto normalize(const CSwizzle<ElementType, rows, columns, SwizzleDesc> &swizzle)
			-> decltype(swizzle / length(swizzle))
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
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				-> decltype(mul(left, right))
				{
					return mul(left, right);
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				inline auto operator ,(
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
				-> decltype(mul(left, right))
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
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc> &right)
				-> decltype(mul(left, right))
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
				-> decltype(mul(left, right))
				{
					return mul(left, right);
				}
#			pragma endregion
#		pragma endregion

#		undef ARITHMETIC_OPS
#		undef REL_OPS
#		undef GENERATE_OPERATORS_MACRO
#		undef GENERATE_OPERATORS
	}
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED

#	pragma warning(pop)

#endif//__VECTOR_MATH_H__
#endif