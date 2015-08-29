/**
\author		Alexey Shaydurov aka ASH
\date		4.12.2013 (c)Alexey Shaydurov

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
function template with same name from other namespace can have hier priority when passing vector arguments
(because no conversion required in contrast to vector->CSwizzle conversion required for function in VectorMath)
consider overloads with vector arguments to eliminate this issue

apply now have overloads similar to valarray's apply (in order to handle functions having template overloads) and some other overloads in addition
but it still does not perform as desired (vector<int, ...>.apply(floor) for example)
consider using preprocessor instead of templates or overloading each target function (floor(const vector<T, ...> &)

no need for '&& = const &&' overloads because '&& = const &' will be used in this case ('&&' can not be converted to non-const '&')
same applies for 'op='
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

#ifdef MSVC_LIMITATIONS
	template<typename ElementType>
	class CDataContainer<ElementType, ROWS, COLUMNS>: public std::conditional<ROWS == 0, CSwizzle<ElementType, 0, COLUMNS>, CEmpty>::type
	{
	protected:
		// forward ctors/dtor/= to _data
		CDataContainer() = default;
		CDataContainer(const CDataContainer &src) : _data(src._data)
		{
		}
		CDataContainer(CDataContainer &&src) : _data(std::move(src._data))
		{
		}
		~CDataContainer()
		{
			_data.~CData<ElementType, ROWS, COLUMNS>();
		}
		void operator =(const CDataContainer &right)
		{
			_data = right._data;
		}
		void operator =(CDataContainer &&right)
		{
			_data = std::move(right._data);
		}
	public:
		union
		{
			CData<ElementType, ROWS, COLUMNS> _data;
#ifdef MSVC_SWIZZLE_ASSIGN_WORKAROUND
#			define SWIZZLE_OBJECT(swizzle_seq)																	\
				CSwizzle<ElementType, ROWS, COLUMNS, CSwizzleDesc<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>, false, 1>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_1, swizzle_seq), );
			GENERATE_SWIZZLES((SWIZZLE_OBJECT))
#			define SWIZZLE_OBJECT(swizzle_seq)																	\
				CSwizzle<ElementType, ROWS, COLUMNS, CSwizzleDesc<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>, false, 2>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_2, swizzle_seq), );
			GENERATE_SWIZZLES((SWIZZLE_OBJECT))
#			define SWIZZLE_OBJECT(swizzle_seq)																	\
				CSwizzle<ElementType, ROWS, COLUMNS, CSwizzleDesc<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>, true, 1>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_1, swizzle_seq), _);
			GENERATE_SWIZZLES((SWIZZLE_OBJECT))
#			define SWIZZLE_OBJECT(swizzle_seq)																	\
				CSwizzle<ElementType, ROWS, COLUMNS, CSwizzleDesc<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>, true, 2>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_2, swizzle_seq), _);
			GENERATE_SWIZZLES((SWIZZLE_OBJECT))
#else
			GENERATE_SWIZZLES((SWIZZLE_OBJECT))
#endif
		};
	};
#else
#	define TRIVIAL_CTOR_FORWARD CDataContainerImpl() = default;
#	define NONTRIVIAL_CTOR_FORWARD CDataContainerImpl(): _data() {}
#	define DATA_CONTAINER_IMPL_SPECIALIZATION(trivialCtor)																										\
		template<typename ElementType>																															\
		class CDataContainerImpl<ElementType, ROWS, COLUMNS, trivialCtor>: public std::conditional<ROWS == 0, CSwizzle<ElementType, 0, COLUMNS>, CEmpty>::type	\
		{																																						\
		protected:																																				\
			/*forward ctors/dtor/= to _data*/																													\
			BOOST_PP_IIF(trivialCtor, TRIVIAL_CTOR_FORWARD, NONTRIVIAL_CTOR_FORWARD)																			\
			CDataContainerImpl(const CDataContainerImpl &src): _data(src._data)																					\
			{																																					\
			}																																					\
			CDataContainerImpl(CDataContainerImpl &&src): _data(std::move(src._data))																			\
			{																																					\
			}																																					\
			~CDataContainerImpl()																																\
			{																																					\
				_data.~CData<ElementType, ROWS, COLUMNS>();																										\
			}																																					\
			void operator =(const CDataContainerImpl &right)																									\
			{																																					\
				_data = right._data;																															\
			}																																					\
			void operator =(CDataContainerImpl &&right)																											\
			{																																					\
				_data = std::move(right._data);																													\
			}																																					\
		public:																																					\
			union																																				\
			{																																					\
				CData<ElementType, ROWS, COLUMNS> _data;																										\
				/*gcc does not allow class definition inside anonymous union*/																					\
				GENERATE_SWIZZLES((SWIZZLE_OBJECT))																												\
			};																																					\
		};
	DATA_CONTAINER_IMPL_SPECIALIZATION(0)
	DATA_CONTAINER_IMPL_SPECIALIZATION(1)
#	undef TRIVIAL_CTOR_FORWARD
#	undef NONTRIVIAL_CTOR_FORWARD
#	undef DATA_CONTAINER_IMPL_SPECIALIZATION

	template<typename ElementType>
	class CDataContainer<ElementType, ROWS, COLUMNS>: public CDataContainerImpl<ElementType, ROWS, COLUMNS, std::has_trivial_default_constructor<CData<ElementType, ROWS, COLUMNS>>::value>
	{
	protected:
		CDataContainer() = default;
		CDataContainer(const CDataContainer &) = default;
		CDataContainer(CDataContainer &&) = default;
		~CDataContainer() = default;
		CDataContainer &operator =(const CDataContainer &) = default;
		CDataContainer &operator =(CDataContainer &&) = default;
	};
#endif

#	undef NAMING_SET_1
#	undef NAMING_SET_2
#	undef SWIZZLE_OBJECT
#endif

#	pragma region generate typedefs
		// tuple: (C++, HLSL, GLSL)
#		define SCALAR_TYPES_MAPPINGS										\
			((bool, bool, b))												\
			((signed long, int, i))((unsigned long, uint, ui))				\
			((signed long long, long, l))((unsigned long long, ulong, ul))	\
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
	#			define GLSL_CLASS2(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(GLSL_SCALAR_TYPE(scalar_types_mapping), mat), ROWS)
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

#	if defined _MSC_VER && _MSC_VER < 1800 || defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ >= 4 && __GNUC_MINOR__ < 7))
#	error old compiler version
#	endif

#	pragma warning(push)
#	pragma warning(disable: 4003 4005/*temp for MSVC_SWIZZLE_ASSIGN_WORKAROUND*/)

#	include "C++11 stub.h"
#	include <cassert>
#	include <utility>
#	include <type_traits>
#	include <functional>
#	include <iterator>
#	include <algorithm>
#	include <initializer_list>
#	include <memory>	// temp for unique_ptr
#	include <boost\preprocessor\facilities\apply.hpp>
#	include <boost\preprocessor\iteration\iterate.hpp>
//#	include <boost\preprocessor\repetition\repeat.hpp>
#	include <boost\preprocessor\repetition\repeat_from_to.hpp>
#	include <boost\preprocessor\repetition\for.hpp>
#	include <boost\preprocessor\control\while.hpp>
#	include <boost\preprocessor\control\if.hpp>
#	include <boost\preprocessor\control\iif.hpp>
#	include <boost\preprocessor\arithmetic\inc.hpp>
#	include <boost\preprocessor\arithmetic\dec.hpp>
#	include <boost\preprocessor\arithmetic\add.hpp>
#	include <boost\preprocessor\arithmetic\sub.hpp>
#	include <boost\preprocessor\arithmetic\mul.hpp>
#	include <boost\preprocessor\arithmetic\mod.hpp>
#	include <boost\preprocessor\comparison\equal.hpp>
#	include <boost\preprocessor\comparison\not_equal.hpp>
#	include <boost\preprocessor\comparison\less.hpp>
#	include <boost\preprocessor\logical\and.hpp>
#	include <boost\preprocessor\logical\bitand.hpp>
#	include <boost\preprocessor\logical\bitor.hpp>
#	include <boost\preprocessor\logical\not.hpp>
#	include <boost\preprocessor\selection\max.hpp>
#	include <boost\preprocessor\tuple\elem.hpp>
#	include <boost\preprocessor\tuple\to_seq.hpp>
#	include <boost\preprocessor\tuple\rem.hpp>
#	include <boost\preprocessor\seq\seq.hpp>
#	include <boost\preprocessor\seq\size.hpp>
#	include <boost\preprocessor\seq\elem.hpp>
#	include <boost\preprocessor\seq\first_n.hpp>
#	include <boost\preprocessor\seq\push_back.hpp>
#	include <boost\preprocessor\seq\pop_front.hpp>
#	include <boost\preprocessor\seq\replace.hpp>
#	include <boost\preprocessor\seq\fold_left.hpp>
#	include <boost\preprocessor\seq\transform.hpp>
#	include <boost\preprocessor\seq\for_each.hpp>
#	include <boost\preprocessor\seq\cat.hpp>
#	include <boost\preprocessor\seq\to_tuple.hpp>
#	include <boost\mpl\placeholders.hpp>
#	include <boost\mpl\vector_c.hpp>
#	include <boost\mpl\range_c.hpp>
#	include <boost\mpl\at.hpp>
#	include <boost\mpl\min_max.hpp>
#	include <boost\mpl\sort.hpp>
#	include <boost\mpl\size.hpp>
#	include <boost\mpl\unique.hpp>
#	include <boost\mpl\comparison.hpp>
#	include <boost\mpl\or.hpp>
#	include <boost\mpl\bitor.hpp>
#	include <boost\mpl\shift_left.hpp>
#	include <boost\mpl\times.hpp>
#	include <boost\mpl\integral_c.hpp>
#	include <boost\mpl\iterator_range.hpp>
#	include <boost\mpl\deref.hpp>
#	include <boost\mpl\begin.hpp>
#	include <boost\mpl\advance.hpp>
#	include <boost\mpl\distance.hpp>
#	include <boost\mpl\iter_fold.hpp>
#	include <boost\mpl\find.hpp>

