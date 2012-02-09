/**
\author		Alexey Shaydurov aka ASH
\date		9.2.2012 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

/*
limitations due to lack of C++11 support

VS 2010 does not catch errors like vec.xx = vec.xx and does nothing for things like vec.x = vec.x

different versions of CDataContainer now inherited from specialized CSwizzle
sizeof(vector<float, 3>) in this case is 12 for both gcc and VS2010
if vector inherited from specialized CSwizzle instead of CDataContainer then sizeof(...) is 12 for gcc and 16 for VS2010
TODO: try inherit vector from CSwizzle for future versions of VS
*/

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
#	define GENERATE_ZERO_SEQ_PRED(d, state) BOOST_PP_LESS(BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(2, 1, state)), BOOST_PP_TUPLE_ELEM(2, 0, state))
#	define GENERATE_ZERO_SEQ_OP(d, state) (BOOST_PP_TUPLE_ELEM(2, 0, state), BOOST_PP_SEQ_PUSH_BACK(BOOST_PP_TUPLE_ELEM(2, 1, state), 0))
#	define GENERATE_ZERO_SEQ(size) BOOST_PP_TUPLE_ELEM(2, 1, BOOST_PP_WHILE(GENERATE_ZERO_SEQ_PRED, GENERATE_ZERO_SEQ_OP, (size, )))
#
#	define IS_SEQ_ZERO_OP(s, state, elem) BOOST_PP_BITAND(state, BOOST_PP_EQUAL(elem, 0))
#	define IS_SEQ_ZERO(seq) BOOST_PP_SEQ_FOLD_LEFT(IS_SEQ_ZERO_OP, 1, seq)
#
#	define IS_SEQ_UNIQUE_PRED(d, state) BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(3, 2, state)))
#	define IS_SEQ_ELEM_UNIQUE(s, state, elem) (BOOST_PP_BITAND(BOOST_PP_TUPLE_ELEM(2, 0, state), BOOST_PP_NOT_EQUAL(elem, BOOST_PP_TUPLE_ELEM(2, 1, state))), BOOST_PP_TUPLE_ELEM(2, 1, state))
#	define IS_SEQ_UNIQUE_OP(d, state) (BOOST_PP_BITAND(BOOST_PP_TUPLE_ELEM(3, 0, state), BOOST_PP_TUPLE_ELEM(2, 0, BOOST_PP_SEQ_FOLD_LEFT(IS_SEQ_ELEM_UNIQUE, (1, BOOST_PP_SEQ_ELEM(BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_TUPLE_ELEM(3, 2, state))), BOOST_PP_SEQ_FIRST_N(BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_TUPLE_ELEM(3, 2, state))))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(3, 1, state)), BOOST_PP_TUPLE_ELEM(3, 2, state))
#	define IS_SEQ_UNIQUE(seq) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(IS_SEQ_UNIQUE_PRED, IS_SEQ_UNIQUE_OP, (1, 1, seq)))
#
#	define INC_MODULO(elem, modulo1, modulo2) BOOST_PP_MOD(BOOST_PP_IF(BOOST_PP_MOD(BOOST_PP_MOD(BOOST_PP_INC(elem), 4), modulo1), BOOST_PP_INC(elem), BOOST_PP_SUB(BOOST_PP_ADD(elem, 4), BOOST_PP_MOD(BOOST_PP_ADD(elem, 4), 4))), BOOST_PP_MUL(modulo2, 4))
#	// ?: BOOST_PP_BITAND does not work in VS 2010
#	define INC_SEQ_PRED(d, state) BOOST_PP_AND(BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 3, state), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(4, 0, state))), BOOST_PP_BITOR(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(4, 3, state), 0), BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(BOOST_PP_DEC(BOOST_PP_TUPLE_ELEM(4, 3, state)), BOOST_PP_TUPLE_ELEM(4, 0, state)), 0)))
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
#	/*define SWIZZLES_INNER_LOOP_MACRO(r, state) \
		class\
		{\
		} BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(4, 1, state)), BOOST_PP_ARRAY_DATA(BOOST_PP_TUPLE_ELEM(4, 1, state)))));*/
