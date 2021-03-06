#pragma once

#define NOMINMAX

#include <cstddef>
#include <wrl/client.h>

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	/*
	ComPtr is publicly accessible but it should be treated cautiously.
	Do not perform actions that causes to reference loosing or ownership transfer. It can results in GPU accessing to destroyed resource.
	In particular do not move construct or move assign ComPtr from tracked resource. Copy construct/assign on the other hand is valid.
	It is possible to remove public access to ComPtr to prevent invalid usage but it would be less concise; therewith direct access to ComPtr is probably rarely needed.
	*/
	template<class Interface>
	class TrackedResource : public WRL::ComPtr<Interface>
	{
		typedef WRL::ComPtr<Interface> Resource;

	public:
		using typename Resource::InterfaceType;

	private:
		template<bool move>
		void Retire();

	public:
		using Resource::Resource;
		TrackedResource() = default;
		TrackedResource(const TrackedResource &) = default;
		TrackedResource(TrackedResource &&) = default;
		~TrackedResource();

	public:
		TrackedResource &operator =(nullptr_t src);
		TrackedResource &operator =(InterfaceType *src), &operator =(const Resource &src), &operator =(Resource &&src);
		template<class Other>
		TrackedResource &operator =(const WRL::ComPtr<Other> &src);
		template<class Other>
		TrackedResource &operator =(WRL::ComPtr<Other> &&src);
		TrackedResource &operator =(const TrackedResource &src), &operator =(TrackedResource &&src);

	public:
		using Resource::operator &;	// for const version
		auto operator &();

	public:
		void Attach(InterfaceType *src);
		auto Detach();
		auto ReleaseAndGetAddressOf();
		auto Reset();
		void Swap(Resource &src), Swap(Resource &&src);
		void Swap(TrackedResource &src), Swap(TrackedResource &&src);
	};
}