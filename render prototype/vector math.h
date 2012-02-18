/**
\author		Alexey Shaydurov aka ASH
\date		18.2.2012 (c)Alexey Shaydurov

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
#	/*define SWIZZLES_INNER_LOOP_MACRO(r, state)	\
		class										\
		{											\
		} BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(4, 1, state)), BOOST_PP_ARRAY_DATA(BOOST_PP_TUPLE_ELEM(4, 1, state)))));*/
#	define TRANSFORM_SWIZZLE(NAMING_SET, swizzle_seq) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, NAMING_SET, swizzle_seq))
//#	define SWIZZLE(state) BOOST_PP_SEQ_CAT(BOOST_PP_SEQ_TRANSFORM(IDX_2_SYMBOL, BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state)))
//#	define APPLY_CALLBACK(callback, arg) callback(arg)
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) APPLY_CALLBACK(BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state) BOOST_PP_TUPLE_ELEM(4, 0, state)(BOOST_PP_TUPLE_ELEM(4, 1, state))
//#	define SWIZZLES_INNER_LOOP_MACRO(r, state)	\
//		class BOOST_PP_CAT(C, SWIZZLE(state))	\
//		{										\
//		} SWIZZLE(state);
#	define SWIZZLES_INTERMIDIATE_LOOP_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_INTERMIDIATE_LOOP_MACRO(r, state) BOOST_PP_FOR_##r((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INNER_LOOP_OP, SWIZZLES_INNER_LOOP_MACRO)
#	define SWIZZLES_OP(r, state) (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, BOOST_PP_WHILE(SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, (BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)))), BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(4, 2, state)), BOOST_PP_TUPLE_ELEM(4, 3, state))
#	define SWIZZLES_MACRO(r, state) BOOST_PP_FOR_##r((BOOST_PP_TUPLE_ELEM(4, 0, state), BOOST_PP_TUPLE_ELEM(4, 1, state), 0, BOOST_PP_TUPLE_ELEM(4, 3, state)), SWIZZLES_INNER_LOOP_PRED, SWIZZLES_INTERMIDIATE_LOOP_OP, SWIZZLES_INTERMIDIATE_LOOP_MACRO)
#	define SWIZZLES(z, i, CALLBACK)											\
		SWIZZLES_INNER_LOOP_MACRO(z, (CALLBACK, GENERATE_ZERO_SEQ(i), , ))	\
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
#	define PACK_SWIZZLE_OP(d, state) (BOOST_PP_TUPLE_ELEM(3, 0, state) + (BOOST_PP_SEQ_HEAD(BOOST_PP_TUPLE_ELEM(3, 2, state)) << BOOST_PP_TUPLE_ELEM(3, 1, state)), BOOST_PP_ADD_D(d, BOOST_PP_TUPLE_ELEM(3, 1, state), 4), BOOST_PP_SEQ_TAIL(BOOST_PP_TUPLE_ELEM(3, 2, state)))
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
#	define GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq)																									\
		template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector>	\
		CSwizzle &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)							\
		{																																						\
			/*static_assert(mpl::size<mpl::unique<mpl::sort<SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>::type, std::is_same<mpl::_, mpl::_>>::type>::value == mpl::size<SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>::value, "!");*/\
			/*static_assert(BOOST_PP_SEQ_SIZE(leftSwizzleSeq) <= TSwizzleTraits<rightColumns, CRightSwizzleVector>::TDimension::value, "operator =: too small src dimension");*/\
			/*BOOST_PP_FOR((0, BOOST_PP_SEQ_SIZE(leftSwizzleSeq), leftSwizzleSeq, CRightSwizzleVector), GENERATE_SCALAR_OPERATION_PRED, GENERATE_SCALAR_OPERATION_OP, GENERATE_SCALAR_OPERATION_MACRO)*/\
			CSwizzleAssign<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>::operator =(right);					\
			return *this;																																		\
		}
#	define GENERATE_COPY_ASSIGN_OPERATOR(leftSwizzleSeq)																						\
		CSwizzle &operator =(const CSwizzle &right)																								\
		{																																		\
			/*return operator =<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>(right);*/		\
			CSwizzleAssign<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>::operator =(right);	\
			return *this;																														\
		}
#	define GENERATE_SCALAR_ASSIGN_OPERATOR(leftSwizzleSeq)																						\
		CSwizzle &operator =(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar)	\
		{																																		\
			CSwizzleAssign<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>::operator =(scalar);	\
			return *this;																														\
		}
#	define GENERATE_INIT_LIST_ASSIGGN_OPERATOR(leftSwizzleSeq)																						\
		CSwizzle &operator =(std::initializer_list<CInitListItem<ElementType>> initList)															\
		{																																			\
			CSwizzleAssign<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(leftSwizzleSeq), SWIZZLE_SEQ_2_VECTOR(leftSwizzleSeq)>::operator =(initList);	\
			return *this;																															\
		}