#	define TRANSFORM_SWIZZLE(NAMING_SET, swizzle_seq) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, NAMING_SET, swizzle_seq))
//#	define SWIZZLE(state) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state)))
//#	define APPLY_CALLBACK(callback, arg) callback(arg)
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) APPLY_CALLBACK(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) BOOST_PP_TUPLE_ELEM(4, 0, state)(BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) \
//		class BOOST_PP_CAT(C, SWIZZLE(state))\
//		{\
//		} SWIZZLE(state);
#	define SWIZZLES_INTERMIDIATE_LOOP_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_INTERMIDIATE_LOOP_MACRO(r, state) BOOST_PP_FOR((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, SWIZZLES_INNER_LOOP_MACRO)
#	define SWIZZLES_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_MACRO(r, state) BOOST_PP_FOR((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, SWIZZLES_INTERMIDIATE_LOOP_MACRO)
#	define SWIZZLES(z, i, CALLBACK) \
		SWIZZLES_INNER_LOOP_MACRO(z, (CALLBACK, GENERATE_ZERO_SEQ(i), , ))\
		/*																		 cur iteration		max iterations
																					   ^				  ^
																					   |_______	   _______|
																							   |  |	*/\
		BOOST_PP_FOR((CALLBACK, INC_SEQ(GENERATE_ZERO_SEQ(i), COLUMNS, BOOST_PP_MAX(ROWS, 1)), 0, 64), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_OP, SWIZZLES_MACRO)
#	define GENERATE_SWIZZLES(CALLBACK) BOOST_PP_REPEAT_FROM_TO(1, 5, SWIZZLES, CALLBACK)
#
//#	define SWIZZLE_CLASSNAME(swizzle_seq) BOOST_PP_CAT(C, BOOST_PP_SEQ_CAT(swizzle_seq))
#	define SWIZZLE_SEQ(state) BOOST_PP_TUPLE_ELEM(4, 1, state)
#	define SWIZZLE_SEQ_2_VECTOR(seq) mpl::vector_c<unsigned int, BOOST_PP_TUPLE_REM_CTOR(BOOST_PP_SEQ_SIZE(seq), BOOST_PP_SEQ_TO_TUPLE(seq))>
#
#	define PACK_SWIZZLE_PRED(d, state) BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(3, 2, state))
#	define PACK_SWIZZLE_OP(d, state) (BOOST_PP_TUPLE_ELEM(3, 0, state) + (BOOST_PP_SEQ_HEAD(BOOST_PP_TUPLE_ELEM(3, 2, state)) << BOOST_PP_TUPLE_ELEM(3, 1, state)), BOOST_PP_ADD(BOOST_PP_TUPLE_ELEM(3, 1, state), 4), BOOST_PP_SEQ_TAIL(BOOST_PP_TUPLE_ELEM(3, 2, state)))
#	//																							   result		insert pos
#	//																								  ^				^
#	//																								  |____		____|
#	//																									   |   |
#	define PACK_SWIZZLE(seq) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(PACK_SWIZZLE_PRED, PACK_SWIZZLE_OP, (0u, 0, seq)))
//#
//#	define GET_SWIZZLE_ELEMENT_SEQ(seq, i, cv) (GET_SWIZZLE_ELEMENT(COLUMNS, BOOST_PP_SEQ_ELEM(i, seq), cv))
//#	define GET_SWIZZLE_ELEMENT_VECTOR(vector, i, cv) (GET_SWIZZLE_ELEMENT(COLUMNS, (mpl::at_c<vector, i>::type::value), cv))
//#
//#	define GENERATE_SCALAR_OPERATION_PRED(r, state) BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define GENERATE_SCALAR_OPERATION_OP(r, state) (BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 0, state)), BOOST_PP_TUPLE_ELEM(4, 1, state), BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 3, state))
//#	define GENERATE_SCALAR_OPERATION_MACRO(r, state) GET_SWIZZLE_ELEMENT_SEQ(BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 0, state), ) = GET_SWIZZLE_ELEMENT_VECTOR(BOOST_PP_TUPLE_ELEM(4, 3, state), BOOST_PP_TUPLE_ELEM(4, 0, state), const);

