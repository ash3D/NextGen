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

struct ID3D12Resource;

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	class TrackedResource : public WRL::ComPtr<ID3D12Resource>
	{
		typedef WRL::ComPtr<ID3D12Resource> Resource;

	private:
		template<bool move>
		void Retire();

	public:
		TrackedResource() = default;
		TrackedResource(const TrackedResource &) = default;
		TrackedResource(TrackedResource &&) = default;
		~TrackedResource();

	public:
		TrackedResource &operator =(nullptr_t src);
		TrackedResource &operator =(Resource::InterfaceType *src), &operator =(const Resource &src), &operator =(Resource &&src);
		template<class Other>
		TrackedResource &operator =(const WRL::ComPtr<Other> &src);
		template<class Other>
		TrackedResource &operator =(WRL::ComPtr<Other> &&src);
		TrackedResource &operator =(const TrackedResource &src), &operator =(TrackedResource &&src);

	public:
		auto operator &();

	public:
		void Attach(Resource::InterfaceType *src);
		auto Detach();
		auto ReleaseAndGetAddressOf();
		auto Reset();
		void Swap(Resource &src), Swap(Resource &&src);
		void Swap(TrackedResource &src), Swap(TrackedResource &&src);
	};

	inline TrackedResource::~TrackedResource()
	{
		Retire<true>();
	}

	inline TrackedResource &TrackedResource::operator =(nullptr_t src)
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	inline TrackedResource &TrackedResource::operator =(Resource::InterfaceType *src)
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	inline TrackedResource &TrackedResource::operator =(const Resource &src)
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	inline TrackedResource &TrackedResource::operator =(Resource &&src)
	{
		Retire<noexcept(Resource::operator =(std::move(src)))>();
		Resource::operator =(std::move(src));
		return *this;
	}

	template<class Other>
	TrackedResource &TrackedResource::operator =(const WRL::ComPtr<Other> &src)
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	template<class Other>
	TrackedResource &TrackedResource::operator =(WRL::ComPtr<Other> &&src)
	{
		Retire<noexcept(Resource::operator =(src))>();
		Resource::operator =(src);
		return *this;
	}

	inline TrackedResource &TrackedResource::operator =(const TrackedResource &src)
	{
		return operator =(static_cast<const Resource &>(src));
	}

	inline TrackedResource &TrackedResource::operator =(TrackedResource &&src)
	{
		return operator =(static_cast<Resource &&>(src));
	}

	inline auto TrackedResource::operator &()
	{
		Retire<false>();
		return Resource::operator &();
	}

	inline void TrackedResource::Attach(Resource::InterfaceType *src)
	{
		Retire<noexcept(Resource::Attach(src))>();
		Resource::Attach(src);
	}

	inline auto TrackedResource::Detach()
	{
		// force copy as reference returned outside
		Retire<false>();
		return Resource::Detach();
	}

	inline auto TrackedResource::ReleaseAndGetAddressOf()
	{
		Retire<noexcept(Resource::ReleaseAndGetAddressOf())>();
		return Resource::ReleaseAndGetAddressOf();
	}

	inline auto TrackedResource::Reset()
	{
		Retire<noexcept(Resource::Reset())>();
		return Resource::Reset();
	}

	inline void TrackedResource::Swap(Resource &src)
	{
		// force copy as reference goes to src
		Retire<false>();
		Resource::Swap(src);
	}

	inline void TrackedResource::Swap(Resource &&src)
	{
		// allow move as src is temp
		Retire<noexcept(Resource::Swap(std::move(src)))>();
		Resource::Swap(std::move(src));
	}

	inline void TrackedResource::Swap(TrackedResource &src)
	{
		// no need to retire as all resources are still tracked
		Resource::Swap(src);
	}

	inline void TrackedResource::Swap(TrackedResource &&src)
	{
		// no need to retire as all resources are still tracked
		Resource::Swap(std::move(src));
	}
}