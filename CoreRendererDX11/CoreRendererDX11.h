/**
\author		Alexey Shaydurov aka ASH
\date		20.7.2015 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "Common.h"
#include <d3d11_2.h>

namespace WRL = Microsoft::WRL;

class CCoreRendererDX11 final : public ICoreRenderer
{
	IEngineCore &_engineCore;

	WRL::ComPtr<ID3D11Device2> _device;
	WRL::ComPtr<IDXGISwapChain2> _swapChain;

	TCrRndrInitResults _stInitResults;
	bool _16bitColor;

	D3DCOLOR _clearColor = 0;	// default OpenGL value
	float _lineWidth = 1;

	TMatrix4x4 _projXform = MatrixIdentity();

	CBroadcast<> _frameEndBroadcast, _cleanBroadcast;

	class CDynamicBufferBase
	{
		static const/*expr*/ unsigned int _baseStartSize = 1024u * 1024u, _baseLimit = 32u * 1024u * 1024u;
	private:
		WRL::ComPtr<ID3D11Buffer> _buffer;
	private:
		const CBroadcast<>::CCallbackHandle _frameEndCallbackHandle{};
	private:
		const unsigned int _limit = _baseLimit;
		unsigned int _size, _offset = 0, _lastFrameSize = 0;
	protected:
		CDynamicBufferBase() = default;
		CDynamicBufferBase(CCoreRendererDX11 &parent, bool vertex/*index if false*/);
		CDynamicBufferBase(CDynamicBufferBase &) = delete;
		void operator =(CDynamicBufferBase &) = delete;
		~CDynamicBufferBase();
	public:
		unsigned int FillSegment(ID3D11DeviceContext2 *context, const void *data, unsigned int size);
	private:
		void _CreateBuffer();
	private:
		void _OnFrameEnd();
	};

	class CDynamicVB : public CDynamicBufferBase
	{
	public:
		CDynamicVB(CCoreRendererDX11 &parent) : CDynamicBufferBase(parent, true) {}
	} *_immediateVB = nullptr;

	class CDynamicIB : public CDynamicBufferBase
	{
	public:
		CDynamicIB() = default;
		CDynamicIB(CCoreRendererDX11 &parent) : CDynamicBufferBase(parent, false) {}
	} *_immediateIB = nullptr;

	class CGeometryProviderBase;
	class CGeometryProvider;
	class CCoreGeometryBufferBase;
	class CCoreGeometryBufferSoftware;
	class CCoreGeometryBufferDynamic;
	class CCoreGeometryBufferStatic;

	class CInputLayoutCache
	{
		/*
		currently there is small amount of unique input layouts
		therefore fixed-sized LUT would be propably faster
		*/
		union tag
		{
			unsigned char packed;
			struct
			{
				bool _2D : 1, normal : 1, uv : 1, color : 1;
			};
		public:
			inline tag(const TDrawDataDesc &desc);
			inline bool operator ==(const tag src) const { return packed == src.packed; }
		};
		struct THash
		{
			inline size_t operator ()(const tag src) const { return HashValue(src.packed); }
		};
		typedef std::unordered_map<tag, WRL::ComPtr<ID3D11InputLayout>, THash> TCache;
		TCache _cache;
	public:
		const TCache::mapped_type &GetLayout(ID3D11Device2 *device, const TDrawDataDesc &desc);
	} _VBLayoutCache;

	class CCoreTexture;

	class CStageTextureCache
	{
		typedef std::unordered_map<DXGI_FORMAT, WRL::ComPtr<ID3D11Texture2D>> TCache;
		TCache _cache;
	public:
		CStageTextureCache(CStageTextureCache &) = delete;
		void operator =(CStageTextureCache &) = delete;
	public:
		const TCache::mapped_type &GetTexture(ID3D11Device2 *device, unsigned int width, unsigned int height, TCache::key_type format);
	} _rendertargetCache;

	class CMSAARendertargetPool
	{
		struct TRenderTargetDesc
		{
			UINT width, height;
			DXGI_FORMAT format;
		public:
			inline bool operator ==(const TRenderTargetDesc &src) const;
		};
		struct THash
		{
			inline size_t operator ()(const TRenderTargetDesc &src) const;
		};
		struct TRenderTarget
		{
			WRL::ComPtr<ID3D11Texture2D> rt;
			uint_least32_t idleTime;
		public:
			TRenderTarget(WRL::ComPtr<ID3D11Texture2D> &&rt) : rt(std::move(rt)), idleTime() {}
		};
		typedef std::unordered_multimap<TRenderTargetDesc, TRenderTarget, THash> TPool;
		TPool _pool;
		const CBroadcast<>::CCallbackHandle _cleanCallbackHandle;
		static const/*expr*/ size_t _maxPoolSize = 16;
		static const/*expr*/ uint_least32_t _maxIdle = 10;
	public:
		explicit CMSAARendertargetPool(CCoreRendererDX11 &parent);
		CMSAARendertargetPool(CMSAARendertargetPool &) = delete;
		void operator =(CMSAARendertargetPool &) = delete;
	public:
		const WRL::ComPtr<ID3D11Texture2D> &GetRendertarget(CCoreRendererDX11 &parent, const TPool::key_type &desc);
	} _MSAARendertargetPool{ *this };

	class COffscreenDepth
	{
		WRL::ComPtr<ID3D11Texture2D> _depth;
	public:
		COffscreenDepth(COffscreenDepth &) = delete;
		void operator =(COffscreenDepth &) = delete;
	public:
		WRL::ComPtr<ID3D11Texture2D> Get(ID3D11Device2 *device, UINT width, UINT height, const DXGI_SAMPLE_DESC &MSAA);
	} _offscreenDepth;

	static const/*expr*/ DXGI_FORMAT _offscreenDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	CCoreTexture *_curRenderTarget = nullptr;
	WRL::ComPtr<IDirect3DSurface9> _screenColorTarget, _screenDepthTarget;
	D3DVIEWPORT9 _screenViewport;
	uint_least8_t _selectedTexLayer = 0;

	struct TBindings
	{
		std::unique_ptr<WRL::ComPtr<IDirect3DSurface9>[]> rendertargets;
		WRL::ComPtr<IDirect3DSurface9> deptStensil;
#ifdef SAVE_ALL_STATES
		WRL::ComPtr<IDirect3DIndexBuffer9> IB;
		struct TVertexStream
		{
			WRL::ComPtr<IDirect3DVertexBuffer9> VB;
			UINT offset;
			UINT stride;
			UINT freq;
		};
		std::unique_ptr<TVertexStream[]> vertexStreams;
		WRL::ComPtr<IDirect3DVertexDeclaration9> VBDecl;
#endif
#if 1
		/*
		VS 2013 does not support default move ctor generation for such struct
		TODO: try to remove it in future VS version
		*/
	public:
		TBindings() = default;
		TBindings(TBindings &&src) :
			rendertargets(std::move(src.rendertargets)),
			deptStensil(std::move(src.deptStensil))
#ifdef SAVE_ALL_STATES
			,
			IB(std::move(src.IB)),
			vertexStreams(std::move(src.vertexStreams)),
			VBDecl(std::move(src.VBDecl))
#endif
		{}
#endif
	};
	std::stack<TBindings> _bindingsStack;

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
		std::unique_ptr<TTextureStates[]> textureStates;
		D3DVIEWPORT9 viewport;
		RECT scissorRect;