#	define BOOST_PP_ITERATION_LIMITS (1, 4)
#	define BOOST_PP_FILENAME_2 "vector math.h"
#	include BOOST_PP_ITERATE()
#else
	/*
	gcc does not allow explicit specialization in class scope => CSwizzle can not be inside CDataContainer
	ElementType needed in order to compiler can deduce template args for operators
	*/
#	define GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq) \
		template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector>\
		CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)\
		{\
			/*static_assert(mpl::size<mpl::unique<mpl::sort<SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>::type, std::is_same<mpl::_, mpl::_>>::type>::value == mpl::size<SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>::value, "!");*/\
			static_assert(BOOST_PP_SEQ_SIZE(leftSwizzleSeq) <= mpl::size<CRightSwizzleVector>::type::value, "operator =: too few components in src");\
			/*BOOST_PP_FOR((0, BOOST_PP_SEQ_SIZE(leftSwizzleSeq), leftSwizzleSeq, CRightSwizzleVector), GENERATE_SCALAR_OPERATION_PRED, GENERATE_SCALAR_OPERATION_OP, GENERATE_SCALAR_OPERATION_MACRO)*/\
			for (unsigned int idx = 0; idx < BOOST_PP_SEQ_SIZE(leftSwizzleSeq); idx++)\
			{\
				(*this)[idx] = right[idx];\
			}\
			return *this;\
		};
#	define GENERATE_ASSIGN_OPERATOR(leftSwizzleSeq) \
		CSwizzle &operator =(const CSwizzle &right)\
		{\
			return operator =<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>(right);\
		};
#	ifdef MSVC_LIMITATIONS
		// VS 2010 does not allow operator = in union members
#		define GENERATE_ASSIGN_OPERATORS(leftSwizzleSeq) GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq)
#		define DISABLE_ASSIGN_OPERATOR(leftSwizzleSeq) /*private: CSwizzle &operator =(const CSwizzle &);*/
#	else
#		define GENERATE_ASSIGN_OPERATORS(leftSwizzleSeq) \
			GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq)\
			GENERATE_ASSIGN_OPERATOR(leftSwizzleSeq)
#		define DISABLE_ASSIGN_OPERATOR(leftSwizzleSeq) CSwizzle &operator =(const CSwizzle &) = delete;
#	endif
#	define SWIZZLES_INNER_LOOP_MACRO(r, state) \
		template<typename ElementType>\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>: public CGenericSwizzleCommon<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>\
		{\
		public:\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))\
		};
	GENERATE_SWIZZLES()
