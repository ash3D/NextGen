/**
\author		Alexey Shaydurov aka ASH
\date		30.1.2012 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#if BOOST_PP_IS_ITERATING
#	define DIMENSION BOOST_PP_ITERATION()
#	define EMPTY1(arg1)
#	define XYZW (x, y, z, w)
#	define RGBA (r, g, b, a)
//#	define GENERATE_ZERO_SEQ_PRED(d, state) BOOST_PP_LESS(BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(2, 1, state)), BOOST_PP_TUPLE_ELEM(2, 0, state))
//#	define GENERATE_ZERO_SEQ_OP(d, state) (BOOST_PP_TUPLE_ELEM(2, 0, state), BOOST_PP_ARRAY_PUSH_BACK(BOOST_PP_TUPLE_ELEM(2, 1, state), 0))
//#	define GENERATE_ZERO_SEQ(size) BOOST_PP_TUPLE_ELEM(2, 1, BOOST_PP_WHILE(GENERATE_ZERO_SEQ_PRED, GENERATE_ZERO_SEQ_OP, (size, (0, ()))))
//#	define IS_SEQ_ZERO_OP(s, state, elem) BOOST_PP_BITAND(state, BOOST_PP_EQUAL(elem, 0))
//#	define IS_SEQ_ZERO(array) BOOST_PP_SEQ_FOLD_LEFT(IS_SEQ_ZERO_OP, 1, BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_ARRAY_SIZE(array), BOOST_PP_ARRAY_DATA(array)))
//#	define INC_SEQ_PRED(d, state) BOOST_PP_BITAND(BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(3, 0, state))), BOOST_PP_BITOR(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(3, 2, state), 0), BOOST_PP_EQUAL(BOOST_PP_ARRAY_ELEM(BOOST_PP_DEC(BOOST_PP_TUPLE_ELEM(3, 2, state)), BOOST_PP_TUPLE_ELEM(3, 0, state)), 0)))
//#	define INC_SEQ_OP(d, state) (BOOST_PP_ARRAY_REPLACE(BOOST_PP_TUPLE_ELEM(3, 0, state), BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_MOD(BOOST_PP_INC(BOOST_PP_ARRAY_ELEM(BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_TUPLE_ELEM(3, 0, state))), BOOST_PP_TUPLE_ELEM(3, 1, state))), BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(3, 2, state)))
///*																														cur idx
//																														   ^
//																														   |	*/
//#	define INC_SEQ(array, modulo) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(INC_SEQ_PRED, INC_SEQ_OP, (array, modulo, 0)))
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
//#	define IS_SEQ_UNIQUE_PRED(d, state) BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 2), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(4, 3)))
//#	define IS_SEQ_UNIQUE_OP(d, state) (BOOST_PP_BITAND(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_NOT_EQUAL(BOOST_PP_SEQ_ELEM(BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 3)), BOOST_PP_TUPLE_ELEM(4, 1, state))), BOOST_PP_TUPLE_ELEM(4, 1, state), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2)), BOOST_PP_TUPLE_ELEM(4, 3))
//#	define IS_SEQ_UNIQUE(seq) BOOST_PP_TUPLE_ELEM(4, 0, BOOST_PP_SEQ_WHILE(IS_SEQ_UNIQUE_PRED, IS_SEQ_UNIQUE_OP, (1, BOOST_PP_SEQ_ELEM(0, seq), 1, seq)))
#
#	define INC_SEQ_PRED(d, state) BOOST_PP_AND(BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(3, 0, state))), BOOST_PP_BITOR(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(3, 2, state), 0), BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(BOOST_PP_DEC(BOOST_PP_TUPLE_ELEM(3, 2, state)), BOOST_PP_TUPLE_ELEM(3, 0, state)), 0)))
#	define INC_SEQ_OP(d, state) (BOOST_PP_SEQ_REPLACE(BOOST_PP_TUPLE_ELEM(3, 0, state), BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_MOD(BOOST_PP_INC(BOOST_PP_SEQ_ELEM(BOOST_PP_TUPLE_ELEM(3, 2, state), BOOST_PP_TUPLE_ELEM(3, 0, state))), BOOST_PP_TUPLE_ELEM(3, 1, state))), BOOST_PP_TUPLE_ELEM(3, 1, state), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(3, 2, state)))
/*																											  cur idx
																												 ^
																												 |	*/
