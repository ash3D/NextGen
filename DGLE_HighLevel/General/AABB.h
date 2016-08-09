/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <limits>
#include <algorithm>
#include "nv_algebra.h"

// AABB class
class AABB
{
public:
	explicit AABB(
		const vec3 &min = vec3(+std::numeric_limits<nv_scalar>::infinity(), +std::numeric_limits<nv_scalar>::infinity(), +std::numeric_limits<nv_scalar>::infinity()),
		const vec3 &max = vec3(-std::numeric_limits<nv_scalar>::infinity(), -std::numeric_limits<nv_scalar>::infinity(), -std::numeric_limits<nv_scalar>::infinity()))
		throw() :
		_min(min), _max(max)
	{
	}

	template<class Iterator>
	AABB(Iterator begin, Iterator end) throw() :
		_min(+std::numeric_limits<nv_scalar>::infinity(), +std::numeric_limits<nv_scalar>::infinity(), +std::numeric_limits<nv_scalar>::infinity()),
		_max(-std::numeric_limits<nv_scalar>::infinity(), -std::numeric_limits<nv_scalar>::infinity(), -std::numeric_limits<nv_scalar>::infinity())
	{
		class MinMax
		{
		public:
			MinMax(vec3 &min, vec3 &max) : _min(min), _max(max) {}
			void operator ()(const vec3 &vertex) const
			{
				if (vertex.x < _min.x)
					_min.x = vertex.x;

				if (vertex.y < _min.y)
					_min.y = vertex.y;

				if (vertex.z < _min.z)
					_min.z = vertex.z;

				if (vertex.x > _max.x)
					_max.x = vertex.x;

				if (vertex.y > _max.y)
					_max.y = vertex.y;

				if (vertex.z > _max.z)
					_max.z = vertex.z;
			}
		private:
			vec3 &_min, &_max;
		};
		std::for_each(begin, end, MinMax(_min, _max));
	}

	AABB &Refit(const AABB &aabb) throw()
	{
		if (aabb._min.x < _min.x)
			_min.x = aabb._min.x;

		if (aabb._min.y < _min.y)
			_min.y = aabb._min.y;

		if (aabb._min.z < _min.z)
			_min.z = aabb._min.z;

		if (aabb._max.x > _max.x)
			_max.x = aabb._max.x;

		if (aabb._max.y > _max.y)
			_max.y = aabb._max.y;

		if (aabb._max.z > _max.z)
			_max.z = aabb._max.z;

		return *this;
	}

	AABB &xform(const mat4 &matrix) throw()
	{
		// create box vertices
		vec3 box[8] =
		{
			vec3(_min.x, _min.y, _min.z),
			vec3(_min.x, _min.y, _max.z),
			vec3(_min.x, _max.y, _min.z),
			vec3(_min.x, _max.y, _max.z),
			vec3(_max.x, _min.y, _min.z),
			vec3(_max.x, _min.y, _max.z),
			vec3(_max.x, _max.y, _min.z),
			vec3(_max.x, _max.y, _max.z)
		};

		// xform
		class CXformer
		{
		public:
			CXformer(const mat4 &matrix) : _matrix(matrix) {}
			void operator ()(vec3 &vertex)
			{
				vec3 xformed_vertex;
				mult_pos(xformed_vertex, _matrix, vertex);
				vertex = xformed_vertex;
			}
		private:
			const mat4 &_matrix;
		};
		std::for_each(box, box + _countof(box), CXformer(matrix));

		// build aabb for xformed box
		this->~AABB(); // currently nop
		new(this) AABB(box, box + _countof(box));

		return *this;
	}

	AABB &Blow(nv_scalar eps = nv_eps) throw()
	{
		const vec3 offset(eps, eps, eps);
		_min -= offset;
		_max += offset;

		return *this;
	}

	const vec3 &min() const throw()
	{
		return _min;
	}

	const vec3 &max() const throw()
	{
		return _max;
	}

	const vec3 center() const throw()
	{
		return nv_zero_5 * (_max + _min);
	}

	const vec3 extents() const throw()
	{
		return nv_zero_5 * size();
	}

	const vec3 size() const throw()
	{
		return _max - _min;
	}
private:
	vec3 _min, _max;
};