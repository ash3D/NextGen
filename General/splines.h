/**
\author		Alexey Shaydurov aka ASH
\date		27.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <vector>
#include <initializer_list>
#include <utility>	// for std::pair
#include <type_traits>
#define DISABLE_MATRIX_SWIZZLES
#include "vector math.h"

namespace Math
{
	namespace Splines
	{
		template<typename ScalarType, unsigned int dimension, unsigned int degree>
		class CBezier
		{
			static_assert(!std::is_integral<ScalarType>::value, "integral types for bezier control points is not allowed");
		public:
			typedef VectorMath::vector<ScalarType, dimension> TPoint;
		public:
			CBezier(const TPoint (&controlPoints)[degree + 1]);
			template<class ...CPoints>
			CBezier(const CPoints &...controlPoints);
		public:
			TPoint operator ()(ScalarType u) const;
			template<typename Iterator>
			void Tessellate(Iterator output, ScalarType delta, bool emitFirstPoint = true) const;
		private:
			template<unsigned idx>
			inline void _Init();
			template<unsigned idx, class CCurPoint, class ...CRestPoints>
			inline void _Init(const CCurPoint &curPoint, const CRestPoints &...restPoints);
		private:
			template<typename Iterator>
			static void _Subdiv(Iterator output, ScalarType delta, const TPoint controlPoints[degree + 1]);
		private:
			TPoint _controlPoints[degree + 1];
		};

		namespace Impl
		{
			template<typename ScalarType, unsigned int dimension>
			class CBezierInterpolationBase
			{
				static_assert(!std::is_integral<ScalarType>::value, "integral types for spline interpolation points is not allowed");
			protected:
				typedef CBezier<ScalarType, dimension, 3> TBezier;
			public:
				typedef typename TBezier::TPoint TPoint;
			};

			template<typename ScalarType, unsigned int dimension, template<typename ScalarType, unsigned int dimension> class CBezierInterpolationImpl>
			class CBezierInterpolationCommon: public CBezierInterpolationImpl<ScalarType, dimension>
			{
			protected:
				// TODO: use C++11 inheriting ctor
				template<typename Iterator>
				CBezierInterpolationCommon(Iterator begin, Iterator end):
				CBezierInterpolationImpl<ScalarType, dimension>(begin, end) {}
				CBezierInterpolationCommon(std::initializer_list<typename CBezierInterpolationImpl<ScalarType, dimension>::TPoint> points):
				CBezierInterpolationImpl<ScalarType, dimension>(points) {}
			public:
				template<typename Iterator>
				void Tessellate(Iterator output, ScalarType delta) const;
			};
		}

		namespace Impl
		{
			template<typename ScalarType, unsigned int dimension>
			class CCatmullRom: public CBezierInterpolationBase<ScalarType, dimension>
			{
			public:
				using typename CBezierInterpolationBase<ScalarType, dimension>::TPoint;
			protected:
				template<typename Iterator>
				CCatmullRom(Iterator begin, Iterator end);
				CCatmullRom(std::initializer_list<TPoint> points);
			public:
				TPoint operator ()(ScalarType u) const;
			protected:
				typedef std::vector<TPoint> TPoints;
			protected:
				typename CBezierInterpolationBase<ScalarType, dimension>::TBezier _Segment(typename TPoints::size_type i) const;
			protected:
				TPoints _points;
			};
		}

		template<typename ScalarType, unsigned int dimension>
		class CCatmullRom: public Impl::CBezierInterpolationCommon<ScalarType, dimension, Impl::CCatmullRom>
		{
#ifdef MSVC_LIMITATIONS
		public:
			using typename CBezierInterpolationCommon::TPoint;
		public:
			template<typename Iterator>
			CCatmullRom(Iterator begin, Iterator end):
			CBezierInterpolationCommon(begin, end) {}
			CCatmullRom(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon(points) {}
#else
		public:
			using typename CBezierInterpolationCommon<ScalarType, dimension, Impl::CCatmullRom>::TPoint;
		public:
			// TODO: use C++11 inheriting ctor
			template<typename Iterator>
			CCatmullRom(Iterator begin, Iterator end):
			CBezierInterpolationCommon<ScalarType, dimension, Impl::CCatmullRom>(begin, end) {}
			CCatmullRom(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon<ScalarType, dimension, Impl::CCatmullRom>(points) {}
#endif
		};

		namespace Impl
		{
			template<typename ScalarType, unsigned int dimension>
			class CBesselOverhauser: public CBezierInterpolationBase<ScalarType, dimension>
			{
			public:
				using typename CBezierInterpolationBase<ScalarType, dimension>::TPoint;
			protected:
				template<typename Iterator>
				CBesselOverhauser(Iterator begin, Iterator end);
				CBesselOverhauser(std::initializer_list<TPoint> points);
			private:
				template<typename Iterator>
				void _Init(Iterator begin, Iterator end);
			public:
				TPoint operator ()(ScalarType u) const;
			protected:
				typedef std::vector<std::pair<ScalarType, TPoint>> TPoints;
			protected:
				typename CBezierInterpolationBase<ScalarType, dimension>::TBezier _Segment(typename TPoints::size_type i) const;
			protected:
				TPoints _points;
			};
		}

		template<typename ScalarType, unsigned int dimension>
		class CBesselOverhauser: public Impl::CBezierInterpolationCommon<ScalarType, dimension, Impl::CBesselOverhauser>
		{
#ifdef MSVC_LIMITATIONS
		public:
			using typename CBezierInterpolationCommon::TPoint;
		public:
			// TODO: use C++11 inheriting ctor
			template<typename Iterator>
			CBesselOverhauser(Iterator begin, Iterator end):
			CBezierInterpolationCommon(begin, end) {}
			CBesselOverhauser(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon(points) {}
		};
#else
		public:
			using typename CBezierInterpolationCommon<ScalarType, dimension, Impl::CBesselOverhauser>::TPoint;
		public:
			// TODO: use C++11 inheriting ctor
			template<typename Iterator>
			CBesselOverhauser(Iterator begin, Iterator end):
			CBezierInterpolationCommon<ScalarType, dimension, Impl::CBesselOverhauser>(begin, end) {}
			CBesselOverhauser(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon<ScalarType, dimension, Impl::CBesselOverhauser>(points) {}
		};
#endif
	}
}

#include "splines.inl"