#	define INC_SEQ(seq, modulo) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(INC_SEQ_PRED, INC_SEQ_OP, (seq, modulo, 0)))
#
#	define IDX_2_SYMBOL(s, NAMING_SET, i) BOOST_PP_TUPLE_ELEM(4, i, NAMING_SET)
#
#	define SWIZZLES_INNER_LOOP_PRED(r, state) BOOST_PP_BITAND(BOOST_PP_NOT(IS_SEQ_ZERO(BOOST_PP_TUPLE_ELEM(4, 1, state))), BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 3, state)))
#	define SWIZZLES_INNER_LOOP_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), INC_SEQ(BOOST_PP_TUPLE_ELEM(4, 1, state), DIMENSION), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) \
//		class\
//		{\
//		} BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(4, 1, state)), BOOST_PP_ARRAY_DATA(BOOST_PP_TUPLE_ELEM(4, 1, state)))));
#	define TRANSFORM_SWIZZLE(NAMING_SET, swizzle_seq) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, NAMING_SET, swizzle_seq))
//#	define SWIZZLE(state) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state)))
//#	define APPLY_CALLBACK(callback, arg) callback(arg)
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) APPLY_CALLBACK(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) BOOST_PP_TUPLE_ELEM(4, 0, state)(BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) \
//		class BOOST_PP_CAT(C, SWIZZLE(state))\
//		{\
//		} SWIZZLE(state);
#	define SWIZZLES_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_MACRO(r, state) BOOST_PP_FOR((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, SWIZZLES_INNER_LOOP_MACRO)
#	define SWIZZLES(z, i, CALLBACK) \
		SWIZZLES_INNER_LOOP_MACRO(z, (CALLBACK, GENERATE_ZERO_SEQ(i), , ))\
/*															cur iteration	   max iterations
																  ^					 ^
																  |_______	  _______|
																		  |	 |	*/\
		BOOST_PP_FOR((CALLBACK, INC_SEQ(GENERATE_ZERO_SEQ(i), DIMENSION), 0, 64), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_OP, SWIZZLES_MACRO)
#	define GENERATE_SWIZZLES(CALLBACK) BOOST_PP_REPEAT_FROM_TO(1, 5, SWIZZLES, CALLBACK)
#
//#	define SWIZZLE_CLASSNAME(swizzle_seq) BOOST_PP_CAT(C, BOOST_PP_SEQ_CAT(swizzle_seq))
#	define SWIZZLE_SEQ(state) BOOST_PP_TUPLE_ELEM(4, 1, state)
#	define SWIZZLE_VECTOR(state) mpl::vector_c<unsigned int, BOOST_PP_TUPLE_REM_CTOR(BOOST_PP_SEQ_SIZE(SWIZZLE_SEQ(state)), BOOST_PP_SEQ_TO_TUPLE(SWIZZLE_SEQ(state)))>
#
#	define PACK_SWIZZLE_PRED(d, state) BOOST_PP_SEQ_SIZE(BOOST_PP_TUPLE_ELEM(3, 2, state))
#	define PACK_SWIZZLE_OP(d, state) (BOOST_PP_TUPLE_ELEM(3, 0, state) + (BOOST_PP_SEQ_HEAD(BOOST_PP_TUPLE_ELEM(3, 2, state)) << BOOST_PP_TUPLE_ELEM(3, 1, state)), BOOST_PP_ADD(BOOST_PP_TUPLE_ELEM(3, 1, state), 2), BOOST_PP_SEQ_TAIL(BOOST_PP_TUPLE_ELEM(3, 2, state)))
/*																								   result		insert pos
																									  ^				^
																									  |____		____|
																										   |   |	*/