#	ifdef MSVC_LIMITATIONS
#		define TEMPLATE
#	else
#		define TEMPLATE template
#	endif

//#	define GET_SWIZZLE_ELEMENT(vectorDimension, idx, cv) (reinterpret_cast<cv CData<ElementType, vectorDimension> &>(*this)._data[(idx)])
//#	define GET_SWIZZLE_ELEMENT_PACKED(vectorDimension, packedSwizzle, idx, cv) (GET_SWIZZLE_ELEMENT(vectorDimension, packedSwizzle >> ((idx) << 1) & 3u, cv))

	namespace Math
	{
		namespace VectorMath
		{
#			define ARITHMETIC_OPS (+)(-)(*)(/)
#			define REL_OPS (==)(!=)(<)(<=)(>)(>=)
#			define GENERATE_OPERATORS_MACRO(r, callback, op) callback(op)
#			define GENERATE_OPERATORS(callback, ops) BOOST_PP_SEQ_FOR_EACH(GENERATE_OPERATORS_MACRO, callback, ops)

			namespace mpl = boost::mpl;

			class CEmpty {};

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

#ifdef MSVC_LIMITATIONS
			template<class DstSwizzleDesc, class SrcSwizzleDesc, bool assign>
			class DetectSwizzleWARHazardImpl
#else
			template
			<
				typename ElementType, unsigned int rows, unsigned int columns,
				class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
			>
			class DetectSwizzleWARHazard<ElementType, rows, columns, DstSwizzleDesc, ElementType, rows, columns, SrcSwizzleDesc, assign>
#endif
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
					typedef typename std::conditional<assign && DstWasAlreadyWritten::value, DstWasModified<>, DstWasAlreadyWritten>::type type;
				};
				typedef typename mpl::iter_fold<CCuttedSrcSwizzleVector, std::false_type, mpl::or_<mpl::_1, Pred<mpl::_2>>>::type Result;
			public:
				static constexpr typename Result::value_type value = Result::value;
			};

#ifdef MSVC_LIMITATIONS
			template
			<
				typename ElementType, unsigned int rows, unsigned int columns,
				class DstSwizzleDesc, class SrcSwizzleDesc, bool assign
			>
			class DetectSwizzleWARHazard<ElementType, rows, columns, DstSwizzleDesc, ElementType, rows, columns, SrcSwizzleDesc, assign> :
			public DetectSwizzleWARHazardImpl<DstSwizzleDesc, SrcSwizzleDesc, assign>
			{
			};
#endif
			
			template<typename ElementType, unsigned int dimension>
			class vector;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class matrix;

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, bool odd = false, unsigned namingSet = 1>
			class CSwizzleCommon;

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, bool odd = false, unsigned namingSet = 1>
			class CSwizzleAssign;

			// rows = 0 for vectors
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, bool odd = false, unsigned namingSet = 1, typename isWriteMaskValid = std::integral_constant<bool, SwizzleDesc::isWriteMaskValid>>
			class CSwizzle;

			template<typename ElementType, unsigned int rows, unsigned int columns, bool trivialCtor>
			class CDataContainerImpl;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CDataContainer;

			template<typename ElementType>
			class CInitListItem;

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
#ifdef MSVC_LIMITATIONS
			bool all(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::integral_constant<bool, SwizzleDesc::isWriteMaskValid>> &src);
