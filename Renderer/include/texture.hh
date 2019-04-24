#pragma once

#define NOMINMAX

#include <utility>
#include <vector>
#include <filesystem>
#include <wrl/client.h>
#include "../tracked resource.h"

struct ID3D12Resource;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	namespace TerrainMaterials
	{
		class Textured;
	}

	enum class TextureUsage
	{
		AlbedoMap,
	};

	namespace Impl
	{
		class Viewport;

		/*
			store textures in sys RAM for now which means accessing it over PCIe on discrete GPUs, thouugh it is optimal for iGPUs (no redundant copy needed)
			need to develop [batched] copy (sys RAM -> VRAM) mechanism on dedicated DMA cmd queue (enable it for discrete GPUs only)
		*/
		class Texture
		{
			WRL::ComPtr<ID3D12Resource> tex;	// in system RAM

		protected:
			// acquire GPU lifetime tracked resource
			operator TrackedResource<ID3D12Resource>() const;

		public:
			explicit Texture(const std::filesystem::path &fileName, TextureUsage usage);

			// define outside to break dependency on ComPtr`s implementation
		protected:
			Texture(const Texture &);
			Texture(Texture &&);
			Texture &operator =(const Texture &), &operator =(Texture &&);
			~Texture();

		private:
			static std::vector<std::pair<WRL::ComPtr<ID3D12Resource>, int/*dst state (src is COMMON)*/>> pendingBarriers;

		protected:
			//static class PendingBarriersHandle
			//{
			//	friend class Impl::Texture;

			//private:
			//	PendingBarriersHandle() = default;
			//	PendingBarriersHandle(PendingBarriersHandle &) = delete;
			//	void operator =(PendingBarriersHandle &) = delete;

			//public:
			//	~PendingBarriersHandle();

			//public:
			//	const decltype(pendingBarriers) *operator ->() const noexcept { return &pendingBarriers; }
			//} AcquirePendingBarriers();
			static decltype(pendingBarriers) AcquirePendingBarriers() noexcept;
		};
	}

	class Texture : public Impl::Texture
	{
		friend class TerrainMaterials::Textured;
		friend class Impl::Viewport;	// for pending barriers

	public:
		using Impl::Texture::Texture;

		// hide from protected
	private:
		using Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>;
		using Impl::Texture::AcquirePendingBarriers;
	};
}