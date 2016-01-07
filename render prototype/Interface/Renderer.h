/**
\author		Alexey Shaydurov aka ASH
\date		07.01.2016 (c)Korotkov Andrey

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#pragma once

#if defined _MSC_VER && _MSC_VER < 1900
#error old compiler version
#endif

#ifndef _MSC_VER
#define _In_count_(size)
#endif

#ifdef _MSC_VER
#	define NOVTABLE __declspec(novtable)
#	define RESTRICT __declspec(restrict)
#else
#	define NOVTABLE
#	define RESTRICT
#endif

#pragma region considerations
/*
dedicated classes: CB, SB
dedicated DS class (whole update, 1D or 2D only, can't be used as rendertarget)
dedicated IB/VB class (dynamic draw)
dedicated SB SRV/UAV
draw from sysmem using dynamic IB/VB
other dynamic resources (textures, buffers, CBs)
dynamic textures can only have 1 mip level and 1 array element
multisampled textures (no update, no cubemaps, no mip levels)
Map() waiting handling via callback
specifying dynamically constructed data to dynamic resources via callbacks (maybe for static resources too)
multithreaded resource access (problem with callbacks)
access to whole resource (all mipmaps and array elements) in order to allow CopyResource() to be used (can be usefull for file I/O)
xyz access can be more convenient then zyx one
multithreading layer for old APIs
template for format (especially for structured buffer)
*/
#pragma endregion

//#define DISABLE_MATRIX_SWIZZLES
//#include "vector math.h"

#include <cstdint>
#include <memory>		// for shared_ptr
#include <type_traits>
#include <iterator>
#include <utility>
#include <limits>
#include <algorithm>
#include <cassert>

namespace DGLE2
{
	namespace Renderer
	{
		/*
		delete frees memory from current mudule's CRT heap
		Renderer implementation can use it's own CRT => delete can work incorrectly
		IDtor base class resolves this problem
		*/
		//class IDtor
		//{
		//public:
		//	void operator ~() const
		//	{
		//		this->~IDtor();
		//		operator delete(_parent, _deleter);
		//	}
		//private:
		//	void (&_deleter)(void *);
		//	void *const _parent;
		//	void operator delete(void *object, decltype(_deleter) deleter)
		//	{
		//		deleter(object);
		//	}
		//protected:
		//	IDtor(decltype(_deleter) deleter, decltype(_parent) parent): _deleter(deleter), _parent(parent) {}
		//	virtual ~IDtor() = default;
		//};
		struct NOVTABLE IDtor
		{
			// need const qualifier to allow const object destruction (like conventional dtor)
			virtual void operator ~() const = 0;
		protected:
			~IDtor() = default;
		};

		inline void dtor(const IDtor *const object)
		{
			~*object;
		};

		namespace LowLevel
		{
			struct TCaps
			{
			};
#pragma region State objects
#pragma region Common
			enum E_COMPARISON_FUNC
			{
				COMPARISON_NEVER,
				COMPARISON_LESS,
				COMPARISON_EQUAL,
				COMPARISON_LESS_EQUAL,
				COMPARISON_GREATER,
				COMPARISON_NOT_EQUAL,
				COMPARISON_GREATER_EQUAL,
				COMPARISON_ALWAYS
			};
#pragma endregion

#pragma region Blend
			enum E_BLEND
			{
				BLEND_ZERO,
				BLEND_ONE,
				BLEND_SRC_COLOR,
				BLEND_INV_SRC_COLOR,
				BLEND_SRC_ALPHA,
				BLEND_INV_SRC_ALPHA,
				BLEND_DEST_ALPHA,
				BLEND_INV_DEST_ALPHA,
				BLEND_DEST_COLOR,
				BLEND_INV_DEST_COLOR,
				BLEND_SRC_ALPHA_SAT,
				BLEND_BLEND_FACTOR,
				BLEND_INV_BLEND_FACTOR,
				BLEND_SRC1_COLOR,
				BLEND_INV_SRC1_COLOR,
				BLEND_SRC1_ALPHA,
				BLEND_INV_SRC1_ALPHA
			};

			enum E_BLEND_OP
			{
				BLEND_OP_ADD,
				BLEND_OP_SUBTRACT,
				BLEND_OP_REV_SUBTRACT,
				BLEND_OP_MIN,
				BLEND_OP_MAX
			};

			struct TBlendStateDesc
			{
				bool alphaToCoverageEnable, independentBlendEnable;
				struct
				{
					bool		blendEnable;
					E_BLEND		srcBlend;
					E_BLEND		destBlend;
					E_BLEND_OP	blendOp;
					E_BLEND		srcBlendAlpha;
					E_BLEND		destBlendAlpha;
					E_BLEND_OP	blendOpAlpha;
					uint8_t		renderTargetWriteMask;
				} renderTarget[8];
			};

			struct NOVTABLE IBlendState
			{
				virtual operator TBlendStateDesc() const = 0;
			};
#pragma endregion

#pragma region DepthStencil
			enum DEPTH_WRITE_MASK
			{
				DEPTH_WRITE_MASK_ZERO,
				DEPTH_WRITE_MASK_ALL
			};

			enum E_STENCIL_OP
			{
				STENCIL_OP_KEEP,
				STENCIL_OP_ZERO,
				STENCIL_OP_REPLACE,
				STENCIL_OP_INCR_SAT,
				STENCIL_OP_DECR_SAT,
				STENCIL_OP_INVERT,
				STENCIL_OP_INCR,
				STENCIL_OP_DECR
			};

