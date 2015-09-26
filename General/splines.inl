/**
\author		Alexey Shaydurov aka ASH
\date		27.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "splines.h"
#include <algorithm>
#include <iterator>
#include <type_traits>

template<typename ScalarType, unsigned int dimension, unsigned int degree>
Math::Splines::CBezier<ScalarType, dimension, degree>::CBezier(const TPoint (&controlPoints)[degree + 1])
{
	std::copy_n(controlPoints, std::extent<typename std::remove_reference<decltype(controlPoints)>::type>::value, _controlPoints);
}

template<typename ScalarType, unsigned int dimension, unsigned int degree>
template<unsigned idx>
inline void Math::Splines::CBezier<ScalarType, dimension, degree>::_Init()
{
	constexpr auto count = std::extent<typename std::remove_reference<decltype(_controlPoints)>::type>::value;
	static_assert(idx >= count, "too few control point");
	static_assert(idx <= count, "too many control point");
}

template<typename ScalarType, unsigned int dimension, unsigned int degree>
template<unsigned idx, class CCurPoint, class ...CRestPoints>
inline void Math::Splines::CBezier<ScalarType, dimension, degree>::_Init(const CCurPoint &curPoint, const CRestPoints &...restPoints)
{
	static_assert(std::is_convertible<CCurPoint, TPoint>::value, "invalid control point type");
	_controlPoints[idx] = curPoint;
	_Init<idx + 1>(restPoints...);
}

template<typename ScalarType, unsigned int dimension, unsigned int degree>
template<class ...CPoints>
Math::Splines::CBezier<ScalarType, dimension, degree>::CBezier(const CPoints &...controlPoints)
{
	_Init<0>(controlPoints...);
}

/*
direct bezier evaluation currently used as it requires less operations
(especially for high dimensions) (if binomial coefficient computed at compile time)
but De Casteljau’s subdivision used for tessellation as it provides more reliable
“sufficiently close to being a straight line” test then midpoint test avaliable at both methods
*/

// consider using variadic template to unroll loop
template<typename ScalarType, unsigned int dimension, unsigned int degree>
auto Math::Splines::CBezier<ScalarType, dimension, degree>::operator ()(ScalarType u) const -> TPoint
{
	ScalarType factor1 = 1, factors2[degree + 1];
	factors2[degree] = 1;
	for (signed i = degree - 1; i >= 0; i--)
		factors2[i] = factors2[i + 1] * (1 - u);
	TPoint result = 0;
	for (unsigned i = 0; i <= degree; i++, factor1 *= u)
		result += boost::math::binomial_coefficient<unsigned int>(degree, i) * factor1 * factors2[i] * _controlPoints[i];
	return result;
}

template<typename ScalarType, unsigned int dimension, unsigned int degree>
template<typename Iterator>
void Math::Splines::CBezier<ScalarType, dimension, degree>::Tessellate(Iterator output, ScalarType delta, bool emitFirstPoint) const
{
	if (emitFirstPoint) *output++ = _controlPoints[0];
	_Subdiv(output, delta, _controlPoints);
}

template<typename ScalarType, unsigned int dimension, unsigned int degree>
template<typename Iterator>
void Math::Splines::CBezier<ScalarType, dimension, degree>::_Subdiv(Iterator output, ScalarType delta, const TPoint controlPoints[degree + 1])
{
	// TODO: move to Math
	const auto point_line_dist = [](const TPoint &point, const TPoint &lineBegin, const TPoint &lineEnd) -> ScalarType
	{
		const TPoint point_dir = point - lineBegin, line_dir = lineEnd - lineBegin;
		// project point_dir on line_dir
		const TPoint proj = dot(point_dir, line_dir) / dot(line_dir, line_dir) * line_dir;
		return length(point_dir - proj);
	};

	const auto stop_test = [&point_line_dist, &controlPoints, delta]() -> bool
	{
		for (unsigned i = 1; i < degree; i++)
		{
			if (point_line_dist(controlPoints[i], controlPoints[0], controlPoints[degree]) > delta)
				return false;
		}
		return true;
	};

	if (stop_test())
		*output++ = (controlPoints[degree]);
	else
	{
		constexpr auto intermediate_points_count = (degree + 1) * 2 - 1;
		TPoint intermediate_points[intermediate_points_count];	// will hold control points for 2 subdivided curves (with 1 common point)
		for (unsigned i = 0; i <= degree; i++)
			intermediate_points[i * 2] = controlPoints[i];
		for (unsigned insert_idx_begin = 1, insert_idx_end = intermediate_points_count; insert_idx_begin <= degree; insert_idx_begin++, insert_idx_end--)
		{
			for (unsigned insert_idx = insert_idx_begin; insert_idx < insert_idx_end; insert_idx += 2)
				intermediate_points[insert_idx] = lerp(intermediate_points[insert_idx - 1], intermediate_points[insert_idx + 1], ScalarType(.5));
		}
		_Subdiv(output, delta, intermediate_points + 0);
		_Subdiv(output, delta, intermediate_points + degree);
	}
}