#	ifdef MSVC_LIMITATIONS
		// VS 2010 does not allow operator = in union members
#		define GENERATE_ASSIGN_OPERATORS(leftSwizzleSeq)		\
			GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq)	\
			GENERATE_SCALAR_ASSIGN_OPERATOR(leftSwizzleSeq)		\
			GENERATE_INIT_LIST_ASSIGGN_OPERATOR(leftSwizzleSeq)
#		define DISABLE_ASSIGN_OPERATOR(leftSwizzleSeq) /*private: CSwizzle &operator =(const CSwizzle &);*/
#		define SWIZZLE_TRIVIAL_CTORS_DTOR
#	else
#		define GENERATE_ASSIGN_OPERATORS(leftSwizzleSeq)		\
			GENERATE_TEMPLATED_ASSIGN_OPERATOR(leftSwizzleSeq)	\
			GENERATE_COPY_ASSIGN_OPERATOR(leftSwizzleSeq)		\
			GENERATE_SCALAR_ASSIGN_OPERATOR(leftSwizzleSeq)		\
			GENERATE_INIT_LIST_ASSIGGN_OPERATOR(leftSwizzleSeq)
#		define DISABLE_ASSIGN_OPERATOR(leftSwizzleSeq) CSwizzle &operator =(const CSwizzle &) = delete;
#		define SWIZZLE_TRIVIAL_CTORS_DTOR			\
			CSwizzle() = default;					\
			CSwizzle(const CSwizzle &) = delete;	\
			~CSwizzle() = default;
#	endif
#ifdef MSVC_LIMITATIONS
#	define SWIZZLES_INNER_LOOP_MACRO(r, state)																																							\
		template<typename ElementType>																																									\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), false, 1>:																\
		public BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), CSwizzleAssign, CSwizzleCommon)<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
		{																																																\
		public:																																															\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))																		\
		protected:																																														\
			SWIZZLE_TRIVIAL_CTORS_DTOR																																									\
		};
	GENERATE_SWIZZLES()
#	define SWIZZLES_INNER_LOOP_MACRO(r, state)																																							\
		template<typename ElementType>																																									\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), false, 2>:																\
		public BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), CSwizzleAssign, CSwizzleCommon)<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
		{																																																\
		public:																																															\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))																		\
		protected:																																														\
			SWIZZLE_TRIVIAL_CTORS_DTOR																																									\
		};
	GENERATE_SWIZZLES()
#	define SWIZZLES_INNER_LOOP_MACRO(r, state)																																							\
		template<typename ElementType>																																									\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), true, 1>:																\
		public BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), CSwizzleAssign, CSwizzleCommon)<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
		{																																																\
		public:																																															\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))																		\
		protected:																																														\
			SWIZZLE_TRIVIAL_CTORS_DTOR																																									\
		};
	GENERATE_SWIZZLES()
#	define SWIZZLES_INNER_LOOP_MACRO(r, state)																																							\
		template<typename ElementType>																																									\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), true, 2>:																\
		public BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), CSwizzleAssign, CSwizzleCommon)<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
		{																																																\
		public:																																															\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))																		\
		protected:																																														\
			SWIZZLE_TRIVIAL_CTORS_DTOR																																									\
		};
	GENERATE_SWIZZLES()
#else
#	define SWIZZLES_INNER_LOOP_MACRO(r, state)																																							\
		template<typename ElementType>																																									\
		class CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>:																			\
		public BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), CSwizzleAssign, CSwizzleCommon)<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
		{																																																\
		public:																																															\
			BOOST_PP_IIF(IS_SEQ_UNIQUE(SWIZZLE_SEQ(state)), GENERATE_ASSIGN_OPERATORS, DISABLE_ASSIGN_OPERATOR)(SWIZZLE_SEQ(state))																		\
		protected:																																														\
			SWIZZLE_TRIVIAL_CTORS_DTOR																																									\
		};
	GENERATE_SWIZZLES()
#endif

#	pragma region(generate typedefs)
		// tuple: (C++, hlsl, glsl)
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
#			define GLSL_CLASS(scalar_types_mapping) BOOST_PP_CAT(BOOST_PP_CAT(BOOST_PP_CAT(GLSL_SCALAR_TYPE(scalar_types_mapping), mat), ROWS), COLUMNS)
#		endif

#		define GENERATE_TYPEDEF(r, graphics_class, elem) typedef CPP_CLASS(elem) graphics_class(elem);

		namespace hlsl
		{
			BOOST_PP_SEQ_FOR_EACH(GENERATE_TYPEDEF, HLSL_CLASS, SCALAR_TYPES_MAPPINGS)
		}

		namespace glsl
		{
			BOOST_PP_SEQ_FOR_EACH(GENERATE_TYPEDEF, GLSL_CLASS, SCALAR_TYPES_MAPPINGS)
		}

