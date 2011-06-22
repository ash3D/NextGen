/**
\author		Alexey Shaydurov aka ASH
\date		22.3.2011 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __LOW_LEVEL_RENDERER_H__
#define __LOW_LEVEL_RENDERER_H__

// clarify MSVC version
#ifndef _MSC_VER
#define _In_count_(size)
#endif

#pragma region("considerations")
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

#include <string>

namespace DGLE2
{
	//class I
	namespace LowLevelRenderer
	{
#pragma region("State objects")
#pragma region("Common")
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

#pragma region("Blend")
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
			bool lphaToCoverageEnable, independentBlendEnable;
			struct
			{
				bool		blendEnable;
				E_BLEND		srcBlend;
				E_BLEND		destBlend;
				E_BLEND_OP	blendOp;
				E_BLEND		srcBlendAlpha;
				E_BLEND		destBlendAlpha;
				E_BLEND		blendOpAlpha;
				ubyte		renderTargetWriteMask;
			} renderTarget[8];
		};

		class IBlendState
		{
		public:
			virtual operator TBlendStateDesc() const = 0;
		};
#pragma endregion

#pragma region("DepthStencil")
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
			ubyte				stencilReadMask;
			ubyte				stencilWriteMask;
			struct
			{
				E_STENCIL_OP		stencilFailOp;
				E_STENCIL_OP		stencilDepthFailOp;
				E_STENCIL_OP		stencilPassOp;
				E_COMPARISON_FUNC	stencilFunc;
			}					frontFace, backFace;
		};

		class IDepthStencilState
		{
		public:
			virtual operator TDepthStencilDesc() const = 0;
		};
#pragma endregion

#pragma region("InputLayout")
		enum E_VERTEX_FORMAT
		{
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
			uint					semanticIndex;
			E_VERTEX_FORMAT			format;
			uint					inputSlot;
			uint					alignedByteOffset;
			E_INPUT_CLASSIFICATION	inputSlotClass;
			uint					instanceDataStepRate;
		};

		class IInputLayout
		{
		public:
		};
#pragma endregion

#pragma region("Rasterizer")
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

		class IRasterizerState
		{
		public:
			virtual operator TRasterizerStateDesc() const = 0;
		};
#pragma endregion

#pragma region("Sampler")
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
			uint					maxAnisotropy;
			E_COMPARISON_FUNC		comparisonFunc;
			float					borderColor[4];
			float					minLOD, maxLOD;
		};

		class ISamplerState
		{
		public:
			virtual operator TSamplerStateDesc() const = 0;
		};
#pragma endregion
#pragma endregion

#pragma region("Resources")
		class IDeviceContext;

		template<class Resource, class Format>
		class IMapped;

		enum E_FORMAT
		{
		};

		enum E_IB_FORMAT
		{
			IBF_16,
			IBF_32
		};

		class IResource
		{
		public:
		protected:
			//virtual Map(IDeviceContext &context) = 0;
		};

		class ICBuffer
		{
		public:
		};

		class IBuffer: public IResource
		{
		public:
		};

		class IStructuredBuffer: public IBuffer
		{
		public:
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
		class IRow
		{
		public:
			CResData<Format> operator [](uint x);
		};

		template<class Format>
		class ISlice
		{
		public:
			IRow<Format> operator [](uint y);
		};

		class ITexture1D;

		template<class Format>
		class IMapped<ITexture1D, Format>: public IRow<Format>
		{
		public:
		};

		class IResource1D: public IResource
		{
		public:
			template<class Format>
			IMapped<ITexture1D, Format> Map(IDeviceContext &context, uint mipSlice, uint arraySlice);
		};

		class ITexture1D: public IResource1D
		{
		public:
			template<class Format>
			IMapped<ITexture1D, Format> Map(IDeviceContext &context, uint mipSlice, uint arraySlice, uint left, uint right);
		};

		class IDepthStencilTexture1D: public IResource1D
		{
		public:
		};

		class TInitData2D
		{
			TInitData2D(uint pitch, const void *data): pitch(pitch), data(data) {}
			uint pitch;
			const void *data;
		};

		class ITexture2D;

		template<class Format>
		class IMapped<ITexture2D, Format>: public ISlice<Format>
		{
		public:
		};

		class ITexture2D: public IResource
		{
		public:
			template<class Format>
			IMapped<ITexture2D, Format> Map(IDeviceContext &context);
		};

		class TInitData3D
		{
			TInitData3D(uint pitch, uint slicePitch, const void *data): pitch(pitch), slicePitch(slicePitch), data(data) {}
			uint pitch, slicePitch;
			const void *data;
		};

		class ITexture3D;

		template<class Format>
		class IMapped<ITexture3D, Format>
		{
		public:
			ISlice<Format> operator [](uint z);
		};

		class ITexture3D: public IResource
		{
		public:
			template<class Format>
			IMapped<ITexture3D, Format> Map(IDeviceContext &context);
		};
#pragma endregion

#pragma region("Resource views")
#pragma region("DepthStencil")
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
					uint mipSlice;
				} texture1D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture1DArray;
				struct
				{
					uint mipSlice;
				} texture2D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture2DArray;
				struct
				{
				} texture2DMS;
				struct
				{
					uint firstArraySlice;
					uint arraySize;
				} texture2DMSArray;
			};
		};
		class IDepthStencilView
		{
		public:
			virtual operator TDepthStencilViewDesc() const = 0;
		};

#pragma endregion

#pragma region("RenderTarget")
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
					uint elementOffset;
					uint elementWidth;
				} buffer;
				struct
				{
					uint mipSlice;
				} texture1D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture1DArray;
				struct
				{
					uint mipSlice;
				} texture2D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture2DArray;
				struct
				{
				} texture2DMS;
				struct
				{
					uint firstArraySlice;
					uint arraySize;
				} texture2DMSArray;
				struct
				{
					uint mipSlice;
					uint firstWSlice;
					uint wSize;
				} texture3D;
			};
		};

		class IRenderTargetView
		{
		public:
			virtual operator TRenderTargetViewDesc() const = 0;
		};
#pragma endregion

#pragma region("ShaderResource")
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
					uint elementOffset;
					uint elementWidth;
				} buffer;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
				} texture1D;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
					uint firstArraySlice;
					uint arraySize;
				} texture1DArray;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
				} texture2D;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
					uint firstArraySlice;
					uint arraySize;
				} texture2DArray;
				struct
				{
				} texture2DMS;
				struct
				{
					uint firstArraySlice;
					uint arraySize;
				} texture2DMSArray;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
				} texture3D;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
				} textureCube;
				struct
				{
					uint mostDetailedMip;
					uint mipLevels;
					uint first2DArrayFace;
					uint numCubes;
				} textureCubeArray;
				struct
				{
					uint firstElement;
					uint numElements;
					bool raw;
				} bufferEx;
			};
		};

		class IShaderResourceView
		{
		public:
			virtual operator TShaderResourceViewDesc() const = 0;
		};
#pragma endregion

#pragma region("UnorderedAccess")
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
					uint firstElement;
					uint numElements;
					bool raw, append, counter;
				} buffer;
				struct
				{
					uint mipSlice;
				} texture1D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture1DArray;
				struct
				{
					uint mipSlice;
				} texture2D;
				struct
				{
					uint mipSlice;
					uint firstArraySlice;
					uint arraySize;
				} texture2DArray;
				struct
				{
					uint mipSlice;
					uint firstWSlice;
					uint wSize;
				} texture3D;
			};
		};

		class IUnorderedAccessView
		{
		public:
			virtual operator TUnorderedAccessView() const  = 0;
		};
#pragma endregion
#pragma endregion

#pragma region("High level textures")
		namespace HighLevelTextures
		{
			// usefull for LUT
			class ITexture1D
			{
			};

			class ITexture2D
			{
			};

			class ITexture3D
			{
			};

			class ITextureCube
			{
			};

			class IMatrialTexture
			{
			};

			class IMatrialTexture2D: public IMatrialTexture
			{
			};

			class IMatrialTexture3D: public IMatrialTexture
			{
			};
		}
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

		class IDeviceContext
		{
		public:
			virtual ~IDeviceContext() {}
			// 2D
			// !
			// consider packing coords and color into single struct (it may results in better memory access pattern)
			virtual void DrawPoint(float x, float y, uint32 color, float size = 1) = 0;
			virtual void DrawLine(uint vcount, _In_count_(vcount) const float coords[][2], _In_count_(vcount) const uint32 colors[], bool closed, float width = 1) = 0;
			virtual void DrawRect(float x, float y, float width, float height, uint32 color, float angle = 0) = 0;
			virtual void DrawRect(float x, float y, float width, float height, uint32 (&colors)[4], float angle = 0) = 0;
			virtual void DrawPolygon(uint vcount, _In_count_(vcount) const float coords[][2], uint32 color, float angle = 0) = 0;
			virtual void DrawCircle(float x, float y, float r, uint32 color) = 0;
			virtual void DrawEllipse(float x, float y, float rx, float ry, uint32 color, float angle = 0) = 0;

			// low level immediate draw commands
			virtual void Draw(uint vertexCount, uint startVertexLocation) = 0;
			virtual void DrawAuto() = 0;
			virtual void DrawIndexed(uint indexCount, uint startIndexLocation, int baseVertexLocation) = 0;
			virtual void DrawIndexedInstancedIndirtect(IBuffer &args, uint offset) = 0;
			virtual void DrawInstanced(uint vertexCount, uint instanceCount, uint startVertexLocation, uint startInstanceLocation) = 0;
			virtual void DrawInstancedIndirect(IBuffer &args, uint offset) = 0;

			// low level IA
			virtual E_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const = 0;
			virtual IInputLayout *GetInputLayout() const = 0;
			virtual void GetIndexBuffer(IBuffer *&IB, E_IB_FORMAT &format, uint &offset) const = 0;
			virtual void GetVertexBuffers(uint startSlot, uint VBCount, _In_count_(VBCount) IBuffer *VBs[], _In_count_(VBCount) uint strides[], _In_count_(VBCount) uint offsets[]) const = 0;
			virtual void SetPrimitiveTopology(E_PRIMITIVE_TOPOLOGY topology) = 0;
			virtual void SetInputLayout(IInputLayout *layout) = 0;
			virtual void SetIndexBuffer(IBuffer *IB, E_IB_FORMAT format, uint offset) = 0;
			virtual void SetVertexBuffers(uint startSlot, uint VBCount, _In_count_(VBCount) IBuffer *const VBs[], _In_count_(VBCount) const uint strides[], _In_count_(VBCount) const uint offsets[]) = 0;

			// specify geometry from sysmem
			virtual void SetIndexBuffer(uint size, _In_count_(size / sizeof(uint16)) const uint16 IB[]) = 0;
			virtual void SetIndexBuffer(uint size, _In_count_(size / sizeof(uint32)) const uint32 IB[]) = 0;
			virtual void SetVertexBuffers(uint size, _In_count_(size) const void *VB) = 0;

			// legacy FFP
			virtual void BindTexture(ITexture2D *texture, uint layer = 0) = 0;
		};

		class IDevice
		{
		public:
			virtual ~IDevice() {}
			virtual IDeviceContext *GetDeviceContext() = 0;
			virtual void SetMode(uint width, uint height) = 0;
			virtual void SetMode(uint idx) = 0;
			virtual void ToggleFullscreen(bool fullscreen) = 0;
			virtual void test() = 0;

			// Create state objects
			virtual IBlendState *CreateBlendState(const TBlendStateDesc &desc) = 0;
			virtual IDepthStencilState *CreateDepthStencilState(const TDepthStencilDesc &desc) = 0;
			virtual IInputLayout *CreateInputLayout(uint descCount, _In_count_(numElements) const TInputElementDesc descs[]) = 0;
			virtual IRasterizerState *CreateRasterizerState(const TRasterizerStateDesc &desc) = 0;
			virtual ISamplerState *CreateSamplerState(const TSamplerStateDesc &desc) = 0;

			// Create resources
			virtual ICBuffer *CreateCBuffer(uint size, const void *initData = NULL) = 0;
			virtual IBuffer *CreateBuffer(uint size, bool IB, bool VB, bool SO, bool SR, bool RT, bool UA, const void *initData = NULL) = 0;
			virtual IStructuredBuffer *CreateStructuredBuffer(uint structSize, uint structCount, const void *initData = NULL) = 0;
			virtual ITexture1D *CreateTexture1D(uint width, uint mipLevels, uint arraySize, E_FORMAT format, bool SR, bool DS, bool RT, bool UA, const void *initData = NULL) = 0;
			virtual ITexture2D *CreateTexture2D(uint width, uint height, uint mipLevels, uint arraySize, E_FORMAT format, bool cubeMap, bool SR, bool DS, bool RT, bool UA, const TInitData2D *initData = NULL) = 0;
			virtual ITexture3D *CreateTexture3D(uint width, uint height, uint depth, uint mipLevels, E_FORMAT format, bool SR, bool RT, bool UA, const TInitData3D *initData = NULL) = 0;

			// Create resource views
			virtual void TestDepthStencilViewDesc(IResource &resource, const TDepthStencilViewDesc *desc = NULL) = 0;
			virtual void TestRenderTargetViewDesc(IResource &resource, const TRenderTargetViewDesc *desc = NULL) = 0;
			virtual void TestShaderResourceViewDesc(IResource &resource, const TShaderResourceViewDesc *desc = NULL) = 0;
			virtual void TestUnordererAccessViewDesc(IResource &resource, const TUnorderedAccessView *desc = NULL) = 0;
			virtual IDepthStencilView *CreateDepthStencilView(IResource &resource, const TDepthStencilViewDesc *desc = NULL) = 0;
			virtual IRenderTargetView *CreateRenderTargetView(IResource &resource, const TRenderTargetViewDesc *desc = NULL) = 0;
			virtual IShaderResourceView *CreateShaderResourceView(IResource &resource, const TShaderResourceViewDesc *desc = NULL) = 0;
			virtual IUnorderedAccessView *CreateUnorderedResourceView(IResource &resource, const TUnorderedAccessView *desc = NULL) = 0;
		};

		struct TOutput
		{
			TOutput() {}
			TOutput(uint width, uint heigth): width(width), height(height) {}
			uint width, height;
		};

		struct TDispMode//: TOutput
		{
			//TDispMode() {}
			//TDispMode(const TOutput &output): TOutput(output) {}
			uint width, height;
			float refreshRate;
			std::string desc;
		};

		class IDisplayModes
		{
		public:
			virtual ~IDisplayModes() {}
			virtual uint Count() const = 0;
			virtual TDispMode operator [](uint idx) const = 0;
		};

		const IDisplayModes &GetDisplayModes();
		IDevice *CreateDevice(HWND hwnd, uint width, uint height, bool fullscreen = false, uint refreshRate = 0, bool multithreaded = true);

		class ICommandList
		{
		public:
		};
	}
}

#endif//__LOW_LEVEL_RENDERER_H__