#else
			bool all(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
#ifdef MSVC_LIMITATIONS
			bool any(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::integral_constant<bool, SwizzleDesc::isWriteMaskValid>> &src);
#else
			bool any(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
#ifdef MSVC_LIMITATIONS
			bool none(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::integral_constant<bool, SwizzleDesc::isWriteMaskValid>> &src);
#else
			bool none(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns>
			bool all(const matrix<ElementType, rows, columns> &src);

			template<typename ElementType, unsigned int rows, unsigned int columns>
			bool any(const matrix<ElementType, rows, columns> &src);

			template<typename ElementType, unsigned int rows, unsigned int columns>
			bool none(const matrix<ElementType, rows, columns> &src);

			// generic for matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CData final
			{
				friend class matrix<ElementType, rows, columns>;
				friend bool all<>(const matrix<ElementType, rows, columns> &);
				friend bool any<>(const matrix<ElementType, rows, columns> &);
				friend bool none<>(const matrix<ElementType, rows, columns> &);

				template<typename, unsigned int, unsigned int, class, bool, unsigned>
				friend class CSwizzleCommon;

				friend class CDataContainer<ElementType, rows, columns>;
				friend class CDataContainerImpl<ElementType, rows, columns, false>;
				friend class CDataContainerImpl<ElementType, rows, columns, true>;
#		ifndef MSVC_LIMITATIONS
			private:
				CData() = default;
				CData(const CData &) = default;
				CData(CData &&) = default;
				~CData() = default;
				CData &operator =(const CData &) = default;
				CData &operator =(CData &&) = default;
#		endif
			private:
#		ifdef MSVC_LIMITATIONS
				ElementType _rows[rows][columns];
#		else
				vector<ElementType, columns> _rows[rows];
#		endif
			};

			// specialization for vector
			template<typename ElementType, unsigned int dimension>
			class CData<ElementType, 0, dimension> final
			{
				friend class vector<ElementType, dimension>;

				template<typename, unsigned int, unsigned int, class, bool, unsigned>
				friend class CSwizzleCommon;

				friend class CDataContainer<ElementType, 0, dimension>;
				friend class CDataContainerImpl<ElementType, 0, dimension, false>;
				friend class CDataContainerImpl<ElementType, 0, dimension, true>;
#		ifndef MSVC_LIMITATIONS
			private:
				CData() = default;
				CData(const CData &) = default;
				CData(CData &&) = default;
				~CData() = default;
				CData &operator =(const CData &) = default;
				CData &operator =(CData &&) = default;
#		endif
			private:
				ElementType _data[dimension];
			};

			// generic vector/matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CDataContainer : public std::conditional<rows == 0, CSwizzle<ElementType, 0, columns>, CEmpty>::type
			{
			protected:
				CData<ElementType, rows, columns> _data;
#		ifndef MSVC_LIMITATIONS
				CDataContainer() = default;
				CDataContainer(const CDataContainer &) = default;
				CDataContainer(CDataContainer &&) = default;
				~CDataContainer() = default;
				CDataContainer &operator =(const CDataContainer &) = default;
				CDataContainer &operator =(CDataContainer &&) = default;
#		endif
			};

#			pragma region Flat idx accessors
			template<typename Src>
			class CFlatIdxAccessor;

			template<typename Scalar>
			class CFlatIdxAccessor
			{
				const Scalar &_scalar;
			public:
				CFlatIdxAccessor(const Scalar &scalar) noexcept : _scalar(scalar) {}
			public:
				const Scalar &operator [](unsigned idx) const noexcept
				{
					  assert(idx < dimension);
					  return _scalar;
			}
			public:
				static constexpr unsigned dimension = 1;
			};

			// assert not required for swizzle and matrix because it's 'operator []' already have assert

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			class CFlatIdxAccessor<const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet>>
			{
				typedef const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzle;
				TSwizzle &_swizzle;
			public:
				CFlatIdxAccessor(TSwizzle &swizzle) noexcept : _swizzle(swizzle) {}
			public:
				const ElementType &operator [](unsigned idx) const noexcept
				{
					  return _swizzle[idx];
			}
			public:
				static constexpr unsigned dimension = SwizzleDesc::TDimension::value;
			};

			template<typename ElementType, unsigned int dimension>
			class CFlatIdxAccessor<const vector<ElementType, dimension>>: public CFlatIdxAccessor<const CSwizzle<ElementType, 0, dimension>>
			{
			public:
				// TODO: use C++11 inheriting ctor
				CFlatIdxAccessor(const vector<ElementType, dimension> &vector) :
					CFlatIdxAccessor<const CSwizzle<ElementType, 0, dimension>>(vector) {}
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CFlatIdxAccessor<const matrix<ElementType, rows, columns>>
			{
				typedef const matrix<ElementType, rows, columns> TMatrix;
				TMatrix &_matrix;
			public:
				CFlatIdxAccessor(TMatrix &matrix) noexcept : _matrix(matrix) {}
			public:
				const ElementType &operator [](unsigned idx) const noexcept
				{
					  return _matrix[idx / columns][idx % columns];
			}
			public:
				static constexpr unsigned dimension = rows * columns;
			};

			template<typename Src>
			inline CFlatIdxAccessor<Src> CreateFlatIdxAccessor(Src &src) noexcept
			{
				return CFlatIdxAccessor<Src>(src);
			}
#			pragma endregion TODO: move to vector/matrix base class in order to hide from user

#			pragma region Initializer list
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
				CInitListItem(const CSwizzle<ItemElementType, itemRows, itemColumns, ItemSwizzleDesc> &item) noexcept:
				_getItemElement(_GetItemElement<ItemSwizzleDesc::TDimension::value>),
					_item(new vector<ElementType, ItemSwizzleDesc::TDimension::value>(item), CDeleter(_Delete<ItemSwizzleDesc::TDimension::value>)),
					_itemSize(ItemSwizzleDesc::TDimension::value)
				{
				}

				template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
				CInitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) noexcept:
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
#			pragma endregion

			// specializations for graphics vectors/matrices
#			define BOOST_PP_ITERATION_LIMITS (0, 4)
#			define BOOST_PP_FILENAME_1 "vector math.h"
#			include BOOST_PP_ITERATE()

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc = CVectorSwizzleDesc<columns>, bool odd = false, unsigned namingSet = 1>
			class CSwizzleBase
			{
				/*
						  static
							 ^
							 |
				CSwizzleBase -> CSwizzle
				*/
				typedef CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzle;
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleBase() = default;
				CSwizzleBase(const CSwizzleBase &) = default;
				~CSwizzleBase() = default;
				CSwizzleBase &operator =(const CSwizzleBase &) = default;
#endif
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
			public:
				template<typename F>
				vector<typename std::result_of<F &(ElementType)>::type, SwizzleDesc::TDimension::value> apply(F f) const;

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
#ifdef MSVC_LIMITATIONS	// workaround for lack of expression SFINAE in VC
					return apply<ElementType(ElementType)>(f);
#else
					return apply<ElementType>(f);
#endif
				}

				vector<ElementType, SwizzleDesc::TDimension::value> apply(ElementType f(const ElementType &)) const
				{
#ifdef MSVC_LIMITATIONS	// workaround for lack of expression SFINAE in VC
					return apply<ElementType(const ElementType &)>(f);
#else
					return apply<ElementType>(f);
#endif
				}
			};

			// CSwizzle inherits from this to reduce preprocessor generated code for faster compiling
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			class CSwizzleCommon: public CSwizzleBase<ElementType, rows, columns, SwizzleDesc, odd, namingSet>
			{
				typedef CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzle;
				typedef matrix<ElementType, rows, columns> Tmatrix;
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleCommon() = default;
				CSwizzleCommon(const CSwizzleCommon &) = delete;
				~CSwizzleCommon() = default;
				CSwizzleCommon &operator =(const CSwizzleCommon &) = delete;
#endif
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
			template<typename ElementType, unsigned int vectorDimension, class SwizzleDesc, bool odd, unsigned namingSet>
			class CSwizzleCommon<ElementType, 0, vectorDimension, SwizzleDesc, odd, namingSet>: public CSwizzleBase<ElementType, 0, vectorDimension, SwizzleDesc, odd, namingSet>
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
				typedef CSwizzle<ElementType, 0, vectorDimension, SwizzleDesc, odd, namingSet> TSwizzle;
				typedef vector<ElementType, vectorDimension> Tvector;
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleCommon() = default;
				CSwizzleCommon(const CSwizzleCommon &) = delete;
				~CSwizzleCommon() = default;
				CSwizzleCommon &operator =(const CSwizzleCommon &) = delete;
#endif
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
					return reinterpret_cast<const CData &>(static_cast<const TSwizzle &>(*this))._data[idx];
				}
				ElementType &operator [](unsigned int idx) noexcept
				{
					return const_cast<ElementType &>(static_cast<const CSwizzleCommon &>(*this)[idx]);
				}
			};

			/*
			CVectorSwizzleDesc<vectorDimension> required for VS 2013
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzleCommon<ElementType, 0, vectorDimension, CVectorSwizzleDesc<vectorDimension>>: public CSwizzleBase<ElementType, 0, vectorDimension>
			{
				typedef vector<ElementType, vectorDimension> Tvector;
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleCommon() = default;
				CSwizzleCommon(const CSwizzleCommon &) = default;
				~CSwizzleCommon() = default;
				CSwizzleCommon &operator =(const CSwizzleCommon &) = default;
#endif
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

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			class CSwizzleAssign: public CSwizzleCommon<ElementType, rows, columns, SwizzleDesc, odd, namingSet>
			{
				typedef CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzle;
				// TODO: remove 'public'
			public:
				using typename CSwizzleCommon<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::TOperationResult;

			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleAssign() = default;
				CSwizzleAssign(const CSwizzleAssign &) = default;
				~CSwizzleAssign() = default;
#endif

			private:
				template<typename Arg, bool WARHazard>
				class SelectAssign
				{
					typedef void (CSwizzleAssign::*const TAssign)(Arg);
				public:
#ifdef MSVC_LIMITATIONS
					static TAssign Assign;
#else
					static constexpr TAssign Assign = WARHazard ? TAssign(&CSwizzleAssign::AssignCopy) : TAssign(&CSwizzleAssign::AssignDirect);
#endif
				};

			private:
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#ifdef MSVC_LIMITATIONS
				inline void AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet, std::integral_constant<bool, RightSwizzleDesc::isWriteMaskValid>> &right);
#else
				inline void AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right);
#endif

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#ifdef MSVC_LIMITATIONS
				inline void AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet, std::integral_constant<bool, RightSwizzleDesc::isWriteMaskValid>> &right);
#else
				inline void AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right);
#endif

			public:
#ifndef MSVC_LIMITATIONS
#if defined MSVC_LIMITATIONS || defined __GNUC__
				inline TOperationResult &operator =(const CSwizzleAssign &right)
#else
				inline TOperationResult &operator =(const CSwizzleAssign &right) &
#endif
				{
					return operator =<false>(static_cast<const TSwizzle &>(right));
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				inline TOperationResult &&operator =(const CSwizzleAssign &right) &&
				{
					return operator =<false>(static_cast<const TSwizzle &>(right));
				}
#endif
#endif

				template<bool WARHazard, typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
				TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
				{
					constexpr auto Assign = SelectAssign<const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &, WARHazard>::Assign;
					(this->*Assign)(right);
					return *this;
				}

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
#else
				TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &
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

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
				TOperationResult &&operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &&
				{
					return std::move(operator =<false>(right));
				}
#endif

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right)
#else
				TOperationResult &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right) &
#endif
				{
					return operator =<false>(right);
				}

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				TOperationResult &operator =(const vector<RightElementType, rightDimension> &right)
#else
				TOperationResult &operator =(const vector<RightElementType, rightDimension> &right) &
#endif
				{
					return *this = static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightDimension>
				TOperationResult &&operator =(const vector<RightElementType, rightDimension> &right) &&
				{
					return std::move(*this) = static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);
				}
#endif

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				TOperationResult &operator =(const vector<RightElementType, rightDimension> &&right)
#else
				TOperationResult &operator =(const vector<RightElementType, rightDimension> &&right) &
#endif
				{
					return *this = static_cast<const CSwizzle<RightElementType, 0, rightDimension> &&>(right);
				}

				template<typename RightElementType>
				inline TOperationResult &operator =(const RightElementType &scalar);

				inline TOperationResult &operator =(std::initializer_list<CInitListItem<ElementType>> initList);
			protected:
				template<unsigned idx>
				inline void _Init();
				template<unsigned startIdx = 0u, typename TCurSrc, typename ...TRestSrc>
				inline void _Init(const TCurSrc &curSrc, const TRestSrc &...restSrc);
			public:
				using CSwizzleCommon<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::operator [];
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<unsigned idx>
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::_Init()
			{
				static constexpr auto dimension = SwizzleDesc::TDimension::value;
				static_assert(idx >= dimension, "too few src elements");
				static_assert(idx <= dimension, "too many src elements");
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<unsigned startIdx, typename TCurSrc, typename ...TRestSrc>
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::_Init(const TCurSrc &curSrc, const TRestSrc &...restSrc)
			{
				const auto src_accesssor = CreateFlatIdxAccessor(curSrc);
				for (unsigned i = 0; i < src_accesssor.dimension; i++)
				{
					operator [](i + startIdx).~ElementType();
					new(&operator [](i + startIdx)) ElementType(src_accesssor[i]);
				}
				_Init<startIdx + src_accesssor.dimension>(restSrc...);
			}

#ifdef MSVC_LIMITATIONS
			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<typename Arg, bool WARHazard>
			typename CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::template SelectAssign<Arg, WARHazard>::TAssign CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::SelectAssign<Arg, WARHazard>::Assign = WARHazard ? CSwizzleAssign::SelectAssign<Arg, WARHazard>::TAssign(&CSwizzleAssign::AssignCopy) : CSwizzleAssign::SelectAssign<Arg, WARHazard>::TAssign(&CSwizzleAssign::AssignDirect);
#endif

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#ifdef MSVC_LIMITATIONS
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet, std::integral_constant<bool, RightSwizzleDesc::isWriteMaskValid>> &right)
#else
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::AssignDirect(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
#endif
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
					(const CDataContainer<ElementType, rows, columns> *)static_cast<const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> *>(this) !=
					(const void *)(const CDataContainer<RightElementType, rightRows, rightColumns> *)&right));
				static_assert(SwizzleDesc::TDimension::value <= RightSwizzleDesc::TDimension::value, "operator =: too small src dimension");
				for (unsigned idx = 0; idx < SwizzleDesc::TDimension::value; idx++)
					(*this)[idx] = right[idx];
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#ifdef MSVC_LIMITATIONS
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet, std::integral_constant<bool, RightSwizzleDesc::isWriteMaskValid>> &right)
#else
			inline void CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::AssignCopy(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
#endif
			{
				// make copy and call AssignDirect()
				AssignDirect(vector<RightElementType, RightSwizzleDesc::TDimension::value>(right));
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<typename RightElementType>
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::operator =(const RightElementType &scalar) -> TOperationResult &
			{
				for (unsigned idx = 0; idx < SwizzleDesc::TDimension::value; idx++)
					(*this)[idx] = scalar;
				return *this;
			}

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			inline auto CSwizzleAssign<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> TOperationResult &
			{
				unsigned dst_idx = 0;
				for (const auto &item: initList)
					for (unsigned item_element_idx = 0; item_element_idx < item.GetItemSize(); item_element_idx++)
						(*this)[dst_idx++] = item[item_element_idx];
				assert(dst_idx == columns);
				return *this;
			}

			// this specialization used as base class for CDataContainer to eliminate need for various overloads
			/*
			CVectorSwizzleDesc<vectorDimension> required for VS 2013
			TODO: try with newer version
			*/
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzle<ElementType, 0, vectorDimension, CVectorSwizzleDesc<vectorDimension>, false, 1, std::true_type>: public CSwizzleAssign<ElementType, 0, vectorDimension>
			{
			protected:
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = default;
				~CSwizzle() = default;
				CSwizzle &operator =(const CSwizzle &) = default;
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			class CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::false_type>: public CSwizzleCommon<ElementType, rows, columns, SwizzleDesc>
			{
			public:
#ifndef MSVC_LIMITATIONS
				CSwizzle &operator =(const CSwizzle &) = delete;
#endif
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = delete;
				~CSwizzle() = default;
#endif
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			class CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::true_type>: public CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>
			{
			public:
#ifdef MSVC_LIMITATIONS
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
#else
				CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &
#endif
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(right);
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
				CSwizzle &&operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &&
				{
					return std::move(*this).CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(right);
				}
#endif

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right)
#else
				CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right) &
#endif
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(std::move(right));
				}

				template<typename RightElementType>
				CSwizzle &operator =(const RightElementType &scalar)
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(scalar);
				}

				CSwizzle &operator =(std::initializer_list<CInitListItem<ElementType>> initList)
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(initList);
				}

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				CSwizzle &operator =(const vector<RightElementType, rightDimension> &right)
#else
				CSwizzle &operator =(const vector<RightElementType, rightDimension> &right) &
