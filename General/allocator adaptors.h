#pragma once

#include <utility>
#include <type_traits>
#include <memory>

namespace Misc
{
#pragma region adaptors
	// grants access to hidden ctor/dtor\
	have to be declared as friend
	template<class Alloc>
	class AllocatorProxyAdaptor : public Alloc
	{
		using Alloc::Alloc;
		typedef std::allocator_traits<Alloc> AllocTraits;

	public:
		template<typename T>
		struct rebind
		{
			typedef AllocatorProxyAdaptor<typename AllocTraits::template rebind_alloc<T>> other;
		};

	public:
		template<class Class, typename ...Params>
		void construct(Class *ptr, Params &&...params)
		{
			::new((void *)ptr) Class(std::forward<Params>(params)...);
		}

		template<class Class>
		void destroy(Class *ptr)
		{
			ptr->~Class();
		}
	};

	// replaces value init with default one\
	useful for avoiding zeroing out in STL containers if it is not needed\
	based on http://stackoverflow.com/a/21028912/273767
	template<class Alloc>
	class DefaultInitAllocatorAdaptor : public Alloc
	{
		using Alloc::Alloc;
		typedef std::allocator_traits<Alloc> AllocTraits;

	public:
		template<typename T>
		struct rebind
		{
			typedef DefaultInitAllocatorAdaptor<typename AllocTraits::template rebind_alloc<T>> other;
		};

	public:
		template<typename Class>
		void construct(Class *ptr) noexcept(std::is_nothrow_default_constructible_v<Class>)

		{
			::new((void *)ptr) Class;
		}

		template<typename Class, typename ...Args>
		void construct(Class *ptr, Args &&...args)
		{
			AllocTraits::construct(*this, ptr, std::forward<Args>(args)...);
		}
	};
#pragma endregion

#pragma region aliases
	template<typename T, template<typename> class Alloc = std::allocator>
	using AllocatorProxy = AllocatorProxyAdaptor<Alloc<T>>;

	template<typename T, template<typename> class Alloc = std::allocator>
	using DefaultInitAllocator = DefaultInitAllocatorAdaptor<Alloc<T>>;
#pragma endregion
}