#		undef SCALAR_TYPES_MAPPINGS
#		undef CPP_SCALAR_TYPE
#		undef HLSL_SCALAR_TYPE
#		undef GLSL_SCALAR_TYPE
#		undef CPP_CLASS
#		undef HLSL_CLASS
#		undef GLSL_CLASS
#		undef GENERATE_TYPEDEF
#	pragma endregion

#	undef SWIZZLES_INNER_LOOP_MACRO
#	undef GENERATE_TEMPLATED_ASSIGN_OPERATOR
#	undef GENERATE_COPY_ASSIGN_OPERATOR
#	undef GENERATE_INIT_LIST_ASSIGGN_OPERATOR
#	undef GENERATE_ASSIGN_OPERATORS
#	undef DISABLE_ASSIGN_OPERATOR
#	undef SWIZZLE_TRIVIAL_CTORS_DTOR
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
		// forward ctors/dtor/= to _data
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
			_data = right._data;
			return *this;
		}
	public:
		union
		{
			CData<ElementType, ROWS, COLUMNS> _data;
			// gcc does not allow class definition inside anonymous union
#			define NAMING_SET_1 BOOST_PP_IF(ROWS, MATRIX_ZERO_BASED, XYZW)
#			define NAMING_SET_2 BOOST_PP_IF(ROWS, MATRIX_ONE_BASED, RGBA)
#ifdef MSVC_LIMITATIONS
#			define SWIZZLES_INNER_LOOP_MACRO(r, state)																						\
				CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), false, 1>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_1, SWIZZLE_SEQ(state)), );
			GENERATE_SWIZZLES()
#			define SWIZZLES_INNER_LOOP_MACRO(r, state)																						\
				CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), false, 2>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_2, SWIZZLE_SEQ(state)), );
			GENERATE_SWIZZLES()
#			define SWIZZLES_INNER_LOOP_MACRO(r, state)																						\
				CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), true, 1>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_1, SWIZZLE_SEQ(state)), _);
			GENERATE_SWIZZLES()
#			define SWIZZLES_INNER_LOOP_MACRO(r, state)																						\
				CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state)), true, 2>	\
					BOOST_PP_CAT(TRANSFORM_SWIZZLE(NAMING_SET_2, SWIZZLE_SEQ(state)), _);
			GENERATE_SWIZZLES()
#else
#			define SWIZZLES_INNER_LOOP_MACRO(r, state)																				\
				CSwizzle<ElementType, ROWS, COLUMNS, PACK_SWIZZLE(SWIZZLE_SEQ(state)), SWIZZLE_SEQ_2_VECTOR(SWIZZLE_SEQ(state))>	\
					TRANSFORM_SWIZZLE(NAMING_SET_1, SWIZZLE_SEQ(state)),															\
					TRANSFORM_SWIZZLE(NAMING_SET_2, SWIZZLE_SEQ(state));
			GENERATE_SWIZZLES()