template<typename ScalarType, unsigned int dimension, template<typename ScalarType, unsigned int dimension> class CBezierInterpolationImpl>
template<typename Iterator>
void Math::Splines::CBezierInterpolationCommon<ScalarType, dimension, CBezierInterpolationImpl>::Tessellate(Iterator output, ScalarType delta) const
{
	for (TPoints::size_type i = 1; i < _points.size() - 2; i++)
		_Segment(i).Tessellate(output, delta, i == 1);
}

template<typename ScalarType, unsigned int dimension>
template<typename Iterator>
Math::Splines::CCatmullRomImpl<ScalarType, dimension>::CCatmullRomImpl(Iterator begin, Iterator end):
_points(begin, end)
{
	_ASSERT(_points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension>
Math::Splines::CCatmullRomImpl<ScalarType, dimension>::CCatmullRomImpl(std::initializer_list<TPoint> points):
_points(points)
{
	_ASSERT(_points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension>
auto Math::Splines::CCatmullRomImpl<ScalarType, dimension>::operator ()(ScalarType u) const -> TPoint
{
	_ASSERTE(u >= 0 && u <= 1);
	// [0..1]->[0..m-2]->[1..m-1]
	u *= _points.size() - 3, u++;
	// ensure i < m
	const TPoints::size_type i = std::min(floor(u), _points.size() - 2);
	return _Segment(i)(u - i);
}

template<typename ScalarType, unsigned int dimension>
auto Math::Splines::CCatmullRomImpl<ScalarType, dimension>::_Segment(typename TPoints::size_type i) const -> TBezier
{
	_ASSERT(i >= 1 && i < _points.size() - 2);
	return TBezier(_points[i], _points[i] + (_points[i + 1] - _points[i - 1]) / 6, _points[i + 1] - (_points[i + 2] - _points[i]) / 6, _points[i + 1]);
}

template<typename ScalarType, unsigned int dimension>
template<typename Iterator>
Math::Splines::CBesselOverhauserImpl<ScalarType, dimension>::CBesselOverhauserImpl(Iterator begin, Iterator end)
{
	Reserve(_points, begin, end);
	_Init(begin, end);
}

template<typename ScalarType, unsigned int dimension>
Math::Splines::CBesselOverhauserImpl<ScalarType, dimension>::CBesselOverhauserImpl(std::initializer_list<TPoint> points)
{
	_points.reserve(points.size());
	_Init(points.begin(), points.end());
}

template<typename ScalarType, unsigned int dimension>
template<typename Iterator>
void Math::Splines::CBesselOverhauserImpl<ScalarType, dimension>::_Init(Iterator begin, Iterator end)
{
	_ASSERTE(begin != end);
	_points.emplace_back(TPoints::value_type(0, *begin));
	transform(std::next(begin), end, std::back_inserter(_points), [this](const TPoint &curPoint)
	{
		/*
		it seems that 'std' namespace used here ('VectorMath::' before 'distance' required)
		TODO: try with other compilers
		*/
		return CBesselOverhauserImpl<ScalarType, dimension>::TPoints::value_type(_points.back().first + VectorMath::distance(_points.back().second, curPoint), curPoint);
	});
	_ASSERT(_points.size() >= 4);
}

template<typename ScalarType, unsigned int dimension>
auto Math::Splines::CBesselOverhauserImpl<ScalarType, dimension>::operator ()(ScalarType u) const -> TPoint
{
	_ASSERTE(u >= 0 && u <= 1);
	// [0..1]->[u_begin..u_end]
	const ScalarType u_begin = std::next(_points.begin())->first, u_end = std::prev(_points.end(), 2)->first;
	u_end - u_begin;
	u *= u_end - u_begin, u += u_begin;
	// ensure u in [u_begin..u_end]
	u = std::min(std::max(u, u_begin), u_end);
	struct
	{
		bool operator ()(ScalarType left, TPoints::const_reference right) const
		{
			return left < right.first;
		}
		bool operator ()(TPoints::const_reference left, ScalarType right) const
		{
			return left.first < right;
		}
	} point_order;
	auto p1 = std::lower_bound(_points.begin(), _points.end(), u, point_order);
	if (p1 == std::next(_points.begin())) ++p1;
	const auto p0 = std::prev(p1);
	return _Segment(std::distance(_points.begin(), p0))((u - p0->first) / (p1->first - p0->first));
}

template<typename ScalarType, unsigned int dimension>
auto Math::Splines::CBesselOverhauserImpl<ScalarType, dimension>::_Segment(typename TPoints::size_type i) const -> TBezier
{
	_ASSERT(i >= 1 && i < _points.size() - 2);
	const auto v_segment = [this](TPoints::size_type j)
	{
		return (_points[j + 1].second - _points[j].second) / (_points[j + 1].first - _points[j].first);
	};
	const TPoint segment_vels[3] = {v_segment(i - 1), v_segment(i), v_segment(i + 1)};
	const auto offset = [this, &segment_vels, i](TPoints::size_type shift)
	{
		return
			(
				(_points[i + 1 + shift].first - _points[i + shift].first) * segment_vels[0 + shift] +
				(_points[i + shift].first - _points[i - 1 + shift].first) * segment_vels[1 + shift]
			) /
			((_points[i + 1 + shift].first - _points[i - 1 + shift].first) * 3) *
			(_points[i + 1].first - _points[i].first);
	};
	return TBezier(_points[i].second, _points[i].second + offset(0), _points[i + 1].second - offset(1), _points[i + 1].second);
}