#endif
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(right);
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightDimension>
				CSwizzle &&operator =(const vector<RightElementType, rightDimension> &right) &&
				{
					return std::move(*this).CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(right);
				}
#endif

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				CSwizzle &operator =(const vector<RightElementType, rightDimension> &&right)
#else
				CSwizzle &operator =(const vector<RightElementType, rightDimension> &&right) &
#endif
				{
					return CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =(std::move(right));
				}
#else
				CSwizzle &operator =(const CSwizzle &) = default;
				using CSwizzleAssign<ElementType, rows, columns, SwizzleDesc>::operator =;
#endif
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = delete;
				~CSwizzle() = default;
#endif
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd = false, unsigned namingSet = 1>
			class CSwizzleIteratorImpl: public std::iterator<std::forward_iterator_tag, const ElementType>
			{
				const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &_swizzle;
				unsigned _i;
			protected:
				CSwizzleIteratorImpl(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &swizzle, unsigned i):
				_swizzle(swizzle), _i(i) {}
				~CSwizzleIteratorImpl() = default;
				// required by stl => public
			public:
				typename std::conditional
				<
					sizeof(typename CSwizzleIteratorImpl::value_type) <= sizeof(void *),
					typename CSwizzleIteratorImpl::value_type,
					typename CSwizzleIteratorImpl::reference
				>::type operator *() const
				{
					return _swizzle[_i];
				}
				typename CSwizzleIteratorImpl::pointer operator ->() const
				{
					return &_swizzle[_i];
				}
				bool operator ==(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc, odd, namingSet> src) const noexcept
				{
					assert(&_swizzle == &src._swizzle);
					return _i == src._i;
				}
				bool operator !=(CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc, odd, namingSet> src) const noexcept
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

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd = false, unsigned namingSet = 1>
			class CSwizzleIterator: public CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc, odd, namingSet>
			{
				friend bool all<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);
				friend bool any<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);
				friend bool none<>(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src);

				// use C++11 inheriting ctor
				CSwizzleIterator(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &swizzle, unsigned i):
				CSwizzleIteratorImpl<ElementType, rows, columns, SwizzleDesc, odd, namingSet>(swizzle, i) {}
				// copy ctor required by stl => public
				~CSwizzleIterator() = default;
			};