			struct TDepthStencilDesc
			{
				bool				depthEnable;
				DEPTH_WRITE_MASK	depthWriteMask;
				E_COMPARISON_FUNC	depthFunc;
				bool				stencilEnable;
				uint8_t				stencilReadMask;
				uint8_t				stencilWriteMask;
				struct
				{
					E_STENCIL_OP		stencilFailOp;
					E_STENCIL_OP		stencilDepthFailOp;
					E_STENCIL_OP		stencilPassOp;
					E_COMPARISON_FUNC	stencilFunc;
				}					frontFace, backFace;
			};

			struct NOVTABLE IDepthStencilState
			{
				virtual operator TDepthStencilDesc() const = 0;
			};
#pragma endregion

#pragma region InputLayout
			enum E_VERTEX_FORMAT
			{
				VERTEX_FORMAT_R32G32B32A32_FLOAT	= 2,
				VERTEX_FORMAT_R32G32B32A32_UINT		= 3,
				VERTEX_FORMAT_R32G32B32A32_SINT		= 4,
				VERTEX_FORMAT_R32G32B32_FLOAT		= 6,
				VERTEX_FORMAT_R32G32B32_UINT		= 7,
				VERTEX_FORMAT_R32G32B32_SINT		= 8,
				VERTEX_FORMAT_R16G16B16A16_FLOAT	= 10,
				VERTEX_FORMAT_R16G16B16A16_UNORM	= 11,
				VERTEX_FORMAT_R16G16B16A16_UINT		= 12,
				VERTEX_FORMAT_R16G16B16A16_SNORM	= 13,
				VERTEX_FORMAT_R16G16B16A16_SINT		= 14,
				VERTEX_FORMAT_R32G32_FLOAT			= 16,
				VERTEX_FORMAT_R32G32_UINT			= 17,
				VERTEX_FORMAT_R32G32_SINT			= 18,
				VERTEX_FORMAT_R10G10B10A2_UNORM		= 24,
				VERTEX_FORMAT_R10G10B10A2_UINT		= 25,
				VERTEX_FORMAT_R11G11B10_FLOAT		= 26,
				VERTEX_FORMAT_R8G8B8A8_UNORM		= 28,
				VERTEX_FORMAT_R8G8B8A8_UINT			= 29,
				VERTEX_FORMAT_R8G8B8A8_SNORM		= 30,
				VERTEX_FORMAT_R8G8B8A8_SINT			= 31,
				VERTEX_FORMAT_R16G16_FLOAT			= 34,
				VERTEX_FORMAT_R16G16_UNORM			= 35,
				VERTEX_FORMAT_R16G16_UINT			= 36,
				VERTEX_FORMAT_R16G16_SNORM			= 37,
				VERTEX_FORMAT_R16G16_SINT			= 38,
				VERTEX_FORMAT_R32_FLOAT				= 41,
				VERTEX_FORMAT_R32_UINT				= 42,
				VERTEX_FORMAT_R32_SINT				= 43,
				VERTEX_FORMAT_R8G8_UNORM			= 49,
				VERTEX_FORMAT_R8G8_UINT				= 50,
				VERTEX_FORMAT_R8G8_SNORM			= 51,
				VERTEX_FORMAT_R8G8_SINT				= 52,
				VERTEX_FORMAT_R16_FLOAT				= 54,
				VERTEX_FORMAT_R16_UNORM				= 56,
				VERTEX_FORMAT_R16_UINT				= 57,
				VERTEX_FORMAT_R16_SNORM				= 58,
				VERTEX_FORMAT_R16_SINT				= 59,
				VERTEX_FORMAT_R8_UNORM				= 61,
				VERTEX_FORMAT_R8_UINT				= 62,
				VERTEX_FORMAT_R8_SNORM				= 63,
				VERTEX_FORMAT_R8_SINT				= 64,
				VERTEX_FORMAT_B8G8R8A8_UNORM		= 87,
				VERTEX_FORMAT_B8G8R8X8_UNORM		= 88
			};

			enum E_INPUT_CLASSIFICATION
			{
				INPUT_PER_VERTEX_DATA,
				INPUT_PER_INSTANCE_DATA
			};

#define APPEND_ALIGNED_ELEMENT (0xffffffff)

			struct TInputElementDesc
			{
				const char				*semanticName;
				unsigned int			semanticIndex;
				E_VERTEX_FORMAT			format;
				unsigned int			inputSlot;
				unsigned int			alignedByteOffset;
				E_INPUT_CLASSIFICATION	inputSlotClass;
				unsigned int			instanceDataStepRate;
			};

			struct NOVTABLE IInputLayout
			{
			};
#pragma endregion

#pragma region Rasterizer
			enum E_FILL_MODE
			{
				FILL_WIREFRAME,
				FILL_SOLID
			};

			enum E_CULL_MODE
			{
				CULL_NONE,
				CULL_FRONT,
				CULL_BACK
			};

			struct TRasterizerStateDesc
			{
				E_FILL_MODE	fillMode;
				E_CULL_MODE	cullMode;
				bool		frontCounterClockwise;
				int			depthBias;
				float		depthBiasClamp;
				float		slopeScaledDepthBias;
				bool		depthClipEnable;
				bool		scissorEnable;
				bool		multisampleEnable;
				bool		antialiasedLineEnable;
			};

