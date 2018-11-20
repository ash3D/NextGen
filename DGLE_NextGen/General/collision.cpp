/**
\author		Alexey Shaydurov aka ASH
\date		22.08.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "collision.h"
#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <type_traits>
#include <cstdint>
#include <cfenv>
#include <cstring>	// for memcpy
#include <cassert>

using namespace std;
#if INCLUDE_DGLE_EXTENSIONS
using namespace DGLE;
#endif
namespace Collision = Math::Collision;
#if INCLUDE_DGLE_EXTENSIONS
namespace CollisionStat = Collision::CollisionStat;
#endif

static_assert(numeric_limits<nv_scalar>::has_quiet_NaN, "quiet NaN support required");

template<typename IB_format>
template<typename Iterator>
Collision::CCollisionEdges<IB_format>::CCollisionEdges(Iterator begin, Iterator end, const vec3 *__restrict VB) noexcept
{
	struct TEdge : CCollisionEdges<IB_format>::TEdge
	{
		TEdge(IB_format v0, IB_format v1, typename iterator_traits<Iterator>::reference tri) : tri(tri)
		{
			TEdge::v0 = v0;
			TEdge::v1 = v1;
		}
		bool operator <(const TEdge &right) const
		{
			IB_format
				left_sorted[] = { min(v0, v1), max(v0, v1) },
				right_sorted[] = { min(right.v0, right.v1), max(right.v0, right.v1) };
			return lexicographical_compare(left_sorted, left_sorted + size(left_sorted), right_sorted, right_sorted + size(right_sorted));
		}
		using CCollisionEdges::TEdge::v0;
		using CCollisionEdges::TEdge::v1;
		typename iterator_traits<Iterator>::reference tri;
	};
	typedef set<TEdge> TEdges;
	class CEdgeGenerator
	{
	public:
		CEdgeGenerator(TEdges &edges, const vec3 *__restrict VB) : _edges(edges), _VB(VB) {}
		void operator ()(typename iterator_traits<Iterator>::reference tri)
		{
			vec3 n2;
			cross(n2, _VB[tri[1]] - _VB[tri[0]], _VB[tri[2]] - _VB[tri[0]]);
			unsigned int v0 = 0, v1 = 1;
			do
			{
				const auto inserted = _edges.insert(TEdge(tri[v0], tri[v1], tri));
				if (!inserted.second)	// this edge was already inserted
				{
					// remove edge if angle <= pi
					// 1 - old tri
					// 2 - new tri
					vec3 e1 = _VB[inserted.first->v1] - _VB[inserted.first->v0], n1, n2, n1_cross_e1;
					cross(n1, _VB[inserted.first->tri[1]] - _VB[inserted.first->tri[0]], _VB[inserted.first->tri[2]] - _VB[inserted.first->tri[0]]);
					cross(n1_cross_e1, n1, e1);
					nv_scalar dp;
					if (dot(dp, n1_cross_e1, n2) >= nv_zero)	// angle <= pi
						_edges.erase(inserted.first);
				}
				v0 = v1++;
				if (v1 == 3) v1 = 0;
			} while (v0 < 2);
		}
	private:
		TEdges &_edges;
		const vec3 *const __restrict _VB;
	};
	TEdges edges;
	for_each(begin, end, CEdgeGenerator(edges, VB));
	_edges.reserve(edges.size());
	copy(edges.begin(), edges.end(), back_inserter(_edges));
}

template<typename IB_format>
Collision::CCollisionEdges<IB_format>::CCollisionEdges(const void *__restrict &data) noexcept
{
	if (!data) return;

	const uint32_t &size = *reinterpret_cast<const uint32_t *>(data);
	auto &buffer = reinterpret_cast<const uint8_t *__restrict &>(data);
	_edges.resize(size);
	memcpy(_edges.data(), buffer += sizeof size, sizeof(TEdges::value_type) * size);
	buffer += sizeof(TEdges::value_type) * size;
}

#if INCLUDE_DGLE_EXTENSIONS
template<typename IB_format>
Collision::CCollisionEdges<IB_format>::CCollisionEdges(IFile *pFile) noexcept
{
	if (!pFile) return;

	uint read;
	uint32 size;

	AssertHR(pFile->Read(&size, sizeof size, read));

	if (size != 0)
	{
		_edges.resize(size);
		AssertHR(pFile->Read(_edges.data(), sizeof(TEdges::value_type) * size, read));
	}
}

template<typename IB_format>
uint Collision::CCollisionEdges<IB_format>::SaveToFile(IFile *pFile) const noexcept
{
	uint res = 0;

	if (!pFile)
		return res;

	uint writen;
	uint32 size = _edges.size();

	AssertHR(pFile->Write(&size, sizeof size, writen));

	res += writen;

	if (size != 0)
	{
		AssertHR(pFile->Write(_edges.data(), sizeof(TEdges::value_type) * size, writen));
		res += writen;
	}

	return res;
}
#endif

template Collision::CCollisionEdges<uint16_t>;
template Collision::CCollisionEdges<uint32_t>;
template Collision::CCollisionEdges<uint16_t>::CCollisionEdges(const uint16_t(*begin)[3], const uint16_t(*end)[3], const vec3 *__restrict VB) noexcept;
template Collision::CCollisionEdges<uint32_t>::CCollisionEdges(const uint32_t(*begin)[3], const uint32_t(*end)[3], const vec3 *__restrict VB) noexcept;

// (?) check for 0 that avoids NaN improves perf for x87 and SSE1 modes

static nv_scalar __fastcall dotX(const vec3 &vec) noexcept
{
	return vec.x;
}

static nv_scalar __fastcall dotY(const vec3 &vec) noexcept
{
	return vec.y;
}

static nv_scalar __fastcall dotZ(const vec3 &vec) noexcept
{
	return vec.z;
}

static nv_scalar fMax, bMin;

static bool __fastcall TestPlane(nv_scalar dir_dot_n_inv, nv_scalar orig_dot_n, nv_scalar plane_dist) noexcept
{
	nv_scalar ray_dist = (plane_dist - orig_dot_n) * dir_dot_n_inv;
	if (dir_dot_n_inv < 0)	// front face
	{
		if (ray_dist > fMax)
		{
			if (ray_dist > bMin)
				return false;
			fMax = ray_dist;
		}
	}
	else					// back face
		if (ray_dist < bMin)
		{
			if (ray_dist < 0 || ray_dist < fMax)
				return false;
			bMin = ray_dist;
		}
	return true;
}

extern nv_scalar Collision::RayAABBIntersect(const vec3 &rayOrig, const vec3 &rayDir, const vec3 &AABBCenter, const vec3 &AABBExtents) noexcept
{
	fMax = -numeric_limits<nv_scalar>::infinity();
	bMin = +numeric_limits<nv_scalar>::infinity();
#if __clang__
	static nv_scalar(__fastcall *AABBnormals[3])(const vec3 &) = { dotX, dotY, dotZ };
#else
	static nv_scalar(__fastcall *AABBnormals[3])(const vec3 &) noexcept = { dotX, dotY, dotZ };
#endif
	vec3
		AABBdist_pos = AABBExtents + AABBCenter,
		AABBdist_neg = AABBExtents - AABBCenter;
	for (unsigned i = 0; i < 3; i++)
	{
		nv_scalar
			dir_dot_n = AABBnormals[i](rayDir),
			orig_dot_n = AABBnormals[i](rayOrig);
		if (dir_dot_n != 0)
		{
			nv_scalar dir_dot_n_inv = nv_one / dir_dot_n;
			if (!TestPlane(dir_dot_n_inv, orig_dot_n, AABBdist_pos[i]) || !TestPlane(-dir_dot_n_inv, -orig_dot_n, AABBdist_neg[i]))
				return numeric_limits<nv_scalar>::quiet_NaN();
		}
		else	// || plane
			if (orig_dot_n > AABBdist_pos[i] || -orig_dot_n > AABBdist_neg[i])
				return numeric_limits<nv_scalar>::quiet_NaN();
	}

	return fMax > 0 ? fMax : bMin;
}

template<unsigned int k>
extern vec2 Collision::RayKDOPIntersect(const vec3 &rayOrig, const vec3 &rayDir, const PlanePair(&planes)[k]) noexcept
{
	fMax = -numeric_limits<nv_scalar>::infinity();
	bMin = +numeric_limits<nv_scalar>::infinity();
	for (unsigned i = 0; i < k; i++)
	{
		nv_scalar dir_dot_n, orig_dot_n;
		dot(dir_dot_n, rayDir, planes[i].n);
		dot(orig_dot_n, rayOrig, planes[i].n);
		if (dir_dot_n != 0)
		{
			nv_scalar dir_dot_n_inv = nv_one / dir_dot_n;
			if (!TestPlane(dir_dot_n_inv, orig_dot_n, planes[i].dist[0]) || !TestPlane(-dir_dot_n_inv, -orig_dot_n, planes[i].dist[1]))
				return vec2(numeric_limits<nv_scalar>::quiet_NaN(), numeric_limits<nv_scalar>::quiet_NaN());//return numeric_limits<nv_scalar>::quiet_NaN();
		}
		else	// || plane
			if (orig_dot_n > planes[i].dist[0] || -orig_dot_n > planes[i].dist[1])
				return vec2(numeric_limits<nv_scalar>::quiet_NaN(), numeric_limits<nv_scalar>::quiet_NaN());//return numeric_limits<nv_scalar>::quiet_NaN();
	}

	//return fMax > 0 ? fMax : bMin;
	return vec2(fMax, bMin);
}

template vec2 Collision::RayKDOPIntersect(const vec3 &rayOrig, const vec3 &rayDir, const PlanePair(&planes)[3]) noexcept;

static inline nv_scalar RayPlaneIntersect(const vec3 &rayOrig, const vec3 &rayDir, const vec3 &n, const vec3 &v) noexcept
{
	nv_scalar dir_dot_n;
	if (dot(dir_dot_n, rayDir, n) == 0)
		return numeric_limits<nv_scalar>::quiet_NaN();
	nv_scalar dirv_dot_n;
	dot(dirv_dot_n, v - rayOrig, n);
	return dirv_dot_n / dir_dot_n;
}

static inline bool PointInsideTri(const vec3 &e1, const vec3 &e2, const vec3 &f) noexcept
{
	nv_scalar e1_dot_e1, e1_dot_e2, e2_dot_e2;
	dot(e1_dot_e1, e1, e1);
	dot(e1_dot_e2, e1, e2);
	dot(e2_dot_e2, e2, e2);
	nv_scalar B, C, D = e1_dot_e1 * e2_dot_e2 - e1_dot_e2 * e1_dot_e2;
	vec3 bary;
	dot(B, e2_dot_e2 * e1 - e1_dot_e2 * e2, f);
	bary.y = B / D;
	if (bary.y < 0 || bary.y > 1)
		return false;
	dot(C, e1_dot_e1 * e2 - e1_dot_e2 * e1, f);
	bary.z = C / D;
	if (bary.z < 0 || bary.z > 1)
		return false;
	bary.x = 1.f - bary.y - bary.z;
	return bary.x >= 0;
}

extern nv_scalar Collision::RayTriIntersect(const vec3 &rayOrig, const vec3 &rayDir, const vec3 &v0, const vec3 &v1, const vec3 &v2) noexcept
{
	// compute tri edges
	vec3
		e1 = v1 - v0,
		e2 = v2 - v0;

	// compute tri normal
	vec3 n;
	cross(n, e1, e2);

	// find ray intersection with tri plane
	nv_scalar dist = RayPlaneIntersect(rayOrig, rayDir, n, v0);
	//if (dist < 0)
	//	return dist;
	vec3 intersection = rayOrig + dist * rayDir;

	// determine if intersection inside tri
	return PointInsideTri(e1, e2, intersection - v0) ? dist : numeric_limits<nv_scalar>::quiet_NaN();
}

// returns point closer to -inf along V
extern nv_scalar Collision::RaySphereIntersect(vec3 S, const vec3 &V, const vec3 &C, nv_scalar r2) noexcept
{
	if (V.sq_norm() == nv_zero) return numeric_limits<nv_scalar>::quiet_NaN();
	// move coord origin to sphere center
	S -= C;

	nv_scalar S_dot_V;
	dot(S_dot_V, S, V);
	nv_scalar D_div_4 = S_dot_V * S_dot_V - V.sq_norm() * (S.sq_norm() - r2);
	if (D_div_4 < 0) return numeric_limits<nv_scalar>::quiet_NaN();
	return (-S_dot_V - sqrt(D_div_4)) / V.sq_norm();
}

// returns point closer to -inf along V
extern nv_scalar Collision::RayCilinderIntersect(vec3 R0, const vec3 &R0R1, const vec3 &C0, const vec3 &C0C1, nv_scalar r2) noexcept
{
	if (C0C1.sq_norm() == nv_zero) return numeric_limits<nv_scalar>::quiet_NaN();
	R0 -= C0;
	nv_scalar R0R1_dot_C0C1, R0_dot_C0C1, R0_dot_R0R1;
	dot(R0R1_dot_C0C1, R0R1, C0C1);
	dot(R0_dot_C0C1, R0, C0C1);
	dot(R0_dot_R0R1, R0, R0R1);
	nv_scalar rcpsqCOC1 = nv_one / C0C1.sq_norm();
	nv_scalar a = R0R1.sq_norm() - R0R1_dot_C0C1 * R0R1_dot_C0C1 * rcpsqCOC1;
	if (a == nv_zero) return numeric_limits<nv_scalar>::quiet_NaN();
	nv_scalar b_div_2 = R0_dot_R0R1 - R0_dot_C0C1 * R0R1_dot_C0C1 * rcpsqCOC1;
	nv_scalar c = R0.sq_norm() - r2 - R0_dot_C0C1 * R0_dot_C0C1 * rcpsqCOC1;
	nv_scalar D_div_4 = b_div_2 * b_div_2 - a * c;
	if (D_div_4 < 0) return numeric_limits<nv_scalar>::quiet_NaN();
	nv_scalar dist = (-b_div_2 - sqrt(D_div_4)) / a;
	vec3 P = R0 + dist * R0R1;
	nv_scalar P_dot_C0C1;
	dot(P_dot_C0C1, P, C0C1);
	if (P_dot_C0C1 < nv_zero || P_dot_C0C1 > C0C1.sq_norm()) return numeric_limits<nv_scalar>::quiet_NaN();
	return dist;
}

#if INCLUDE_DGLE_EXTENSIONS
static unsigned int
	collideAndSlideInvokeCount,
	collideAndSlideIterationCount,
	cullerInvokeCount, cullerRejectCount,
	triColliderInvokeCount,
	edgeColliderInvokeCount,
	vertexColliderInvokeCount;

extern void CollisionStat::StartProfiling() noexcept
{
	// clear counters
	collideAndSlideInvokeCount = collideAndSlideIterationCount = cullerInvokeCount = cullerRejectCount = triColliderInvokeCount = edgeColliderInvokeCount = vertexColliderInvokeCount = 0;
}

// TODO: reuse single function here and in "Shared.h"
static inline void AssertHR(const HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

extern void CALLBACK CollisionStat::ProfilerEventHandler(void *pEngineCore, IBaseEvent *pEvent)
{
	E_EVENT_TYPE ev_type;
	AssertHR(pEvent->GetEventType(ev_type));
	switch (ev_type)
	{
	case ET_ON_PROFILER_DRAW:
		IEngineCore *const &p_engine_core = reinterpret_cast<IEngineCore *const &>(pEngineCore);
		AssertHR(p_engine_core->RenderProfilerText("--Collision Stats--", TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Collide and slide invoke count: " + to_string((unsigned long long)collideAndSlideInvokeCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Collide and slide iteration count: " + to_string((unsigned long long)collideAndSlideIterationCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Culler invoke count: " + to_string((unsigned long long)cullerInvokeCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Culler reject count: " + to_string((unsigned long long)cullerRejectCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Tri collider invoke count: " + to_string((unsigned long long)triColliderInvokeCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Edge collider invoke count: " + to_string((unsigned long long)edgeColliderInvokeCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText(("Vertex collider invoke count: " + to_string((unsigned long long)vertexColliderInvokeCount)).c_str(), TColor4()));
		AssertHR(p_engine_core->RenderProfilerText("-------------------", TColor4()));
		break;
	}
}
#endif

void Collision::CSphereXformHandler::SetXform(const mat4 &xform)
{
	// xform center and direction
	mult_pos(_xformedSphere.c, xform, _c);
	mult_dir(_xformedSphere.dir, xform, _dir);

	// calc scale factor (examine any xformed unit vector length)
	vec3 test_vec;
	mult_dir(test_vec, xform, vec3_x);
	_xformedSphere.r2 = _r2 * test_vec.sq_norm();
	_xformedSphere.r = sqrt(_xformedSphere.r2);

	// save normal xform
	transpose(xform.get_rot(_normalXform));
}

bool Collision::CCuller::operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &center, const vec3 &extents) const
{
#if INCLUDE_DGLE_EXTENSIONS
	++cullerInvokeCount;
#endif

	const CSphereXformHandler::TMovingSphere &xformed_sphere = sphereXformHandler;
	const mat4 xform(mat4_id);
	// xform normals
	// positive normals equals basis vectors
	PlanePair planes[3];
	int i = 0;
	do
	{
		// original pos normal: {+{i==0, i==1, i==3}, -(extents[i] + center[i])}
		// original neg normal: {-{i==0, i==1, i==3}, -(extents[i] - center[i])}
		vec4 col = xform.col(i);
		planes[i].n = vec3(col.vec_array);
		// offset planes by sphere radius
		nv_scalar offset = xformed_sphere.r / planes[i].n.norm();
		planes[i].dist[0] = xform(3, 3) * (extents[i] + center[i]) - col.w + offset;
		planes[i].dist[1] = xform(3, 3) * (extents[i] - center[i]) + col.w + offset;
	} while (++i < 3);

	// perform intersection test
	vec2 intersection = RayKDOPIntersect(xformed_sphere.c, xformed_sphere.dir, planes);
	bool cull = !(intersection.x <= nv_zero && intersection.y >= nv_zero || intersection.x <= intersection.y && (!_finite || intersection.x >= nv_zero) && intersection.x < _dist);

#if INCLUDE_DGLE_EXTENSIONS
	if (cull) ++cullerRejectCount;
#endif

	return cull;
}

void Collision::CTriCollider::operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
#if INCLUDE_DGLE_EXTENSIONS
	++triColliderInvokeCount;
#endif

	const CSphereXformHandler::TMovingSphere &xformed_sphere = sphereXformHandler;

	// compute tri edges
	vec3
		e1 = v1 - v0,
		e2 = v2 - v0;

	// compute tri normal
	vec3 n;
	cross(n, e1, e2);
	n.normalize();
	const vec3 offset = xformed_sphere.r * n;

	auto CollideFace = [&](vec3 v0, vec3 v1, vec3 v2, const vec3 &offset, const vec3 &n)
	{
		// offset tri along normal
		v0 += offset;
		v1 += offset;
		v2 += offset;

		if (CBasePrimitiveCollider::operator ()(RayTriIntersect(xformed_sphere.c, xformed_sphere.dir, v0, v1, v2)))
			SetNormal(n, sphereXformHandler);
	};

	CollideFace(v0, v1, v2, offset, n);
	if (_doubleSide)
		CollideFace(v0, v1, v2, -offset, -n);
}

void Collision::CEdgeCollider::operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v0, const vec3 &v1)
{
#if INCLUDE_DGLE_EXTENSIONS
	++edgeColliderInvokeCount;
#endif

	const CSphereXformHandler::TMovingSphere &xformed_sphere = sphereXformHandler;
	vec3 e = v1 - v0;
	nv_scalar cur_dist = RayCilinderIntersect(xformed_sphere.c, xformed_sphere.dir, v0, e, xformed_sphere.r2);
	if (CBasePrimitiveCollider::operator ()(cur_dist))
	{
		// get vector from v0 to intersection point
		vec3 n = xformed_sphere.c + cur_dist * xformed_sphere.dir - v0;

		// extract normal component
		nv_scalar n_dot_e;
		dot(n_dot_e, n, e);
		n -= n_dot_e / e.sq_norm() * e;
		SetNormal(n, sphereXformHandler);
	}
}

void Collision::CVertexCollider::operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v)
{
#if INCLUDE_DGLE_EXTENSIONS
	++vertexColliderInvokeCount;
#endif

	const CSphereXformHandler::TMovingSphere &xformed_sphere = sphereXformHandler;
	nv_scalar cur_dist = RaySphereIntersect(xformed_sphere.c, xformed_sphere.dir, v, xformed_sphere.r2);
	if (CBasePrimitiveCollider::operator ()(cur_dist))
	{
		vec3 n = xformed_sphere.c + cur_dist * xformed_sphere.dir - v;
		SetNormal(n, sphereXformHandler);
	}
}

// finite param:
// true - segment intersection [0, 1)
// false - ray intersection (-inf, +1)
#if defined _MSC_VER && _MSC_VER <= 1915 && !__clang__
#pragma fenv_access(on)
#else
#pragma STDC FENV_ACCESS ON
#endif
extern auto Collision::SphereCollide(const IGeometryProvider &geometryProvider, const vec3 &c, nv_scalar r2, const vec3 &dir, nv_scalar skinWidth, bool doubleSide, bool finite) noexcept -> TCollideResult
{
	TCollideResult result;
	if (dir.sq_norm() == nv_zero)
	{
		result.dist = numeric_limits<nv_scalar>::quiet_NaN();
		return result;
	}

	// ensure result.dist >= 1 after += then -= dir_scaled_skin_width
	const auto default_rounddir = fegetround();
	[[maybe_unused]] int status = fesetround(FE_UPWARD);
	assert(status == 0);

	const nv_scalar dir_scaled_skin_width = skinWidth / dir.norm();
	result.dist = nv_one + dir_scaled_skin_width;

	CTriCollider tri_handler(result, doubleSide, finite);
	CEdgeCollider edge_handler(result, finite);
	CVertexCollider vertex_handler(result, finite);
	CSphereXformHandler xform_handler(c, r2, dir);
	geometryProvider(tri_handler, edge_handler, vertex_handler, CCuller(result.dist, finite), xform_handler);

	result.dist -= dir_scaled_skin_width;
	if (finite && result.dist < nv_zero) result.dist = nv_zero;
	status = fesetround(default_rounddir);
	assert(status == 0);
	return result;
}
#if defined _MSC_VER && _MSC_VER <= 1915 && !__clang__
#pragma fenv_access(off)
#else
#pragma STDC FENV_ACCESS DEFAULT
#endif

extern vec3 Collision::SphereCollideAndSlide(const IGeometryProvider &geometryProvider, vec3 c, nv_scalar r2, vec3 dir, nv_scalar skinWidth, nv_scalar minDist, unsigned int maxSlides) noexcept
{
#if INCLUDE_DGLE_EXTENSIONS
	++collideAndSlideInvokeCount;
#endif

	do
	{
#if INCLUDE_DGLE_EXTENSIONS
		++collideAndSlideIterationCount;
#endif

		TCollideResult collision = SphereCollide(geometryProvider, c, r2, dir, skinWidth, false, true);
		if (collision.dist < nv_one)
		{
			c += collision.dist * dir;
			dir *= nv_one - collision.dist;
			// remove dir's normal projection
			nv_scalar dir_dot_n;
			dir -= dot(dir_dot_n, dir, collision.n) * collision.n;
		}
		else
			return c + dir;
	} while (--maxSlides && dir.sq_norm() > minDist * minDist);
	return c;
}