#			pragma region generate operators
				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
				inline const typename CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::TOperationResult &operator +(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
				inline typename CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::TOperationResult &operator +(CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
				inline vector<ElementType, SwizzleDesc::TDimension::value> operator -(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src)
				{
					vector<ElementType, SwizzleDesc::TDimension::value> result;
					for (typename SwizzleDesc::TDimension::value_type i = 0; i < SwizzleDesc::TDimension::value; i++)
						result[i] = -src[i];
					return result;
				}

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						bool WARHazard,																																	\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						if (WARHazard)																																	\
							return operator op##=<false>(left, vector<RightElementType, RightSwizzleDesc::TDimension::value>(right));									\
						else																																			\
						{																																				\
							assert((																																	\
							!DetectSwizzleWARHazard																														\
								<																																		\
									LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																			\
									RightElementType, rightRows, rightColumns, RightSwizzleDesc,																		\
									false																																\
								>::value ||																																\
								(const CDataContainer<LeftElementType, leftRows, leftColumns> *)(&left) !=																\
								(const void *)(const CDataContainer<RightElementType, rightRows, rightColumns> *)&right));												\
							static_assert(LeftSwizzleDesc::isWriteMaskValid, "operator "#op"=: invalid write mask");													\
							static_assert(LeftSwizzleDesc::TDimension::value <= RightSwizzleDesc::TDimension::value, "operator "#op"=: too small src dimension");		\
							for (typename LeftSwizzleDesc::TDimension::value_type i = 0; i < LeftSwizzleDesc::TDimension::value; i++)									\
								left[i] op##= right[i];																													\
							return left;																																\
						}																																				\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						static constexpr auto WARHazard = DetectSwizzleWARHazard																						\
						<																																				\
							LeftElementType, leftRows, leftColumns, LeftSwizzleDesc,																					\
							RightElementType, rightRows, rightColumns, RightSwizzleDesc,																				\
							false																																		\
						>::value;																																		\
						return operator op##=<WARHazard>(left, right);																									\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &&operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &&left,													\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						return std::move(operator op##=<false>(left, right));																							\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right)										\
					{																																					\
						return operator op##=<false>(left, right);																										\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const vector<RightElementType, rightDimension> &right)																								\
					{																																					\
						return left op##= static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);													\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &&operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &&left,													\
					const vector<RightElementType, rightDimension> &right)																								\
					{																																					\
						return std::move(left) op##= static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);											\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

				// VS 2013 call 'CSwizzle & op= const CSwizzle &' instead of 'CSwizzle & op= const CSwizzle &&' for variant with static_cast<const CSwizzle &&>
#ifdef MSVC_LIMITATIONS
#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const vector<RightElementType, rightDimension> &&right)																								\
					{																																					\
						return operator op##=<false>(left, right);																										\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION
#else
#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					const vector<RightElementType, rightDimension> &&right)																								\
					{																																					\
						return left op##= static_cast<const CSwizzle<RightElementType, 0, rightDimension> &&>(right);													\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION
#endif

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType																														\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,													\
					RightElementType right)																																\
					{																																					\
						static_assert(LeftSwizzleDesc::isWriteMaskValid, "operator "#op"=: invalid write mask");														\
						for (typename LeftSwizzleDesc::TDimension::value_type i = 0; i < LeftSwizzleDesc::TDimension::value; i++)										\
							left[i] op##= right;																														\
						return left;																																	\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType																														\
					>																																					\
					inline typename CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet>::TOperationResult &operator op##=(		\
					CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &&left,													\
					RightElementType right)																																\
					{																																					\
						return left op##= right;																														\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#ifdef MSVC_LIMITATIONS
#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline vector<LeftElementType, leftDimension> &&operator op##=(																						\
					vector<LeftElementType, leftDimension> &&left,																										\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right)										\
					{																																					\
						return static_cast<CSwizzle<LeftElementType, 0, leftDimension> &&>(left) op##= right;															\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline vector<LeftElementType, leftDimension> &&operator op##=(																						\
					vector<LeftElementType, leftDimension> &&left,																										\
					const vector<RightElementType, rightDimension> &&right)																								\
					{																																					\
						return static_cast<CSwizzle<LeftElementType, 0, leftDimension> &&>(left) op##= right;															\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION
#endif

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline vector																																		\
					<																																					\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																	\
						mpl::min																																		\
						<																																				\
							typename LeftSwizzleDesc::TDimension,																										\
							typename RightSwizzleDesc::TDimension																										\
						>::type::value																																	\
					> operator op(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						vector																																			\
						<																																				\
							decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																\
							mpl::min																																	\
							<																																			\
								typename LeftSwizzleDesc::TDimension,																									\
								typename RightSwizzleDesc::TDimension																									\
							>::type::value																																\
						> result(left);																																	\
						return std::move(result) op##= right;																											\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline vector																																		\
					<																																					\
						bool,																																			\
						mpl::min																																		\
						<																																				\
							typename LeftSwizzleDesc::TDimension,																										\
							typename RightSwizzleDesc::TDimension																										\
						>::type::value																																	\
					> operator op(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						constexpr unsigned int dimension = mpl::min																										\
						<																																				\
							typename LeftSwizzleDesc::TDimension,																										\
							typename RightSwizzleDesc::TDimension																										\
						>::type::value;																																	\
						vector<bool, dimension> result;																													\
						for (unsigned i = 0; i < dimension; i++)																										\
							result[i] = left[i] op right[i];																											\
						return result;																																	\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType																														\
					>																																					\
					inline vector																																		\
					<																																					\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																	\
						LeftSwizzleDesc::TDimension::value																												\
					> operator op(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const RightElementType &right)																														\
					{																																					\
						vector																																			\
						<																																				\
							decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																\
							LeftSwizzleDesc::TDimension::value																											\
						> result(left);																																	\
						return std::move(result) op##= right;																											\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType																														\
					>																																					\
					inline vector																																		\
					<																																					\
						bool,																																			\
						LeftSwizzleDesc::TDimension::value																												\
					> operator op(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const RightElementType &right)																														\
					{																																					\
						constexpr unsigned int dimension = LeftSwizzleDesc::TDimension::value;																			\
						vector<bool, dimension> result;																													\
						for (unsigned i = 0; i < dimension; i++)																										\
							result[i] = left[i] op right;																												\
						return result;																																	\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType,																														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline vector																																		\
					<																																					\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																	\
						RightSwizzleDesc::TDimension::value																												\
					> operator op(																																		\
					const LeftElementType &left,																														\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						vector																																			\
						<																																				\
							decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),																\
							RightSwizzleDesc::TDimension::value																											\
						> result(left);																																	\
						return std::move(result) op##= right;																											\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType,																														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline vector																																		\
					<																																					\
						bool,																																			\
						RightSwizzleDesc::TDimension::value																												\
					> operator op(																																		\
					const LeftElementType &left,																														\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					{																																					\
						constexpr unsigned int dimension = RightSwizzleDesc::TDimension::value;																			\
						vector<bool, dimension> result;																													\
						for (unsigned i = 0; i < dimension; i++)																										\
							result[i] = left op right[i];																												\
						return result;																																	\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline auto operator op(																															\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const vector<RightElementType, rightDimension> &right)																								\
					-> decltype(left op static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right))														\
					{																																					\
						return left op static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);														\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline auto operator op(																															\
					const vector<LeftElementType, leftDimension> &left,																									\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					-> decltype(static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left) op right)														\
					{																																					\
						return static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left) op right;															\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline auto operator op(																															\
					const vector<LeftElementType, leftDimension> &left,																									\
					const vector<RightElementType, rightDimension> &right) -> decltype(																					\
					static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left) op																			\
					static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right))																			\
					{																																					\
						return																																			\
							static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left) op																	\
							static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right);																	\
					};
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION
#			pragma endregion

			template<typename ElementType_, unsigned int dimension_>
			class vector: public CDataContainer<ElementType_, 0, dimension_>
			{
				static_assert(dimension_ > 0, "vector dimension should be positive");
				//using CDataContainer<ElementType_, 0, dimension_>::_data;
			public:
				typedef ElementType_ ElementType;
				static constexpr unsigned int dimension = dimension_;

				vector() = default;

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, bool srcOdd, unsigned srcNamingSet>
#ifdef MSVC_LIMITATIONS
				vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet, std::integral_constant<bool, SrcSwizzleDesc::isWriteMaskValid>> &src);
#else
				vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet> &src);
#endif

				template<typename SrcElementType, unsigned int srcDimenstion>
				vector(const vector<SrcElementType, srcDimenstion> &src);

				template<typename SrcElementType>
				vector(const SrcElementType &scalar);

				template<typename First, typename ...Rest>
				vector(const First &first, const Rest &...rest);

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				vector(const matrix<SrcElementType, srcRows, srcColumns> &src);

				vector(std::initializer_list<CInitListItem<ElementType>> initList);

				//template<typename TIterator>
				//explicit vector(TIterator src);

				template<typename SrcElementType>
				vector(const SrcElementType (&src)[dimension]);

#ifdef MSVC_LIMITATIONS
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
#else
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &
#endif
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(right);
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
				vector &&operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right) &&
				{
					return std::move(*this).CSwizzleAssign<ElementType, 0, dimension>::operator =(right);
				}
#endif

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right)
#else
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &&right) &
#endif
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(std::move(right));
				}

				template<typename RightElementType>
				vector &operator =(const RightElementType &scalar)
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(scalar);
				}

				vector &operator =(std::initializer_list<CInitListItem<ElementType>> initList)
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(initList);
				}

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				vector &operator =(const vector<RightElementType, rightDimension> &right)
#else
				vector &operator =(const vector<RightElementType, rightDimension> &right) &
#endif
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(right);
				}

#if !(defined MSVC_LIMITATIONS || defined __GNUC__)
				template<typename RightElementType, unsigned int rightDimension>
				vector &&operator =(const vector<RightElementType, rightDimension> &right) &&
				{
					return std::move(*this).CSwizzleAssign<ElementType, 0, dimension>::operator =(right);
				}
#endif

				template<typename RightElementType, unsigned int rightDimension>
#if defined MSVC_LIMITATIONS || defined __GNUC__
				vector &operator =(const vector<RightElementType, rightDimension> &&right)
#else
				vector &operator =(const vector<RightElementType, rightDimension> &&right) &
#endif
				{
					return CSwizzleAssign<ElementType, 0, dimension>::operator =(std::move(right));
				}
#else
				using CSwizzleAssign<ElementType, 0, dimension>::operator =;
#endif

				const ElementType &operator [](unsigned int idx) const noexcept;

				ElementType &operator [](unsigned int idx) noexcept;
			private:
				using CSwizzleAssign<ElementType, 0, dimension>::_Init;
			};

			template<typename ElementType_, unsigned int rows_, unsigned int columns_>
			class matrix: public CDataContainer<ElementType_, rows_, columns_>
			{
				static_assert(rows_ > 0, "matrix should contain at leat 1 row");
				static_assert(columns_ > 0, "matrix should contain at leat 1 column");
				friend bool all<>(const matrix<ElementType_, rows_, columns_> &);
				friend bool any<>(const matrix<ElementType_, rows_, columns_> &);
				friend bool none<>(const matrix<ElementType_, rows_, columns_> &);
				typedef vector<ElementType_, columns_> TRow;
			public:
				typedef ElementType_ ElementType;
				static constexpr unsigned int rows = rows_, columns = columns_;

				matrix() = default;

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				matrix(const matrix<SrcElementType, srcRows, srcColumns> &src);

				template<typename SrcElementType>
				matrix(const SrcElementType &scalar);

				template<typename First, typename ...Rest>
				matrix(const First &first, const Rest &...rest);

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, bool srcOdd, unsigned srcNamingSet>
#ifdef MSVC_LIMITATIONS
				matrix(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet, std::integral_constant<bool, SrcSwizzleDesc::isWriteMaskValid>> &src);
#else
				matrix(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet> &src);
#endif

				template<typename SrcElementType, unsigned int srcDimenstion>
				matrix(const vector<SrcElementType, srcDimenstion> &src);

				matrix(std::initializer_list<CInitListItem<ElementType>> initList);

				//template<typename TIterator>
				//explicit matrix(TIterator src);

				template<typename SrcElementType>
				matrix(const SrcElementType (&src)[rows][columns]);

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				matrix &operator =(const matrix<RightElementType, rightRows, rightColumns> &right);

				template<typename SrcElementType>
				matrix &operator =(const SrcElementType &scalar);

				matrix &operator =(std::initializer_list<CInitListItem<ElementType>> initList);

				const matrix &operator +() const noexcept
				{
					return *this;
				}

				matrix &operator +() noexcept
				{
					return *this;
				}

				matrix operator -() const;

#				define OPERATOR_DECLARATION(op)																\
					template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>	\
					matrix &operator op##=(const matrix<RightElementType, rightRows, rightColumns> &right);
				GENERATE_OPERATORS(OPERATOR_DECLARATION, ARITHMETIC_OPS)
#				undef OPERATOR_DECLARATION

#				define OPERATOR_DECLARATION(op)						\
					template<typename RightElementType>				\
					matrix &operator op##=(RightElementType right);
				GENERATE_OPERATORS(OPERATOR_DECLARATION, ARITHMETIC_OPS)
#				undef OPERATOR_DECLARATION

				const TRow &operator [](unsigned int idx) const noexcept;

				TRow &operator [](unsigned int idx) noexcept;
			public:
				template<typename F>
				matrix<typename std::result_of<F &(ElementType)>::type, rows, columns> apply(F f) const;

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
#ifdef MSVC_LIMITATIONS	// workaround for lack of expression SFINAE in VC
					return apply<ElementType(ElementType)>(f);
#else
					return apply<ElementType>(f);
#endif
				}

				matrix<ElementType, rows, columns> apply(ElementType f(const ElementType &)) const
				{
#ifdef MSVC_LIMITATIONS	// workaround for lack of expression SFINAE in VC
					return apply<ElementType(const ElementType &)>(f);
#else
					return apply<ElementType>(f);
#endif
				}
			private:
				template<unsigned idx>
				inline void _Init();
				template<unsigned startIdx = 0u, typename TCurSrc, typename ...TRestSrc>
				inline void _Init(const TCurSrc &curSrc, const TRestSrc &...restSrc);
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
			template<typename F>
			inline vector<typename std::result_of<F &(ElementType)>::type, SwizzleDesc::TDimension::value>
			CSwizzleBase<ElementType, rows, columns, SwizzleDesc, odd, namingSet>::apply(F f) const
			{
				constexpr unsigned int dimension = SwizzleDesc::TDimension::value;
				vector<typename std::result_of<F &(ElementType)>::type, dimension> result;
				for (unsigned i = 0; i < dimension; i++)
					result[i] = f(static_cast<const TSwizzle &>(*this)[i]);
				return result;
			}

#			pragma region vector impl
				template<typename ElementType, unsigned int dimension>
				inline const ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) const noexcept
				{
					assert(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}

				template<typename ElementType, unsigned int dimension>
				inline ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) noexcept
				{
					assert(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, bool srcOdd, unsigned srcNamingSet>
#ifdef MSVC_LIMITATIONS
				inline vector<ElementType, dimension>::vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet, std::integral_constant<bool, SrcSwizzleDesc::isWriteMaskValid>> &src)
#else
				inline vector<ElementType, dimension>::vector(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet> &src)
#endif
				{
					static_assert(dimension <= SrcSwizzleDesc::TDimension::value, "\"copy\" ctor: too small src dimension");
					for (unsigned i = 0; i < dimension; i++)
					{
						operator [](i).~ElementType();
						new(&operator [](i)) ElementType(src[i]);
					}
				}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType, unsigned int srcDimenstion>
				inline vector<ElementType, dimension>::vector(const vector<SrcElementType, srcDimenstion> &src) :
					vector(static_cast<const CSwizzle<SrcElementType, 0, srcDimenstion> &>(src))
				{
				}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType>
				inline vector<ElementType, dimension>::vector(const SrcElementType &scalar)
				{
					std::fill_n(CDataContainer<ElementType, 0, dimension>::_data._data, dimension, scalar);
				}

				template<typename ElementType, unsigned int dimension>
				template<typename First, typename ...Rest>
				vector<ElementType, dimension>::vector(const First &first, const Rest &...rest)
				{
					_Init(first, rest...);
				}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				vector<ElementType, dimension>::vector(const matrix<SrcElementType, srcRows, srcColumns> &src)
				{
					_Init(src);
				}

				//template<typename ElementType, unsigned int dimension>
				//template<typename TIterator>
				//inline vector<ElementType, dimension>::vector(TIterator src)
				//{
				//	std::copy_n(src, dimension, CDataContainer<ElementType, 0, dimension>::_data._data);
				//}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType>
				inline vector<ElementType, dimension>::vector(const SrcElementType (&src)[dimension])
				{
					std::copy_n(src, dimension, CDataContainer<ElementType, 0, dimension>::_data._data);
				}

				template<typename ElementType, unsigned int dimension>
				inline vector<ElementType, dimension>::vector(std::initializer_list<CInitListItem<ElementType>> initList)
				{
					operator =(initList);
				}
#			pragma endregion

#			pragma region matrix impl
				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const noexcept -> const typename matrix::TRow &
				{
					assert(idx < rows);
#				ifdef MSVC_LIMITATIONS
					return reinterpret_cast<const TRow &>(CDataContainer<ElementType, rows, columns>::_data._rows[idx]);
#				else
					return CDataContainer<ElementType, rows, columns>::_data._rows[idx];
#				endif
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) noexcept -> typename matrix::TRow &
				{
					assert(idx < rows);
#				ifdef MSVC_LIMITATIONS
					return reinterpret_cast<TRow &>(CDataContainer<ElementType, rows, columns>::_data._rows[idx]);
#				else
					return CDataContainer<ElementType, rows, columns>::_data._rows[idx];
#				endif
				}

				/*
				note:
				it is impossible to init array in ctor's init list
				therefore default ctor for each array element (vector row in our case) called before ctor body
				it necessitate to call dtor before row ctor
				*/

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				inline matrix<ElementType, rows, columns>::matrix(const matrix<SrcElementType, srcRows, srcColumns> &src)
				{
					static_assert(rows <= srcRows, "\"copy\" ctor: too few rows in src");
					static_assert(columns <= srcColumns, "\"copy\" ctor: too few columns in src");
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					{
						auto curRow = &operator [](rowIdx);
						curRow->~TRow();
						new(curRow) TRow(src[rowIdx]);
					}
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType>
				inline matrix<ElementType, rows, columns>::matrix(const SrcElementType &scalar)
				{
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					{
						auto curRow = &operator [](rowIdx);
						curRow->~TRow();
						new(curRow) TRow(scalar);
					}
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<unsigned idx>
				inline void matrix<ElementType, rows, columns>::_Init()
				{
					constexpr auto dimension = rows * columns;
					static_assert(idx >= dimension, "too few src elements");
					static_assert(idx <= dimension, "too many src elements");
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<unsigned startIdx, typename TCurSrc, typename ...TRestSrc>
				inline void matrix<ElementType, rows, columns>::_Init(const TCurSrc &curSrc, const TRestSrc &...restSrc)
				{
					const auto src_accesssor = CreateFlatIdxAccessor(curSrc);
					for (unsigned i = 0; i < src_accesssor.dimension; i++)
					{
						const auto r = (i + startIdx) / columns, c = (i + startIdx) % columns;
						operator [](r).operator [](c).~ElementType();
						new(&operator [](r).operator [](c)) ElementType(src_accesssor[i]);
					}
					_Init<startIdx + src_accesssor.dimension>(restSrc...);
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename First, typename ...Rest>
				matrix<ElementType, rows, columns>::matrix(const First &first, const Rest &...rest)
				{
					_Init(first, rest...);
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, class SrcSwizzleDesc, bool srcOdd, unsigned srcNamingSet>
#ifdef MSVC_LIMITATIONS
				matrix<ElementType, rows, columns>::matrix(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet, std::integral_constant<bool, SrcSwizzleDesc::isWriteMaskValid>> &src)
#else
				matrix<ElementType, rows, columns>::matrix(const CSwizzle<SrcElementType, srcRows, srcColumns, SrcSwizzleDesc, srcOdd, srcNamingSet> &src)
#endif
				{
					_Init(src);
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType, unsigned int srcDimenstion>
				matrix<ElementType, rows, columns>::matrix(const vector<SrcElementType, srcDimenstion> &src)
				{
					_Init(src);
				}

				//template<typename ElementType, unsigned int rows, unsigned int columns>
				//template<typename TIterator>
				//inline matrix<ElementType, rows, columns>::matrix(TIterator src)
				//{
				//	for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++, src += columns)
				//	{
				//		auto curRow = &operator [](rowIdx);
				//		curRow->~TRow();
				//		new(curRow) TRow(src);
				//	}
				//}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType>
				inline matrix<ElementType, rows, columns>::matrix(const SrcElementType (&src)[rows][columns])
				{
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					{
						auto curRow = &operator [](rowIdx);
						curRow->~TRow();
						new(curRow) TRow(src[rowIdx]);
					}
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> matrix &
				{
					unsigned dst_idx = 0;
					for (const auto &item: initList)
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
				inline auto matrix<ElementType, rows, columns>::operator =(const matrix<RightElementType, rightRows, rightColumns> &right) -> matrix &
				{
					static_assert(rows <= rightRows, "operator =: too few rows in src");
					static_assert(columns <= rightColumns, "operator =: too few columns in src");
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
						operator [](rowIdx) = right[rowIdx];
					return *this;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename SrcElementType>
				inline auto matrix<ElementType, rows, columns>::operator =(const SrcElementType &scalar) -> matrix &
				{
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
						operator [](rowIdx) = scalar;
					return *this;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator -() const -> matrix
				{
					matrix result;
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
						result[rowIdx] = -operator [](rowIdx);
					return result;
				}

#				define OPERATOR_DEFINITION(op)																													\
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
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																			\
					template<typename ElementType, unsigned int rows, unsigned int columns>								\
					template<typename RightElementType>																	\
					inline auto matrix<ElementType, rows, columns>::operator op##=(RightElementType right) -> matrix &	\
					{																									\
						for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)												\
							operator [](rowIdx) op##= right;															\
						return *this;																					\
					}
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																						\
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
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																									\
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
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																\
					template																				\
					<																						\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,			\
						typename RightElementType															\
					>																						\
					inline matrix																			\
					<																						\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),		\
						leftRows, leftColumns																\
					> operator op(																			\
					const matrix<LeftElementType, leftRows, leftColumns> &left,								\
					const RightElementType &right)															\
					{																						\
						matrix																				\
						<																					\
							decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),	\
							leftRows, leftColumns															\
						>																					\
						result(left);																		\
						return result op##= right;															\
					}
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)														\
					template																		\
					<																				\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,	\
						typename RightElementType													\
					>																				\
					inline matrix																	\
					<																				\
						bool,																		\
						leftRows, leftColumns														\
					> operator op(																	\
					const matrix<LeftElementType, leftRows, leftColumns> &left,						\
					const RightElementType &right)													\
					{																				\
						matrix<bool, leftRows, leftColumns> result;									\
						for (unsigned i = 0; i < leftRows; i++)										\
							result[i] = left[i] op right;											\
						return result;																\
					}
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)																\
					template																				\
					<																						\
						typename LeftElementType,															\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns		\
					>																						\
					inline matrix																			\
					<																						\
						decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),		\
						rightRows, rightColumns																\
					> operator op(																			\
					const LeftElementType &left,															\
					const matrix<RightElementType, rightRows, rightColumns> &right)							\
					{																						\
						matrix																				\
						<																					\
							decltype(std::declval<LeftElementType>() op std::declval<RightElementType>()),	\
							rightRows, rightColumns															\
						>																					\
						result(left);																		\
						return result op##= right;															\
					}
				GENERATE_OPERATORS(OPERATOR_DEFINITION, ARITHMETIC_OPS)
#				undef OPERATOR_DEFINITION

#				define OPERATOR_DEFINITION(op)															\
					template																			\
					<																					\
						typename LeftElementType,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns	\
					>																					\
					inline matrix																		\
					<																					\
						bool,																			\
						rightRows, rightColumns															\
					> operator op(																		\
					const LeftElementType &left,														\
					const matrix<RightElementType, rightRows, rightColumns> &right)						\
					{																					\
						matrix<bool, rightRows, rightColumns> result;									\
						for (unsigned i = 0; i < rightRows; i++)										\
							result[i] = left op right[i];												\
						return result;																	\
					}
				GENERATE_OPERATORS(OPERATOR_DEFINITION, REL_OPS)
#				undef OPERATOR_DEFINITION

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename F>
				inline auto matrix<ElementType, rows, columns>::apply(F f) const -> matrix<typename std::result_of<F &(ElementType)>::type, rows, columns>
				{
					matrix<typename std::result_of<F &(ElementType)>::type, rows, columns> result;
					for (unsigned i = 0; i < rows; i++)
						result[i] = (*this)[i].apply(f);
					return result;
				}
#			pragma endregion

#			pragma region min/max functions
				// std::min/max requires explicit template param if used for different types => provide scalar version
#				define FUNCTION_DEFINITION(f)																			\
					template<typename LeftElementType, typename RightElementType>										\
					inline auto f(const LeftElementType &left, const RightElementType &right) -> decltype(left - right)	\
					{																									\
						return std::f<decltype(left - right)>(left, right);												\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline auto f(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					-> decltype(left - right)																															\
					{																																					\
						typedef decltype(left - right) TResult;																											\
						TResult result;																																	\
						for (unsigned i = 0; i < TResult::dimension; i++)																								\
							result[i] = std::f<typename TResult::ElementType>(left[i], right[i]);																		\
						return result;																																	\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType																														\
					>																																					\
					inline auto f(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const RightElementType &right)																														\
					-> decltype(left - right)																															\
					{																																					\
						typedef decltype(left - right) TResult;																											\
						TResult result;																																	\
						for (unsigned i = 0; i < TResult::dimension; i++)																								\
							result[i] = std::f<typename TResult::ElementType>(left[i], right);																			\
						return result;																																	\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType,																														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline auto f(																																		\
					const LeftElementType &left,																														\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					-> decltype(left - right)																															\
					{																																					\
						typedef decltype(left - right) TResult;																											\
						TResult result;																																	\
						for (unsigned i = 0; i < TResult::dimension; i++)																								\
							result[i] = std::f<typename TResult::ElementType>(left, right[i]);																			\
						return result;																																	\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,			\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline auto f(																																		\
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,												\
					const vector<RightElementType, rightDimension> &right)																								\
					-> decltype(VectorMath::f(left, static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right)))											\
					{																																					\
						return VectorMath::f(left, static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right));											\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet	\
					>																																					\
					inline auto f(																																		\
					const vector<LeftElementType, leftDimension> &left,																									\
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)										\
					-> decltype(VectorMath::f(static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left), right))											\
					{																																					\
						return VectorMath::f(static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left), right);											\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline auto f(																																		\
					const vector<LeftElementType, leftDimension> &left,																									\
					const vector<RightElementType, rightDimension> &right) -> decltype(VectorMath::f(																	\
					static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left),																				\
					static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right)))																			\
					{																																					\
						return VectorMath::f(																															\
							static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left),																		\
							static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right));																	\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType, unsigned int leftDimension,																							\
						typename RightElementType																														\
					>																																					\
					inline auto f(																																		\
					const vector<LeftElementType, leftDimension> &left,																									\
					const RightElementType &right)																														\
					-> decltype(VectorMath::f(static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left), right))											\
					{																																					\
						return VectorMath::f(static_cast<const CSwizzle<LeftElementType, 0, leftDimension> &>(left), right);											\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)																															\
					template																																			\
					<																																					\
						typename LeftElementType,																														\
						typename RightElementType, unsigned int rightDimension																							\
					>																																					\
					inline auto f(																																		\
					const LeftElementType &left,																														\
					const vector<RightElementType, rightDimension> &right)																								\
					-> decltype(VectorMath::f(left, static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right)))											\
					{																																					\
						return VectorMath::f(left, static_cast<const CSwizzle<RightElementType, 0, rightDimension> &>(right));											\
					};
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)															\
					template																			\
					<																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,		\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns	\
					>																					\
					auto f(																				\
					const matrix<LeftElementType, leftRows, leftColumns> &left,							\
					const matrix<RightElementType, rightRows, rightColumns> &right)						\
					-> decltype(left - right)															\
					{																					\
						typedef decltype(left - right) TResult;											\
						TResult result;																	\
						for (unsigned i = 0; i < TResult::rows; i++)									\
							result[i] = VectorMath::f(left[i], right[i]);								\
						return result;																	\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)															\
					template																			\
					<																					\
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,		\
						typename RightElementType														\
					>																					\
					auto f(																				\
					const matrix<LeftElementType, leftRows, leftColumns> &left,							\
					const RightElementType &right)														\
					-> decltype(left - right)															\
					{																					\
						typedef decltype(left - right) TResult;											\
						TResult result;																	\
						for (unsigned i = 0; i < TResult::rows; i++)									\
							result[i] = VectorMath::f(left[i], right);									\
						return result;																	\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION(f)															\
					template																			\
					<																					\
						typename LeftElementType,														\
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns	\
					>																					\
					auto f(																				\
					const LeftElementType &left,														\
					const matrix<RightElementType, rightRows, rightColumns> &right)						\
					-> decltype(left - right)															\
					{																					\
						typedef decltype(left - right) TResult;											\
						TResult result;																	\
						for (unsigned i = 0; i < TResult::rows; i++)									\
							result[i] = VectorMath::f(left, right[i]);									\
						return result;																	\
					}
				FUNCTION_DEFINITION(min)
				FUNCTION_DEFINITION(max)
#				undef FUNCTION_DEFINITION
#			pragma endregion

#			pragma region all/any/none functions
#ifdef MSVC_LIMITATIONS
#				define FUNCTION_DEFINITION(f)																																	\
					template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>									\
					inline bool f(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet, std::integral_constant<bool, SwizzleDesc::isWriteMaskValid>> &src)	\
					{																																							\
						typedef CSwizzleIterator<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzleIterator;														\
						return std::f##_of(TSwizzleIterator(src, 0), TSwizzleIterator(src, SwizzleDesc::TDimension::value), [](typename std::conditional						\
						<																																						\
							sizeof(typename TSwizzleIterator::value_type) <= sizeof(void *),																					\
							typename TSwizzleIterator::value_type,																												\
							typename TSwizzleIterator::reference																												\
						>::type element) -> bool {return element;});																											\
					};
#else
#				define FUNCTION_DEFINITION(f)																												\
					template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>				\
					inline bool f(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &src)												\
					{																																		\
						typedef CSwizzleIterator<ElementType, rows, columns, SwizzleDesc, odd, namingSet> TSwizzleIterator;									\
						return std::f##_of(TSwizzleIterator(src, 0), TSwizzleIterator(src, SwizzleDesc::TDimension::value), [](typename std::conditional	\
						<																																	\
							sizeof(typename TSwizzleIterator::value_type) <= sizeof(void *),																\
							typename TSwizzleIterator::value_type,																							\
							typename TSwizzleIterator::reference																							\
						>::type element) -> bool {return element;});																						\
					};