#endif
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
#	include <initializer_list>
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
#	include <boost\mpl\at.hpp>
#	include <boost\mpl\min_max.hpp>
#	include <boost\mpl\sort.hpp>
#	include <boost\mpl\size.hpp>
#	include <boost\mpl\unique.hpp>
#	include <boost\mpl\equal_to.hpp>
#	include <boost\mpl\not.hpp>
#	include <boost\mpl\identity.hpp>
#	include <boost\mpl\integral_c.hpp>

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

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd = false, unsigned namingSet = 1>
			class CSwizzleCommon;

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd = false, unsigned namingSet = 1>
			class CSwizzleAssign;

			// rows = 0 for vectors
			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd = false, unsigned namingSet = 1>
			class CSwizzle;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CDataContainer;

			template<typename ElementType>
			class CInitListItem;

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class CData final
			{
				friend class matrix<ElementType, rows, columns>;

				template<typename, unsigned int, unsigned int, unsigned short, class, bool, unsigned>
				friend class CSwizzleCommon;

				friend class CDataContainer<ElementType, rows, columns>;
#		ifndef MSVC_LIMITATIONS
			private:
				CData() = default;
				CData(const CData &) = default;
				~CData() = default;
				CData &operator =(const CData &) = default;
#		endif
			private:
#		ifdef MSVC_LIMITATIONS
				ElementType _rows[rows][columns];
#		else
				vector<ElementType, columns> _rows[rows];
#		endif
			};

			template<typename ElementType, unsigned int dimension>
			class CData<ElementType, 0, dimension> final
			{
				friend class vector<ElementType, dimension>;

				template<typename, unsigned int, unsigned int, unsigned short, class, bool, unsigned>
				friend class CSwizzleCommon;

				friend class CDataContainer<ElementType, 0, dimension>;
#		ifndef MSVC_LIMITATIONS
			private:
				CData() = default;
				CData(const CData &) = default;
				~CData() = default;
				CData &operator =(const CData &) = default;
#		endif
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

			template<unsigned int vectorDimension, class CSwizzleVector>
			struct TSwizzleTraits
			{
			private:
				typedef mpl::not_<std::is_void<CSwizzleVector>> TIsSwizzle;
			public:
				typedef typename std::conditional<TIsSwizzle::value, mpl::size<CSwizzleVector>, mpl::integral_c<unsigned, vectorDimension>>::type TDimension;
			private:
				typedef typename std::conditional<TIsSwizzle::value, mpl::sort<CSwizzleVector>, mpl::identity<void>>::type::type CSortedSwizzleVector;
				typedef typename std::conditional<TIsSwizzle::value, mpl::unique<CSortedSwizzleVector, std::is_same<mpl::_, mpl::_>>, mpl::identity<void>>::type::type CUniqueSwizzleVector;
			public:
				typedef typename std::conditional<TIsSwizzle::value, mpl::equal_to<mpl::size<CUniqueSwizzleVector>, mpl::size<CSwizzleVector>>, std::true_type>::type TIsWriteMaskValid;
			};

			// specializations for graphics vectors/matrices
#			define BOOST_PP_ITERATION_LIMITS (0, 0)
#			define BOOST_PP_FILENAME_1 "vector math.h"
#			include BOOST_PP_ITERATE()

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd = false, unsigned namingSet = 1>
			class CSwizzleBase
			{
				/*
						  static
							 ^
							 |
				CSwizzleBase -> CSwizzle
				*/
				typedef CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet> TSwizzle;
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
			};

			// CSwizzle inherits from this to reduce preprocessor generated code for faster compiling
			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
			class CSwizzleCommon: public CSwizzleBase<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet>
			{
				typedef CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet> TSwizzle;
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
			public:
				const ElementType &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE((idx < TSwizzleTraits<columns, CSwizzleVector>::TDimension::value));
					idx = packedSwizzle >> (idx << 2u) & (1u << 4u) - 1u;
					auto row = idx >> 2 & 3u, column = idx & 3u;
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
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<ElementType &>(static_cast<const CSwizzleCommon &>(*this)[idx]);
				}
			};

			// specialization for vectors
			template<typename ElementType, unsigned int vectorDimension, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
			class CSwizzleCommon<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector, odd, namingSet>: public CSwizzleBase<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector, odd, namingSet>
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
				typedef CSwizzle<ElementType, 0, vectorDimension, packedSwizzle, CSwizzleVector, odd, namingSet> TSwizzle;
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
			public:
				const ElementType &operator [](unsigned int idx) const noexcept
				{
					_ASSERTE((idx < TSwizzleTraits<vectorDimension, CSwizzleVector>::TDimension::value));
					idx = packedSwizzle >> (idx << 2u) & (1u << 4u) - 1u;
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
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<ElementType &>(static_cast<const CSwizzleCommon &>(*this)[idx]);
				}
			};

			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzleCommon<ElementType, 0, vectorDimension, 0u, void>: public CSwizzleBase<ElementType, 0, vectorDimension, 0u, void>
			{
				typedef vector<ElementType, vectorDimension> Tvector;
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleCommon() = default;
				CSwizzleCommon(const CSwizzleCommon &) = default;
				~CSwizzleCommon() = default;
				CSwizzleCommon &operator =(const CSwizzleCommon &) = default;
#endif
			public:
				typedef Tvector TOperationResult;
			public:
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

				operator Tvector &() noexcept
				{
					/* const version returns (const &), not value; *this object is not const => it is safe to use const_cast */
					return const_cast<Tvector &>(operator const Tvector &());
				}

				const ElementType &operator [](unsigned int idx) const noexcept
				{
					return operator const Tvector &()[idx];
				}

				ElementType &operator [](unsigned int idx) noexcept
				{
					return operator Tvector &()[idx];
				}
			};

			template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
			class CSwizzleAssign: public CSwizzleCommon<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet>
			{
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzleAssign() = default;
				CSwizzleAssign(const CSwizzleAssign &) = default;
				~CSwizzleAssign() = default;
#endif

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet>
				void operator =(const CSwizzleAssign<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector> &right)
				{
					typedef TSwizzleTraits<columns, CSwizzleVector> TLeftSwizzleTraits;
					typedef TSwizzleTraits<rightColumns, CRightSwizzleVector> TRightSwizzleTraits;
					static_assert(TLeftSwizzleTraits::TDimension::value <= TRightSwizzleTraits::TDimension::value, "operator =: too small src dimension");
					vector<RightElementType, TRightSwizzleTraits::TDimension::value> right_copy(static_cast<const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &>(right));
					for (unsigned idx = 0; idx < TLeftSwizzleTraits::TDimension::value; idx++)
					{
						(*this)[idx] = right_copy[idx];
					}
				}

#ifndef MSVC_LIMITATIONS
				/*
				it is not necessary to create copy of 'right' here (because there is no swizzles)
				forwarding to templated operator = shorter (it is commented out below) but it leads to unnecessary 'right' copy
				*/
				void operator =(const CSwizzleAssign &right)
				{
					typedef TSwizzleTraits<columns, CSwizzleVector> TLeftSwizzleTraits;
					for (unsigned idx = 0; idx < TLeftSwizzleTraits::TDimension::value; idx++)
					{
						(*this)[idx] = right[idx];
					}
					//operator =<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet>(right);
				}
#endif

				void operator =(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar)
				{
					typedef TSwizzleTraits<columns, CSwizzleVector> TLeftSwizzleTraits;
					for (unsigned idx = 0; idx < TLeftSwizzleTraits::TDimension::value; idx++)
					{
						(*this)[idx] = scalar;
					}
				}

				void operator =(std::initializer_list<CInitListItem<ElementType>> initList)
				{
					unsigned dst_idx = 0;
#ifdef MSVC_LIMITATIONS
					for (auto iter = initList.begin(), end = initList.end(); iter != end; ++iter)
						for (unsigned item_element_idx = 0; item_element_idx < iter->GetItemSize(); item_element_idx++)
							(*this)[dst_idx++] = iter->operator [](item_element_idx);
#else
					for (const auto &item: initList)
						for (unsigned item_element_idx = 0; item_element_idx < item.GetItemSize(); item_element_idx++)
							(*this)[dst_idx++] = item[item_element_idx];
#endif
					_ASSERTE(dst_idx == columns);
				}
			};

			// this specialization used as base class for CDataContainer to eliminate need for various overloads
			template<typename ElementType, unsigned int vectorDimension>
			class CSwizzle<ElementType, 0, vectorDimension, 0u, void>: public CSwizzleAssign<ElementType, 0, vectorDimension, 0u, void>
			{
			protected:
#ifndef MSVC_LIMITATIONS
				CSwizzle() = default;
				CSwizzle(const CSwizzle &) = default;
				~CSwizzle() = default;
				CSwizzle &operator =(const CSwizzle &) = default;
#endif
			};

