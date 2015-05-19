/**
\author		Alexey Shaydurov aka ASH
\date		20.5.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "Common.h"

#ifndef NO_BUILTIN_RENDERER

//#define SAVE_ALL_STATES

#include "FixedFunctionPipelineDX9.h"

#include <d3d9.h>

namespace WRL = Microsoft::WRL;

class CCoreRendererDX9 final : public ICoreRenderer
{
	IEngineCore &_engineCore;

	WRL::ComPtr<IDirect3DDevice9> _device;

	TCrRndrInitResults _stInitResults;
	bool _16bitColor;

	D3DCOLOR _clearColor = 0;	// default OpenGL value
	float _lineWidth = 1;

	CFixedFunctionPipelineDX9 *_FFP;

	unsigned int _maxTexResolution[2], _maxAnisotropy, _maxTexUnits, _maxTexStages, _maxRTs, _maxVertexStreams, _maxClipPlanes, _maxVSFloatConsts;
	DWORD _textureAddressCaps;
	bool _NPOTTexSupport, _NSQTexSupport, _mipmapSupport, _anisoSupport;

	class CSurfacePool
	{
		struct TSurfaceDesc
		{
			UINT width, height;
			D3DFORMAT format;
			D3DMULTISAMPLE_TYPE MSAA;
		public:
			inline bool operator ==(const TSurfaceDesc &src) const;
		};
		struct THash
		{
			size_t operator ()(const TSurfaceDesc &src) const;
		};
		struct TSurface
		{
			WRL::ComPtr<IDirect3DSurface9> surface;
			uint_least32_t idleTime = 0;
		};
		typedef std::unordered_multimap<TSurfaceDesc, TSurface, THash> TSurfaces;
		TSurfaces _surfaces;
		HRESULT(__stdcall IDirect3DDevice9::*const _createSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle);
		const BOOL _boolParam;
		static const/*expr*/ size_t _maxPoolSize = 16;
		static const/*expr*/ uint_least32_t _maxIdle = 10;
	public:
		enum class E_TYPE
		{
			DEPTH,
			COLOR,
			COLOR_LOCKABLE,
		};
		explicit CSurfacePool(E_TYPE type);
	public:
		WRL::ComPtr<IDirect3DSurface9> GetSurface(IDirect3DDevice9 *device, const TSurfaces::key_type &desc);
		void Clean();
		void Clear() { _surfaces.clear(); }
	} _depthPool, _colorPool, _MSAAcolorPool;
	class COffscreenDepth
	{
		WRL::ComPtr<IDirect3DSurface9> _surface;
	public:
		WRL::ComPtr<IDirect3DSurface9> Get(IDirect3DDevice9 *device, UINT width, UINT height, D3DMULTISAMPLE_TYPE MSAA);
		void Clear() { _surface.Reset(); }
	} _offcreenDepth;
	static const/*expr*/ D3DFORMAT _offscreenDepthFormat = D3DFMT_D24S8;
	ICoreTexture *_curRenderTarget = nullptr;
	WRL::ComPtr<IDirect3DSurface9> _screenColorTarget, _screenDepthTarget;
	D3DVIEWPORT9 _screenViewport;
	uint_least8_t _selectedTexLayer = 0;

	struct TRTBindings
	{
		std::unique_ptr<WRL::ComPtr<IDirect3DSurface9> []> rendertargets;
		WRL::ComPtr<IDirect3DSurface9> deptStensil;
#if 1
		/*
		VS 2013 does not support default move ctor generation for such struct
		TODO: try to remove it future VS version
		*/
	public:
		TRTBindings() = default;
		TRTBindings(TRTBindings &&src) :
			rendertargets(std::move(src.rendertargets)),
			deptStensil(std::move(src.deptStensil))
		{}
#endif
	};
	std::stack<TRTBindings> _rtBindingsStack;

	/*
		VS 2013 does not support constexpr => have to define array content in .cpp => have to expicit specify array size here (it used later in TStates struct)
		TODO: move array content definition from .cpp to .h when consexpr support appears in order to get rid of explicit array size specification
	*/
	static const/*expr*/ D3DRENDERSTATETYPE _rederStateTypes[103];
	static const/*expr*/ D3DSAMPLERSTATETYPE _samplerStateTypes[13];
	static const/*expr*/ D3DTEXTURESTAGESTATETYPE _stageStateTypes[18];

	struct TStates
	{
		std::array<DWORD, std::extent<decltype(_rederStateTypes)>::value> renderStates;
		struct TTextureStates
		{
			std::array<DWORD, std::extent<decltype(_samplerStateTypes)>::value> samplerStates;
			std::array<DWORD, std::extent<decltype(_stageStateTypes)>::value> stageStates;
			WRL::ComPtr<IDirect3DBaseTexture9> texture;
		};
		std::unique_ptr<TTextureStates []> textureStates;
		D3DVIEWPORT9 viewport;
		RECT scissorRect;
#ifdef SAVE_ALL_STATES
		std::unique_ptr<float [][4]> clipPlanes;
#endif
		WRL::ComPtr<IDirect3DIndexBuffer9> IB;
		struct TVertexStream
		{
			WRL::ComPtr<IDirect3DVertexBuffer9> VB;
			UINT offset;
			UINT stride;
			UINT freq;
		};
		std::unique_ptr<TVertexStream []> vertexStreams;
		WRL::ComPtr<IDirect3DVertexDeclaration9> VBDecl;
#ifdef SAVE_ALL_STATES
		DWORD FVF;
		FLOAT NPatchMode;
		WRL::ComPtr<IDirect3DVertexShader9> VS;
		WRL::ComPtr<IDirect3DPixelShader9> PS;
		std::unique_ptr<float [][4]> VSFloatConsts;
		float PSFloatConsts[224][4];
		int VSIntConsts[16][4], PSIntConsts[16][4];
		BOOL VSBoolConsts[16][4], PSBoolConsts[16][4];
#endif
		D3DMATERIAL9 material;
		D3DCOLOR clearColor;
		float lineWidth;
		uint_least8_t selectedTexLayer;
#if 1
		/*
			VS 2013 does not support default move ctor generation for such struct
			TODO: try to remove it future VS version
		*/
	public:
		TStates() = default;
		TStates(TStates &&src) :
			renderStates(std::move(src.renderStates)),
			textureStates(std::move(src.textureStates)),
			viewport(std::move(src.viewport)),
			scissorRect(std::move(src.scissorRect)),
#ifdef SAVE_ALL_STATES
			clipPlanes(std::move(src.clipPlanes)),
#endif
			IB(std::move(src.IB)),
			vertexStreams(std::move(src.vertexStreams)),
			VBDecl(std::move(src.VBDecl)),
#ifdef SAVE_ALL_STATES
			FVF(std::move(src.FVF)),
			NPatchMode(std::move(src.NPatchMode)),
			VS(std::move(src.VS)),
			PS(std::move(src.PS)),
			VSFloatConsts(std::move(src.VSFloatConsts)),
#endif
			material(std::move(src.material)),
			clearColor(std::move(src.clearColor)),
			lineWidth(std::move(src.lineWidth)),
			selectedTexLayer(std::move(src.selectedTexLayer))
		{
#ifdef SAVE_ALL_STATES
			memcpy(PSFloatConsts, src.PSFloatConsts, sizeof PSFloatConsts);
			memcpy(VSIntConsts, src.VSIntConsts, sizeof VSIntConsts);
			memcpy(PSIntConsts, src.PSIntConsts, sizeof PSIntConsts);
			memcpy(VSBoolConsts, src.VSBoolConsts, sizeof VSBoolConsts);
			memcpy(PSBoolConsts, src.PSBoolConsts, sizeof PSBoolConsts);
#endif
		}
#endif
	};
	std::stack<TStates> _stateStack;