#endif
				FUNCTION_DEFINITION(all)
				FUNCTION_DEFINITION(any)
				FUNCTION_DEFINITION(none)
#				undef FUNCTION_DEFINITION

#				define FUNCTION_DEFINITION1(f) FUNCTION_DEFINITION2(f, f)
#				ifdef MSVC_LIMITATIONS
#				define FUNCTION_DEFINITION2(f1, f2)																											\
					template<typename ElementType, unsigned int rowCount, unsigned int columnCount>															\
					bool f1(const matrix<ElementType, rowCount, columnCount> &src)																			\
					{																																		\
						const auto &rows = reinterpret_cast<const matrix<ElementType, rowCount, columnCount>::TRow (&)[rowCount]>(src._data._rows);			\
						return std::f2##_of(rows, rows + rowCount, f1<ElementType, 0, columnCount, CVectorSwizzleDesc<columnCount>, false, 1>);				\
					};
#				else
#				define FUNCTION_DEFINITION2(f1, f2)																											\
					template<typename ElementType, unsigned int rows, unsigned int columns>																	\
					bool f1(const matrix<ElementType, rows, columns> &src)																					\
					{																																		\
						return std::f2##_of(src._data._rows, src._data._rows + rows, f1<ElementType, 0, columns, CVectorSwizzleDesc<columns>, false, 1>);	\
					};
