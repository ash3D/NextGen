/**
\author		Alexey Shaydurov aka ASH
\date		20.03.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "CompilerCheck.h"
#include <array>
#include <vector>
#include <initializer_list>
#include <utility>
#include <type_traits>
#include <tuple>
#define DISABLE_MATRIX_SWIZZLES
#include "vector math.h"

namespace Math::Splines
{
	/*
		Scalar args currently passed by const reference.
		For common types (float, double) pass by value can be more efficient
		but many of functions are inlined so there should not be performance difference.
		For more complicated scalar types (complex or user defined high precision types for example)
		pass by const reference can be more efficient.
		Similar approach used in STL (functions like min/max always accepts refs regardless of type complexity).
	*/

	/*
		Direct bezier evaluation currently used as it requires less operations
		(especially for high dimensions) (if binomial coefficient being computed at compile time).
		But De Casteljau’s subdivision used for tessellation as it provides more reliable
		“sufficiently close to being a straight line” test then midpoint test avaliable at both methods.
	*/

#pragma region CompositePoint
	template<class Pos, class ...Attribs>
	struct CompositePoint
	{
		Pos pos;
		std::tuple<Attribs...> attribs;

	private:
		struct AddAssign;
		struct SubAssign;
		struct MulAssign;
		struct DivAssign;

	private:
		// op point
		template<class Functor, size_t ...idx>
		inline auto OpPoint(std::index_sequence<idx...>) const
#if defined _MSC_VER && _MSC_VER <= 1900
			-> CompositePoint<Pos, Attribs...>;
#else
			-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(pos))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(attribs)))>...>;
#endif

		// point op point
		template<class Functor, size_t ...idx, class RightPos, class ...RightAttribs>
		static inline auto PointOpPoint(std::index_sequence<idx...>, const CompositePoint<Pos, Attribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right)
#if defined _MSC_VER && _MSC_VER <= 1900
			-> CompositePoint<Pos, Attribs...>;
#else
			-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left.pos, right.pos))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(left.attribs), std::get<idx>(right.attribs)))>...>;
#endif

		// point op scalar
		template<class Functor, size_t ...idx, typename Scalar>
		static inline auto PointOpScalar(std::index_sequence<idx...>, const CompositePoint<Pos, Attribs...> &left, const Scalar &right)
#if defined _MSC_VER && _MSC_VER <= 1900
			-> CompositePoint<Pos, Attribs...>;
#else
			-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left.pos, right))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(left.attribs), right))>...>;
#endif

		// scalar op point
		template<class Functor, size_t ...idx, typename Scalar>
		static inline auto ScalarOpPoint(std::index_sequence<idx...>, const Scalar &left, const CompositePoint<Pos, Attribs...> &right)
#if defined _MSC_VER && _MSC_VER <= 1900
			-> CompositePoint<Pos, Attribs...>;
#else
			-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left, right.pos))>, std::decay_t<decltype(std::declval<Functor>()(left, std::get<idx>(right.attribs)))>...>;
#endif

		// point op= point
		template<class Functor, size_t idx = 0, class SrcPos, class ...SrcAttribs>
#if defined _MSC_VER && _MSC_VER <= 1900
		inline CompositePoint &PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src, std::true_type = std::true_type());
#else
		inline std::enable_if_t<idx < sizeof...(Attribs), CompositePoint &> PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src);
#endif

		template<class Functor, size_t idx = 0, class SrcPos, class ...SrcAttribs>
#if defined _MSC_VER && _MSC_VER <= 1900
		inline CompositePoint &PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src, std::false_type);
#else
		inline std::enable_if_t<idx == sizeof...(Attribs), CompositePoint &> PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src);
#endif

		// point op= scalar
		template<class Functor, size_t idx = 0, typename Scalar>
#if defined _MSC_VER && _MSC_VER <= 1900
		inline CompositePoint &PointOpScalar(const Scalar &src, std::true_type = std::true_type());
#else
		inline std::enable_if_t<idx < sizeof...(Attribs), CompositePoint &> PointOpScalar(const Scalar &src);
#endif

		template<class Functor, size_t idx = 0, typename Scalar>
#if defined _MSC_VER && _MSC_VER <= 1900
		inline CompositePoint &PointOpScalar(const Scalar &src, std::false_type);
#else
		inline std::enable_if_t<idx == sizeof...(Attribs), CompositePoint &> PointOpScalar(const Scalar &src);
