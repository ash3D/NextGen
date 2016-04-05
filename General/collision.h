/**
\author		Alexey Shaydurov aka ASH
\date		23.03.2016 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "DGLE.h"
#include "nv_algebra.h"
#include <vector>

namespace Math
{
	namespace Collision
	{
		template<typename IB_format>
		class CCollisionEdges
		{
		public:
			struct TEdge
			{
				IB_format v0, v1;
			};
		private:
			typedef std::vector<TEdge> TEdges;
			TEdges _edges;

		public:
			CCollisionEdges() throw() {}
			template<typename Iterator>
			CCollisionEdges(Iterator begin, Iterator end, const vec3 *__restrict VB) throw();
			CCollisionEdges(const void *__restrict &data) throw();
			CCollisionEdges(DGLE::IFile *pFile) throw();
			typename TEdges::const_iterator Begin() const throw() { return _edges.begin(); }
			typename TEdges::const_iterator End() const throw() { return _edges.end(); }
			DGLE::uint SaveToFile(DGLE::IFile *pFile) const throw();
		};

		extern nv_scalar RayAABBIntersect(const vec3 &rayOrig, const vec3 &rayDir, const vec3 &AABBCenter, const vec3 &AABBExtents) throw();

		struct PlanePair
		{
			vec3 n;
			vec2 dist;	// dist along n
		};

		template<unsigned int k>
		extern vec2 RayKDOPIntersect(const vec3 &rayOrig, const vec3 &rayDir, const PlanePair(&planes)[k]) throw();
		extern nv_scalar RayTriIntersect(const vec3 &rayOrig, const vec3 &rayDir, const vec3 &v0, const vec3 &v1, const vec3 &v2) throw();
		extern nv_scalar RaySphereIntersect(vec3 rayOrig, const vec3 &rayDir, const vec3 &sphereCenter, nv_scalar sphereRadius2) throw();
		extern nv_scalar RayCilinderIntersect(vec3 rayOrig, const vec3 &rayDir, const vec3 &cilinderOrig, const vec3 &cilinderDir, nv_scalar cilinderRadius2) throw();

		namespace CollisionStat
		{
			extern void StartProfiling() throw();
			extern void CALLBACK ProfilerEventHandler(void *pEngineCore, DGLE::IBaseEvent *pEvent);
		}

		struct TCollideResult
		{
			nv_scalar	dist;
			vec3		n;
		};

		class CSphereXformHandler
		{
		public:
			CSphereXformHandler(const vec3 &c, nv_scalar r2, const vec3 &dir) :
				_c(c), _r2(r2), _dir(dir)
			{
				_xformedSphere.c = c, _xformedSphere.dir = dir, _xformedSphere.r2 = r2, _xformedSphere.r = sqrt(r2);
			}
			struct TMovingSphere
			{
				vec3 c, dir;
				nv_scalar r, r2;
			};
			operator const TMovingSphere &() const
			{
				return _xformedSphere;
			}
			const mat3 &GetNormalXform() const
			{
				return _normalXform;
			}
			void SetXform(const mat4 &xform);
		private:
			const vec3 &_c;
			const nv_scalar _r2;
			const vec3 &_dir;
			TMovingSphere _xformedSphere;
			mat3 _normalXform;
		};

		class CCuller
		{
		public:
			CCuller(const nv_scalar &dist, bool finite) :
				_dist(dist), _finite(finite)
			{
			}
			bool operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &center, const vec3 &extents) const;
		private:
			volatile const nv_scalar &_dist;
			const bool _finite;
		};

		class CBasePrimitiveCollider
		{
		protected:
			CBasePrimitiveCollider(TCollideResult &result, bool finite) :
				_result(result), _finite(finite)
			{
			}
			bool operator ()(nv_scalar curDist)
			{
				if ((!_finite || curDist >= nv_zero) && curDist < _result.dist)
				{
					_result.dist = curDist;
					return true;
				}
				return false;
			}
			void SetNormal(const vec3 &n, const CSphereXformHandler &sphereXformHandler)
			{
				normalize(_result.n = sphereXformHandler.GetNormalXform() * n);
			}
			TCollideResult &_result;
		private:
			const bool _finite;
		};

		// collide with expanded tris
		class CTriCollider final : private CBasePrimitiveCollider
		{
			const bool _doubleSide;
		public:
			CTriCollider(TCollideResult &result, bool doubleSide, bool finite) :
				CBasePrimitiveCollider(result, finite), _doubleSide(doubleSide)
			{
			}
			void operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v0, const vec3 &v1, const vec3 &v2);
		};

		// collide with edge's cilinders
		class CEdgeCollider final : private CBasePrimitiveCollider
		{
		public:
			CEdgeCollider(TCollideResult &result, bool finite) :
				CBasePrimitiveCollider(result, finite)
			{
			}
			void operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v0, const vec3 &v1);
		};

		// collide with verex's spheres
		class CVertexCollider final : private CBasePrimitiveCollider
		{
		public:
			CVertexCollider(TCollideResult &result, bool finite) :
				CBasePrimitiveCollider(result, finite)
			{
			}
			void operator ()(const CSphereXformHandler &sphereXformHandler, const vec3 &v);
		};

		class IGeometryProvider
		{
		public:
			virtual void operator ()(CTriCollider &triHandler, CEdgeCollider &edgeHandler, CVertexCollider &vertexHandler, const CCuller &culler, CSphereXformHandler &xformHandler) const throw(...) = 0;
		};

		extern TCollideResult SphereCollide(const IGeometryProvider &geometryProvider, const vec3 &c, nv_scalar r2, const vec3 &dir, nv_scalar skinWidth, bool doubleSide, bool finite) throw();
		extern vec3 SphereCollideAndSlide(const IGeometryProvider &geometryProvider, vec3 c, nv_scalar r2, vec3 dir, nv_scalar skinWidth, nv_scalar minDist, unsigned int maxSlides) throw();
	}
}