private:

	D3DPRESENT_PARAMETERS _GetPresentParams(TEngineWindow &wnd) const;

	void _PushStates();
	void _PopStates();

	inline D3DTRANSFORMSTATETYPE _MatrixType_DGLE_2_D3D(E_MATRIX_TYPE dgleType) const;

	void _FlipRectY(uint &y, uint height) const;

	template<unsigned idx = 0>
	inline void _BindVB(const TDrawDataDesc &drawDesc, const WRL::ComPtr<IDirect3DVertexBuffer9> &VB, UINT stream = 0) const;

	static void DGLE_API EventsHandler(void *pParameter, IBaseEvent *pEvent);

public:

	CCoreRendererDX9(IEngineCore &engineCore);
	CCoreRendererDX9(CCoreRendererDX9 &) = delete;
	void operator =(CCoreRendererDX9 &) = delete;

	static inline uint GetVertexSize(const TDrawDataDesc &stDesc);

	inline bool GetNPOTTexSupport() const{ return _NPOTTexSupport; }
	inline bool GetNSQTexSupport() const { return _NSQTexSupport; }
	inline bool GetMipmapSupport() const { return _mipmapSupport; }
	inline auto GetMaxTextureWidth() const -> decltype(_maxTexResolution[0]) { return _maxTexResolution[0]; }
	inline auto GetMaxTextureHeight() const -> decltype(_maxTexResolution[1]) { return _maxTexResolution[1]; }

	DGLE_RESULT DGLE_API Prepare(TCrRndrInitResults &stResults) override;
	DGLE_RESULT DGLE_API Initialize(TCrRndrInitResults &stResults, TEngineWindow &stWin, E_ENGINE_INIT_FLAGS &eInitFlags) override;
	DGLE_RESULT DGLE_API Finalize() override;
	DGLE_RESULT DGLE_API AdjustMode(TEngineWindow &stNewWin) override;
	DGLE_RESULT DGLE_API MakeCurrent() override;
	DGLE_RESULT DGLE_API Present() override;

	DGLE_RESULT DGLE_API SetClearColor(const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetClearColor(TColor4 &stColor) override;
	DGLE_RESULT DGLE_API Clear(bool bColor, bool bDepth, bool bStencil) override;
	DGLE_RESULT DGLE_API SetViewport(uint x, uint y, uint width, uint height) override;
	DGLE_RESULT DGLE_API GetViewport(uint &x, uint &y, uint &width, uint &height) override;
	DGLE_RESULT DGLE_API SetScissorRectangle(uint x, uint y, uint width, uint height) override;
	DGLE_RESULT DGLE_API GetScissorRectangle(uint &x, uint &y, uint &width, uint &height) override;
	DGLE_RESULT DGLE_API SetLineWidth(float fWidth) override;
	DGLE_RESULT DGLE_API GetLineWidth(float &fWidth) override;
	DGLE_RESULT DGLE_API SetPointSize(float fSize) override;
	DGLE_RESULT DGLE_API GetPointSize(float &fSize) override;
	DGLE_RESULT DGLE_API ReadFrameBuffer(uint uiX, uint uiY, uint uiWidth, uint uiHeight, uint8 *pData, uint uiDataSize, E_TEXTURE_DATA_FORMAT eDataFormat) override;
	DGLE_RESULT DGLE_API SetRenderTarget(ICoreTexture *pTexture) override;
	DGLE_RESULT DGLE_API GetRenderTarget(ICoreTexture *&prTexture) override;
	DGLE_RESULT DGLE_API CreateTexture(ICoreTexture *&prTex, const uint8 *pData, uint uiWidth, uint uiHeight, bool bMipmapsPresented, E_CORE_RENDERER_DATA_ALIGNMENT eDataAlignment, E_TEXTURE_DATA_FORMAT eDataFormat, E_TEXTURE_LOAD_FLAGS eLoadFlags) override;
	DGLE_RESULT DGLE_API CreateGeometryBuffer(ICoreGeometryBuffer *&prBuffer, const TDrawDataDesc &stDrawDesc, uint uiVerticesCount, uint uiIndicesCount, E_CORE_RENDERER_DRAW_MODE eMode, E_CORE_RENDERER_BUFFER_TYPE eType) override;
	DGLE_RESULT DGLE_API ToggleStateFilter(bool bEnabled) override;
	DGLE_RESULT DGLE_API InvalidateStateFilter() override;
	DGLE_RESULT DGLE_API PushStates() override;
	DGLE_RESULT DGLE_API PopStates() override;
	DGLE_RESULT DGLE_API SetMatrix(const TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType) override;
	DGLE_RESULT DGLE_API GetMatrix(TMatrix4x4 &stMatrix, E_MATRIX_TYPE eMatType) override;
	DGLE_RESULT DGLE_API Draw(const TDrawDataDesc &stDrawDesc, E_CORE_RENDERER_DRAW_MODE eMode, uint uiCount) override;
	DGLE_RESULT DGLE_API DrawBuffer(ICoreGeometryBuffer *pBuffer) override;
	DGLE_RESULT DGLE_API SetColor(const TColor4 &stColor) override;
	DGLE_RESULT DGLE_API GetColor(TColor4 &stColor) override;
	DGLE_RESULT DGLE_API ToggleBlendState(bool bEnabled) override;
	DGLE_RESULT DGLE_API ToggleAlphaTestState(bool bEnabled) override;
	DGLE_RESULT DGLE_API SetBlendState(const TBlendStateDesc &stState) override;
	DGLE_RESULT DGLE_API GetBlendState(TBlendStateDesc &stState) override;
	DGLE_RESULT DGLE_API SetDepthStencilState(const TDepthStencilDesc &stState) override;
	DGLE_RESULT DGLE_API GetDepthStencilState(TDepthStencilDesc &stState) override;
	DGLE_RESULT DGLE_API SetRasterizerState(const TRasterizerStateDesc &stState) override;
	DGLE_RESULT DGLE_API GetRasterizerState(TRasterizerStateDesc &stState) override;
	DGLE_RESULT DGLE_API BindTexture(ICoreTexture *pTex, uint uiTextureLayer) override;
	DGLE_RESULT DGLE_API GetBindedTexture(ICoreTexture *&prTex, uint uiTextureLayer) override;
	DGLE_RESULT DGLE_API GetFixedFunctionPipelineAPI(IFixedFunctionPipeline *&prFFP) override;
	DGLE_RESULT DGLE_API GetDeviceMetric(E_CORE_RENDERER_METRIC_TYPE eMetric, int &iValue) override;
	DGLE_RESULT DGLE_API IsFeatureSupported(E_CORE_RENDERER_FEATURE_TYPE eFeature, bool &bIsSupported) override;
	DGLE_RESULT DGLE_API GetRendererType(E_CORE_RENDERER_TYPE &eType) override;

	DGLE_RESULT DGLE_API GetType(E_ENGINE_SUB_SYSTEM &eSubSystemType) override;

	IDGLE_BASE_IMPLEMENTATION(ICoreRenderer, INTERFACE_IMPL(IEngineSubSystem, INTERFACE_IMPL_END))
};

#endif