#	undef SWIZZLES_INNER_LOOP_MACRO
#	undef GENERATE_TEMPLATED_ASSIGN_OPERATOR
#	undef GENERATE_ASSIGN_OPERATOR
#	undef GENERATE_ASSIGN_OPERATORS
//#	define GENERATE_SWIZZLE_CLASS(swizzle_seq) \
//			class CSwizzle<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)>\
//			{\
//			};
//		GENERATE_SWIZZLES(GENERATE_SWIZZLE_CLASS)
//#	undef GENERATE_SWIZZLE_CLASS

	template<typename ElementType>
	class CDataContainer<ElementType, ROWS, COLUMNS>: public std::conditional<ROWS == 0, CSwizzle<ElementType, 0, COLUMNS, 0u, void>, CEmpty>::type
	{
	protected:
		CDataContainer(): _data()
		{
		}
		CDataContainer(const CDataContainer &src): _data(src._data)
		{
		}
		~CDataContainer()
		{
			_data.~CData<ElementType, ROWS, COLUMNS>();
		}
		CDataContainer &operator =(const CDataContainer &right)
		{
			_data = right.data;
			return *this;
		}
	public:
		union
		{
			CData<ElementType, ROWS, COLUMNS> _data;
			// gcc does not allow class definition inside anonymous union
#			define NAMING_SET_1 BOOST_PP_IF(ROWS, MATRIX_ZERO_BASED, XYZW)
#			define NAMING_SET_2 BOOST_PP_IF(ROWS, MATRIX_ONE_BASED, RGBA)
#			define SWIZZLES_INNER_LOOP_MACRO(r, state) CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))> TRANSFORM_SWIZZLE(NAMING_SET_1, SWIZZLE_SEQ(state)), TRANSFORM_SWIZZLE(NAMING_SET_2, SWIZZLE_SEQ(state));
			GENERATE_SWIZZLES()
#			undef NAMING_SET_1
#			undef NAMING_SET_2
#			undef SWIZZLES_INNER_LOOP_MACRO
//#			define GENERATE_SWIZZLE_OBJECT(swizzle_seq) CSwizzle<SWIZZLE_SEQ_2_VECTOR(swizzle_seq)> TRANSFORM_SWIZZLE(XYZW, swizzle_seq), TRANSFORM_SWIZZLE(RGBA, swizzle_seq);
//			GENERATE_SWIZZLES(GENERATE_SWIZZLE_OBJECT)
//#			undef GENERATE_SWIZZLE_OBJECT
		};
	};
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
#	undef SWIZZLE
#	undef TRANSFORM_SWIZZLE
//#	undef APPLY_CALLBACK
//#	undef SWIZZLES_INNER_LOOP_MACRO
#	undef SWIZZLES_INTERMIDIATE_LOOP_OP
#	undef SWIZZLES_INTERMIDIATE_LOOP_MACRO
#	undef SWIZZLES_OP
#	undef SWIZZLES_MACRO
#	undef SWIZZLES
#	undef GENERATE_SWIZZLES
//#	undef SWIZZLE_CLASSNAME
#	undef SWIZZLE_SEQ
//#	undef SWIZZLE_SEQ_2_VECTOR
#	undef PACK_SWIZZLE_PRED
#	undef PACK_SWIZZLE_OP
#	undef PACK_SWIZZLE
//#	undef GET_SWIZZLE_ELEMENT_SEQ
//#	undef GET_SWIZZLE_ELEMENT_VECTOR
//#	undef GENERATE_SCALAR_OPERATION_PRED
//#	undef GENERATE_SCALAR_OPERATION_OP
//#	undef GENERATE_SCALAR_OPERATION_MACRO
#endif
#else
#	ifndef __VECTOR_MATH_H__
#	define __VECTOR_MATH_H__

#	include "C++11 dummies.h"
#	include <crtdbg.h>
#	include <stddef.h>
#	ifdef MSVC_LIMITATIONS
		/*
			declval is not included in VS2010 => use boost
		*/
#		include <boost\utility\declval.hpp>
		using boost::declval;
#	else
#		include <utility>
		using std::declval;
#	endif
#	include <type_traits>
#	include <algorithm>
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
#	include <boost\preprocessor\seq\cat.hpp>
#	include <boost\preprocessor\seq\to_tuple.hpp>
#	include <boost\mpl\placeholders.hpp>
#	include <boost\mpl\vector_c.hpp>
#	include <boost\mpl\at.hpp>
#	include <boost\mpl\min_max.hpp>
#	include <boost\mpl\sort.hpp>
#	include <boost\mpl\size.hpp>
#	include <boost\mpl\unique.hpp>
#	include <boost\mpl\equal_to.hpp>
#	include <boost\mpl\not.hpp>
#	include <boost\mpl\identity.hpp>