#endif

	public:
		CompositePoint() = default;

		template<class SrcPos, class ...SrcAttribs>
		CompositePoint(SrcPos &&pos, SrcAttribs &&...attribs);

		template<class SrcPos, class ...SrcAttribs>
		CompositePoint(const CompositePoint<SrcPos, SrcAttribs...> &src);

		template<class SrcPos, class ...SrcAttribs>
		CompositePoint(CompositePoint<SrcPos, SrcAttribs...> &&src);

	public:
		template<class SrcPos, class ...SrcAttribs>
		CompositePoint &operator =(const CompositePoint<SrcPos, SrcAttribs...> &src);

		template<class SrcPos, class ...SrcAttribs>
		CompositePoint &operator =(CompositePoint<SrcPos, SrcAttribs...> &&src);

	public:
		const CompositePoint &operator +() const { return *this; }
		auto operator -() const;

	public:
		// point += point
		template<class SrcPos, class ...SrcAttribs>
		CompositePoint &operator +=(const CompositePoint<SrcPos, SrcAttribs...> &src);

		// point -= point
		template<class SrcPos, class ...SrcAttribs>
		CompositePoint &operator -=(const CompositePoint<SrcPos, SrcAttribs...> &src);

		// point *= scalar
		template<typename Scalar>
		CompositePoint &operator *=(const Scalar &src);

		// point /= scalar
		template<typename Scalar>
		CompositePoint &operator /=(const Scalar &src);

		// point + point
		template<class LeftPos, class ...LeftAttribs, class RightPos, class ...RightAttribs>
		friend auto operator +(const CompositePoint<LeftPos, LeftAttribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right);

		// point - point
		template<class LeftPos, class ...LeftAttribs, class RightPos, class ...RightAttribs>
		friend auto operator -(const CompositePoint<LeftPos, LeftAttribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right);

		// point * scalar
		template<class SrcPos, class ...SrcAttribs, typename Scalar>
		friend auto operator *(const CompositePoint<SrcPos, SrcAttribs...> &left, const Scalar &right);

		// scalar * point
		template<class SrcPos, class ...SrcAttribs, typename Scalar>
		friend auto operator *(const Scalar &left, const CompositePoint<SrcPos, SrcAttribs...> &right);

		// point / scalar
		template<class SrcPos, class ...SrcAttribs, typename Scalar>
		friend auto operator /(const CompositePoint<SrcPos, SrcAttribs...> &left, const Scalar &right);
	};

	template<class Point>
	inline decltype(auto) GetPos(const Point &point)
	{
		return point.pos;
	}

	template<typename ScalarType, unsigned int dimension>
	inline decltype(auto) GetPos(const VectorMath::vector<ScalarType, dimension> &point)
	{
		return point;
	}
