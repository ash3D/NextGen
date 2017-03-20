/**
\author		Alexey Shaydurov aka ASH
\date		20.03.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "splines.h"
#include <algorithm>
#include <iterator>
#include <functional>
#include <cmath>			// for fmin/fmax
#include <cassert>
#if defined _MSC_VER && _MSC_VER <= 1900
#include <boost/math/special_functions/binomial.hpp>
#endif
#include "general math.h"	// for lerp
#include "misc.h"			// for Reserve()

#pragma region CompositePoint
namespace Math::Splines
{
	template<class Pos, class ...Attribs>
	struct CompositePoint<Pos, Attribs...>::AddAssign
	{
		template<typename Dst, typename Src>
		constexpr void operator ()(Dst &dst, const Src &src) const { dst += src; }
	};

	template<class Pos, class ...Attribs>
	struct CompositePoint<Pos, Attribs...>::SubAssign
	{
		template<typename Dst, typename Src>
		constexpr void operator ()(Dst &dst, const Src &src) const { dst -= src; }
	};

	template<class Pos, class ...Attribs>
	struct CompositePoint<Pos, Attribs...>::MulAssign
	{
		template<typename Dst, typename Src>
		constexpr void operator ()(Dst &dst, const Src &src) const { dst *= src; }
	};

	template<class Pos, class ...Attribs>
	struct CompositePoint<Pos, Attribs...>::DivAssign
	{
		template<typename Dst, typename Src>
		constexpr void operator ()(Dst &dst, const Src &src) const { dst /= src; }
	};

	// op point
	template<class Pos, class ...Attribs>
	template<class Functor, size_t ...idx>
	inline auto CompositePoint<Pos, Attribs...>::OpPoint(std::index_sequence<idx...>) const
#if defined _MSC_VER && _MSC_VER <= 1900
		-> CompositePoint<Pos, Attribs...>
#else
		-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(pos))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(attribs)))>...>
#endif
	{
		constexpr Functor op;
		return{ op(pos), op(std::get<idx>(attribs))... };
	}

	// point op point
	template<class Pos, class ...Attribs>
	template<class Functor, size_t ...idx, class RightPos, class ...RightAttribs>
	inline auto CompositePoint<Pos, Attribs...>::PointOpPoint(std::index_sequence<idx...>, const CompositePoint<Pos, Attribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right)
#if defined _MSC_VER && _MSC_VER <= 1900
		-> CompositePoint<Pos, Attribs...>
#else
		-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left.pos, right.pos))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(left.attribs), std::get<idx>(right.attribs)))>...>
#endif
	{
		constexpr Functor op;
		return{ op(left.pos, right.pos), op(std::get<idx>(left.attribs), std::get<idx>(right.attribs))... };
	}

	// point op scalar
	template<class Pos, class ...Attribs>
	template<class Functor, size_t ...idx, typename Scalar>
	inline auto CompositePoint<Pos, Attribs...>::PointOpScalar(std::index_sequence<idx...>, const CompositePoint<Pos, Attribs...> &left, const Scalar &right)
#if defined _MSC_VER && _MSC_VER <= 1900
		-> CompositePoint<Pos, Attribs...>
#else
		-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left.pos, right))>, std::decay_t<decltype(std::declval<Functor>()(std::get<idx>(left.attribs), right))>...>
#endif
	{
		constexpr Functor op;
		return{ op(left.pos, right), op(std::get<idx>(left.attribs), right)... };
	}

	// scalar op point
	template<class Pos, class ...Attribs>
	template<class Functor, size_t ...idx, typename Scalar>
	inline auto CompositePoint<Pos, Attribs...>::ScalarOpPoint(std::index_sequence<idx...>, const Scalar &left, const CompositePoint<Pos, Attribs...> &right)
#if defined _MSC_VER && _MSC_VER <= 1900
		-> CompositePoint<Pos, Attribs...>
#else
		-> CompositePoint<std::decay_t<decltype(std::declval<Functor>()(left, right.pos))>, std::decay_t<decltype(std::declval<Functor>()(left, std::get<idx>(right.attribs)))>...>
#endif
	{
		constexpr Functor op;
		return { op(left, right.pos), op(left, std::get<idx>(right.attribs))... };
	}

	// point op= point
	template<class Pos, class ...Attribs>
	template<class Functor, size_t idx, class SrcPos, class ...SrcAttribs>
#if defined _MSC_VER && _MSC_VER <= 1900
	inline auto CompositePoint<Pos, Attribs...>::PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src, std::true_type) -> CompositePoint &
#else
	inline auto CompositePoint<Pos, Attribs...>::PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src) -> std::enable_if_t<idx < sizeof...(Attribs), CompositePoint &>
#endif
	{
		Functor()(std::get<idx>(attribs), std::get<idx>(src.attribs));
#if defined _MSC_VER && _MSC_VER <= 1900
		constexpr auto nextIdx = idx + 1;
		return PointOpPoint<Functor, nextIdx>(src, std::bool_constant<nextIdx < sizeof...(Attribs)>());
#else
		return PointOpPoint<Functor, idx + 1>(src);
#endif
	}

	template<class Pos, class ...Attribs>
	template<class Functor, size_t idx, class SrcPos, class ...SrcAttribs>
#if defined _MSC_VER && _MSC_VER <= 1900
	inline auto CompositePoint<Pos, Attribs...>::PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src, std::false_type) -> CompositePoint &
#else
	inline auto CompositePoint<Pos, Attribs...>::PointOpPoint(const CompositePoint<SrcPos, SrcAttribs...> &src) -> std::enable_if_t<idx == sizeof...(Attribs), CompositePoint &>
#endif
	{
		Functor()(pos, src.pos);
		return *this;
	}

	// point op= scalar
	template<class Pos, class ...Attribs>
	template<class Functor, size_t idx, typename Scalar>
#if defined _MSC_VER && _MSC_VER <= 1900
	inline auto CompositePoint<Pos, Attribs...>::PointOpScalar(const Scalar &src, std::true_type) -> CompositePoint &
#else
	inline auto CompositePoint<Pos, Attribs...>::PointOpScalar(const Scalar &src) -> std::enable_if_t<idx < sizeof...(Attribs), CompositePoint &>
#endif
	{
		Functor()(std::get<idx>(attribs), src);
#if defined _MSC_VER && _MSC_VER <= 1900
		constexpr auto nextIdx = idx + 1;
		return PointOpScalar<Functor, nextIdx>(src, std::bool_constant<nextIdx < sizeof...(Attribs)>());
#else
		return PointOpScalar<Functor, idx + 1>(src);
#endif
	}

	template<class Pos, class ...Attribs>
	template<class Functor, size_t idx, typename Scalar>
#if defined _MSC_VER && _MSC_VER <= 1900
	inline auto CompositePoint<Pos, Attribs...>::PointOpScalar(const Scalar &src, std::false_type)->CompositePoint &
#else
	inline auto CompositePoint<Pos, Attribs...>::PointOpScalar(const Scalar &src) -> std::enable_if_t<idx == sizeof...(Attribs), CompositePoint &>
#endif
	{
		Functor()(pos, src);
		return *this;
	}

	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline CompositePoint<Pos, Attribs...>::CompositePoint(SrcPos &&pos, SrcAttribs &&...attribs) :
	pos(std::forward<SrcPos>(pos)), attribs(std::forward<SrcAttribs>(attribs)...) {}

	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline CompositePoint<Pos, Attribs...>::CompositePoint(const CompositePoint<SrcPos, SrcAttribs...> &src) :
	pos(src.pos), attribs(src.attribs) {}

	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline CompositePoint<Pos, Attribs...>::CompositePoint(CompositePoint<SrcPos, SrcAttribs...> &&src) :
	pos(std::move(src.pos)), attribs(std::move(src.attribs)) {}

	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline auto CompositePoint<Pos, Attribs...>::operator =(const CompositePoint<SrcPos, SrcAttribs...> &src) -> CompositePoint &
	{
		pos = src.pos;
		attribs = src.attribs;
		return *this;
	}

	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline auto CompositePoint<Pos, Attribs...>::operator =(CompositePoint<SrcPos, SrcAttribs...> &&src) -> CompositePoint &
	{
		pos = std::move(src.pos);
		attribs = std::move(src.attribs);
		return *this;
	}

	template<class Pos, class ...Attribs>
	inline auto CompositePoint<Pos, Attribs...>::operator -() const
	{
		return OpPoint<std::negate<>>(std::index_sequence_for<Attribs...>());
	}

	// point += point
	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline auto CompositePoint<Pos, Attribs...>::operator +=(const CompositePoint<SrcPos, SrcAttribs...> &src) -> CompositePoint &
	{
		return PointOpPoint<AddAssign>(src);
	}

	// point -= point
	template<class Pos, class ...Attribs>
	template<class SrcPos, class ...SrcAttribs>
	inline auto CompositePoint<Pos, Attribs...>::operator -=(const CompositePoint<SrcPos, SrcAttribs...> &src) -> CompositePoint &
	{
		return PointOpPoint<SubAssign>(src);
	}

	// point *= scalar
	template<class Pos, class ...Attribs>
	template<typename Scalar>
	inline auto CompositePoint<Pos, Attribs...>::operator *=(const Scalar &src) -> CompositePoint &
	{
		return PointOpScalar<MulAssign>(src);
	}

	// point /= scalar
	template<class Pos, class ...Attribs>
	template<typename Scalar>
	inline auto CompositePoint<Pos, Attribs...>::operator /=(const Scalar &src) -> CompositePoint &
	{
		return PointOpScalar<DivAssign>(src);
	}

	// point + point
	template<class LeftPos, class ...LeftAttribs, class RightPos, class ...RightAttribs>
	inline auto operator +(const CompositePoint<LeftPos, LeftAttribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right)
	{
		return CompositePoint<LeftPos, LeftAttribs...>::PointOpPoint<std::plus<>>(std::index_sequence_for<LeftAttribs...>(), left, right);
	}

	// point - point
	template<class LeftPos, class ...LeftAttribs, class RightPos, class ...RightAttribs>
	inline auto operator -(const CompositePoint<LeftPos, LeftAttribs...> &left, const CompositePoint<RightPos, RightAttribs...> &right)
	{
		return CompositePoint<LeftPos, LeftAttribs...>::PointOpPoint<std::minus<>>(std::index_sequence_for<LeftAttribs...>(), left, right);
	}

	// point * scalar
	template<class Pos, class ...Attribs, typename Scalar>
	inline auto operator *(const CompositePoint<Pos, Attribs...> &left, const Scalar &right)
	{
		return CompositePoint<Pos, Attribs...>::PointOpScalar<std::multiplies<>>(std::index_sequence_for<Attribs...>(), left, right);
	}

	// scalar * point
	template<class Pos, class ...Attribs, typename Scalar>
	inline auto operator *(const Scalar &left, const CompositePoint<Pos, Attribs...> &right)
	{
		return CompositePoint<Pos, Attribs...>::ScalarOpPoint<std::multiplies<>>(std::index_sequence_for<Attribs...>(), left, right);
	}

	// point / scalar
	template<class Pos, class ...Attribs, typename Scalar>
	inline auto operator /(const CompositePoint<Pos, Attribs...> &left, const Scalar &right)
	{
		return CompositePoint<Pos, Attribs...>::PointOpScalar<std::divides<>>(std::index_sequence_for<Attribs...>(), left, right);
	}
}
#pragma endregion

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
template<size_t ...idx>
inline Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::CBezier(const typename ControlPoints::value_type (&src)[degree + 1], std::index_sequence<idx...>) :
	controlPoints{ typename ControlPoints::value_type(src[idx])... } {}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::CBezier(const typename ControlPoints::value_type (&src)[degree + 1]) :
	CBezier(src, std::make_index_sequence<degree + 1>()) {}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::CBezier(const ControlPoints &controlPoints) :
	controlPoints(controlPoints) {}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::CBezier(ControlPoints &&controlPoints) :
	controlPoints(std::move(controlPoints)) {}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
template<class ...Points>
Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::CBezier(Points &&...controlPoints) :
controlPoints{ std::forward<Points>(controlPoints)... }
{
	constexpr auto cout = degree + 1;
	static_assert(sizeof...(Points) >= cout, "too few control points");
	static_assert(sizeof...(Points) <= cout, "too many control points");
}

#if !(defined _MSC_VER && _MSC_VER <= 1900)
template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
template<size_t ...idx>
inline auto Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::operator ()(ScalarType u, std::index_sequence<idx...>) const -> typename ControlPoints::value_type
{
	static_assert(degree >= 2);
	ScalarType factor1 = u;
	const ScalarType factors2[degree] = { 1 - u, factors2[idx] * factors2[0] ... };
	using Combinatorics::C;
	return C<degree, 0> * factors2[degree - 1] * controlPoints[0] +
		C<degree, 1> * factor1 * factors2[degree - 2] * controlPoints[1] +
		(C<degree, idx + 2> * (factor1 *= u) * factors2[degree - 3 - idx] * controlPoints[2 + idx] + ... + C<degree, degree> * factor1 * u * controlPoints[degree]);
}
#endif

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
auto Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::operator ()(ScalarType u) const -> typename ControlPoints::value_type
{
#if defined _MSC_VER && _MSC_VER <= 1900
	ScalarType factor1 = 1, factors2[degree + 1];
	factors2[degree] = 1;
	for (signed i = degree - 1; i >= 0; i--)
		factors2[i] = factors2[i + 1] * (1 - u);
	auto result = typename ControlPoints::value_type();	// value init
	for (unsigned i = 0; i <= degree; i++, factor1 *= u)
		result += boost::math::binomial_coefficient<ScalarType>(degree, i) * factor1 * factors2[i] * controlPoints[i];
	return result;
#else
	if constexpr (degree == 0)
		return controlPoints[0];
	else if constexpr (degree == 1)
		return lerp(controlPoints[0], controlPoints[1], u);
	else
		return operator ()(u, std::make_index_sequence<degree - 2>());
#endif
}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
template<typename Iterator>
void Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::Tessellate(Iterator output, ScalarType delta, bool emitFirstPoint) const
{
	if (emitFirstPoint) *output++ = controlPoints[0];
	Subdiv(output, delta, controlPoints);
}

template<typename ScalarType, unsigned int dimension, unsigned int degree, class ...Attribs>
template<typename Iterator, size_t ...idx>
void Math::Splines::CBezier<ScalarType, dimension, degree, Attribs...>::Subdiv(Iterator output, ScalarType delta, const ControlPoints &controlPoints, std::index_sequence<idx...>)
{
	// TODO: move to Math
	const auto point_line_dist = [](typename ControlPoints::const_reference point, typename ControlPoints::const_reference lineBegin, typename ControlPoints::const_reference lineEnd) -> ScalarType
	{
		const auto point_dir = GetPos(point) - GetPos(lineBegin), line_dir = GetPos(lineEnd) - GetPos(lineBegin);
		// project point_dir on line_dir
		const auto proj = dot(point_dir, line_dir) / dot(line_dir, line_dir) * line_dir;
		return length(point_dir - proj);
	};

	const auto stop_test = [&point_line_dist, &controlPoints, delta]() -> bool
	{
#if defined _MSC_VER && _MSC_VER <= 1900
		for (unsigned i = 1; i < degree; i++)
		{
			if (point_line_dist(controlPoints[i], controlPoints[0], controlPoints[degree]) > delta)
				return false;
		}
		return true;
#else
		return (point_line_dist(controlPoints[idx + 1], controlPoints[0], controlPoints[degree]) > delta || ...);
#endif
	};

	if (stop_test())
		*output++ = (controlPoints[degree]);
	else
	{
		constexpr auto intermediate_points_count = (degree + 1) * 2 - 1;
		typename ControlPoints::value_type intermediate_points[intermediate_points_count];	// will hold control points for 2 subdivided curves (with 1 common point)
		for (unsigned i = 0; i <= degree; i++)
			intermediate_points[i * 2] = controlPoints[i];
		for (unsigned insert_idx_begin = 1, insert_idx_end = intermediate_points_count; insert_idx_begin <= degree; insert_idx_begin++, insert_idx_end--)
		{
			for (unsigned insert_idx = insert_idx_begin; insert_idx < insert_idx_end; insert_idx += 2)
				intermediate_points[insert_idx] = lerp(intermediate_points[insert_idx - 1], intermediate_points[insert_idx + 1], ScalarType(.5));
		}
		Subdiv(output, delta, intermediate_points + 0);
		Subdiv(output, delta, intermediate_points + degree);
	}
}

template<template<typename ScalarType, unsigned int dimension, class ...Attribs> class CBezierInterpolationImpl, typename ScalarType, unsigned int dimension, class ...Attribs>
template<typename Iterator>
void Math::Splines::Impl::CBezierInterpolationCommon<CBezierInterpolationImpl, ScalarType, dimension, Attribs...>::Tessellate(Iterator output, ScalarType delta) const
{
	for (Points::size_type i = 1; i < points.size() - 2; i++)
		Segment(i).Tessellate(output, delta, i == 1);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
template<typename Iterator>
Math::Splines::Impl::CCatmullRom<ScalarType, dimension, Attribs...>::CCatmullRom(Iterator begin, Iterator end) :
points(begin, end)
{
	assert(points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
Math::Splines::Impl::CCatmullRom<ScalarType, dimension, Attribs...>::CCatmullRom(std::initializer_list<Point> points) :
points(points)
{
	assert(points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
auto Math::Splines::Impl::CCatmullRom<ScalarType, dimension, Attribs...>::operator ()(ScalarType u) const -> Point
{
	//assert(u >= 0 && u <= 1);
	// [0..1]->[0..m-2]->[1..m-1]
	u *= points.size() - 3, u++;
	// ensure 1 <= i < m
	//const Points::size_type i = std::min<Points::size_type>(floor(u), points.size() - 3);
	const ScalarType i = fmin(fmax(floor(u), 1), points.size() - 3);
	return Segment(i)(u - i);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
auto Math::Splines::Impl::CCatmullRom<ScalarType, dimension, Attribs...>::Segment(typename Points::size_type i) const -> Bezier
{
	assert(i >= 1 && i < points.size() - 2);
	return Bezier(points[i], points[i] + (points[i + 1] - points[i - 1]) / 6, points[i + 1] - (points[i + 2] - points[i]) / 6, points[i + 1]);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
template<typename Iterator>
Math::Splines::Impl::CBesselOverhauser<ScalarType, dimension, Attribs...>::CBesselOverhauser(Iterator begin, Iterator end)
{
	Reserve(points, begin, end);
	Init(begin, end);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
Math::Splines::Impl::CBesselOverhauser<ScalarType, dimension, Attribs...>::CBesselOverhauser(std::initializer_list<Point> points)
{
	points.reserve(points.size());
	Init(points.begin(), points.end());
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
template<typename Iterator>
void Math::Splines::Impl::CBesselOverhauser<ScalarType, dimension, Attribs...>::Init(Iterator begin, Iterator end)
{
	assert(begin != end);
	points.emplace_back(Points::value_type(0, *begin));
	transform(std::next(begin), end, std::back_inserter(points), [this](const Point &curPoint)
	{
		/*
			it seems that 'std' namespace used here ('VectorMath::' before 'distance' required)
			TODO: try with other compilers
		*/
		return Points::value_type(points.back().first + VectorMath::distance(GetPos(points.back().second), GetPos(curPoint)), curPoint);
	});
	assert(points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
auto Math::Splines::Impl::CBesselOverhauser<ScalarType, dimension, Attribs...>::operator ()(ScalarType u) const -> Point
{
	//assert(u >= 0 && u <= 1);
	// [0..1]->[u_begin..u_end]
	const ScalarType u_begin = std::next(points.begin())->first, u_end = std::prev(points.end(), 2)->first;
	u *= u_end - u_begin, u += u_begin;
	// ensure u in [u_begin..u_end]
	//u = fmin(fmax(u, u_begin), u_end);
	struct
	{
		bool operator ()(ScalarType left, Points::const_reference right) const
		{
			return left < right.first;
		}
		bool operator ()(Points::const_reference left, ScalarType right) const
		{
			return left.first < right;
		}
	} point_order;
	auto p1 = std::lower_bound(points.begin(), points.end(), u, point_order);
	if (std::distance(points.begin(), p1) < 2)
		p1 = std::next(points.begin(), 2);
	else if (std::distance(p1, points.end()) < 2)
		p1 = std::prev(points.end(), 2);
	const auto p0 = std::prev(p1);
	return Segment(std::distance(points.begin(), p0))((u - p0->first) / (p1->first - p0->first));
}

template<typename ScalarType, unsigned int dimension, class ...Attribs>
auto Math::Splines::Impl::CBesselOverhauser<ScalarType, dimension, Attribs...>::Segment(typename Points::size_type i) const -> Bezier
{
	assert(i >= 1 && i < points.size() - 2);
	const auto v_segment = [this](Points::size_type j)
	{
		return (points[j + 1].second - points[j].second) / (points[j + 1].first - points[j].first);
	};
	const Point segment_vels[3] = {v_segment(i - 1), v_segment(i), v_segment(i + 1)};
	const auto offset = [this, &segment_vels, i](Points::size_type shift)
	{
		return
			(
				(points[i + 1 + shift].first - points[i + shift].first) * segment_vels[0 + shift] +
				(points[i + shift].first - points[i - 1 + shift].first) * segment_vels[1 + shift]
			) /
			((points[i + 1 + shift].first - points[i - 1 + shift].first) * 3) *
			(points[i + 1].first - points[i].first);
	};
	return Bezier(points[i].second, points[i].second + offset(0), points[i + 1].second - offset(1), points[i + 1].second);
}