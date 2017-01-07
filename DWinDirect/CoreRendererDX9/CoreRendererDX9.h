/**
\author		Alexey Shaydurov aka ASH
\date		07.01.2017 (c)Andrey Korotkov

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "Common.h"
#ifdef MSVC_LIMITATIONS
#include <boost/version.hpp>
#include <boost/optional.hpp>
#if BOOST_VERSION < 106300
#error Old boost version. 1.63.0 or later required.
#endif
namespace std_boost = boost;
#else
#include <optional>
namespace std_boost = std;
#endif
#include <vector>
#include <deque>
#include <tuple>

#define SAVE_ALL_STATES 0
#define ENABLE_DOWNCAST_TO_WRAPPER 0

#include "FixedFunctionPipelineDX9.h"

#include <comdef.h>
#include <d3d9.h>

#if 0
#	define LOST_DEVICE_RETURN_CODE E_FAIL
#else
#	define LOST_DEVICE_RETURN_CODE S_FALSE
#endif

#define CHECK_DEVICE(coreRenderer)		\
	if ((coreRenderer).DeviceLost())	\
		return LOST_DEVICE_RETURN_CODE

class CCoreRendererDX9 final : public ICoreRenderer
{
	IEngineCore &_engineCore;

	IDirect3DDevice9Ptr _device;

	TCrRndrInitResults _stInitResults;
	bool _16bitColor;
	bool _deviceLost = false;

	D3DCOLOR _clearColor = 0;	// default OpenGL value
	float _lineWidth = 1;

	CFixedFunctionPipelineDX9 *_FFP = nullptr;

	unsigned int _maxTexResolution[2], _maxAnisotropy, _maxTexUnits, _maxTexStages, _maxRTs, _maxVertexStreams, _maxClipPlanes, _maxVSFloatConsts;
	DWORD _textureAddressCaps;
	bool _NPOTTexSupport, _NSQTexSupport, _mipmapSupport, _anisoSupport, _GPUTimeQuerySupport;

	TMatrix4x4 _projXform = MatrixIdentity();

	int _profilerState = 0;

	CBroadcast<> _frameEndBroadcast, _clearBroadcast, _cleanBroadcast;
	CBroadcast<const IDirect3DDevice9Ptr &> _restoreBroadcast;

	class CDynamicBufferBase
	{
		static constexpr unsigned int _baseStartSize = 1024u * 1024u, _baseLimit = 32u * 1024u * 1024u;
	private:
		const CBroadcast<>::CCallbackHandle _frameEndCallbackHandle{};
	protected:
		const CBroadcast<>::CCallbackHandle _clearCallbackHandle{};
		CBroadcast<const IDirect3DDevice9Ptr &>::CCallbackHandle _restoreCallbackHandle;
	private:
		const unsigned int _limit = _baseLimit;
		unsigned int _lastFrameSize = 0;
	protected:
		unsigned int _size, _offset = 0;
	protected:
		CDynamicBufferBase() = default;
		CDynamicBufferBase(CCoreRendererDX9 &parent, unsigned int sizeMultiplier,
			CBroadcast<>::CCallbackHandle &&clearCallbackHandle, CBroadcast<const IDirect3DDevice9Ptr &>::CCallbackHandle &&restoreCallbackHandle);
		CDynamicBufferBase(CDynamicBufferBase &) = delete;
		void operator =(CDynamicBufferBase &) = delete;
		~CDynamicBufferBase();
	public:
		unsigned int FillSegment(const void *data, unsigned int size);
	protected:
		static inline DWORD _Usage(bool points);
	private:
		inline void _CreateBuffer();
	private:
		void _OnFrameEnd();
	private:
		virtual void _CreateBufferImpl() = 0;
		virtual void _FillSegmentImpl(const void *data, unsigned int size, DWORD lockFlags) = 0;
		virtual const IDirect3DResource9Ptr _GetBuffer() const = 0;
		virtual void _OnGrow(const IDirect3DResource9Ptr &oldBuffer, unsigned int oldOffset) {}
	};

	class CDynamicVB : public CDynamicBufferBase
	{
#if 1
		using CDynamicBufferBase::_size;
		using CDynamicBufferBase::_offset;
#endif
		IDirect3DVertexBuffer9Ptr _VB;
	protected:
		typedef decltype(_VB) InterfacePtr;
	private:
		inline void _CreateBuffer(const IDirect3DDevice9Ptr &device, DWORD usage);
		inline void _CreateBuffer(DWORD usage);
	public:
		CDynamicVB(CCoreRendererDX9 &parent, bool points);
	public:
		void Reset(bool points);
		const IDirect3DVertexBuffer9Ptr &GetVB() const { return _VB; }
	private:
		void _Restore(const IDirect3DDevice9Ptr &device, bool points);
	private:
		void _CreateBufferImpl() override final;
		void _FillSegmentImpl(const void *data, unsigned int size, DWORD lockFlags) override final;
		const IDirect3DResource9Ptr _GetBuffer() const override final { return _VB; }
	} *_immediateVB = nullptr, *_immediatePointsVB = nullptr;
	CDynamicVB *_GetImmediateVB(bool points) const;

	class CDynamicIB : public CDynamicBufferBase
	{
#if 1
		using CDynamicBufferBase::_size;
		using CDynamicBufferBase::_offset;
#endif
		IDirect3DIndexBuffer9Ptr _IB;
	protected:
		typedef decltype(_IB) InterfacePtr;
	private:
		inline void _CreateBuffer(const IDirect3DDevice9Ptr &device, DWORD usage, D3DFORMAT format);
		inline void _CreateBuffer(DWORD usage, D3DFORMAT format);
	public:
		CDynamicIB() = default;
		CDynamicIB(CCoreRendererDX9 &parent, bool points, bool _32);
	public:
		void Reset(bool points, bool _32);
		const IDirect3DIndexBuffer9Ptr &GetIB() const { return _IB; }
	private:
		void _Restore(const IDirect3DDevice9Ptr &device, bool points, bool _32);
	private:
		void _CreateBufferImpl() override final;
		void _FillSegmentImpl(const void *data, unsigned int size, DWORD lockFlags) override final;
		const IDirect3DResource9Ptr _GetBuffer() const override final { return _IB; }
	} *_immediateIB16 = nullptr, *_immediateIB32 = nullptr, *_immediatePointsIB16 = nullptr, *_immediatePointsIB32 = nullptr;
	CDynamicIB *_GetImmediateIB(bool points, bool _32) const;

	class CGeometryProviderBase;
	class CGeometryProvider;
	class CCoreGeometryBufferBase;
	class CCoreGeometryBufferSoftware;
	class CCoreGeometryBufferDynamic;
	class CCoreGeometryBufferStatic;

	class CVertexDeclarationCache
	{
		/*
			currently there is small amount of unique vertex declarations
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
		typedef std::unordered_map<tag, IDirect3DVertexDeclaration9Ptr, THash> TCache;
		TCache _cache;
	public:
		const TCache::mapped_type &GetDecl(IDirect3DDevice9 *device, const TDrawDataDesc &desc);
	} _VBDeclCache;

	class CCoreTexture;

	class CRendertargetCache
	{
		typedef std::unordered_map<D3DFORMAT, IDirect3DSurface9Ptr> TCache;
		TCache _cache;
		const CBroadcast<>::CCallbackHandle _clearCallbackHandle;
	public:
		CRendertargetCache(CCoreRendererDX9 &parent);
		CRendertargetCache(CRendertargetCache &) = delete;
		void operator =(CRendertargetCache &) = delete;
	public:
		const TCache::mapped_type &GetRendertarget(IDirect3DDevice9 *device, unsigned int width, unsigned int height, TCache::key_type format);
	} _rendertargetCache{ *this };

	class CImagePool
	{
		struct TImageDesc
		{
			UINT width, height;
			D3DFORMAT format;
		public:
			inline bool operator ==(const TImageDesc &src) const;
		};
		struct THash
		{
			inline size_t operator ()(const TImageDesc &src) const;
		};
		struct TImage
		{
			IDirect3DResource9Ptr image;
			uint_least32_t idleTime;
		public:
			TImage(IDirect3DResource9Ptr &&image) : image(std::move(image)), idleTime() {}
		};
	protected:
		typedef std::unordered_multimap<TImageDesc, TImage, THash> TPool;
	private:
		TPool _pool;
		const CBroadcast<>::CCallbackHandle _clearCallbackHandle, _cleanCallbackHandle;
		static constexpr size_t _maxPoolSize = 16;
		static constexpr uint_least32_t _maxIdle = 10;
	protected:
		explicit CImagePool(CCoreRendererDX9 &parent, bool managed = false);
		CImagePool(CImagePool &) = delete;
		void operator =(CImagePool &) = delete;
	protected:
		const IDirect3DResource9Ptr &_GetImage(IDirect3DDevice9 *device, const TPool::key_type &desc);
	private:
		virtual IDirect3DResource9Ptr _CreateImage(IDirect3DDevice9 *device, const TPool::key_type &desc) const = 0;
	};

	class CMSAARendertargetPool : CImagePool
	{
	public:
		explicit CMSAARendertargetPool(CCoreRendererDX9 &parent);
		inline IDirect3DSurface9Ptr GetRendertarget(IDirect3DDevice9 *device, const TPool::key_type &desc);
	private:
		IDirect3DResource9Ptr _CreateImage(IDirect3DDevice9 *device, const TPool::key_type &desc) const override;
	} _MSAARendertargetPool{ *this };

	class CTexturePool : CImagePool
	{
		const bool _managed, _mipmaps;
	public:
		CTexturePool(CCoreRendererDX9 &parent, bool managed, bool mipmaps);
		inline IDirect3DTexture9Ptr GetTexture(IDirect3DDevice9 *device, const TPool::key_type &desc);
	private:
		IDirect3DResource9Ptr _CreateImage(IDirect3DDevice9 *device, const TPool::key_type &desc) const override;
	}
	_texturePools[2][2] =
	{
		{ { *this, false, false }, { *this, false, true } },
		{ { *this, true, false }, { *this, true, true } }
	};

	class COffscreenDepth
	{
		IDirect3DSurface9Ptr _surface;
		const CBroadcast<>::CCallbackHandle _clearCallbackHandle;
	public:
		COffscreenDepth(CCoreRendererDX9 &parent);
		COffscreenDepth(COffscreenDepth &) = delete;
		void operator =(COffscreenDepth &) = delete;
	public:
		IDirect3DSurface9Ptr Get(IDirect3DDevice9 *device, UINT width, UINT height, D3DMULTISAMPLE_TYPE MSAA);
	} _offscreenDepth{ *this };

	template<D3DQUERYTYPE type>
	struct QueryTypeTraits;

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_TIMESTAMP>
	{
		typedef UINT64 TResult;
		static constexpr bool isRanged = false;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_TIMESTAMPDISJOINT>
	{
		typedef BOOL TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_TIMESTAMPFREQ>
	{
		typedef UINT64 TResult;
		static constexpr bool isRanged = false;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_PIPELINETIMINGS>
	{
		typedef D3DDEVINFO_D3D9PIPELINETIMINGS TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_INTERFACETIMINGS>
	{
		typedef D3DDEVINFO_D3D9INTERFACETIMINGS TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_VERTEXTIMINGS>
	{
		typedef D3DDEVINFO_D3D9STAGETIMINGS TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_PIXELTIMINGS>
	{
		typedef D3DDEVINFO_D3D9STAGETIMINGS TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_BANDWIDTHTIMINGS>
	{
		typedef D3DDEVINFO_D3D9BANDWIDTHTIMINGS TResult;
		static constexpr bool isRanged = true;
	};

	template<>
	struct QueryTypeTraits<D3DQUERYTYPE_CACHEUTILIZATION>
	{
		typedef D3DDEVINFO_D3D9CACHEUTILIZATION TResult;
		static constexpr bool isRanged = true;
	};

	template<D3DQUERYTYPE type>
	class CQuery;

	class CQueryPool
	{
		std::vector<IDirect3DQuery9Ptr> _pool;
	public:
		CQueryPool() = default;
		CQueryPool(CQueryPool &) = delete;
		void operator =(CQueryPool &) = delete;
	private:
		IDirect3DQuery9Ptr _GetQuery(IDirect3DDevice9 *device, D3DQUERYTYPE type);
	public:
		template<D3DQUERYTYPE type>
		CQuery<type> GetQuery(IDirect3DDevice9 *device) { return _GetQuery(device, type); }
		void Clear() noexcept { _pool.clear(); }
	} _GPUTimeQueryPool, _AdvancedProfilerQueryPool;

	class CQueryBase
	{
		IDirect3DQuery9Ptr _query;
	protected:
		CQueryBase() = default;
		CQueryBase(IDirect3DQuery9Ptr &&ptr) : _query(std::move(ptr)) {}
		CQueryBase(CQueryBase &&) = default;
		CQueryBase &operator =(CQueryBase &&) = default;
		~CQueryBase() = default;
	public:
		operator bool() const noexcept { return _query; }
		void Start(), Stop();
		bool Ready() noexcept;
	protected:
		bool _GetData(void *dst, DWORD size) noexcept;
	};

	template<typename TResult>
	class CQueryAccess : public CQueryBase
	{
		using CQueryBase::CQueryBase;
	public:
		std_boost::optional<TResult> GetData() noexcept;
	};

	template<typename TResult>
	class CPointQuery : public CQueryAccess<TResult>
	{
		using CQueryAccess<TResult>::CQueryAccess;
		using CQueryBase::Start;
		using CQueryBase::Stop;
	public:
		void Issue() { Stop(); }
	};

	template<D3DQUERYTYPE type>
	using SelectQuery = std::conditional_t<QueryTypeTraits<type>::isRanged,
		CQueryAccess<typename QueryTypeTraits<type>::TResult>,
		CPointQuery<typename QueryTypeTraits<type>::TResult>>;

	template<D3DQUERYTYPE type>
	class CQuery final : public SelectQuery<type>
	{
		friend class CQueryPool;
#ifdef MSVC_LIMITATIONS
		static constexpr D3DQUERYTYPE _type = type;
		using SelectQuery<_type>::SelectQuery;
#else
		using SelectQuery<type>::SelectQuery;
#endif
	public:
		CQuery() = default;
	};

	template<D3DQUERYTYPE ...types>
	using QueryPack = std::tuple<CQuery<types>...>;

	template<D3DQUERYTYPE type, class Pack>
	static inline decltype(auto) _GetQuery(Pack &&pack) noexcept
	{
		return std::get<CQuery<type>>(std::forward<Pack>(pack));
	}

	template<class Item>
	class CQueryQueueBase
	{
	protected:
		std::deque<Item> _queue;
	protected:
		CQueryQueueBase() = default;
		CQueryQueueBase(CQueryQueueBase &) = delete;
		void operator =(CQueryQueueBase &) = delete;
		~CQueryQueueBase() = default;
	public:
		void Insert(Item &&item);
		void Clear() noexcept { _queue.clear(); }
	};

	// for query pack
	template<D3DQUERYTYPE ...types>
	class CQueryQueue final : public CQueryQueueBase<QueryPack<types...>>
	{
#ifndef MSVC_LIMITATIONS
		template<size_t ...idx>
		bool _Ready(std::index_sequence<idx...>) noexcept;
#endif
		template<size_t ...idx>
		auto _GetData(std::index_sequence<idx...>);
	public:
		std_boost::optional<std::tuple<typename QueryTypeTraits<types>::TResult...>> Extract();
	};

	// for single query
	template<D3DQUERYTYPE type>
	class CQueryQueue<type> final :
#if ENABLE_DOWNCAST_TO_WRAPPER
		public CQueryQueueBase<CQueryBase>	// need to clarify is it legal
#else
		public CQueryQueueBase<CQuery<type>>
#endif
	{
	public:
		std_boost::optional<typename QueryTypeTraits<type>::TResult> Extract();
	};
	
	// GPU time
	CQueryQueue<D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMPFREQ, D3DQUERYTYPE_TIMESTAMPDISJOINT> _GPUTimeQueryQueue;
	CQuery<D3DQUERYTYPE_TIMESTAMP> _startGPUTimeQuery;
	QueryPack<D3DQUERYTYPE_TIMESTAMPFREQ, D3DQUERYTYPE_TIMESTAMPDISJOINT> _GPUTimeFreqQuery;
	std_boost::optional<float> _lastGPUTime;

	// advanced profiler queries

	template<D3DQUERYTYPE type>
	struct TProfilerTask
	{
		CQuery<type> query;
		CQueryQueue<type> queryQueue;
		std_boost::optional<typename QueryTypeTraits<type>::TResult> result;
		bool supported;
	};

	template<D3DQUERYTYPE ...types>
	using ProfilerTasks = std::tuple<TProfilerTask<types>...>;

	template<D3DQUERYTYPE type, class Tasks>
	static inline decltype(auto) _GetProfilerTask(Tasks &&tasks) noexcept
	{
		return std::get<TProfilerTask<type>>(std::forward<Tasks>(tasks));
	}

#ifdef MSVC_LIMITATIONS
#ifdef __cpp_lib_apply
	template<typename F, D3DQUERYTYPE firstType, D3DQUERYTYPE ...restTypes>
	static inline void _ProfilerTasksApplyImpl(F f, TProfilerTask<firstType> &head, TProfilerTask<restTypes> &...tail);

	// terminator
	static inline void _DestroyQueriesImpl() {}
#else
	template<D3DQUERYTYPE firstType, D3DQUERYTYPE ...restTypes, typename F, class Tasks>
	static inline void _ProfilerTasksApplyImpl(F f, Tasks &tasks);

	// terminator
	template<typename F, class Tasks>
	static inline void _ProfilerTasksApplyImpl(F, Tasks &) {}
#endif
#endif

	// 1 call site per instantiation
	template<typename F, D3DQUERYTYPE ...types>
	static inline void _ProfilerTasksApply(F f, ProfilerTasks<types...> &tasks);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskCheckSupport(TProfilerTask<type> &task);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskStart(TProfilerTask<type> &task);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskStop(TProfilerTask<type> &task);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskCollectResults(TProfilerTask<type> &task);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskDestroy(TProfilerTask<type> &task);

	template<D3DQUERYTYPE type>
	inline void _ProfilerTaskAbort(TProfilerTask<type> &task);

	ProfilerTasks<D3DQUERYTYPE_INTERFACETIMINGS, D3DQUERYTYPE_PIPELINETIMINGS, D3DQUERYTYPE_BANDWIDTHTIMINGS, D3DQUERYTYPE_VERTEXTIMINGS, D3DQUERYTYPE_PIXELTIMINGS, D3DQUERYTYPE_CACHEUTILIZATION> _advancedProfilerTasks;

	static constexpr D3DFORMAT _offscreenDepthFormat = D3DFMT_D24S8;
	CCoreTexture *_curRenderTarget = nullptr;
	IDirect3DSurface9Ptr _screenColorTarget, _screenDepthTarget;
	D3DVIEWPORT9 _screenViewport;
	uint_least8_t _selectedTexLayer = 0;

	struct TBindings
	{
		std::unique_ptr<IDirect3DSurface9Ptr []> rendertargets;
		IDirect3DSurface9Ptr depthStencil;
#if SAVE_ALL_STATES
		IDirect3DIndexBuffer9Ptr IB;
		struct TVertexStream
		{
			IDirect3DVertexBuffer9Ptr VB;
			UINT offset;
			UINT stride;
			UINT freq;
		};
		std::unique_ptr<TVertexStream []> vertexStreams;
		IDirect3DVertexDeclaration9Ptr VBDecl;
#endif
	};
	std::stack<TBindings> _bindingsStack;

	static constexpr D3DRENDERSTATETYPE _rederStateTypes[] =
	{
		D3DRS_ZENABLE,
		D3DRS_FILLMODE,
		D3DRS_SHADEMODE,
		D3DRS_ZWRITEENABLE,
		D3DRS_ALPHATESTENABLE,
		D3DRS_LASTPIXEL,
		D3DRS_SRCBLEND,
		D3DRS_DESTBLEND,
		D3DRS_CULLMODE,
		D3DRS_ZFUNC,
		D3DRS_ALPHAREF,
		D3DRS_ALPHAFUNC,
		D3DRS_DITHERENABLE,
		D3DRS_ALPHABLENDENABLE,
		D3DRS_FOGENABLE,
		D3DRS_SPECULARENABLE,
		D3DRS_FOGCOLOR,
		D3DRS_FOGTABLEMODE,
		D3DRS_FOGSTART,
		D3DRS_FOGEND,
		D3DRS_FOGDENSITY,
		D3DRS_RANGEFOGENABLE,
		D3DRS_STENCILENABLE,
		D3DRS_STENCILFAIL,
		D3DRS_STENCILZFAIL,
		D3DRS_STENCILPASS,
		D3DRS_STENCILFUNC,
		D3DRS_STENCILREF,
		D3DRS_STENCILMASK,
		D3DRS_STENCILWRITEMASK,
		D3DRS_TEXTUREFACTOR,
		D3DRS_WRAP0,
		D3DRS_WRAP1,
		D3DRS_WRAP2,
		D3DRS_WRAP3,
		D3DRS_WRAP4,
		D3DRS_WRAP5,
		D3DRS_WRAP6,
		D3DRS_WRAP7,
		D3DRS_CLIPPING,
		D3DRS_LIGHTING,
		D3DRS_AMBIENT,
		D3DRS_FOGVERTEXMODE,
		D3DRS_COLORVERTEX,
		D3DRS_LOCALVIEWER,
		D3DRS_NORMALIZENORMALS,
		D3DRS_DIFFUSEMATERIALSOURCE,
		D3DRS_SPECULARMATERIALSOURCE,
		D3DRS_AMBIENTMATERIALSOURCE,
		D3DRS_EMISSIVEMATERIALSOURCE,
		D3DRS_VERTEXBLEND,
		D3DRS_CLIPPLANEENABLE,
		D3DRS_POINTSIZE,
		D3DRS_POINTSIZE_MIN,
		D3DRS_POINTSPRITEENABLE,
		D3DRS_POINTSCALEENABLE,
		D3DRS_POINTSCALE_A,
		D3DRS_POINTSCALE_B,
		D3DRS_POINTSCALE_C,
		D3DRS_MULTISAMPLEANTIALIAS,
		D3DRS_MULTISAMPLEMASK,
		D3DRS_PATCHEDGESTYLE,
		D3DRS_DEBUGMONITORTOKEN,
		D3DRS_POINTSIZE_MAX,
		D3DRS_INDEXEDVERTEXBLENDENABLE,
		D3DRS_COLORWRITEENABLE,
		D3DRS_TWEENFACTOR,
		D3DRS_BLENDOP,
		D3DRS_POSITIONDEGREE,
		D3DRS_NORMALDEGREE,
		D3DRS_SCISSORTESTENABLE,
		D3DRS_SLOPESCALEDEPTHBIAS,
		D3DRS_ANTIALIASEDLINEENABLE,
		D3DRS_MINTESSELLATIONLEVEL,
		D3DRS_MAXTESSELLATIONLEVEL,
		D3DRS_ADAPTIVETESS_X,
		D3DRS_ADAPTIVETESS_Y,
		D3DRS_ADAPTIVETESS_Z,
		D3DRS_ADAPTIVETESS_W,
		D3DRS_ENABLEADAPTIVETESSELLATION,
		D3DRS_TWOSIDEDSTENCILMODE,
		D3DRS_CCW_STENCILFAIL,
		D3DRS_CCW_STENCILZFAIL,
		D3DRS_CCW_STENCILPASS,
		D3DRS_CCW_STENCILFUNC,
		D3DRS_COLORWRITEENABLE1,
		D3DRS_COLORWRITEENABLE2,
		D3DRS_COLORWRITEENABLE3,
		D3DRS_BLENDFACTOR,
		D3DRS_SRGBWRITEENABLE,
		D3DRS_DEPTHBIAS,
		D3DRS_WRAP8,
		D3DRS_WRAP9,
		D3DRS_WRAP10,
		D3DRS_WRAP11,
		D3DRS_WRAP12,
		D3DRS_WRAP13,
		D3DRS_WRAP14,
		D3DRS_WRAP15,
		D3DRS_SEPARATEALPHABLENDENABLE,
		D3DRS_SRCBLENDALPHA,
		D3DRS_DESTBLENDALPHA,
		D3DRS_BLENDOPALPHA,
	};

	static constexpr D3DSAMPLERSTATETYPE _samplerStateTypes[] =
	{
		D3DSAMP_ADDRESSU,
		D3DSAMP_ADDRESSV,
		D3DSAMP_ADDRESSW,
		D3DSAMP_BORDERCOLOR,
		D3DSAMP_MAGFILTER,
		D3DSAMP_MINFILTER,
		D3DSAMP_MIPFILTER,
		D3DSAMP_MIPMAPLODBIAS,
		D3DSAMP_MAXMIPLEVEL,
		D3DSAMP_MAXANISOTROPY,
		D3DSAMP_SRGBTEXTURE,
		D3DSAMP_ELEMENTINDEX,
		D3DSAMP_DMAPOFFSET,
	};

	static constexpr D3DTEXTURESTAGESTATETYPE _stageStateTypes[] =
	{
		D3DTSS_COLOROP,
		D3DTSS_COLORARG1,
		D3DTSS_COLORARG2,
		D3DTSS_ALPHAOP,
		D3DTSS_ALPHAARG1,
		D3DTSS_ALPHAARG2,
		D3DTSS_BUMPENVMAT00,
		D3DTSS_BUMPENVMAT01,
		D3DTSS_BUMPENVMAT10,
		D3DTSS_BUMPENVMAT11,
		D3DTSS_TEXCOORDINDEX,
		D3DTSS_BUMPENVLSCALE,
		D3DTSS_BUMPENVLOFFSET,
		D3DTSS_TEXTURETRANSFORMFLAGS,
		D3DTSS_COLORARG0,
		D3DTSS_ALPHAARG0,
		D3DTSS_RESULTARG,
		D3DTSS_CONSTANT,
	};

	struct TStates
	{
		std::array<DWORD, std::size(_rederStateTypes)> renderStates;
		struct TTextureStates
		{
			std::array<DWORD, std::size(_samplerStateTypes)> samplerStates;
			std::array<DWORD, std::size(_stageStateTypes)> stageStates;
			IDirect3DBaseTexture9Ptr texture;
		};
		std::unique_ptr<TTextureStates []> textureStates;
		D3DVIEWPORT9 viewport;
		RECT scissorRect;
#if SAVE_ALL_STATES
		std::unique_ptr<float [][4]> clipPlanes;
		DWORD FVF;
		FLOAT NPatchMode;
		IDirect3DVertexShader9Ptr VS;
		IDirect3DPixelShader9Ptr PS;
		std::unique_ptr<float [][4]> VSFloatConsts;
		float PSFloatConsts[224][4];
		int VSIntConsts[16][4], PSIntConsts[16][4];
		BOOL VSBoolConsts[16][4], PSBoolConsts[16][4];
#endif
		D3DMATERIAL9 material;
		D3DCOLOR clearColor;
		float lineWidth;
		uint_least8_t selectedTexLayer;
	};
	std::stack<TStates> _stateStack;

private:

	D3DPRESENT_PARAMETERS _GetPresentParams(TEngineWindow &wnd) const;
	void _ConfigureWindow(const TEngineWindow &wnd, DGLE_RESULT &res);
	HRESULT _BeginScene();
	void _AbortProfiling();
	void _ProfilerStartFrame(HRESULT &hr), _ProfilerStopFrame(HRESULT &hr);

	void _SetProjXform();

	void _PushStates(), _PopStates(), _DiscardStates();

	inline D3DTRANSFORMSTATETYPE _MatrixType_DGLE_2_D3D(E_MATRIX_TYPE dgleType) const;

	void _FlipRectY(uint &y, uint height) const;

	template<unsigned idx = 0>
	inline void _BindVB(const TDrawDataDesc &drawDesc, const IDirect3DVertexBuffer9Ptr &VB, unsigned int baseOffset, UINT stream = 0) const;

	void _Draw(class CGeometryProviderBase &geom);

	// 1 call site
	template<E_EVENT_TYPE>
	inline void _HandleEvent(IBaseEvent *pEvent);

	static void DGLE_API _EventsHandler(void *pParameter, IBaseEvent *pEvent);

public:

	CCoreRendererDX9(IEngineCore &engineCore);
	CCoreRendererDX9(CCoreRendererDX9 &) = delete;
	void operator =(CCoreRendererDX9 &) = delete;

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