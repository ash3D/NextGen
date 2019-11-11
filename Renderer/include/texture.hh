#pragma once

#define NOMINMAX

#include <utility>
#include <forward_list>
#include <filesystem>
#include <future>
#include <wrl/client.h>
#include "../tracked resource.h"

struct ID3D12Resource;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	namespace TerrainMaterials
	{
		class Masked;
		class Standard;
		class Extended;
	}

	enum class TextureUsage
	{
		TVScreen,
		AlbedoMap,
		FresnelMap,
		RoughnessMap,
		NormalMap,
		GlassMask,
	};

	class Texture;

	namespace Impl
	{
		class Object3D;

		class Texture
		{
			WRL::ComPtr<ID3D12Resource> tex;
			TextureUsage usage;					// not const to enable 'operator ='

		protected:
			// acquire GPU lifetime tracked resource
			TrackedResource<ID3D12Resource> Acquire() const;

		public:
			Texture();
			explicit Texture(const std::filesystem::path &fileName, TextureUsage usage, bool enablePacking, bool forceSysRAM);
			static std::shared_future<Renderer::Texture> __cdecl LoadAsync(std::filesystem::path fileName, TextureUsage usage, bool enablePacking, bool forceSysRAM);

			// define outside to break dependency on ComPtr`s implementation
		protected:
			Texture(const Texture &);
			Texture(Texture &&);
			Texture &operator =(const Texture &), &operator =(Texture &&);
			~Texture();

		public:
			explicit operator bool() const noexcept;
			auto Usage() const noexcept { return usage; }

		public:
			static void WaitForPendingLoads();
			static bool PendingLoadsCompleted();

		private:
			static std::forward_list<std::shared_future<Renderer::Texture>> pendingLoads;
		};
	}

	class Texture : public Impl::Texture
	{
		friend class TerrainMaterials::Masked;
		friend class TerrainMaterials::Standard;
		friend class TerrainMaterials::Extended;
		friend class Impl::Object3D;

	public:
		using Impl::Texture::Texture;

		// hide from protected
	private:
		using Impl::Texture::Acquire;
	};
}