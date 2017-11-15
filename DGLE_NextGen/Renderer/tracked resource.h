/**
\author		Alexey Shaydurov aka ASH
\date		15.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <utility>
#include <wrl/client.h>

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	/*
	ComPtr is publically accessible but it should be treated cautiously.
	Do not perform actions that causes to reference loosing or ownership transfer. It can results in GPU accessing to destroyed resource.
	In particular do not move construct or move assign ComPtr from tracked ressource. Copy construct/assign on the other hand is valid.
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

	template<class Interface>
	template<bool move>
	void TrackedResource<Interface>::Retire()
	{
		extern void RetireTrackedResource(WRL::ComPtr<struct IUnknown> resource);
		typedef Resource &Copy, &&Move;
		RetireTrackedResource(static_cast<std::conditional_t<move, Move, Copy>>(*this));
	}

	template<class Interface>
	inline TrackedResource<Interface>::~TrackedResource()
	{
		Retire<true>();
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(nullptr_t src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(InterfaceType *src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(const Resource &src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(Resource &&src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(std::move(src)))>();
		Resource::operator =(std::move(src));
		return *this;
	}

	template<class Interface>
	template<class Other>
	inline auto TrackedResource<Interface>::operator =(const WRL::ComPtr<Other> &src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Interface>
	template<class Other>
	inline auto TrackedResource<Interface>::operator =(WRL::ComPtr<Other> &&src) -> TrackedResource &
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(const TrackedResource &src) -> TrackedResource &
	{
		return operator =(static_cast<const Resource &>(src));
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator =(TrackedResource &&src) -> TrackedResource &
	{
		return operator =(static_cast<Resource &&>(src));
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::operator &()
	{
		// '&' itself does not releases reference (it released if reference is used) => disable move and keep reference
		Retire<false>();
		return Resource::operator &();
	}

	template<class Interface>
	inline void TrackedResource<Interface>::Attach(InterfaceType *src)
	{
		Retire<noexcept(Resource::Attach(src))>();
		Resource::Attach(src);
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::Detach()
	{
		// force copy as reference returned outside
		Retire<false>();
		return Resource::Detach();
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::ReleaseAndGetAddressOf()
	{
		Retire<noexcept(Resource::ReleaseAndGetAddressOf())>();
		return Resource::ReleaseAndGetAddressOf();
	}

	template<class Interface>
	inline auto TrackedResource<Interface>::Reset()
	{
		Retire<noexcept(Resource::Reset())>();
		return Resource::Reset();
	}

	template<class Interface>
	inline void TrackedResource<Interface>::Swap(Resource &src)
	{
		// force copy as reference goes to src
		Retire<false>();
		Resource::Swap(src);
	}

	template<class Interface>
	inline void TrackedResource<Interface>::Swap(Resource &&src)
	{
		// allow move as src is temp
		Retire<noexcept(Resource::Swap(std::move(src)))>();
		Resource::Swap(std::move(src));
	}

	template<class Interface>
	inline void TrackedResource<Interface>::Swap(TrackedResource &src)
	{
		// no need to retire as all resources are still tracked
		Resource::Swap(src);
	}

	template<class Interface>
	inline void TrackedResource<Interface>::Swap(TrackedResource &&src)
	{
		// no need to retire as all resources are still tracked
		Resource::Swap(std::move(src));
	}
}