#	define PACK_SWIZZLE(seq) BOOST_PP_TUPLE_ELEM(3, 0, BOOST_PP_WHILE(PACK_SWIZZLE_PRED, PACK_SWIZZLE_OP, (0u, 0, seq)))
#
#	define GET_SWIZZLE_ELEMENT(i, cv) (reinterpret_cast<cv CVectorData<ElementType, DIMENSION> &>(*this)._data[(i)])
#	define GET_SWIZZLE_ELEMENT_SEQ(seq, i, cv) (GET_SWIZZLE_ELEMENT(BOOST_PP_SEQ_ELEM(i, seq), cv))
#	define GET_SWIZZLE_ELEMENT_PACKED(packedSwizzle, i, cv) (GET_SWIZZLE_ELEMENT(packedSwizzle >> ((i) << 1) & 3u, cv))
#	define GET_SWIZZLE_ELEMENT_VECTOR(vector, i, cv) (GET_SWIZZLE_ELEMENT((mpl::at_c<vector, i>::type::value), cv))
#
#	define GENERATE_SCALAR_OPERATION_PRED(r, state) BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state))
#	define GENERATE_SCALAR_OPERATION_OP(r, state) (BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 0, state)), BOOST_PP_TUPLE_ELEM(4, 1, state), BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define GENERATE_SCALAR_OPERATION_MACRO(r, state) GET_SWIZZLE_ELEMENT_SEQ(BOOST_PP_TUPLE_ELEM(4, 2, state), BOOST_PP_TUPLE_ELEM(4, 0, state), ) = GET_SWIZZLE_ELEMENT_VECTOR(BOOST_PP_TUPLE_ELEM(4, 3, state), BOOST_PP_TUPLE_ELEM(4, 0, state), const);

	/*
	gcc does not allow explicit specialisation in class scope => CSwizzle can not be inside CVectorDataContainer
	ElementType needed in order to compiler can deduce template args for operators
	*/
	template<typename ElementType, unsigned int vectorDimension, unsigned short packedSwizzle, class CSwizzleVector>
	class CSwizzle;
#	define GENERATE_OPERATOR(leftSwizzleSeq) \
		template<typename RightElementType, unsigned int rightVectorDimension, unsigned short rightPackedSwizzle, class CRightSwizzleVector>\
		CSwizzle &operator =(const CSwizzle<RightElementType, rightVectorDimension, rightPackedSwizzle, CRightSwizzleVector> &right)\
		{\
			/*static_assert(mpl::size<mpl::unique<mpl::sort<SWIZZLE_VECTOR(state)>::type, std::is_same<mpl::_, mpl::_>>::type>::value == mpl::size<SWIZZLE_VECTOR(state)>::value, "!");*/\
			static_assert(BOOST_PP_SEQ_SIZE(leftSwizzleSeq) <= mpl::size<CRightSwizzleVector>::type::value, "operator =: too few components in src");\
			BOOST_PP_FOR((0, BOOST_PP_SEQ_SIZE(leftSwizzleSeq), leftSwizzleSeq, CRightSwizzleVector), GENERATE_SCALAR_OPERATION_PRED, GENERATE_SCALAR_OPERATION_OP, GENERATE_SCALAR_OPERATION_MACRO)\
			return *this;\
		};
#	define SWIZZLES_INNER_LOOP_MACRO(r, state) \
		template<typename ElementType>\
		class CSwizzle<ElementType, DIMENSION, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_VECTOR(state)>\
		{\
		public:\
			CSwizzle &operator +() noexcept\
			{\
				return *this;\
			}\
			const CSwizzle &operator +() const noexcept\
			{\
				return *this;\
			}\
			vector<ElementType, DIMENSION> &operator -()\
			{\
				return vector<ElementType, DIMENSION>();\
			}\
			inline const ElementType &operator [](unsigned int idx) const\
			{\
				return GET_SWIZZLE_ELEMENT_PACKED(PACK_SWIZZLE(SWIZZLE_SEQ(state)), idx, const);\
			}\
			inline ElementType &operator [](unsigned int idx)\
			{\
				return GET_SWIZZLE_ELEMENT_PACKED(PACK_SWIZZLE(SWIZZLE_SEQ(state)), idx, );\
				/* const version returns (const &), not value => it is safe to use const_cast */\
				/*return const_cast<ElementType &>(static_cast<const decltype(*this) &>(*this)[idx]);*/\
			}\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_OPERATOR, EMPTY1)(SWIZZLE_SEQ(state))\
		};
	GENERATE_SWIZZLES()