//#	define GET_SWIZZLE_ELEMENT(vectorDimension, idx, cv) (reinterpret_cast<cv CData<ElementType, vectorDimension> &>(*this)._data[(idx)])
//#	define GET_SWIZZLE_ELEMENT_PACKED(vectorDimension, packedSwizzle, idx, cv) (GET_SWIZZLE_ELEMENT(vectorDimension, packedSwizzle >> ((idx) << 1) & 3u, cv))

	namespace DGLE2
	{
		namespace VectorMath
		{
			namespace mpl = boost::mpl;

			template<typename ElementType, unsigned int dimension>
			class vector;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class matrix;

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
			class CGenericSwizzleCommon;

			// rows = 0 for vectors
			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
			class CSwizzle;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CDataContainer;

			class CDataBase
			{
#		ifndef MSVC_LIMITATIONS
			protected:
				CDataBase() = default;
				CDataBase(const CDataBase &) = default;
				~CDataBase() = default;
				CDataBase &operator =(const CDataBase &) = default;
#		endif
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CData: CDataBase final
			{
				friend class matrix<ElementType, rows, columns>;

				//template<typename, unsigned int, unsigned int, unsigned short, class>
				//friend class CSwizzle;

				//template<typename, unsigned int, unsigned int, unsigned short, class>
				//friend class CGenericSwizzleCommon;

				//friend class CDataContainer<ElementType, 0, dimension>;
			private:
#		ifdef MSVC_LIMITATIONS
				ElementType _rows[rows][columns];
#		else
				vector<ElementType, columns> _rows[rows];
#		endif
			};

			template<typename ElementType, unsigned int dimension>
			class CData<ElementType, 0, dimension>: CDataBase final
			{
				friend class vector<ElementType, dimension>;

				template<typename, unsigned int, unsigned int, unsigned short, class>
				friend class CSwizzle;

				template<typename, unsigned int, unsigned int, unsigned short, class>
				friend class CGenericSwizzleCommon;

				friend class CDataContainer<ElementType, 0, dimension>;
			private:
				ElementType _data[dimension];
			};

			class CEmpty {};

			// generic vector/matrix
			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CDataContainer: public std::conditional<rows == 0, CSwizzle<ElementType, 0, columns, 0u, void>, CEmpty>::type
			{
			protected:
				CData<ElementType, rows, columns> _data;
#		ifndef MSVC_LIMITATIONS
				CDataContainer() = default;
				CDataContainer(const CDataContainer &) = default;
				~CDataContainer() = default;
				CDataContainer &operator =(const CDataContainer &) = default;
#		endif
			};

			// specializations for graphics vectors
#			define BOOST_PP_ITERATION_LIMITS (0, 4)
#			define BOOST_PP_FILENAME_1 "vector math.h"
#			include BOOST_PP_ITERATE()

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
			class CSwizzleBase
			{
				/*
						  static
							 ^
							 |
				CSwizzleBase -> CSwizzle
				*/
				typedef CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector> TSwizzle;
			public:
				operator const ElementType &() const noexcept
				{
					return static_cast<const TSwizzle &>(*this)[0];
				}
				operator ElementType () noexcept
				{
					return static_cast<TSwizzle &>(*this)[0];
				}
				operator const ElementType &() noexcept
				{
					return operator ElementType();
				}
			};

			// generic CSwizzle inherits from this to reduce preprocessor generated code for faster compiling
			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
			class CGenericSwizzleCommon: public CSwizzleBase<ElementType, rows, columns, packedSwizzle, CSwizzleVector>
			{
				typedef CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector> TSwizzle;
				typedef matrix<ElementType, rows, columns> Tmatrix;
			public:
				typedef TSwizzle TOperationResult;
			public:
				const ElementType &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < mpl::size<CSwizzleVector>::value);
					idx = packedSwizzle >> (idx << 2u) & (1u << 4u) - 1u;
					auto row = idx >> 2 & 3u, column = idx & 3u;
					/*
									   static	 reinterpret
										  ^			  ^
										  |			  |
					CGenericSwizzleCommon -> CSwizzle -> CMatrixData
					*/
					typedef CData<ElementType, rows, columns> CData;
					return reinterpret_cast<const CData &>(static_cast<const TSwizzle &>(*this))._data[row][column];
				}
				ElementType &operator [](unsigned int idx) noexcept
				{
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<ElementType &>(static_cast<const CGenericSwizzleCommon &>(*this)[idx]);
				}
			};

			// specialization for vectors
			template<typename ElementType, unsigned int vectorDimension, unsigned short packedSwizzle, class CSwizzleVector>
			class CGenericSwizzleCommon<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector>: public CSwizzleBase<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector>
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
				typedef CSwizzle<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector> TSwizzle;
				typedef vector<ElementType, vectorDimension> Tvector;
			public:
				typedef TSwizzle TOperationResult;
			public:
				const ElementType &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < mpl::size<CSwizzleVector>::value);
					idx = packedSwizzle >> (idx << 2u) & (1u << 4u) - 1u;
					/*
									   static	 reinterpret
										  ^			  ^
										  |			  |
					CGenericSwizzleCommon -> CSwizzle -> CData
					*/
					typedef CData<ElementType, 0, vectorDimension> CData;
					return reinterpret_cast<const CData &>(static_cast<const TSwizzle &>(*this))._data[idx];
				}
				ElementType &operator [](unsigned int idx) noexcept
				{
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<ElementType &>(static_cast<const CGenericSwizzleCommon &>(*this)[idx]);
				}
			};

			// this specialization used as base class for CDataContainer to eliminate need for various overloads
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzle<ElementType, 0, vectorDimension, 0u, void>: public CSwizzleBase<ElementType, 0, vectorDimension, 0u, void>
			{
				typedef vector<ElementType, vectorDimension> Tvector;
			public:
				typedef Tvector TOperationResult;
			public:
				operator const Tvector &() const noexcept
				{
					/*
						  static
							 ^
							 |
					CSwizzle -> vector
					*/
					return static_cast<const Tvector &>(*this);
				}
				operator const Tvector &() noexcept
				{
					return static_cast<const CSwizzle *>(this)->operator const Tvector &();
				}
				operator Tvector &() noexcept
				{
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<Tvector &>(operator const Tvector &());
				}
				const ElementType &operator [](unsigned int idx) const
				{
					return operator const Tvector &()[idx];
				}
				ElementType &operator [](unsigned int idx)
				{
					return operator Tvector &()[idx];
				}
			};