#pragma endregion

	template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
	class CBezier
	{
		static_assert(!std::is_integral_v<ScalarType>, "integral types for bezier control points is not allowed");

	public:
		typedef std::array<
			std::conditional_t<sizeof...(Attribs) == 0,
				VectorMath::vector<ScalarType, dimension>,
				CompositePoint<VectorMath::vector<ScalarType, dimension>, Attribs...>>,
			degree + 1> ControlPoints;

	private:
		template<size_t ...idx>
		inline CBezier(const typename ControlPoints::value_type(&controlPoints)[degree + 1], std::index_sequence<idx...>);

	public:
		CBezier(const typename ControlPoints::value_type (&controlPoints)[degree + 1]);
		CBezier(const ControlPoints &controlPoints);
		CBezier(ControlPoints &&controlPoints);

		template<class ...Points>
		CBezier(Points &&...controlPoints);

	public:
		typename ControlPoints::value_type operator ()(ScalarType u) const;

		template<typename Iterator>
		void Tessellate(Iterator output, ScalarType delta, bool emitFirstPoint = true) const;

	private:
#if !(defined _MSC_VER && _MSC_VER <= 1900)
		template<size_t ...idx>
		typename ControlPoints::value_type operator ()(ScalarType u, std::index_sequence<idx...>) const;
#endif

		template<typename Iterator, size_t ...idx>
		static void Subdiv(Iterator output, ScalarType delta, const ControlPoints &controlPoints, std::index_sequence<idx...> = std::make_index_sequence<degree >= 1 ? degree - 1 : 0>());

	private:
		ControlPoints controlPoints;
	};

	namespace Impl
	{
		template<typename ScalarType, unsigned int dimension, class ...Attribs>
		class CBezierInterpolationBase
		{
			static_assert(!std::is_integral_v<ScalarType>, "integral types for spline interpolation points is not allowed");

		protected:
			typedef CBezier<ScalarType, dimension, 3, Attribs...> Bezier;

		public:
			typedef typename Bezier::ControlPoints::value_type Point;
		};

		template<template<typename ScalarType, unsigned int dimension, class ...Attribs> class CBezierInterpolationImpl, typename ScalarType, unsigned int dimension, class ...Attribs>
		class CBezierInterpolationCommon : public CBezierInterpolationImpl<ScalarType, dimension, Attribs...>
		{
#if defined _MSC_VER && _MSC_VER <= 1900
#if 0
			typedef CBezierInterpolationImpl<ScalarType, dimension, Attribs...> Base;

		protected:
			using Base::Base;
#else
		public:
			template<typename Iterator>
			CBezierInterpolationCommon(Iterator begin, Iterator end) :
				CBezierInterpolationImpl<ScalarType, dimension, Attribs...>(begin, end) {}

			CBezierInterpolationCommon(std::initializer_list<typename CBezierInterpolationImpl<ScalarType, dimension, Attribs...>::Point> points) :
				CBezierInterpolationImpl<ScalarType, dimension, Attribs...>(points) {}
#endif
#else
			using CBezierInterpolationImpl<ScalarType, dimension, Attribs...>::CBezierInterpolationImpl;
#endif

		public:
			template<typename Iterator>
			void Tessellate(Iterator output, ScalarType delta) const;
		};
	}

	namespace Impl
	{
		template<typename ScalarType, unsigned int dimension, class ...Attribs>
		class CCatmullRom : public CBezierInterpolationBase<ScalarType, dimension, Attribs...>
		{
			using typename CBezierInterpolationBase<ScalarType, dimension, Attribs...>::Bezier;

		public:
			using typename CBezierInterpolationBase<ScalarType, dimension, Attribs...>::Point;

			// use public rather than protected in order to make it accessible when using inheriting ctor
		public:
			template<typename Iterator>
			CCatmullRom(Iterator begin, Iterator end);
			CCatmullRom(std::initializer_list<Point> points);

		public:
			Point operator ()(ScalarType u) const;

		protected:
			typedef std::vector<Point> Points;

		protected:
			Bezier Segment(typename Points::size_type i) const;

		protected:
			Points points;
		};
	}

	template<typename ScalarType, unsigned int dimension, class ...Attribs>
	class CCatmullRom : public Impl::CBezierInterpolationCommon<Impl::CCatmullRom, ScalarType, dimension, Attribs...>
	{
		typedef Impl::CBezierInterpolationCommon<Impl::CCatmullRom, ScalarType, dimension, Attribs...> Base;

	public:
		using typename Base::Point;

	public:
		using Base::Base;
	};

	namespace Impl
	{
		template<typename ScalarType, unsigned int dimension, class ...Attribs>
		class CBesselOverhauser : public CBezierInterpolationBase<ScalarType, dimension, Attribs...>
		{
			using typename CBezierInterpolationBase<ScalarType, dimension, Attribs...>::Bezier;

		public:
			using typename CBezierInterpolationBase<ScalarType, dimension, Attribs...>::Point;

			// use public rather than protected in order to make it accessible when using inheriting ctor
		public:
			template<typename Iterator>
			CBesselOverhauser(Iterator begin, Iterator end);
			CBesselOverhauser(std::initializer_list<Point> points);

		private:
			template<typename Iterator>
			void Init(Iterator begin, Iterator end);

		public:
			Point operator ()(ScalarType u) const;

		protected:
			typedef std::vector<std::pair<ScalarType, Point>> Points;

		protected:
			Bezier Segment(typename Points::size_type i) const;

		protected:
			Points points;
		};
	}

	template<typename ScalarType, unsigned int dimension, class ...Attribs>
	class CBesselOverhauser : public Impl::CBezierInterpolationCommon<Impl::CBesselOverhauser, ScalarType, dimension, Attribs...>
	{
		typedef Impl::CBezierInterpolationCommon<Impl::CBesselOverhauser, ScalarType, dimension, Attribs...> Base;

	public:
		using typename Base::Point;

	public:
		using Base::Base;
	};
}

#include "splines.inl"