#			pragma region(generate operators)
				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
				inline const typename CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet>::TOperationResult &operator +(const CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
				inline typename CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet>::TOperationResult &operator +(CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet> &src) noexcept
				{
					return src;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector, bool odd, unsigned namingSet>
				inline vector<ElementType, columns> operator -(const CSwizzle<ElementType, rows, columns, packedSwizzle, CSwizzleVector, odd, namingSet> &src)
				{
					typedef TSwizzleTraits<columns, CSwizzleVector> TSwizzleTraits;
					vector<ElementType, columns> result;
					for (typename TSwizzleTraits::TDimension::value_type i = 0; i < TSwizzleTraits::TDimension::value; i++)
						result[i] = -src[i];
					return result;
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet
				>
				inline typename CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector, leftOdd, leftNamingSet>::TOperationResult &operator +=(
				CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right)
				{
					typedef TSwizzleTraits<leftColumns, CLeftSwizzleVector> TLeftSwizzleTraits;
					typedef TSwizzleTraits<rightColumns, CRightSwizzleVector> TRightSwizzleTraits;
					static_assert(TLeftSwizzleTraits::TIsWriteMaskValid::value, "operator +=: invalid write mask");
					static_assert(TLeftSwizzleTraits::TDimension::value <= TRightSwizzleTraits::TDimension::value, "operator +=: too small src dimension");
					vector<RightElementType, TRightSwizzleTraits::TDimension::value> right_copy(right);
					for (typename TLeftSwizzleTraits::TDimension::value_type i = 0; i < TLeftSwizzleTraits::TDimension::value; i++)
						left[i] += right_copy[i];
					return left;
				};

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet
				>
				inline vector
				<
					decltype(declval<LeftElementType>() + declval<RightElementType>()),
					mpl::min
					<
						typename TSwizzleTraits<leftColumns, CLeftSwizzleVector>::TDimension,
						typename TSwizzleTraits<rightColumns, CRightSwizzleVector>::TDimension
					>::type::value
				> operator +(
				const CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right)
				{
					vector
					<
						decltype(declval<LeftElementType>() + declval<RightElementType>()),
						mpl::min
						<
							typename TSwizzleTraits<leftColumns, CLeftSwizzleVector>::TDimension,
							typename TSwizzleTraits<rightColumns, CRightSwizzleVector>::TDimension
						>::type::value
					> result(left);
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
				vector(): CDataContainer() {}
#				else
				vector() = default;
#				endif

				vector(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar);

				template<typename TIterator>
				explicit vector(TIterator src);

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, unsigned short srcPackedSwizzle, class CSrcSwizzleVector, bool srcOdd, unsigned srcNamingSet>
				vector(const CSwizzle<SrcElementType, srcRows, srcColumns, srcPackedSwizzle, CSrcSwizzleVector, srcOdd, srcNamingSet> &src);

				vector(std::initializer_list<CInitListItem<ElementType>> initList);

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet>
				vector &operator =(const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right);

				vector &operator =(std::initializer_list<CInitListItem<ElementType>> initList);

				const ElementType &operator [](unsigned int idx) const noexcept;

				ElementType &operator [](unsigned int idx) noexcept;

				template<typename F>
				vector apply(F f);
			};

			template<typename ElementType, unsigned int rows, unsigned int columns>
			class matrix: public CDataContainer<ElementType, rows, columns>
			{
				static_assert(rows > 0, "matrix should contain at leat 1 row");
				static_assert(columns > 0, "matrix should contain at leat 1 column");
				typedef vector<ElementType, columns> TRow;
			public:
#				ifdef MSVC_LIMITATIONS
				matrix(): CDataContainer() {}
#				else
				matrix() = default;
#				endif

				matrix(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar);

				template<typename TIterator>
				explicit matrix(TIterator src);

				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns>
				matrix(const matrix<SrcElementType, srcRows, srcColumns> &src);

				matrix(std::initializer_list<CInitListItem<ElementType>> initList);

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				matrix &operator =(const matrix<RightElementType, rightRows, rightColumns> &right);

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

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				matrix &operator +=(const matrix<RightElementType, rightRows, rightColumns> &right);

				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				matrix
				<
					decltype(declval<ElementType>() + declval<RightElementType>()),
					mpl::min<mpl::integral_c<unsigned, rows>, mpl::integral_c<unsigned, rightRows>>::type::value,
					mpl::min<mpl::integral_c<unsigned, columns>, mpl::integral_c<unsigned, rightColumns>>::type::value
				> operator +(const matrix<RightElementType, rightRows, rightColumns> &right) const;

				const TRow &operator [](unsigned int idx) const noexcept;

				TRow &operator [](unsigned int idx) noexcept;

				template<typename F>
				matrix apply(F f);
			};

#			pragma region(Initializer list)
				template<typename TargetElementType, typename ItemElementType>
				TargetElementType GetItemElement(const void *item, unsigned)
				{
					return *reinterpret_cast<const ItemElementType *>(item);
				}

				/*
				no need to track odd, namingSet because for different odd, namingSet CSwizzle differs only by type name; layout is same
				only operator [] used here
				*/
				template<typename TargetElementType, typename ItemElementType, unsigned int rows, unsigned int columns, unsigned short packedSwizzle, class CSwizzleVector>
				TargetElementType GetItemElement(const void *item, unsigned idx)
				{
					const auto &swizzle = *reinterpret_cast<const CSwizzle<ItemElementType, rows, columns, packedSwizzle, CSwizzleVector> *>(item);
					return swizzle[idx];
				}

				template<typename TargetElementType, typename ItemElementType, unsigned int rows, unsigned int columns>
				TargetElementType GetItemElement(const void *item, unsigned idx)
				{
					const auto &m = *reinterpret_cast<const matrix<ItemElementType, rows, columns> *>(item);
					return m[idx / columns][idx % columns];
				}

				template<typename ElementType>
				class CInitListItem final
				{
#ifndef MSVC_LIMITATIONS
					CInitListItem() = delete;
					CInitListItem(const CInitListItem &) = delete;
					CInitListItem &operator =(const CInitListItem &) = delete;
#endif
				public:
					CInitListItem(const ElementType &item):
						_getItemElement(GetItemElement<ElementType, ElementType>),
						_item(&item),
						_itemSize(1)
					{
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns, unsigned short itemPackedSwizzle, class CItemSwizzleVector>
					CInitListItem(const CSwizzle<ItemElementType, itemRows, itemColumns, itemPackedSwizzle, CItemSwizzleVector> &item) noexcept:
						_getItemElement(GetItemElement<ElementType, ItemElementType, itemRows, itemColumns, itemPackedSwizzle, CItemSwizzleVector>),
						_item(&item),
						_itemSize(TSwizzleTraits<itemColumns, CItemSwizzleVector>::TDimension::value)
					{
					}

					template<typename ItemElementType, unsigned int itemRows, unsigned int itemColumns>
					CInitListItem(const matrix<ItemElementType, itemRows, itemColumns> &item) noexcept:
						_getItemElement(GetItemElement<ElementType, ItemElementType, itemRows, itemColumns>),
						_item(&item),
						_itemSize(itemRows * itemColumns)
					{
					}

					ElementType operator [](unsigned idx) const
					{
						return _getItemElement(_item, idx);
					}

					unsigned GetItemSize() const noexcept
					{
						return _itemSize;
					}
				private:
					ElementType (&_getItemElement)(const void *, unsigned);
					const void *const _item;
					const unsigned _itemSize;
				};
#			pragma endregion
#			pragma region(vector impl)
				template<typename ElementType, unsigned int dimension>
				inline const ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) const noexcept
				{
					_ASSERTE(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}

				template<typename ElementType, unsigned int dimension>
				inline ElementType &vector<ElementType, dimension>::operator [](unsigned int idx) noexcept
				{
					_ASSERTE(idx < dimension);
					return CDataContainer<ElementType, 0, dimension>::_data._data[idx];
				}

				template<typename ElementType, unsigned int dimension>
				inline vector<ElementType, dimension>::vector(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar)
				{
					std::fill_n(CDataContainer<ElementType, 0, dimension>::_data._data, dimension, scalar);
				}

				template<typename ElementType, unsigned int dimension>
				template<typename TIterator>
				inline vector<ElementType, dimension>::vector(TIterator src)
				{
					std::copy_n(src, dimension, CDataContainer<ElementType, 0, dimension>::_data._data);
				}

				template<typename ElementType, unsigned int dimension>
				template<typename SrcElementType, unsigned int srcRows, unsigned int srcColumns, unsigned short srcPackedSwizzle, class CSrcSwizzleVector, bool srcOdd, unsigned srcNamingSet>
				inline vector<ElementType, dimension>::vector(const CSwizzle<SrcElementType, srcRows, srcColumns, srcPackedSwizzle, CSrcSwizzleVector, srcOdd, srcNamingSet> &src)
				{
					typedef TSwizzleTraits<srcColumns, CSrcSwizzleVector> TSrcSwizzleTraits;
					static_assert(dimension <= TSrcSwizzleTraits::TDimension::value, "\"copy\" ctor: too small src dimension");
					for (unsigned i = 0; i < dimension; i++)
					{
						operator [](i).~ElementType();
						new(&operator [](i)) ElementType(src[i]);
					}
				}

				template<typename ElementType, unsigned int dimension>
				inline auto vector<ElementType, dimension>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> vector &
				{
					CSwizzleAssign<ElementType, 0, dimension, 0u, void>::operator =(initList);
					return *this;
				}

				template<typename ElementType, unsigned int dimension>
				inline vector<ElementType, dimension>::vector(std::initializer_list<CInitListItem<ElementType>> initList)
				{
					operator =(initList);
				}

				template<typename ElementType, unsigned int dimension>
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet>
				inline auto vector<ElementType, dimension>::operator =(const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right) -> vector &
				{
					CSwizzle<ElementType, 0, dimension, 0u, void>::operator =(right);
					return *this;
				}

				template<typename ElementType, unsigned int dimension>
				template<typename F>
				inline auto vector<ElementType, dimension>::apply(F f) -> vector
				{
					vector result;
					for (unsigned i = 0; i < dimension; i++)
						result[i] = f(operator [](i));
					return result;
				}
#			pragma endregion
#			pragma region(matrix impl)
				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) const noexcept -> const typename matrix::TRow &
				{
					_ASSERTE(idx < rows);
#				ifdef MSVC_LIMITATIONS
					return reinterpret_cast<const TRow &>(CDataContainer<ElementType, rows, columns>::_data._rows[idx]);
#				else
					return CDataContainer<ElementType, rows, columns>::_data._rows[idx];
#				endif
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				inline auto matrix<ElementType, rows, columns>::operator [](unsigned int idx) noexcept -> typename matrix::TRow &
				{
					_ASSERTE(idx < rows);
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
				inline matrix<ElementType, rows, columns>::matrix(typename std::conditional<sizeof(ElementType) <= sizeof(void *), ElementType, const ElementType &>::type scalar)
				{
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					{
						auto curRow = &operator [](rowIdx);
						curRow->~TRow();
						new(curRow) TRow(scalar);
					}
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename TIterator>
				inline matrix<ElementType, rows, columns>::matrix(TIterator src)
				{
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
					{
						auto curRow = &operator [](rowIdx);
						curRow->~TRow();
						new(curRow) TRow(*src);
						src += columns;
					}
				}

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
				inline auto matrix<ElementType, rows, columns>::operator =(std::initializer_list<CInitListItem<ElementType>> initList) -> matrix &
				{
					unsigned dst_idx = 0;
#ifdef MSVC_LIMITATIONS
					for (auto iter = initList.begin(), end = initList.end(); iter != end; ++iter)
						for (unsigned item_element_idx = 0; item_element_idx < iter->GetItemSize(); item_element_idx++, dst_idx++)
							(*this)[dst_idx / columns][dst_idx % columns] = iter->operator [](item_element_idx);
#else
					for (const auto &item: initList)
						for (unsigned item_element_idx = 0; item_element_idx < item.GetItemSize(); item_element_idx++, dst_idx++)
							(*this)[dst_idx / columns][dst_idx % columns] = item[item_element_idx];
#endif
					_ASSERTE(dst_idx == rows * columns);
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
				inline auto matrix<ElementType, rows, columns>::operator -() const -> matrix
				{
					matrix result;
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
						result[rowIdx] = -operator [](rowIdx);
					return result;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				inline auto matrix<ElementType, rows, columns>::operator +=(const matrix<RightElementType, rightRows, rightColumns> &right) -> matrix &
				{
					static_assert(rows <= rightRows, "operator +=: too few rows in src");
					static_assert(columns <= rightColumns, "operator +=: too few columns in src");
					for (unsigned rowIdx = 0; rowIdx < rows; rowIdx++)
						operator [](rowIdx) += right[rowIdx];
					return *this;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename RightElementType, unsigned int rightRows, unsigned int rightColumns>
				inline matrix
				<
					decltype(declval<ElementType>() + declval<RightElementType>()),
					mpl::min<mpl::integral_c<unsigned, rows>, mpl::integral_c<unsigned, rightRows>>::type::value,
					mpl::min<mpl::integral_c<unsigned, columns>, mpl::integral_c<unsigned, rightColumns>>::type::value
				> matrix<ElementType, rows, columns>::operator +(const matrix<RightElementType, rightRows, rightColumns> &right) const
				{
					matrix
					<
						decltype(declval<ElementType>() + declval<RightElementType>()),
						mpl::min<mpl::integral_c<unsigned, rows>, mpl::integral_c<unsigned, rightRows>>::type::value,
						mpl::min<mpl::integral_c<unsigned, columns>, mpl::integral_c<unsigned, rightColumns>>::type::value
					>
					result(*this);
					return result += right;
				}

				template<typename ElementType, unsigned int rows, unsigned int columns>
				template<typename F>
				inline auto matrix<ElementType, rows, columns>::apply(F f) -> matrix
				{
					matrix result;
					for (unsigned i = 0; i < rows; i++)
						result[i].apply(f);
					return result;
				}
#			pragma endregion
#			pragma region(mul functions)
				// note: most of these functions are not inline

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet
				>
				inline decltype(declval<LeftElementType>() + declval<RightElementType>()) mul(
				const CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector, leftOdd, leftNamingSet> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right)
				{
					constexpr unsigned rowXcolumnDimension = mpl::min
					<
						typename TSwizzleTraits<leftColumns, CLeftSwizzleVector>::TDimension,
						typename TSwizzleTraits<rightColumns, CRightSwizzleVector>::TDimension
					>::type::value;

					decltype(declval<LeftElementType>() + declval<RightElementType>()) result(0);

					for (unsigned i = 0; i < rowXcolumnDimension; i++)
						result += left[i] * right[i];

					return result;
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns, unsigned short leftPackedSwizzle, class CLeftSwizzleVector, bool leftOdd, unsigned leftNamingSet,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns
				>
				vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), rightColumns> mul(
				const CSwizzle<LeftElementType, leftRows, leftColumns, leftPackedSwizzle, CLeftSwizzleVector, leftOdd, leftNamingSet> &left,
				const matrix<RightElementType, rightRows, rightColumns> &right)
				{
					constexpr unsigned
						resultColumns = rightColumns,
						rowXcolumnDimension = mpl::min
						<
							typename TSwizzleTraits<leftColumns, CLeftSwizzleVector>::TDimension,
							mpl::integral_c<unsigned, rightRows>
						>::type::value;
					typedef decltype(declval<LeftElementType>() + declval<RightElementType>()) ElementType;

					vector<ElementType, resultColumns> result(ElementType(0));

					for (unsigned c = 0; c < resultColumns; c++)
						for (unsigned i = 0; i < rowXcolumnDimension; i++)
							result[c] += left[i] * right[i][c];

					return result;
				}

				template
				<
					typename LeftElementType, unsigned int leftRows, unsigned int leftColumns,
					typename RightElementType, unsigned int rightRows, unsigned int rightColumns, unsigned short rightPackedSwizzle, class CRightSwizzleVector, bool rightOdd, unsigned rightNamingSet
				>
				vector<decltype(declval<LeftElementType>() + declval<RightElementType>()), leftRows> mul(
				const matrix<LeftElementType, leftRows, leftColumns> &left,
				const CSwizzle<RightElementType, rightRows, rightColumns, rightPackedSwizzle, CRightSwizzleVector, rightOdd, rightNamingSet> &right)
				{
					constexpr unsigned
						resultRows = leftRows,
						rowXcolumnDimension = mpl::min
						<
							mpl::integral_c<unsigned, leftColumns>,
							typename TSwizzleTraits<rightColumns, CRightSwizzleVector>::TDimension
						>::type::value;
					typedef decltype(declval<LeftElementType>() + declval<RightElementType>()) ElementType;

					vector<ElementType, resultRows> result(ElementType(0));

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
				matrix<decltype(declval<LeftElementType>() + declval<RightElementType>()), leftRows, rightColumns> mul(
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
					typedef decltype(declval<LeftElementType>() + declval<RightElementType>()) ElementType;

					matrix<ElementType, resultRows, resultColumns> result(ElementType(0));

					for (unsigned r = 0; r < resultRows; r++)
						for (unsigned c = 0; c < resultColumns; c++)
							for (unsigned i = 0; i < rowXcolumnDimension; i++)
								result[r][c] += left[r][i] * right[i][c];

					return result;
				}
#			pragma endregion
		}
	}
//#	undef GET_SWIZZLE_ELEMENT
//#	undef GET_SWIZZLE_ELEMENT_PACKED

	#endif//__VECTOR_MATH_H__
#endif