#			pragma region(generate operators)
				template<unsigned int vectorDimension, class CSwizzleVector>
				struct TSwizzleTraits
				{
				private:
					typedef mpl::not_<std::is_void<CSwizzleVector>> TIsSwizzle;
				public:
					typedef typename std::conditional<TIsSwizzle::value, mpl::size<CSwizzleVector>, std::integral_constant<unsigned, vectorDimension>>::type TDimension;
				private:
					typedef typename std::conditional<TIsSwizzle::value, mpl::sort<CSwizzleVector>, mpl::identity<void>>::type::type CSortedSwizzleVector;
					typedef typename std::conditional<TIsSwizzle::value, mpl::unique<CSortedSwizzleVector, std::is_same<mpl::_, mpl::_>>, mpl::identity<void>>::type::type CUniqueSwizzleVector;
				public:
					typedef typename std::conditional<TIsSwizzle::value, mpl::equal_to<mpl::size<CUniqueSwizzleVector>, mpl::size<CSwizzleVector>>, std::true_type>::type TIsWriteMaskValid;
				};

				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
				inline const typename CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector>::TOperationResult &operator +(const CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
				inline typename CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector>::TOperationResult &operator +(CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
				inline vector<ElementType, columns> operator -(const CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector> &src)
				{
					typedef TSwizzleTraits<columns, CSwizzleVector> TSwizzleTraits;
					vector<ElementType, columns> result;
					for (typename TSwizzleTraits::TDimension::value_type i = 0; i < TSwizzleTraits::TDimension::value; i++)
						result[i] = -src[i];
					return result;
				}

				template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector>
				inline typename CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector>::TOperationResult &operator +=(CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector> &left, const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					typedef TSwizzleTraits<leftColumns, CLeftSwizzleVector> TLeftSwizzleTraits;
					typedef TSwizzleTraits<rightColumns, CRightSwizzleVector> TRightSwizzleTraits;
					static_assert(TLeftSwizzleTraits::TIsWriteMaskValid::value, "operator +=: invalid write mask");
					static_assert(TLeftSwizzleTraits::TDimension::value <= TRightSwizzleTraits::TDimension::value, "operator +=: too few components in src");
					for (typename TLeftSwizzleTraits::TDimension::value_type i = 0; i < TLeftSwizzleTraits::TDimension::value; i++)
						left[i] += right[i];
					return left;
				};

				template<typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector>
				inline vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), mpl::min<mpl::size<CLeftSwizzleVector>, mpl::size<CRightSwizzleVector>>::type::value> operator +(const CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector> &left, const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					/*
					static_cast effect:
					if left is swizzle: conversion operator in CGenericSwizzleCommon (const CGenericSwizzleCommon & -> vector)
					if left is vector: upcast
					*/
					//auto result(static_cast<const vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), mpl::min<mpl::size<CLeftSwizzleVector>, mpl::size<CRightSwizzleVector>>::type::value> &>(left));
					vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), mpl::min<mpl::size<CLeftSwizzleVector>, mpl::size<CRightSwizzleVector>>::type::value> result(left);
					return result += right;
				};
