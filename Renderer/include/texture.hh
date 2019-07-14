#pragma once

#define NOMINMAX

#include <utility>
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

	namespace Impl
	{
		class Object3D;

		class Texture
		{
			WRL::ComPtr<ID3D12Resource> tex;	// in system RAM
			TextureUsage usage;					// not const to enable 'operator ='

		protected:
			// acquire GPU lifetime tracked resource
			operator TrackedResource<ID3D12Resource>() const;

		public:
			Texture();
			explicit Texture(const std::filesystem::path &fileName, TextureUsage usage, bool enablePacking, bool forceSysRAM);

			// define outside to break dependency on ComPtr`s implementation
		protected:
			Texture(const Texture &);
			Texture(Texture &&);
			Texture &operator =(const Texture &), &operator =(Texture &&);
			~Texture();

		public:
			explicit operator bool() const noexcept;
			auto Usage() const noexcept { return usage; }
		};
	}

	class Texture : public Impl::Texture
	{
		friend class TerrainMaterials::Textured;
		friend class TerrainMaterials::Standard;
		friend class TerrainMaterials::Extended;
		friend class Impl::Object3D;

	public:
		using Impl::Texture::Texture;

		// hide from protected
	private:
		using Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>;
	};
}