			struct NOVTABLE IRasterizerState
			{
				virtual operator TRasterizerStateDesc() const = 0;
			};
#pragma endregion

#pragma region Sampler
			enum E_FILTER
			{
				FILTER_MIN_MAG_MIP_POINT,
				FILTER_MIN_MAG_POINT_MIP_LINEAR,
				FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
				FILTER_MIN_POINT_MAG_MIP_LINEAR,
				FILTER_MIN_LINEAR_MAG_MIP_POINT,
				FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
				FILTER_MIN_MAG_LINEAR_MIP_POINT,
				FILTER_MIN_MAG_MIP_LINEAR,
				FILTER_ANISOTROPIC,
				FILTER_COMPARISON_MIN_MAG_MIP_POINT,
				FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
				FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
				FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
				FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
				FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
				FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
				FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
				FILTER_COMPARISON_ANISOTROPIC
			};

			enum E_TEXTURE_ADDRESS_MODE
			{
				TEXTURE_ADDRESS_WRAP,
				TEXTURE_ADDRESS_MIRROR,
				TEXTURE_ADDRESS_CLAMP,
				TEXTURE_ADDRESS_BORDER,
				TEXTURE_ADDRESS_MIRROR_ONCE
			};
		
			struct TSamplerStateDesc
			{
				E_FILTER				filter;
				E_TEXTURE_ADDRESS_MODE	addressU, addressV, addressW;
				float					mipLODBias;
				unsigned int			maxAnisotropy;
				E_COMPARISON_FUNC		comparisonFunc;
				float					borderColor[4];
				float					minLOD, maxLOD;
			};

			struct NOVTABLE ISamplerState
			{
				virtual operator TSamplerStateDesc() const = 0;
			};
#pragma endregion
#pragma endregion

#pragma region Resources
			struct IDeviceContext;

			template<class Resource, class Format>
			struct IMapped;

			enum E_FORMAT
			{
			};

			enum E_IB_FORMAT
			{
				IBF_16,
				IBF_32
			};

			struct NOVTABLE IResource
			{
			protected:
				//virtual Map(IDeviceContext &context) = 0;
			};

			struct NOVTABLE ICBuffer
			{
			};

			struct NOVTABLE IBuffer: public IResource
			{
			};

			struct NOVTABLE IStructuredBuffer: public IBuffer
			{
			};

			template<class Format>
			class CFormattedResData
			{
				operator Format() const;
				Format operator =(Format data);
			};

			template<>
			class CFormattedResData<void>
			{
			};

			template<class Format>
			class CResData: public CFormattedResData<Format>
			{
			public:
				operator void *();
			};

			template<class Format>
			struct NOVTABLE IRow
			{
				CResData<Format> operator [](unsigned x);
			};

			template<class Format>
			struct NOVTABLE ISlice
			{
				IRow<Format> operator [](unsigned y);
			};

			struct ITexture1D;

			template<class Format>
			struct NOVTABLE IMapped<ITexture1D, Format>: public IRow<Format>
			{
			};

			struct NOVTABLE IResource1D: public IResource
			{
				template<class Format>
				IMapped<ITexture1D, Format> Map(IDeviceContext &context, unsigned int mipSlice, unsigned int arraySlice);
			};

			struct NOVTABLE ITexture1D: public IResource1D
			{
				template<class Format>
				IMapped<ITexture1D, Format> Map(IDeviceContext &context, unsigned int mipSlice, unsigned int arraySlice, unsigned int left, unsigned int right);
			};

			struct NOVTABLE IDepthStencilTexture1D: public IResource1D
			{
			};

			struct TInitData2D
			{
				unsigned int pitch;
				const void *data;
			};

			struct ITexture2D;

			template<class Format>
			struct NOVTABLE IMapped<ITexture2D, Format>: public ISlice<Format>
			{
			};

			struct NOVTABLE ITexture2D: public IResource
			{
				template<class Format>
				IMapped<ITexture2D, Format> Map(IDeviceContext &context);
			};

			struct TInitData3D
			{
				unsigned int pitch, slicePitch;
				const void *data;
			};

			struct ITexture3D;

			template<class Format>
			struct NOVTABLE IMapped<ITexture3D, Format>
			{
				ISlice<Format> operator [](unsigned z);
			};

			struct NOVTABLE ITexture3D: public IResource
			{
				template<class Format>
				IMapped<ITexture3D, Format> Map(IDeviceContext &context);
			};
#pragma endregion

#pragma region Resource views
#pragma region DepthStencil
			enum E_DEPTH_STENCIL_FORMAT
			{
			};

			enum E_DSV_DIMENSION
			{
				DSV_DIMENSION_TEXTURE1D,
				DSV_DIMENSION_TEXTURE1DARRAY,
				DSV_DIMENSION_TEXTURE2D,
				DSV_DIMENSION_TEXTURE2DARRAY,
				DSV_DIMENSION_TEXTURE2DMS,
				DSV_DIMENSION_TEXTURE2DMSARRAY
			};

			struct TDepthStencilViewDesc
			{
				E_DEPTH_STENCIL_FORMAT	format;
				E_DSV_DIMENSION			viewDimension;
				bool					readOnlyDepth, readOnlyStencil;
				union
				{
					struct
					{
						unsigned int mipSlice;
					} texture1D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture1DArray;
					struct
					{
						unsigned int mipSlice;
					} texture2D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DArray;
					struct
					{
					} texture2DMS;
					struct
					{
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DMSArray;
				};
			};
			struct NOVTABLE IDepthStencilView
			{
				virtual operator TDepthStencilViewDesc() const = 0;
			};

#pragma endregion

#pragma region RenderTarget
			enum E_RENDER_TARGET_FORMAT
			{
			};