#			pragma endregion

			template<typename ElementType, unsigned int dimension>
			class vector: public CDataContainer<ElementType, 0, dimension>
			{
				static_assert(dimension > 0, "vector dimension should be positive");
				//using CDataContainer<ElementType, dimension>::_data;
			public:
#				ifdef MSVC_LIMITATIONS
				vector() {}
#				else
				vector() = default;
#				endif
				vector(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar)
				{
					std::fill_n(CDataContainer<ElementType, 0, dimension>::_data._data, dimension, scalar);
				}
				template<typename TIterator>
				vector(TIterator src)
				{
					std::copy_n(src, dimension, CDataContainer<ElementType, 0, dimension>::_data._data);
				}
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, unsigned short srcPackedSwizzle, class CSrcSwizzleVector>
				vector(const CSwizzle<SrcElementType, srcRows, srcColumns, srcPackedSwizzle, CSrcSwizzleVector> &src)
				{
					typedef TSwizzleTraits<srcColumns, CSrcSwizzleVector> TSrcSwizzleTraits;
					static_assert(dimension <= TSrcSwizzleTraits::TDimension::value, "\"copy\" ctor: too few components in src");
					for (unsigned i = 0; i < dimension; i++)
						new(&operator [](i)) ElementType(src[i]);
				}
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector>
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					typedef TSwizzleTraits<rightColumns, CRightSwizzleVector> TRightSwizzleTraits;
					static_assert(dimension <= TRightSwizzleTraits::TDimension::value, "operator =: too few components in src");
					for (unsigned i = 0; i < dimension; i++)
						operator [](i) = right[i];
					return *this;
				}
				const ElementType &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}
				ElementType &operator [](unsigned int idx) noexcept
				{
					_ASSERTE(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class matrix: public CDataContainer<ElementType, rows, columns>
			{
				typedef vector<ElementType, columns> TRow;
			public:
				const TRow &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < rows);
#		ifdef MSVC_LIMITATIONS
					return reinterpret_cast<const TRow &>(CDataContainer<ElementType, rows, columns>::_data._rows[idx]);
#		else
					return CDataContainer<ElementType, rows, columns>::_data._rows[idx];
#		endif
				}
				TRow &operator [](unsigned int idx) noexcept
				{
					_ASSERTE(idx < rows);
#		ifdef MSVC_LIMITATIONS
					return reinterpret_cast<TRow &>(CDataContainer<ElementType, rows, columns>::_data._rows[idx]);
#		else
					return CDataContainer<ElementType, rows, columns>::_data._rows[idx];
#		endif
				}
			};
#	pragma region(temp test)
			template class vector<float, 4>;
			template class CSwizzle<float, 0, 4, 0u, void>;
#	pragma endregion
		}
	}
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED

	#endif//__VECTOR_MATH_H__
#endif