#	undef SWIZZLES_INNER_LOOP_MACRO
#	undef GENERATE_OPERATOR
//#	define GENERATE_SWIZZLE_CLASS(swizzle_seq) \
//			class CSwizzle<SWIZZLE_VECTOR(swizzle_seq)>\
//			{\
//			};
//		GENERATE_SWIZZLES(GENERATE_SWIZZLE_CLASS)
//#	undef GENERATE_SWIZZLE_CLASS

	template<typename ElementType>
	class CVectorDataContainer<ElementType, DIMENSION>
	{
	public:
//#	define TEST(r, state) BOOST_PP_TUPLE_ELEM(4, 0, state)(BOOST_PP_TUPLE_ELEM(4, 1, state))
//#define FUNC(arg) static const int i = arg;
//#define ARG 420
//		TEST(0, (FUNC, ARG, , ))
//		static const int i =
//#define PRED1(d, state) BOOST_PP_LESS(state, 64)
//#define OP1(d, state) BOOST_PP_INC(state)
//#define PRED2(d, state) BOOST_PP_LESS(BOOST_PP_TUPLE_ELEM(2, 0, state), 64)
//#define OP2(d, state) (BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2, 0, state)), BOOST_PP_WHILE(PRED1, OP1, 0))
//			BOOST_PP_TUPLE_ELEM(2, 0, BOOST_PP_WHILE(PRED2, OP2, (0, 0)));
		union
		{
			CVectorData<ElementType, DIMENSION> _vectorData;
			// gcc does not allow class definition inside anonymous union
#			define SWIZZLES_INNER_LOOP_MACRO(r, state) CSwizzle<ElementType, DIMENSION, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_VECTOR(state)> TRANSFORM_SWIZZLE(XYZW, SWIZZLE_SEQ(state)), TRANSFORM_SWIZZLE(RGBA, SWIZZLE_SEQ(state));
			GENERATE_SWIZZLES()
#			undef SWIZZLES_INNER_LOOP_MACRO
//#			define GENERATE_SWIZZLE_OBJECT(swizzle_seq) CSwizzle<SWIZZLE_VECTOR(swizzle_seq)> TRANSFORM_SWIZZLE(XYZW, swizzle_seq), TRANSFORM_SWIZZLE(RGBA, swizzle_seq);
//			GENERATE_SWIZZLES(GENERATE_SWIZZLE_OBJECT)
//#			undef GENERATE_SWIZZLE_OBJECT
//#define MACRO1(z, i, data) data
//#define MACRO2(z, i, data) BOOST_PP_REPEAT(64, MACRO1, data)
//#define MACRO3(z, i, data) BOOST_PP_REPEAT(64, MACRO2, data)
//			BOOST_PP_REPEAT(64, MACRO3, ;)
		};
	};

#	undef DIMENSION
#	undef EMPTY1
#	undef XYZW
#	undef RGBA
#	undef GENERATE_ZERO_SEQ_PRED
#	undef GENERATE_ZERO_SEQ_OP
#	undef GENERATE_ZERO_SEQ
#	undef IS_SEQ_ZERO_OP
#	undef IS_SEQ_ZERO
#	undef IS_SEQ_UNIQUE_PRED
#	undef IS_SEQ_ELEM_UNIQUE
#	undef IS_SEQ_UNIQUE_OP
#	undef IS_SEQ_UNIQUE
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
#	undef SWIZZLES_OP
#	undef SWIZZLES_MACRO
#	undef SWIZZLES
#	undef GENERATE_SWIZZLES
//#	undef SWIZZLE_CLASSNAME
#	undef SWIZZLE_SEQ
//#	undef SWIZZLE_VECTOR
#	undef PACK_SWIZZLE_PRED
#	undef PACK_SWIZZLE_OP
#	undef PACK_SWIZZLE
#	undef GET_SWIZZLE_ELEMENT
#	undef GET_SWIZZLE_ELEMENT_SEQ
#	undef GET_SWIZZLE_ELEMENT_PACKED
//#	undef GET_SWIZZLE_ELEMENT_VECTOR
#	undef GENERATE_SCALAR_OPERATION_PRED
#	undef GENERATE_SCALAR_OPERATION_OP
#	undef GENERATE_SCALAR_OPERATION_MACRO
#else
#	ifndef __VECTOR_MATH_H__
#	define __VECTOR_MATH_H__