#ifdef SAVE_ALL_STATES
		std::unique_ptr<float[][4]> clipPlanes;
		DWORD FVF;
		FLOAT NPatchMode;
		WRL::ComPtr<IDirect3DVertexShader9> VS;
		WRL::ComPtr<IDirect3DPixelShader9> PS;
		std::unique_ptr<float[][4]> VSFloatConsts;
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
		TODO: try to remove it in future VS version
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
	void _ConfigureWindow(const TEngineWindow &wnd, DGLE_RESULT &res);

	void _SetProjXform();

	void _PushStates(), _PopStates(), _DiscardStates();

	inline D3DTRANSFORMSTATETYPE _MatrixType_DGLE_2_D3D(E_MATRIX_TYPE dgleType) const;

	void _FlipRectY(uint &y, uint height) const;

	template<unsigned idx = 0>
	inline void _BindVB(const TDrawDataDesc &drawDesc, const WRL::ComPtr<IDirect3DVertexBuffer9> &VB, unsigned int baseOffset, UINT stream = 0) const;

	void _Draw(class CGeometryProviderBase &geom);

	static void DGLE_API EventsHandler(void *pParameter, IBaseEvent *pEvent);

public:

	CCoreRendererDX11(IEngineCore &engineCore);
	CCoreRendererDX11(CCoreRendererDX11 &) = delete;
	void operator =(CCoreRendererDX11 &) = delete;

	inline bool DeviceLost() const noexcept { return _deviceLost; }

	inline bool GetNPOTTexSupport() const { return _NPOTTexSupport; }
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