/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <vector>
#include <initializer_list>
#include <utility>	// for std::pair
#define DISABLE_MATRIX_SWIZZLES
#include "vector math.h"

namespace Math
{
	namespace Splines
	{
		template<typename ScalarType, unsigned int dimension, unsigned int degree>
		class CBezier
		{
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

		template<typename ScalarType, unsigned int dimension>
		class CBezierInterpolationBase
		{
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

		template<typename ScalarType, unsigned int dimension>
		class CCatmullRomImpl: public CBezierInterpolationBase<ScalarType, dimension>
		{
		public:
			using typename CBezierInterpolationBase<ScalarType, dimension>::TPoint;
		protected:
			template<typename Iterator>
			CCatmullRomImpl(Iterator begin, Iterator end);
			CCatmullRomImpl(std::initializer_list<TPoint> points);
		public:
			TPoint operator ()(ScalarType u) const;
		protected:
			typedef std::vector<TPoint> TPoints;
		protected:
			typename CBezierInterpolationBase<ScalarType, dimension>::TBezier _Segment(typename TPoints::size_type i) const;
		protected:
			TPoints _points;
		};

		template<typename ScalarType, unsigned int dimension>
		class CCatmullRom: public CBezierInterpolationCommon<ScalarType, dimension, CCatmullRomImpl>
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
			using typename CBezierInterpolationCommon<ScalarType, dimension, CCatmullRomImpl>::TPoint;
		public:
			// TODO: use C++11 inheriting ctor
			template<typename Iterator>
			CCatmullRom(Iterator begin, Iterator end):
			CBezierInterpolationCommon<ScalarType, dimension, CCatmullRomImpl>(begin, end) {}
			CCatmullRom(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon<ScalarType, dimension, CCatmullRomImpl>(points) {}
#endif
		};

		template<typename ScalarType, unsigned int dimension>
		class CBesselOverhauserImpl: public CBezierInterpolationBase<ScalarType, dimension>
		{
		public:
			using typename CBezierInterpolationBase<ScalarType, dimension>::TPoint;
		protected:
			template<typename Iterator>
			CBesselOverhauserImpl(Iterator begin, Iterator end);
			CBesselOverhauserImpl(std::initializer_list<TPoint> points);
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

		template<typename ScalarType, unsigned int dimension>
		class CBesselOverhauser: public CBezierInterpolationCommon<ScalarType, dimension, CBesselOverhauserImpl>
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
			using typename CBezierInterpolationCommon<ScalarType, dimension, CBesselOverhauserImpl>::TPoint;
		public:
			// TODO: use C++11 inheriting ctor
			template<typename Iterator>
			CBesselOverhauser(Iterator begin, Iterator end):
			CBezierInterpolationCommon<ScalarType, dimension, CBesselOverhauserImpl>(begin, end) {}
			CBesselOverhauser(std::initializer_list<TPoint> points):
			CBezierInterpolationCommon<ScalarType, dimension, CBesselOverhauserImpl>(points) {}
		};
#endif
	}
}

#include "splines.inl"