#	include "C++11 dummies.h"
#	include <crtdbg.h>
#	if defined _MSC_VER & _MSC_VER <= 1600
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
#	include <boost\preprocessor\iteration\iterate.hpp>
#	include <boost\preprocessor\repetition\repeat_from_to.hpp>
#	include <boost\preprocessor\repetition\for.hpp>
#	include <boost\preprocessor\control\while.hpp>
#	include <boost\preprocessor\control\iif.hpp>
#	include <boost\preprocessor\arithmetic\inc.hpp>
#	include <boost\preprocessor\arithmetic\dec.hpp>
#	include <boost\preprocessor\arithmetic\add.hpp>
#	include <boost\preprocessor\arithmetic\mod.hpp>
#	include <boost\preprocessor\comparison\equal.hpp>
#	include <boost\preprocessor\comparison\not_equal.hpp>
#	include <boost\preprocessor\comparison\less.hpp>
#	include <boost\preprocessor\logical\and.hpp>
#	include <boost\preprocessor\logical\bitand.hpp>
#	include <boost\preprocessor\logical\bitor.hpp>
#	include <boost\preprocessor\logical\not.hpp>
#	include <boost\preprocessor\tuple\elem.hpp>
#	include <boost\preprocessor\tuple\to_seq.hpp>
#	include <boost\preprocessor\tuple\rem.hpp>
//#	include <boost\preprocessor\array\size.hpp>
//#	include <boost\preprocessor\array\data.hpp>
//#	include <boost\preprocessor\array\elem.hpp>
//#	include <boost\preprocessor\array\replace.hpp>
//#	include <boost\preprocessor\array\push_back.hpp>
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

	namespace DGLE2
	{
		namespace VectorMath
		{
			namespace mpl = boost::mpl;

			template<typename, unsigned int>
			class vector;

			template<typename ElementType, unsigned int vectorDimension, unsigned short packedSwizzle, class CSwizzleVector>
			class CSwizzle;

			template<typename ElementType, unsigned int dimension>
			class CVectorData
			{
				friend class vector<ElementType, dimension>;
				template<typename, unsigned int, unsigned short, class>
				friend class CSwizzle;
				//CVectorData(const CVectorData &) = default;
				//~CVectorData() = default;
				ElementType _data[dimension];
			};

			// generic vector
			template<typename ElementType, unsigned int dimension>
			class CVectorDataContainer
			{
			protected:
				CVectorData<ElementType, dimension> _vectorData;
				//CVectorDataContainer(const CVectorDataContainer &) = default;
				//~CVectorDataContainer() = default;
			};

			// specialisations for graphics vectors
#			define BOOST_PP_ITERATION_LIMITS (1, 4)
#			define BOOST_PP_FILENAME_1 "vector math.h"
#			include BOOST_PP_ITERATE()

#			pragma region(generate operators)
				template<typename LeftElementType, unsigned int leftVectorDimension, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, typename RightElementType, unsigned int rightVectorDimension, unsigned short rightPackedSwizzle, class CRightSwizzleVector>
				inline CSwizzle<LeftElementType, leftVectorDimension, leftPackedSwizzle, CLeftSwizzleVector> &operator +=(CSwizzle<LeftElementType, leftVectorDimension, leftPackedSwizzle, CLeftSwizzleVector> &left, const CSwizzle<RightElementType, rightVectorDimension, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					static_assert(mpl::size<typename mpl::unique<typename mpl::sort<CLeftSwizzleVector>::type, std::is_same<mpl::_, mpl::_>>::type>::value == mpl::size<CLeftSwizzleVector>::value, "operator +=: invalid write mask");
					static_assert(mpl::size<CLeftSwizzleVector>::type::value <= mpl::size<CRightSwizzleVector>::type::value, "operator +=: too few components in src");
					for (unsigned int i = 0; i < mpl::size<CLeftSwizzleVector>::type::value; i++)
						left[i] += right[i];
					return left;
				};

				template<typename LeftElementType, unsigned int leftVectorDimension, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, typename RightElementType, unsigned int rightVectorDimension, unsigned short rightPackedSwizzle, class CRightSwizzleVector>
				inline vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), mpl::min<mpl::size<CLeftSwizzleVector>, mpl::size<CRightSwizzleVector>>::type::value> operator +(const CSwizzle<LeftElementType, leftVectorDimension, leftPackedSwizzle, CLeftSwizzleVector> &left, const CSwizzle<RightElementType, rightVectorDimension, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					return vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), mpl::min<mpl::size<CLeftSwizzleVector>, mpl::size<CRightSwizzleVector>>::type::value>();// += right;
				};
#			pragma endregion

			template<typename ElementType, unsigned int dimension>
			class vector: public CVectorDataContainer<ElementType, dimension>
			{
				static_assert(dimension > 0, "vector dimension must be positive");
				//using CVectorDataContainer<ElementType, dimension>::_vectorData;
			public:
				ElementType &operator [](unsigned int idx) noexcept
				{
					_ASSERTE(idx < dimension);
					return CVectorDataContainer<ElementType, dimension>::_vectorData._data[idx];
				}
				ElementType operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < dimension);
					return CVectorDataContainer<ElementType, dimension>::_vectorData._data[idx];
				}
			};
#	pragma region(temp test)
			template class vector<float, 4>;
#	pragma endregion
		}
	}

	#endif//__VECTOR_MATH_H__
#endif