#				endif
				FUNCTION_DEFINITION1(all)
				FUNCTION_DEFINITION1(any)
				FUNCTION_DEFINITION2(none, all)	// none for matrix requires special handling
#				undef FUNCTION_DEFINITION1
#				undef FUNCTION_DEFINITION2
#			pragma endregion

#			pragma region mul functions
				// note: most of these functions are not inline

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
				>
				inline decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()) mul(
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
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
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				vector<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()), rightColumns> mul(
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
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
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
				>
				vector<decltype(std::declval<LeftElementType>() * std::declval<RightElementType>()), leftRows> mul(
				const matrix<LeftElementType, leftRows, leftColumns> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
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
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
				>
				inline auto dot(
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
				-> decltype(mul(left, right))
				{
					return mul(left, right);
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
				inline auto length(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &swizzle)
				-> decltype(sqrt(dot(swizzle, swizzle)))
				{
					return sqrt(dot(swizzle, swizzle));
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
				>
				inline auto distance(
				const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
				-> decltype(length(right - left))
				{
					return length(right - left);
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, class SwizzleDesc, bool odd, unsigned namingSet>
				inline auto normalize(const CSwizzle<ElementType, rows, columns, SwizzleDesc, odd, namingSet> &swizzle)
				-> decltype(swizzle / length(swizzle))
				{
					return swizzle / length(swizzle);
				}

#				pragma region "series of matrices delimitted by ',' interpreted as series of successive transforms; inspirited by boost's function superposition"
					template
					<
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
					>
					inline auto operator ,(
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
					-> decltype(mul(left, right))
					{
						return mul(left, right);
					}

					template
					<
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, class LeftSwizzleDesc, bool leftOdd, unsigned leftNamingSet,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns
					>
					inline auto operator ,(
					const CSwizzle<LeftElementType, leftRows, leftColumns, LeftSwizzleDesc, leftOdd, leftNamingSet> &left,
					const matrix<RightElementType, rightRows, rightColumns> &right)
					-> decltype(mul(left, right))
					{
						return mul(left, right);
					}

					template
					<
						typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
						typename RightElementType, unsigned int rightRows, unsigned int rightColumns, class RightSwizzleDesc, bool rightOdd, unsigned rightNamingSet
					>
					inline auto operator ,(
					const matrix<LeftElementType, leftRows, leftColumns> &left,
					const CSwizzle<RightElementType, rightRows, rightColumns, RightSwizzleDesc, rightOdd, rightNamingSet> &right)
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
#				pragma endregion
#			pragma endregion

#			undef ARITHMETIC_OPS
#			undef REL_OPS
#			undef GENERATE_OPERATORS_MACRO
#			undef GENERATE_OPERATORS
		}
	}
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED

#	undef TEMPLATE

#	pragma warning(pop)

#endif//__VECTOR_MATH_H__
#endif