			enum E_RTV_DIMENSION
			{
				RTV_DIMENSION_BUFFER,
				RTV_DIMENSION_TEXTURE1D,
				RTV_DIMENSION_TEXTURE1DARRAY,
				RTV_DIMENSION_TEXTURE2D,
				RTV_DIMENSION_TEXTURE2DARRAY,
				RTV_DIMENSION_TEXTURE2DMS,
				RTV_DIMENSION_TEXTURE2DMSARRAY,
				RTV_DIMENSION_TEXTURE3D
			};

			struct TRenderTargetViewDesc
			{
				E_RENDER_TARGET_FORMAT	format;
				E_RTV_DIMENSION			viewDimension;
				union
				{
					struct
					{
						unsigned int elementOffset;
						unsigned int elementWidth;
					} buffer;
					struct
					{
						unsigned int mipSlice;
					} texture1D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture1DArray;
					struct
					{
						unsigned int mipSlice;
					} texture2D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DArray;
					struct
					{
					} texture2DMS;
					struct
					{
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DMSArray;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstWSlice;
						unsigned int wSize;
					} texture3D;
				};
			};

			struct NOVTABLE IRenderTargetView
			{
				virtual operator TRenderTargetViewDesc() const = 0;
			};
#pragma endregion

#pragma region ShaderResource
			enum E_SHADER_RESOURCE_FORMAT
			{
			};

			enum E_SRV_DIMENSION
			{
				SRV_DIMENSION_BUFFER,
				SRV_DIMENSION_TEXTURE1D,
				SRV_DIMENSION_TEXTURE1DARRAY,
				SRV_DIMENSION_TEXTURE2D,
				SRV_DIMENSION_TEXTURE2DARRAY,
				SRV_DIMENSION_TEXTURE2DMS,
				SRV_DIMENSION_TEXTURE2DMSARRAY,
				SRV_DIMENSION_TEXTURE3D,
				SRV_DIMENSION_TEXTURECUBE,
				SRV_DIMENSION_TEXTURECUBEARRAY,
				SRV_DIMENSION_BUFFEREX
			};

			struct TShaderResourceViewDesc
			{
				E_SHADER_RESOURCE_FORMAT	format;
				E_SRV_DIMENSION				viewDimension;
				union
				{
					struct
					{
						unsigned int elementOffset;
						unsigned int elementWidth;
					} buffer;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
					} texture1D;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture1DArray;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
					} texture2D;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DArray;
					struct
					{
					} texture2DMS;
					struct
					{
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DMSArray;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
					} texture3D;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
					} textureCube;
					struct
					{
						unsigned int mostDetailedMip;
						unsigned int mipLevels;
						unsigned int first2DArrayFace;
						unsigned int numCubes;
					} textureCubeArray;
					struct
					{
						unsigned int firstElement;
						unsigned int numElements;
						bool raw;
					} bufferEx;
				};
			};

			struct NOVTABLE IShaderResourceView
			{
				virtual operator TShaderResourceViewDesc() const = 0;
			};
#pragma endregion

#pragma region UnorderedAccess
			enum E_UNORDERED_ACCESS_FORMAT
			{
			};

			enum E_UAV_DIMENSION
			{
				UAV_DIMENSION_BUFFER,
				UAV_DIMENSION_TEXTURE1D,
				UAV_DIMENSION_TEXTURE1DARRAY,
				UAV_DIMENSION_TEXTURE2D,
				UAV_DIMENSION_TEXTURE2DARRAY,
				UAV_DIMENSION_TEXTURE3D
			};

			struct TUnorderedAccessView
			{
				E_UNORDERED_ACCESS_FORMAT	format;
				E_UAV_DIMENSION				viewDimension;
				union
				{
					struct
					{
						unsigned int firstElement;
						unsigned int numElements;
						bool raw, append, counter;
					} buffer;
					struct
					{
						unsigned int mipSlice;
					} texture1D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture1DArray;
					struct
					{
						unsigned int mipSlice;
					} texture2D;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstArraySlice;
						unsigned int arraySize;
					} texture2DArray;
					struct
					{
						unsigned int mipSlice;
						unsigned int firstWSlice;
						unsigned int wSize;
					} texture3D;
				};
			};

			struct NOVTABLE IUnorderedAccessView
			{
				virtual operator TUnorderedAccessView() const  = 0;
			};
