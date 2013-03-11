/**
\author		Alexey Shaydurov aka ASH
\date		11.3.2013 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#include "stdafx.h"
#include <Unknwn.h>
#include "Interface\Renderer.h"

//#define _2D_FLOAT32_TO_FLOAT16

#if APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#error APPEND_ALIGNED_ELEMENT != D3D11_APPEND_ALIGNED_ELEMENT
#endif

#if 0
using namespace std;
using namespace DGLE2;
using namespace DGLE2::Renderer::HighLevel;
using namespace Textures;
using namespace Materials;
using namespace Geometry;
using namespace Instances;
using namespace Instances::_2D;
using namespace DisplayModes;

//template<class Container>
//class CIterDtor
//{
//public:
//	CIterDtor(Container &container, typename Container::iterator iter): _container(container), _iter(iter) {}
//	~CIterDtor()
//	{
//		_container.erase(_iter);
//	}
//	operator typename Container::reference()
//	{
//		return *_iter;
//	}
//	operator typename Container::const_reference()
//	{
//		return *_iter;
//	}
//private:
//	Container &_container;
//	const typename Container::iterator _iter;
//};
//
//template<class Container>
//class CIterHandle: public CIterDtor<Container>
//{
//public:
//	template<typename Item>
//	inline CIterHandle(Container &container, Item &&item);
//};
//
//template<class Container>
//template<typename Item>
//CIterHandle<Container>::CIterHandle(Container &container, Item &&item):
//	CIterDtor(container)
//{
//}

namespace DX11
{
	using namespace ComPtrs;

	class CRenderer: private CRef<CRenderer>, public IRenderer
	{
	public:
		virtual Materials::IMaterial *CreateMaterial() override;
		virtual Geometry::IMesh *CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords) override;
		virtual IInstance *CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material) override;
	};

//	class CDevice::CRect: public _2D::IRect, private TQuad
//	{
//		virtual ~CRect()
//		{
//			if (parent)
//				(parent->*_container).erase(_thisIter);
//		}
//	public:
//		inline CRect(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float width, float height, uint32 color, float angle):
//			parent(parent), TQuad(x, y, width, height, layer, angle, color),
//			_container(dynamic ? &CDevice::_dynamicRects : &CDevice::_staticRects),
//			_thisIter(((parent->*_container).push_front(this), (parent->*_container).begin()))
////			_thisIter((parent->*_container).insert(lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(less<CRect>())), this))
//		{
//			if (!dynamic)
//				parent->_static2DDirty = true;
//		}
//		inline bool operator <(const CRect &rect) const
//		{
//			return TQuad::operator <(rect);
//		}
//		inline operator const TQuad &() const
//		{
//			return *this;
//		}
//		CDevice *parent;
//	private:
//		TRects CDevice::*const _container;
//		const TRects::const_iterator _thisIter;
//	};
//
//	class CDevice::CEllipse: public _2D::IEllipse, private TQuad
//	{
//		// C++11
//		//CEllipse(const CEllipse &) = delete;
//		//CEllipse &operator =(const CEllipse &) = delete;
//		virtual ~CEllipse()
//		{
//			if (parent)
//				(parent->*_container).erase(_thisIter);
//		}
//	public:
//		inline CEllipse(CDevice *parent, bool dynamic, uint16 layer, float x, float y, float rx, float ry, uint32 color, bool AA, float angle):
//			parent(parent), TQuad(x, y, rx, ry, layer, angle, color),
//			_container(dynamic ? AA ? &CDevice::_dynamicEllipsesAA : &CDevice::_dynamicEllipses : AA ? &CDevice::_staticEllipsesAA : &CDevice::_staticEllipses),
//			_thisIter(((parent->*_container).push_front(this), (parent->*_container).begin()))
////			_thisIter((parent->*_container).insert(lower_bound((parent->*_container).begin(), (parent->*_container).end(), this, CreateDereference(less<CEllipse>())), this))
//		{
//			if (!dynamic)
//				parent->_static2DDirty = true;
//		}
//		inline bool operator <(const CEllipse &rect) const
//		{
//			return TQuad::operator <(rect);
//		}
//		inline operator const TQuad &() const
//		{
//			return *this;
//		}
//		CDevice *parent;
//	private:
//		TEllipses CDevice::*const _container;
//		const TEllipses::const_iterator _thisIter;
//	};

	class CMesh: public CRef<CMesh>, public Geometry::IMesh
	{
		friend IMesh *CRenderer::CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords);
		virtual AABB<3> GetAABB() const override;
	private:
		CMesh(ID3D11Device *device, uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords);
		//virtual ~CMesh() = default;
	private:
		ID3D11BufferPtr	_geometry;
		UINT			_ibOffset, _vbOffsets[1];
		AABB<3>			_aabb;
		DXGI_FORMAT		_IBFormat;
	};

	inline AABB<3> CMesh::GetAABB() const
	{
		return _aabb;
	}

	// 1 call site => inline
	// TODO: clarify 'vcount <= D3D11_16BIT_INDEX_STRIP_CUT_VALUE + 1'
	inline CMesh::CMesh(ID3D11Device *device, uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords):
		_aabb(reinterpret_cast<const float (*)[3]>(coords), reinterpret_cast<const float (*)[3]>(coords) + vcount),
		_IBFormat(vcount <= D3D11_16BIT_INDEX_STRIP_CUT_VALUE + 1 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT)
	{
		decltype(declval<D3D11_BUFFER_DESC>().ByteWidth)
			idxSize = (_IBFormat == DXGI_FORMAT_R16_UINT ? sizeof(uint16) : sizeof(uint32)) * icount,
			coordsSize = sizeof *coords * 3 * vcount;
		const D3D11_BUFFER_DESC desc =
		{
			idxSize + coordsSize,								//ByteWidth
			D3D11_USAGE_IMMUTABLE,								//Usage
			D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER,	//BindFlags
			0,													//CPUAccessFlags
			0,													//MiscFlags
			0													//StructureByteStride
		};

		unique_ptr<uint8_t []> buf(new uint8_t[desc.ByteWidth]);

		_vbOffsets[0] = 0;
		memcpy(buf.get() + _vbOffsets[0], coords, coordsSize);

		_ibOffset = _vbOffsets[0] + coordsSize;
		const auto bufIdx = buf.get() + _ibOffset;
		if (_IBFormat == DXGI_FORMAT_R32_UINT)
			memcpy(bufIdx, idx, idxSize);
		else
			copy(idx, idx + icount, reinterpret_cast<uint16 *>(bufIdx));

		const D3D11_SUBRESOURCE_DATA data =
		{
			buf.get(),	//pSysMem
			0,			//SysMemPitch
			0			//SysMemSlicePitch
		};

		ASSERT_HR(device->CreateBuffer(&desc, &data, &_geometry));
	}

	class CMaterial: public CRef<CMaterial>, public Materials::IMaterial
	{
		friend IMaterial *CRenderer::CreateMaterial();
		virtual void SetAmbientColor(const float color[3]) override;
		virtual void SetDiffuseColor(const float color[3]) override;
		virtual void SetSpecularColor(const float color[3]) override;
		virtual void SetHeightScale(float scale) override;
		virtual void SetEnvAmount(float amount) override;
		virtual void SetShininess(float shininess) override;
		virtual void SetTexMappingMode(Materials::E_TEX_MAPPING texMapping) override;
		virtual void SetNormalTechnique(Materials::E_NORMAL_TECHNIQUE technique) override;
		virtual void SetParallaxTechnique(Materials::E_PARALLAX_TECHNIQUE technique) override;
		virtual void SetDiffuseTexture(IMatrialTexture *texture) override;
		virtual void SetSpecularTexture(IMatrialTexture *texture) override;
		virtual void SetNormalTexture(IMatrialTexture *texture) override;
		virtual void SetHeightTexture(IMatrialTexture *texture) override;
		virtual void SetEnvTexture(IMatrialTexture *texture) override;
		virtual void SetEnvMask(IMatrialTexture *texture) override;
	private:
		CMaterial();
		//virtual ~CMaterial() = default;
	private:
		float
										_ambientColor[3],
										_diffuseColor[3],
										_specularColor[3],

										_heightScale,
										_envAmount,
										_shininess;

		Materials::E_TEX_MAPPING		_texMapping;
		Materials::E_NORMAL_TECHNIQUE	_normalTechnique;
		Materials::E_PARALLAX_TECHNIQUE	_parallaxTechnique;

		//shared_ptr<>
		//								_diffuseTexture,
		//								_specularTexture,
		//								_normalTexture,
		//								_heightTexture,
		//								_envTexture,
		//								_envMask;
	};

	inline void CMaterial::SetAmbientColor(const float color[3])
	{
		memcpy(_ambientColor, color, sizeof _ambientColor);
	}

	inline void CMaterial::SetDiffuseColor(const float color[3])
	{
		memcpy(_diffuseColor, color, sizeof _diffuseColor);
	}

	inline void CMaterial::SetSpecularColor(const float color[3])
	{
		memcpy(_specularColor, color, sizeof _specularColor);
	}

	inline void CMaterial::SetHeightScale(float scale)
	{
		_heightScale = scale;
	};

	inline void CMaterial::SetEnvAmount(float amount)
	{
		_envAmount = amount;
	};

	inline void CMaterial::SetShininess(float shininess)
	{
		_shininess = shininess;
	};

	inline void CMaterial::SetTexMappingMode(Materials::E_TEX_MAPPING texMapping)
	{
		_texMapping = texMapping;
	};

	inline void CMaterial::SetNormalTechnique(Materials::E_NORMAL_TECHNIQUE technique)
	{
		_normalTechnique = technique;
	};

	inline void CMaterial::SetParallaxTechnique(Materials::E_PARALLAX_TECHNIQUE technique)
	{
		_parallaxTechnique = technique;
	};

	inline void CMaterial::SetDiffuseTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetSpecularTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetNormalTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetHeightTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetEnvTexture(IMatrialTexture *texture)
	{
	};

	inline void CMaterial::SetEnvMask(IMatrialTexture *texture)
	{
	};

	// 1 call site => inline
	inline CMaterial::CMaterial():
		_heightScale(1), _envAmount(0), _shininess(128),
		_texMapping(Materials::E_TEX_MAPPING::UV), _normalTechnique(Materials::E_NORMAL_TECHNIQUE::UNPERTURBED), _parallaxTechnique(Materials::E_PARALLAX_TECHNIQUE::NONE)
	{
		fill_n(_ambientColor, extent<decltype(_ambientColor)>::value, 1);
		fill_n(_diffuseColor, extent<decltype(_diffuseColor)>::value, 1);
		fill_n(_specularColor, extent<decltype(_specularColor)>::value, 1);
	}

	class CInstance: CDtor, public IInstance
	{
		friend IInstance *CRenderer::CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material);
	private:
		CInstance(shared_ptr<const CMesh> &&mesh, shared_ptr<const CMaterial> &&material);
		//virtual ~CInstance() = default;
	private:
		const shared_ptr<const CMesh>		_mesh;
		const shared_ptr<const CMaterial>	_material;
	};

	// 1 call site => inline
	inline CInstance::CInstance(shared_ptr<const CMesh> &&mesh, shared_ptr<const CMaterial> &&material):
		_mesh(mesh), _material(material)
	{
	}
}

namespace DX11
{
	Materials::IMaterial *CRenderer::CreateMaterial()
	{
		return new CMaterial;
	}

	Geometry::IMesh *CRenderer::CreateMesh(uint icount, _In_count_(icount) const uint32 *idx, uint vcount, _In_count_(vcount) const float *coords)
	{
		return new CMesh(_device, icount, idx, vcount, coords);
	}

	IInstance *CRenderer::CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material)
	{
		return new CInstance(static_cast<const CMesh &>(mesh).GetRef(), static_cast<const CMaterial &>(material).GetRef());
	}
}
#endif