#pragma endregion
#pragma endregion

			enum E_PRIMITIVE_TOPOLOGY
			{
				PRIMITIVE_TOPOLOGY_UNDEFINED					= 0,
				PRIMITIVE_TOPOLOGY_POINTLIST					= 1,
				PRIMITIVE_TOPOLOGY_LINELIST                     = 2,
				PRIMITIVE_TOPOLOGY_LINESTRIP                    = 3,
				PRIMITIVE_TOPOLOGY_TRIANGLELIST                 = 4,
				PRIMITIVE_TOPOLOGY_TRIANGLESTRIP                = 5,
				PRIMITIVE_TOPOLOGY_LINELIST_ADJ                 = 10,
				PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ                = 11,
				PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ             = 12,
				PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ            = 13,
				PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST    = 33,
				PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST    = 34,
				PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST    = 35,
				PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST    = 36,
				PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST    = 37,
				PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST    = 38,
				PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST    = 39,
				PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST    = 40,
				PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST    = 41,
				PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST   = 42,
				PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST   = 43,
				PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST   = 44,
				PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST   = 45,
				PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST   = 46,
				PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST   = 47,
				PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST   = 48,
				PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST   = 49,
				PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST   = 50,
				PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST   = 51,
				PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST   = 52,
				PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST   = 53,
				PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST   = 54,
				PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST   = 55,
				PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST   = 56,
				PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST   = 57,
				PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST   = 58,
				PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST   = 59,
				PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST   = 60,
				PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST   = 61,
				PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST   = 62,
				PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST   = 63,
				PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST   = 64
			};

			struct NOVTABLE IDeviceContext
			{
				//~IDeviceContext() = default;

				// low level IA
				virtual E_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const = 0;
				virtual RESTRICT IInputLayout *GetInputLayout() const = 0;
				virtual void GetIndexBuffer(IBuffer *&IB, E_IB_FORMAT &format, unsigned int &offset) const = 0;
				virtual void GetVertexBuffers(unsigned int startSlot, unsigned int VBCount, _In_count_(VBCount) IBuffer *VBs[], _In_count_(VBCount) unsigned int strides[], _In_count_(VBCount) unsigned int offsets[]) const = 0;
				virtual void SetPrimitiveTopology(E_PRIMITIVE_TOPOLOGY topology) = 0;
				virtual void SetInputLayout(IInputLayout *layout) = 0;
				virtual void SetIndexBuffer(IBuffer *IB, E_IB_FORMAT format, unsigned int offset) = 0;
				virtual void SetVertexBuffers(unsigned int startSlot, unsigned int VBCount, _In_count_(VBCount) IBuffer *const VBs[], _In_count_(VBCount) const unsigned int strides[], _In_count_(VBCount) const unsigned int offsets[]) = 0;

				// low level immediate draw commands
				virtual void Draw(unsigned int vertexCount, unsigned int startVertexLocation) = 0;
				virtual void DrawIndexed(unsigned int indexCount, unsigned int startIndexLocation, int baseVertexLocation) = 0;
				virtual void DrawInstanced(unsigned int vertexCount, unsigned int instanceCount, unsigned int startVertexLocation, unsigned int startInstanceLocation) = 0;
				virtual void DrawInstancedIndirect(IBuffer &args, unsigned int offset) = 0;
				virtual void DrawIndexedInstancedIndirtect(IBuffer &args, unsigned int offset) = 0;
				virtual void DrawAuto() = 0;

				// specify geometry from sysmem
				// sysmem buffer may needed or not needed to be preserved until Draw* call depending on implementation
				virtual void SetIndexBuffer(unsigned int size, _In_count_(size / sizeof(uint16_t)) const uint16_t IB[]) = 0;
				virtual void SetIndexBuffer(unsigned int size, _In_count_(size / sizeof(uint32_t)) const uint32_t IB[]) = 0;
				virtual void SetVertexBuffers(unsigned int startSlot, unsigned int VBCount, _In_count_(VBCount) const unsigned int sizes[], _In_count_(VBCount) const unsigned int strides[], _In_count_(VBCount) const void *VBs[]) = 0;

				// legacy FFP
				virtual void BindTexture(ITexture2D *texture, unsigned int layer = 0) = 0;
				virtual void SetColor(uint32_t color) = 0;
				virtual void SetWorldViewTransform(const float matrix[4][3]) = 0;
				virtual void SetProjTranform(const float matrix[4][4]) = 0;

				virtual void test() = 0;
			};

			struct NOVTABLE IDevice
			{
				//~IDevice() = default;
				virtual RESTRICT IDeviceContext *GetDeviceContext() = 0;

				virtual TCaps GetCaps() = 0;

				// create state objects
				virtual RESTRICT IBlendState *CreateBlendState(const TBlendStateDesc &desc) = 0;
				virtual RESTRICT IDepthStencilState *CreateDepthStencilState(const TDepthStencilDesc &desc) = 0;
				virtual RESTRICT IInputLayout *CreateInputLayout(unsigned int descCount, _In_count_(numElements) const TInputElementDesc descs[]) = 0;
				virtual RESTRICT IRasterizerState *CreateRasterizerState(const TRasterizerStateDesc &desc) = 0;
				virtual RESTRICT ISamplerState *CreateSamplerState(const TSamplerStateDesc &desc) = 0;

				// create resources
				virtual RESTRICT ICBuffer *CreateCBuffer(unsigned int size, const void *initData = NULL) = 0;
				virtual RESTRICT IBuffer *CreateBuffer(unsigned int size, bool IB, bool VB, bool SO, bool SR, bool RT, bool UA, const void *initData = NULL) = 0;
				virtual RESTRICT IStructuredBuffer *CreateStructuredBuffer(unsigned int structSize, unsigned int structCount, const void *initData = NULL) = 0;
				virtual RESTRICT ITexture1D *CreateTexture1D(unsigned int width, unsigned int mipLevels, unsigned int arraySize, E_FORMAT format, bool SR, bool DS, bool RT, bool UA, const void *initData = NULL) = 0;
				virtual RESTRICT ITexture2D *CreateTexture2D(unsigned int width, unsigned int height, unsigned int mipLevels, unsigned int arraySize, E_FORMAT format, bool cubeMap, bool SR, bool DS, bool RT, bool UA, const TInitData2D *initData = NULL) = 0;
				virtual RESTRICT ITexture3D *CreateTexture3D(unsigned int width, unsigned int height, unsigned int depth, unsigned int mipLevels, E_FORMAT format, bool SR, bool RT, bool UA, const TInitData3D *initData = NULL) = 0;

				// create resource views
				virtual void TestDepthStencilViewDesc(IResource &resource, const TDepthStencilViewDesc *desc = NULL) = 0;
				virtual void TestRenderTargetViewDesc(IResource &resource, const TRenderTargetViewDesc *desc = NULL) = 0;
				virtual void TestShaderResourceViewDesc(IResource &resource, const TShaderResourceViewDesc *desc = NULL) = 0;
				virtual void TestUnordererAccessViewDesc(IResource &resource, const TUnorderedAccessView *desc = NULL) = 0;
				virtual RESTRICT IDepthStencilView *CreateDepthStencilView(IResource &resource, const TDepthStencilViewDesc *desc = NULL) = 0;
				virtual RESTRICT IRenderTargetView *CreateRenderTargetView(IResource &resource, const TRenderTargetViewDesc *desc = NULL) = 0;
				virtual RESTRICT IShaderResourceView *CreateShaderResourceView(IResource &resource, const TShaderResourceViewDesc *desc = NULL) = 0;
				virtual RESTRICT IUnorderedAccessView *CreateUnorderedResourceView(IResource &resource, const TUnorderedAccessView *desc = NULL) = 0;
			};

			struct NOVTABLE ICommandList
			{
			};
		}

		namespace HighLevel
		{
			namespace Textures
			{
				struct NOVTABLE ITexture1D: virtual public IDtor
				{
				protected:
					~ITexture1D() = default;
				};

				struct NOVTABLE ITexture2D: virtual public IDtor
				{
				protected:
					~ITexture2D() = default;
				};

				struct NOVTABLE ITexture3D: virtual public IDtor
				{
				protected:
					~ITexture3D() = default;
				};

				struct NOVTABLE ITextureCube: virtual public IDtor
				{
				protected:
					~ITextureCube() = default;
				};

				struct NOVTABLE IMatrialTexture: virtual public IDtor
				{
				protected:
				protected:
					~IMatrialTexture() = default;
				};

				struct NOVTABLE IMatrialTexture2D: public IMatrialTexture
				{
				protected:
					~IMatrialTexture2D() = default;
				};

				struct NOVTABLE IMatrialTexture3D: public IMatrialTexture
				{
				protected:
					~IMatrialTexture3D() = default;
				};
			}

			namespace Materials
			{
				enum class E_TEX_MAPPING
				{
					UV,		// standard 2D tex coords
					XYZ,	// object space
				};

				enum class E_NORMAL_TECHNIQUE
				{
					UNPERTURBED			= 0,
					NORMAL_MAP_XY		= 1,
					HEIGHT_MAP_HW_DIFF	= 2,
				};

				enum class E_PARALLAX_TECHNIQUE
				{
					NONE	= 0,
					SPHERE	= 1,
					PLANE	= 2,
				};

				struct NOVTABLE IMaterial: virtual public IDtor
				{
					virtual void SetAmbientColor(const float color[3]) = 0;
					virtual void SetDiffuseColor(const float color[3]) = 0;
					virtual void SetSpecularColor(const float color[3]) = 0;
					virtual void SetHeightScale(float scale) = 0;
					virtual void SetEnvAmount(float amount) = 0;
					virtual void SetShininess(float shininess) = 0;
					virtual void SetTexMappingMode(E_TEX_MAPPING texMapping) = 0;
					virtual void SetNormalTechnique(E_NORMAL_TECHNIQUE technique) = 0;
					virtual void SetParallaxTechnique(E_PARALLAX_TECHNIQUE technique) = 0;
					virtual void SetDiffuseTexture(Textures::IMatrialTexture *texture) = 0;
					virtual void SetSpecularTexture(Textures::IMatrialTexture *texture) = 0;
					virtual void SetNormalTexture(Textures::IMatrialTexture *texture) = 0;
					virtual void SetHeightTexture(Textures::IMatrialTexture *texture) = 0;
					virtual void SetEnvTexture(Textures::IMatrialTexture *texture) = 0;
					virtual void SetEnvMask(Textures::IMatrialTexture *texture) = 0;
				protected:
					~IMaterial() = default;
				};
			}

			namespace Geometry
			{
				// TODO: use math lib
				template<unsigned dimension>
				class AABB
				{
					static_assert(dimension == 2 || dimension == 3, "AABB dimension can be either 2 or 3");
				public:
					explicit AABB() noexcept
					{
						for (unsigned i = 0; i < dimension; i++)
						{
							_min[i] = +std::numeric_limits<float>::infinity();
							_max[i] = -std::numeric_limits<float>::infinity();
						}
					}

					template<class TIterator>
					inline AABB(TIterator begin, TIterator end);

					inline void Refit(const AABB &aabb);

					const float (&Min() const noexcept)[dimension]
					{
						return _min;
					}

					const float (&Max() const noexcept)[dimension]
					{
						return _max;
					}

					//const vec3 Center() const
					//{
					//	return nv_zero_5 * (_max + _min);
					//}

					//const vec3 Extents() const
					//{
					//	return nv_zero_5 * (_max - _min);
					//}
				private:
					inline void Refit(const float (&min)[dimension], const float (&max)[dimension]);
				private:
					float _min[dimension], _max[dimension];
				};

#pragma region AABB impl
				template<unsigned dimension>
				void AABB<dimension>::Refit(const float (&min)[dimension], const float (&max)[dimension])
				{
					for (unsigned i = 0; i < dimension; i++)
					{
						_min[i] = std::min(_min[i], min[i]);
						_max[i] = std::max(_max[i], max[i]);
					}
				}

				template<unsigned dimension>
				template<class TIterator>
				AABB<dimension>::AABB(TIterator begin, TIterator end): AABB()
				{
					std::for_each(begin, end, [this](const float (&vertex)[dimension]){Refit(vertex, vertex);});
				}

				template<unsigned dimension>
				void AABB<dimension>::Refit(const AABB &aabb)
				{
					Refit(aabb._min, aabb._max);
				}
#pragma endregion

				struct NOVTABLE IMesh: virtual public IDtor
				{
					virtual AABB<3> GetAABB() const = 0;
				protected:
					~IMesh() = default;
				};
			}

			namespace Instances
			{
				namespace _2D
				{
					struct NOVTABLE IRect: virtual public IDtor
					{
						virtual void SetPos(float x, float y) = 0;
						virtual void SetExtents(float x, float y) = 0;
						virtual void SetColor(uint32_t color) = 0;
						virtual void SetAngle(float angle) = 0;
					protected:
						~IRect() = default;
					};

					struct NOVTABLE IEllipse: virtual public IDtor
					{
						virtual void SetPos(float x, float y) = 0;
						virtual void SetRadii(float rx, float ry) = 0;
						virtual void SetColor(uint32_t color) = 0;
						virtual void SetAngle(float angle) = 0;
					protected:
						~IEllipse() = default;
					};
				}

				struct NOVTABLE IInstance: virtual public IDtor
				{
				protected:
					~IInstance() = default;
				};
			}

			namespace DisplayModes
			{
				struct NOVTABLE IDesc: virtual public IDtor
				{
					virtual operator const char *() const = 0;
				protected:
					~IDesc() = default;
				};

				struct TDispMode
				{
					const unsigned int width, height;
					const float refreshRate;
				};

				struct NOVTABLE IDisplayModes
				{
					friend struct TDispModeDesc;
				public:
					class CIterator;
					virtual unsigned Count() const = 0;
					inline TDispModeDesc operator [](unsigned idx) const;//TDispModeDesc here references to DisplayModes::TDispModeDesc
					inline CIterator begin() const;
					inline CIterator end() const;
				protected:
					struct TDispModeDesc: TDispMode
					{
						TDispModeDesc(const TDispMode &mode, const IDesc *const desc): TDispMode(mode), desc(desc) {}
						const IDesc *const desc;
					};
				private:
					virtual TDispModeDesc Get(unsigned idx) const = 0;
				protected:
					~IDisplayModes() = default;
				};

				struct TDispModeDesc: TDispMode
				{
					const struct TDesc
					{
						operator const char *() const
						{
							return *_desc;
						}
					private:
						friend struct TDispModeDesc;
						TDesc(const IDesc *desc): _desc(desc, dtor) {}
						TDesc(const TDesc &&src): 
						// remove const from _desc in order to call (&&) ctor instead of (const &) one
						_desc(std::move(const_cast<std::remove_const<decltype(src._desc)>::type &>(src._desc)))
						{
						}
						const std::shared_ptr<const IDesc> _desc;
					} desc;
					//TDispModeDesc(const TDispModeDesc &&src):
					//	TDispMode(src),
					//	desc(std::forward<decltype(src.desc)>(src.desc))
					//{
					//}
				private:
					friend TDispModeDesc IDisplayModes::operator[](unsigned) const;
					TDispModeDesc(const IDisplayModes::TDispModeDesc &modeDesc): TDispMode(modeDesc), desc(modeDesc.desc) {}
				};

				TDispModeDesc IDisplayModes::operator [](unsigned idx) const
				{
					return Get(idx);
				}

				class IDisplayModes::CIterator: public std::iterator<std::random_access_iterator_tag, const DisplayModes::TDispModeDesc, unsigned>
				{
					friend class IDisplayModes;
				public:
					inline DisplayModes::TDispModeDesc operator *() const;
					inline bool operator ==(CIterator cmp) const;
					inline bool operator !=(CIterator cmp) const;
					inline bool operator <(CIterator cmp) const;
					inline CIterator &operator +=(difference_type offset);
					inline CIterator &operator -=(difference_type offset);
					inline CIterator &operator ++();
					inline CIterator &operator --();
					inline CIterator operator ++(int);
					inline CIterator operator --(int);
					inline CIterator operator +(difference_type offset) const;
					inline difference_type operator -(CIterator subtrahend) const;
				private:
					inline CIterator(const IDisplayModes &modes, unsigned idx);
					inline void _CheckRange() const;
					inline static void _CheckContainer(CIterator iter1, CIterator iter2);
					const IDisplayModes &_modes;
					unsigned _idx;
				};

#pragma region IDisplayModes::CIterator impl
				void IDisplayModes::CIterator::_CheckRange() const
				{
					assert(_idx >= 0);
					assert(_idx < _modes.Count());
				}

				void IDisplayModes::CIterator::_CheckContainer(CIterator iter1, CIterator iter2)
				{
					assert(&iter1._modes == &iter2._modes);
				}

				TDispModeDesc IDisplayModes::CIterator::operator *() const
				{
					_CheckRange();
					return _modes[_idx];
				}

				bool IDisplayModes::CIterator::operator ==(CIterator cmp) const
				{
					_CheckContainer(*this, cmp);
					return _idx == cmp._idx;
				}

				bool IDisplayModes::CIterator::operator !=(CIterator cmp) const
				{
					return !operator ==(cmp);
				}

				bool IDisplayModes::CIterator::operator <(CIterator cmp) const
				{
					_CheckContainer(*this, cmp);
					return _idx < cmp._idx;
				}

				IDisplayModes::CIterator &IDisplayModes::CIterator::operator +=(difference_type offset)
				{
					_idx += offset;
					return *this;
				}

				IDisplayModes::CIterator &IDisplayModes::CIterator::operator -=(difference_type offset)
				{
					_idx -= offset;
					return *this;
				}

				IDisplayModes::CIterator &IDisplayModes::CIterator::operator ++()
				{
					return operator +=(1);
				}

				IDisplayModes::CIterator &IDisplayModes::CIterator::operator --()
				{
					return operator -=(1);
				}

				IDisplayModes::CIterator IDisplayModes::CIterator::operator ++(int)
				{
					CIterator old(*this);
					operator ++();
					return old;
				}

				IDisplayModes::CIterator IDisplayModes::CIterator::operator --(int)
				{
					CIterator old(*this);
					operator --();
					return old;
				}

				IDisplayModes::CIterator IDisplayModes::CIterator::operator +(difference_type offset) const
				{
					return CIterator(*this) += offset;
				}

				IDisplayModes::CIterator::difference_type IDisplayModes::CIterator::operator -(CIterator subtrahend) const
				{
					_CheckContainer(*this, subtrahend);
					return _idx - subtrahend._idx;
				}

				IDisplayModes::CIterator::CIterator(const IDisplayModes &modes, unsigned idx): _modes(modes), _idx(idx) {}
#pragma endregion

				IDisplayModes::CIterator IDisplayModes::begin() const
				{
					return CIterator(*this, 0);
				}

				IDisplayModes::CIterator IDisplayModes::end() const
				{
					return CIterator(*this, Count());
				}

				extern "C" const IDisplayModes &GetDisplayModes();
			}

			struct NOVTABLE IRenderer: virtual public IDtor
			{
				virtual void SetMode(unsigned int width, unsigned int height) = 0;
				virtual void SetMode(unsigned idx) = 0;
				virtual void ToggleFullscreen(bool fullscreen) = 0;
				virtual void NextFrame() const = 0;

				// resource creation
				// TODO: fill in function args
				//virtual RESTRICT Textures::ITexture1D *CreateTexture1D() = 0;
				//virtual RESTRICT Textures::ITexture2D *CreateTexture2D() = 0;
				//virtual RESTRICT Textures::ITexture3D *CreateTexture3D() = 0;
				//virtual RESTRICT Textures::ITextureCube *CreateTextureCube() = 0;
				//virtual RESTRICT Materials::IMaterial *CreateMaterial() = 0;
				//virtual RESTRICT Geometry::IMesh *CreateMesh(unsigned int icount, _In_count_(icount) const uint32_t *idx, unsigned int vcount, _In_count_(vcount) const float *coords) = 0;
				//virtual RESTRICT Instances::IInstance *CreateInstance(const Geometry::IMesh &mesh, const Materials::IMaterial &material) = 0;

				// immediate 2D
				// consider packing coords and color into single struct (array of structs instead of struct of arrays)(it may results in better memory access pattern)
				virtual void DrawPoint(float x, float y, uint32_t color, float size = 1) const = 0;
				virtual void DrawLine(unsigned int vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32_t colors[], bool closed, float width = 1) const = 0;
				//virtual void DrawTriangles(unsigned int vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32_t colors[]) const = 0;
				virtual void DrawRect(float x, float y, float width, float height, uint32_t color, Textures::ITexture2D *texture, float angle = 0) const = 0;
				virtual void DrawRect(float x, float y, float width, float height, uint32_t (&colors)[4], Textures::ITexture2D *texture, float angle = 0) const = 0;
				virtual void DrawEllipse(float x, float y, float rx, float ry, uint32_t color, bool AA, float angle = 0) const = 0;

				// TODO: move this to engine's derived interface
				virtual void DrawPolygon(unsigned int vcount, _In_count_(vcount) const float coords[][2], uint32_t color) const = 0;
				virtual void DrawCircle(float x, float y, float r, uint32_t color) const = 0;

				// 2D scene
				virtual RESTRICT Instances::_2D::IRect *AddRect(bool dynamic, uint16_t layer, float x, float y, float width, float height, uint32_t color, float angle = 0) = 0;
				virtual RESTRICT Instances::_2D::IEllipse *AddEllipse(bool dynamic, uint16_t layer, float x, float y, float rx, float ry, uint32_t color, bool AA, float angle = 0) = 0;
			protected:
				~IRenderer() = default;
			};

			// TODO: replace HWND with engine cross-platform handle
			extern "C" RESTRICT IRenderer *CreateRenderer(HWND hwnd, unsigned int width, unsigned int height, bool fullscreen = false, unsigned int refreshRate = 0, bool multithreaded = true);
		}
	}
}